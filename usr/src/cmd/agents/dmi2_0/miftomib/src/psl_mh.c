/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_mh.c	1.2 96/09/24 Sun Microsystems"


/****************************************************************************************************************************************************
    Filename: psl_mh.c
    
    Copyright (c) International Business Machines, Corp. 1994 - 1996
    Copyright (c) Intel, Inc. 1992,1993

    Description: DMI Handler
                 The DMI Handler provides the main entry point for management applications.  This entry point, DMI_invoke(), parses the provided
                 DMI command structure.  If it is well-formed, the Task Manager is called and asked to add an entry to the task list.  If it is not,
                 the management application's call-back function is immediatly called with an error status condition.

    Author(s): Paul A. Ruocchio, Alvin I. Pivowar, Steve Hanrahan

    RCS Revision: $Header: n:/sl/rcs/psl_mh.c 1.72 1994/05/18 10:02:58 shanraha Exp $

    Revision History:

        Date     	Author Description
        -------  	---    -----------------------------------------------
        10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
        10/28/93 	sfh		Add call to MH_getTime in MH_handleEvent.  Remove DOS-specific code.
        11/11/93 	sfh		Remove include of types.h.
        11/16/93 	sfh		Modify handleEvent to be a Task and to call pResponseFunc when finished calling event handlers.
        12/07/93 	sfh		Revise so a mgmt app can register only for events.
                                Check to make sure that mgmt app making a normal Dmi request has a callback function.
        12/08/93 	sfh		Change unregister to account for lack of iMgmtHandle member.
        12/08/93 	sfh		Set pResponseFunc to NULL before calling mgmt apps in handleEvent.  This function is for the SL to call the indication
                                originator.
        12/09/93 	sfh		Unlock component when doing callback.
        12/14/93 	sfh		Fix highPerfLookup's boolean test.
        12/29/93 	sfh		MaContext -> maContext; Registry -> registry.
        01/13/94 	sfh		Change parameter to unregisterMgmtApp to DMI_MgmtCommand_t since there is no more unregister command block.
        02/25/94	sfh		Add parent/child model changes.
                                Remove SAD-specific code.
                                Revise unregister code to just do a task switch	to TM_CALLBACK.  The callback then does the unregister.  This makes the
                                handling of this command the same as all other commands.
                                Revise parent/child code.
        04/05/94	sfh		Implement DmiCiInstall command.
        04/08/94	sfh		Revise for new component path ids.
        04/11/94	sfh		Fix install to copy filename to malloced data area only if file type is file name, not file pointer.
        05/10/94	sfh		Change type	of event to DMI_FUNC2 in handleEvent.
                                Update to 1.0 spec.
                                Add uninstall capability.
        05/11/94	sfh		Fix install to not fall through to error condition after sending event.
                                Change parent to secondary.
        05/18/94	sfh		Revise MH_executeInstrumentation to build instrumentation vector as pCnfBuf + sizeof(GetAttributeCnf).
        05/20/94    par         Re-write to be os-neutral for PSL v2.x
        06/18/94    par         Changed name to conform to new naming rules
        07/23/94    par         Modified for new *.h files
        02/10/95    par         Removed the old Parent/Child code - NOT USED.
        01/24/96    par         Modified for support of DMI version 1.1

****************************************************************************************************************************************************/


/********************************************************************* INCLUDES *********************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */
#include "psl_main.h"
#include "psl_dh.h"
#include "psl_util.h"
#include "pr_main.h"

/****************************************************************************************************************************************************/


/********************************************************************* DATA ************************************************************************/

MH_Registry_t           *registry = NULL;
CI_Registry_t    *direct = NULL;

static  DMI_STRING *spec,*implementation;
extern  DMI_UNSIGNED MIF_lastComponentInstalled(void);

/****************************************************************************************************************************************************/


/************************************************************** FUNCTION PROTOTYPES *****************************************************************/


