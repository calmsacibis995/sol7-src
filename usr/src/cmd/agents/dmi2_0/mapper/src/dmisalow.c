/* Copyright 10/11/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisalow.c	1.16 96/10/11 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisalow.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support DMI access.             */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        dmiRegister(...)            */
/*                                        dmiUnregister(...)          */
/*                                        issueListComp(...)          */
/*                                        issueListGroup(...)         */
/*                                        issueListAttribute(...)     */
/*                                        issueListDesc(...)          */
/*                                        issueGetAttribute(...)      */
/*                                        issueGetRow(...)            */
/*                                        buildSetAttribute(...)      */
/*                                        issueSetAttribute(...)      */
/*                                        findAttributeAccess(...)    */
/*                                        findAttributeType(...)      */
/*                                        traceReqBuf(...)            */
/*                                        traceKey(...)               */
/*                                        doSemCleanup(...)           */
/*                                        doSemTraceCleanup(...)      */
/*                                        doDmiSetup(...)             */
/*                                        xlateType(...)              */
/*                                        dmiLength(...)              */
/*                                        pitoa(...)                  */
/*                                        prepareForDmiInvoke(...)    */
/*                                        awaitDmiResponse(...)       */
/*                                                                    */
/* End Module Description *********************************************/
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
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  940609    LAUBLI    New module                    */
/*                  940728    GEISER    Added issueListDesc function  */
/*                  950131    LAUBLI    Terminate trace strings:dmiReg*/
/*                  950221    LAUBLI    Resurrected doSemCleanup()    */
/*                  950317    LAUBLI    Added some malloc checks      */
/*                  950322    LAUBLI    Added doSemTraceCleanup()     */
/*                  960701    JITEN     Converting 1.1 to 2.0         */
/*                                                                    */
/* End Change Log *****************************************************/

#include "dmisa.h"     /* global header file for DMI Sub-agent        */

extern char logbuffer[];
#ifdef DMISA_TRACE
void traceReqBuf(DMI_MgmtCommand_t *mgmtreq);
int  traceKey(void *bufbase, ULONG keycount, ULONG keyoffset);

extern unsigned char logbuffer[];
#endif

#ifdef WIN32
CRITICAL_SECTION CriticalSection;
#endif


/*DMI2.0 specific */
extern DmiRpcHandle dmi_rpchandle;
extern char  *default_configdir;
extern char  *default_sub_warntime;
extern char  *default_sub_exptime;
extern int    default_failure_threshold;
extern int    default_trap_forward_to_magent; 
extern char  *configdir;
extern char  *sub_warntime;
extern char  *sub_exptime;
extern int    failure_threshold;
extern int    trap_forward_to_magent; 


/**********************************************************************/
/* Function: dmiRegister()                                            */
/*                                                                    */
/* Attempt to register with the DMI Service Layer                     */
/*                                                                    */
/* Notes:                                                             */
/*   1. Offsets into confirm buffer not checked, so may cause an      */
/*      addressing exception when tracing level >= 4.                 */
/*                                                                    */
/* Reason codes:  DMI_REGISTER_noError                                */
/*                DMI_REGISTER_failed                                 */
/*                DMI_REGISTER_badLevelCheck                          */
/*                DMI_REGISTER_badCmdHandle                           */
/*                DMI_REGISTER_outOfMemroy                            */
/*                DO_DMI_SETUP_noError                                */
/*                DO_DMI_SETUP_createSemFailed                        */
/*                DO_DMI_SETUP_createQueueFailed                      */
/*                DO_INDICATE_SETUP_...                               */
/*                PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*                AWAIT_DMI_RESPONSE_timeoutSem (DMI broke)           */
/*                AWAIT_DMI_RESPONSE_interruptSem                     */
/*                AWAIT_DMI_RESPONSE_postSemError                     */
/*                AWAIT_DMI_RESPONSE_otherSemError                    */
/*                AWAIT_DMI_RESPONSE_readQueueFailed                  */
/*                AWAIT_DMI_RESPONSE_otherQueueError                  */
/**********************************************************************/
int dmiRegister(ULONG *status)
{
DmiRegisterIN   regin;
DmiRegisterOUT  regout;
ConnectIN       connectin;
DmiGetVersionIN  versionin;
DmiGetVersionOUT versionout;
int              bytecount;

   connectin.host = SP_hostname; 
   connectin.nettype = "netpath";
   connectin.servertype = MISERVER;
   connectin.rpctype = ONC;
   
   if (!ConnectToServer(&connectin, &dmi_rpchandle)) {
           return DMI_REGISTER_failed;
   }
  
   regin.handle = 0;
   DmiRegister(regin, &regout, &dmi_rpchandle);
   if (regout.error_status != DMIERR_NO_ERROR) {
          /*dmi_error(regout.error_status); */
          return DMI_REGISTER_failed;
   }
   DmiHandle = (unsigned long) *regout.handle;

   versionin.handle = DmiHandle;
   DmiGetVersion(versionin, &versionout, &dmi_rpchandle);
   if (versionout.error_status != DMIERR_NO_ERROR) {
        /*dmi_error(versionout.error_status);  */
        return DMI_REGISTER_failed;
   }else {
      
   if ((versionout.dmiSpecLevel->body.body_len) >  sizeof(SpecLevel))
           bytecount = sizeof(SpecLevel);
   else
           bytecount = versionout.dmiSpecLevel->body.body_len;
   memset(SpecLevel, 0, sizeof(SpecLevel));
   memcpy(SpecLevel, versionout.dmiSpecLevel->body.body_val, bytecount);

   if ((versionout.description->body.body_len) >  sizeof(SLDescription))
           bytecount = sizeof(SLDescription);
   else
           bytecount = versionout.description->body.body_len;
   memset(SLDescription, 0, sizeof(SLDescription));
   memcpy(SLDescription, versionout.description->body.body_val, bytecount);
   }
   if (status)
      *status = DMI_REGISTER_noError;
   return DMI_REGISTER_noError;
}

/**********************************************************************/
/* Function: dmiUnregister()                                          */
/*                                                                    */
/* Input parameters:                                                  */
/*    ID         - component ID                                       */
/*    Group - group ID                                                */
/*    Attrib   - attribute ID                                         */
/*    flag     - command (0=next,1=first,10=listgroup)                */
/*    Handle- handle of sub-agent DMI mgmt. application (only for flag=10) */
/*                                                                    */
/* Reason codes:  DMI_UNREGISTER_noError                              */
/*                DMI_UNREGISTER_failed                               */
/**********************************************************************/
int dmiUnregister(int exceptionprocflag, ULONG dmireqhandle)
{
DmiUnregisterIN   unregisterin;
DmiUnregisterOUT  unregisterout; 

   unregisterin.handle = DmiHandle;
   DmiUnregister(unregisterin, &unregisterout, &dmi_rpchandle);
   if (unregisterout.error_status != DMIERR_NO_ERROR) {
        /*dmi_error(unregisterout.error_status); */
        return DMI_UNREGISTER_failed;
   }
   if (!DisconnectToServer(&dmi_rpchandle)) {
        return DMI_UNREGISTER_failed;
   } 

   return DMI_UNREGISTER_noError;
}

/**********************************************************************/
/* Function: issueListComp()                                          */
/*                                                                    */
/* Input parameters:                                                  */
/*    compi        - component ID                                     */
/*    flag         - command (0=next,1=first,10=listgroup)            */
/*    maxCount -  # of components demanded                            */
/*                                                                    */
/* Output:                                                            */
/*    Ptr. to confirm buffer for the requested component              */
/*    NULL ptr. if DMI command in Mgmt block is of an unexpected type */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *listcompout points to storage allocated but not freed        */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/*                                                                    */
/* Reason codes:  ISSUE_LIST_COMP_noError                             */
/*                ISSUE_LIST_COMP_failed                              */
/*                ISSUE_LIST_COMP_outOfMemory                         */
/**********************************************************************/
int issueListComp(DmiListComponentsOUT **listcompout,ULONG dummy,
                  ULONG compi,SHORT flag,ULONG maxCount)
{
   DmiListComponentsIN     listcompin;

 *listcompout = (DmiListComponentsOUT *)calloc(1,sizeof(DmiListComponentsOUT));

   if (!*listcompout) return ISSUE_LIST_COMP_outOfMemory;

   memset((char *)&listcompin,0,sizeof(DmiListComponentsIN)); 

   listcompin.handle = DmiHandle;

   switch(flag) {
      case DMISA_FIRST:
         listcompin.requestMode = DMI_FIRST;
         break;
      case DMISA_NEXT:
         listcompin.requestMode = DMI_NEXT;
         break;
      case DMISA_THIS:
         listcompin.requestMode = DMI_UNIQUE;
         break;
   }

   listcompin.maxCount = maxCount;  /*The count is demanded by the caller*/
   listcompin.getPragma = 1;
   listcompin.getDescription = 1;
   listcompin.compId = compi;


   DmiListComponents(listcompin, *listcompout, &dmi_rpchandle);
   if ((*listcompout)->error_status != DMIERR_NO_ERROR) {
          /*dmi_error((*listcompout)->error_status); */
          return ISSUE_LIST_COMP_failed; 
   }
  

   return ISSUE_LIST_COMP_noError;  
}

/**********************************************************************/
/* Function: issueListGroup()                                         */
/*                                                                    */
/* Input parameters:                                                  */
/*   compi         - component ID                                     */
/*   groupi - group ID                                                */
/*   flag     - command (0=next,1=first,10=listgroup)                 */
/*   maxCount - # of groups within a component demanded               */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *listgroupout points to storage allocated but not freed      */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Reason codes:  ISSUE_LIST_GROUP_noError                            */
/*                ISSUE_LIST_GROUP_failed                             */
/*                ISSUE_LIST_GROUP_outOfMemory                        */
/**********************************************************************/
int issueListGroup(DmiListGroupsOUT **listgroupout,ULONG compi,ULONG groupi,SHORT flag,ULONG maxCount)
{


   DmiListGroupsIN     listgroupin;

  *listgroupout = (DmiListGroupsOUT *)calloc(1,sizeof(DmiListGroupsOUT));

   if (!*listgroupout) return ISSUE_LIST_GROUP_outOfMemory;

   memset((char *)&listgroupin,0,sizeof(DmiListGroupsIN)); 

   listgroupin.handle = DmiHandle;

   switch(flag) {
      case DMISA_FIRST:
         listgroupin.requestMode = DMI_FIRST;
         break;
      case DMISA_NEXT:
         listgroupin.requestMode = DMI_NEXT;
         break;
      case DMISA_THIS:
         listgroupin.requestMode = DMI_UNIQUE;
         break;
   }

   listgroupin.maxCount = maxCount;  /*demanded by the caller*/
   listgroupin.getPragma = 1;
   listgroupin.getDescription = 1;
   listgroupin.compId = compi;
   listgroupin.groupId = groupi;


   DmiListGroups(listgroupin, *listgroupout, &dmi_rpchandle);
   if ((*listgroupout)->error_status != DMIERR_NO_ERROR) {
          /*dmi_error((*listgroupout)->error_status); */
          return ISSUE_LIST_GROUP_failed; 
   }
  

   return ISSUE_LIST_GROUP_noError;  

}

