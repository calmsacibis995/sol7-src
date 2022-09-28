/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_om.c	1.3 96/09/24 Sun Microsystems"


/*****************************************************************************************************************************************

    Filename: psl_om.c
    
    Copyright (c) International Business Machines, Corp. 1994
    Copyright (c) Intel, Inc. 1992-1994

    Description: Overlay Manager

    Author(s): Paul A. Ruocchio, Alvin I. Pivowar, Steve Hanrahan

         RCS Revision: $Header: n:/sl/rcs/psl_om.c 1.44 1994/04/15 13:32:50 shanraha Exp $

    Revision History:

        Date        Author  Description
        -------     ------  -----------------------------------------------
        03/19/93    aip     Creation date.
        10/28/93    sfh     Change dos_dmi.h to os_dmi.h.
        11/11/93    sfh     Remove include of types.h.
        02/22/94    sfh     Redo to conform to asm build version changes.
        02/24/94    sfh     Fix runIt to pass correct parameter to overlayRunOrCancel.
        03/04/94    sfh     Add parent/child code.  Revise parent/child code.
        03/23/94    sfh     Convert fstring functions to string. Remove _pascal.
        04/15/94    sfh     Add countSelector to usages of DmiInstrumentationVector where applicable.
        05/20/94    par     Completely re-written to be OS-Neutral, really portable!
        06/18/94    par     Changed name to conform to new naming standards.

    Description: Overlay Manager


*****************************************************************************************************************************************/

/*************************************************************** INCLUDES ***************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */
#include "psl_util.h"
#include "psl_dh.h"
#include "db_api.h"

/*****************************************************************************************************************************************/


/***************************************************************** DEFINES **************************************************************/

#define BFP(type, parts) (type _FAR *)((char _FAR *)parts)

/**************************************************************** TYPEDEFS ***************************************************************/

/*****************************************************************************************************************************************/

		
/**************************************************************** GLOBALS ****************************************************************/

static OM_OverlayTable_t *OverlayTable = NULL;
static void OM_InsertOverlay(OM_OverlayTable_t _FAR *Overlay);
static BOOL _OM_AddToPending(OM_OverlayTable_t *Overlay,DMI_MgmtCommand_t _FAR *dmiCommand);
static void _OM_RemoveFromPending(OM_OverlayTable_t *Overlay,DMI_MgmtCommand_t _FAR *dmiCommand);
static void _OM_OverlayBusyCall(OM_OverlayTable_t *Overlay);

extern CI_Registry_t    *direct;

/*****************************************************************************************************************************************/


TM_TaskStatus_t OM_loader(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *NameDesc)
{
OM_OverlayTable_t   *Overlay,*NewOvl;
SL_ErrorCode_t      Error = SLERR_NO_ERROR;
TM_Task_t           nextTask;

    NewOvl = (OM_OverlayTable_t *)NameDesc;
    OS_EnterCritSec();     /* protect ourselves while we look through the overlay list */
    Overlay = OverlayTable;   /* start at the beginning                      */
    while(Overlay != (OM_OverlayTable_t *)NULL){   /* walk through the list  */
        if (Overlay->Type == OM_OVERLAY){    /* this one is an overlay, check it out */
            if((Overlay->componentId == NewOvl->componentId) &&
               (stringCmp(Overlay->symbolicName,NewOvl->symbolicName) == 0)){  /* found it, don't need the new table entry, free it */
                OM_FreeOverlay(NewOvl);   /* get rid of the new overlay table, we don't need it */
                Overlay->UseCount++;       /* increment the number of users of this overlay */
                break;     /* bug out of the while loop, and start this thing up */
            }
        }
        Overlay = Overlay->next;    /* advance to the next element           */
    }
    OS_ExitCritSec();    /* OK, we are in the clear                          */
    if(Overlay == (OM_OverlayTable_t *)NULL){    /* set up a new one before we start it up */
        Error = OS_LoadOverlay(NewOvl);   /* ask the OS dependant code to load up the overlay */
        if (Error == SLERR_NO_ERROR){    /* all went well, set it up to run  */
            /* Free path name memory, but keep the rest for now              */
            OS_Free(NewOvl->pathName);
            NewOvl->pathName = NULL;
            Overlay = NewOvl;            /* we will need this below          */
            Overlay->UseCount++;       /* increment the number of users of this overlay */
            OM_InsertOverlay(NewOvl);    /* chain it into the list           */
        }
        else OM_FreeOverlay(NewOvl); /* free up the new overlay block        */
    }
    if(Error != SLERR_NO_ERROR){  /* we've had an error somewhere in the load process */
        SL_buildError(dmiCommand, Error);
        nextTask = TM_CALLBACK;
    }
    else nextTask = TM_RUN_COMPONENT;
    TM_taskSwitch(nextTask, dmiCommand,(void *)Overlay);
    return TM_NOT_DONE;
}

