/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisatrp.c	1.12 96/10/07 Sun Microsystems"

/***   START OF SPECIFICATIONS   ******************************************/
/*                                                                        */
/* Source File Name       : DMISATRP.c                                    */
/*                                                                        */
/* Module  Name           : GENTTRAP                                      */
/*                                                                        */
/* Program Name           : DMISA.EXE                                     */
/*                                                                        */
/* IBM Confidential (IBM Confidential-Restricted                          */
/* when combined with the Aggregated OCO Source                           */
/* Programs/Modules for this Product)                                     */
/*                                                                        */
/* OCO Source Materials                                                   */
/*                                                                        */
/* The Source code for this program is not published or otherwise         */
/* divested of its trade secrets, irrespective of what has been           */
/* deposited with the U.S. Copyright Office                               */
/*                                                                        */
/* 5763SS1  (C) Copyright IBM Corp. 1994                                  */
/*                                                                        */
/* NOTE:                                                                  */
/*  1994 is the original year the source code is written.                 */
/*  1994 is the year any changes to the source code is made.              */
/*                                                                        */
/*                                                                        */
/* Source File Description:                                               */
/*                                                                        */
/* This module contains the routine used by the Client Agent programs     */
/*                                                                        */
/* Function List(exported)                                                */
/*                                                                        */
/*                                                                        */
/* Change Activity:                                                       */
/*                                                                        */
/* CFD list:                                                              */
/* Flag&Reason  Rls  Date   Pgmr      Description                         */
/* ------------ ---- ------ --------  ------------------------------------*/
/*                   940502 MLESKA    New code for PSWI Client Agent to   */
/*                                    parse DMI Indication and generate a */
/*                                    trap.                               */
/*                                                                        */
/*                   940620 DGEISER   Complete the New Code               */
/*                   950317 LAUBLI    Add check to malloc return          */
/* End CFD List.                                                          */
/* Additional Notes About the Change Activity                             */
/* End Change Activity.                                                   */
/***   END OF SPECIFICATIONS   ********************************************/

/**********************************************************************/
/* Includes                                                           */
/**********************************************************************/

#include "gentrap.h"
#include "dmisa.h"

#ifdef SOLARIS2
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pdu.h>
#include <trap.h>
#include <pagent.h>
#include <dpitosar.h>
extern char error_label[1000];
extern int trap_forward_to_magent;
#endif
/**********************************************************************/
/* Global variables                                                   */
/**********************************************************************/

   snmp_dpi_set_packet    *varbind_p;
   Trap_In_ptr             dmi_trap_ptr;

   DMI_Indicate_t       *Indicate_p;
   void                 *IndicateEnd_p;          /* pointer to the end of the buffer */
   void                 *Indicate_ptr;
   char                 *Indicate_p_char;      /* ptr to void for pointer arith */
   extern char  logbuffer[];
#ifdef DMISA_TRACE
   extern unsigned char  logbuffer[];
#endif
   int                    notify_value;           /* value to set CA event type attrubute if problem record is sent */

/**********/
   DMI_ListComponentCnf_t *pdmicnfLC;          /* ptr to install/uninstall data*/
   DMI_ListAttributeCnf_t *pdmicnfLA;
   DMI_ListAttributeReq_t *pdmireqLA;
   DMI_GetAttributeCnf_t  *pdmicnfGA;
   DMI_GetAttributeReq_t  *pdmireqGA;
   DMI_GetRowReq_t        *pdmireqGR;
   DMI_GetRowCnf_t        *pdmicnfGR;
   DmiString_t             *work;
   DmiListAttributesOUT   *listattrout;
   DmiAttributeInfo_t     *attrinfo;

   ULONG                   desiredcomp,desiredgroup,desiredattribute;
   ULONG                   dmierror;

typedef BYTE INSTALL_OID [7];
INSTALL_OID aInstallOid[ ] =
  {
     "1.1.1.",
     "1.1.2.",
     "1.1.3.",
     "1.1.4.",
     "1.1.5."
  };

   Thr_Param                thr_arg;
/**********************************************************************/
/* Internal function prototypes                                                            */
/**********************************************************************/

int generateTrap( void     *trap_ptr);
int findSoftware(void *pIndicate);
int buildVarbData(int event_type,void *argp);
int handleEventInd( void *);
int handleUnInstallInd( void *);
int handleStartTrap(int type);
int checkCAevent ( void *, DMI_GetRowCnf_t **, ULONG *, ULONG *, ULONG *);
int checkCAeventType (DMI_ListAttributeCnf_t  *, DMI_GetAttributeCnf_t *);
int setCAeventType (int notify, ULONG compi, ULONG groupi, ULONG attributei);

#ifndef OS2
#define strnicmp(p1,p2,p3) strcmp(p1,p2) /* verify this $MED */
#endif

/*====================================================================*/
/* Code                                                                                     */
/*====================================================================*/



/* Function Specificaton **********************************************/
/*                                                                    */
/*  Name:  parseDmiIndicate                                     */
/*                                                                    */
/*  Description:  This function parses the DMIIndication and     */
/*                 based upon its contents, builds a trap-like    */
/*                 structure, which is forwarded to the gentrap  */
/*                 routine.                                          */
/*                                                                    */
/*  Input:  pointer to indication block                                        */
/*                                                                    */
/*  Output:  Status                               */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:  Exceptions resignaled to caller                      */
/*                                                                    */
/* End Function Specification *****************************************/



#if 0
This part to be removed after testing
int   parseDmiIndicate( DMI_Indicate_t   *Indicate_ptr2)
{

   int rc=0;
/*============================================================*/
/* Code for parsedmiIndicate                                                 */
/*============================================================*/

 Indicate_p = (DMI_Indicate_t  *)Indicate_ptr2;


 if (Indicate_p->iLevelCheck != DMI_LEVEL_CHECK )  /* check the indication lvl */
   {
    DMISA_TRACE_LOG1(LEVEL1,"Incorrect DMI level, the level received was %x\n", Indicate_p->iLevelCheck);
     return(Parse_DmiLvl_Err);
   }


 if (Indicate_p->pResponseFunc != NULL)    /* chk for illegal ptr to respfunc */
  {
   DMISA_TRACE_LOG1(LEVEL1,"Indicate Response pointer = %x , should be NULL.\n", Indicate_p->pResponseFunc);
   return Parse_Rtnfunc_Err;
  }


 if (Indicate_p->oIndicationData > Indicate_p->iCmdLen)    /* chk if the offset is bigger than the buffer */
  {
   DMISA_TRACE_LOG1(LEVEL1,"Indication Data  Offset is = %x , greater than buffer length.\n", Indicate_p->oIndicationData);
   return Parse_Rtnfunc_Err;
  }

/*****************************************************************/
/* Some of these values, like the varbinds will be obtainable in the structures  */
/* of the DMI Indicate message.  Others like Community  and Subagent OID       */
/* are constants.                                                                                     */
/* Other data that is required for a trap, such as timestamps, and IP address   */
/* of originating system, will be filled in by the Agent.   >>>                          */
/*****************************************************************/
   DMISA_TRACE_LOG1(LEVEL1,"DMI Indicate Type = %d .\n", Indicate_p->iIndicationType);
   DMISA_TRACE_LOGBUF(LEVEL1);

 rc = Parse_NoErr;                                        /* initialize rc to no err*/

 switch (Indicate_p->iIndicationType) {     /* What type of indication is it?*/
    case DMI_UNINSTALL_INDICATION:
    case DMI_INSTALL_INDICATION:
    case DMI_EVENT_INDICATION:
      dmi_trap_ptr = (Trap_In *) malloc (sizeof (Trap_In));
      if (!dmi_trap_ptr) {   /* did we get it                                                         */
          DMISA_TRACE_LOG(LEVEL1,"Out of Memory, Cannot build a Trap.\n");
          return(Parse_Memory_Err);
      } /* end if */
      break;
   default:
      DMISA_TRACE_LOG1(LEVEL1,"Invalid Indication Type %d\n", Indicate_p->iIndicationType);
      return(Parse_Invindic_Err);
 }

 strcpy(dmi_trap_ptr->community, DEFAULT_COMMUNITY);     /* set community name */
 strcpy(dmi_trap_ptr->enterprise, DMI_SUBAGENT);    /* set the enterprise oid*/
 dmi_trap_ptr->trap_generic = Ca_Trap;             /* set the Specific trap type*/
 varbind_p = snmp_dpi_set_packet_NULL_p;   /* init vb chain to NULL pointer */
 IndicateEnd_p = Indicate_p + Indicate_p->iCmdLen;   /* determine end of the ind buffer for use later  */
 switch (Indicate_p->iIndicationType) {     /* What type of indication is it?*/

    case DMI_UNINSTALL_INDICATION:
    case DMI_INSTALL_INDICATION:
                                                      /* For Install or Uninstall */
       rc = handleUnInstallInd(Indicate_p);
       break;

    case DMI_EVENT_INDICATION:
                                                  /* for a DMI Event indication */
       rc = handleEventInd(Indicate_p);
       break;

    default:
       return(Parse_Invindic_Err);     /* The default is an Invalid Indicate from DMI */
/*     break;  // Note this line of code is commented out             */
     } /* endswitch */

     free(dmi_trap_ptr);
     return (rc);
} /* end function */
#endif

