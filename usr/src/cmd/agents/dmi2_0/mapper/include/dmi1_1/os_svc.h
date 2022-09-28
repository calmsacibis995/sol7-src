/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_svc.h	1.3 96/10/02 Sun Microsystems"

/********************************************************************************************************

    Filename: os_svc.h


    Description: Include Information for all the porting concerns

*********************************************************************************************************/

#ifndef SVC_H_FILE
#define SVC_H_FILE
 
#include <sys/types.h>

#include "os_dmi.h"
#include "psl_om.h"
#include "psl_dh.h"
#include "pr_err.h"

#include <errno.h>
#include <thread.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "error.h"

#include "os_sys.h"     /* pull in the trace vector information              */
#include "os_skt.h"
#include "os_lib.h"

#define DMIPATH "/home/yiru/sva/dmisl/bin/dmisl"
#define DITIMEOUT 20
#define OS_SL_COMPID "5765-622"

#define OVERLAY_PATH "/home/yiru/sva/overlay"

#define BIF_SDK_CODE   "bif.c"
#define BIF_SDK_HEADER "bif.h"
#define BIF_CODE_GENERATOR_HEADER "biftodmi.h"

typedef union _dwh {
   char pChar[1];
   char *string;
   int  Int;
   int  *pInt;
   DataStr RcvData;
   DataPtr pRcvData;
   DMI_MgmtCommand_t Cmd;
   DMI_MgmtCommand_t *pCmd;
   MH_Registry_t Registry;
   MH_Registry_t *pRegistry;
   } DWH, *pDWH;

DWH dwh;

#define THREADSCHEDULER SCHED_RR
#define THREADMAXPRIORITY PRI_RR_MAX
#define THREADNORMALPRIORITY (PRI_RR_MAX + PRI_RR_MIN) / 2

ULONG           OS_init(void *InitData);       /* service layer initialization function */
ULONG           OS_term(void);                 /* service layer termination function */
ULONG           OS_StartSL(void);              /* starts the service layer, after initialization */

void os_critSecInit();

void            OS_getTime(DMI_TimeStamp_t _FAR *ts);

DMI_UNSIGNED    OS_LoadOverlay(OM_OverlayTable_t _FAR *NewOvl);   /* ask the OS dependant code to load up the overlay */
DMI_UNSIGNED    OS_OverlayCall(DMI_UNSIGNED CallType,OM_OverlayTable_t *Overlay,DMI_MgmtCommand_t *dmiCommand); /* call type is either OM_RUN or OM_CANCEL */
void *DITimerThread(void *junk);
void            OS_UnloadOverlay(OM_OverlayTable_t *Overlay);   /* ask the OS specific code to unload an overlay */

void            OS_unregisterApp(MH_Registry_t *Reg);
void            OS_unregisterDirect(CI_Registry_t *Reg);

void            OS_callback(DMI_MgmtCommand_t *dmiCommand,MH_Registry_t *Application);
void            OS_Indicate(DMI_Indicate_t *Indicate,MH_Registry_t* Application);
void            OS_EventResp(DMI_FUNC3_OUT responseFunc,void *osContext);
DMI_UNSIGNED    OS_CallParent(DMI_MgmtCommand_t *dmiCommand,MH_Registry_t *reg,DMI_FUNC3_OUT MH_childCallback);
void            OS_InstallNotice(char *Message,void *OS_Context,DMI_MgmtCommand_t *dmiCommand);   /* ask the OS code to tell the user about this error */

DMI_UNSIGNED    OS_MIFbuftoFile(char *mifFile,char *buffer,ULONG length);
void            OS_BackupMifFile(char *mifFile);   /* copy the newly installed MIF file to the backup dir */
void            OS_GetParserMessage(PR_ErrorNumber_t errorNumber,unsigned long line,
                    unsigned long col,char *buffer,unsigned long length);

int os_processSocketMessage(DataPtr RcvData);
void os_CleanupRegistry(int Socket);  /* clean up the Registry     */
#if 0
int os_FindSocket(int pid);  /* gets the socket for a given pid */
#endif
#endif