/**********************************************************************/
/* Function: issueListAttribute()                                     */
/*                                                                    */
/* Input parameters:                                                  */
/*   compi    - component ID                                          */
/*   groupi   - group ID                                              */
/*   Attrib   - attribute ID                                          */
/*   flag     - command (0=next,1=first,10=listgroup)                 */
/*   Handle- handle of sub-agent DMI mgmt. application (only for flag=10)*/
/*                                                                    */
/* Output:                                                            */
/*       Ptr. to confirm buffer for the requested description         */
/*       NULL ptr. if DMI command in Mgmt block is of an unexpected type */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *listattrout points to storage allocated but not freed        */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Reason codes:  ISSUE_LIST_ATTRIBUTE_noError                        */
/*                ISSUE_LIST_ATTRIBUTE_failed                         */
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                    */
/**********************************************************************/
int issueListAttribute(DmiListAttributesOUT **listattrout ,ULONG compi,ULONG groupi, ULONG attribi,SHORT flag,ULONG maxCount)
{


   DmiListAttributesIN     listattrin;

 *listattrout = (DmiListAttributesOUT *)calloc(1,sizeof(DmiListAttributesOUT));

   if (!*listattrout) return ISSUE_LIST_ATTRIBUTE_outOfMemory;

   memset((char *)&listattrin,0,sizeof(DmiListAttributesIN)); 

   listattrin.handle = DmiHandle;

   switch(flag) {
      case DMISA_FIRST:
         listattrin.requestMode = DMI_FIRST;
         break;
      case DMISA_NEXT:
         listattrin.requestMode = DMI_NEXT;
         break;
      case DMISA_THIS:
         listattrin.requestMode = DMI_UNIQUE;
         break;
   }

   listattrin.maxCount = maxCount;  /*demanded by the caller*/
   listattrin.getPragma = 1;
   listattrin.getDescription = 1;
   listattrin.compId = compi;
   listattrin.groupId = groupi;
   listattrin.attribId = attribi;


   DmiListAttributes(listattrin, *listattrout, &dmi_rpchandle);
   if ((*listattrout)->error_status != DMIERR_NO_ERROR) {
          /*dmi_error((*listattrout)->error_status);  */
          return ISSUE_LIST_ATTRIBUTE_failed; 
   }
  

   return ISSUE_LIST_ATTRIBUTE_noError;  

}