static void             MH_cancel(DMI_MgmtCommand_t _FAR *dmiCommand, boolean doCallback);
static void             MH_listCommand(DMI_MgmtCommand_t _FAR *dmiCommand);
static DMI_UNSIGNED     MH_registerDirect(DMI_RegisterCiInd_t _FAR *reg,void *OS_Context);
static void             MH_registerMgmtApp(DMI_RegisterMgmtReq_t _FAR *dmiCommand,void *OS_Context);
static MH_Registry_t    *MH_registryLookup(DMI_UNSIGNED maHandle);
static DMI_UNSIGNED     MH_unregisterDirect(DMI_UnregisterCiInd_t _FAR *dmiCommand);
static void             MH_unregisterMgmtApp(MH_Registry_t *Remove);
static DMI_ListComponentReq_t *BuildListComp(DMI_UNSIGNED iComponentId,DMI_UNSIGNED *InstallBufSize,DMI_ListComponentCnf_t **cnfBuf);

/****************************************************************************************************************************************************/

ULONG MH_init(void)  /* module initialization function                       */
{
    spec = implementation = (DMI_STRING *)NULL;    /* initialize these to NULL */
    if((spec = (DMI_STRING *)OS_Alloc(sizeof(DMI_STRING) + sizeof(SPEC_LEVEL) + 1) ) != (DMI_STRING *)NULL){
        sprintf(spec->body,"%s",SPEC_LEVEL);
        spec->length = strlen(spec->body);
        if((implementation = (DMI_STRING *)OS_Alloc(sizeof(DMI_STRING) + sizeof(OS_SL_VERSION) + 6)) != (DMI_STRING *)NULL){
            sprintf(implementation->body,"%s",OS_SL_VERSION);
            implementation->length = strlen(implementation->body);
            return SLERR_NO_ERROR;    /* all went well, and we are ready for action */
        }
        OS_Free(spec);
    }
    return SLERR_OUT_OF_MEMORY;        /* return the general error           */
}

ULONG MH_term(void)  /* module termination function                          */
{
    if(spec != (DMI_STRING *)NULL) OS_Free(spec);
    if(implementation != (DMI_STRING *)NULL) OS_Free(implementation);
    return 0;
}


SL_ErrorCode_t MH_main(DMI_MgmtCommand_t _FAR *dmiCommand,void *OS_Context)
{
DMI_UNSIGNED    command;
SL_ErrorCode_t  status;
MH_Registry_t   *reg;

    command = dmiCommand->iCommand;
    status = SLERR_NO_ERROR;
    if ((dmiCommand->iLevelCheck != DMI_LEVEL_CHECK) && (dmiCommand->iLevelCheck != DMI_LEVEL_CHECK_V1)) status = SLERR_ILLEGAL_DMI_LEVEL;
    else {
        switch(command){
            case DmiRegisterMgmtCmd:
            case DmiRegisterCiCmd:
            case DmiUnregisterCiCmd:
            case DmiCiInstallCmd:
            case DmiCiUninstallCmd:
                break;                  /* Not mgmt handle                   */
            default:
                reg = MH_registryLookup(dmiCommand -> iMgmtHandle);
                if (reg == NULL) status = SLERR_ILLEGAL_HANDLE;
/* Make sure "normal" DMI commands have confirm function and are not registered just for indications */
                else if (reg->completionCallback == NULL) status = SLERR_NULL_COMPLETION_FUNCTION;
                break;
        }
    }
    if (status != SLERR_NO_ERROR) goto check_out;
    switch (command){
        case DmiUnregisterMgmtCmd:
        case DmiCancelCmd:
        case DmiUnregisterCiCmd:
        case DmiCiInstallCmd:
        case DmiCiUninstallCmd:
        case DmiSetAttributeCmd:
        case DmiSetReserveAttributeCmd:
        case DmiSetReleaseAttributeCmd:
            break;  
        default:
            if (dmiCommand->pCnfBuf == NULL) status = SLERR_NULL_RESPONSE_BUFFER;
            if(dmiCommand->iCnfBufLen < 512UL) status = SLERR_ILL_FORMED_COMMAND;
            if (status != SLERR_NO_ERROR) goto check_out;
            break;
    }
    /*  Initialize dmiCommand  */
    dmiCommand->iStatus = SLERR_NO_ERROR;
    dmiCommand -> iCnfCount = 0;  /* Cnf count initialized to 0 by parent */
    /*  Execute the command  */
    switch (command){
        case DmiRegisterCiCmd:
            status = MH_registerDirect((DMI_RegisterCiInd_t _FAR *)dmiCommand,OS_Context);
            break;
        case DmiUnregisterCiCmd:
            status = MH_unregisterDirect((DMI_UnregisterCiInd_t _FAR *)dmiCommand);
            break;
        case DmiRegisterMgmtCmd:
            MH_registerMgmtApp((DMI_RegisterMgmtReq_t _FAR *) dmiCommand,OS_Context);
            break;
        case DmiCancelCmd:
            MH_cancel((DMI_MgmtCommand_t _FAR *) dmiCommand, TRUE);
            break;
        case DmiUnregisterMgmtCmd:
            MH_completionCallback((DMI_MgmtCommand_t _FAR *)dmiCommand,(void _FAR *)0);
            break;
        case DmiCiInstallCmd:
            if(!TM_taskAdd(TM_INSTALL, (DMI_MgmtCommand_t  _FAR *)dmiCommand,OS_Context)) status = SLERR_OUT_OF_MEMORY;
            break;
        case DmiCiUninstallCmd:
            if(!TM_taskAdd(TM_UNINSTALL, (DMI_MgmtCommand_t  _FAR *)dmiCommand,OS_Context)) status = SLERR_OUT_OF_MEMORY;
            break;
        default:
            switch (command & 0xff00){
                case DMI_LIST_COMMANDS:
                    MH_listCommand(dmiCommand);
                    if (! TM_taskAdd(TM_DB, dmiCommand, 0)) status = SLERR_OUT_OF_MEMORY;
                    break;
                case DMI_GET_SET_COMMANDS:
                    /* Need first word of resp buf 0 as flag for get group key building */
                    /* but only for non-p/c commands (p/c cmds set it themsevles) */
                    *(DMI_UNSIGNED _FAR *)dmiCommand->pCnfBuf = 0;
                    if (! TM_taskAdd(TM_DB, dmiCommand, 0)) status = SLERR_OUT_OF_MEMORY;
                    break;
                default:
                    status = SLERR_ILLEGAL_COMMAND;
                    break;
            }
    }
check_out:
    if (status != SLERR_NO_ERROR) SL_buildError(dmiCommand, status);
    return status;     /* return the status to the caller */
}