/* Function Specificaton **********************************************/
/*                                                                    */
/*  Name:  handleStartTrap                                 */
/*                                                                    */
/*  Description:                                                  */
/* Function to send out a coldstart or warmstart trap.   */
/*                                                                    */
/*  Input:  Indicator for cold or wram start Trap     */
/*                                                                    */
/*  Output:  Return Code                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */

/*****************************************************************/

int  handleStartTrap( int type)
{
   int rc;

   dmi_trap_ptr = (Trap_In_ptr)calloc(1, sizeof(Trap_In));  /* &dmi_trap;                   ** pointer to trap data */
   if (!dmi_trap_ptr) rc = 100;  /* added 950317.gwl                                     */
   else {
      dmi_trap_ptr->trap_generic = SNMP_Trap_ColdStart;                 /* set cold start trap */
      if (type == DMISA_REQ_WARMSTART) {
          dmi_trap_ptr->trap_generic = SNMP_Trap_WarmStart;           /* set to  warm start */
      } /* endif */

      dmi_trap_ptr->trap_specific = DMI_SpecTrap_Zero;                     /* set specific to zero */
      strcpy (dmi_trap_ptr->community, DEFAULT_COMMUNITY);          /* set the community */
      strcpy (dmi_trap_ptr->enterprise, DMI_SUBAGENT);                    /* set the eterp. ID    */
      strcpy (dmi_trap_ptr->varBinds_a[0].vb_oid,DMI_SUBAGENT);                  /* set complete data OID */
      strcpy (dmi_trap_ptr->varBinds_a[0].vb_inst,".2000"/*".1.1.1"*/);                   /* set the instance to 1.1.1 */
      dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_OCTET_STRING; /* set OID type          */
      strcpy (dmi_trap_ptr->varBinds_a[0].data.vb_data,DMI_SUBAGENT_NAME); /* set the data to the name */
      dmi_trap_ptr->varBinds_a[0].vb_datalen = strlen(DMI_SUBAGENT_NAME);
      dmi_trap_ptr->varBind_count = 1;                                          /* number of varbinds to 1 */
      rc= generateTrap(dmi_trap_ptr);                                            /* go generate the trap*/

      free(dmi_trap_ptr);
   }
   return (rc);
}

/* Function Specificaton **********************************************/
/*                                                                                     */
/*  Name:  handleEventIndication                                                  */
/*                                                                                     */
/*  Description:  This function handles an Event Indication, checking              */
/*                 to make sure the correct data is in place before                  */
/*                 generating a trap.                                                  */
/*                                                                                     */
/*  Input:  pointer to Indiciation data,                                              */
/*                                                                                     */
/*  Output:  rc for successful or unsuccessful indiation                             */
/*                                                                                     */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:  Exceptions resignaled to caller                      */
/*                                                                    */
/*--pointers-----------Data Blocks ---------------------------------------------------*/
/* event_ptr ---->   DmiEventData                                                                                */
/*                                iClassCount                                                                              */
/* eventclass_ptr----->  DmiClassData[]                                                                         */
/*                                     iComponentId                                                                      */
/* eventnametr --------->  osClassString                                                                      */
/*                                     iRowCount                                                                         */
/* rowlist_ptr------------> DmiGetRowCnf[]                                                                  */
/*                                         iGroupId                                                                         */
/*                                         iGroupId                                                                         */
/*                                         oGroupKeyList                                                                */
/*                                         iAttributeCount                                                               */
/* attribute_ptr------------>   DmiGetAttributeCnf[]                                                       */
/*                                               iGroupId                                                                    */
/*                                               iAttributeId                                                                */
/*                                               iAttributeType                                                            */
/*                                               oAttributeValue                                                          */
/*                                                                                                                               */
/* End Function Specification *******************************************************/
#if 0

int   handleEventInd( void *indication_p)
{
This is an old function now defunt for DMI2.0 implementation.
Will be cleaned up after testing.
       DMI_GetRowCnf_t *rowlist_ptr;         /* ptr to DMI Event data  */
       int   rc=0;
       DMI_EventData_t        *event_ptr;          /* ptr to DMI Event data  */
       ULONG compi, groupi, attributei;

       dmi_trap_ptr->trap_specific = DMI_SpecTrap_Event;
                                       /* set specific trap type to event type */
                                                                              /* get pointer to Indication data (Event ) */
       event_ptr = (DMI_EventData_t *)((char *)Indicate_p +  Indicate_p->oIndicationData);
       if (Indicate_p->iComponentId == 0 ) {
           DMISA_TRACE_LOG(LEVEL1,"Indication call has a component Id = 0 \n");
        } /* endif  */
              compi = Indicate_p->iComponentId;      /* localize the component id */
              rc = checkCAevent(event_ptr, &rowlist_ptr, &compi, &groupi, &attributei);

              if (rc == CA_EVENT_OK) {
                   rc = buildVarbData(Indicate_p, event_ptr, rowlist_ptr,
                                      rowlist_ptr->iAttributeCount,
                                      compi, groupi);
                   if (rc == Varbind_build_ok) {
                        rc = generateTrap(dmi_trap_ptr);            /* gen trap */
                        if (rc == 0 && notify_value != 0) {            /*  if trap sent and CAevent value is not zero */
                           /* than go set the attribute to the value */
                           rc = setCAeventType  (notify_value, compi, groupi, attributei);
                        } /* endif */
                   } /* end if check for good varbind list */
              } /* end if for checkCAevent */
         return (rc);
} /* end function */
#endif