/**********************************************************************/
/* Function: issueListDescription()                                   */
/*                                                                    */
/* Input parameters:                                                  */
/*   compi         - component ID                                     */
/*   groupi        - group ID                                         */
/*   attri           - attribute ID                                   */
/*   flag     - command --  list desc comp, list desc group, or list desc attri */
/*   dmireqhandle- handle of sub-agent DMI mgmt. application (only for flag=10) */
/* Output:                                                            */
/*   Ptr. to desc.   buffer for the requested description             */
/*   NULL ptr. if DMI command in Mgmt block is of an unexpected type  */
/*                                                                    */
/* Notes:                                                             */
/*   1.  description points to storage allocated but not freed        */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Reason codes:  ISSUE_LIST_DESCRIPTION_noError                      */
/*                ISSUE_LIST_DESCRIPTION_failed                       */
/*                ISSUE_LIST_DESCRIPTION_outOfMemory                  */
/**********************************************************************/
int issueListDesc(char * description,ULONG compi,ULONG groupi,ULONG attri,
                         SHORT flag)
{

DmiListComponentsOUT  *listcompout;
DmiListGroupsOUT      *listgroupout;
DmiListAttributesOUT  *listattrout;
DmiAttributeInfo_t    *attrinfo;
DmiGroupInfo_t        *groupinfo;
DmiComponentInfo_t    *compinfo;
ULONG                  dummy=0L;

   switch(flag){
      case DMISA_COMPDESC:
      if (issueListComp( &listcompout,dummy,compi,DMISA_THIS,1)) {
           if (listcompout)
              free_listcompout(listcompout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }
      
      if (!listcompout->reply) { 
           free_listcompout(listcompout);
           return ISSUE_LIST_DESCRIPTION_failed;
      } 
      if (listcompout->reply->list.list_len > 0) {
         compinfo = listcompout->reply->list.list_val;
         description = (char *)calloc(1,compinfo->description->body.body_len+1);
         if (!description) {
           free_listcompout(listcompout);
           return ISSUE_LIST_DESCRIPTION_outOfMemory;
         }
         memcpy(description, compinfo->description->body.body_val,
                             compinfo->description->body.body_len);
         free_listcompout(listcompout);   
      } else {
         free_listcompout(listcompout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }
         break;
      case DMISA_GROUPDESC:
      if (issueListGroup(&listgroupout,compi,groupi,DMISA_THIS,1)) {
           if (listgroupout)
             free_listgroupout(listgroupout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }

      if (!listgroupout->reply) { 
           free_listgroupout(listgroupout);
           return ISSUE_LIST_DESCRIPTION_failed;
      } 
      if (listgroupout->reply->list.list_len > 0) {
         groupinfo = listgroupout->reply->list.list_val;
         description = (char *)calloc(1,groupinfo->description->body.body_len+1);
         if (!description) {
           free_listgroupout(listgroupout);
           return ISSUE_LIST_DESCRIPTION_outOfMemory;
         }
         memcpy(description, groupinfo->description->body.body_val,
                             groupinfo->description->body.body_len);
         free_listgroupout(listgroupout);   
      } else {
         free_listgroupout(listgroupout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }

         break;
      case DMISA_ATTRDESC:
      if (issueListAttribute(&listattrout ,compi,groupi,attri,DMISA_THIS,1)) {
           if (listattrout)
                free_listattrout(listattrout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }

      if (!listattrout->reply) { 
           free_listattrout(listattrout);
           return ISSUE_LIST_DESCRIPTION_failed;
      } 
      if (listattrout->reply->list.list_len > 0) {
         attrinfo = listattrout->reply->list.list_val;
         description = (char *)calloc(1,attrinfo->description->body.body_len+1);
         if (!description) {
           free_listattrout(listattrout);
           return ISSUE_LIST_DESCRIPTION_outOfMemory;
         }
         memcpy(description, attrinfo->description->body.body_val,
                             attrinfo->description->body.body_len);
         free_listattrout(listattrout);   
      } else {
         free_listattrout(listattrout);
           return ISSUE_LIST_DESCRIPTION_failed;
      }
         break;
   }

   
   return ISSUE_LIST_DESCRIPTION_noError;  
}

/**********************************************************************/
/* Function: issueGetAttribute()                                      */
/*                                                                    */
/* Input parameters:                                                  */
/*   getattrout - ptr. to ptr. of request buffer, yet to be allocated */
/*   compi      - component ID                                        */
/*   groupi     - group ID                                            */
/*   attribi    - attribute ID                                        */
/*   keycount   - number of attributes in the key                     */
/*   keyblock   - ptr. to DmiGroupKeyData blocks and key values       */
/*   Handle     - handle of sub-agent DMI mgmt. application           */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *getattrout points to storage allocated but not freed        */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Reason codes:  ISSUE_GET_ATTRIBUTE_noError                         */
/*                ISSUE_GET_ATTRIBUTE_failed                          */
/*                ISSUE_GET_ATTRIBUTE_outOfMemory                     */
/**********************************************************************/
int issueGetAttribute(DmiGetAttributeOUT **getattrout, ULONG compi,
           ULONG groupi,ULONG attribi, ULONG keycount, ULONG keyblocklen,
           DMI_GroupKeyData_t *keyblock)
{

  DmiGetAttributeIN       getattrin;
  DmiAttributeValues_t    *keylist;


   keylist = (DmiAttributeValues_t *)calloc(1, sizeof(DmiAttributeValues_t));
   if (!keylist)
        return ISSUE_GET_ATTRIBUTE_outOfMemory;
   *getattrout = (DmiGetAttributeOUT *)calloc(1,sizeof(DmiGetAttributeOUT));
   if (!*getattrout) {
        free(keylist);
        return ISSUE_GET_ATTRIBUTE_outOfMemory;
   }
   
   groupkeydata_to_keylist(keyblock, keylist, keycount);

   memset((char *)&getattrin, 0, sizeof(DmiGetAttributeIN));

   getattrin.handle = DmiHandle; 
   getattrin.compId = compi;
   getattrin.groupId = groupi;
   getattrin.attribId = attribi;
   getattrin.keyList = keylist;
   
   DmiGetAttribute(getattrin,  *getattrout, &dmi_rpchandle);
   if ((*getattrout)->error_status != DMIERR_NO_ERROR) {
        /*dmi_error((*getattrout)->error_status); */
        free_attrvalues(keylist);
        /*free_getattrout(*getattrout); free in calling routine*/
        return ISSUE_GET_ATTRIBUTE_failed;
   } 
        free_attrvalues(keylist);

   return ISSUE_GET_ATTRIBUTE_noError;

}


/**********************************************************************/
/* Function: issueGetRow()                                            */
/*                                                                    */
/* Input parameters:                                                  */
/*   **getmultout - ptr. to ptr. of yet-to-be-allocated Confirm buffer*/
/*   compi        - component ID                                      */
/*   groupi       - group ID                                          */
/*   attribi      - attribute ID                                      */
/*   Handle       - handle of sub-agent DMI mgmt. application         */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *getmultout points to storage allocated but not freed        */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Reason codes:  ISSUE_GET_ROW_noError                               */
/*                ISSUE_GET_ROW_failed                                */
/*                ISSUE_GET_ROW_outOfMemory                           */
/**********************************************************************/
int issueGetRow(DmiGetMultipleOUT **getmultout,ULONG compi,ULONG groupi,
            ULONG attribi, ULONG keycount, ULONG keyblocklen,
            DMI_GroupKeyData_t *keyblock, SHORT flag)
{

   DmiGetMultipleIN      *getmultin;
   DmiAttributeValues_t    *keylist;
   DmiRowRequest_t         *rowreq;
   DmiAttributeIds         *attrid;


   attrid= (DmiAttributeIds *)calloc(1, sizeof(DmiAttributeIds));
   if (!attrid)
        return ISSUE_GET_ROW_outOfMemory;
   getmultin= (DmiGetMultipleIN *)calloc(1, sizeof(DmiGetMultipleIN));
   if (!getmultin) {
        free(attrid);
        return ISSUE_GET_ROW_outOfMemory;
   }
   getmultin->request = (DmiMultiRowRequest_t *)calloc(1,
                                        sizeof(DmiMultiRowRequest_t));
   if (!getmultin->request) {
        free(attrid);
        free(getmultin);
        return ISSUE_GET_ROW_outOfMemory;
   }
   keylist = (DmiAttributeValues_t *)calloc(1, sizeof(DmiAttributeValues_t));
   if (!keylist) {
        free(attrid);
        free(getmultin->request);
        free(getmultin);
        return ISSUE_GET_ROW_outOfMemory;
   }
   *getmultout = (DmiGetMultipleOUT *)calloc(1,sizeof(DmiGetMultipleOUT));
   if (!*getmultout) {
        free(attrid);
        free(getmultin->request);
        free(getmultin);
        free(keylist);
        return ISSUE_GET_ROW_outOfMemory;
   }
   
   groupkeydata_to_keylist(keyblock, keylist, keycount);

   getmultin->handle = DmiHandle; 
    
   getmultin->request->list.list_len = 1;
   rowreq= getmultin->request->list.list_val = (DmiRowRequest_t *)calloc (1,
                                               sizeof(DmiRowRequest_t));
   if (!getmultin->request->list.list_val) {
        free(getmultin->request);
        free(getmultin);
        free_attrvalues(keylist);
        /*free(*getmultout); free in calling routine*/
        free(attrid);
        return(ISSUE_GET_ROW_outOfMemory);
   }
       


   rowreq->compId = compi;
   rowreq->groupId = groupi;

   switch(flag) {
      case DMISA_FIRST:
         rowreq->requestMode = DMI_FIRST;
         break;
      case DMISA_NEXT:
         rowreq->requestMode = DMI_NEXT;
         break;
      case DMISA_THIS:
         rowreq->requestMode = DMI_UNIQUE;
         break;
   }
  if (flag == DMISA_FIRST) 
   rowreq->keyList = NULL; /*keylist; */
  else
   rowreq->keyList = keylist;

   rowreq->ids = NULL; /*attrid; */
#if 0
   rowreq->ids->list.list_len = 1; 
   rowreq->ids->list.list_val = (DmiId_t *)calloc(1,sizeof(DmiId_t));
   if (!rowreq->ids->list.list_val) {
        free(getmultin->request->list.list_val);
        free(getmultin->request);
        free(getmultin);
        free_attrvalues(keylist);
        /*free(*getmultout);*/
        free(attrid);
        return(ISSUE_GET_ROW_outOfMemory);
   }
   *rowreq->ids->list.list_val = (DmiId_t)(++attribi);
#endif
   
/*******/

   DmiGetMultiple(*getmultin,  *getmultout, &dmi_rpchandle);
   if ((*getmultout)->error_status != DMIERR_NO_ERROR) {
        /*dmi_error((*getmultout)->error_status); */
        free(getmultin->request->list.list_val);
        free(getmultin->request);
        free(getmultin);
        free_attrvalues(keylist);
        /*free(*getmultout);*/
        free(attrid);
        return (ISSUE_GET_ROW_failed);
   } 

        free(getmultin->request->list.list_val);
        free(getmultin->request);
        free(getmultin);
        free_attrvalues(keylist);
        free(attrid);
   return ISSUE_GET_ROW_noError;
}

/**********************************************************************/
/* Function: buildSetAttribute()                                      */
/*                                                                    */
/* Build a RESERVE, SET, or RELEASE request buffer, to be sent to the */
/* DMI service layer.                                                 */
/*                                                                    */
/* Notes:                                                             */
/*   1.*setattribin must be set to NULL before entering this routine, */
/*       but only for the first attribute placed in any given request */
/*       block.                                                       */
/*   2.  *setattribin points to storage allocated but not freed       */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/* Input parameters:                                                  */
/*  setattribin - ptr. to ptr. of request buffer, yet to be allocated */
/*   compi      - component ID                                        */
/*   groupi     - group ID                                            */
/*   attribi    - attribute ID                                        */
/*   keycount   - number of attributes in the key                     */
/*   keyblock   - ptr. to DmiGroupKeyData blocks and key values       */
/*   Handle     - handle of sub-agent DMI mgmt. application           */
/*                                                                    */
/* Reason codes:  BUILD_SET_ATTRIBUTE_noError                         */
/*                BUILD_SET_ATTRIBUTE_failed                          */
/*                BUILD_SET_ATTRIBUTE_outOfMemory                     */
/**********************************************************************/
int buildSetAttribute(DmiSetAttributeIN **setattribin, 
           ULONG groupi, ULONG attribi, ULONG keycount,
           ULONG keyblocklen, DMI_GroupKeyData_t *keyblock,
           ULONG attribvaluelen, ULONG attributetype, void *attributevalue)
/* setattrib->iComponentId = Attrib->Parent->Parent->ElementID;   // set to the currently selected component*/
/* setattrib->DmiSetAttributeList[0].iGroupId = Attrib->Parent->ElementID;*/
/* setattrib->DmiSetAttributeList[0].iAttributeId = Attrib->ElementID;*/
{

  DmiAttributeValues_t  *keylist;
  DmiDataUnion_t        *datavalue;
  


   *setattribin = (DmiSetAttributeIN *)calloc(1, sizeof(DmiSetAttributeIN));
   if (!*setattribin) {
        return BUILD_SET_ATTRIBUTE_outOfMemory;
   }
   keylist = (DmiAttributeValues_t *)calloc(1, sizeof(DmiAttributeValues_t));
   if (!keylist) {
        free(*setattribin);
        return BUILD_SET_ATTRIBUTE_outOfMemory;
   }
   datavalue = (DmiDataUnion_t *)calloc(1, sizeof(DmiDataUnion_t));
   if (!datavalue) {
        free(*setattribin);
        free(keylist);
        return BUILD_SET_ATTRIBUTE_outOfMemory;
   }
   datavalue->DmiDataUnion_u.str = (DmiString_t *)calloc(1,sizeof(DmiString_t));
   if (!datavalue->DmiDataUnion_u.str) {
        free(*setattribin);
        free(keylist);
        free(datavalue);
        return BUILD_SET_ATTRIBUTE_outOfMemory;
   }
   

   groupkeydata_to_keylist(keyblock, keylist, keycount);

/* The setMode, and the compId fields to be set in issueSetAttribute */
   (*setattribin)->handle = DmiHandle;
   (*setattribin)->groupId = groupi;
   (*setattribin)->attribId = attribi;
    
   (*setattribin)->keyList = keylist;
   (*setattribin)->value = datavalue;
   
   switch(attributetype) {
           case MIF_COUNTER:
                  datavalue->type = MIF_COUNTER;
                  memcpy((char *)&datavalue->DmiDataUnion_u.counter,
                         (char *)attributevalue,
                                 attribvaluelen);
                          break;
           case MIF_COUNTER64:
                  datavalue->type = MIF_COUNTER64;
                  memcpy((char *)&datavalue->DmiDataUnion_u.counter64,
                         (char *)attributevalue,
                                 attribvaluelen);
                          break;
           case MIF_GAUGE: 
                  datavalue->type = MIF_GAUGE;
                  memcpy((char *)&datavalue->DmiDataUnion_u.gauge,
                         (char *)attributevalue,
                                 attribvaluelen);
                          break;
           case MIF_INTEGER:
                  datavalue->type = MIF_INTEGER;
                  memcpy((char *)&datavalue->DmiDataUnion_u.integer,
                         (char *)attributevalue,
                                 attribvaluelen);
                          break;
           case MIF_INTEGER64:
                  datavalue->type = MIF_INTEGER64;
                  memcpy((char *)&datavalue->DmiDataUnion_u.integer64,
                         (char *)attributevalue,
                                 attribvaluelen);
                          break;
           case MIF_OCTETSTRING: 
                  datavalue->type = MIF_OCTETSTRING;
                  datavalue->DmiDataUnion_u.octetstring->body.body_len =
                                  attribvaluelen;
                  datavalue->DmiDataUnion_u.octetstring->body.body_val =
                   (char *)calloc(1, attribvaluelen+1);
          memcpy((char *)datavalue->DmiDataUnion_u.octetstring->body.body_val,
                 (char *)attributevalue,
                         attribvaluelen);
                          break;
           case MIF_DISPLAYSTRING: 
                  datavalue->type = MIF_DISPLAYSTRING;
                  datavalue->DmiDataUnion_u.str->body.body_len =
                                  attribvaluelen;
                  datavalue->DmiDataUnion_u.str->body.body_val =
                   (char *)calloc(1, attribvaluelen+1);
          memcpy((char *)datavalue->DmiDataUnion_u.str->body.body_val,
                 (char *)attributevalue,
                         attribvaluelen);
                          break;
           case MIF_DATE: 
                  datavalue->type = MIF_DATE;
                  datavalue->DmiDataUnion_u.date =
                   (DmiTimestamp_t *)calloc(1, sizeof(DmiTimestamp_t));
          memcpy((char *)datavalue->DmiDataUnion_u.date,
                 (char *)attributevalue,
                         attribvaluelen);
                  
                          break;
           default:
  
                   free(*setattribin);
                   free_attrvalues(keylist);
                   free(datavalue);
                   return BUILD_SET_ATTRIBUTE_failed;
                          break;

   }
   
   return BUILD_SET_ATTRIBUTE_noError;  
}



/**********************************************************************/
/* Function: issueSetAttribute()                                      */
/*                                                                    */
/* Input parameters:                                                  */
/*   setattribin- ptr. to ptr. of request buffer, allocated by        */
/*                                buildSetAttribute                   */
/*   compi      - component ID                                        */
/*   groupi     - group ID                                            */
/*   attribi    - attribute ID                                        */
/*   keycount   - number of attributes in the key                     */
/*   keyblock   - ptr. to DmiGroupKeyData blocks and key values       */
/*   Handle     - handle of sub-agent DMI mgmt. application           */
/*                                                                    */
/* Notes:                                                             */
/*   1.  (*setattrib)->DmiMgmtCommand.pCnfBuf points to storage       */
/*       allocated but not freed by this function.  It must be freed  */
/*       by the caller.                                               */
/*                                                                    */
/* Reason codes:  ISSUE_SET_ATTRIBUTE_noError                         */
/*                ISSUE_SET_ATTRIBUTE_failed                          */
/*                ISSUE_SET_ATTRIBUTE_outOfMemory                     */
/**********************************************************************/
int issueSetAttribute(DmiSetAttributeIN **setattribin, ULONG compi, ULONG cmd)
{


   DmiSetAttributeOUT   attribout;

   (*setattribin)->compId = compi;
   switch(cmd) {
     case DmiSetReserveAttributeCmd:    
         (*setattribin)->setMode = DMI_RESERVE;
         break;
     case DmiSetAttributeCmd:    
         (*setattribin)->setMode = DMI_SET;
         break;
     case DmiSetReleaseAttributeCmd:    
         (*setattribin)->setMode = DMI_RELEASE;
         break;
   }


   DmiSetAttribute(**setattribin, &attribout, &dmi_rpchandle);
   if (attribout.error_status != DMIERR_NO_ERROR) {
          /*dmi_error(attribout.error_status); */
          return ISSUE_SET_ATTRIBUTE_failed;
   }

   return ISSUE_SET_ATTRIBUTE_noError;
}


/*********************************************************************/
/* Function: findAttributeAccess()                                   */
/*                                                                   */
/* Determine the attribute access information for the specified      */
/* attribute.                                                        */
/*                                                                   */
/* Return codes: FIND_ATTRIBUTE_ACCESS_noError                       */
/*               FIND_ATTRIBUTE_ACCESS_notFound                      */
/*               FIND_ATTRIBUTE_ACCESS_otherError                    */
/*               FIND_ATTRIBUTE_ACCESS_dmiBroke                      */
/*               ISSUE_LIST_ATTRIBUTE_outOfMemory                    */
/*********************************************************************/
int findAttributeAccess(int *access, ULONG compi, ULONG groupi, ULONG attributei)
{
   DmiListAttributesOUT *listattrout;
   int     rcsub, rc = FIND_ATTRIBUTE_ACCESS_noError;
   int     acc;

   rcsub = issueListAttribute(
           &listattrout,
           compi,
           groupi,
           attributei,
           DMISA_THIS,
           1);

   if (rcsub == ISSUE_LIST_ATTRIBUTE_noError) {
      if (listattrout->reply->list.list_len == 1) {
           acc = (int )listattrout->reply->list.list_val->access; 
           *access =  acc & MIF_ACCESS_MODE_MASK;  /* Bitwise AND*/
      }
      else
           rc = FIND_ATTRIBUTE_ACCESS_otherError;
   }else {
           rc = FIND_ATTRIBUTE_ACCESS_notFound;
   }

   if (listattrout) {
       free_listattrout(listattrout);
   }

   return rc;
}


/*********************************************************************/
/* Function: findAttributeType()                                     */
/*                                                                   */
/* Determine the attribute type of the specified attribute.          */
/*                                                                   */
/* Return codes: FIND_ATTRIBUTE_TYPE_noError                         */
/*               FIND_ATTRIBUTE_TYPE_notFound                        */
/*               FIND_ATTRIBUTE_TYPE_otherError                      */
/*               FIND_ATTRIBUTE_TYPE_dmiBroke                        */
/*               ISSUE_LIST_ATTRIBUTE_outOfMemory                    */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*********************************************************************/
int findAttributeType(ULONG *type, ULONG compi, ULONG groupi, ULONG attributei)
{

   DmiListAttributesOUT *listattrout;
   int     rcsub, rc = FIND_ATTRIBUTE_ACCESS_noError;
   int     acc;

   rcsub = issueListAttribute(
           &listattrout,
           compi,
           groupi,
           attributei,
           DMISA_THIS,
           1);

   if (rcsub == ISSUE_LIST_ATTRIBUTE_noError) {
      if (listattrout->reply->list.list_len == 1) {
           *type = (int )listattrout->reply->list.list_val->type;
      }
      else
           rc = FIND_ATTRIBUTE_ACCESS_otherError;
   }else {
           rc = FIND_ATTRIBUTE_ACCESS_notFound;
   }

   if (listattrout) {
       free_listattrout(listattrout);
   }

   return rc;

}

#ifdef DMISA_TRACE
/**********************************************************************/
/* Function: traceReqBuf()                                            */
/*                                                                    */
/* Input parameters:                                                  */
/*         ID         - component ID                                  */
/*         Group - group ID                                           */
/*         Attrib   - attribute ID                                    */
/*         flag     - command (0=next,1=first,10=listgroup)           */
/*         Handle- handle of sub-agent DMI mgmt. application (only for flag=10) */
/*                                                                    */
/* Reason codes:  none                                                */
/**********************************************************************/
void traceReqBuf(DMI_MgmtCommand_t *mgmtreq)
{
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iLevelCheck  %u",mgmtreq->iLevelCheck);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iCommand     %X",mgmtreq->iCommand);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iMgmtHandle  %u",mgmtreq->iMgmtHandle);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iCmdHandle   %u",mgmtreq->iCmdHandle);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.osLanguage   %X",mgmtreq->osLanguage);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iCnfBufLen   %u",mgmtreq->iCnfBufLen);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.*pCnfBuf     %p",mgmtreq->pCnfBuf);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iRequestCount%u",mgmtreq->iRequestCount);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iCmdLen      %u",mgmtreq->iCmdLen);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iCnfCount    %u",mgmtreq->iCnfCount);
   DMISA_TRACE_LOG1(LEVEL4,"        DmiMgmtCommand.iStatus      %X",mgmtreq->iStatus);
}