TM_TaskStatus_t MH_completionCallback(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter)
{
MH_Registry_t   *reg;
DMI_UNSIGNED Command;

    OS_EnterCritSec();
    Command = dmiCommand->iCommand;
    reg = MH_registryLookup(dmiCommand -> iMgmtHandle);
    if (reg == (MH_Registry_t *) 0) SL_buildError(dmiCommand, SLERR_ILLEGAL_HANDLE);
    else{
        OS_callback(dmiCommand,reg);     /* issue the callback to the application */
        if (Command == DmiUnregisterMgmtCmd) MH_unregisterMgmtApp(reg);
    }
    OS_ExitCritSec();
    return TM_DONE;
}


 
TM_TaskStatus_t MH_handleEvent(DMI_Indicate_t _FAR *event, void _FAR *osContext)
{
MH_Registry_t *Work;

    event->pResponseFunc = (DMI_FUNC3_OUT)NULL; /* set the response function ptr to NULL */
    if (event->DmiTimeStamp.sYear[0] == '\0') OS_getTime(&event->DmiTimeStamp);
    OS_EnterCritSec();
    Work = registry;     /* point to the base register entry                 */
    while(Work != (MH_Registry_t *)NULL){    /* walk thorugh the list        */
        if(Work->eventCallback != (DMI_FUNC3_OUT)NULL)
            OS_Indicate(event,Work);             /* call the application         */
        Work = Work->next;     /* point to the next application in the list  */
    }
    OS_ExitCritSec();
    return TM_DONE;
}


static void MH_listCommand(DMI_MgmtCommand_t _FAR *dmiCommand)
{
    switch(dmiCommand -> iCommand) {
        case DmiListFirstAttributeCmd:
            dmiCommand -> iCommand = DmiListNextAttributeCmd;
            ((DMI_ListAttributeReq_t _FAR *) dmiCommand) -> iAttributeId = 0;
            break;
        case DmiListFirstComponentCmd:
            dmiCommand -> iCommand = DmiListNextComponentCmd;
            ((DMI_ListComponentReq_t _FAR *) dmiCommand) -> iComponentId = 0;
            break;
        case DmiListFirstGroupCmd:
            dmiCommand -> iCommand = DmiListNextGroupCmd;
            ((DMI_ListGroupReq_t _FAR *) dmiCommand) -> iGroupId = 0;
            break;
    }
}


