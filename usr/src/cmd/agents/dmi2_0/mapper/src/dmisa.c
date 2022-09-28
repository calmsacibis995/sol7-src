/* Copyright 12/17/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisa.c	1.29 96/12/17 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisa.c                                                    */
/*                                                                    */
/* Copyright **********************************************************/
/*                                                                    */
/* Copyright:                                                         */
/*   Licensed Materials - Property of IBM                             */
/*   This product contains "Restricted Materials of IBM"              */
/*   xxxx-xxx (C) Copyright IBM Corp. 1994.                           */
/*   All rights reserved.                                             */
/*   US Government Users Restricted Rights -                          */
/*   Use, duplication or disclosure restricted by GSA ADP Schedule    */
/*   Contract with IBM Corp.                                          */
/*   See IBM Copyright Instructions.                                  */
/*                                                                    */
/* End Copyright ******************************************************/
/*                                                                    */
/*  Description:                                                      */
/*  This program performs a translation of DMI to SNMP protocols.     */
/*  Requests from an SNMP manager pass through the SNMP agent,        */
/*  which submits its requests through the DPI interface (RFC 1228).  */
/*  Operations the agent requests of this sub-agent are as follows:   */
/*      o  GET                                                        */
/*      o  GETNEXT                                                    */
/*      o  SET                                                        */
/*  Also, the sub-agent can initiate communication with the agent.    */
/*  It sends a TRAP to the agent on receipt of each DMI indication,   */
/*  which can be one of three types: component INSTALL, component     */
/*  UNINSTALL, and EVENT.                                             */
/*                                                                    */
/*  The main() function and its sub-functions does the following:     */
/*    1. Initialize program resources, interfaces with DMI and DPI,   */
/*       and the translation table retained in memory.                */
/*       Send a cold-start trap.                                      */
/*    2. Handles requests from the SNMP agent.                        */
/*    3. Reinitializes DMI and DPI interfaces on occurrence           */
/*       of a "severe" error and rebuilds the translation table       */
/*       by rescanning for .MAP files and rescanning the MIF database */
/*       for all installed components.  Then send a warm-start trap.  */
/*       Note: If the warm-start trap is caused by the interface to   */
/*             the Service Layer goes disconnects and then reconnects,*/
/*             the warm-start will not be sent until AFTER a manager  */
/*             initiates a request from the subagent.  The warmstart, */
/*             hence, is semi-synchronous in certain conditions.      */
/*  Program termination is handled by the exitRoutine() function.     */
/*                                                                    */
/*  Note: Only those components whose component names                 */
/*        are present in a .MAP file found by this program.           */
/*        (See DMISAMAP.C for details.)                               */
/*                                                                    */
/*  This file contains the following functions:                       */
/*                                        usage(...)                  */
/*                                        main(...)                   */
/*                                        initSubagent(...)           */
/*                                        dpiConnectAndOpen(...)      */
/*                                        dpiRegister(...)            */
/*                                        dpiRegisterOne(...)         */
/*                                        traceReqBuf(...)            */
/*                                        doDmiCleanup(...)           */
/*                                        doDmiSetup(...)             */
/*                                        dpiUnreg(...)               */
/*                                        dpiUnregOne(...)            */
/*                                        dpiUnregByAgent(...)        */
/*                                        dpiClose(...)               */
/*                                        dpiCloseByAgent(...)        */
/*                                        dpiDisconnect(...)          */
/*                                        exitRoutine(...)            */
/*                                                                    */
/* End Module Description *********************************************/
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  941214    LAUBLI    New module                    */
/*                  950125    LAUBLI    Added error code to trace info*/
/*                  950207    LAUBLI    Update IP address for ANYNET  */
/*                  950317    LAUBLI    Added malloc check            */
/*                  960620    JITEN     Replaced SNMP_DPI with Agent Relay*/ 
/*                  960701    JITEN     Implementing DMI2.0           */
/* End Change Log *****************************************************/

static char    ibmid[] = "Copyright IBM Corporation 1994 LICENSED MATERIAL -"
                         " PROGRAM PROPERTY OF IBM";

#define INCL_DOSMODULEMGR
#include <ctype.h>
#include <locale.h>

#ifndef WIN32
#include <netdb.h>
#endif

#include "dmisa.h"                  /* global header file for DMI Sub-agent*/
#include <syslog.h>


#ifdef WIN32                  /* LLR 09-11-95 included winsock header */
#include <winsock.h>
#endif

#ifdef OS2
#include <types.h>
#include <utils.h>
#include <sys\socket.h>
#include <netinet\in.h>
#include <snmpcomm.h> // $MED *here - was transport\comm\include\snmp_comm.h
#endif

#ifdef SOLARIS2
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pdu.h>
#include <trap.h>
#include <pagent.h>
#include <dpitosar.h>
#endif

#define HOST_NAME_BUFFER_LEN         256
#define COMMUNITY_NAME_BUFFER_LEN    256

#ifdef WIN32
BOOL bWSAStartup = FALSE;
#endif

#ifndef WIN32            /* this has been moved from dmisa.h */
int Connected = FALSE;
#endif

OidPrefix_t    *XlateList;
KeyValueList_t *KeyValueList;
ReserveList_t  *ReserveList;        /* Set to NULL if no list exists  */
int             WatchReserveListFlag; /* set to nonzero if there is a ReserveList thread */

char   *ResponseBuffer;             /* buffer used to issue our requests*/
ULONG   DmiHandle;                   /* application handle returned by the service layer*/
char    SpecLevel[256];             /* buffer used for the spec level of the service layer*/
char    SLDescription[256];         /* implmentation description      */
char    SP_hostname[256];           /* The hostname of the SP */
ULONG   CompCounter,GroupCounter,AttributeCounter,AttrCounter;
ULONG   SComponent,SGroup,SAttribute;
snmp_dpi_u64   value4 = {1L,1L};

int     ExceptionProcFlag;
int     Dmi_Registered_Flag;  /* used to tolerate an  rc=300 on 1st DosResetEventSem*/
int     Dpi_Registered_Flag;
int     Dpi_Opened_Flag;

/*This is relocated to dmisa.c as it gives multiply defined errors */
/*#ifndef WIN32 
int     Connected = FALSE;
#endif*/

int     DmiState;
unsigned long    DpiAgentWaitTimeout;
int     DpiWaitForAgentTimeout;
int     DpiRegTimeout;
int     DpiHandle;
ULONG   DmiReqHandle;

#ifdef WIN32			/* LLR 09-11-95 added section for NT              */
#define QUE_NAME "DMISA.QUE"
PID     OwningPID;    /* PID of queue owner */
ULONG   ElementCode;  /* Request a particular element */
BOOL    NoWait;       /* No wait if queue is empty */
BYTE    ElemPriority; /* Priority of element being added */

UCHAR   SemName[40];  /* Semaphore name */
HEV     IndSemHandle;    /* Indication buffering event1 semaphore handle, added 950221.gwl */
HEV     queueMutex;    /* Confirm buffering event semaphore handle */
HEV     Ind3SemHandle;   /* Indication buffering event2 semaphore handle, added 950221.gwl */
HMTX    DmiAccessMutex;  /* Mutual exclusion semaphore handle */
ULONG   flAttr;       /* Creation attributes */
BOOL    fState;       /* Initial state of the semaphore */
ULONG   ulPostCt;     /* Post count for the event semaphore */
DWORD   ulIndTimeout; /* Number of milliseconds to wait for Indicate to complete */
QueueEntryPtr   pQueueHead;     /* head of confirm queue  */


#elif defined OS2
#define QUE_NAME "\\QUEUES\\DMISA.QUE"
PID     OwningPID;    /* PID of queue owner */
HQUEUE  QueueHandle;  /* Queue handle */
REQUESTDATA    QRequest;   /* Request-identification data */
ULONG   ElementCode;  /* Request a particular element */
BOOL32  NoWait;       /* No wait if queue is empty */
BYTE    ElemPriority; /* Priority of element being added */

UCHAR   SemName[40];  /* Semaphore name */
HEV     CnfSemHandle;    /* Confirm buffering event semaphore handle */
HEV     IndSemHandle;    /* Indication buffering event1 semaphore handle, added 950221.gwl */
HEV     Ind3SemHandle;   /* Indication buffering event2 semaphore handle, added 950221.gwl */
HMTX    DmiAccessMutex;  /* Mutual exclusion semaphore handle */
ULONG   flAttr;       /* Creation attributes */
BOOL32  fState;       /* Initial state of the semaphore */
ULONG   ulPostCt;     /* Post count for the event semaphore */
#else
mutex_t DmiAccessMutex; /* Mutual exclusion semaphore handle */
QueueEntryPtr   pQueueHead;     /* head of confirm queue  */
mutex_t queueMutex;     /* Mutual exclusion semaphore handle */
cond_t  queueCond;      /* Event semaphore handle */
mutex_t indicateMutex;                 /* mutex semaphore */
cond_t  indicateCond;                  /* condition semaphore */
#endif
ULONG   ulDmiTimeout; /* Number of milliseconds to wait for DMI response */
ULONG   ulIndTimeout; /* Number of milliseconds to wait for Indicate to complete */
ULONG   ulIndWaitTimeout; /* Number of milliseconds to wait for Indicate to arrive */

static char    HostName[HOST_NAME_BUFFER_LEN];
static char    Community[COMMUNITY_NAME_BUFFER_LEN] = DEFAULT_COMMUNITY;
char  logbuffer[200];
#ifdef DMISA_TRACE
unsigned char  logbuffer[132];
#endif
snmp_dpi_hdr  *Hdr_p;
static char   *CompMibOid = SUN_ARCH_DMI_COMPMIB;
int            state = DMISA_NONE;
/*************AGENT RELAY SPECIFIC********************************************/
extern int socket_handle;
Address address;
Address my_address;
SNMP_pdu *pdu;
Agent   subagent;
struct sockaddr_in sock_in;
extern char error_label[1000];
/*           DMI2.0 specific *****************************************/
DmiRpcHandle  dmi_rpchandle;
char *default_configdir = "/etc/dmi/conf";
char *default_sub_warntime = "20101231110000.000000-420   ";
char *default_sub_exptime =  "20101231120000.000000-420   ";
int   default_failure_threshold = 1;
int   default_trap_forward_to_magent = 1;