void OM_FreeOverlay(OM_OverlayTable_t *Overlay)
{
OM_OverlayTable_t *Look,*Prev;

    if(Overlay != (OM_OverlayTable_t *)NULL){
        OS_EnterCritSec();
        Prev = (OM_OverlayTable_t *)NULL;
        Look = OverlayTable;
        while((Look != (OM_OverlayTable_t *)NULL) && (Look != Overlay)){   /* walk through the table */
            Prev = Look;
            Look = Look->next;
        }
        if(Look != (OM_OverlayTable_t *)NULL){   /* we've found it, is in the chain - remove it */
            if(Prev == (OM_OverlayTable_t *)NULL) OverlayTable = Look->next;
            else Prev->next = Look->next;
        }
        else Look = Overlay;
        OS_ExitCritSec();
        if(Look->Type == OM_OVERLAY){   /* we only want to do this cleanup if it is an Overlay type */
            if(Look->pathName) OS_Free(Look->pathName);     /* free up the new path name space */
            if(Look->symbolicName) OS_Free(Look->symbolicName); /* free up the symbolic name space */
            if(Look->OSdata) OS_UnloadOverlay(Look);        /* call the OS specific stuff to unload it */
        }
        OS_Free(Look);                                     /* finally, free the table entry */
    }
}


static void OM_InsertOverlay(OM_OverlayTable_t _FAR *Overlay)
{
OM_OverlayTable_t *p;

    OS_EnterCritSec();
    if (OverlayTable == (OM_OverlayTable_t *)NULL) OverlayTable = Overlay;   /* only one in the table */
    else{  /* need to add it to the end of the list                          */
        for (p = OverlayTable;p->next != NULL;p = p->next);
        p->next = Overlay;
    }
    Overlay->next = (OM_OverlayTable_t *)NULL;    /* make sure we are terminated here */
    OS_ExitCritSec();
    return;
}


static void OM_overlayNameBuild(DMI_MgmtCommand_t _FAR *dmiCommand, DMI_STRING _FAR *instrumentationName, DMI_STRING _FAR *pathname)
{
OM_OverlayTable_t   *ovInfo = NULL;
DMI_STRING        nullString = {1, '\0'};
DMI_UNSIGNED        command;

    if ((ovInfo = (OM_OverlayTable_t *)OS_Alloc(sizeof(OM_OverlayTable_t))) != NULL){
        memset(ovInfo,0,(sizeof(OM_OverlayTable_t)));   /* clear out the whole thing */
        if((ovInfo->symbolicName = OS_Alloc(stringLen(instrumentationName) + 4)) != NULL){ /* Make room for 0 terminator */
            stringCpy(ovInfo->symbolicName, instrumentationName);
            stringCat(ovInfo->symbolicName, &nullString);
            if ((ovInfo->pathName = OS_Alloc(stringLen(pathname) + 4)) != NULL){ /* Make room for 0 terminator */
                stringCpy(ovInfo->pathName, pathname);
                stringCat(ovInfo->pathName, &nullString);
                command = dmiCommand->iCommand;
                if (command == DmiGetAttributeCmd) ovInfo->componentId = ((DMI_GetAttributeReq_t _FAR *)dmiCommand)->iComponentId;
                else if (command > DmiGetRowCmd)  ovInfo->componentId = ((DMI_GetRowReq_t _FAR *)dmiCommand)->iComponentId;
                else ovInfo->componentId = ((DMI_SetAttributeReq_t _FAR *)dmiCommand)->iComponentId;
                ovInfo->Type = OM_OVERLAY;
                TM_taskSwitch(TM_CALL_COMPONENT, (DMI_MgmtCommand_t _FAR *)dmiCommand, ovInfo);
                return;
            }
            OS_Free(ovInfo->symbolicName);
        }
        OS_Free(ovInfo);
    }
    SL_buildError(dmiCommand,SLERR_OUT_OF_MEMORY);   /* there is only one type of error we can have in here */
    TM_taskSwitch(TM_CALLBACK, (DMI_MgmtCommand_t _FAR *)dmiCommand, ovInfo);
}



