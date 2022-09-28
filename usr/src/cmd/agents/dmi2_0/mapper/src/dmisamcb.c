/* Copyright 08/20/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisamcb.c	1.4 96/08/20 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisamcb.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support DMI access.             */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        dmiConfirmHandler(...)      */
/*                                        putDmiResponse(...)         */
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
/*                  941214    LAUBLI    New module                    */
/*                  950131    LAUBLI    Increase trace level          */
/*                  950317    LAUBLI    Added check to malloc         */
/*                                                                    */
/* End Change Log *****************************************************/

#include "dmisa.h"   /* global header file for DMI Sub-agent          */

extern char logbuffer[];  /* for tracing                         */
#ifdef DMISA_TRACE
unsigned char logbuffer[132];  /* for tracing                         */
#endif

/* static KeyListPtr decodeToTagList(USHORT KeyCount,DMI_GroupKeyData_t *Keys,ULONG BaseOffset);*/
static int putDmiResponse(void *pReqBuf);

/**********************************************************************/
/* Function:  dmi_confirm_handler()                                   */
/*                                                                    */
/* Receive DMI response to previously-issued request.                 */
/* Called by Service Layer.                                           */
/*                                                                    */
/* Error conditions:                                                  */
/*   1. When queue entry length != 12, return NULL in pReqBuf pointer */
/**********************************************************************/
void APIENTRY dmiConfirmHandler(void *Command)
{
DMI_MgmtCommand_t *miCommand;
DMI_RegisterCnf_t *Response;
DMI_STRING *Work;

   miCommand = Command;

#ifndef WIN32
   DMISA_TRACE_LOG1(LEVEL5,"  dmi_cnf_handler:  Returned from Service Layer.  iStatus = %ul\n",miCommand->iStatus);
   DMISA_TRACE_LOG1(LEVEL5,"  miCommand = %p\n",miCommand);
#endif

   switch(miCommand->iCommand){    /* switch on the command in use    */
      case DmiRegisterMgmtCmd:
            if(((DMI_MgmtCommand_t *) miCommand) -> iStatus == 0){   /* all went well*/
                Response = (DMI_RegisterCnf_t *)miCommand->pCnfBuf;  /* type it first, it's easier*/
                DmiHandle = Response->iDmiHandle;
                if (Response->DmiVersion.osDmiSpecLevel < miCommand->iCnfBufLen &&
                    Response->DmiVersion.osImplDesc < miCommand->iCnfBufLen) {
                   Work = (DMI_STRING *)((char *)Response + Response->DmiVersion.osDmiSpecLevel);  /* point to the MIF string*/
#ifndef WIN32
                   DMISA_TRACE_LOG1(LEVEL4,"  Response = %p\n",(void *)Response);
                   DMISA_TRACE_LOG1(LEVEL4,"  Response->DmiVersion.osDmiSpecLevel = %p.",(Response + Response->DmiVersion.osDmiSpecLevel));
#endif

                   memset(SpecLevel,0,sizeof(SpecLevel));
                   strncpy(SpecLevel,Work->body,Work->length);
                   Work = (DMI_STRING *)((char *)Response + Response->DmiVersion.osImplDesc);
                   memset(SLDescription,0,sizeof(SLDescription));
                   strncpy(SLDescription,Work->body,Work->length);
                } else {
                   /* Got an addressing problem in the confirm buffer */
                   DMISA_TRACE_LOG(LEVEL1, "Addressing problem in confirm buffer during registration.");
                }
            } else {    /* this is an error condition                 */
                DMISA_TRACE_LOG1(LEVEL1,"ERROR, SL Unable to register: %x",((DMI_MgmtCommand_t *)miCommand)->iStatus);
            }
            break;
        case DmiUnregisterMgmtCmd:  /* this is the unregister request callback*/
        case DmiListComponentDescCmd:   /* the list component command */
        case DmiListAttributeDescCmd:   /* this is the description command for the attribute*/
        case DmiListGroupDescCmd:       /* list the group description */
        case DmiListFirstComponentCmd:
        case DmiListNextComponentCmd:
        case DmiListFirstGroupCmd:
        case DmiListNextGroupCmd:
        case DmiGetFirstRowCmd:    /* response to the query row information....*/
        case DmiGetNextRowCmd:
        case DmiListComponentCmd:
        case DmiListGroupCmd:  /* used to load up the attribute display dialog*/
        case DmiListAttributeCmd:  /* used to load up the attribute display dialog*/
        case DmiListFirstAttributeCmd:
        case DmiListNextAttributeCmd:
        case DmiSetAttributeCmd:
        case DmiGetAttributeCmd:
            break;
    }
    putDmiResponse( miCommand );  /* if bad return, no special action - cannot handle it*/
}

