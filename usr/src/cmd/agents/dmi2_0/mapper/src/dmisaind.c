/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisaind.c	1.6 96/10/02 Sun Microsystems"

/***   START OF SPECIFICATIONS   **************************************/
/*                                                                    */
/* Source File Name       : dmisaind.c                                */
/*                                                                    */
/* Module  Name           : DMISAIND                                  */
/*                                                                    */
/*                                                                    */
/* IBM Confidential (IBM Confidential-Restricted                      */
/* when combined with the Aggregated OCO Source                       */
/* Programs/Modules for this Product)                                 */
/*                                                                    */
/* OCO Source Materials                                               */
/*                                                                    */
/* The Source code for this program is not published or otherwise     */
/* divested of its trade secrets, irrespective of what has been       */
/* deposited with the U.S. Copyright Office                           */
/*                                                                    */
/* 5763SS1  (C) Copyright IBM Corp. 1994                              */
/*                                                                    */
/* NOTE:                                                              */
/*  1994 is the original year the source code is written.             */
/*  1994 is the year any changes to the source code is made.          */
/*                                                                    */
/*                                                                    */
/* Source File Description:                                           */
/*                                                                    */
/* This module contains the routines used by the DMI subagent  program*/
/* used to handle the DMI indication function                         */
/*  3 Funtions:                                                       */
/*           dmiIndicateSetup - called at initation to set up waitForIndication thread  */
/*                                      and semaphore used to communicate the Servervice call.      */
/*           dmiIndicateHandler - entry point registered with the DMI Service Layer */
/*           waitForIndication -  wait on the semphore and handles the indication.  */
/*                                                                                      */
/*                                                                                      */
/* Change Activity:                                                               */
/*                                                                                */
/* CFD list:                                                                      */
/* Flag&Reason    Rls    Date   Pgmr        Description                           */
/* ------------ ---- ------ --------  --------------------------------------------*/
/*                            940622  DGeiser:    New code for DMI Subagent       */
/*                                                to setup DMI Indications.       */
/*                            941102  Laubli:     Added DMISAIND3 semaphore       */
/*                                                                                */
/* End CFD List.                                                                  */
/* Additional Notes About the Change Activity                                     */
/* End Change Activity.                                                           */
/***   END OF SPECIFICATIONS   ***************************************/
#include "dmisa.h"   /* global header file for DMI Sub-agent          */
#include "gentrap.h"
#define MAX_CONCURRENT_INDICATIONS 64

extern char logbuffer[];
#ifdef DMISA_TRACE
unsigned char logbuffer[132];
#endif
DMI_Indicate_t  *dmiBlock_ptr;
DMI_Indicate_t  *IndBuff[MAX_CONCURRENT_INDICATIONS]; /* Buffer for the Indicate Data */
DMI_ListComponentCnf_t *NewComp;
UCHAR   IndSemName[40];                        /* Indication Semaphore name */
UCHAR   Ind3SemName[40];                       /* Indication Semaphore name */
/*ULONG   Ind3Timeout = -1;  */                    /* Indefinite wait */
typedef struct    /* use linked list instead of array for indicates */
{
  PVOID          pNext;
  DMI_Indicate_t IndBuff;
} IndNode;
IndNode *pIndList;