TM_TaskStatus_t OM_executeInstrumentation(DMI_MgmtCommand_t _FAR *dmiCommand, DMI_InstrumentationVector_t  _FAR *instrumentationVector)
{
void _FAR                               *cnfBuf = dmiCommand->pCnfBuf;
DMI_UNSIGNED                            command = dmiCommand->iCommand;
DMI_InstrumentationDescriptor_t _FAR    *instrumentationDesc;
MIF_Id_t                    componentId;
MIF_Id_t                    groupId;
MIF_Id_t                    attributeId;
DMI_RegisterCiInd_t         *reg;
TM_Task_t                   nextTask = TM_DIRECT;
CI_Registry_t          _FAR *h;
DMI_GetAttributeCnf_t  _FAR *ThisReq;

    instrumentationDesc = &instrumentationVector->DmiInstrumentation[instrumentationVector->countSelector.iSelector];
    if (instrumentationDesc->osPathname != 0) /*  this is an overlay, go ahead and set it up */
        OM_overlayNameBuild(dmiCommand, BFP(DMI_STRING, cnfBuf + instrumentationVector->osInstrumentationName),BFP(DMI_STRING, cnfBuf + instrumentationDesc->osPathname));
    else{
        switch (command){
            case DmiGetAttributeCmd:
                componentId = ((DMI_GetAttributeReq_t _FAR *)dmiCommand)->iComponentId;
                break;
            case DmiSetAttributeCmd:
            case DmiSetReserveAttributeCmd:
            case DmiSetReleaseAttributeCmd:
                componentId = ((DMI_SetAttributeReq_t _FAR *)dmiCommand)->iComponentId;
                break;
            default:        /* Command has been verified                     */
                componentId = ((DMI_GetRowReq_t _FAR *)dmiCommand)->iComponentId;
                break;
        }
        ThisReq = (DMI_GetAttributeCnf_t _FAR *)(dmiCommand->DmiCiCommand.pCnfBuf);
        attributeId = ThisReq->iAttributeId;
        groupId     = ThisReq->iGroupId;
        reg = MH_directLookup(componentId, groupId, attributeId,(DMI_UNSIGNED)SL_ENVIRONMENT_ID);
        OS_EnterCritSec();    /* turn off the other threads while we search for this thing */
        h = direct;           /* start at the front end of this thing            */
        while(h != (CI_Registry_t _FAR *)NULL){   /* walk through the list            */
            if(reg == h->ciRegistry) break;
            h = h->next;      /* advance to the next element                     */
        }
        OS_ExitCritSec();
        if (h == (CI_Registry_t *)NULL){ /* Shouldn't happen unless parent changes something! */
            SL_buildError(dmiCommand, SLERR_UNKNOWN_CI_REGISTRY);
            nextTask = TM_CALLBACK;
        }
        TM_taskSwitch(nextTask, dmiCommand, (void *)h);
    }
    return TM_NOT_DONE;
}