char *configdir ;
char *sub_warntime ;
char *sub_exptime ;
int   failure_threshold ;
int   trap_forward_to_magent ;
int   TraceLevel;


/* FUNCTION DECLARATIONS:                                             */
int         main(int argc, char *argv[]);
static void usage(char *cmd_p);
static int  initSubAgent(int argc, char *argv[]);
static int  dpiConnectAndOpen(void);
int  dpiRegister(OidPrefix_t *xlatelist);
static int  dpiRegisterOne(char *group_p,int treeindex);
int  dpiUnreg(OidPrefix_t *xlatelist);
static int  dpiUnregOne(char *group_p,int treeindex);
static int  dpiUnregByAgent(snmp_dpi_hdr *hdr_p, snmp_dpi_ureg_packet *pack_p);
static int  dpiCloseByAgent(snmp_dpi_hdr *hdr_p,snmp_dpi_close_packet *pack_p);
static int  dpiClose(void);
static void dpiDisconnect(void);
int dpiAreYouThere(void);


/*********************************************************************/
/* Function:  main()                                                 */
/*                                                                   */
/* Description:                                                      */
/*   1. Perform DMI sub-agent initialization.                        */
/*   2. Register with the DMI Service Layer.                         */
/*   3. Build translation table to be maintained in memory.          */
/*   4. Connect to, open, and register with the SNMP agent thru DPI. */
/*   5. Send cold-start or warm-start trap to SNMP agent.            */
/*   6. Handle SNMP requests (GET, GETNEXT, SET).                    */
/*   7. If "severe" error occurs, unregister from DPI and DMI and    */
/*      reinitialize, starting from step (2) above.                  */
/*                                                                   */
/*   Program is terminated under the following conditions:           */
/*      - exit function instantiation failed                         */
/*      - program initialization failed                              */
/*      - request/release of TODMI mutex semaphore failed            */
/*      - heap memory allocation failed                              */
/*                                                                   */
/*                                                                   */
/*   Structures for this program are contained primarily in these    */
/*   header files:  DMISA.H, DMI.H, and SNMP_DPI.H.                  */
/*                                                                   */
/*                                                                   */
/*  THREADING MODEL       key:     =  thread blocked                 */
/*  ===============                |  thread not blocked             */
/*                                 x  end of thread                  */
/*            |                                                      */
/*   initialize_program -+----------------------> begin_trace_thread */
/*            |          '-> begin_indicate_thread            |      */
/*            |                    | <--------------------,   |      */
/*            |                    |                      |   |      */
/*            |                    |     dmi_ind_handler  |   |      */
/*            |              waitIND1evtsem     |         |   |      */
/*            |                    =     requestmutex3sem |   |      */
/*            |                    =     postINDevtsem    |   |      */
/*            |                    = <-- releasemutex3sem |   |      */
/*            |           requestDmiAccessMutex |         |   |      */
/*            |             get_indication      x         |   |      */
/*            |                send_trap                  |   |      */
/*            |                    =                      |   |      */
/*  ,-------> |                    =                      |   |      */
/*  |         |                    =                      |   |      */
/*  |  releaseDmiAccessMutex ----> |                      |   |      */
/*  |  dpi_wait_packet       updateXlateList              |   |      */
/*  |  requestDmiAccessMutex       |                      |   |      */
/*  |         =                    |                      |   |      */
/*  |         =  <---------- releaseDmiAccessMutex        |   |      */
/*  |         |              requestmutex3sem             |   |      */
/*  |         |   checkifmoreindtoprocess(queryINDevtsem) |   |      */
/*  |         |              ifnomoreind,resetINDevtsem   |   |      */
/*  |         |              releasemutex3sem             |   |      */
/*  |   process_packet             '----------------------'   |      */
/*  |         |                                               |      */
/*  | ,-----> | --DMIinvoke--> dmi_cnf_handler                |      */
/*  | | waitCNF1evtsem               |                        |      */
/*  | |       =                      |                        |      */
/*  | |       = <------------- postCNF1evtsem                 |      */
/*  | | resetCNF1evtsem              |                        |      */
/*  | |       |                      x                        |      */
/*  | '-------+                                               |      */
/*  |         |->begin_reservelist_thread(forSETonly)         |      */
/*  |         |           |                                   |      */
/*  |         |           |<------------,                     |      */
/*  '---------'   requestDmiAccessMutex |                     |      */
/*                  cleanReserveList()  |                     |      */
/*                releaseDmiAccessMutex |                     |      */
/*                        +-------------'                     |      */
/*                        |                                   |      */
/*                        x                                   |      */
/*                                                                   */
/*   Note: Trace levels are currently set to the following:          */
/*           1 = Errors traced                                       */
/*           2 = 1 + Trace translation status                        */
/*           3 = 2 + Trace internal low-level status                 */
/*           4 = 3 + Trace DMI control blocks                        */
/*           5 = 4 + Trace DPI PDUs and semaphore passing            */
/*                                                                   */
/*********************************************************************/
int main(int argc, char *argv[])
{
   unsigned char *packet_p;
   int            rcdpi, rcsem, rc = 0, waitdmisa, leave;
   int            sentcoldstart = DMISA_FALSE;
   APIRET         rcdos;
   ULONG          length, istatus;
   int            permloop=DMISA_TRUE;
   int            dmiretry=0; 
   int            snmpretry=0;
#ifdef DMISA_TRACE
   time_t         starttime;
   time_t         currenttime;
#endif

#ifdef OS2
   /* Define DOS exit routine                                         */
   rcdos = DosExitList(EXLST_ADD | ROUTINE_ORDER,
                       (PFNEXITLIST) exitRoutine);
   if ( rcdos != 0 ) { /* 1=invalid_function, 8=not_enough_mem, 13=invalid_data*/
      printf("Error: DosExitList return code = %u.\n",rcdos);
      DMI_SLEEP(2000);
      exit(1);
   }
#endif

#ifdef AIX41
   signal(SIGHUP, SIG_IGN);   /* so SMIT acutally starts us */
#endif

#ifdef WIN32
   if (!SetConsoleCtrlHandler(exitRoutine, TRUE)){
         printf("Error: SetConsoleCtrlHandler");
         DMI_SLEEP(2000);
         exit(1);
   }
#endif

   /* message catalog setup */
   setlocale(LC_ALL, "");
#ifndef AIX41
   printf( COPYRIGHT1 );
   printf( COPYRIGHT2 );
#else
   printf("%s", msgDmi(MAPPER_VERSION, MAPPER_VERSION_D));
   printf("%s", msgDmi(OS_SL_COPYRIGHT, OS_SL_COPYRIGHT_D));
#endif

   rc = initSubAgent(argc, argv);
   if (rc != 0) {
      DMISA_TRACE_LOG1(LEVEL1,"Initialization failed.  Return code = %d.", rc);  /* changed, 950220.gwl */
      DMI_SLEEP(2000);
      exit(1);                  /* state = 0  No connection                  */
   }                            /*       = 1  Connected to DMI only          */
                                /*       = 2  DMI-connected & XlateList built*/
                                /*       = 3  DMI/XlateList & DPI opened     */
                                /*       = 4  DMI/XlateList & DPI opened/conn*/
                                /*       = 5  DMI/Xlate & DPI opened/conn/reg*/
   while (permloop) {  /* do forever */
      rc = 0;              /* initialize                              */
      waitdmisa = DMISA_FALSE;  /* initialize                         */
      if (state < DMI_ONLY) {
         if (DMI_REGISTER_noError == (rc = dmiRegister(&istatus))) { /* register with DMI*/
            Dmi_Registered_Flag = DMISA_TRUE;
            state = DMI_ONLY;
            dmiretry=0;
         } else {
            waitdmisa = DMISA_TRUE;
            if (++dmiretry > 10) {   /* Try for about a minute */
            DMISA_TRACE_LOG1(LEVEL1,"Registration with DMI failed. err = %d.", rc);
            exit(1);
            }
            sleep(10);
            continue;
         }
         if (Ind_SubscribeWithSP()) {
             DMISA_TRACE_LOG(LEVEL1,"Failed to Subscribe with SP for Indication delivery");
             exit(1);
         }
         if (Ind_AddFilterToSP()) {
             DMISA_TRACE_LOG(LEVEL1,"Failed to add filter to SP for Event delivery\n");
             exit(1);
         }
      }

      if (waitdmisa == DMISA_FALSE && state < DMI_XLATE) {
         if (BUILD_XLATE_noError == (rc = buildXlate(&XlateList))) { /* build translate list*/
            state = DMI_XLATE;
         } else {
            waitdmisa = DMISA_TRUE;
            DMISA_TRACE_LOG1(LEVEL1,"Unable to build XlateList proplerly. Error code %d.", rc);
         }
      }
      if (waitdmisa == DMISA_FALSE && state < DMI_XLATE_DPIOPEN) {
         if (DPI_CONNECT_AND_OPEN_noError == dpiConnectAndOpen()) { /* connect/open DPI*/
            state = DMI_XLATE_DPIOPEN;
            snmpretry = 0;
         } else {
            waitdmisa = DMISA_TRUE;
            if (++snmpretry > 10) {
              DMISA_TRACE_LOG(LEVEL1,"Unable to connect to snmpdx");
              exit(1);
            }
         }
      }
      if (waitdmisa == DMISA_FALSE && state < DMI_XLATE_DPIREG) {
         if (DPI_REGISTER_noneRegistered != dpiRegister(XlateList)) { /* register subtrees*/
            if (!sentcoldstart) {       /* Issue TRAP to SNMP Agent*/
               handleStartTrap(DMISA_REQ_COLDSTART);
               sentcoldstart = DMISA_TRUE;
            } else {
               handleStartTrap(DMISA_REQ_WARMSTART);
            }
            state = DMI_XLATE_DPIREG;  /* Ready to process SNMP requests*/
         }                                /* for those components registered*/
      }
      if (waitdmisa == DMISA_FALSE) {
         leave = DMISA_FALSE;       /* Initialize               */
         DMISA_TRACE_LOG(LEVEL2,"Ready to receive requests from the SNMP agent.");

         while (state == DMI_XLATE_DPIREG && leave == DMISA_FALSE) {

#ifdef WIN32
            rcsem = ReleaseMutex(DmiAccessMutex);
            if (rcsem == FALSE)           /* Release Mutex*/
               rcsem = 1;                     /* Signify that an error occurred. */
            else
               rcsem = 0;                 /* No error occurred */


#elif defined OS2
            rcsem = DosReleaseMutexSem(DmiAccessMutex);  /* Release Mutex*/
#else
            rcsem = mutex_unlock(&DmiAccessMutex); /* Release Mutex*/
#endif
            if (rcsem != 0) {  /* can get 0, 6invhndl, 288notowner*/
               DMISA_TRACE_LOG1(LEVEL1,"Release Mutex error: rc = %u.", rcsem);
               exit(-1);
            }

#ifndef SOLARIS2
            rcdpi = DPIawait_packet_from_agent( /* wait for DPI packet */
                    DpiHandle,              /* on this connection      */
                    DpiWaitForAgentTimeout, /* ( -1 = wait forever )   */
                    &packet_p,              /* receives ptr to packet  */
                    &length);               /* receives packet length  */
#else
            pdu = snmp_pdu_receive(socket_handle, &address, error_label); 
            if (!pdu) {
          DMISA_TRACE_LOG1(LEVEL1,"Error receiving PDU %s.", error_label);
             rcdpi = DPI_RC_NOK;
            }else {
            
             rcdpi = DPI_RC_OK;
            if (TraceLevel == 5) {
            printf("Request PDU................\n");
            trace_snmp_pdu(pdu);
            }

            }

#endif
#ifdef DMISA_TRACE
               time(&starttime);  /* Calculate the time the operation is started*/
#endif

#ifdef WIN32      /* LLR  09-11-95 added for WIN32 */
                            (DWORD) rcsem = WaitForSingleObject(DmiAccessMutex, ulIndTimeout);
                            if ((rcsem != WAIT_FAILED) && (rcsem != WAIT_TIMEOUT))
                              rcsem = 0;

#elif defined OS2
            rcsem = DosRequestMutexSem(DmiAccessMutex, ulIndTimeout);
#else
            rcsem = mutex_lock(&DmiAccessMutex); /*$MED no timeout */
#endif
            if (rcsem != 0) {  /* can get 0, 6invhndl, 95interrupt, 103toomanysemreq,*/
                               /*         105semownerdied, 640timeout*/
               DMISA_TRACE_LOG1(LEVEL1,"MutexSem error (DmiAccessMutex): rc = %u.", rcsem);
               exit(-1);
            }
            if (rcdpi != DPI_RC_OK) {
               DMISA_TRACE_LOG1(LEVEL1, "Error receiving packet from agent; rc = %d.", rcdpi);
               DMISA_TRACE_LOG(LEVEL1, "Will attempt to re-establish connection.");
               rc = DMISA_ERROR_dpiBroke;
               break;
            }

#ifndef SOLARIS2
            Hdr_p = pDPIpacket(packet_p);  /* Parse the DPI packet    */
#else
            Hdr_p = pdu_to_dpihdr(pdu); /*Convert PDU to DPI packet */
#endif
            if (Hdr_p == snmp_dpi_hdr_NULL_p) { /* If parse fails     */
               DMISA_TRACE_LOG(LEVEL1,"Error in parsing packet from agent.");
               rc = DMISA_ERROR_dpiBroke;
               break;
            }

            if (KeyValueList &&           /* if KeyValueList exists and is valid*/
                Hdr_p->packet_type != SNMP_DPI_GETNEXT) {
               freeKeyValueList(); /* Free storage used to speed up GetNexts*/
            }

            switch(Hdr_p->packet_type) {   /* Handle by DPI type      */
               case SNMP_DPI_GET:
                  rc = doGet(
                          Hdr_p,
                          Hdr_p->data_u.get_p,
                          XlateList);
                  if (rc != DO_GET_noError) {
                     DMISA_TRACE_LOG1(LEVEL1, "Error on GET request: rc = %d.",rc);
                     leave = DMISA_TRUE;
                  }
                  break;
               case SNMP_DPI_GETNEXT:
                  rc = doGetNext(
                          Hdr_p,
                          Hdr_p->data_u.next_p,
                          XlateList);
                  if (rc != DO_GET_NEXT_noError) {
                     DMISA_TRACE_LOG1(LEVEL1, "Error on GETNEXT request: rc = %d.",rc);
                     leave = DMISA_TRUE;
                  }
                  break;
               case SNMP_DPI_SET:
               case SNMP_DPI_COMMIT:
               case SNMP_DPI_UNDO:
                 while(permloop) {
                  rc = doSet(
                          Hdr_p,
                          Hdr_p->data_u.set_p,
                          XlateList);
                  if ((rc == DO_SET_noError) &&
                      (Hdr_p->packet_type == SNMP_DPI_COMMIT)) {
                       break;
                  }
                  if ((rc == DO_SET_noError) &&
                     (Hdr_p->packet_type == SNMP_DPI_SET)) {
                     Hdr_p->packet_type = SNMP_DPI_COMMIT;
                     continue;
                  }
                  if (rc != DO_SET_noError) {
                     DMISA_TRACE_LOG1(LEVEL1, "Error on SET request: rc = %d.",rc);
                     leave = DMISA_TRUE;
                     break;
                  }
                  }
                  break;
               case SNMP_DPI_UNREGISTER:
                  rc = dpiUnregByAgent(Hdr_p, Hdr_p->data_u.ureg_p);
                       /* Assume it worked, even if it failed   */
                       /* Subsequent DPI close will cause unregister*/
                  Dpi_Registered_Flag = DMISA_FALSE;
                  state = DMI_XLATE_DPIOPEN;
                  break;
               case SNMP_DPI_CLOSE:
                  rc = dpiCloseByAgent(Hdr_p, Hdr_p->data_u.close_p);
                  state = DMI_XLATE;
                  break;
               default:
                  DMISA_TRACE_LOG1(LEVEL1,"Unexpected DPI packet type %d\n", Hdr_p->packet_type);
                  leave = DMISA_TRUE;
                  break;
            } /* endswitch */

            if (Hdr_p) fDPIparse(Hdr_p);  /* Free the parsed DPI packet    */

#ifdef DMISA_TRACE
            /* Trace operation duration.                        */
            time(&currenttime);  /* calculate the time the operation ends*/
            if (&currenttime != NULL && currenttime != -1L &&
                &currenttime != NULL && currenttime != -1L) { /* then we have good times*/
               DMISA_TRACE_LOG2(LEVEL3,"Operation time within sub-agent: %02d:%02d (minutes:seconds)\n", (currenttime-starttime) / 60, (currenttime-starttime) % 60);
            }
#endif
         } /* endwhile */
         if (DMISA_ERROR_outOfMemory == rc % 10) {
            DMISA_TRACE_LOG1(LEVEL1,"Out of memory.  Return code = %d\n", rc);
            exit(-1);
         }
      } /* endif WAITDMISA == DMISA_FALSE */

      if (rcdpi != DPI_RC_OK) {
         if (dpiAreYouThere() == 0) {
             rc = rcdpi = 0;
             waitdmisa = DMISA_FALSE;
         }
         else {
             state = DMI_XLATE;
         }
      } /* bad rc from dpi */

      /* handle severe errors by re-initializing as necessary. */
      if ((DMISA_ERROR_dmiBroke == rc) || (DMISA_ERROR_dmiBroke == (rc % 10)))
      {
         if (state >= DMI_XLATE_DPIREG) {
            rcdpi = dpiUnreg(XlateList);
            state = DMI_XLATE_DPIOPEN;
         }
         if (state >= DMI_XLATE_DPIOPEN) {
            dpiClose();
            state = DMI_XLATE_DPICONN;
         }
         if (state >= DMI_XLATE_DPICONN) {
            dpiDisconnect();
            if (DMISA_ERROR_dmiBroke == rc) state = DMI_XLATE;
         }
         if (state >= DMI_XLATE && DMISA_ERROR_dmiBroke == (rc % 10)) {
            if (XlateList) freeXlate();
            state = DMI_ONLY;
         }
         if (state >= DMI_ONLY && DMISA_ERROR_dmiBroke == (rc % 10)) {
                         /* rc from within inner while loop              */
            if (DMI_UNREGISTER_noError != (rc = dmiUnregister(ExceptionProcFlag, ++DmiReqHandle))) {
               DMISA_TRACE_LOG1(LEVEL1,"Unregister from DMI failed. rc = %d.", rc);
            } else DMISA_TRACE_LOG(LEVEL2,"DMI unregistered.");
            state = DMISA_NONE;
         }
      } /* handle severe errors by re-initializing as necessary. */

      if (waitdmisa == DMISA_TRUE) {
          DMI_SLEEP(CONNECT_RETRY_TIME); /* Sleep before next connection attempt*/
      } else if (state == DMISA_NONE) {
          DMI_SLEEP(DMI_UNREG_REREG_PAUSE); /* At least give the DMI time to unregister*/
      }

   } /* endwhile */

}