static void MH_registerMgmtApp(DMI_RegisterMgmtReq_t _FAR *dmiCommand,void *OS_Context)
{
MH_Registry_t           *new;
DMI_RegisterCnf_t _FAR  *r;

    /* Allow indications only                                                */
    if (dmiCommand->pConfirmFunc == NULL && dmiCommand->pIndicationFunc == NULL)
        SL_buildError(dmiCommand, SLERR_NULL_COMPLETION_FUNCTION);
    /*  Build a new registry entry with current context and addresses.  */
    new = (MH_Registry_t *) OS_Alloc(sizeof(MH_Registry_t));
    if (new == (MH_Registry_t *) 0) {
        SL_buildError(dmiCommand, SLERR_OUT_OF_MEMORY);
        return;
    }
    memset(new,0,sizeof(MH_Registry_t));
    new -> completionCallback = dmiCommand -> pConfirmFunc;
    new -> eventCallback = dmiCommand -> pIndicationFunc;
    new->environmentId = MIF_UNKNOWN_ENVIRONMENT;
    new->maContext = OS_Context;    /* save the OS sepcific infomration here */
    /*  Insert the registry entry in the linked list and assign a handle.  */

    OS_EnterCritSec();    /* protect ourselves while we chain it in to the registry list */
    new->next = registry;
    registry = new;
    OS_ExitCritSec();

    /*  Build the response buffer, and return.  */
    r = (DMI_RegisterCnf_t _FAR *) SL_buildResponse(dmiCommand, (DMI_UNSIGNED)stringLen(spec), (char _FAR *)spec,
        (DMI_UNSIGNED)stringLen(implementation), (char _FAR *)implementation);
    ((DMI_MgmtCommand_t _FAR *)dmiCommand)->iMgmtHandle = (DMI_UNSIGNED)new;   /* use the ptr as the handle - it's safer */
    if (r != NULL) r->iDmiHandle = (DMI_UNSIGNED)new;
    if (!TM_taskAdd(TM_CALLBACK, (DMI_MgmtCommand_t  _FAR *)dmiCommand, NULL)){
        SL_buildError(dmiCommand, SLERR_OUT_OF_MEMORY);
        MH_unregisterMgmtApp(new);   /* clean this out if we fail */
    }
}


static MH_Registry_t *MH_registryLookup(DMI_UNSIGNED maHandle)
{
MH_Registry_t *p;

    OS_EnterCritSec();   /* protect ourselves while we look through the list */
    p = registry;        /* grab a working pointer here                      */
    while(p != (MH_Registry_t *)NULL){   /* walk thorugh the list            */
        if((DMI_UNSIGNED)p == maHandle) break;
        p = p->next;    /* advance to the next element                       */
    }
    OS_ExitCritSec();
    return p;          /* this is either a valid, matching entry, or NULL    */
}


/* Note:  We know registry exists because we verify before calling           */

static void MH_unregisterMgmtApp(MH_Registry_t *Remove)
{
MH_Registry_t   *l,*Prev;

    OS_EnterCritSec();    /* again, protect ourselves while we play with the chains... */
    Prev = (MH_Registry_t *)NULL;    /* this will make it possible to remove the element correctly */
    l = registry;
    while(l != (MH_Registry_t *)NULL){    /* walk through the list           */
        if(l == Remove){   /* this is the one, remove it       */
            if(Prev == (MH_Registry_t *)NULL) registry = l->next; /* this is the front of the list */
            else Prev->next = l->next;    /* it is in the middle of the list somewhere */
            break;
        }
        Prev = l;      /* save this one as the previous ptr                  */
        l = l->next;   /* advance to the next element                        */
    }
    if(l != (MH_Registry_t *)NULL){   /* we did find the one we wanted to remove, so clean it up */
        OM_cancel(DmiCancelAllCmd,(DMI_UNSIGNED)l,0);   /* clean up the commands on the overlays too */
        TM_cancel(DmiCancelAllCmd,(DMI_UNSIGNED)l,0);   /* cancel the command */
        OS_unregisterApp(l);          /* let the OS stuff clean up any context it may have set */
        OS_Free(l);   /* return the registry block to the pool               */
    }
    OS_ExitCritSec();
    return;
}