/* This function sets up an overlay descriptor for a direct interface call.  */
TM_TaskStatus_t OM_direct(DMI_MgmtCommand_t _FAR *dmiCommand,void _FAR *highPerf)
{
OM_OverlayTable_t   *Overlay;
TM_Task_t           nextTask;

    nextTask = TM_CALLBACK;   /* default to the error condition              */
    OS_EnterCritSec();     /* protect ourselves while we look through the overlay list */
    Overlay = OverlayTable;   /* start at the beginning                      */
    while(Overlay != (OM_OverlayTable_t *)NULL){   /* walk through the list  */
        if((Overlay->Type == OM_DIRECT) &&    /* this one is an overlay, check it out */
           (highPerf == Overlay->OSdata) ){ 
            Overlay->UseCount++;       /* increment the number of users of this overlay */
            break; /* found it, don't need the new table entry */
        }
        Overlay = Overlay->next;    /* advance to the next element           */
    }
    OS_ExitCritSec();    /* OK, we are in the clear                          */
    if(Overlay == (OM_OverlayTable_t *)NULL){  /* didn't find it, create a new one */
        if((Overlay = (OM_OverlayTable_t *)OS_Alloc(sizeof(OM_OverlayTable_t))) != NULL){
            memset(Overlay,0,(sizeof(OM_OverlayTable_t)));   /* clear out the whole thing */
            Overlay->Type = OM_DIRECT;     /* this is a direct overlay creature */
            Overlay->componentId = ((CI_Registry_t *)highPerf)->ciRegistry->iComponentId;
            Overlay->OVStatus = OM_OVL_IDLE;     /* all ready for that first big call */
            Overlay->OSdata = highPerf;    /* put the direct interface stuff here */
            Overlay->UseCount++;       /* increment the number of users of this overlay */
            OM_InsertOverlay(Overlay);    /* chain it into the list          */
            nextTask = TM_RUN_COMPONENT;
        }
        else SL_buildError(dmiCommand, SLERR_OUT_OF_MEMORY);
    }
    else nextTask = TM_RUN_COMPONENT;
    TM_taskSwitch(nextTask, dmiCommand,(void *)Overlay);
    return TM_NOT_DONE;
}


TM_TaskStatus_t OM_run(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *Ovl)
{
OM_OverlayTable_t   *Overlay;

    /* Find overlay to run next -- note we will only make one pass through the table... */
    OS_EnterCritSec();
    Overlay = OverlayTable;   /* point to the first one in the list          */
    while((Overlay != (OM_OverlayTable_t *)NULL) && (Overlay != (OM_OverlayTable_t *)Ovl))   /* let's make sure this overlay is still around */
        Overlay = Overlay->next;
    if(Overlay == (OM_OverlayTable_t *)NULL){  /* this overlay is not in our lists, error time */
        SL_buildError(dmiCommand,OMERR_OVERLAY_NOT_FOUND);   /* there is no overlay for this one loaded */
        OS_ExitCritSec();
        return TM_DONE_CALLBACK;
    }
    if(_OM_AddToPending(Overlay,dmiCommand) == FALSE){ /* chain in the command to the list */
        SL_buildError(dmiCommand,SLERR_OUT_OF_MEMORY);   /* there is no overlay for this one loaded */
        OS_ExitCritSec();
        return TM_DONE_CALLBACK;
    }
    _OM_OverlayBusyCall(Overlay);
    OS_ExitCritSec();
    return TM_IDLE;
}

static void _OM_OverlayBusyCall(OM_OverlayTable_t *Overlay)
{
DMI_Confirm_t       FakeConfirm;   /* used for immediate bad return codes */
OM_OverlayPending_t *Walk;

    OS_EnterCritSec();
    if(Overlay->OVStatus != OM_OVL_BUSY){  /* this one is looking for work to do   */
        Overlay->OVStatus = OM_OVL_BUSY;      /* set this up so we can handle the callback properly */
        FakeConfirm.iStatus = OS_OverlayCall(OM_RUN,Overlay,Overlay->Pending->Command);     /* execute the current worklist item */
        if (FakeConfirm.iStatus != SLERR_NO_ERROR) { /* Houston, we've had a problem...   */
            if(FakeConfirm.iStatus == CPERR_CI_TERMINATED){   /* the overlay timed out on us */
                Walk = Overlay->Pending;    /* start walking the pending overlay command list */
                while(Walk != (OM_OverlayPending_t *)NULL){   /* walk through the rest of the list */
                    SL_buildError(Walk->Command,CPERR_CI_TERMINATED);   /* there is no overlay for this one anymore */
                    TM_taskRequeue(TM_CALLBACK,Walk->Command,NULL);
                    _OM_RemoveFromPending(Overlay,Walk->Command);   /* remove this one from the list */
                    Walk = Overlay->Pending;    /* advance to the next pending overlay command */
                }
                OM_FreeOverlay(Overlay);   /* remove the overlay... */
            }
            else{  /* it went well, go ahead and send the confirm */
                FakeConfirm.iLevelCheck = DMI_LEVEL_CHECK;  /* set the fake level */
                FakeConfirm.pDmiMgmtCommand = Overlay->Pending->Command;
                OM_responseDone(&FakeConfirm);
            }
        }
    }
    OS_ExitCritSec();
}