/*********************************************************************/
/* Function:  usage()                                                */
/*                                                                   */
/* Description:                                                      */
/*   Send to stdout a description of command parameters              */
/*   and their use, then exit the program.                           */
/*                                                                   */
/* Return codes: None. No return to caller of this function.         */
/*********************************************************************/
static void usage(char *cmd_p)
{
   printf("\nUsage: %s -s SP hostname [-h] [-c config dir.] [-d debuglevel]\n",cmd_p);
   printf("             SP hostname is host, where DMI service provider\n");
   printf("             is running.Without -d option it starts as a daemon.\n");
   printf("             -h for command line usage\n");
   printf("             debuglevel is an integer from 1 to 5.\n");
   DMI_SLEEP(2000);  /* pause                                       */
   exit(1);
}


/*********************************************************************/
/* Function:  initSubAgent()                                         */
/*                                                                   */
/* Description:                                                      */
/*   o  Process command arguments.  (If an error is detected,        */
/*      control is not returned to the caller of this function.      */
/*      The program is terminated.)                                  */
/*   o  Initialize the trace function.                               */
/*   o  Create MutEx semaphore for Confirm/Indicate synchronization  */
/*      control.-- This is defunct for DMI2.0  RPC  implementation   */
/*   o  Perform RPC registration for MI callback functions           */
/*   o  Make additions to filter table/ subscription table           */
/*                                                                   */
/* Return codes:  INIT_SUB_AGENT_noError                             */
/*                INIT_SUB_AGENT_createSemFailed                     */
/*                INIT_SUB_AGENT_outOfMemory                         */
/*                DO_DMI_SETUP_noError                               */
/*                DO_DMI_SETUP_createSemFailed                       */
/*                DO_DMI_SETUP_createQueueFailed                     */
/*********************************************************************/
int initSubAgent(int argc, char *argv[])
{
   char   *cmd_p = (char *) NULL;
   int     i, rc;
   APIRET  rcsem;
   int opt;
   extern char * optarg;
   extern int    optind;
   unsigned int DmiDebug = 0;

#ifdef DMISA_TRACE
   unsigned int DmiDebug = 0;
   char   *debugopt;

   TraceLevel =
   TracetoScreen = DMISA_FALSE;
#endif

   DpiAgentWaitTimeout = DPI_AGENT_WAIT_TIMEOUT; /* Initialize        */
   DpiWaitForAgentTimeout = DPI_WAIT_FOREVER;    /* Initialize        */
   DpiRegTimeout = DPI_REG_TIMEOUT;              /* Initialize        */
   ulIndTimeout = INDICATION_TIMEOUT;            /* Initialize        */
   Dmi_Registered_Flag = DMISA_FALSE;
   ExceptionProcFlag   = DMISA_FALSE;
   KeyValueList        = NULL;        /* Initialize - used for GetNext operation*/
   ReserveList         = NULL;        /* Initialize - used for Set operation*/
   XlateList           = NULL;
   WatchReserveListFlag= 0;
   
   configdir = default_configdir;
   sub_warntime = default_sub_warntime;
   sub_exptime = default_sub_exptime;
   failure_threshold = default_failure_threshold;
   trap_forward_to_magent = default_trap_forward_to_magent;


   if (argc > 0) {
      cmd_p = argv[0];
   }

   while((opt = getopt(argc, argv, "c:s:d:h")) != EOF) {
          switch(opt) {
            case 'c':
                      configdir = strdup(optarg);
                      break;  
            case 's':
                      strcpy(SP_hostname, optarg);        
                      break; 
            case 'd':
                      DmiDebug = atoi(optarg); 
                      if (DmiDebug < LEVEL5 && DmiDebug > 0) {
                           TraceLevel = DmiDebug;
                      } else if (DmiDebug >= LEVEL5) {
                           TraceLevel = LEVEL5;
                      } 
                      break;
            case 'h':
            default:
                      usage(cmd_p);
                 
          }
   } /*end of while*/
   if (optind != argc)
           usage(cmd_p);


#ifdef DMISA_TRACE
   if (TraceLevel >= LEVEL5)
      TracetoScreen = DMISA_FALSE;
   strcpy(TraceFileMain,"DMISMAIN.LOG");
   strcpy(TraceFileOverflow,"DMISOVER.LOG");
   traceInit();
#endif

   Initconfig();

   if (!strlen(SP_hostname)) {
          usage(cmd_p);
          exit(1);
   }
/*Make a daemon process of this current process*/
   if (TraceLevel == 0) 
          make_daemon(); 


   ResponseBuffer = malloc(REGCNFSIZE);    /* Used for DMI registration response*/
   if (!ResponseBuffer) return INIT_SUB_AGENT_outOfMemory;  /* added 950317.gwl */

   /* Create semaphore used to synchronize DPI requests (Get/Getnext/Set)*/
   /* with Indication thread's update of XlateList in response to     */
   /* DMI Installs and Uninstalls.                                    */
#ifdef WIN32         /* LLR 09-11-95 added for WIN32 lock semaphore */
   strcpy((CHAR *)SemName,"DMISAIND2");
   DmiAccessMutex = CreateMutex((LPSECURITY_ATTRIBUTES) NULL, TRUE,     SemName);
   (DWORD) rcsem=GetLastError();

#elif defined OS2
   strcpy((CHAR *)SemName,"\\SEM32\\DMISAIND2");
   flAttr = 0;          /* Unused, since this is a named semaphore */
   fState = 1;          /* Semaphore initial state is "owned" */
   rcsem = DosCreateMutexSem( SemName, &DmiAccessMutex, flAttr, fState );
      /* On successful return, DmiAccessMutex contains handle of new system event semaphore*/
#elif AIX325
   rcsem = pthread_mutex_init(&DmiAccessMutex, pthread_mutexattr_default);
#else
   rcsem = mutex_init(&DmiAccessMutex, USYNC_THREAD, NULL);
   rcsem = mutex_lock(&DmiAccessMutex); /*$MED no timeout */
#endif
   if ( rcsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname,*/
                       /*        285dupname, 290toomanyhndls          */
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateMutexSem error: rc = %u.", rcsem);
      return INIT_SUB_AGENT_createSemFailed;
   } else {
      DMISA_TRACE_LOG1(LEVEL5,"Mutex semaphore %u successfully created.", DmiAccessMutex);
   }

/*RPC registration of MI callback services */
   if (!register_callback_rpc()) {
      DMISA_TRACE_LOG1(LEVEL1,"Failed to register callback error = %d",errno);
      return INIT_SUB_AGENT_rpcregfailed;
   }

   rc = doDmiSetup();
   if (rc != DO_DMI_SETUP_noError) return rc;

   return INIT_SUB_AGENT_noError;
}