/**********************************************************************/
/* Function:  traceKey()                                              */
/*                                                                    */
/*     Input parameters:                                              */
/*         ID         - component ID                                  */
/*         Group - group ID                                           */
/*         Attrib   - attribute ID                                    */
/*         flag     - command (0=next,1=first,10=listgroup)           */
/*         Handle- handle of sub-agent DMI mgmt. application (only for flag=10) */
/*                                                                    */
/* Reason codes:  TRACE_KEY_noError                                   */
/*                TRACE_KEY_outOfMemory                               */
/**********************************************************************/
int traceKey(void *bufbase, ULONG keycount, ULONG keyoffset)
{
   DMI_GroupKeyData_t *workkey;
   DMI_STRING         *work;
   char *temp;
   int   i, j, ln;

   workkey = (DMI_GroupKeyData_t *)((char *)bufbase + keyoffset);
   for (i=1;i<=keycount;i++,workkey++) {   /* add error handling      */
      DMISA_TRACE_LOG3(LEVEL4,"          Key attr. id,type,off  %u,%u,%X",
              workkey->iAttributeId,workkey->iAttributeType,workkey->oKeyValue);
      work = (DMI_STRING *)((char *)bufbase + workkey->oKeyValue);
      if (workkey->iAttributeType == MIF_DISPLAYSTRING) {
         temp = malloc(work->length + 1);
         if (!temp) return TRACE_KEY_outOfMemory;
         strncpy(temp,work->body,work->length);
         temp[work->length] = '\0';
         ln = sprintf(logbuffer,"          oAttributeValue          %u,",work->length);
         strncpy(logbuffer + ln, temp, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
         DMISA_TRACE_LOGBUF(LEVEL4);
         free(temp);
#if defined(OS2) || defined(WIN32)
      } else if (workkey->iAttributeType == MIF_OCTETSTRING) {
         temp = malloc(work->length * 3 + 1);
         if (!temp) return TRACE_KEY_outOfMemory;
         for (j=0; j < work->length; j++) {
             _itoa(work->body[j],&temp[j*3],16);
            temp[j*3+2] = ' ';
         }
         temp[work->length * 3] = '\0';
         ln = sprintf(logbuffer,"          oAttributeValue          %u,",work->length);
         strncpy(logbuffer + ln, temp, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
         DMISA_TRACE_LOGBUF(LEVEL4);
         free(temp);
#endif
      } else if (workkey->iAttributeType == MIF_DATE) {
         temp = malloc(28 + 1);
         if (!temp) return TRACE_KEY_outOfMemory;
         strncpy(temp,(char *)&(work->length),28);
         temp[28] = '\0';
         ln = sprintf(logbuffer,"          oAttributeValue          ");
         strncpy(logbuffer + ln, temp, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
         DMISA_TRACE_LOGBUF(LEVEL4);
         free(temp);
      } else if (workkey->iAttributeType == MIF_INTEGER64) {
         DMISA_TRACE_LOG2(LEVEL4,"          oAttributeValue          %X%X",*work,*(ULONG *)((char *)work + 4));
      } else if (workkey->iAttributeType == MIF_COUNTER64) {
         DMISA_TRACE_LOG2(LEVEL4,"          oAttributeValue          %X%X",*work,*(ULONG *)((char *)work + 4));
      } else if (workkey->iAttributeType == MIF_INTEGER) {
         DMISA_TRACE_LOG1(LEVEL4,"          oAttributeValue          %d",*work);
      } else if (workkey->iAttributeType == MIF_COUNTER ||
                 workkey->iAttributeType == MIF_GAUGE) {
         DMISA_TRACE_LOG2(LEVEL4,"          oAttributeValue          %X,%u",workkey->oKeyValue,*work);
      }
   } /* endfor */

   return TRACE_KEY_noError;
}
#endif

/**********************************************************************/
/* Function:  doDmiSetup()                                            */
/*                                                                    */
/*            Create semaphore and queue for DMI accesses             */
/*                                                                    */
/* Reason codes:  DO_DMI_SETUP_noError                                */
/*                DO_DMI_SETUP_createSemFailed                        */
/*                DO_DMI_SETUP_createQueueFailed                      */
/*                DO_INDICATE_SETUP_                                  */
/**********************************************************************/
int doDmiSetup(void)
{
APIRET rcque,rcsem,rcind;

   /* Create semaphore and queue used to pass information back from service layer*/
#ifdef OS2
   strcpy( (CHAR *)SemName,"\\SEM32\\DMISAEVENT1");
   flAttr = 0;          /* Unused, since this is a named semaphore */
   fState = 0;          /* Semaphore initial state is "reset" */
   rcsem = DosCreateEventSem( SemName, &CnfSemHandle, flAttr, fState );
                /* On successful return, CnfSemHandle contains handle of new system event semaphore*/
   if ( rcsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname, 285dupname, 290toomanyhndls*/
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateEventSem error: return code = %lu.", rcsem);
      return DO_DMI_SETUP_createSemFailed;
   } else {
      DMISA_TRACE_LOG1(LEVEL5," Event semaphore %u successfully created.",CnfSemHandle);
   }
#endif

   /* Create the queue                                                */
#ifdef WIN32                            /* LLR */
   InitializeCriticalSection(&CriticalSection);
   pQueueHead = NULL;
   strcpy( (CHAR *)SemName,"DMISAEVENT1");
   queueMutex = CreateEvent((LPSECURITY_ATTRIBUTES) NULL, TRUE, FALSE, SemName);
   (DWORD) rcque=GetLastError();

   if ( rcque != 0 ) { /* can be 0, 87invparm, 332quedup, 334quenomem, 335queinvname*/
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateQueue error: return code = %u.", rcque);
      return DO_DMI_SETUP_createQueueFailed;
   } else {
      DMISA_TRACE_LOG(LEVEL5," Queue successfully created.");
   }

#elif defined OS2
   rcque = DosCreateQueue( &QueueHandle,          /* Queue handle */
                           QUE_FIFO |             /* Ordering for elements */
                           QUE_CONVERT_ADDRESS,   /*16-bit address conversion */
                           (UCHAR *) QUE_NAME );  /* Queue name string */
   if ( rcque != 0 ) { /* can be 0, 87invparm, 332quedup, 334quenomem, 335queinvname*/
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateQueue error: return code = %u.", rcque);
      return DO_DMI_SETUP_createQueueFailed;
   } else {
      DMISA_TRACE_LOG1(LEVEL5," Queue successfully created. QueueHandle = %p.",QueueHandle);
   }
#elif AIX325
   pQueueHead = NULL;
   pthread_mutex_init(&queueMutex, pthread_mutexattr_default);
   pthread_cond_init(&queueCond, pthread_condattr_default);
#else
   pQueueHead = NULL;
   mutex_init(&queueMutex, USYNC_THREAD, NULL);
   cond_init(&queueCond, USYNC_THREAD, NULL);
   /* probably need to create this thread non-detached - play with attributes */
#endif
   rcind = dmiIndicateSetup();
   if (rcind != DMI_INDICATE_SETUP_noError) {
      DMISA_TRACE_LOG(LEVEL1, "Indication setup failed  (Create Semaphore or Thread).");
      return rcind;
   }
  return DO_DMI_SETUP_noError;
}

/**********************************************************************/
/* Function:  doSemCleanup()                                          */
/*                                                                    */
/* Clean up at end of program                                         */
/*                                                                    */
/* Reason codes:  none                                                */
/**********************************************************************/
void doSemCleanup(void)
{                                 /* added entire function 950221.gwl */
APIRET rcsem;

#if 0
APIRET rcque;

   // Close queue
   rcque = DosCloseQueue( QueueHandle );
#ifdef DMISA_TRACE
   if (rcque != 0 ) { // can be 0, 337queinvhandle
      DMISA_TRACE_LOG2(LEVEL1,"DoCloseQueue error: return code = %u, QueueHandle = %p.", rcque,QueueHandle);
   }
   else {
      DMISA_TRACE_LOG(LEVEL5,"Queue successfully closed.");
   }
#endif
#endif

#ifdef OS2
   /* Post & Close Event semaphores */
   if ((rcsem = DosPostEventSem( CnfSemHandle )) && rcsem != 299) { /* Assignment intended */
      DMISA_TRACE_LOG1(LEVEL1,"DosPostEventSem error on confirm semaphore: return code = %u", rcsem);
   }
   rcsem = DosCloseEventSem( CnfSemHandle ); /* can be 0, 6invhandle, 301sembusy */
#ifdef DMISA_TRACE
   if (rcsem) {
      DMISA_TRACE_LOG1(LEVEL1,"DosCloseEventSem error on confirm semaphore: return code = %u", rcsem);
   } else {
      DMISA_TRACE_LOG(LEVEL5," Confirm semaphore successfully closed.");
   }
#endif
   if ((rcsem = DosPostEventSem( IndSemHandle )) && rcsem != 299) { /* Assignment intended */
      DMISA_TRACE_LOG1(LEVEL1,"DosPostEventSem error on indication1 semaphore: return code = %u", rcsem);
   }
   if (rcsem = DosCloseEventSem( IndSemHandle )) { /* Assignment intended; can be 0, 6invhandle, 301sembusy */
      DMISA_TRACE_LOG1(LEVEL1,"DosCloseEventSem error on indication1 semaphore: return code = %u", rcsem);
   } else {
      DMISA_TRACE_LOG(LEVEL5," Indication1 semaphore successfully closed.");
   }

   /* Release & Close Mutex semaphores */
   if ((rcsem = DosReleaseMutexSem( Ind3SemHandle )) && rcsem != 288) { /* Assignment intended */
      DMISA_TRACE_LOG1(LEVEL1,"DosReleaseMutexSem error on Indication2 semaphore: return code = %u", rcsem);
   }
   if (rcsem = DosCloseMutexSem( Ind3SemHandle )) { /* Assignment intended; can be 0, 6invhandle, 301sembusy */
      DMISA_TRACE_LOG1(LEVEL1,"DosCloseMutexSem error on Indication2 semaphore: return code = %u", rcsem);
   } else {
      DMISA_TRACE_LOG(LEVEL5," Indication2 semaphore successfully closed.");
   }
   if ((rcsem = DosReleaseMutexSem( DmiAccessMutex )) && rcsem != 288) { /* Assignment intended */
      DMISA_TRACE_LOG1(LEVEL1,"DosReleaseMutexSem error on DmiAccess semaphore: return code = %u", rcsem);
   }
   if (rcsem = DosCloseMutexSem( DmiAccessMutex )) { /* Assignment intended; can be 0, 6invhandle, 301sembusy */
      DMISA_TRACE_LOG1(LEVEL1,"DosCloseMutexSem error on DmiAccess semaphore: return code = %u", rcsem);
   } else {
      DMISA_TRACE_LOG(LEVEL5," DmiAccess semaphore successfully closed.");
   }
// need aix too! $MED
#elif defined WIN32
   WSACleanup();                    // Discontinue connection, temporary place to put cleanup calls LLR 10-2-95
   if ((rcsem= CloseHandle( queueMutex )))  { /* can be 0, 6invhandle, 301sembusy */
	  DMISA_TRACE_LOG(LEVEL5," Indication1 semaphore successfully closed.");
   } else {
	  DMISA_TRACE_LOG(LEVEL1,"CloseHandle error on queue mutex semaphore.");
   }

   if (rcsem = CloseHandle( IndSemHandle )) { /* Assignment intended; can be 0, 6invhandle, 301sembusy */
      DMISA_TRACE_LOG(LEVEL5," Indication1 semaphore successfully closed.");
   } else {	     
      DMISA_TRACE_LOG(LEVEL1,"CloseHandle error on indication1 semaphore.");
   }

   if (rcsem = CloseHandle( Ind3SemHandle )) { /* Assignment intended; can be 0, 6invhandle, 301sembusy */
      DMISA_TRACE_LOG(LEVEL5," Indication2 semaphore successfully closed.");
   } else {
      DMISA_TRACE_LOG(LEVEL1,"CloseHandle error on Indication2 semaphore.");
   }

   if ((rcsem = CloseHandle( DmiAccessMutex )) ) { /* Assignment intended */
      DMISA_TRACE_LOG(LEVEL1,"DmiAccess semaphore sucessfully closed.");
   } else {
      DMISA_TRACE_LOG(LEVEL1,"CloseHandle error on DmiAccess semaphore.");
   }

#endif

}


#ifdef DMISA_TRACE
/**********************************************************************/
/* Function:  doSemTraceCleanup()                                     */
/*                                                                    */
/* Clean up at end of program                                         */
/*                                                                    */
/* Reason codes:  none                                                */
/**********************************************************************/
void doSemTraceCleanup(void)
{                                 /* added entire function 950322.gwl */
#ifdef OS2
// $MED AIX?
   // Post & Close Event semaphores
   DosPostEventSem( hevWriteStart );
   DosCloseEventSem( hevWriteStart );

   // Release & Close Mutex semaphores
   DosReleaseMutexSem( hmtxLogbuf );
   DosCloseMutexSem( hmtxLogbuf );
   DosReleaseMutexSem( hmtxWritebuf );
   DosCloseMutexSem( hmtxWritebuf );
#elif defined WIN32
   CloseHandle( hevWriteStart );
   CloseHandle( hmtxLogbuf );
   CloseHandle( hmtxWritebuf );
#endif

}
#endif


/**********************************************************************/
/* Function:  xlateType()                                             */
/*                                                                    */
/*             Translate TYPE syntax:   From DMI to DPI               */
/*                                      From DPI to DMI               */
/*  The length parameter is set to 0 if a (non-date) string,          */
/*  otherwise, to the length in bytes of the attribute.               */
/*  It is not, however, set under error conditions.                   */
/*                                                                    */
/* Return codes:                                                      */
/*     XLATE_TYPE_noError            - Translation of type successful */
/*     XLATE_TYPE_noError_opaqueType - DPI opaque data type received  */
/*     XLATE_TYPE_unexpectedType     - type cannot be translated      */
/*     XLATE_TYPE_mifUnknownType     - MIF unknown data type received,*/
/*                                     should raise DPI error         */
/*     XLATE_TYPE_otherError                                          */
/* Question: Does high-order bit in DPI type need to be masked off?   */
/**********************************************************************/
int xlateType(ULONG *outtype, ULONG intype, int todpiordmi, ULONG *length)
{

   if ( todpiordmi == TODMI ) {
      switch ( intype ) {
      case  SNMP_TYPE_Integer32:
         *outtype = MIF_INTEGER;
         break;
      case  SNMP_TYPE_OCTET_STRING:
         *outtype = MIF_OCTETSTRING;
         break;
      case  SNMP_TYPE_Counter32:
         *outtype = MIF_COUNTER;
         break;
      case  SNMP_TYPE_Gauge32:
         *outtype = MIF_GAUGE;
         break;
      case  SNMP_TYPE_DisplayString:
         *outtype = MIF_DISPLAYSTRING;
         break;
      case  SNMP_TYPE_Counter64:
         *outtype = MIF_COUNTER64;
         break;
      case  SNMP_TYPE_Opaque:
         return XLATE_TYPE_noError_opaqueType;
      /* break;  // Note that this line of code is commented out to clean up compilation*/
      default:
         return XLATE_TYPE_unexpectedType;
      /* break;  // Note that this line of code is commented out to clean up compilation*/
      }
   } else if ( todpiordmi == TODPI ) {
       switch ( intype )  {
      case  MIF_INTEGER:
         *outtype = SNMP_TYPE_Integer32;
         *length = sizeof(int);
         break;
      case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
         *outtype = SNMP_TYPE_OCTET_STRING;  /* integer64 looks like OCTETSTRING to SNMP*/
#else
         *outtype = SNMP_TYPE_Counter64;  /* integer64 looks like counter64 to SNMP*/
#endif
         *length = sizeof(snmp_dpi_u64);
         break;
      case  MIF_OCTETSTRING:
         *outtype = SNMP_TYPE_OCTET_STRING;
         *length = 0UL;
         break;
      case  MIF_COUNTER:
#ifdef COUNTER64_IS_COUNTER
      case MIF_COUNTER64:
#endif
         *outtype = SNMP_TYPE_Counter32;
         *length = sizeof(ULONG);
         break;
#ifndef COUNTER64_IS_COUNTER
      case MIF_COUNTER64:
         *outtype = SNMP_TYPE_Counter64;
         *length = sizeof(snmp_dpi_u64);
         break;
#endif
      case  MIF_GAUGE:
         *outtype = SNMP_TYPE_Gauge32;
         *length = sizeof(ULONG);
         break;
      case  MIF_DATE:
         *outtype = SNMP_TYPE_DisplayString;
         *length = sizeof(DmiTimestamp_t) ;
         break;
      case  MIF_DISPLAYSTRING:
         *outtype = SNMP_TYPE_DisplayString;
         *length = 0UL;
         break;
      case MIF_UNKNOWN_DATA_TYPE:
         DMISA_TRACE_LOG(LEVEL1,"MIF_UNKNOWN_DATA_TYPE received from Service Layer.");
         return XLATE_TYPE_mifUnknownType;
      /* break;  // Note that this line of code is commented out      */
      default:
         return XLATE_TYPE_unexpectedType;
      /* break;  // Note that this line of code is commented out      */
      }
   } else {
      return XLATE_TYPE_otherError;
   }
   return XLATE_TYPE_noError;
}

/**********************************************************************/
/* Function:  dmiLength()                                             */
/*                                                                    */
/*    Determine length of DMI attribute, non-zero only the type is    */
/*    a valid, known type of fixed length.                            */
/*                                                                    */
/* Input:   DMI attribute type                                        */
/* Output:  Attribute length or 0                                     */
/**********************************************************************/
ULONG dmiLength(int intype)
{
   switch ( intype )  {  /* Warning: There are no break; statements following case statements*/
      case  MIF_INTEGER:
         return sizeof(LONG);
      /* break;  // Note that this line of code is commented out      */
      case  MIF_INTEGER64:
         return sizeof(snmp_dpi_u64);
      /* break;  // Note that this line of code is commented out      */
      case  MIF_OCTETSTRING:
         return 0UL;
      /* break;  // Note that this line of code is commented out      */
      case  MIF_COUNTER:
         return sizeof(ULONG);
      /* break;  // Note that this line of code is commented out      */
      case  MIF_COUNTER64:
         return sizeof(snmp_dpi_u64);
      /* break;  // Note that this line of code is commented out      */
      case  MIF_GAUGE:
      /* break;  // Note that this line of code is commented out      */
         return sizeof(ULONG);
      case MIF_DATE:
         return sizeof(DmiTimestamp_t);
      /* break;  // Note that this line of code is commented out      */
      case MIF_DISPLAYSTRING:
         return 0UL;
      /* break;  // Note that this line of code is commented out      */
      case MIF_UNKNOWN_DATA_TYPE:
         return 0UL;
      /* break;  // Note that this line of code is commented out      */
      default:
         return 0UL;
      /* break;  // Note that this line of code is commented out      */
   }
}

/**********************************************************************/
/* pitoa: Translate unsigned long integer to ascii with a             */
/*        decimal representation.                                     */
/*                                                                    */
/* Input:  ULONG int                                                  */
/*         Ptr. to character string, at least 11 characters long      */
/*                                                                    */
/* Output: No return variable.  String-terminated ASCII representation*/
/*         of the integer is placed in the string pointed to by s.    */
/**********************************************************************/
void pitoa(ULONG n, char s[])
{
int  i = 10, j = 0;

   s[i] = '\0';               /* null terminate the string            */
   do {
      s[--i] = n % 10 + '0';  /* get next digit                       */
   } while ((n /= 10) > 0);   /* are there more digits?                */

   while (i > 0 && i <= 10) {
      s[j++] = s[i++];
   }
}

/**********************************************************************/
/* Function:  prepareForDmiInvoke()                                   */
/*                                                                    */
/*     Reset a semaphore before issueing a DMI request                */
/*                                                                    */
/* Reason codes: PREAPRE_FOR_DMI_INVOKE_noError                       */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed                */
/**********************************************************************/
int prepareForDmiInvoke(void)
{
   APIRET rc;

#ifdef DMISA_TRACE
#ifdef OS2
   /* Query event semaphore                                           */
   rc = DosQueryEventSem(CnfSemHandle, &ulPostCt );
   DMISA_TRACE_LOG2(LEVEL5," Semaphore handle = %u.  Post count before semaphore reset = %u.",CnfSemHandle,ulPostCt);
#endif
#endif

   /* Reset event semaphore                                           */
#ifdef OS2
   rc = DosResetEventSem(CnfSemHandle, &ulPostCt );  /* ulPostCt: number of posts since last reset*/
   /* I don't *think* anything needs to be done here for AIX... $MED */
   if (!(rc == 0 ||         /* && Dmi_Registered_Flag ||              */
         rc == 300 && !Dmi_Registered_Flag) ) {  /* rc = 0, 6invhandle, 300alreadyreset*/
      DMISA_TRACE_LOG2(LEVEL1,"DosResetEventSem error: return code = %u, count = %u.", rc, ulPostCt );
      return PREPARE_FOR_DMI_INVOKE_resetSemFailed;
   }
   DMISA_TRACE_LOG1(LEVEL5," DosResetEventSem successful.  rc=%u.",rc);
#elif defined WIN32
   rc = ResetEvent(queueMutex );
   DMISA_TRACE_LOG1(LEVEL5," ResetEvent queueMutex.  rc=%u.",rc);

#else
/* $MED i don't think there's any need (or concept) to unlock cnfsem or qmutex here */
   mutex_lock(&queueMutex); /* $MED prevent premature adding to queue */
   /* could this cause deadlock $MED */
#endif

   return PREPARE_FOR_DMI_INVOKE_noError;
}
/*********************************************************************/
/* Function to await Service Layer response to the issued DMI MI command */
/*                                                                   */
/* Return codes:  AWAIT_DMI_RESPONSE_noError                         */
/*                AWAIT_DMI_RESPONSE_timeoutSem (DMI broke)          */
/*                AWAIT_DMI_RESPONSE_interruptSem                    */
/*                AWAIT_DMI_RESPONSE_postSemError                    */
/*                AWAIT_DMI_RESPONSE_otherSemError                   */
/*                AWAIT_DMI_RESPONSE_readQueueFailed                 */
/*                AWAIT_DMI_RESPONSE_otherQueueError                 */
/*********************************************************************/
int awaitDmiResponse(QueueEntryStr **ppqueueentry)
{
ULONG   datalength;   /* Length of element being added */
APIRET rcque,rcsem;

   /* Wait for semaphore                                              */
   DMISA_TRACE_LOG(LEVEL5," Waiting on semaphore.");
   ulDmiTimeout = DMI_REQUEST_TIMEOUT;  /* Could add some smart code to vary this.*/
#ifdef WIN32

   (DWORD) rcsem = WaitForSingleObject(queueMutex, ulDmiTimeout);
   if ((rcsem != WAIT_FAILED) && (rcsem != WAIT_TIMEOUT)) {
     rcsem = 0;
     #ifdef DMISA_TRACE
               DMISA_TRACE_LOG(LEVEL5," WaitForSingleObject <queueMutex> ended successfully.");
     #endif
   }
#elif defined OS2
   rcsem = DosWaitEventSem(CnfSemHandle, ulDmiTimeout );
#else
   do { /* wait for something to be put on the queue */
      rcsem = cond_wait(&queueCond, &queueMutex); /* no timeout code yet $MED */
   } while (pQueueHead == NULL);
#endif


#ifdef WIN32
   if (rcsem == 0) { /* rcsem = 0, 6invhndl, 8outofmemory, 95interrupt, 640timeout*/
     EnterCriticalSection(&CriticalSection);
     *ppqueueentry = pQueueHead;
     pQueueHead    = pQueueHead->pNext;
     LeaveCriticalSection(&CriticalSection);
     rcsem = SetEvent(queueMutex);
   } else if ( rcsem == WAIT_TIMEOUT ) {
      DMISA_TRACE_LOG(LEVEL1,"DosWaitEventSem call timed out.");

      /* Set event semaphore :-) Free for All -Own Private Idaho :-)   */
      rcsem = SetEvent(queueMutex);
      DMISA_TRACE_LOG1(LEVEL5," SetEvent queueMutex.  rc=%u.",rcsem);

      /* no handling for timeout yet in AIX, not sure what to do $MED */
      if ( rcsem == 0 ) {
         DMISA_TRACE_LOG(LEVEL1,"SetEvent error. <awaitDmiResponse>" );
         return AWAIT_DMI_RESPONSE_postSemError;
      }
      DMISA_TRACE_LOG(LEVEL5,"  SetEvent successful. <awaitDmiResponse>" );
      return AWAIT_DMI_RESPONSE_timeoutSem;
   } else {
      DMISA_TRACE_LOG1(LEVEL1,"error: return code = %u.", rcsem);
      return AWAIT_DMI_RESPONSE_otherSemError;
   }


#elif defined OS2
   if (rcsem == 0) { /* rcsem = 0, 6invhndl, 8outofmemory, 95interrupt, 640timeout*/
#ifdef DMISA_TRACE
      DMISA_TRACE_LOG(LEVEL5," DosWaitEventSem ended successfully.");
      rcsem = DosQueryEventSem(CnfSemHandle, &ulPostCt );
      DMISA_TRACE_LOG2(LEVEL5," Semaphore handle = %u, Post count after waiting = %u.",CnfSemHandle,ulPostCt);
#endif
      /* Read from the queue                                          */
      QRequest.pid = OwningPID;      /* Set request data block to indicate queue owner*/
      ElementCode = 0;               /* Indicate that the read should start at front of queue*/
      NoWait = 0;                    /* Indicate read should wait if queue currently empty*/
      rcque = DosReadQueue( QueueHandle,
                            &QRequest,
                            &datalength,
                            (void **)ppqueueentry,
                            ElementCode,
                            NoWait,
                            &ElemPriority,
                            0 );
      if (rcque != 0) {  /* rcque = 0, 87invparm, 330procnotowned, 333elementnotexist,*/
                             /*             337invhandle, 342empty, 433invalidwait*/
         DMISA_TRACE_LOG1(LEVEL1,"DosRead Queue error: return code = %u.", rcque );
         return AWAIT_DMI_RESPONSE_readQueueFailed;
      } else {
         DMISA_TRACE_LOG1(LEVEL5," DosReadQueue successful, length = %u.", datalength );
      }
      if (datalength != sizeof(QueueEntryStr)) {  /* Error: queue entry is of unexpected length*/
         (*ppqueueentry)->pReqBuf = NULL;
         DMISA_TRACE_LOG1(LEVEL1,"Queue length error, length = %u.",datalength);
         return AWAIT_DMI_RESPONSE_otherQueueError;
      }
   } else if ( rcsem == ERROR_TIMEOUT ) {
      DMISA_TRACE_LOG(LEVEL1,"DosWaitEventSem call timed out.");

      /* Post event semaphore                                         */
      rcsem = DosPostEventSem(CnfSemHandle);
      /* no handling for timeout yet in AIX, not sure what to do $MED */
      if ( rcsem != 0 ) {  /* rcsem = 0, 6invhandle, 298toomanyposts, 299alreadyposted*/
         DMISA_TRACE_LOG1(LEVEL1,"DosPostEventSem error: return code = %u", rcsem );
         return AWAIT_DMI_RESPONSE_postSemError;
      }
      DMISA_TRACE_LOG(LEVEL5,"  DosPostEventSem successful." );
      return AWAIT_DMI_RESPONSE_timeoutSem;
   } else {
      DMISA_TRACE_LOG1(LEVEL1,"DosWaitEventSem error: return code = %u.", rcsem);
      return AWAIT_DMI_RESPONSE_otherSemError;
   }
#else
   mutex_unlock(&queueMutex);
   /*OS_EnterCritSec(); */
   *ppqueueentry = pQueueHead;
   pQueueHead    = pQueueHead->pNext;
   /*OS_ExitCritSec();*/
#endif

   return AWAIT_DMI_RESPONSE_noError;
}

/*******************************************************************************/
void swap64(char *InValue,char *OutValue)
{
   short i;
#if defined(OS2) || defined(WIN32)
   for (i=0; i<sizeof(snmp_dpi_u64); ++i) {
      *(OutValue + i) = *(InValue + sizeof(snmp_dpi_u64) - 1 - i);
   }
#else
   typedef struct {unsigned short word[4];} PR_Int64_t;
   PR_Int64_t *InWords, *OutWords;

   InWords = (PR_Int64_t *)InValue;
   OutWords = (PR_Int64_t *)OutValue;
   for (i = 0;i < 4;i++ ) {
      OutWords->word[i] = InWords->word[i];
   }
#endif
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiListComponentsOUT structure                               ******/
void free_listcompout(DmiListComponentsOUT *listcompout) {
 

DmiComponentInfo_t *compinfo=NULL;
int  count;

      if (!listcompout)
           return;
      if (listcompout->reply) {
      compinfo = listcompout->reply->list.list_val;
      for(count= 1; count<=listcompout->reply->list.list_len;count++) {
      if ((compinfo) && (compinfo->name) && 
         (compinfo->name->body.body_val)) {
           free(compinfo->name->body.body_val);
           free(compinfo->name);
      }
      if ((compinfo) && (compinfo->pragma) &&
        (compinfo->pragma->body.body_val)) {
           free(compinfo->pragma->body.body_val);
           free(compinfo->pragma);
      }
      if ((compinfo) && (compinfo->description) && 
         (compinfo->description->body.body_val)) {
           free(compinfo->description->body.body_val);
           free(compinfo->description);
      }
      compinfo++;
      }
      if (listcompout->reply->list.list_val) 
           free(listcompout->reply->list.list_val);
      free(listcompout->reply);
      }
      free(listcompout);
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiListGroupsOUT structure                               ******/
void free_listgroupout(DmiListGroupsOUT *listgroupout) {
 
DmiGroupInfo_t *groupptr=NULL;
int  count;

    if (!listgroupout)
        return; 
    if (listgroupout->reply) {
    groupptr = listgroupout->reply->list.list_val; 
    for(count=1; count<= listgroupout->reply->list.list_len; count++) {
      if ((groupptr) && (groupptr->name) && 
         (groupptr->name->body.body_val)) {
           free(groupptr->name->body.body_val);
           free(groupptr->name);
      }
      if ((groupptr) && (groupptr->pragma) && 
         (groupptr->pragma->body.body_val)) {
           free(groupptr->pragma->body.body_val);
           free(groupptr->pragma);
      }
      if ((groupptr) && (groupptr->className) && 
         (groupptr->className->body.body_val)) {
           free(groupptr->className->body.body_val);
           free(groupptr->className);
      }
      if ((groupptr) && (groupptr->description) && 
         (groupptr->description->body.body_val)) {
          free(groupptr->description->body.body_val);
          free(groupptr->description);
      }
      if ((groupptr) && (groupptr->keyList) &&
        (groupptr->keyList->list.list_val)) {
         free(groupptr->keyList->list.list_val);
         free(groupptr->keyList);
      }
      groupptr++;
    }  
      if (listgroupout->reply->list.list_val)
           free(listgroupout->reply->list.list_val);
      free(listgroupout->reply);
      }
      free(listgroupout);
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiListAttributesOUT structure                               ******/
void free_listattrout(DmiListAttributesOUT *listattrout) {

DmiAttributeInfo_t *attribptr=NULL;
DmiEnumInfo_t      *enumptr=NULL;
int     count,loop;

    if (!listattrout)
        return;
    if (listattrout->reply) {
    attribptr = listattrout->reply->list.list_val;
    for(count=1; count<=listattrout->reply->list.list_len;count++) {
      if ((attribptr) && (attribptr->name) &&
         (attribptr->name->body.body_val)) {
           free(attribptr->name->body.body_val);
           free(attribptr->name);
      }
      if ((attribptr) && (attribptr->pragma) &&
         (attribptr->pragma->body.body_val)) {
           free(attribptr->pragma->body.body_val);
           free(attribptr->pragma);
      }
      if ((attribptr) && (attribptr->description) &&
         (attribptr->description->body.body_val)) {
          free(attribptr->description->body.body_val);
          free(attribptr->description);
      }
      if (attribptr->enumList) {
      enumptr = attribptr->enumList->list.list_val;
      for(loop =1 ;loop<=attribptr->enumList->list.list_len;loop++) {
        if ((enumptr) && (enumptr->name) &&
          (enumptr->name->body.body_val)) {
           free(enumptr->name->body.body_val);
           free(enumptr->name);
        }
        enumptr++;
      } 
      if (attribptr->enumList->list.list_val)
         free(attribptr->enumList->list.list_val);
      free(attribptr->enumList);
      }
      attribptr++;
    }
      if (listattrout->reply->list.list_val)
           free(listattrout->reply->list.list_val);
      free(listattrout->reply);
    }
    free(listattrout);
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiAttributeValues_t structure                               ******/
void free_attrvalues(DmiAttributeValues_t *keylist) {

DmiAttributeData_t *attribptr=NULL;
int     count;

    if (keylist) {
    attribptr = keylist->list.list_val;
    for(count=1; count<=keylist->list.list_len;count++) {
           switch(attribptr->data.type) {
           case MIF_COUNTER:
                          break;
           case MIF_COUNTER64:
                          break;
           case MIF_GAUGE: 
                          break;
           case MIF_INTEGER:
                          break;
           case MIF_INTEGER64:
                          break;
           case MIF_OCTETSTRING: 
            if ((attribptr) &&
             (attribptr->data.DmiDataUnion_u.octetstring)) {
              if(attribptr->data.DmiDataUnion_u.octetstring->body.body_len)
               free(attribptr->data.DmiDataUnion_u.octetstring->body.body_val);
               free(attribptr->data.DmiDataUnion_u.octetstring);
            }
                          break;
           case MIF_DISPLAYSTRING: 
            if ((attribptr) &&
                (attribptr->data.DmiDataUnion_u.str)) { 
             if (attribptr->data.DmiDataUnion_u.str->body.body_len)
               free(attribptr->data.DmiDataUnion_u.str->body.body_val);
               free(attribptr->data.DmiDataUnion_u.str);
            }
                          break;
           case MIF_DATE: 
            if ((attribptr) && (attribptr->data.DmiDataUnion_u.date))
                 free(attribptr->data.DmiDataUnion_u.date);
                 break;
           }
      attribptr++;
    }
      if (keylist->list.list_val)
           free(keylist->list.list_val);
      free(keylist);
    }
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiGetAttributeOUT   structure                               ******/
void free_getattrout(DmiGetAttributeOUT *getattrout) {

DmiDataUnion_t *dataptr=NULL;
int     count;

    if (!getattrout)
         return;
    dataptr = getattrout->value;
    if (dataptr) {
           switch(dataptr->type) {
           case MIF_COUNTER:
                          break;
           case MIF_COUNTER64:
                          break;
           case MIF_GAUGE:
                          break;
           case MIF_INTEGER:
                          break;
           case MIF_INTEGER64:
                          break;
           case MIF_OCTETSTRING:
            if ((dataptr) && 
                (dataptr->DmiDataUnion_u.octetstring) &&
                (dataptr->DmiDataUnion_u.octetstring->body.body_val)) {
              if(dataptr->DmiDataUnion_u.octetstring->body.body_len)
               free(dataptr->DmiDataUnion_u.octetstring->body.body_val);
               free(dataptr->DmiDataUnion_u.octetstring);
            }
                          break;
           case MIF_DISPLAYSTRING:
            if ((dataptr) &&
               (dataptr->DmiDataUnion_u.str) &&
              (dataptr->DmiDataUnion_u.str->body.body_val)) {
              if (dataptr->DmiDataUnion_u.str->body.body_len)
               free(dataptr->DmiDataUnion_u.str->body.body_val);
               free(dataptr->DmiDataUnion_u.str);
            }
                          break;
           case MIF_DATE:
            if ((dataptr) && (dataptr->DmiDataUnion_u.date))
                 free(dataptr->DmiDataUnion_u.date);
                          break;
           }
           free(dataptr);
      }
     free(getattrout);
}


/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the     **/
/*****  DmiGetMultipleOUT     structure                               ******/

void free_getmultout(DmiGetMultipleOUT *getmultout) {

int loop,count;
DmiMultiRowData_t  *multrowdata;
DmiRowData_t       *rowdata;

     
      if (!getmultout)
           return;
      multrowdata = getmultout->rowData;
      if (multrowdata) {
        rowdata = multrowdata->list.list_val;
      for(count=1; count<=multrowdata->list.list_len;count++) {
       if ((rowdata) && (rowdata->className) && 
          (rowdata->className->body.body_val)) {
           free(rowdata->className->body.body_val);
           free(rowdata->className);
       }
       free_attrvalues(rowdata->keyList);
       free_attrvalues(rowdata->values);
       rowdata++;
      }
      if (multrowdata->list.list_val)
         free(multrowdata->list.list_val);
      free(multrowdata);
      }
      free(getmultout);
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the     **/
/*****  DmiSetAttributeIN     structure                               ******/

void free_setAttributeIN(DmiSetAttributeIN *setattrin) {

DmiDataUnion_t *dataptr;

    if (!setattrin)
         return;
    if (setattrin->keyList)
         free_attrvalues(setattrin->keyList);
    dataptr = setattrin->value;  
    if (dataptr) {
           switch(dataptr->type) {
           case MIF_COUNTER:
                          break;
           case MIF_COUNTER64:
                          break;
           case MIF_GAUGE:
                          break;
           case MIF_INTEGER:
                          break;
           case MIF_INTEGER64:
                          break;
           case MIF_OCTETSTRING:
            if ((dataptr->DmiDataUnion_u.octetstring) &&
              (dataptr->DmiDataUnion_u.octetstring->body.body_val)) {
             if(dataptr->DmiDataUnion_u.octetstring->body.body_len)
               free(dataptr->DmiDataUnion_u.octetstring->body.body_val);
               free(dataptr->DmiDataUnion_u.octetstring);
            }
                          break;
           case MIF_DISPLAYSTRING:
            if ((dataptr->DmiDataUnion_u.str) &&
              (dataptr->DmiDataUnion_u.str->body.body_val)) {
             if (dataptr->DmiDataUnion_u.str->body.body_len)
               free(dataptr->DmiDataUnion_u.str->body.body_val);
               free(dataptr->DmiDataUnion_u.str);
            }
                          break;
           case MIF_DATE:
            if (dataptr->DmiDataUnion_u.date)
                 free(dataptr->DmiDataUnion_u.date);
                          break;
           }
           free(dataptr);
      }
      free(setattrin);
}

/***************************************************************************/
/*Converts from DMI_GroupKeyData_t struct to DmiAttributeValues_t struct */
int  groupkeydata_to_keylist(DMI_GroupKeyData_t *grpkeydata,
                             DmiAttributeValues_t *keylist, int keycount) {

  DmiAttributeData_t    *data, *tmpattrdata;   
  DMI_GroupKeyData_t    *tmpkeydata=grpkeydata;
  int                   i;


  data = (DmiAttributeData_t *)calloc(keycount, sizeof(DmiAttributeData_t));
  if (!data) {
     free(keylist);
     return 0;   /*out of memory error , which the calling func. will return */
  }

  tmpattrdata = data;
  
  keylist->list.list_len = keycount;
  keylist->list.list_val = data;

  for(i = 0; i < keycount; i++,tmpattrdata++, tmpkeydata++) {

    tmpattrdata->id = tmpkeydata->iAttributeId;
    tmpattrdata->data.type = tmpkeydata->iAttributeType;

    switch(tmpkeydata->iAttributeType) {
        case MIF_INTEGER:
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.integer),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(int)); 
              break;
        case MIF_GAUGE:
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.gauge),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(int)); 
             break;
        case MIF_COUNTER:
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.counter),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(int)); 
             break;
        case MIF_INTEGER64:
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.integer64),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(DmiInteger64_t)); 
             break;
        case MIF_COUNTER64:
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.counter64),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(DmiCounter64_t)); 
             break;
        case MIF_DISPLAYSTRING:
             tmpattrdata->data.DmiDataUnion_u.str = (DmiString_t *)calloc(1,
                                                   sizeof(DmiString_t));
               if (!tmpattrdata->data.DmiDataUnion_u.str)
                         return 0;
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.str->body.body_len), 
             (char *) &((DMI_STRING*)((char *)grpkeydata+(tmpkeydata->oKeyValue)))->length, sizeof(ULONG));
             if (tmpattrdata->data.DmiDataUnion_u.str->body.body_len) {
             tmpattrdata->data.DmiDataUnion_u.str->body.body_val = 
              (char*)calloc(1,tmpattrdata->data.DmiDataUnion_u.str->body.body_len+1);

             memcpy(tmpattrdata->data.DmiDataUnion_u.str->body.body_val,
                    ((DMI_STRING*)((char *)grpkeydata+(tmpkeydata->oKeyValue)))->body,
                    tmpattrdata->data.DmiDataUnion_u.str->body.body_len);
             }
             break; 
        case MIF_OCTETSTRING:
             tmpattrdata->data.DmiDataUnion_u.octetstring = 
                  (DmiOctetString_t *)calloc(1, sizeof(DmiOctetString_t));
               if (!tmpattrdata->data.DmiDataUnion_u.octetstring)
                         return 0;
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_len), 
             (char *) &((DMI_STRING*)((char *)grpkeydata+(tmpkeydata->oKeyValue)))->length, sizeof(ULONG));
             if (tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_len) {
             tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_val = 
              (char*)calloc(1,tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_len+1);

             memcpy(tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_val,
                    ((DMI_STRING*)((char *)grpkeydata+(tmpkeydata->oKeyValue)))->body,
                    tmpattrdata->data.DmiDataUnion_u.octetstring->body.body_len);
             }
             break;
        case MIF_DATE:
             tmpattrdata->data.DmiDataUnion_u.date = (DmiTimestamp_t *)
                                          calloc(1, sizeof(DmiTimestamp_t)); 
              if (!tmpattrdata->data.DmiDataUnion_u.date)
                        return 0;
             memcpy((char *)&(tmpattrdata->data.DmiDataUnion_u.date),
                    (char *)((char *)grpkeydata+(tmpkeydata->oKeyValue)),
                            sizeof(DmiTimestamp_t)); 
             break;
        default:
             return 0;
    }

  } 
   