static BOOL _OM_AddToPending(OM_OverlayTable_t *Overlay,DMI_MgmtCommand_t _FAR *dmiCommand)
{
OM_OverlayPending_t *New,*Walk,**Assign;
BOOL Found = FALSE;

    OS_EnterCritSec();
    Walk = Overlay->Pending;    /* need to put this at the end of the pending list */
    Assign = &(Overlay->Pending);   /* nothing pending here */
    while(Walk != (OM_OverlayPending_t *)NULL){ 
        if(Walk->Command == dmiCommand){   /* the command is already in the list */
            Found = TRUE;
            break;       /* indicate that we've located the command in the list already, bug out */
        }
        Assign = &(Walk->Next);
        Walk = Walk->Next;
    }
    if(Found == FALSE){
        if((New = OS_Alloc(sizeof(OM_OverlayPending_t))) != (OM_OverlayPending_t *)NULL){
            memset(New,0,sizeof(OM_OverlayPending_t));
            New->Command = dmiCommand;
            (*Assign) = New;
            Found = TRUE;
        }
    }
    OS_ExitCritSec();
    return Found;     /* return either TRUE or FALSE */
}

static void _OM_RemoveFromPending(OM_OverlayTable_t *Overlay,DMI_MgmtCommand_t _FAR *dmiCommand)
{
OM_OverlayPending_t *Walk,*Prev;

    OS_EnterCritSec();
    Walk = Overlay->Pending;    /* need to put this at the end of the pending list */
    Prev = (OM_OverlayPending_t *)NULL;
    while(Walk != (OM_OverlayPending_t *)NULL){
        if(Walk->Command == dmiCommand){   /* we've found it */
            if(Prev == (OM_OverlayPending_t *)NULL) Overlay->Pending = Walk->Next;
            else Prev->Next = Walk->Next;
            OS_Free(Walk);   /* return the element to the pool */
            break;
        }
        Prev = Walk;         /* save this one */
        Walk = Walk->Next;   /* advance to the next element */
    }
    OS_ExitCritSec();
}


ULONG DMI_FUNC_ENTRY OM_responseDone(DMI_Confirm_t _FAR *ciConfirm)
{
OM_OverlayTable_t *Overlay = OverlayTable;
DMI_MgmtCommand_t *ciCommand = ciConfirm->pDmiMgmtCommand;
DMI_MgmtCommand_t *dmiCommand;
DMI_UNSIGNED Flag = 0;

    if ((ciConfirm->iLevelCheck == DMI_LEVEL_CHECK) || (ciConfirm->iLevelCheck == DMI_LEVEL_CHECK_V1)){  /* this is a good response */
        OS_EnterCritSec();
        while (Overlay != NULL){    /* find the command in the overlay maze... */
            if((Overlay->OVStatus == OM_OVL_BUSY) && (Overlay->Pending != (OM_OverlayPending_t *)NULL)){
                dmiCommand = Overlay->Pending->Command;
                if(dmiCommand != (DMI_MgmtCommand_t *)NULL){
                    if ( ciCommand->iCmdHandle == dmiCommand->iCmdHandle &&
                         ciCommand->iMgmtHandle == dmiCommand->iMgmtHandle) {
                        Overlay->OVStatus = OM_OVL_IDLE;        /* ready for more work, if there is any */
                        _OM_RemoveFromPending(Overlay,dmiCommand);
                        if(Overlay->Pending != (OM_OverlayPending_t *)NULL)   /* there are still commands waiting here */
                            TM_taskRequeue(TM_RUN_COMPONENT,Overlay->Pending->Command,(void *)Overlay);
                        if(Overlay->UseCount) Overlay->UseCount--;   /* reset this so it will get cleaned up later */
                        if(dmiCommand->pCnfBuf != ciCommand->pCnfBuf){
                            memcpy((char *)(dmiCommand->pCnfBuf),
                                   (char *)(ciCommand->pCnfBuf),
                                    dmiCommand->iCnfBufLen);
                            ciConfirm->pDmiMgmtCommand = dmiCommand;   /* reset the confirm to point to the right buffer */
                            Flag = 1;    /* indicate that we've changed the confirm ptr */
                        }
                        SL_packResponse(ciConfirm);
                        if(dmiCommand->iStatus != SLERR_NO_ERROR) TM_taskRequeue(TM_CALLBACK,dmiCommand,NULL);
                        else TM_taskRequeue(TM_DB,dmiCommand, NULL);   /* send this back to the tasker, database time */
                        if(Flag) ciConfirm->pDmiMgmtCommand = ciCommand;
                        break;
                    }
                }
            }
            Overlay = Overlay->next;
        }
        OS_ExitCritSec();
    }
    else SL_buildError(ciCommand, SLERR_ILLEGAL_DMI_LEVEL);   /* this response is an error */
    return SLERR_NO_ERROR;
}