/*********************************************************************/
/* Function:  dpiConnectAndOpen()                                    */
/*                                                                   */
/* Description:                                                      */
/* Set up a connection and identify ourself to the SNMP agent.       */
/* Setting up the connection is one simple call to the the function  */
/* DPIconnect_to_agent_TCP().                                        */
/* The function returns an negative error code if an error occurs.   */
/* If the connection setup is successful, then it returns a handle   */
/* which represents the connection, and which we must use on         */
/* subsequent calls to send or await DPI packets.                    */
/*                                                                   */
/* The second step is to identify the subagent to the agent. This is */
/* done by making a DPI-OPEN packet, sending it to the agent and     */
/* then awaiting the response from the agent. The agent may accept   */
/* or deny the OPEN request.                                         */
/* Making a DPI-OPEN packet is done by calling mkDPIopen().          */
/* The function returns a ptr to a static buffer holding the DPI     */
/* packet if success. If it fails, it returns a NULL ptr.            */
/*                                                                   */
/* Once the DPI-OPEN packet has been created, we must send it to     */
/* the agent. We can use the DPIsend_packet_to_agent() function.     */
/* This function returs DPI_RC_OK (value zero) if success, otherwise */
/* an appropriate DPI_RC_xxx error code as defined in snmp_dpi.h.    */
/*                                                                   */
/* Now we must wait for a response to the DPI-OPEN. To await such    */
/* a response, we call the DPIawait_packet_from_agent() function.    */
/* This function returns DPI_RC_OK (value zero) if success, otherwise*/
/* an appropriate DPI_RC_xxx error code as defined in snmp_dpi.h.    */
/*                                                                   */
/* The last step is to ensure that we indeed get a DPI-RESPONSE      */
/* back from the agent, and if we did then we must ensure the        */
/* agent indeed accepted us as a valid subagent. This is shown       */
/* by the error_code field in the DPI response packet.               */
/*                                                                   */
/* Return codes:   DPI_CONNECT_AND_OPEN_noError                      */
/*                 DPI_CONNECT_AND_OPEN_connectFailed                */
/*                 DPI_CONNECT_AND_OPEN_openFailed                   */
/*********************************************************************/
static int dpiConnectAndOpen(void)
{
   unsigned char *packet_p;
   int            rcdpi, rc;

#ifdef WIN32      /* LLR 09-12-95 added for WIN32 */
   struct in_addr ipaddr; /* $MED */
   WORD       wVersionRequested;
   WSADATA    wsaData;
   char       localbuf[HOST_NAME_BUFFER_LEN];
   int        err,
              len = HOST_NAME_BUFFER_LEN;
   LPHOSTENT  lpHostInfo;
#endif

#ifdef OS2
   struct in_addr ipaddr;
   char          *hostnameaddr;
#endif
   unsigned long  length;
   snmp_dpi_hdr  *hdr_p;
typedef int (LINKAGE _DPICONNTCP)(char *, char *);
typedef _DPICONNTCP *DPICONNTCP;
DPICONNTCP DpiConnTcp;

#ifdef OS2
   HMODULE ModuleHandle;
   BYTE LoadError[8];
#endif

   DMISA_TRACE_LOG(LEVEL2, "Attempting connection to local agent.");

#ifdef WIN32     /* LLR 09-12-95 added for WIN32 */

  if (!bWSAStartup) {
     bWSAStartup = TRUE;
     wVersionRequested = MAKEWORD(1,1);
     err = WSAStartup( wVersionRequested, &wsaData);
     if (err != 0 ) {

           DMISA_TRACE_LOG(LEVEL2, "Winsock initialization failed.");

           return DPI_CONNECT_AND_OPEN_getHostNameFailed;
     }
  }


   err = gethostname( localbuf, len);
   if (err != 0 ) {
      DMISA_TRACE_LOG(LEVEL2, "Gethostname did not succeed.");
         return DPI_CONNECT_AND_OPEN_getHostNameFailed;
   } else {
       DMISA_TRACE_LOG(LEVEL2, "Gethostname succeeded.");
   }

// LLR 09-21-95 We can get the following information for debug but all that
//                              was needed was the hostname that is in localbuf above.

   lpHostInfo = gethostbyname(localbuf);
   if (lpHostInfo == NULL ) {
      DMISA_TRACE_LOG(LEVEL2, "Gethostbyname did not succeed.");

         return DPI_CONNECT_AND_OPEN_getHostNameFailed;
   }

   DpiHandle = DPIconnect_to_agent_TCP(  /* (TCP) connect to agent  */
                   localbuf,             /* host name/address       */
                   Community);           /* snmp community name     */

#elif defined OS2

   DpiHandle = DPIconnect_to_agent_SHM(1);

   if (DpiHandle < 0) {

      rc=DosLoadModule(LoadError,8,"DPI20DLL",&ModuleHandle);
      if(rc==0)
      {
          rc=DosQueryProcAddr(ModuleHandle,0,"DPICONNECT_TO_AGENT_TCP",(PFN *)&DpiConnTcp);
          if (rc==0)
          {
             hostnameaddr = getenv ("HOSTNAME");
             if (hostnameaddr)
                strncpy(HostName, hostnameaddr, sizeof(HostName));
             else
                HostName[0] = 0;
             DpiHandle = DpiConnTcp(HostName, Community);
             if ((DpiHandle < 0) && (HostName[0])) {
                HostName[0] = 0;
                DpiHandle = DpiConnTcp(HostName, Community);
             }
          }
      }
   }
#else
   rcdpi = gethostname(HostName, HOST_NAME_BUFFER_LEN);
   if (rcdpi != 0) {
      DMISA_TRACE_LOG(LEVEL1,"Unable to get local host name.");
      return DPI_CONNECT_AND_OPEN_getHostNameFailed;
   }
/*CHANGE from _TCP to _UDP to suit Agent Relay */
   DpiHandle = DPIconnect_to_agent_UDP(  /* (UDP) connect to agent  */
                   HostName,             /* host name/address       */
                   Community);           /* snmp community name     */
#endif

   if (DpiHandle < 0)
      return DPI_CONNECT_AND_OPEN_connectFailed;

#ifndef SOLARIS2
   packet_p = mkDPIopen(              /* Make DPI-OPEN packet    */
                    DMI_SUBAGENT,     /* Our identification      */
                   "DMISA SubAgent",  /* description             */
                    DpiAgentWaitTimeout,  /* Overall DPI timeout */
                    MAX_VAR_BINDS_PER_PACKET, /* max varBinds/packet*/
                    DPI_NATIVE_CSET,  /* native character set    */
                    0L,                   /* password length         */
                    (unsigned char *)0);  /* ptr to password         */
   if (!packet_p) {
      DMISA_TRACE_LOG(LEVEL1,"Unable to build DPI OPEN packet.");
      return DPI_CONNECT_AND_OPEN_openFailed;
   }

   rcdpi  = DPIsend_packet_to_agent(      /* send OPEN packet        */
                DpiHandle,                /* on this connection      */
                packet_p,                 /* this is the packet      */
                DPI_PACKET_LEN(packet_p));/* and this is its length  */
   if (rcdpi != DPI_RC_OK) {
      DMISA_TRACE_LOG1(LEVEL1,"Unable to send DPI OPEN packet to SNMP agent, error code %d.", rcdpi);
      return DPI_CONNECT_AND_OPEN_openFailed;
   }

   rcdpi  = DPIawait_packet_from_agent(   /* wait for response       */
                DpiHandle,                /* on this connection      */
                300,                      /* timeout in seconds      */
                &packet_p,                /* receives ptr to packet  */
                &length);                 /* receives packet length  */
   if (rcdpi != DPI_RC_OK) {
      DMISA_TRACE_LOG1(LEVEL1,"Error in awaiting DPI OPEN packet from SNMP agent, error code %d.", rcdpi);
      return DPI_CONNECT_AND_OPEN_openFailed;
   }
   rc = DPI_CONNECT_AND_OPEN_noError;

   hdr_p = pDPIpacket(packet_p);          /* Parse the DPI packet    */
   if (hdr_p == snmp_dpi_hdr_NULL_p) {    /* Return if parse fails   */
      DMISA_TRACE_LOG(LEVEL1,"Parse of OPEN Response packet failed.");
      rc = DPI_CONNECT_AND_OPEN_openFailed;
   } else if (hdr_p->packet_type != SNMP_DPI_RESPONSE) {
      DMISA_TRACE_LOG(LEVEL1,"Packet received on OPEN request was not a Response packet.");
      rc = DPI_CONNECT_AND_OPEN_openFailed;
   } else if (hdr_p->data_u.resp_p->error_code != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG(LEVEL1,"Error code in Response packet indicates OPEN request failed.");
      rc = DPI_CONNECT_AND_OPEN_openFailed;
   }

   if (hdr_p) fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
   return rc;
#else
 if (!open_socket(&sock_in)) {
    /*DMISA_TRACE_LOG1(LEVEL1, "Unable to open/bind a socket error = %d",errno);*/
    return DPI_CONNECT_AND_OPEN_openFailed;
 }
 memset(&subagent, 0, sizeof(subagent)); /*Initialize all the subagent fields*/ 
 
 subagent.agent_id = DpiHandle;
 subagent.agent_status = SSA_OPER_STATUS_ACTIVE;
 subagent.name     = DMI_SUBAGENT;
 /*subagent.description =  "DMISA SubAgent"; Not Impl in lib */
 subagent.timeout     = DpiAgentWaitTimeout;
 /*subagent.max_var_binds = MAX_VAR_BINDS_PER_PACKET; Not Impl in lib */
 subagent.address       = sock_in;
 my_address             = sock_in;
 
 if (!subagent_register(&subagent))    {
      DMISA_TRACE_LOG(LEVEL1,"Error registering subagent with relay agent");
      return DPI_CONNECT_AND_OPEN_openFailed;
     }
 rc = DPI_CONNECT_AND_OPEN_noError;
 return rc;
#endif

}