/* Function Specificaton **********************************************/
/*                                                                    */
/*  Name:  generateTrap                                          */
/*                                                                    */
/*  Description:  This function generates a trap based on   */
/*                a DPI packet that will be used to forward the trap  */
/*                via the DPI 2.0 interface with the SNMP agent.          */
/*                                                                    */
/*  Input:  Trap structure                                            */
/*                                                                    */
/*  Output:  Trap forwarded to trap managers                          */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:  Exceptions resignaled to caller                      */
/*                                                                    */
/* End Function Specification *****************************************/
int  generateTrap( void     *trap_ptr)      /* ptr to trap structure   */
{

   union vardata {
             char        val_data[MaxNumVbchars];            /* -  data (char string) */
             Trap_Int64  val_u64;                  /* -  data 64b int      */
             Trap_Int32  val_u32;                  /*    -  data 32b int   */
             Trap_U_Int32  val_uu32;              /*    -  data 32b int   */
          };
   int handle;
   int varbind_index;
   static int snd_rc;                  /* rtn code from DPI send call */
   unsigned char  *trap_packet_p;      /* pointer to trp on call      */
   int trap_data_type;                 /* trap data type              */
   int trap_data_size;                 /* size of trap data           */
   char dpi_oid[256];        /* Object ID for DPI           */
   char dpi_oid_inst[256];    /* Instanace of the OID      */
   snmp_dpi_set_packet *dpi_varBind_p = /* ptr to dpi varbind         */
                   snmp_dpi_set_packet_NULL_p; /* Null pointer        */

   union vardata datavalue;
   union vardata *trap_data_p;          /* pointer to trap data        */
   Trap_In  *trap_p;

 /* ------------------ set initial values ----------------------*/
   trap_data_p = &datavalue;
   trap_p =  trap_ptr;
   handle = DpiHandle;    /*  set DPI handle that was from DPI register */


 /*------------------- start functional code here ----------------*/
   if (trap_p->varBind_count>0)        /* Varbinds exist?             */
   {
     for (varbind_index = 0;
        varbind_index <= trap_p->varBind_count-1;
        ++varbind_index)
     {


       switch (trap_p->varBinds_a[varbind_index].vb_datatype)
                                         /* Set DPI data type, data size,
                                           and pointer to data based on
                                           SMI type                   */
       {
         case SNMP_SMI_INTEGER:
           trap_data_type = SNMP_TYPE_Integer32;
           trap_data_size = sizeof(Trap_Int32);
           trap_data_p->val_u32 =
                 trap_p->varBinds_a[varbind_index].data.vb_u32;
           break;
         case SNMP_SMI_Counter:
           trap_data_type = SNMP_TYPE_Counter32;
           trap_data_size = sizeof(Trap_Int32);
           trap_data_p->val_u32 =
                trap_p->varBinds_a[varbind_index].data.vb_u32;
           break;
         case SNMP_SMI_Gauge:
           trap_data_type = SNMP_TYPE_Gauge32;
           trap_data_size = sizeof(Trap_Int32);
           trap_data_p->val_u32 =
                 trap_p->varBinds_a[varbind_index].data.vb_u32;
           break;
         case SNMP_SMI_TimeTicks:
           trap_data_type = SNMP_TYPE_TimeTicks;
           trap_data_size = sizeof(Trap_Int32);
           trap_data_p->val_u32 =
                 trap_p->varBinds_a[varbind_index].data.vb_u32;
           break;
         case SNMP_SMI_UInteger32:
           trap_data_type = SNMP_TYPE_UInteger32;
           trap_data_size = sizeof(Trap_U_Int32);
           trap_data_p->val_uu32 =
                 trap_p->varBinds_a[varbind_index].data.vb_uu32;
           break;
         case SNMP_SMI_Counter64:
           trap_data_type = SNMP_TYPE_Counter64;
           trap_data_size = sizeof(Trap_Int64);
           trap_data_p->val_u64 =
                 trap_p->varBinds_a[varbind_index].data.vb_u64;
           break;
         case SNMP_SMI_Opaque:
           trap_data_type = SNMP_TYPE_Opaque;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_BIT_STRING:
           trap_data_type = SNMP_TYPE_BIT_STRING;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_OCTET_STRING:
           trap_data_type = SNMP_TYPE_OCTET_STRING;
           trap_data_size = trap_p->varBinds_a[varbind_index].vb_datalen;
           memcpy(trap_data_p->val_data,
                  trap_p->varBinds_a[varbind_index].data.vb_data,
                  trap_data_size);
           break;
         case SNMP_SMI_DISPLAYSTRING:
           trap_data_type = SNMP_TYPE_DisplayString;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_NsapAddress:
           trap_data_type = SNMP_TYPE_NsapAddress;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_OBJECT_IDENTIFIER:
           trap_data_type = SNMP_TYPE_OBJECT_IDENTIFIER;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_IpAddress:
           trap_data_type = SNMP_TYPE_IpAddress;
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;
         case SNMP_SMI_NULL:
           trap_data_type = SNMP_TYPE_NULL;         /* hndl NULL */
           trap_data_size = strlen(trap_p->varBinds_a[varbind_index].data.vb_data);
           strncpy(trap_data_p->val_data,
                   trap_p->varBinds_a[varbind_index].data.vb_data,
                   sizeof(trap_p->varBinds_a[varbind_index].data.vb_data));
           break;

        default :
           return 0;                      /* return                   */
        /* break;  // Note that this line of code is commented out    */
       }
       strcpy(dpi_oid, trap_p->varBinds_a[varbind_index].vb_oid); /* Get  OID */
       strcpy(dpi_oid_inst, trap_p->varBinds_a[varbind_index].vb_inst); /* Get Instance */

                          dpi_varBind_p=mkDPIset(       /* Make DPI set packet      */
                          dpi_varBind_p,  /* ptr to varbind chain     */
                          dpi_oid,        /* ptr to object ID         */
                          dpi_oid_inst,           /* instance of varbind      */
                          trap_data_type, /* type of data             */
                          trap_data_size, /* size of data             */
                          trap_data_p);   /* data itself              */

       if (!dpi_varBind_p)                 /* if null pointer       */
       {
         return (100);                        /* return                */
       }
     }
   }

#ifndef SOLARIS2
   trap_packet_p = mkDPItrap(            /* Make DPI trap packet      */
                      trap_p->trap_generic,   /* Set gen code              */
                      trap_p->trap_specific,  /* Set spec code             */
                      dpi_varBind_p,     /* ptr to varbind chain      */
                      trap_p->enterprise);          /* enterprise oid            */

   if (!trap_packet_p)                   /* if null pointer           */
   {
     return (100);                           /* return                    */
   }
    snd_rc = DPIsend_packet_to_agent(     /* Send DPI trap packet      */
                    handle,
                    trap_packet_p,       /* ptr to varbind chain      */
                    DPI_PACKET_LEN(trap_packet_p));      /* len       */

    if (snd_rc !=0)            /* Send call did not work successfully */
    {
     DMISA_TRACE_LOG1(LEVEL1, "Send Trap failed Return code = %d \n", snd_rc);
    }
#else
    if (!create_and_dispatch_trap(trap_p->trap_generic,trap_p->trap_specific,
                               dpi_varBind_p, trap_p->enterprise,error_label)) {
   DMISA_TRACE_LOG1(LEVEL1, "Send Trap failed Return code = %s \n", error_label);
     snd_rc = DPI_RC_NOK;
    }else
     snd_rc = DPI_RC_OK;
     
    if (dpi_varBind_p) fDPIset_packet(dpi_varBind_p); 
#endif
    return snd_rc;
}  /* end of function  */