void OM_OverlayCleanup(void)  /* called by the tasker, to clean up and unload any overlay we are done with */
{
OM_OverlayTable_t   *Overlay,*Next;

    Next = OverlayTable;            /* point to the front of the list        */
    while(Next != (OM_OverlayTable_t *)NULL){ /* walk through the list       */
        Overlay = Next;    /* this is the one we want to work on now         */
        Next = Next->next; /* save the next element, in case we remove this one on this pass */
        if((Overlay->Pending == (OM_OverlayPending_t *)NULL) && (!Overlay->UseCount))
            OM_FreeOverlay(Overlay);
    }
}

void OM_cancel(DMI_UNSIGNED command, DMI_UNSIGNED maHandle, DMI_UNSIGNED cmdHandle)
{
OM_OverlayTable_t   *Overlay,*Next;
BOOL KillFlag = FALSE;
BOOL LocalKill;
OM_OverlayPending_t *Walk,*Hold;

    OS_EnterCritSec();  /* we are going to be playing with the tables here... */
    Next = OverlayTable;            /* point to the front of the list        */
    while(Next != (OM_OverlayTable_t *)NULL){ /* walk through the list       */
        Overlay = Next;    /* this is the one we want to work on now         */
        Next = Next->next; /* save the next element, in case we remove this one on this pass */
        if((command == DmiKillOverlayCmd) && ((DMI_UNSIGNED)(Overlay->OSdata) == maHandle)) KillFlag = TRUE;
        Walk = Overlay->Pending;
        while(Walk != (OM_OverlayPending_t *)NULL){  /* walk through the pending list */
            LocalKill = FALSE;
            Hold = Walk->Next;   /* save the next element, in case we ditch this one */
            if((Walk->Command->iMgmtHandle == maHandle) &&
               ((Walk->Command->iCmdHandle == cmdHandle) || (command == DmiCancelAllCmd)) ) LocalKill = TRUE;
            if(KillFlag == TRUE || LocalKill == TRUE){
                SL_buildError(Walk->Command,CPERR_CI_TERMINATED);
                if(Overlay->OVStatus == OM_OVL_BUSY){  /* there is a command in process on this thing, cancel it */
                    OS_OverlayCall(OM_CANCEL,Overlay,Walk->Command);
                    Overlay->OVStatus = OM_OVL_IDLE;
                    TM_taskRequeue(TM_CALLBACK,Walk->Command, NULL);
                }
                else TM_taskSwitch(TM_CALLBACK,Walk->Command, NULL);
                _OM_RemoveFromPending(Overlay,Walk->Command);
            }
            Walk = Hold;    /* reset to the next element, if there is one */
        }
        if(Overlay->UseCount) Overlay->UseCount--;   /* reset this so it will get cleaned up later */
    }
    OS_ExitCritSec();   /* OK, all done                                      */
}