/*********************************************************************/
/* Function: dmiIndicateSetup()                                      */
/*                                                                   */
/* Description:                                                      */
/*   Set up at initialization of program.                            */
/*********************************************************************/
int dmiIndicateSetup (void)
{
#if defined(OS2) || defined(WIN32)
TID    tidThread;
#else
thread_t       tidThread;                     /* thread id for indicate wait */
/*pthread_attr_t  tidThreadAttrs; Not reqd in Solaris threads*/
#endif
/* could try #define TID pthread_t $MED */
APIRET rc_indsem,rc_indthr;

   /* Create event semaphore used to pass an INDICATION from the service layer*/

#ifdef WIN32
   strcpy( (CHAR *)IndSemName,"DMISAIND1");
   IndSemHandle = CreateEvent((LPSECURITY_ATTRIBUTES) NULL, TRUE, TRUE, IndSemName );

   (DWORD) rc_indsem=GetLastError();

                /* On successful return, SemHandle contains handle of new system event semaphore                                     */
   if ( rc_indsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname, 285dupname, 290toomanyhndls */
      DMISA_TRACE_LOG(LEVEL1,"CreateEvent error for indication. <dmiIndicateSetup>");
      return(FAILURE);
   }

   DMISA_TRACE_LOG1(LEVEL2," Event semaphore %u successfully created for indication handling.",IndSemHandle);


#elif defined OS2
   strcpy( (CHAR *)IndSemName,"\\SEM32\\DMISAIND1");
   flAttr = 0;          /* Unused, since this is a named semaphore */
   fState = 0;          /* Semaphore initial state is "Set" */
   rc_indsem = DosCreateEventSem( IndSemName, &IndSemHandle, flAttr, fState );
                /* On successful return, SemHandle contains handle of new system event semaphore                                     */
   if ( rc_indsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname, 285dupname, 290toomanyhndls */
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateEventSem error for indication: return code = %u", rc_indsem);
      return(FAILURE);
   }

   DMISA_TRACE_LOG1(LEVEL2," Event semaphore %u successfully created for indication handling.",IndSemHandle);
#elif AIX325
   pthread_cond_init(&indicateCond, pthread_condattr_default);
#else
   cond_init(&indicateCond, USYNC_THREAD, NULL);
#endif

   /* Create mutex semaphore used to synchronize indication receipt with indication processing.*/
   /* Used in addition to the event semaphore.                        */

#ifdef WIN32
   strcpy( (CHAR *)Ind3SemName,"DMISAIND3");

   Ind3SemHandle = CreateMutex((LPSECURITY_ATTRIBUTES) NULL, FALSE, Ind3SemName );

   (DWORD) rc_indsem=GetLastError();

                /* On successful return, SemHandle contains handle of new system event semaphore                                     */
   if ( rc_indsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname, 285dupname, 290toomanyhndls */
      DMISA_TRACE_LOG(LEVEL1,"CreateMutex error for indication. <dmiIndicateSetup>");
      return(FAILURE);
   }

   DMISA_TRACE_LOG1(LEVEL2," Event semaphore %u successfully created for indication handling.",IndSemHandle);

#elif defined OS2
   strcpy((CHAR *)Ind3SemName,"\\SEM32\\DMISAIND3");
   flAttr = 0;          /* Unused, since this is a named semaphore */
   fState = 0;          /* Semaphore initial state is "owned" */
   rc_indsem = DosCreateMutexSem( Ind3SemName, &Ind3SemHandle, flAttr, fState );
      /* On successful return, Ind3SemHandle contains handle of new system event semaphore*/
   if ( rc_indsem != 0 ) { /* can be 0, 8outofmemory, 87invparm, 123invname,*/
                       /*        285dupname, 290toomanyhndls          */
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateMutexSem error: return code = %u\n", rc_indsem);
      return(FAILURE);
   }
#elif AIX325
   pthread_mutex_init(&indicateMutex, pthread_mutexattr_default);
   pIndList = NULL; /* initialize empty queue (queue is a linked list */
#else
   mutex_init(&indicateMutex, USYNC_THREAD, NULL);
   pIndList = NULL; /* initialize empty queue (queue is a linked list */
#endif

/************************************************************************/
/*  This is where we start a thread to handle the Indication calls.  We will let the  */
/*   IssueReg function register with the DMI our entry point for indication calls.    */
/*   which is dmi_indication_handler .                                                */
/***********************************************************************/

   pIndList = NULL;
#ifdef WIN32
   tidThread = _beginthread(waitForIndication, 8192, NULL);
   if (tidThread == (TID)-1) {
         rc_indthr = 1;   /* Thread creation failed.*/
   } else
         rc_indthr = 0;


#elif defined OS2
   tidThread = (ULONG) _beginthread(waitForIndication, NULL, 8192, NULL); /* $MED */
#else
#ifdef AIX325
   rc_indthr = pthread_create(&tidThread,
                              pthread_attr_default,
                              waitForIndication,      /* address of function */
                              NULL);
#else

   rc_indthr = thr_create(NULL, NULL, waitForIndication, NULL,
                          THR_DETACHED | THR_NEW_LWP, &tidThread);
#endif
   if (rc_indthr)
   {
      DMISA_TRACE_LOG1(LEVEL1,"DosCreateThread error: return code = %ld\n", rc_indthr);
      DMISA_TRACE_LOGBUF(LEVEL1);
      return(FAILURE);
   }
#endif

   DMISA_TRACE_LOG(LEVEL2," Thread successfully created.\n");
   return(SUCCESS);
}