return 1; /*success */
}

int  Ind_SubscribeWithSP() {

DmiListClassNamesIN    listclassnamesin;
DmiListClassNamesOUT   listclassnamesout;
DmiRowData_t           rowdata;
DmiClassNameInfo_t     *classnameinfo;
DmiString_t            className, rpctype, transtype, subaddr;
char                   buffer[200];
char                   hostname[100];
DmiAttributeValues_t   keylist_attrval, table_attrval;
DmiAttributeData_t     keylist_attrdat[7];
DmiAddRowIN            addrowin;
DmiAddRowOUT           addrowout;
char                  *tmpptr;
int  count;

   memset(buffer, 0, sizeof(buffer));
/*Search for the Group Id of the Subscription Table */
   listclassnamesin.handle = DmiHandle;
   listclassnamesin.maxCount = 0;   /*For all the groups*/
   listclassnamesin.compId = 1;     /*The SP */
   DmiListClassNames(listclassnamesin, &listclassnamesout, &dmi_rpchandle);
   if (listclassnamesout.error_status != DMIERR_NO_ERROR) {
        /*dmi_error(listclassnamesout.error_status); */
        DMISA_TRACE_LOG(LEVEL1,"Error in Listing the Class Names of Compid = 1");
        return SUBSCRIBE_WITH_SP_Error; 
   }
   
   if (!listclassnamesout.reply) {
      DMISA_TRACE_LOG(LEVEL1,"Error: NULL DmiClassNameList_t* returned in DmiListClassNames");
      return SUBSCRIBE_WITH_SP_Error; 
   }
   
   classnameinfo = listclassnamesout.reply->list.list_val;
   if (!classnameinfo) {
      DMISA_TRACE_LOG(LEVEL1,"Error:NULL DmiClassNameInfo_t* in the DmiClassNameList struct");
      return SUBSCRIBE_WITH_SP_Error; 
   }

   for(count=0; count < listclassnamesout.reply->list.list_len; count++) {
      tmpptr = (char *)calloc(1, classnameinfo->className->body.body_len+1);
      if (!tmpptr) return SUBSCRIBE_WITH_SP_Error;
      memcpy(tmpptr, classnameinfo->className->body.body_val, 
                    classnameinfo->className->body.body_len);
        if (strstr(tmpptr, "SP Indication Subscription") &&
             (classnameinfo->id != 0)) {
           free(tmpptr);
           break;
        }
        classnameinfo++;
        free(tmpptr);
   }  

   if (count == listclassnamesout.reply->list.list_len) {
      DMISA_TRACE_LOG(LEVEL1,"Error:No class found for Indication Subscription");
      return SUBSCRIBE_WITH_SP_Error;
   }

/*Frame the Subscription Entry in the DmiRowData Structure */   

   keylist_attrval.list.list_len = 4;
   keylist_attrval.list.list_val = keylist_attrdat;

   table_attrval.list.list_len = 7;
   table_attrval.list.list_val = keylist_attrdat;

   keylist_attrdat[0].id = 1;
   keylist_attrdat[0].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[0].data.DmiDataUnion_u.str = &rpctype;
   rpctype.body.body_len = strlen("ONC RPC");
   rpctype.body.body_val = "ONC RPC";

   keylist_attrdat[1].id = 2;
   keylist_attrdat[1].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[1].data.DmiDataUnion_u.str = &transtype;
   transtype.body.body_len = strlen("ncacn_ip_tcp");
   transtype.body.body_val = "ncacn_ip_tcp";

   keylist_attrdat[2].id = 3;
   keylist_attrdat[2].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[2].data.DmiDataUnion_u.str = &subaddr;

   if (gethostname(hostname, sizeof(hostname))) {
      DMISA_TRACE_LOG1(LEVEL1,"gethostname function failed errno= %d",errno);
      return SUBSCRIBE_WITH_SP_Error;
   }
   
   sprintf(buffer, "%s:%ul:%ul",hostname, MAPPER_PROGNUM, MAPPER_VERSNUM);
   subaddr.body.body_len = strlen(buffer);
   subaddr.body.body_val = buffer;
 
   keylist_attrdat[3].id = 4;
   keylist_attrdat[3].data.type = MIF_INTEGER;
   keylist_attrdat[3].data.DmiDataUnion_u.integer = 1;

   keylist_attrdat[4].id = 5;
   keylist_attrdat[4].data.type = MIF_DATE;
   keylist_attrdat[4].data.DmiDataUnion_u.date=
         (DmiTimestamp_t *) sub_warntime; 
#if 0
         (DmiTimestamp_t *) "19960804120800.000000-420   ";
#endif

   keylist_attrdat[5].id = 6;
   keylist_attrdat[5].data.type = MIF_DATE;
   keylist_attrdat[5].data.DmiDataUnion_u.date=
         (DmiTimestamp_t *) sub_exptime; 
#if 0
         (DmiTimestamp_t *) "19961231120000.000000-420   ";
#endif

   keylist_attrdat[6].id = 7;
   keylist_attrdat[6].data.type = MIF_INTEGER;
   keylist_attrdat[6].data.DmiDataUnion_u.integer = failure_threshold;


   rowdata.compId = 1;
   rowdata.groupId = classnameinfo->id;
   
   className.body.body_len = classnameinfo->className->body.body_len;
   className.body.body_val = 
          (char *)calloc(1, classnameinfo->className->body.body_len+1);
   if (!className.body.body_val) return SUBSCRIBE_WITH_SP_Error; 
   memcpy(className.body.body_val, classnameinfo->className->body.body_val,
          classnameinfo->className->body.body_len);
   
   rowdata.className = &className;
   rowdata.keyList   = &keylist_attrval; 
   rowdata.values    = &table_attrval;

   addrowin.handle = DmiHandle;
   addrowin.rowData = &rowdata;

   DmiAddRow(addrowin, &addrowout, &dmi_rpchandle);
   if ((addrowout.error_status != DMIERR_NO_ERROR) &&
      (addrowout.error_status != DMIERR_ROW_EXIST)) { 
      DMISA_TRACE_LOG(LEVEL1,"Error in Adding Row for Subscription Table Entry");
      if (className.body.body_val) free(className.body.body_val);
      return SUBSCRIBE_WITH_SP_Error;
   }

   if (className.body.body_val) free(className.body.body_val);
   if (listclassnamesout.reply) 
           free_listclassnameout(listclassnamesout.reply);
   return SUBSCRIBE_WITH_SP_noError;  
}