/*********************************************************************/
/* Function:  dpiRegister()                                          */
/*                                                                   */
/* Description:                                                      */
/* After having set up a connection and identifying ourselves        */
/* to the agent,                                                     */
/*   o  attempt to register the Component MIB.                       */
/*   o  attempt to register all MIB subtrees present in XlateList    */
/*      provided it begins with 1.3.6.1.4.1.2.5.11.1.m, where m is a */
/*      positive integer other than 1, or 1,3,6,1,4,1,n, where n is  */
/*      a positive integer other than 2.                             */
/*      This forces all IBM MIBs to be hung under Architecture.DMI   */
/*      and all non-IBM MIBs to be hung under the Private leg.       */
/* Note that any given sub-tree may be registered with DPI even      */
/* when no associated component is installed in the DMI MIF database.*/
/*                                                                   */
/* If currently registered, an attempt to reregister will be         */
/* entered in the trace.                                             */
/*                                                                   */
/* Each registration results in a flag being set for future          */
/* unregistration.                                                   */
/*                                                                   */
/* If time out with DPI occurs, quadruple the timeout value and      */
/* re-register.  Lengthen this timeout value only once.              */
/*                                                                   */
/* Assumption: No duplicate OID prefixes reside in XlateList.        */
/*                                                                   */
/* Return codes:  DPI_REGISTER_allRegistered  - all attempted        */
/*                           registrations were successful           */
/*                DPI_REGISTER_someRegistered -some but not all      */
/*                           attempted registrations were successful */
/*                DPI_REGISTER_noneRegistered - none of attempted    */
/*                           registrations were successful           */
/*********************************************************************/
int dpiRegister(OidPrefix_t *xlatelist)
{
   OidPrefix_t *regoid;
/* char        *tempoid;                                              */
   int          someregistered = DMISA_FALSE, rc;
   int          somenotregistered = DMISA_FALSE;
   int          treeindex = 1;
#ifdef DMISA_TRACE
   int          ln;
#endif

   rc = dpiRegisterOne(CompMibOid, treeindex);  /* Register Component MIB        */
   if (rc == DPI_REGISTER_ONE_noError ||
       rc == DPI_REGISTER_ONE_alreadyRegistered ) {
      someregistered = DMISA_TRUE;
   } else {
      somenotregistered = DMISA_TRUE;
   }

/* Uncomment commented lines in this function if any OID prefixes may not end with a '.'*/
/* tempoid = (char *)NULL;  // initialize                             */
   regoid = xlatelist;
   while (regoid != NULL) {
      if ( (!strncmp(regoid->pOidPrefix,SUN_ARCH_DMI,
                    strlen(SUN_ARCH_DMI)) &&
            strncmp(regoid->pOidPrefix,SUN_ARCH_DMI_COMPMIB,
                    strlen(SUN_ARCH_DMI_COMPMIB))              ) ||
           (!strncmp(regoid->pOidPrefix,PRIVATE,strlen(PRIVATE)) &&
            strlen(regoid->pOidPrefix) > strlen(PRIVATE) &&
            regoid->pOidPrefix[strlen(PRIVATE)] != '2') ) {

/*       if (regoid->pOidPrefix[strlen(regoid->pOidPrefix) - 1] != '.'){*/
/*          tempoid = (char *)malloc(strlen(regoid->pOidPrefix) + 1); */
/*          strcpy(tempoid,regoid->pOidPrefix);                       */
/*          strcat(tempoid,".");  // Prefix must end with dot         */
                                             /* for registration      */
/*       }                                                            */
         regoid->regTreeIndex = ++treeindex; 
         rc = dpiRegisterOne(regoid->pOidPrefix,regoid->regTreeIndex);
         if (rc == DPI_REGISTER_ONE_noError ||
             rc == DPI_REGISTER_ONE_alreadyRegistered ) {
            regoid->iRegFlag = DMISA_TRUE;  /* should be fine if already registered*/
            someregistered = DMISA_TRUE;
         } else {
            somenotregistered = DMISA_TRUE;
#ifdef DMISA_TRACE
            ln = sprintf(logbuffer,"Unable to register OID prefix .");
            strncpy(logbuffer + ln, regoid->pOidPrefix,
                    (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
            DMISA_TRACE_LOGBUF(LEVEL1);
#endif
         }
/*       free((void *)tempoid);                                       */
      }
      regoid = regoid->pNextOidPre;
   }

   if (someregistered == DMISA_TRUE && somenotregistered == DMISA_TRUE) {
      Dpi_Registered_Flag = DMISA_TRUE;
      return DPI_REGISTER_someRegistered;
   }
   if (someregistered == DMISA_FALSE && somenotregistered == DMISA_TRUE) {
      Dpi_Registered_Flag = DMISA_FALSE;
      return DPI_REGISTER_noneRegistered;
   }

   Dpi_Registered_Flag = DMISA_TRUE;

   return DPI_REGISTER_allRegistered;
}

/*********************************************************************/
/* Function:  dpiRegisterOne()                                       */
/*                                                                   */
/* After we have setup a connection to the agent and after we have   */
/* identified ourselves, we must register one or more MIB subtrees   */
/* for which we want to be responsible (handle all SNMP requests).   */
/* To do so, the subagent must create a DPI-REGISTER packet and send */
/* it to the agent. The agent will then send a response to indicate  */
/* success or failure of the register request.                       */
/*                                                                   */
/* To create a DPI-REGISTER packet, the subagent uses a call to the  */
/* mkDPIregister() function.                                         */
/* The function returns a ptr to a static buffer holding the DPI     */
/* packet if success. If it fails, it returns a NULL ptr.            */
/*                                                                   */
/* Now we must send this DPI-REGISTER packet to the agent. We use    */
/* the DPIsend_packet_to_agent() function to do so, in a similar way */
/* as we did send the DPI_OPEN packet. Then we wait for a response   */
/* from the agent. Again, we use the DPIawait_packet_from_agent()    */
/* function in the same way as we awaited a response on the DPI-OPEN */
/* request. Once we have received the response, we must check the    */
/* return code to ensure that registration was successful.           */
/*                                                                   */
/* Return codes:   DPI_REGISTER_ONE_noError                          */
/*                 DPI_REGISTER_ONE_alreadyRegistered                */
/*                 DPI_REGISTER_ONE_makeFailed                       */
/*                 DPI_REGISTER_ONE_sendFailed                       */
/*                 DPI_REGISTER_ONE_awaitFailed                      */
/*                 DPI_REGISTER_ONE_parseFailed                      */
/*                 DPI_REGISTER_ONE_notResponse                      */
/*                 DPI_REGISTER_ONE_errorInResponse                  */
/*********************************************************************/
static int dpiRegisterOne(char *group_p, int treeindex)
{
   unsigned char *packet_p;
   int            rcdpi, rc;
   unsigned long  length;
   snmp_dpi_hdr  *hdr_p;
#ifdef DMISA_TRACE
   int            ln;
#endif

#ifndef SOLARIS2
   packet_p = mkDPIregister(         /* Make DPIregister packet */
                    DpiRegTimeout,   /* timeout in seconds, was 3, was variable */
                    0,               /* requested priority      */
                    group_p,         /* ptr to the subtree, as trailing dot*/
                    DPI_BULK_NO);    /* Map GetBulk into GetNext*/
   if (!packet_p) {
      DMISA_TRACE_LOG(LEVEL1,"Unable to build DPI REGISTER packet.");
      return DPI_REGISTER_ONE_makeFailed;
   }

   rcdpi  = DPIsend_packet_to_agent(       /* send REGISTER packet    */
                DpiHandle,                 /* on this connection      */
                packet_p,                  /* this is the packet      */
                DPI_PACKET_LEN(packet_p)); /* and this is its length  */
   if (rcdpi != DPI_RC_OK) {
      DMISA_TRACE_LOG1(LEVEL1,"Unable to send DPI REGISTER packet to SNMP agent, error code %d.", rcdpi);
      return DPI_REGISTER_ONE_sendFailed;
   }

   rcdpi  = DPIawait_packet_from_agent(   /* wait for response       */
                DpiHandle,                /* on this connection      */
                6,                        /* timeout in seconds      */
                &packet_p,                /* receives ptr to packet  */
                &length);                 /* receives packet length  */
   if (rcdpi != DPI_RC_OK) {
      DMISA_TRACE_LOG1(LEVEL1,"Error awaiting DPI REGISTER packet from SNMP agent, rc = %d.", rcdpi);
      return DPI_REGISTER_ONE_awaitFailed;
   }

   rc = DPI_REGISTER_ONE_noError;

   hdr_p = pDPIpacket(packet_p);          /* parse DPI packet        */
   if (hdr_p == snmp_dpi_hdr_NULL_p) {    /* If we fail to parse it  */
      DMISA_TRACE_LOG(LEVEL1,"Parse of REGISTER Response packet failed.");
      rc = DPI_REGISTER_ONE_parseFailed;
   } else if (hdr_p->packet_type != SNMP_DPI_RESPONSE) {
      DMISA_TRACE_LOG(LEVEL1,"Packet received on REGISTER request was not a Response packet.");
      rc = DPI_REGISTER_ONE_notResponse;
   } else if (hdr_p->data_u.resp_p->error_code == SNMP_ERROR_DPI_alreadyRegistered) {
      rc = DPI_REGISTER_ONE_alreadyRegistered;
   } else if (hdr_p->data_u.resp_p->error_code != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG1(LEVEL1,"Error in Response packet indicates REGISTER request failed, rc %d.", hdr_p->data_u.resp_p->error_code);
      rc = DPI_REGISTER_ONE_errorInResponse;
#ifdef DMISA_TRACE
   } else {   /* If registration successful, trace                    */
      ln = sprintf(logbuffer,"Registered with SNMP agent OID prefix ");
      strncpy(logbuffer + ln, group_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      DMISA_TRACE_LOGBUF(LEVEL1);
#endif
   }

   if (hdr_p) fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
   return rc;
#else
      if (!subtree_register(DpiHandle, group_p, treeindex, DpiRegTimeout,
                            0, DPI_BULK_NO, 0))    
         return DPI_REGISTER_ONE_errorInResponse;
      else
         return DPI_REGISTER_ONE_noError; 
#endif

}


/*********************************************************************/
/* Function:  dpiUnreg()                                             */
/*                                                                   */
/* Description:                                                      */
/*   An agent can send an UNREGISTER packet if some other subagent   */
/*   does a register for the same subtree at a higher priority.      */
/*   In this case we can decide to keep the connection open,         */
/*   we may regain control over the subtree if that higher priority  */
/*   registration goes away.                                         */
/*   An agent can also send an UNREGISTER if, for instance, an SNMP  */
/*   manager tells it to "invalidate" the subagent connection or the */
/*   registered subtree. In this case we decide to give up.          */
/*                                                                   */
/* Reason codes: DPI_UNREG_noError                                   */
/*               DMISA_dpiBroke                                      */
/*********************************************************************/
int dpiUnreg(OidPrefix_t *xlatelist)
{
   int  rcsub, rc = DPI_UNREG_noError;

   rcsub = dpiUnregOne(CompMibOid, 1);  /* unregister Component MIB      */
   if (rcsub != DPI_UNREG_ONE_noError) {
      DMISA_TRACE_LOG1(LEVEL1,"Return code from DPI Unregister  = %d.",rcsub);
      rc = DMISA_ERROR_dpiBroke;
   }

   while (xlatelist != NULL) {  /* unregister MIBs found in Service Layer*/
      if (xlatelist->iRegFlag == DMISA_TRUE) {
         rcsub = dpiUnregOne(xlatelist->pOidPrefix,xlatelist->regTreeIndex);
         xlatelist->iRegFlag = DMISA_FALSE;  /* even if unregister fails,*/
                                    /* assume we have become unregistered*/
         if (rcsub != DPI_UNREG_ONE_noError) {
            DMISA_TRACE_LOG1(LEVEL1,"Return code from DPI Unregister  = %d.",rcsub);
            rc = DMISA_ERROR_dpiBroke;
         }
      }
      xlatelist = xlatelist->pNextOidPre;
   }

   return rc;
}


/*********************************************************************/
/* Function: dpiUnregOne()                                           */
/*                                                                   */
/* Description:                                                      */
/* An agent can send an UNREGISTER packet if some other subagent does*/
/* a register for the same subtree at a higher priority. In this case*/
/* we can decide to keep the connection open, we may regain control  */
/* over the subtree if that higher priority registration goes away.  */
/* An agent can also send an UNREGISTER if, for instance, an SNMP    */
/* manager tells it to "invalidate" the subagent connection or the   */
/* registered subtree. In this case we decide to give up.            */
/*                                                                   */
/* Reason codes: DPI_UNREG_ONE_noError                               */
/*               DPI_UNREG_ONE_makePacketFailed                      */
/*               DPI_UNREG_ONE_sendFailed                            */
/*               DPI_UNREG_ONE_awaitFailed                           */
/*               DPI_UNREG_ONE_noPacketReceived                      */
/*               DPI_UNREG_ONE_notResponse                           */
/*               DPI_UNREG_ONE_unexpectedRC                          */
/*********************************************************************/
static int dpiUnregOne(char *group_p, int treeindex)
{
   unsigned char *packet_p;
   int            rcdpi, i;
   unsigned long  length;
   snmp_dpi_hdr  *hdr_p;
#ifdef DMISA_TRACE
   int            ln;
#endif

#ifndef SOLARIS2
   /* Grab packet up to three times so we clear out repeated request packets */
   /* from the SNMP agent, thus allowing the unregister packet to come through.*/
   i = 1;
   do {

      packet_p = mkDPIunregister(        /* make UNREGISTER packet  */
              SNMP_UNREGISTER_goingDown, /* UNREGISTER reason code  */
              group_p);                  /* oid prefix to unregister*/
      if (!packet_p) {
         DMISA_TRACE_LOG(LEVEL1,"Unable to build DPI UNREGISTER packet.");
         return DPI_UNREG_ONE_makePacketFailed;
      }

      rcdpi = DPIsend_packet_to_agent(   /* send UNREGISTER packet  */
              DpiHandle,                 /* on this connection      */
              packet_p,                  /* this is the packet      */
              DPI_PACKET_LEN(packet_p)); /* and this is its length  */
      if (rcdpi != DPI_RC_OK) {
#ifdef DMISA_TRACE
         ln = sprintf(logbuffer,"Unable to send DPI UNREGISTER packet to SNMP agent, rc = %d", rcdpi);
         strncpy(logbuffer + ln, group_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
         DMISA_TRACE_LOGBUF(LEVEL1);
#endif
         return DPI_UNREG_ONE_sendFailed;
      }

      rcdpi  = DPIawait_packet_from_agent( /* wait for response       */
              DpiHandle,                 /* on this connection      */
              DPI_UNREG_TIMEOUT,         /* timeout in seconds      */
              &packet_p,                 /* receives ptr to packet  */
              &length);                  /* receives packet length  */
      if (rcdpi != DPI_RC_OK) {
         DMISA_TRACE_LOG1(LEVEL1,"Error awaiting DPI UNREGISTER packet from SNMP agent, rc = %d.", rcdpi);
         return DPI_UNREG_ONE_awaitFailed;
      }

      hdr_p = pDPIpacket(packet_p);      /* parse DPI packet        */
      if (hdr_p == snmp_dpi_hdr_NULL_p) {
         DMISA_TRACE_LOG(LEVEL1,"Parse of UNREGISTER Response packet failed.");
         if (hdr_p) fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
         return DPI_UNREG_ONE_noPacketReceived;
      }
      if (hdr_p) fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
   } while (i++ <= 3 && hdr_p->packet_type != SNMP_DPI_RESPONSE);

   if (hdr_p->packet_type != SNMP_DPI_RESPONSE) {
      DMISA_TRACE_LOG(LEVEL1,"Packet received on UNREGISTER request was not a Response packet.");
      return DPI_UNREG_ONE_notResponse;
   }

   if (hdr_p->data_u.resp_p->error_code != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG1(LEVEL1,"Error in Response packet indicates UNREGISTER request failed, rc = %d.", hdr_p->data_u.resp_p->error_code);
      return DPI_UNREG_ONE_unexpectedRC;
   }

#ifdef DMISA_TRACE
   ln = sprintf(logbuffer,"Unregistered from SNMP agent OID prefix ");
   strncpy(logbuffer + ln, group_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
   DMISA_TRACE_LOGBUF(LEVEL1);
#endif

#else
      if (!subtree_unregister(DpiHandle, group_p, treeindex))  
          return DPI_UNREG_ONE_unexpectedRC;
#endif
   return DPI_UNREG_ONE_noError;

}


/*********************************************************************/
/* Function:  dpiUnregByAgent()                                      */
/*                                                                   */
/* Description:                                                      */
/*   Unregister the specified sub-tree.                              */
/*                                                                   */
/* Reason codes:  UNREG_BY_DPI_noError                               */
/*                UNREG_BY_DPI_failed                                */
/*********************************************************************/
static int dpiUnregByAgent( snmp_dpi_hdr *hdr_p,
                           snmp_dpi_ureg_packet *pack_p)
{
   unsigned char *packet_p = (unsigned char *)pack_p;
   snmp_dpi_hdr *hdr=hdr_p;
   int           rcdpi;

#ifndef SOLARIS2
#ifdef DMISA_TRACE
   int           ln;

   DMISA_TRACE_LOG1(LEVEL1, "DPI UNREGISTER received from agent, reason=%d\n", pack_p->reason_code);
   ln = sprintf(logbuffer,"     subtree=");
   strncpy(logbuffer + ln, pack_p->group_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
   DMISA_TRACE_LOGBUF(LEVEL1);
#endif

   packet_p = mkDPIresponse(            /* make RESPONSE packet       */
                hdr_p,                  /* ptr to packet to respond to*/
                SNMP_ERROR_DPI_noError, /* RESPONSE error code        */
                0,                      /* index to varBind in error  */
                NULL);                  /* ptr to varBinds, a chain   */

   if (!packet_p) return UNREG_BY_DPI_failed;

   rcdpi  = DPIsend_packet_to_agent(       /* send UNREGISTER packet  */
                DpiHandle,                 /* on this connection      */
                packet_p,                  /* this is the packet      */
                DPI_PACKET_LEN(packet_p)); /* and this is its length  */

   if (rcdpi != DPI_RC_OK) return UNREG_BY_DPI_failed;
#endif
/*For Solaris Agent Relay is not going to unregister. Hence we will never enter this code */
 
   return UNREG_BY_DPI_noError;
}


/*********************************************************************/
/* Function:  dpiClose()                                             */
/*                                                                   */
/* Description:                                                      */
/*   Close the connection.                                           */
/*                                                                   */
/* Reason codes:  DPI_CLOSE_noError                                  */
/*                DPI_CLOSE_failed                                   */
/*********************************************************************/
static int dpiClose(void)
{
#ifndef SOLARIS2
   unsigned char *pack_p;
   int            rc;


   pack_p = mkDPIclose(SNMP_CLOSE_goingDown);
   if (!pack_p) return DPI_CLOSE_failed;

   rc = DPIsend_packet_to_agent(       /* send UNREGISTER packet  */
           DpiHandle,                  /* on this connection      */
           pack_p,                     /* this is the packet      */
           DPI_PACKET_LEN(pack_p));    /* and this is its length  */

   if (rc != DPI_RC_OK) {
/*$MED DMISA_TRACE_LOG1(LEVEL1,"Error in sending DPI CLOSE packet to SNMP agent, error code %d.", rc); */
      return DPI_CLOSE_failed;
   }

#endif
/* For Solaris Agent Relay ,this case is covered by dpiDisconnect */ 
   return DPI_CLOSE_noError;
}


/*********************************************************************/
/* Function:  dpiCloseByAgent()                                      */
/*                                                                   */
/* Disconnect from DPI following:                                    */
/*  (1) receipt of DPI close,                                        */
/*  (2) any failed attempt to connect to DPI, or                     */
/*  (3) as recovery from any failure on a DPI function call          */
/*      (before the next connection attempt)                         */
/*                                                                   */
/* Reason code  -  DPI_CLOSE_BY_AGENT_dpiDown                        */
/*********************************************************************/
static int dpiCloseByAgent(snmp_dpi_hdr *hdr_p, snmp_dpi_close_packet *pack_p)
{
snmp_dpi_hdr *hdr=hdr_p;
snmp_dpi_close_packet *pack = pack_p;
#ifndef SOLARIS2
   DMISA_TRACE_LOG1(LEVEL2,"DPI CLOSE packet received from agent, reason=%d\n", pack_p->reason_code);

   DPIdisconnect_from_agent(DpiHandle);
#endif
/*For Solaris Agent will never close, hence this path is redundant */
   return DPI_CLOSE_BY_AGENT_dpiDown;
} /* end of dpiCloseByAgent() */


/*********************************************************************/
/* Function:  dpiDisconnect()                                        */
/*                                                                   */
/* Description:                                                      */
/*   Disconnect from the SNMP agent, from TCP.                       */
/*                                                                   */
/* Reason codes:  none                                               */
/*********************************************************************/
static void dpiDisconnect(void)
{
#ifndef SOLARIS2
   DPIdisconnect_from_agent(DpiHandle);
#else
   subagent_unregister(&subagent);
#endif
}


/**********************************************************************/
/* Function:  exitRoutine()                                           */
/*                                                                    */
/* Unregister from DPI & DMI. Clean up resources owned by this process*/
/*   NOTE: It is possible the calls in this function may fail,        */
/*         in which case we may never return, preventing the process  */
/*         from being normally closed                                 */
/*   Error conditions:                                                */
/*   1. When queue entry length != 12, return NULL in pReqBuf pointer */
/***********************************************************************/
void APIENTRY exitRoutine(void)
{
   int rc;

   ExceptionProcFlag = DMISA_TRUE;

   DMISA_TRACE_LOG(LEVEL4, "Entered exit routine.");

#ifdef DMISA_TRACE
   traceFlush();  /* Do this first, in case we never return from unregistering.*/
#endif

/* Next 4 lines commented out due to DPI not being able to receive a send_packet while*/
/*   waiting forever on await_packet.                                 */
/* if (state >= DMI_XLATE_DPIREG) {                                   */
/*    rc = dpiUnreg(XlateList);                                       */
/*    state = DMI_XLATE_DPIOPEN;                                      */
/* }                                                                  */
   if (state >= DMI_XLATE_DPIOPEN) {
      dpiClose();
      state = DMI_XLATE_DPICONN;
   }
   if (state >= DMI_XLATE_DPICONN) {
      dpiDisconnect();
/*    if (DMISA_ERROR_dmiBroke == rc) state = DMI_XLATE;  // I mean rc from main()*/
   }

   if (KeyValueList) freeKeyValueList();   /* Free allocated storage for GETNEXT*/
   if (XlateList) freeXlate();             /* Free allocated storage for SNMP-DMI translation*/
   if (ReserveList) setCleanReserveList(DMISA_IMMEDIATE); /* Release any reserved attributes*/
                                                          /* and free associated storage*/
   if (state >= DMI_XLATE) {
      if (DMI_UNREGISTER_noError != (rc = dmiUnregister(ExceptionProcFlag,
                                                        ++DmiReqHandle))) {
         DMISA_TRACE_LOG1(LEVEL1,"Unregister from DMI service layer failed. rc = %d.", rc);
      } else DMISA_TRACE_LOG(LEVEL2,"DMI unregistered.");
      state = DMISA_NONE;
   }

   free(ResponseBuffer);  /* Used for DMI Registration response       */
   doSemCleanup();        /* uncommented 950221.gwl                   */
#ifdef DMISA_TRACE
   traceFlush();  /* Do it again to catch these last few messages     */
   doSemTraceCleanup();        /* uncommented 950221.gwl              */
#endif

    DMI_SLEEP(2000);  /* pause                                       */

#ifdef __DEBUG_ALLOC__
   _dump_allocated(16);
#endif

#ifdef OS2
   DosExitList(EXLST_EXIT,          /* Function request code/order    */
               (PFNEXITLIST) exitRoutine);     /* Address of routine  */
#endif

} /* end of exitRoutine() */

/*********************************************************************/
int dpiAreYouThere()
{
#ifndef SOLARIS2
   unsigned char *packet_p;
   int            rcdpi;
   ULONG          length;
   snmp_dpi_hdr  *hdr_p;

   packet_p = mkDPIayt();
   if (!packet_p) {
      DMISA_TRACE_LOG(LEVEL1,"Unable to build DPI (are you there) packet.");
      return(1);
   }

   rcdpi  = DPIsend_packet_to_agent(      /* send ARE YOU THERE      */
                DpiHandle,                /* on this connection      */
                packet_p,                 /* this is the packet      */
                DPI_PACKET_LEN(packet_p));/* and this is its length  */

   if (rcdpi != DPI_RC_OK) {
      DMISA_TRACE_LOG1(LEVEL1,"Unable to send DPI AYT packet to SNMP agent, error code %d.", rcdpi);
      return(1);
   }

   while(DMISA_TRUE) {
      rcdpi  = DPIawait_packet_from_agent(   /* wait for response       */
                   DpiHandle,                /* on this connection      */
                   300,                      /* timeout in seconds      */
                   &packet_p,                /* receives ptr to packet  */
                   &length);                 /* receives packet length  */
      if (rcdpi != DPI_RC_OK) {
         DMISA_TRACE_LOG1(LEVEL1,"Error in awaiting DPI AYT packet from SNMP agent, error code %d.", rcdpi);
         return(1);
      }


      hdr_p = pDPIpacket(packet_p);          /* Parse the DPI packet    */
      if (hdr_p == snmp_dpi_hdr_NULL_p) {    /* Return if parse fails   */
         DMISA_TRACE_LOG(LEVEL1,"Parse of AYT Response packet failed.");
         return(1);
      }

      switch(hdr_p->packet_type) {   /* Handle by DPI type      */
         case SNMP_DPI_RESPONSE:
              if (hdr_p->data_u.resp_p->error_code == SNMP_ERROR_DPI_noError) {
                 DMISA_TRACE_LOG(LEVEL1,"DPI was there!  Back to normal...");
                 fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
                 return(0);
              }
              else {
                 DMISA_TRACE_LOG1(LEVEL1,"Error code in AYT Response packet, rc = %X",hdr_p->data_u.resp_p->error_code);

                 fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
                 return(1);
              }
              break;
         default:
              DMISA_TRACE_LOG1(LEVEL1,"Didn't get AYT response, got %d instead",hdr_p->packet_type);

              packet_p = mkDPIresponse(            /* Make DPIresponse packet */
                      hdr_p,                       /* ptr parsed request      */
                      SNMP_ERROR_resourceUnavailable,/* error code              */
                      0,                           /* index of object in error*/
                      NULL);                       /* varBind response data   */
              if (!packet_p) {
                 DMISA_TRACE_LOG(LEVEL1,"couldn't make response packet");
                 fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
                 return(1);
              }

              rcdpi = DPIsend_packet_to_agent(     /* send OPEN packet        */
                      DpiHandle,                   /* on this connection      */
                      packet_p,                    /* this is the packet      */
                      DPI_PACKET_LEN(packet_p));    /* and this is its length  */
              if (rcdpi != SNMP_ERROR_DPI_noError) {
                 DMISA_TRACE_LOG(LEVEL1,"DPISend_packet_to_agent failed.");
                 fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
                 return(1);
              }
              DMISA_TRACE_LOG(LEVEL1,"Handled queued frame correctly.");
              fDPIparse(hdr_p);  /* Free the parsed DPI packet    */
      } /* endswitch */
   } /* end while */
#else

       if (!agent_alive())
           return 1;
       else
           return 0;
#endif
} /* end dpiAreYouThere */

int register_callback_rpc() {
  SVCXPRT *transport;
  char mname[FMNAMESZ+1];

  if (!ioctl(0, I_LOOK, mname) &&
                (!strcmp(mname, "sockmod") || !strcmp(mname, "timod")))
    {
      transport = svc_tli_create (0, (struct netconfig *)NULL,
                                    (struct t_bind *)NULL, 0, 0);
      if ( transport == NULL )
          return FALSE;
       if (!svc_reg(transport, MAPPER_PROGNUM, MAPPER_VERSNUM, dmi2_client_0x1,
                         (struct netconfig *)NULL))
          return FALSE;
     }
   else
     {
       if (!svc_create(dmi2_client_0x1, MAPPER_PROGNUM, MAPPER_VERSNUM, "netpath"))
                return FALSE;
     }

  return TRUE;

}

void make_daemon() {

struct rlimit  rlp;
int maxopenfiles, i;
pid_t  newval;

   getrlimit(RLIMIT_NOFILE, &rlp); /*Max open file descriptors */
   maxopenfiles = (int) rlp.rlim_cur;

   /*close all open file descriptors */
   for(i = 0; i < maxopenfiles; i++)
            close(i);

   /*change CWD to root */        
   chdir("/");

   /*File creation mask */
   umask(0);

   /*Fork the child */
   if (fork())      /*Parent should exit*/
       exit(0);

   /*Dissassociate form Process Group */ 
   newval = setpgrp();

   signal(SIGHUP, SIG_IGN); /*immune from pgrp leader death*/
 
   /*Background tty write attempted */ 
   signal(SIGTTOU, SIG_IGN);
   signal(SIGTTIN, SIG_IGN);
   signal(SIGTSTP, SIG_IGN);

   /*Ignore death of child*/
   signal(SIGCLD, SIG_IGN);

   /*openlog("snmpXdmid", LOG_CONS, LOG_DAEMON );*/
}

int  Initconfig() {
int  permloop=1;
char *tmpptr;
char filename[500];
char line[300];
char key[100];
char val[300];
char tmpwarntime[30], tmpexptime[30];
int  tmpfailthresh=0;
int  tmptrapforward=-1;
FILE *fp;
uid_t  euid;

   if (TraceLevel == 0)
      openlog("snmpXdmid", LOG_CONS, LOG_DAEMON);

   euid = geteuid();
   if (euid != 0) {
      DMISA_TRACE_LOG1(LEVEL1,"Not running with root permission. Current euid = %ld.", euid); 
      exit(1);
   }

   while(permloop) {
     sprintf(filename,"%s/snmpXdmid.conf",configdir); 
     if (!(fp = fopen(filename, "r"))) {
         if (configdir == default_configdir) {
          DMISA_TRACE_LOG(LEVEL1, "Error opening default config. file, assuming default config. values\n");
              return 0;
         }
         DMISA_TRACE_LOG(LEVEL1, "Error opening User config. file,opening default config file\n");
         configdir = default_configdir;
         memset(filename, 0, sizeof(filename));
         continue;
     }
     if (fp == NULL) {
         configdir = default_configdir;
         return 0;
     }
     else 
         break;
   }

   while(fgets(line, sizeof(line), fp) != NULL) {
      tmpptr = line;
      while((*tmpptr) && (*tmpptr <= ' ')) 
                tmpptr++;
      if (*tmpptr == '#')
             continue; 
      sscanf(line,"%s = %s", key , val);
      if (!strcmp(key, "WARNING_TIMESTAMP"))
          strcpy(tmpwarntime , val);
      if (!strcmp(key, "EXPIRATION_TIMESTAMP"))
          strcpy(tmpexptime, val);           
      if (!strcmp(key, "FAILURE_THRESHOLD"))
           tmpfailthresh = atoi(val); 
      if (!strcmp(key, "TRAP_FORWARD_TO_MAGENT"))
           tmptrapforward = atoi(val);
   }
   fclose(fp);

   /*validate warntime*/
     if ((strlen(tmpwarntime) <= sizeof(DmiTimestamp_t)) &&
         (tmpwarntime[14] == '.') && 
         ((tmpwarntime[21] == '+') || (tmpwarntime[21] == '-'))) {
         sub_warntime = calloc(1, sizeof(DmiTimestamp_t)+1);
         strcpy(sub_warntime, tmpwarntime);
         memset(&sub_warntime[25], ' ', 3);
         sub_warntime[28] = 0; 
     }

   /*validate exp time*/
     if ((strlen(tmpexptime) <= sizeof(DmiTimestamp_t)) &&
         (tmpexptime[14] == '.') && 
         ((tmpexptime[21] == '+') || (tmpexptime[21] == '-'))) {
         sub_exptime = calloc(1, sizeof(DmiTimestamp_t)+1);
         strcpy(sub_exptime, tmpexptime);
         memset(&sub_exptime[25], ' ', 3);
         sub_exptime[28] = 0; 
     }

     if (tmpfailthresh > 0)
       failure_threshold = tmpfailthresh; 

     if (tmptrapforward != -1)
       trap_forward_to_magent = tmptrapforward;
     return 0;
} 


/* I wanted to replace DMI_Registered & DPI_Registered flags with the existing state machine */
/* I wanted to expand this sub-agent to handle multiple var-binds per PDU */
/* Once Bert fixes the DPI to return the tooBig condition, incorporate it into this code */
/* I wanted to get rid of duplication of traced lines, only occuring when written to screen */
/* I wanted to successfully unregister all OIDs */
/* I wanted to understand whether there were any more things I assumed the agent checked, */
/*   when in fact, it didn't */
/* I wanted to check that all DMI errors meant what I thought they meant, and that this code */
/*   reacted appropriately */
/* TEMPORARY CHANGE: PUT EVERY TRACE ENTRY IMMEDIATELY OUT TO FILE */