/*******************************************************************************/
/* Function:  putDmiResponse()                                                 */
/*                                                                             */
/* Description:                                                                */
/*   Copy DMI Response and put pointers into queue entry in response           */
/*   to a previous request submitted by this sub-agent to the DMI.             */
/*                                                                             */
/* Notes:                                                                      */
/*   1.  pQueueEntry points to storage allocated but not freed                 */
/*       by this function.  It must be freed by the caller.                    */
/*                                                                             */
/* Return code:                                                                */
/*   PUT_DMI_RESPONSE_noError     Successfully completed executing the function*/
/*   PUT_DMI_RESPONSE_writeQErr   Return code from DosWriteQueue is non-zero   */
/*   PUT_DMI_RESPONSE_postSemErr  Return code from DosPostEventSem is non-zero */
/*******************************************************************************/
int putDmiResponse( void *pReqBuf )     /* to bring up DMI S.L. response into DMI sub-agent*/
{
QueueEntryStr *pQueueEntry;
APIRET rcsem,rcque;

   /* Query event semaphore                                           */
#ifdef WIN32
/* Not sure what to do here. For now do nothing.               */
#elif defined OS2
/* this is really only relevant for timeout, add later $MED */
   rcsem = DosQueryEventSem(CnfSemHandle, &ulPostCt );

   if (rcsem == 0 && ulPostCt == 0) {  /* If semaphore is reset       */
      DMISA_TRACE_LOG2(LEVEL5,"  Semaphore handle = %d, Post count before posting = %d.\n", CnfSemHandle,ulPostCt);
#else
      mutex_lock(&queueMutex); /* right? $MED */
#endif

      /* Create queue entry                                           */
      pQueueEntry = ( void * )malloc( sizeof( QueueEntryStr ));
      if (pQueueEntry) {                                          /* added 950317.gwl */
         memset(pQueueEntry,0,sizeof( QueueEntryStr ));   /* necessary to meet good practice standards?*/
         pQueueEntry->pReqBuf = pReqBuf;
         pQueueEntry->pQueueEntry = pQueueEntry;

#ifndef WIN32
         DMISA_TRACE_LOG3(LEVEL5,"  Queue Entry: Req = %p, QueueEntry = %p.  Confirm buffer = %p",
                       pQueueEntry->pReqBuf,pQueueEntry->pQueueEntry,
                       ((DMI_MgmtCommand_t *)pQueueEntry->pReqBuf)->pCnfBuf);
#endif

         /* Write into queue                                             */
#ifdef OS2
         QRequest.ulData = 0;  /*   Assume no special data needs to be transferred*/
         ElemPriority = 0;
         rcque = DosWriteQueue( QueueHandle, QRequest.ulData,
                                (ULONG)  sizeof(QueueEntryStr),
                                pQueueEntry, ElemPriority );
         if ( rcque != 0 ) {  /* rcque = 0, 334quenomem, 337queinvhandle */
            DMISA_TRACE_LOG1(LEVEL1,"DosWrite Queue error: return code = %u", rcque );
            return PUT_DMI_RESPONSE_writeQErr;
         }
         if ( rcque == 0 ) {
            DMISA_TRACE_LOG1(LEVEL5,"  DosWriteQueue successful, length = %u.", (ULONG)sizeof(QueueEntryStr));
         }
#else
/* LLR 09-15-95 Note: use for Win32 also */
         if (pQueueHead == NULL) /* queue is empty... */
            pQueueHead = pQueueEntry;
         else /* put entry at end of queue... */
         {
            QueueEntryStr *pQueueTemp = pQueueHead;
            while (pQueueTemp->pNext != NULL)
               pQueueTemp = pQueueTemp->pNext;
            pQueueTemp->pNext = pQueueEntry;
         } /* put entry at end of queue */
#endif

         /* Post event semaphore                                         */
#ifdef OS2
         rcsem = DosPostEventSem(CnfSemHandle);
#elif defined WIN32
         if(SetEvent(queueMutex))
		   rcsem = 0;
		 else
		   rcsem = 1;
		 
#else
         rcsem = cond_broadcast(&queueCond);
         rcsem = mutex_unlock(&queueMutex);
#endif
         if ( rcsem != 0 ) {  /* rcsem = 0, 6invhandle, 298toomanyposts, 299alreadyposted*/
            DMISA_TRACE_LOG1(LEVEL1,"DosPostEventSem error: return code = %u.", rcsem );
            return PUT_DMI_RESPONSE_postSemErr;
         }

#ifndef WIN32
         DMISA_TRACE_LOG(LEVEL5,"  DosPostEventSem successful." );
#endif

      } else {

#ifndef WIN32
         DMISA_TRACE_LOG(LEVEL1,"Error: Memory allocation in DMISAMCB failed."); /* added 950317.gwl */
#endif

      }
#ifdef OS2
   }
#endif

   return PUT_DMI_RESPONSE_noError;
}