static DMI_UNSIGNED MH_unregisterDirect(DMI_UnregisterCiInd_t _FAR *dmiCommand)
{
CI_Registry_t *l, *Prev;
DMI_UNSIGNED RC = SLERR_NO_ERROR;

    OS_EnterCritSec();    /* again, protect ourselves while we play with the chains... */
    Prev = (CI_Registry_t *)NULL;    /* this will make it possible to remove the element correctly */
    l = direct;
    while(l != (CI_Registry_t *)NULL){    /* walk through the list           */
        if((DMI_UNSIGNED)l == (DMI_UNSIGNED)(dmiCommand->iCiHandle)){   /* this is the one, remove it */
            if(Prev == (CI_Registry_t *)NULL) direct = l->next; /* this is the front of the list */
            else Prev->next = l->next;    /* it is in the middle of the list somewhere */
            break;
        }
        Prev = l;      /* save this one as the previous ptr                  */
        l = l->next;   /* advance to the next element                        */
    }
    OS_ExitCritSec();
    if(l != (CI_Registry_t *)NULL){     /* we found it, now clean it up      */
        OM_cancel(DmiKillOverlayCmd,(DMI_UNSIGNED)(l->maContext),0);   /* remove all of the elements from the overlay */
        OS_unregisterDirect(l);   /* give the OS stuff a chance to cleanup any context */
        OS_Free(l);   /* return the registry block to the pool               */
    }
    else RC = SLERR_UNKNOWN_CI_REGISTRY;
    return RC;
}


/***************************************************************************************************************************************************
    Function:   MH_registerDirect

    Parameters: dmiCommand      Pointer to DmiRegisterCiInd or DmiRegisterParentCiInd., and the OS_Context

    Description: This function adds a new  CiRegistry to the direct registry.  Note that the parent should call this function before adding to its
                 registry, as attribute validation is done here.

    Returns:    Nothing.

***************************************************************************************************************************************************/


static DMI_UNSIGNED MH_registerDirect(DMI_RegisterCiInd_t _FAR *dmiCommand,void *OS_Context)
{
DMI_AccessData_t _FAR   *t;
DB_Attribute_t          *(*attrFunc)(MIF_Id_t, MIF_Id_t, MIF_Id_t);
DMI_RegisterCnf_t _FAR  *resp;
CI_Registry_t           *new;
unsigned short                errorCode = SLERR_NO_ERROR;

    if (dmiCommand->pAccessFunc == NULL){
        errorCode = SLERR_NULL_ACCESS_FUNCTION;
        goto error;
    }
    if (dmiCommand->pCancelFunc == NULL){
        errorCode = SLERR_NULL_CANCEL_FUNCTION;
        goto error;
    }
    /* Check all dyads for validity                                          */
    if(dmiCommand->iAccessListCount == 0){  /* we need at least one here */
        errorCode = SLERR_ILL_FORMED_COMMAND;
        goto error;
    }
    for (t = dmiCommand->DmiAccessList; t < &dmiCommand->DmiAccessList[dmiCommand->iAccessListCount]; t++){
        if((unsigned char *)t > ((unsigned char *)dmiCommand + dmiCommand->DmiMgmtCommand.iCmdLen)){
            errorCode = SLERR_ILL_FORMED_COMMAND;
            goto error;
        }
        if (t->iGroupId == 0){
            if (MIF_groupFindNext(dmiCommand->iComponentId, 0) == NULL){
                errorCode = DBERR_GROUP_NOT_FOUND;
                goto error;
            }
        }
        else{
            attrFunc = MIF_attributeGet;
            if (t->iAttributeId == 0) attrFunc = MIF_attributeFindNext;
            if(attrFunc(dmiCommand->iComponentId, t->iGroupId, t->iAttributeId) == NULL){
                errorCode = DBERR_ATTRIBUTE_NOT_FOUND;
                goto error;
            }
        }
    }
    if ((new = (CI_Registry_t *)OS_Alloc(sizeof(CI_Registry_t))) == NULL){
        errorCode = SLERR_OUT_OF_MEMORY;
        goto error;
    }
    memset(new,0,sizeof(CI_Registry_t));
    new->ciRegistry = dmiCommand;   /* save the pointer to the CiRegisterInd structure */
    new->environmentId = SL_ENVIRONMENT_ID;   /* Must be registrating in our context */
    new->maContext = OS_Context;
    OS_EnterCritSec();     /* everything looks good, go ahead and chain it into the list */
    new->next = direct;
    direct = new;
    OS_ExitCritSec();
    resp = (DMI_RegisterCnf_t _FAR *)SL_buildResponse(dmiCommand, (DMI_UNSIGNED)stringLen(spec), (char _FAR *)spec,
        (DMI_UNSIGNED)stringLen(implementation), (char _FAR *)implementation);
    resp->iDmiHandle = (DMI_UNSIGNED)(void _FAR *)new;
    dmiCommand->DmiMgmtCommand.iCnfCount = 1;
error:
    return errorCode;
}