/* Function Specificaton **********************************************/
/*                                                                    */
/*  Name:  buildVarbData                                     */
/*                                                                    */
/*  Description:  This function builds the varbind data for an event.   */
/*                                                                    */
/*  Input:  pointer to Event data,                                          */
/*          attribute count,                                          */
/*                                                                    */
/*  Output:  rc for successful or un successful build                    */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:  Exceptions resignaled to caller                      */
/*                                                                    */
/* End Function Specification *****************************************/
int buildVarbData(int event_type, void *argp)
{
    DmiSubscriptionNoticeIN   *subsnotice;
    DmiDeliverEventIN         *deliverev; 
    int                       rowdatacount, attrcount;
    DmiRowData_t              *rowdata;
    DmiAttributeValues_t      *valuep;

    char ca_prefix[MaxNumOidchars]; /* allocate mem for prefix */
    char  *OIDprefix;                      /* allocate a  prt for the OID prefix */
    char l_groupid_char[11];            /* allocate char group id*/
    char l_componentid_char[11];     /* allocate char comp id*/
    char l_attributeid_char[11];        /* allocate char attrb id*/
    char *attribute_value_p;            /* allocate ptr for  the actual value */
     int *val_int_p;                          /* pointer to int */
    Trap_Int64 *val_int64_p;            /* pointer to int64 */
     int x,cnt;
    ULONG attributei;
    DmiAttributeData_t   *attribute_ptr;
    int                  total_varcount=0;

#ifdef INT64_IS_OCTETSTRING
   char                  *snmp64int;
   int                    i;
#endif

    if (event_type == DMI_SpecTrap_Event) {
            deliverev = (DmiDeliverEventIN *)argp;
            rowdatacount = deliverev->rowData->list.list_len;
            rowdata = deliverev->rowData->list.list_val;
            desiredcomp = deliverev->compId;
    }else {
            subsnotice = (DmiSubscriptionNoticeIN *)argp;
            rowdatacount = 1;
            rowdata = &(subsnotice->rowData);
            desiredcomp = rowdata->compId;
    }  

    OIDprefix = findOidPrefix(desiredcomp,  XlateList); /* go find the OID this comp has assigned to it*/
    if (OIDprefix == 0 ) {
       DMISA_TRACE_LOG1(LEVEL2,":s Indicate received that is not translated to an OID.  Component ID =  %x\n", desiredcomp);
        return (Varbind_build_error);
    } /* endif */

    for (cnt=1; cnt <=rowdatacount; cnt++, rowdata++) {
    pitoa(rowdata->groupId, &l_groupid_char[0]);   /* conv groupid to char string */
    attrcount = rowdata->values->list.list_len;
    attribute_ptr = rowdata->values->list.list_val;
    total_varcount += attrcount;

    for (x=0; x < attrcount; x++, attribute_ptr++)
       {            /* loop through all of the attribute blocks (if any) */
            strcpy(ca_prefix,OIDprefix);   /* cpy prefix to work buf */
            strcat(ca_prefix,"1.");                    /* concatenate .1. to prefix */
            strcat(ca_prefix,l_groupid_char);      /* concatenate group id */
            strcat(ca_prefix,".1.");                    /* concatenate .1 reserv */

            attributei = attribute_ptr->id;
                                                     /* localize attribute id */
           pitoa(attributei, &l_attributeid_char[0]);  /* attributeid to char str*/
           strcat(ca_prefix,l_attributeid_char);    /* concatenate attribute id*/
           strcat(ca_prefix,".");                        /* concatenate a DOT */


           pitoa(desiredcomp, &l_componentid_char[0]);  /* compid to char str*/
           strcpy(dmi_trap_ptr->varBinds_a[x].vb_oid,ca_prefix);
                                                          /* set the oid prefix*/
           strcpy(dmi_trap_ptr->varBinds_a[x].vb_inst,l_componentid_char);
                                                          /* set the instance */

           switch (attribute_ptr->data.type)
           {
               case MIF_COUNTER:
                  dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_Counter; /* set data type */
                  dmi_trap_ptr->varBinds_a[x].data.vb_u32 =
                     attribute_ptr->data.DmiDataUnion_u.counter; 
                  break;
               case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                         SNMP_SMI_OCTET_STRING; /* set data type */
                     dmi_trap_ptr->varBinds_a[x].vb_datalen = sizeof(snmp_dpi_u64);
                     snmp64int = (char *)&(dmi_trap_ptr->varBinds_a[x].data.vb_u64);
                     swap64((char *)&(attribute_ptr->data.DmiDataUnion_u.integer64)
                      ,(char *)snmp64int);
#else
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_Counter64; /* set data type */
                     dmi_trap_ptr->varBinds_a[x].data.vb_u64 = 
                          attribute_ptr->data.DmiDataUnion_u.integer64;
#endif
                     break;
               case MIF_COUNTER64:
#ifdef COUNTER64_IS_COUNTER
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_Counter; /* set data type */
                     val_int64_p = (Trap_Int64 *)&(attribute_ptr->data.DmiDataUnion_u.counter64);
                     dmi_trap_ptr->varBinds_a[x].data.vb_u32 = val_int64_p->low;
#else
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_Counter64; /* set data type */
                     dmi_trap_ptr->varBinds_a[x].data.vb_u64 = 
                         attribute_ptr->data.DmiDataUnion_u.counter64;
#endif
                     break;
               case MIF_GAUGE:
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_Gauge; /* set data type */
                     dmi_trap_ptr->varBinds_a[x].data.vb_u32 = 
                            attribute_ptr->data.DmiDataUnion_u.gauge;
                     break;
               case MIF_INTEGER:
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_INTEGER; /* set data type */
                     dmi_trap_ptr->varBinds_a[x].data.vb_u32 = 
                       attribute_ptr->data.DmiDataUnion_u.integer;
                     break;
               case MIF_DISPLAYSTRING:
               case MIF_DATE:
                    dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_DISPLAYSTRING; /* set data type */
                     if (attribute_ptr->data.type == MIF_DATE)
                      {
                       strncpy(dmi_trap_ptr->varBinds_a[x].data.vb_data,
            (char *)attribute_ptr->data.DmiDataUnion_u.date,sizeof(DmiTimestamp_t));
             dmi_trap_ptr->varBinds_a[x].data.vb_data[sizeof(DmiTimestamp_t)] = 0;
                                               /* gentrap expects null terminated strng*/
                      }
                     else
                      {
                       work = attribute_ptr->data.DmiDataUnion_u.str;
                      if (work->body.body_len < MaxNumVbchars)
                       dmi_trap_ptr->varBinds_a[x].vb_datalen = work->body.body_len;
                      else
                        dmi_trap_ptr->varBinds_a[x].vb_datalen = MaxNumVbchars-1;
                       strncpy(dmi_trap_ptr->varBinds_a[x].data.vb_data,
                        work->body.body_val,dmi_trap_ptr->varBinds_a[x].vb_datalen);
                       dmi_trap_ptr->varBinds_a[x].data.vb_data[dmi_trap_ptr->varBinds_a[x].vb_datalen]
                                      = '\0';     /* gentrap expects null terminated strng*/
                      } /* endif */
                     break;
               case MIF_OCTETSTRING:
                       dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_OCTET_STRING; /* set data type */
                       work =(DmiString_t *)attribute_ptr->data.DmiDataUnion_u.octetstring;
                     if (work->body.body_len < MaxNumVbchars)
                       dmi_trap_ptr->varBinds_a[x].vb_datalen = work->body.body_len;
                     else
                       dmi_trap_ptr->varBinds_a[x].vb_datalen = MaxNumVbchars-1;
                       memcpy(dmi_trap_ptr->varBinds_a[x].data.vb_data, /* $MED */
                       work->body.body_val,dmi_trap_ptr->varBinds_a[x].vb_datalen);
                     break;
               case MIF_UNKNOWN_DATA_TYPE:
                     dmi_trap_ptr->varBinds_a[x].vb_datatype =
                            SNMP_SMI_UNKNOWN; /* set data type */
                     break;
               default:

                     break;
           } /* endswitch */
        } /* endfor for attrcount*/
     }  /*endfor for rowdatacount */
     dmi_trap_ptr->varBind_count = total_varcount;     /* varbind count */
    return(Varbind_build_ok);
}
/* Function Specificaton **********************************************/
/*                                                                                     */
/*  Name:  handleUnInstallIndication                                            */
/*                                                                                     */
/*  Description:  This function handles an Install/Uninstall Indication, checking  */
/*                 to make sure the correct data is in place before                  */
/*                 generating a trap.                                                  */
/*                                                                                     */
/*  Input:  pointer to Indiciation data,                                              */
/*                                                                                     */
/*  Output:  rc for successful or unsuccessful indiation                             */
/*                                                                                     */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:  Exceptions resignaled to caller                      */
/*                                                                    */
/**                                                       **/
/* End Function Specification *****************************************/
#if 0
This part to be removed after testing 
int   handleUnInstallInd( void *indication_p)
{

/*-------------local defines -------------------------------------*/
   int                   k, rc_sw;
#ifdef DMISA_TRACE
   int                   ln;
#endif
   int rc = 0;
   char                  l_componentid_char[11];  /* allocate place  for char comp id*/
   ULONG                 keycount = 0UL, keyblocklen = 0UL;
   DMI_GroupKeyData_t   *keyblock = 0;
   int                   attrCnt;            /* Attribute count*/
   int                   attrID;             /* Attribute ID*/

  desiredcomp = Indicate_p->iComponentId;                       /* set the comp id  */
  pdmicnfLC = (DMI_ListComponentCnf_t *)((char *)Indicate_p +
                  Indicate_p->oIndicationData);                        /* get pointer to Confirm List data block  */
  if (pdmicnfLC->oClassNameList > Indicate_p->iCmdLen) {  /* check if the offset is beyound the buffer */
            return (Parse_Invindic_Err);                                                      /* return with invalid indicate error */
  } /* endif */
  rc_sw =   findSoftware(Indicate_p);           /* call to determine hw or sw */
  pitoa(desiredcomp, &l_componentid_char[0]);                /* compid to char str*/


/*------------ determine hw or sw and set appropriate trap types --------*/
  if (Indicate_p->iIndicationType == DMI_INSTALL_INDICATION)
    {   /* This is INSTALL                                            */
      if ( rc_sw  ==  DMI_SW )
        {
          dmi_trap_ptr->trap_specific = DMI_SpecTrap_InstallSW;
        }
       else
        {
          dmi_trap_ptr->trap_specific = DMI_SpecTrap_InstallHW;
        } /* endif */

    }
   else
    { /* This is UN_INSTALL                                           */
     if (rc_sw == DMI_SW)
       {
         dmi_trap_ptr->trap_specific = DMI_SpecTrap_UnInstallSW;
       }
      else
       {
         dmi_trap_ptr->trap_specific = DMI_SpecTrap_UnInstallHW;
       } /* endif */
    } /* endif */


/*------------------- build varbinds to be sent to gentrap ------------------*/
/*   The OID will be:    SUN_ARCH_DMI_COMPMIB.1.1.a.c                                    */
/*                                        a = objects 1 to 7               */
/*                                        c = component ID               */

  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.7.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set inst. */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_INTEGER; /* set data type  */
  dmi_trap_ptr->varBinds_a[0].data.vb_uu32 = Indicate_p->iComponentId; /* set val of oid */

  strcpy(dmi_trap_ptr->varBinds_a[1].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[1].vb_oid,"1.1.8.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[1].vb_inst,l_componentid_char); /* set the instance */
  dmi_trap_ptr->varBinds_a[1].vb_datatype = SNMP_SMI_OCTET_STRING; /* set data type */
  work = (DMI_STRING  *) ((char *)pdmicnfLC + pdmicnfLC->osComponentName);
                                                              /* get  to the  Component Name */
  memcpy(dmi_trap_ptr->varBinds_a[1].data.vb_data, (char *)&(work->body),work->length);
  dmi_trap_ptr->varBinds_a[1].data.vb_data[work->length] = '\0';
  dmi_trap_ptr->varBinds_a[1].vb_datalen = work->length;
#ifdef DMISA_TRACE
  ln = sprintf(logbuffer,"Install/UnInstall Trap Routine Type = %d , Component = ", dmi_trap_ptr->trap_specific);
  ln = LOG_BUF_LEN - ln;
  strncpy(logbuffer + ln, (char *)&(work->body), (ln > 0) ? ln : 0);
  DMISA_TRACE_LOGBUF(LEVEL1);
#endif

  attrCnt = 0;                                                    /* start with zero attribute count*/
  if (Indicate_p->iIndicationType == DMI_UNINSTALL_INDICATION)
     {
     deleteFromXlate (desiredcomp, XlateList);              /* go tell xlate an un-install  occurred*/
     }
     else
     {
      addToXlate( desiredcomp, (char *)&(dmi_trap_ptr->varBinds_a[1].data.vb_data), XlateList);
                                                              /* go tell xlate a MIF was added  */

      desiredgroup = 1;                                         /* This should alway be Group 1 */
      desiredattribute = 0;                                    /* DMI wants 0 to start at the first */
      dmierror =issueGetRow(
                  &pdmireqGR,
                  desiredcomp,
                  desiredgroup,
                  desiredattribute,
                  keycount,
                  keyblocklen,
                  keyblock,
                  DMISA_FIRST,
                  ++DmiReqHandle);
      if (!dmierror)
         {
         if (pdmireqGR->DmiMgmtCommand.iStatus == SLERR_NO_ERROR ||
             pdmireqGR->DmiMgmtCommand.iStatus == OMERR_OVERLAY_NOT_FOUND)
            {
             pdmicnfGR = (DMI_GetRowCnf_t *)(pdmireqGR->DmiMgmtCommand.pCnfBuf);
                                                           /* get confirm ptr */
             attrCnt = pdmicnfGR->iAttributeCount;         /* get the Attribute  Count */
             if (attrCnt > 5) {                         /* did we get more than 5 attributes */
                 attrCnt   = 5;                          /*  only use the first 5 */
             } /* endif */
             pdmicnfGA = pdmicnfGR->DmiGetAttributeList;   /* point to  Attribute List */

            for  (k=0; (k < attrCnt) && (rc == 0); k++)
               {
                 attrID = pdmicnfGA->iAttributeId;                       /* get the Attrib ID*/
                 work = (DMI_STRING *) ( (char *)pdmicnfGR + pdmicnfGA->oAttributeValue); /* offset to value */

                 switch (attrID)
                  {                   /* Which Attribute                  */
                    case 1:           /* Attribute Id for "Mfg"           */
                    case 2:           /* Attribute ID for "Product"       */
                    case 3:           /* Attribute ID for "Version"       */
                    case 4:           /* Attribute ID for "Serial Number" */
                           if ((pdmicnfGA->iAttributeType == MIF_DISPLAYSTRING) || (pdmicnfGA->iAttributeType == MIF_OCTETSTRING))
                           {
                              strcpy(dmi_trap_ptr->varBinds_a[attrID+1].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
                              strcat(dmi_trap_ptr->varBinds_a[attrID+1].vb_oid,(const char *)aInstallOid[attrID-1]);
                              strcpy(dmi_trap_ptr->varBinds_a[attrID+1].vb_inst,
                                      l_componentid_char);                  /* set the instance */
                              dmi_trap_ptr->varBinds_a[attrID+1].vb_datatype =
                                      SNMP_SMI_OCTET_STRING;     /* set data type   */
                              memcpy(dmi_trap_ptr->varBinds_a[attrID+1].data.vb_data,
                                      (char *)&(work->body),work->length);           /* set value of oid    */
                              dmi_trap_ptr->varBinds_a[attrID+1].vb_datalen = work->length;
                           }
                           else
                              rc = 100; /* defense against defective components */
                           break;
                    case 5:                               /* Attribute ID for "Installation" */
                           if (pdmicnfGA->iAttributeType == MIF_DATE)
                           {
                              strcpy(dmi_trap_ptr->varBinds_a[6].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
                              strcat(dmi_trap_ptr->varBinds_a[6].vb_oid,"1.1.5.");  /* fill in the object */
                              strcpy(dmi_trap_ptr->varBinds_a[6].vb_inst,
                                       l_componentid_char);               /* set the instance */
                              dmi_trap_ptr->varBinds_a[6].vb_datatype =
                                       SNMP_SMI_DISPLAYSTRING;           /* set data type */
                              /* note the date type is not a string, so the DMI_STRING cast won't work here */
                              strncpy(dmi_trap_ptr->varBinds_a[6].data.vb_data,
                                       (char *)&(work->length),DATE_LENGTH_snmp); /*                  set value of oid */
                              dmi_trap_ptr->varBinds_a[6].data.vb_data[DATE_LENGTH_snmp] = '\0';   /* null terminate */
                           }
                           else
                              rc = 100; /* defense against defective components */
                           break;

                    default:
                           break;
                  } /* endswitch */

                 pdmicnfGA++;                               /* increment to next attribute value */
               } /* end for */
           } /* end if */
        } /* end if */
        if (pdmireqGR)
           {
           if (pdmireqGR->DmiMgmtCommand.pCnfBuf) free(pdmireqGR->DmiMgmtCommand.pCnfBuf);
           free(pdmireqGR);
        } /* endif */
    } /* endif */
    dmi_trap_ptr->varBind_count = attrCnt + 2;                   /* varbind count */
                                                                 /*# of Attributes plus 2*/
    if (!rc)
       rc = generateTrap(dmi_trap_ptr);                                 /* call generate trap */
    return(rc);                                                                /* return the return code */
 }                                                                                 /* End of Un/Install */
#endif
 /* Function Specificaton **********************************************/
/*                                                                                                           */
/*  Name:  findSofware                                                                              */
/*                                                                                                           */
/*  Description:                                                                                         */
/* Function to determine if this is a h/w or s/w Indication.                                */
/* Each Group's Class Name will be scanned looking for the word "software".     */
/* If it is not found,  a H/W trap trap will be reported.                                     */
/*                                                                                                           */
/*  Input:  Pointer to buffer                                                                        */
/*                                                                                                           */
/*  Output:  Return Code Hardware or Software                                            */
/*                                                                                                          */
/*  Exit Normal:  Return to caller                                                                */
/*****************************************************************/
#if 0
To be removed
int  findSoftware( void *pIndication)
{
  DMI_ClassNameData_t *classnamedata_ptr; /* ptr to Class Name data */
  DMI_ClassNameData_t *tempnamedata_ptr; /* temp adjustable ptr to CN */
  char                        *classdata;              /* ptr to class name string */
  int                    tempdataoffset;
  int                     classdata_size;              /* size of class name string */
  int i, j;
       pdmicnfLC = (DMI_ListComponentCnf_t *)((char *)Indicate_p +
                          Indicate_p->oIndicationData);

       classnamedata_ptr = (DMI_ClassNameData_t *)((char *)pdmicnfLC +
                              pdmicnfLC->oClassNameList);
                                        /* get pointer to class Name data blocks */

       for (i=0; i < pdmicnfLC->iClassListCount; i++ )
        {                             /* loop on all the data blocks in the list */

         tempnamedata_ptr =  classnamedata_ptr++;
                                   /* temp ptr to the instance of the class string*/
         tempdataoffset = tempnamedata_ptr->osClassString; /* offset to string*/
         classdata = (char *)((char *)pdmicnfLC + tempdataoffset);
                                       /* pointer to the actual class name string */
         classdata_size = * ((int *) classdata);  /* get size of string */

         for (j=0; j < (((classdata_size + 1) <  strlen(Ca_SW)) ? 0:
                                  ((classdata_size +1) - strlen(Ca_SW))); j++)

          {        /* loop through string looking for software - any case */

            if (0 == strnicmp(&classdata[j],Ca_SW,strlen(Ca_SW)))
             {          /* if there is a string compare in the class data */
                        /* one of the Groups Class Names had the word "software" in it. */
              return(DMI_SW);
              }   /* end for if software found */

       }    /* endfor string loop for software search*/
    }    /* endfor datablock loop*/
       return(DMI_HW);

}  /* end Function */
#endif
/* Function Specificaton **********************************************/
/*                                                                                                          */
/*  Name:  checkCAevent                                                                           */
/*                                                                                                           */
/*  Description:  This function check for a Client Access Event                         */
/*                 by looking for an event name of CA/400                                  */
/*                 if found it doe a list to check for a attribute with the                */
/*                 name "Event Type" and if found it check if the value                */
/*                 matchs the value in the enumation list for a Client Access          */
/*                 problem record.                                                                    */
/*                                                                                                           */
/*  Input:  pointer to Indiciation data,                                                           */
/*                                                                                                           */
/*  Output:  rc  CA_EVENT_OK           it is a not a Client Access Event              */
/*                                                                                                           */
/*                  CA_EVENT_IGNORE     means it is a error indcate event              */
/*                                                 and the trap should not be generated      */
/*                                                 Checks are made on the size of the        */
/*                                                 component class name, attribute name    */
/*                                                 and enumname to ensure that they are not*/
/*                                                 greater than 256. If they are the event is */
/*                                                 ignored.                                               */
/*                  CA_PROBLEM_RECORD this is a event type to send a problem record */
/*  Exit Normal:  Return to caller                                                                 */
/*                                                                                                           */
/*  Exit Error:  none                                                                                  */
/*                                                                                                          */
/*--pointers-----------Data Blocks ---------------------------------------------------*/
/* event_ptr ---->   DmiEventData                                                                                */
/*                                iClassCount                                                                              */
/* eventclass_ptr----->  DmiClassData[]                                                                        */
/*                                     iComponentId                                                                      */
/* eventnametr --------->  osClassString                                                                      */
/*                                     iRowCount                                                                         */
/*                                     oDmiRowList                                                                       */
/* rowlist_ptr------------> DmiGetRowCnf                                                                     */
/*                                         iGroupId                                                                          */
/*                                         oGroupKeyList                                                                 */
/*                                         iAttributeCount                                                                */
/* attribute_ptr------------>   DmiGetAttributeCnf[]                                                         */
/*                                               iGroupId                                                                     */
/*                                               iAttributeId                                                                */
/*                                               iAttributeType                                                           */
/*                                               oAttributeValue                                                         */
/****The issueListAttribute Confirm Dtat Block***************************************/
/* pdmucnfLA --------------> DMIListAttributeCnf                                               */
/*                                               iAttributeId                                                    */
/* attributenameptr ----------->   osAttributeName                                             */
/*                                               iAttributeAccess                                             */
/*                                               IAttrubuteType                                              */
/*                                               iAttributeMaxSize                                           */
/*                                               iEnumListCount                                              */
/* enumlist_ptr ---------------->   oEnumList[]                                                  */
/*                                                    iEnumValue                                              */
/* enumname --------------------->  osEnumName                                            */
/*                                                                                                                 */
/* End Function Specification **********************************************/

#if 0
To be removed
int   checkCAevent ( void *indication_p, DMI_GetRowCnf_t **prowlist_ptr,
                     ULONG *pcompi, ULONG *pgroupi, ULONG *pattributei)
{
      int   k, l, m, n, q, rc=0;
      DMI_GetRowCnf_t  *rowlist_ptr;         /* ptr to DMI Event data  */
      DMI_GetAttributeCnf_t  *attribute_ptr;      /* ptr to attribute data  */
      DMI_ClassData_t        *eventclass_ptr;     /* ptr to event class data */
      DMI_EventData_t        *event_ptr;          /* ptr to DMI Event data  */
      size_t                  eventname_size;     /* size of event name string */
      char                   *eventnameptr;      /* ptr to event name string */
      char                   *attributenameptr;   /* ptr to string name of attrib */
      size_t                  attributename_size; /* size of attribute name string */
      ULONG                  *rowlistOffset_ptr;  /* ptr to row list offset      */

      event_ptr = (DMI_EventData_t *)((char *)Indicate_p +  Indicate_p->oIndicationData);
      *pcompi = Indicate_p->iComponentId;
      eventclass_ptr = &(event_ptr->DmiClassList[0]);
      rowlistOffset_ptr = (ULONG *)((char *)Indicate_p +  eventclass_ptr->oDmiRowList);     /* set the rowlist pointer */
      if ((char *)rowlistOffset_ptr  > (char *)IndicateEnd_p) {
                                                             /* chk if the rowlist pointer is beyond the buffer */
         rc = CA_EVENT_IGNORE;                   /* ignore this event, something  is wrong with it */
         return (rc);                                    /* we are out of here */
      } /* endif */
      rowlist_ptr = (DMI_GetRowCnf_t *)((char *)Indicate_p + (*rowlistOffset_ptr));
      *prowlist_ptr = rowlist_ptr;
      *pgroupi = rowlist_ptr->iGroupId;

      for (k=0; k < event_ptr->iClassCount; k++)
        {                               /* loop through class counts (if any) */
        work = (DMI_STRING *) ((char *)Indicate_p + eventclass_ptr->osClassString);
                                                                         /* get pointer to list of event classes */

        eventnameptr = (char *)&(work->body);        /* get pointer to class name */
        eventname_size = work->length;              /* size of class name string*/
         if (((char * )eventnameptr + eventname_size)  > (char *)IndicateEnd_p) {
                                                                /* chk if the name is beyond the buffer */
             rc = CA_EVENT_IGNORE;                   /* ignore this event, something  is wrong with it */
             return (rc);                                    /* we are out of here */
         } /* endif */

         for (l=0; l < (((eventname_size + 1) < strlen(Ca_400)) ? 0:
            ((eventname_size + 1) - strlen(Ca_400)));  l++)
           {               /* loop through class name string, looking for CA 400 */

           if (0 == strnicmp(&eventnameptr[l], Ca_400, strlen(Ca_400)))
            {                            /* if we find CA 400 do the following */

            for (m=0; m < eventclass_ptr->iRowCount; m++)
              {                  /* loop through all the rows that exist (if any) */

               for (n=0; n < rowlist_ptr->iAttributeCount; n++)
                {            /* loop through all of the attribute blocks (if any) */
                 attribute_ptr = &(rowlist_ptr->DmiGetAttributeList[n]);
                                        /* get pointer to current attribute block */
                 *pattributei = attribute_ptr->iAttributeId;

                 rc = issueListAttribute(&listattrout,
                                            Indicate_p->iComponentId,
                                            rowlist_ptr->iGroupId,
                                            attribute_ptr->iAttributeId,
                                            DMISA_THIS,
                                             1);
                          /* call is list attribute passing it the component id, */
                          /* group id, attribute id, the flag for type of cmd    */
                          /* and a handle.  In return we get a ptr to a ptr to  */
                          /* the DmiListAttributeCnf block.   */
                 if (rc != ISSUE_LIST_ATTRIBUTE_noError ) {
                     if ( listattrout != NULL) {
                        free_listattrout(listattrout);
                     } /* endif */
                     return(CA_EVENT_IGNORE);  /* get out, something is wrong */
                 } /* endif */
                 attrinfo = listattrout->reply->list.list_val;
                            /* get pointer from a pointer to the attribute*/
                 work = attrinfo->name;
                                                                         /* get pointer to attribute name */
                 attributename_size = work->body.body_len; /* get the size of the name */
                 if (attributename_size > MaxNameSize) {
                     if (listattrout) 
                       free_listattrout(listattout);
                     rc = CA_EVENT_IGNORE;                   /* ignore this event, something  is wrong with it */
                     return (rc);                                    /* we are out of here */
                 } /* endif */
                 attributenameptr = (work->body.body_val);   /*point to the name */

                 for (q=0; q < (((attributename_size + 1) < strlen(Ca_Event))?   0:
                               ((attributename_size + 1) - strlen(Ca_Event)));  q++)   /* looking for an attribute */
                                                                                                            /* wth a size >10 and the */
                                                                                                             /* name EventType        */
                  {            /* loop through string for event type characters */
               if(0 == strnicmp(&attributenameptr[q], Ca_Event, strlen(Ca_Event)))
                    {           /* if that piece of string was found...continue */
                if ((attrinfo->enumList) && (attrinfo->enumList->list.list_len > 0))
                       {                         /* if Enumeration blocks exist */
                        rc =  checkCAeventType (pdmicnfLA, attribute_ptr);
                        if (listattrout) 
                           free_listattrout(listattrout);
                        return (rc);

                       } else /* no enumation, so this is not a CA Event */
                      {
                       rc = CA_EVENT_OK;
                     } /* endif chk for Enum count */
                    } else  /* no it is not a CA event type */
                    {
                     rc = CA_EVENT_OK;
                    } /* endif Event Type*/
                   }  /* loop for event type */
                      if (listattrout) 
                         free_listattrout(listattrout);
               } /* endfor number of attribute lists*/
            } /* endfor number of row lists*/
            break;
         }  else { /* client access was not found */
             rc =  CA_EVENT_OK;
         } /* endif  check for Client Access check */
      } /* endfor string loop for Client Access search*/
      eventclass_ptr++;
   } /* endfor event classes found*/
   return (rc);
} /* end function */
#endif

/* Function Specificaton **********************************************/
/*                                                                                                          */
/*  Name:  checkCAeventType                                                                   */
/*                                                                                                          */
/*  Description:  This function checks he  Client Access Event type                  */
/*                 attribute   for error event or problem record event.                  */
/*                 If a problem record event, it goes on to find the value the         */
/*                 thje Event Type attribute should be set to if the trap is sent.      */
/*                                                                                                           */
/*  Input:  pointer to list attribute confirm buffer                                           */
/*                                                                                                           */
/*  Output:  rc = CA_EVENT_OK           it is a not a Client Access Event              */
/*                                                                                                           */
/*              rc =  CA_EVENT_IGNORE     means it is a error indcate event              */
/*                                                 and the trap should not be generated      */
/*                                                                                                            */
/*                   Notify_value            set to value in CA Event Type enum for    */
/*                                                 problem record sent                              */
/*  Exit Normal:  Return to caller                                                                 */
/*                                                                                                           */
/*  Exit Error:  none                                                                                  */
/*                                                                                                          */
/******************************************************************/
#if 0
To be removed
int    checkCAeventType  ( DMI_ListAttributeCnf_t  *ppdmicnfLA,
                           DMI_GetAttributeCnf_t  *attribute_ptr)
{
   int             r,s, rc=0 ;
   int             pr_len;
   DMI_EnumData_t *enumlist_ptr;       /* pointer to enumeration data */
   char           *enumname;           /* pointer to enumeration name */
   size_t          enumname_size;      /* size of enumeration name */
   DMI_EnumData_t *tempenumdata_ptr;   /* temp poiner to enum name */
   char           *attrvaluep;

   /* get ptr to beginning of all enum blocks                                 */
   enumlist_ptr = (DMI_EnumData_t *) ((char*)pdmicnfLA + pdmicnfLA->oEnumList);
   notify_value = 0;                                           /* set the notify value to zero */
   for (r=0; r < pdmicnfLA->iEnumListCount ; r++)
    {      /* loop through the list of enumeration blocks */

     tempenumdata_ptr =  enumlist_ptr;
                         /* tmp ptr to current enum block */
     work = (DMI_STRING *) ((char *)pdmicnfLA  + tempenumdata_ptr->osEnumName);
                        /* ptr to the enum name string */
     enumname_size = work->length;  /* get the size of the name */
     if (enumname_size > MaxNameSize) {
          rc = CA_EVENT_IGNORE;                   /* ignore this event, something  is wrong with it */
          return (rc);                                    /* we are out of here */
      } /* endif */
     enumname = (char *)&(work->body);   /*point to the name */

     pr_len = strlen(Ca_PR);
     for (s=0;  s < (((enumname_size + 1) < pr_len)? 0:
                                 ((enumname_size+1) - pr_len)); s++)
       {       /* loop through enum name for Send Prob.... */
           if  (0 == strnicmp(&enumname[s],Ca_PR,pr_len))
           {
              /*  now check if the Value of the attribute with the Name "Event Type" is */
              /*  equal to the value specified in the enumation list for Problem record.    */
              /* If it is, indicate that it is ok to send a trap. If not, this is a CA error event */
              /* and it should be ignored.                                                                 */
              attrvaluep = ((char *)Indicate_p + attribute_ptr->oAttributeValue);
                                                                   /* get the attribute value */
              if (((char * )attrvaluep)  > (char *)IndicateEnd_p) {
                                                                      /* chk if the value pointer  is beyond the buffer */
                   rc = CA_EVENT_IGNORE;                   /* ignore this event, something  is wrong with it */
                   return (rc);                                    /* we are out of here */
              } /* endif */
              DMISA_TRACE_LOG2(LEVEL1,":s DMI Indicate Event Value = %d, Enumated Value = %d.\n", *attrvaluep, tempenumdata_ptr->iEnumValue);

                if   (*attrvaluep == tempenumdata_ptr->iEnumValue)  /*is the Event Type value equal */
                {                                                                      /* the value from the enumation*/
                    enumlist_ptr = (DMI_EnumData_t *)
                                             ((char*)pdmicnfLA + pdmicnfLA->oEnumList);
                    for (r=0; r < pdmicnfLA->iEnumListCount ; r++)
                          {      /* loop through the list of enumeration blocks */
                          tempenumdata_ptr =  enumlist_ptr;
                          /* tmp ptr to current enum block */
                          work = (DMI_STRING *) ((char *)pdmicnfLA  + tempenumdata_ptr->osEnumName);
                                                                                    /* ptr to the enum name string */
                          enumname = (char *)&(work->body);    /*point to the name */

                          pr_len = strlen(Ca_Notify);                   /* get the lenght of the name */
                          for (s=0;  s < (((enumname_size + 1) < pr_len)? 0:
                                                             ((enumname_size+1) - pr_len)); s++)
                               {                               /* loop through enum name for Notify Sent.... */
                                if  (0 == strnicmp(&enumname[s],Ca_Notify,pr_len))   {
                                     notify_value = tempenumdata_ptr->iEnumValue;
                                     r = pdmicnfLA->iEnumListCount +1;    /* break  enum loop */
                                     s =  enumname_size+1;                     /* break compare loop */
                                } /*end for setting the value */
                           }/* endif check for CA Notify Sent Enum Name */
                           enumlist_ptr++;
                     } /* endfor enumeration struct exist */


                    rc = CA_EVENT_OK;                          /* indicate that it is ok to send a event trap */
                } else {
                    rc  = CA_EVENT_IGNORE;                    /* indicate that this is the CA error  indicate   */
                } /*end for value check */
                return (rc);  /* get out of this routine here */
            }/* endif check for CA Problem Record */
       } /* endfor strlen of Send Problem Record */
       rc = CA_EVENT_OK;
       enumlist_ptr++;
   } /* endfor enumeration struct exist */
   return  (rc);
}  /*   end of Function  */
#endif
/*****************************************************************/
/* Function Specificaton                                                                           */
/*                                                                                                      */
/*  Name:  setCAeventType                                                                   */
/*                                                                                                       */
/*  Description:  This function will set the  Client Access Event type              */
/*                 attribute to the value specified in the enum list.                      */
/*                 The value is determined in the checkCAeventType routine.       */
/*                 If an error occurres, the value is  not set.                                */
/*                                                                                                        */
/*  Input:  value to be set                                                                       */
/*                                                                                                       */
/*  Output:  rc = rcsub                 return from SET function                        */
/*                                                                                                      */
/*  Exit Normal:  Return to caller                                                           */
/*                                                                                                     */
/*  Exit Error:  none                                                                            */
/*                                                                                                    */
/*****************************************************************/
#if 0 
To be removed
int setCAeventType (int notify, ULONG compi, ULONG groupi, ULONG attributei)
{
   DMI_SetAttributeReq_t  *pdmireqSA;
   DMI_GroupKeyData_t   *pkeyblock = NULL;
   ULONG               keycount = 0UL, keyblocklen = 0UL;
   ULONG               attributetype, attributelen;
   int                     rcsub = 0;
   ULONG                reqbuflen, dmistat=0, cmd;
   char                   *pattributevalue;

   attributetype = MIF_INT;                                                /*  set type to integer */
   attributelen = sizeof(MIF_INT);                                       /* set the size the integer */
   pattributevalue = (char *)&notify;                                  /* point to the value    */

   cmd = DmiSetReserveAttributeCmd;                                 /*   do the reserve   */

   while (cmd != SET_COMPLETE)  {
       pdmireqSA = NULL;                                                  /* set to NULL so the build works ok*/
       rcsub = buildSetAttribute(
                  &pdmireqSA,
                  &reqbuflen,
                  groupi,
                  attributei,
                  keycount,
                  keyblocklen,
                  pkeyblock,
                  attributelen,
                  attributetype,
                  pattributevalue);
       if (rcsub != BUILD_SET_ATTRIBUTE_noError ) {
           if (pdmireqSA) free(pdmireqSA);
           break;                                                            /* we are out of here*/
       } else {
          rcsub = issueSetAttribute(
                   &pdmireqSA,
                   compi,
                   cmd,
                   reqbuflen,
                  attributetype,
                  ++DmiReqHandle);
         dmistat = pdmireqSA->DmiMgmtCommand.iStatus;
         if (pdmireqSA) {
              if (pdmireqSA->DmiMgmtCommand.pCnfBuf) free(pdmireqSA->DmiMgmtCommand.pCnfBuf);
              free(pdmireqSA);
         }
      }  /* endif  */

      if (cmd == DmiSetReserveAttributeCmd) {
          cmd = DmiSetAttributeCmd;                         /* set the cmd to do SET*/
      } else {
         if (cmd == DmiSetAttributeCmd) {
             if (rcsub != ISSUE_SET_ATTRIBUTE_noError || dmistat != SLERR_NO_ERROR ) {
                cmd = DmiSetReleaseAttributeCmd;      /* set the cmd to Release*/
                dmistat = SLERR_NO_ERROR;                /*  set status so this release works*/
             } else {
                break;                                              /* completed ok*/
             } /* endif */
         } else {
             cmd = SET_COMPLETE;                          /*  this was a release, we are finished*/
         } /* endif */
      } /* endif */
      if (rcsub != ISSUE_SET_ATTRIBUTE_noError || dmistat != SLERR_NO_ERROR) {
             break;
      } /*  endif */
   }  /* end while */

   return (rcsub);
} /* end of function */
#endif


DmiErrorStatus_t handle_CompLangGrpIndication(int event_type, void *argp) {

/*-------------local defines -------------------------------------*/
   int                   k, rc_sw;
#ifdef DMISA_TRACE
   int                   ln;
#endif
   int rc = 0;
   char                  l_componentid_char[11];  /* allocate place  for char comp i
d*/
   ULONG                 keycount = 0UL, keyblocklen = 0UL;
   DMI_GroupKeyData_t   *keyblock = 0;
   int                   attrCnt;            /* Attribute count*/
   int                   attrID;             /* Attribute ID*/
   DmiComponentAddedIN    *compadd;
   DmiComponentDeletedIN  *compdel;
   DmiLanguageAddedIN     *langadd;
   DmiLanguageDeletedIN   *langdel;
   DmiGroupAddedIN        *grpadd;
   DmiGroupDeletedIN      *grpdel;
   thread_t               selfid;
   OidPrefix_t           *xltlist;
   


 dmi_trap_ptr = (Trap_In *) malloc (sizeof (Trap_In));
 if (!dmi_trap_ptr) {   /* did we get it
                  */
   DMISA_TRACE_LOG(LEVEL1,"Out of Memory, Cannot build a Trap.\n");
   return(DMIERR_OUT_OF_MEMORY);
 } /* end if */

 strcpy(dmi_trap_ptr->community, DEFAULT_COMMUNITY);     /* set community name */
 strcpy(dmi_trap_ptr->enterprise, DMI_SUBAGENT);    /* set the enterprise oid*/
 dmi_trap_ptr->trap_generic = Ca_Trap;             /* set the Specific trap type*/
 dmi_trap_ptr->trap_specific = event_type;
 varbind_p = snmp_dpi_set_packet_NULL_p;   /* init vb chain to NULL pointer */

 switch(event_type) {
   case DMI_SpecTrap_ComponentAdded: 
       compadd = (DmiComponentAddedIN *)argp;
       desiredcomp = compadd->info->id;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.7.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set inst. */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_INTEGER; /* set data type  */
  dmi_trap_ptr->varBinds_a[0].data.vb_uu32 = desiredcomp; /* set val of oid */

  strcpy(dmi_trap_ptr->varBinds_a[1].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[1].vb_oid,"1.1.8.");  /* fill in the last of the object */

  strcpy(dmi_trap_ptr->varBinds_a[1].vb_inst,l_componentid_char); /* set the instance */
  dmi_trap_ptr->varBinds_a[1].vb_datatype = SNMP_SMI_OCTET_STRING; /* set data type */
  memcpy(dmi_trap_ptr->varBinds_a[1].data.vb_data, 
           compadd->info->name->body.body_val,
           compadd->info->name->body.body_len);  /*Component Name */
 dmi_trap_ptr->varBinds_a[1].data.vb_data[compadd->info->name->body.body_len]=0;
 dmi_trap_ptr->varBinds_a[1].vb_datalen = compadd->info->name->body.body_len;

 dmi_trap_ptr->varBind_count = 2;     /* varbind count */

 if (thr_create(NULL, NULL, (void *(*)(void *))thr_rebuildXlate,
                (void *)NULL, THR_DETACHED | THR_NEW_LWP,
                        &selfid)) {
    DMISA_TRACE_LOG1(LEVEL1,"dmisa:Unable to  create a thread to add component =%d to repository",desiredcomp);
    exit(1);
 }
                              break;

   case DMI_SpecTrap_ComponentDeleted:
       compdel = (DmiComponentDeletedIN *)argp;
       desiredcomp = compdel->compId;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.7.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set inst. */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_INTEGER; /* set data type  */
  dmi_trap_ptr->varBinds_a[0].data.vb_uu32 = desiredcomp; /* set val of oid */

 dmi_trap_ptr->varBind_count = 1;     /* varbind count */

 deleteFromXlate(desiredcomp, XlateList);

                              break;
   case DMI_SpecTrap_LanguageAdded:
       langadd = (DmiLanguageAddedIN *)argp;
       desiredcomp = langadd->compId;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/

  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.12.");  /* fill in the last of the object */

  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set the instance */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_OCTET_STRING; /* set data type */
  memcpy(dmi_trap_ptr->varBinds_a[0].data.vb_data, 
           langadd->language->body.body_val,
           langadd->language->body.body_len);  /*Component Name */
 dmi_trap_ptr->varBinds_a[0].data.vb_data[langadd->language->body.body_len]=0;
 dmi_trap_ptr->varBinds_a[0].vb_datalen = langadd->language->body.body_len;

 dmi_trap_ptr->varBind_count = 1;     /* varbind count */
                              break;

   case DMI_SpecTrap_LanguageDeleted:
       langdel = (DmiLanguageDeletedIN *)argp;
       desiredcomp = langdel->compId;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/

  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.12.");  /* fill in the last of the object */

  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set the instance */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_OCTET_STRING; /* set data type */
  memcpy(dmi_trap_ptr->varBinds_a[0].data.vb_data, 
           langdel->language->body.body_val,
           langdel->language->body.body_len);  /*Component Name */
 dmi_trap_ptr->varBinds_a[0].data.vb_data[langdel->language->body.body_len]=0;
 dmi_trap_ptr->varBinds_a[0].vb_datalen = langdel->language->body.body_len;

 dmi_trap_ptr->varBind_count = 1;     /* varbind count */
                              break;
   case DMI_SpecTrap_GroupAdded:
       grpadd  = (DmiGroupAddedIN *)argp;
       desiredcomp = grpadd->compId;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.10.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set inst. */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_INTEGER; /* set data type  */
  dmi_trap_ptr->varBinds_a[0].data.vb_uu32 = grpadd->info->id; /* set val of oid */

  strcpy(dmi_trap_ptr->varBinds_a[1].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[1].vb_oid,"1.1.11.");  /* fill in the last of the object */

  strcpy(dmi_trap_ptr->varBinds_a[1].vb_inst,l_componentid_char); /* set the instance */
  dmi_trap_ptr->varBinds_a[1].vb_datatype = SNMP_SMI_OCTET_STRING; /* set data type */
  memcpy(dmi_trap_ptr->varBinds_a[1].data.vb_data, 
           grpadd->info->name->body.body_val,
           grpadd->info->name->body.body_len);  /*Component Name */
 dmi_trap_ptr->varBinds_a[1].data.vb_data[grpadd->info->name->body.body_len]=0;
 dmi_trap_ptr->varBinds_a[1].vb_datalen = grpadd->info->name->body.body_len;

 dmi_trap_ptr->varBind_count = 2;     /* varbind count */

 xltlist= deleteFromXlate(desiredcomp, XlateList);

 if (!xltlist)
      break;
 thr_arg.compid = desiredcomp;
 thr_arg.compname = xltlist->pCompName; 
 thr_arg.xlatelist = XlateList;

 if (thr_create(NULL, NULL, (void *(*)(void *))thr_addToXlate,
                (void *)&thr_arg, THR_DETACHED | THR_NEW_LWP,
                        &selfid)) {
    DMISA_TRACE_LOG1(LEVEL1,"dmisa:Unable to  create a thread to add component =%d to repository",desiredcomp);
    exit(1);
 }
                              break;

   case DMI_SpecTrap_GroupDeleted:
       grpdel = (DmiGroupDeletedIN *)argp;
       desiredcomp = grpdel->compId;
       pitoa(desiredcomp, &l_componentid_char[0]);   /* compid to char str*/
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_oid,SUN_ARCH_DMI_COMPMIB);  /*Set the OID */
  strcat(dmi_trap_ptr->varBinds_a[0].vb_oid,"1.1.10.");  /* fill in the last of the object */
  strcpy(dmi_trap_ptr->varBinds_a[0].vb_inst,l_componentid_char); /* set inst. */
  dmi_trap_ptr->varBinds_a[0].vb_datatype = SNMP_SMI_INTEGER; /* set data type  */
  dmi_trap_ptr->varBinds_a[0].data.vb_uu32 = grpdel->groupId; /* set val of oid */
 dmi_trap_ptr->varBind_count = 1;     /* varbind count */

 deleteGroupFromXlate(grpdel->groupId, XlateList);
                              break;

   default:
                  return(DMIERR_ILLEGAL_HANDLE);
 }

 if (trap_forward_to_magent)
    rc = generateTrap(dmi_trap_ptr);

 free(dmi_trap_ptr);

 if (rc == 0)
   return (DMIERR_NO_ERROR);
 else
   return (DMIERR_ILLEGAL_HANDLE);
}


DmiErrorStatus_t  handle_Event_SubsNotice(int event_type, void *argp) {
     
int rc = 0;
ULONG compi, groupi, attributei;
DmiSubscriptionNoticeIN *subsnotice;

 dmi_trap_ptr = (Trap_In *) malloc (sizeof (Trap_In));
 if (!dmi_trap_ptr) {   /* did we get it
                  */
   DMISA_TRACE_LOG(LEVEL1,"Out of Memory, Cannot build a Trap.\n");
   return(DMIERR_OUT_OF_MEMORY);
 } /* end if */

 strcpy(dmi_trap_ptr->community, DEFAULT_COMMUNITY);     /* set community name */
 strcpy(dmi_trap_ptr->enterprise, DMI_SUBAGENT);    /* set the enterprise oid*/
 dmi_trap_ptr->trap_generic = Ca_Trap;             /* set the Specific trap type*/
 varbind_p = snmp_dpi_set_packet_NULL_p;   /* init vb chain to NULL pointer */

     switch(event_type) {
         case DMI_SpecTrap_Event:
           dmi_trap_ptr->trap_specific = DMI_SpecTrap_EventOccurence;
           if (buildVarbData(event_type, argp)) {
                free(dmi_trap_ptr);
                return(DMIERR_ILLEGAL_HANDLE);
           }
                             break;

         case DMI_SpecTrap_SubscriptionNotice:

           subsnotice = (DmiSubscriptionNoticeIN *)argp;

           if (subsnotice->expired)
              dmi_trap_ptr->trap_specific = DMI_SpecTrap_Subscription_exp;
           else
              dmi_trap_ptr->trap_specific = DMI_SpecTrap_Subscription_warning;

           if (buildVarbData(event_type, argp)) {
                free(dmi_trap_ptr);
                return(DMIERR_ILLEGAL_HANDLE);
           }
                             break;
         default:
                  return(DMIERR_ILLEGAL_HANDLE);
     }

     if (trap_forward_to_magent)
           rc = generateTrap(dmi_trap_ptr);
     
     free(dmi_trap_ptr);

     if (rc)
       return(DMIERR_ILLEGAL_HANDLE);
     else
       return(DMIERR_NO_ERROR);
     
}

void *thr_addToXlate(void *thr_arg) {
Thr_Param *arg_ptr;

   
   arg_ptr = (Thr_Param *)thr_arg;
   
 mutex_lock(&DmiAccessMutex);
 addToXlate(arg_ptr->compid, arg_ptr->compname, arg_ptr->xlatelist); 
 mutex_unlock(&DmiAccessMutex);
 return NULL;
}

void *thr_rebuildXlate(void *thr_arg) {

 mutex_lock(&DmiAccessMutex);
 dpiUnreg(XlateList);
 if (XlateList) freeXlate(); 
 buildXlate(&XlateList);
 dpiRegister(XlateList); 
 mutex_unlock(&DmiAccessMutex);
 return NULL;
}