/***********************************************************************/
/* Function:  dmiIndicateHandler()                                     */
/*                                                                     */
/* Description:                                                        */
/*   Receive DMI indication request.  Called by Service Layer.         */
/*   This routines run under the thread of the Service Layer.          */
/*   It Saves the the Indication block, post the indicate semaphore    */
/*   and gives control back to the SL.                                 */
/*   The routine waitForIndication is waiting for this semaphore       */
/*   and will handle the indication block.                             */
/*                                                                     */
/***********************************************************************/
void APIENTRY dmiIndicateHandler( void  *indBlock )
{
APIRET     rc_indsem;
IndNode   *pIndNode;

   dmiBlock_ptr = (DMI_Indicate_t  *)indBlock;                         /* set the buffer pointer */
   DMISA_TRACE_LOG(LEVEL4,"  dmi_indicate_handler:  Call from Service Layer.");
   DMISA_TRACE_LOG1(LEVEL4, "                               pointer = %p.", dmiBlock_ptr);

   if (dmiBlock_ptr) {
      /* Lock Indication semaphore                                    */
#ifdef WIN32
          rc_indsem = WaitForSingleObject(Ind3SemHandle, Ind3Timeout);

          if ((rc_indsem == WAIT_FAILED) || (rc_indsem == WAIT_TIMEOUT))
             rc_indsem = 1;
		  else
		     rc_indsem = 0;

      if (rc_indsem != 0) {  /* can get 0, 6invhndl, 95interrupt, 103toomanysemreq,*/
                         /*         105semownerdied, 640timeout       */
         DMISA_TRACE_LOG(LEVEL1,"Request Mutex error. <dmiIndicateHandler>");
         DMISA_TRACE_LOGBUF(LEVEL1);
         return;  /* So we lose this indication, sorry...             */
      }

#elif defined OS2
      rc_indsem = DosRequestMutexSem(Ind3SemHandle, Ind3Timeout);
      if (rc_indsem != 0) {  /* can get 0, 6invhndl, 95interrupt, 103toomanysemreq,*/
                         /*         105semownerdied, 640timeout       */
         DMISA_TRACE_LOG1(LEVEL1,"DosRequestMutexSem error: return code = %u.", rc_indsem);
         DMISA_TRACE_LOGBUF(LEVEL1);
         return;  /* So we lose this indication, sorry...             */
      }

      /* Query Indication semaphore                                   */
      rc_indsem = DosQueryEventSem(IndSemHandle, &ulPostCt );  /* do this within mutex*/
      if (!rc_indsem) {
         DMISA_TRACE_LOG2(LEVEL2,"  Semaphore INDICATION = %d,  Post count before posting = %d.\n",IndSemHandle,ulPostCt);
#else
         mutex_lock(&indicateMutex); /* wait for opportunity to add to queue */
#endif

         pIndNode = malloc(dmiBlock_ptr->iCmdLen+sizeof(PVOID));
         if (!pIndNode)
            DMISA_TRACE_LOG(LEVEL1, "Out of Memory, Cannot handle an Indication.\n");
         else
         {
            memcpy(&(pIndNode->IndBuff),dmiBlock_ptr,dmiBlock_ptr->iCmdLen); /* Copy the Indicate Block */
            pIndNode->pNext = NULL;
         }

         if (pIndList == NULL) /* queue is empty... */
            pIndList = pIndNode;
         else /* put entry at end of queue... */
         {
            IndNode *pIndTemp = pIndList;
            while (pIndTemp->pNext != NULL)
               pIndTemp = pIndTemp->pNext;
            pIndTemp->pNext = pIndNode;
         } /* put entry at end of queue */

         /* Post event semaphore                                   */
#ifdef WIN32
         rc_indsem = SetEvent(IndSemHandle);
         DMISA_TRACE_LOG(LEVEL5," SetEvent INDICATION. ");

         /* no handling for timeout yet in AIX, not sure what to do $MED */
         if ( rc_indsem == 0 ) {
               DMISA_TRACE_LOG(LEVEL1,"SetEvent INDICATION error. <dmiIndicateHandler>" );
               free (pIndNode);                     /* Free the InBlock Pointer */

         } else {
             #ifdef DMISA_TRACE
              if (rc_indsem != 0) {
                 DMISA_TRACE_LOG(LEVEL5, "  SetEvent INDICATION  successful. <dmiIndicateHandler>" );
              }
         #endif
         }

         if (!ReleaseMutex(Ind3SemHandle)) {
            DMISA_TRACE_LOG(LEVEL1,"ReleaseMutex error.");
            DMISA_TRACE_LOGBUF(LEVEL1);

         }


#elif defined OS2
         rc_indsem = DosPostEventSem(IndSemHandle);
         if (rc_indsem != 0 && rc_indsem != 299) {  /* rcsem = 0, 6invhandle, 298toomanyposts, 299alreadyposted*/
            DMISA_TRACE_LOG1(LEVEL1,"DosPostEventSem INDICATION error: return code = %u.", rc_indsem );
            free (pIndNode);                     /* Free the InBlock Pointer */
#ifdef DMISA_TRACE
         } else {
            if (rc_indsem == 0) {
               DMISA_TRACE_LOG(LEVEL5, "  DosPostEventSem INDICATION  successful." );
            }

            /* Query event semaphore                            */
            rc_indsem = DosQueryEventSem(IndSemHandle, &ulPostCt );
            DMISA_TRACE_LOG2(LEVEL5,"  Semaphore handle = %d, Post count after posting = %d.\n",IndSemHandle,ulPostCt);
#endif
         }
      } /* end if */
      rc_indsem = DosReleaseMutexSem(Ind3SemHandle);  /* Release Mutex*/
      if (rc_indsem != 0) {  /* can get 0, 6invhndl, 288notowner      */
         DMISA_TRACE_LOG1(LEVEL1,"DosReleaseMutexSem error: return code = %u.", rc_indsem);
         DMISA_TRACE_LOGBUF(LEVEL1);
      }
#else
      cond_broadcast(&indicateCond);
      mutex_unlock(&indicateMutex);
#endif
   } else {
      DMISA_TRACE_LOG(LEVEL1, "Indication was lost, as number of concurrent indications was exceeded.");
   }

   /* Return to the Service Layer                                     */
}
/***********************************************************************/
/*  This is where the indicate thread will exectute                    */
/*     We wait for ever for the event Semaphore to be set              */
/***********************************************************************/
/***********************************************************************/
/* Function:  waitForIndication()                                      */
/*                                                                     */
/* Description:                                                        */
/*   Function to await Service Layer call to handle an indication      */
/*   The function will loop forever with a wait on the the semaphore   */
/*   to be set by dmi_indicate_handler when the Service layer calls it.*/
/***********************************************************************/
#if defined(OS2) || defined(AIX325) || defined(WIN32)
void waitForIndication(void)
#else
void *waitForIndication(void *nevermind)
#endif
{
APIRET rc_indsem,rc_par,rc_sem;
IndNode *pIndNode;
fd_set   read_fds;
struct rlimit  rlp;
int dtbsize,permloop=1;


   getrlimit(RLIMIT_NOFILE, &rlp);
   dtbsize = (int) rlp.rlim_cur;
   
   /* Wait forever semaphore                                          */

   while (permloop)  { /* Loop forever  */

#ifdef WIN32
   (DWORD) rc_indsem = WaitForSingleObject(IndSemHandle, INFINITE);

   if (rc_indsem == WAIT_FAILED)
     rc_indsem = 1;
   else
     rc_indsem = 0;

#elif defined OS2
      rc_indsem = DosWaitEventSem(IndSemHandle, ulIndWaitTimeout = SEM_INDEFINITE_WAIT);
#else
/**************************************************************************/
#if 0
The old methodology of using indicateMutex,indicateCond as a serialization
technique between this thread and the Service Layer response is discontinued,
in order to integrate the RPC. Instead this thread will now listen on the
RPC socket handles and dispatch the service procedures for mi_callbacks.
Hence with the RPC implementation the indicateMutex , indicateCond are now
defunct. However in each of the MI service functions DmiAccessMutex 
should be locked/unlocked       ---JITEN 

      pthread_mutex_lock(&indicateMutex); /* $MED prevent premature adding to queue */
      if (!pIndList)
         do { /* wait for something to be put on the queue */
            rc_indsem = pthread_cond_wait(&indicateCond, &indicateMutex); /* no timeout code yet $MED */
         } while (pIndList == NULL);
      /* any need to check for thread termination? $MED */
      pthread_mutex_unlock(&indicateMutex);
#endif
/*************************************************************************/
/*THE RPC code handling starts here ..... */

#if 0
svc_run();
continue;
#endif

FD_ZERO(&read_fds);

read_fds = svc_fdset;

switch(select( dtbsize, &read_fds, NULL, NULL, NULL)) {
                 case -1:
                 case 0:
                                continue;

                 default:
                           mutex_lock(&DmiAccessMutex);
                           svc_getreqset(&read_fds);
                           mutex_unlock(&DmiAccessMutex);
                           break; 
}

#endif

#if 0
      if ( rc_indsem != 0 ) {
         DMISA_TRACE_LOG1(LEVEL1,"DosWaitEventSem error for INDICATION: return code = %u.", rc_indsem);
      } else { /* got mutex semaphore ok... */
         DMISA_TRACE_LOG(LEVEL5, "  DosWaitEventSem for INDICATION ended successfully.");

         pIndNode = pIndList;
         while (pIndNode) {
            pIndList = pIndNode->pNext;
            /* get dmi access semaphore */
#ifdef WIN32
          rc_sem = WaitForSingleObject(DmiAccessMutex, ulIndTimeout);

      if ((rc_sem != WAIT_FAILED) && (rc_sem != WAIT_TIMEOUT))
                rc_sem = 0;

#elif defined OS2
            rc_sem = DosRequestMutexSem(DmiAccessMutex, ulIndTimeout);
#else
            rc_sem = mutex_lock(&DmiAccessMutex); /*$MED no timeout */
#endif
            if (rc_sem != 0) {  /* can get 0, 6invhndl, 95interrupt, 103toomanysemreq,*/
                                /*         105semownerdied, 640timeout*/
               DMISA_TRACE_LOG1(LEVEL1,"Trap Routine DosRequestMutexSem error: return code = %u.", rc_sem);
               DMISA_TRACE_LOG(LEVEL1, " Indication lost .. semaphore TimeOut. \n");
            } else {                                                  /* we got the semaphore*/
               /* is cast on next line ok? $MED */
               rc_par =  parseDmiIndicate(&(pIndNode->IndBuff));     /* go handle the indicate*/
               if (rc_par != 0 ) {
                  DMISA_TRACE_LOG1(LEVEL2,"parse_dmi_indicate status = %d \n",  rc_par);
               }
            }
            free (pIndNode);                     /* Free the InBlock Pointer */
#ifdef WIN32
            if(!ReleaseMutex(DmiAccessMutex)){  /* Release Mutex*/
               DMISA_TRACE_LOG(LEVEL1,"Trap routine ReleaseMutex error.");
            }

#elif defined OS2
            rc_sem = DosReleaseMutexSem(DmiAccessMutex);  /* Release Mutex*/
            if (rc_sem != 0) {  /* can get 0, 6invhndl, 288notowner   */
               DMISA_TRACE_LOG1(LEVEL1,"Trap routine DosReleaseMutexSem error: return code = %u.", rc_sem);
            }
#else
            mutex_unlock(&DmiAccessMutex); /* Release Mutex */
            /*pthread_yield(); */ /* This is not a valid POSIX call */
#endif
            pIndNode = pIndList; /* point to next node in list */

         } /* while (pIndNode) */
#ifdef WIN32
         if (!ResetEvent(IndSemHandle)){  /* ulPostCt: number of posts since last reset*/
            DMISA_TRACE_LOG(LEVEL1,"ResetEvent error.  <waitForIndication>" );
         } else {
            DMISA_TRACE_LOG(LEVEL5," ResetEvent successful.  <waitForIndication>");
                 }
#elif defined OS2
         rc_sem = DosResetEventSem(IndSemHandle, &ulPostCt );  /* ulPostCt: number of posts since last reset*/
         if (rc_sem) {  /* rc = 0, 6invhandle, 300alreadyreset*/
            DMISA_TRACE_LOG2(LEVEL1,"DosResetEventSem error: return code = %u, count = %u.",
                    rc_sem, ulPostCt );
         }
         DMISA_TRACE_LOG1(LEVEL5," DosResetEventSem successful.  rc=%u.",rc_sem);
#endif
      } /* end if */
/*THIS PART IS NOW DEFUNCT FOR RPC --- When fully functional will be removed*/
#endif
     /* go back a wait again -- Program termination or semaphore error to end it  */
   } /* end while */
   return NULL;
}


DmiErrorStatus_t *
_dmideliverevent_0x1_svc(DmiDeliverEventIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_Event_SubsNotice(DMI_SpecTrap_Event, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmicomponentadded_0x1_svc(DmiComponentAddedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_ComponentAdded, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmicomponentdeleted_0x1_svc(DmiComponentDeletedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_ComponentDeleted, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmilanguageadded_0x1_svc(DmiLanguageAddedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_LanguageAdded, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmilanguagedeleted_0x1_svc(DmiLanguageDeletedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_LanguageDeleted, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmigroupadded_0x1_svc(DmiGroupAddedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_GroupAdded, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmigroupdeleted_0x1_svc(DmiGroupDeletedIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_CompLangGrpIndication(DMI_SpecTrap_GroupDeleted, (void *)argp);

	return (&result);
}

DmiErrorStatus_t *
_dmisubscriptionnotice_0x1_svc(DmiSubscriptionNoticeIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */
result = handle_Event_SubsNotice(DMI_SpecTrap_SubscriptionNotice, (void *)argp);

	return (&result);
}