/***************************************************************************************************************************************************
    Function:   MH_directLookup

    Parameters: componentId, groupId, attribId      Triad describing attribute direct interface should handle.
                environmentId                       Id of the operating system environment interested in.

    Description:This functions searches the direct interface registry looking to match the triad and environment given with a registered interface.
                Registered interfaces with group ids of 0 handle the entire component; those with attribute id of 0 handle the entire group.
                If the environment id is 0, we will look for any environment.

    Returns:    The registration structure of the reguested or first registry or NULL if not found.

***************************************************************************************************************************************************/


DMI_RegisterCiInd_t _FAR *MH_directLookup(DMI_UNSIGNED componentID, DMI_UNSIGNED groupID, DMI_UNSIGNED attribID, DMI_UNSIGNED environmentId)
{
CI_Registry_t               *h;
DMI_RegisterCiInd_t _FAR    *r = (DMI_RegisterCiInd_t _FAR *)NULL;
DMI_AccessData_t _FAR       *t;
USHORT                      Flag = FALSE;

    OS_EnterCritSec();    /* turn off the other threads while we search for this thing */
    h = direct;           /* start at the front end of this thing            */
    while(h != (CI_Registry_t *)NULL){   /* walk through the list            */
        if (environmentId != 0 && h->environmentId != environmentId){
            h = h->next;      /* advance to the next element                     */
            continue;
        }
        r = h->ciRegistry;
        if (r->iComponentId == componentID){
            for (t = r->DmiAccessList; t < &r->DmiAccessList[r->iAccessListCount]; t++)
                if (t->iGroupId == 0 || (t->iGroupId == groupID && (t->iAttributeId == 0 || t->iAttributeId == attribID))){
                    Flag = TRUE;
                    break;
                }
        }
        if(Flag == TRUE) break;   /* bug out, we've found the sucker.        */
        h = h->next;      /* advance to the next element                     */
    }
    OS_ExitCritSec();
    if(Flag == TRUE) return r;     /* return the element that we've located  */
    return NULL;                   /* didn't find the thing, must not have it? */
}

static void MH_cancel(DMI_MgmtCommand_t _FAR *dmiCommand, boolean doCallback)
{
    OM_cancel(dmiCommand -> iCommand, dmiCommand -> iMgmtHandle, dmiCommand -> iCmdHandle);
    TM_cancel(dmiCommand -> iCommand, dmiCommand -> iMgmtHandle, dmiCommand -> iCmdHandle);
    if(doCallback)
        if(!TM_taskAdd(TM_CALLBACK, (DMI_MgmtCommand_t  _FAR *)dmiCommand, NULL)) 
            SL_buildError(dmiCommand, SLERR_OUT_OF_MEMORY);
}