int  Ind_AddFilterToSP() {

DmiListClassNamesIN    listclassnamesin;
DmiListClassNamesOUT   listclassnamesout;
DmiRowData_t           rowdata;
DmiClassNameInfo_t     *classnameinfo;
DmiString_t            className, rpctype, transtype, subaddr, classfilt;
char                   buffer[200];
char                   hostname[100];
DmiAttributeValues_t   keylist_attrval, table_attrval;
DmiAttributeData_t     keylist_attrdat[7];
DmiAddRowIN            addrowin;
DmiAddRowOUT           addrowout;
char     *tmpptr; 
int  count;

   memset(buffer, 0, sizeof(buffer));
/*Search for the Group Id of the Subscription Table */
   listclassnamesin.handle = DmiHandle;
   listclassnamesin.maxCount = 0;   /*For all the groups*/
   listclassnamesin.compId = 1;     /*The SP */
   DmiListClassNames(listclassnamesin, &listclassnamesout, &dmi_rpchandle);
   if (listclassnamesout.error_status != DMIERR_NO_ERROR) {
        /*dmi_error(listclassnamesout.error_status); */
     DMISA_TRACE_LOG(LEVEL1,"Error in Listing the Class Names of Compid=1");
     return ADD_INDFILTER_TO_SP_Error;
   }
  
   if (!listclassnamesout.reply) {
      DMISA_TRACE_LOG(LEVEL1, "Error: NULL DmiClassNameList_t* returned in DmiListClassNames");
      return ADD_INDFILTER_TO_SP_Error;
   }
  
   classnameinfo = listclassnamesout.reply->list.list_val;
   if (!classnameinfo) {
      DMISA_TRACE_LOG(LEVEL1,"Error:NULL DmiClassNameInfo_t* in the DmiClassNameList struct");
      return ADD_INDFILTER_TO_SP_Error;
   }

   for(count=0; count < listclassnamesout.reply->list.list_len; count++) {
      tmpptr = (char *)calloc(1, classnameinfo->className->body.body_len+1);
      if (!tmpptr) return ADD_INDFILTER_TO_SP_Error;
      memcpy(tmpptr, classnameinfo->className->body.body_val,
                     classnameinfo->className->body.body_len);
        if (strstr(tmpptr, "SPFilterInformation") && 
                   (classnameinfo->id != 0)) {
           free(tmpptr);
           break;
        }
        classnameinfo++;
        free(tmpptr);
   }

   if (count == listclassnamesout.reply->list.list_len) {
      DMISA_TRACE_LOG(LEVEL1,"Error:No class found for Filter Information");
      return ADD_INDFILTER_TO_SP_Error;
   }

/*Frame the Filter Entry in the DmiRowData Structure */

   keylist_attrval.list.list_len = 6;
   keylist_attrval.list.list_val = keylist_attrdat;

   table_attrval.list.list_len = 7;
   table_attrval.list.list_val = keylist_attrdat;

   keylist_attrdat[0].id = 1;
   keylist_attrdat[0].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[0].data.DmiDataUnion_u.str = &rpctype;
   rpctype.body.body_len = strlen("ONC RPC");
   rpctype.body.body_val = "ONC RPC";

   keylist_attrdat[1].id = 2;
   keylist_attrdat[1].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[1].data.DmiDataUnion_u.str = &transtype;
   transtype.body.body_len = strlen("ncacn_ip_tcp");
   transtype.body.body_val = "ncacn_ip_tcp";

   keylist_attrdat[2].id = 3;
   keylist_attrdat[2].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[2].data.DmiDataUnion_u.str = &subaddr;

   if (gethostname(hostname, sizeof(hostname))) {
      DMISA_TRACE_LOG1(LEVEL1,"gethostname function failed errno= %d\n",errno);
      return ADD_INDFILTER_TO_SP_Error;
   }

   sprintf(buffer, "%s:%ul:%ul",hostname, MAPPER_PROGNUM, MAPPER_VERSNUM);
   subaddr.body.body_len = strlen(buffer);
   subaddr.body.body_val = buffer;

   keylist_attrdat[3].id = 4;
   keylist_attrdat[3].data.type = MIF_INTEGER;
   keylist_attrdat[3].data.DmiDataUnion_u.integer = 1;

   keylist_attrdat[4].id = 5;
   keylist_attrdat[4].data.type = MIF_INTEGER;
   keylist_attrdat[4].data.DmiDataUnion_u.integer= 0xFFFFFFFF;

   keylist_attrdat[5].id = 6;
   keylist_attrdat[5].data.type = MIF_DISPLAYSTRING;
   keylist_attrdat[5].data.DmiDataUnion_u.str=&classfilt;
   classfilt.body.body_len = strlen("||");
   classfilt.body.body_val = "||";

   keylist_attrdat[6].id = 7;
   keylist_attrdat[6].data.type = MIF_INTEGER;
   keylist_attrdat[6].data.DmiDataUnion_u.integer = 0xFFFFFFFF;


   rowdata.compId = 1;
   rowdata.groupId = classnameinfo->id; 

   className.body.body_len = classnameinfo->className->body.body_len;
   className.body.body_val = 
             (char *)calloc(1,classnameinfo->className->body.body_len+1);
   if (!className.body.body_val) return ADD_INDFILTER_TO_SP_Error;
   memcpy(className.body.body_val, classnameinfo->className->body.body_val,
            classnameinfo->className->body.body_len);

   rowdata.className = &className;
   rowdata.keyList   = &keylist_attrval;
   rowdata.values    = &table_attrval;

   addrowin.handle = DmiHandle;
   addrowin.rowData = &rowdata;

   DmiAddRow(addrowin, &addrowout, &dmi_rpchandle);
   if ((addrowout.error_status != DMIERR_NO_ERROR) &&
      (addrowout.error_status != DMIERR_ROW_EXIST)) { 
      DMISA_TRACE_LOG(LEVEL1,"Error in Adding Row for Subscription Table Entry");
      if (className.body.body_val) free(className.body.body_val);
      return ADD_INDFILTER_TO_SP_Error;
   }

   if (className.body.body_val) free(className.body.body_val);
   if (listclassnamesout.reply) 
           free_listclassnameout(listclassnamesout.reply);
   return ADD_INDFILTER_TO_SP_noError;

}
/****************************************************************************/
void free_listclassnameout(DmiClassNameList_t *classnamelist) {
int i;
DmiClassNameInfo_t *classnameinfo;
   
    if (classnamelist) {
      classnameinfo = classnamelist->list.list_val;
      for(i=0; i< classnamelist->list.list_len; i++) {
         if (classnameinfo->className->body.body_len)  
              free(classnameinfo->className->body.body_val);
         if (classnameinfo->className)
               free(classnameinfo->className);
         classnameinfo++;
      }
      if (classnamelist->list.list_val)
          free(classnamelist->list.list_val);
      free(classnamelist);
    }
} 