static DMI_ListComponentReq_t *BuildListComp(DMI_UNSIGNED iComponentId,DMI_UNSIGNED *InstallBufSize,DMI_ListComponentCnf_t **cnfBuf)
{
DMI_ListComponentReq_t   *listComponent = NULL;

    /* Build a listComponentCnf block by creating a command block    */
    (*InstallBufSize) = sizeof(DMI_ListComponentReq_t);  /* start at least this big */
    do{  /* keep trying to build a confirm buffer, until we get a big enough block */
        (*InstallBufSize) += 512;  /* increment by 512 bytes each time we allocate */
        if(listComponent != (DMI_ListComponentReq_t *)NULL) OS_Free(listComponent);   /* free up the memory block, we are done with it */
        listComponent = OS_Alloc(sizeof(DMI_ListComponentReq_t) + (*InstallBufSize));	/* Space for confirm buf */
        if (listComponent == NULL) return (DMI_ListComponentReq_t *)NULL;      /* Can't generate event, but install is okay */
        (*cnfBuf) = BP(DMI_ListComponentCnf_t, listComponent + sizeof(DMI_ListComponentReq_t));
        listComponent->DmiMgmtCommand.iLevelCheck = DMI_LEVEL_CHECK;
        listComponent->DmiMgmtCommand.iCommand = DmiListComponentCmd;
        listComponent->DmiMgmtCommand.iCnfBufLen = ((*InstallBufSize) - sizeof(DMI_ListComponentReq_t));
        listComponent->DmiMgmtCommand.pCnfBuf = (*cnfBuf);
        listComponent->DmiMgmtCommand.iCnfCount = 0;
        listComponent->DmiMgmtCommand.iStatus = SLERR_NO_ERROR;
        listComponent->iComponentId = iComponentId;
        listComponent->osClassString = 0;
        DH_listComponent(listComponent);
    }while((listComponent->DmiMgmtCommand.iStatus == SLERR_BUFFER_FULL) ||
           (listComponent->DmiMgmtCommand.iStatus == SLERR_NO_ERROR_MORE_DATA));
    return listComponent;
}

TM_TaskStatus_t MH_uninstall(DMI_CiUninstallData_t _FAR *dmiCommand,void *OS_Context)
{
SL_ErrorCode_t          retVal;
DMI_ListComponentReq_t  *listComponent = NULL;
DMI_ListComponentCnf_t  *cnfBuf = NULL;
DMI_Indicate_t          *uninstallEvent;
boolean                 listComponentFailed = TRUE;        /* So we know if we can send the event */
MIF_Pos_t               componentPos;
DMI_UNSIGNED            InstallBufSize = 0;

    /*  Does the specified component exist?                                  */
    componentPos = DB_componentIdToPos(dmiCommand->iComponentId);
    if ((componentPos.page == 0) && (componentPos.offset == 0)) retVal = DBERR_COMPONENT_NOT_FOUND;
    else{  /* we've got it, remove the thing now...                          */
        /* Build a listComponentCnf block by creating a command block.  Have to do this before deleting component! */
        listComponent = BuildListComp(dmiCommand->iComponentId,&InstallBufSize,&cnfBuf);
        if (listComponent != NULL)      /* we've got the memory to build the indication */
            listComponentFailed = FALSE;
        if (DB_componentDelete(componentPos) != MIF_OKAY) retVal = SLERR_FILE_ERROR;
        else{   /*  Is it truly gone?                                        */
            componentPos = DB_componentIdToPos(dmiCommand->iComponentId);
            if ((componentPos.page == 0) && (componentPos.offset == 0)) retVal = SLERR_NO_ERROR;
            else retVal = SLERR_FILE_ERROR;
        }
    }
    MIF_writeCache(); /* make sure that the MIF cache is dumped to disk */
    if (retVal == SLERR_NO_ERROR) {
        if (listComponentFailed == FALSE){
            /* Generate an event for all mgmt apps                           */
            memset((char _FAR *)listComponent,0,sizeof(DMI_Indicate_t));
            uninstallEvent = (DMI_Indicate_t *)listComponent;
            uninstallEvent->iLevelCheck = DMI_LEVEL_CHECK;
            uninstallEvent->iCmdLen = InstallBufSize;
            uninstallEvent->iIndicationType = DMI_UNINSTALL_INDICATION;
            uninstallEvent->iComponentId = dmiCommand->iComponentId;
            uninstallEvent->oIndicationData = (char *)cnfBuf - (char *)uninstallEvent;
            MH_handleEvent(uninstallEvent, NULL);
        }
    }
    else SL_buildError(dmiCommand, retVal);
    if (listComponent != NULL) OS_Free(listComponent);
    OS_InstallNotice((char *)NULL,OS_Context,(DMI_MgmtCommand_t *)dmiCommand);   /* inform the caller that we are finished */
    return TM_DONE;    /* clean this task out of the work list */
}

TM_TaskStatus_t MH_install(DMI_CiInstallData_t _FAR *dmiCommand,void _FAR *OS_Context)
{
DMI_FileData_t  _FAR     *fileData;
char                     *mifFile,*Temp;
SL_ErrorCode_t           errorCode = SLERR_NO_ERROR;
DMI_STRING _FAR          *filePointer;
DMI_ListComponentReq_t   *listComponent = NULL;
DMI_ListComponentCnf_t   *cnfBuf = NULL;
DMI_Indicate_t           *installEvent;
DMI_UNSIGNED             FileCount;            /* number of file elements in this request */
DMI_UNSIGNED             InstallIndType;       /* indication type to emit after the install completes */
DMI_UNSIGNED             InstallBufSize = 0;  /* preset the install indication block to 512 bytes */

    fileData = dmiCommand->DmiFileList;
    (dmiCommand->DmiMgmtCommand.iCnfCount) = 0;  /* reset the confirm count, just in case */
    for(FileCount = 0;FileCount != dmiCommand->iFileCount;FileCount++,fileData++){  /* walk through file list */
        filePointer = BFP(DMI_STRING, dmiCommand + fileData->oFileData);
        mifFile = (char *)NULL;
        errorCode = SLERR_NO_ERROR;    /* preset this to the no-error case       */
        switch (fileData->iFileType){
            case DMI_MIF_FILE_POINTER: /* If file pointer, copy data to a temp file */
            case MIF_SNMP_MAPPING_FILE_POINTER_FILE_TYPE:
                Temp = tmpnam((char *)NULL);
                mifFile = OS_Alloc(strlen(Temp));
                if (mifFile != NULL){
                    strcpy(mifFile,Temp);
                    errorCode = OS_MIFbuftoFile(mifFile,filePointer->body, filePointer->length);
                }
                else errorCode = SLERR_OUT_OF_MEMORY;
                break;
            case MIF_SNMP_MAPPING_FILE_NAME_FILE_TYPE:
            case DMI_MIF_FILE_NAME:
                mifFile = OS_Alloc(filePointer->length + 1);
                if (mifFile != NULL){
                    memset(mifFile,0,(filePointer->length + 1));
                    memcpy(mifFile, filePointer->body, (unsigned)filePointer->length);
                }
                else  errorCode = SLERR_OUT_OF_MEMORY;
                break;
            default:
                errorCode = SLERR_INVALID_FILE_TYPE;
                break;
        }
        if(errorCode == SLERR_NO_ERROR){   /* OK, we can go ahead and get things done here */
            errorCode = PR_main(mifFile,&(dmiCommand->iComponentId),&InstallIndType,OS_Context);   /* do all of the really dirty work here */
            if(errorCode == SLERR_NO_ERROR){
                listComponent = BuildListComp(dmiCommand->iComponentId,&InstallBufSize,&cnfBuf);
                if (listComponent != NULL){      /* we've got the memory to build the indication */
                    /* Generate an event for all mgmt apps                   */
                    memset((char _FAR *)listComponent,0,sizeof(DMI_Indicate_t));
                    installEvent = (DMI_Indicate_t *)listComponent;
                    installEvent->iLevelCheck = DMI_LEVEL_CHECK;
                    installEvent->iCmdLen = InstallBufSize;
                    installEvent->iIndicationType = InstallIndType;
                    installEvent->iComponentId = dmiCommand->iComponentId;
                    installEvent->oIndicationData = (char *)cnfBuf - (char *)installEvent;
                    MH_handleEvent(installEvent, NULL);
                    OS_Free(listComponent);   /* free up the memory block, we are done with it */
                }
            }
        }
        if(mifFile != (char *)NULL){
            if((fileData->iFileType == DMI_MIF_FILE_POINTER) && (errorCode == SLERR_NO_ERROR)){
                OS_BackupMifFile(mifFile);   /* ask the OS specific stuff to move this to the backup directory */
                remove(mifFile);
            }
            OS_Free(mifFile);
        }
        if(errorCode != SLERR_NO_ERROR) break;
        else (dmiCommand->DmiMgmtCommand.iCnfCount)++;
    }       /* bottom of file-list for loop */
    if(errorCode != SLERR_NO_ERROR) SL_buildError(dmiCommand, errorCode);
    MIF_writeCache(); /* make sure that the MIF cache is dumped to disk */
    OS_InstallNotice((char *)NULL,OS_Context,(DMI_MgmtCommand_t *)dmiCommand);   /* inform the caller that we are finished */
    return TM_DONE;    /* clean this task out of the work list */
}

