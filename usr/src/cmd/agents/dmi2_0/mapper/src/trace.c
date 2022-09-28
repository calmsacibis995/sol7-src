/* Copyright 08/16/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)trace.c	1.2 96/08/16 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  trace.c                                                    */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support program logging.        */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        traceInit(...)              */
/*                                        trace(...)                  */
/*                                        traceFlushBackground(...)   */
/*                                        traceFlush(...)             */
/*                                        traceWrite(...)             */
/*                                        traceStdout(...)            */
/*                                        traceLogStdout(...)         */
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
/*                                                                    */
/* End Copyright ******************************************************/
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  940509    KOBEL     New module                    */
/*                  950105    LAUBLI    Fixed multiple printf's       */
/*                  950323    LAUBLI    Prevent beginthread in exit   */
/*                                      routine                       */
/*                  950328    LAUBLI    Fix event semaphore           */
/*                                                                    */
/* End Change Log *****************************************************/
/**********************************************************************/
/* Includes                                                           */
/**********************************************************************/

#define INCL_DOS
#define INCL_DOSFILEMGR
#define INCL_DOSSEMAPHORES

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#ifdef OS2
#include <os2.h>
#endif
#include "dmisa.h"      /* was trace.h                                */

extern int TraceLevel;

#ifdef DMISA_TRACE

#define OPEN_FILE      0x01
#define REPLACE_FILE   0x02
#define CREATE_FILE    0x10
#define FILE_NORM      0x00
#define DASD_FLAG      0
#define INHERIT        0x80
#define WRITE_THRU     0
#define FAIL_FLAG      0
#define SHARE_FLAG     0x10
#define ACCESS_FLAG    0x02
#define FILE_SIZE      0L
#define FILE_ATTRIBUTE FILE_NORM
#define EABUF          0L
#define HF_STDOUT      1
#define PIPESIZE       10000
#define PASS           0

#if defined(OS2) || defined(WIN32)
static UCHAR  Log_Buffer[MAX_BUF_LINES][BUF_LINE_LENGTH];
#else
/* no log buffer for now $MED */
static UCHAR  Log_Buffer[BUF_LINE_LENGTH];
#endif
static UCHAR  Write_Buffer[MAX_BUF_LINES][BUF_LINE_LENGTH];
static int   Buf_Line_Count = 0;
static int   Write_Line_Count = 0;
#ifdef OS2
static HPIPE hpR,hpW;
static HFILE hfSave = -1;
static HFILE hfNew = HF_STDOUT;
#elif defined WIN32
static HPIPE hpR,hpW;
static HFILE hfSave;
#endif

static void traceFlushBackground(void);
static void traceWrite(void *pvoid);
static void traceStdout(void *pvoid);
static void traceLogStdout(UCHAR *);


/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceInit(...)                                             */
/*                                                                    */
/*  Description: This function initializes the trace logging          */
/*               routines. This function must be called before using  */
/*               the logging routines.                                */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:   none                                                     */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

void traceInit(void)
{
/* next 9 lines were commented out                                    */
#ifdef OS2
/* no file trace for now in aix $MED */
     if (!TracetoScreen && TraceLevel == LEVEL5) { /* Log screen      */
          DosDupHandle(HF_STDOUT,&hfSave);       /* dup stdout handle */
          DosCreatePipe(&hpR,&hpW,PIPESIZE);     /* create pipe       */
          DosDupHandle(hpW,&hfNew);           /* remap stdout to pipe */

          _beginthread(traceStdout,     /* start stdout trace thread */
                       NULL,
                       64000,
                       NULL);
     }

     DosCreateMutexSem(NULL,               /* mutex for write buffer */
                       &hmtxWritebuf,
                       0,
                       FALSE);
     DosCreateMutexSem(NULL,               /* mutex for log buffer   */
                       &hmtxLogbuf,
                       0,
                       FALSE);
     DosCreateEventSem(NULL,               /* event for write start  */
                       &hevWriteStart,
                       0,
                       FALSE);
#elif defined WIN32
/* no file trace for now in aix $MED */
     if (!TracetoScreen && TraceLevel == LEVEL5) { /* Log screen      */
         CreatePipe(&hpR,&hpW,(LPSECURITY_ATTRIBUTES) NULL, PIPESIZE);     /* create pipe       */
         hfSave = GetStdHandle(STD_OUTPUT_HANDLE);       /* dup stdout handle */
         SetStdHandle(STD_OUTPUT_HANDLE,hpW);           /* remap stdout to pipe */    

          _beginthread(traceStdout,
                         64000,
                         NULL);

     }

     hmtxWritebuf = CreateMutex((LPSECURITY_ATTRIBUTES) NULL, FALSE, (LPCTSTR) NULL);

     hmtxLogbuf = CreateMutex((LPSECURITY_ATTRIBUTES) NULL, FALSE, (LPCTSTR) NULL );

     hevWriteStart = CreateEvent((LPSECURITY_ATTRIBUTES) NULL, TRUE, FALSE, (LPCTSTR) NULL );
              
#endif

}

/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  trace(...)                                                 */
/*                                                                    */
/*  Description: This function provides the trace logging capability. */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:   info_trace_level : trace level for info passed in        */
/*                   info_ptr : pointer to the trace info             */
/*                                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

void trace(int info_trace_level, UCHAR *trace_info_ptr)
{
     time_t  time_stamp;
     UCHAR    *time_ptr;

#ifdef OS2
     DosRequestMutexSem(hmtxLogbuf,           /* get log buffer mutex */
                        SEM_INDEFINITE_WAIT);
#elif defined WIN32
     WaitForSingleObject(hmtxLogbuf, INFINITE);

#endif

     time(&time_stamp);                           /* get time stamp   */
     time_ptr = ctime(&time_stamp);
     *(time_ptr + 24) = '\0';                     /* remove line feed */

     if (Buf_Line_Count >= 0 && Buf_Line_Count < MAX_BUF_LINES) {
          if (info_trace_level <= TraceLevel) {
#if defined(OS2) || defined(WIN32)
               sprintf(Log_Buffer[Buf_Line_Count++],
                       "%s %s\n",time_ptr,trace_info_ptr);
               if (TracetoScreen)
                    printf("%s",Log_Buffer[Buf_Line_Count - 1]);
#else
               sprintf(Log_Buffer,
                       "%s %s\n",time_ptr,trace_info_ptr);
               if (TracetoScreen)
                    printf("%s",Log_Buffer);
#endif
          }                   /* Added brackets to if()  01/05/95.gwl */
     } else
          Buf_Line_Count = 0;            /* line count corrupt, reset */

#ifdef OS2
     if (Buf_Line_Count == MAX_BUF_LINES) {
        if (ExceptionProcFlag) {               /* added 950323.gwl */
           traceFlush();                       /* added 950323.gwl */
        } else {
           traceFlushBackground();               /* flush the buffer */
        }
     }
     DosReleaseMutexSem(hmtxLogbuf);      /* release log buffer mutex */

#elif defined WIN32
     if (Buf_Line_Count == MAX_BUF_LINES) {
        if (ExceptionProcFlag) {               /* added 950323.gwl */
           traceFlush();                       /* added 950323.gwl */
        } else {
           traceFlushBackground();               /* flush the buffer */
        }
     }
     ReleaseMutex(hmtxLogbuf);      /* release log buffer mutex */


#endif

     return;
}

/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceFlushBackground()                                     */
/*                                                                    */
/*  Description: This function flushes the trace buffer to a file     */
/*               utilizing a thread for the file I/O.                 */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:    none                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/
#ifdef OS2

void traceFlushBackground(void)
{
     int i;
     ULONG ulPostcnt;

     DosRequestMutexSem(hmtxWritebuf,SEM_INDEFINITE_WAIT);/* get mutex */
     for (i=0; i<Buf_Line_Count; i++)            /* copy log buffer to */
          strcpy(Write_Buffer[i],Log_Buffer[i]);  /* write buffer      */
     Write_Line_Count = Buf_Line_Count;
     DosReleaseMutexSem(hmtxWritebuf);

     DosResetEventSem(hevWriteStart,&ulPostcnt); /* make sure cleared 950328.gwl*/
     _beginthread(traceWrite,               /* copy write buffer to    */
                  NULL,                     /* trace file using thread */
                  64000,
                  NULL);
                                            /* initialize buffer       */
     Buf_Line_Count = 0;

     DosWaitEventSem(hevWriteStart,     /* wait for trace_write thread */
                     SEM_INDEFINITE_WAIT); /* to get mutex             */
     DosResetEventSem(hevWriteStart,&ulPostcnt); /* just to be clean 950328.gwl */

     return;
}

#elif defined WIN32
void traceFlushBackground(void)
{
     int i;

     WaitForSingleObject(hmtxWritebuf, INFINITE);

     for (i=0; i<Buf_Line_Count; i++)            /* copy log buffer to */
          strcpy(Write_Buffer[i],Log_Buffer[i]);  /* write buffer      */
     Write_Line_Count = Buf_Line_Count;
     ReleaseMutex(hmtxWritebuf);

     ResetEvent(hevWriteStart); /* make sure cleared 950328.gwl*/
     _beginthread(traceWrite,
                  64000,
                  NULL);

                                            /* initialize buffer       */
     Buf_Line_Count = 0;

     WaitForSingleObject(hevWriteStart, INFINITE);

     ResetEvent(hevWriteStart); /* just to be clean 950328.gwl */

	 return;

}
#endif

/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceFlush()                                               */
/*                                                                    */
/*  Description: This function flushes the trace buffer to a file     */
/*               waiting for the file I/O to complete.                */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:    none                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

void APIENTRY traceFlush(void)
{
#ifdef OS2
     int   i;
     void *pvoid = NULL;  /* Initialized to clean up compilation      */

     DosRequestMutexSem(hmtxLogbuf,SEM_INDEFINITE_WAIT); /* get mutex */
     DosRequestMutexSem(hmtxWritebuf,SEM_INDEFINITE_WAIT);
     for (i=0; i<Buf_Line_Count; i++)           /* copy log buffer to */
          strcpy(Write_Buffer[i],Log_Buffer[i]); /* write buffer      */
     Write_Line_Count = Buf_Line_Count;
     DosReleaseMutexSem(hmtxLogbuf);             /* release mutex     */
     DosReleaseMutexSem(hmtxWritebuf);

     traceWrite(pvoid);                      /* copy write buffer to */
                                             /* trace file           */
     Buf_Line_Count = 0;                  /* initialize buffer count */

#elif defined WIN32
     int   i;
     void *pvoid = NULL;  /* Initialized to clean up compilation      */

     WaitForSingleObject(hmtxLogbuf, INFINITE);
     WaitForSingleObject(hmtxWritebuf, INFINITE);
     for (i=0; i<Buf_Line_Count; i++)           /* copy log buffer to */
          strcpy(Write_Buffer[i],Log_Buffer[i]); /* write buffer      */
     Write_Line_Count = Buf_Line_Count;
     ReleaseMutex(hmtxLogbuf);             /* release mutex     */
     ReleaseMutex(hmtxWritebuf);

     traceWrite(pvoid);                      /* copy write buffer to */
                                             /* trace file           */
     Buf_Line_Count = 0;                  /* initialize buffer count */

#endif
     return;
}

#ifdef OS2
/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceWrite()                                               */
/*                                                                    */
/*  Description: This function writes trace data to the log file.     */
/*               When the log file exceeds a certain size, it will    */
/*               copied to a backup file. All new data is always      */
/*               written to the main log file.                        */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:    none                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

static void traceWrite(void *pvoid)
{
     int         i;
     HFILE       hfile;
     APIRET      rc;
     ULONG       action;
     ULONG       info_buf_size;
     ULONG       bytes_written;
     ULONG       loc;
     FILESTATUS3 info_buf;

     DosRequestMutexSem(hmtxWritebuf,SEM_INDEFINITE_WAIT);
     DosPostEventSem(hevWriteStart);    /* notify other threads that */

     if (strlen(TraceFileMain) == 0) /* check trace file name, if no */
          strcpy(TraceFileMain,DEFAULT_MAIN_LOG); /* name,use default*/
     if (strlen(TraceFileOverflow) == 0)
          strcpy(TraceFileOverflow,DEFAULT_OVERFLOW_LOG);

     info_buf_size = sizeof(FILESTATUS3);

     rc = DosOpen(TraceFileMain,                    /* open trace file */
                  &hfile,
                  &action,
                  FILE_SIZE,
                  FILE_ATTRIBUTE,
                  OPEN_FILE | CREATE_FILE,
                  DASD_FLAG | INHERIT |
                  WRITE_THRU | FAIL_FLAG |
                  SHARE_FLAG | ACCESS_FLAG,
                  EABUF);

     if (rc != PASS) {
          DosReleaseMutexSem(hmtxWritebuf);
          return;
     }

     rc = DosQueryFileInfo(hfile,                     /* get file size */
                           1,
                           &info_buf,
                           info_buf_size);

     if (rc != PASS) {
          DosClose(hfile);
          DosReleaseMutexSem(hmtxWritebuf);
          return;
     }


     if (info_buf.cbFile > MAX_LOG_FILE_SIZE) {   /* compare file size */
          DosClose(hfile);
          DosCopy(TraceFileMain,         /* copy main file to overflow */
                  TraceFileOverflow,
                  DCPY_EXISTING);
          rc = DosOpen(TraceFileMain,              /* reopen main file */
                       &hfile,                     /* overwriting      */
                       &action,
                       FILE_SIZE,
                       FILE_ATTRIBUTE,
                       REPLACE_FILE | CREATE_FILE,
                       DASD_FLAG | INHERIT |
                       WRITE_THRU | FAIL_FLAG |
                       SHARE_FLAG | ACCESS_FLAG,
                       EABUF);
          if (rc != PASS) {
               DosReleaseMutexSem(hmtxWritebuf);
               return;
          }
     }

     DosSetFilePtr(hfile,          /* move file pointer to end of file */
                   0,
                   FILE_END,
                   &loc);

     for (i=0; i < Write_Line_Count; i++) {
          DosWrite(hfile,                      /* write buffer to file */
                   (PVOID) Write_Buffer[i],
                   strlen(Write_Buffer[i]),
                   &bytes_written);
     }

     DosClose(hfile);
     DosReleaseMutexSem(hmtxWritebuf);         /* release write buffer */

     return;
}

/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceLogStdout()                                           */
/*                                                                    */
/*  Description: This function parses a stdout string and passes      */
/*               each line to the trace(...) function where it is     */
/*               place into the log file.                             */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:   bufin : pointer to buffer containg stdout data           */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

static void traceLogStdout(UCHAR *bufin)
{
     UCHAR  logbuf1[132];
     UCHAR *token;
     ULONG  len;

/* this function was commented out                                    */
       token = strtok(bufin,"\r\n");

       while (token != NULL) {
            len = (strlen(token) < LOG_BUF_LEN) ? strlen(token) : LOG_BUF_LEN;
                                                /* Added 12/16/94.gwl   */

            strncpy(logbuf1, token, len);       /* Added 12/16/94.gwl   */
            logbuf1[len] = '\0';                /* Added 12/16/94.gwl   */
            trace(LEVEL3,logbuf1);              /* Changed 12/16/94.gwl */
            token = strtok(NULL,"\r\n");    /* remove \r\n from string  */
       }

     return;
}
#elif defined WIN32
/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceWrite()                                               */
/*                                                                    */
/*  Description: This function writes trace data to the log file.     */
/*               When the log file exceeds a certain size, it will    */
/*               copied to a backup file. All new data is always      */
/*               written to the main log file.                        */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:    none                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

static void traceWrite(void *pvoid)
{
     int         i;
     HANDLE      hfile;
     DWORD       rc;
	 ULONG       filesize;
     DWORD       bytes_written;
	 BY_HANDLE_FILE_INFORMATION info_buf;


     WaitForSingleObject(hmtxWritebuf,INFINITE);

     SetEvent(hevWriteStart);    /* notify other threads that */

     if (strlen(TraceFileMain) == 0) /* check trace file name, if no */
          strcpy(TraceFileMain,DEFAULT_MAIN_LOG); /* name,use default*/
     if (strlen(TraceFileOverflow) == 0)
          strcpy(TraceFileOverflow,DEFAULT_OVERFLOW_LOG);


     hfile = CreateFile(TraceFileMain,                    /* open trace file */
				  GENERIC_WRITE,
				  FILE_SHARE_READ,
				  NULL,
				  OPEN_ALWAYS,
				  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				  NULL);

	 rc = GetLastError();
     if ((rc != 0) && (rc != ERROR_ALREADY_EXISTS)) {
          ReleaseMutex(hmtxWritebuf);
          return;
     }

     if (!GetFileInformationByHandle(hfile, &info_buf)) {
          CloseHandle(hfile);
          ReleaseMutex(hmtxWritebuf);
          return;
     }

	 filesize = MAKELONG(info_buf.nFileSizeLow, info_buf.nFileSizeHigh);

     if (filesize > MAX_LOG_FILE_SIZE) {   /* compare file size */
          CloseHandle(hfile);
          CopyFile(TraceFileMain,         /* copy main file to overflow */
                   TraceFileOverflow,
                   FALSE);

        hfile = CreateFile(TraceFileMain,                    /* open trace file */
            			   GENERIC_WRITE,
				  		   FILE_SHARE_READ,
				  		   NULL,
				  		   OPEN_ALWAYS,
				  		   FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
				  		   NULL);

	 	rc = GetLastError();
        if ((rc != 0) && (rc != ERROR_ALREADY_EXISTS)) {
             ReleaseMutex(hmtxWritebuf);
             return;
        }
     }

     SetFilePointer(hfile,          /* move file pointer to end of file */
                    0,
			    	(PLONG) NULL,
                    FILE_END);

     for (i=0; i < Write_Line_Count; i++) {
          WriteFile(hfile,                      /* write buffer to file */
                    Write_Buffer[i],
                    strlen(Write_Buffer[i]),
                    &bytes_written,
                    NULL);
     }

     CloseHandle(hfile);
     ReleaseMutex(hmtxWritebuf);         /* release write buffer */

     return;
}

/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceLogStdout()                                           */
/*                                                                    */
/*  Description: This function parses a stdout string and passes      */
/*               each line to the trace(...) function where it is     */
/*               place into the log file.                             */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:   bufin : pointer to buffer containg stdout data           */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

static void traceLogStdout(UCHAR *bufin)
{
     UCHAR  logbuf1[132];
     UCHAR *token;
     ULONG  len;

/* this function was commented out                                    */
       token = strtok(bufin,"\r\n");

       while (token != NULL) {
            len = (strlen(token) < LOG_BUF_LEN) ? strlen(token) : LOG_BUF_LEN;
                                                /* Added 12/16/94.gwl   */

            strncpy(logbuf1, token, len);       /* Added 12/16/94.gwl   */
            logbuf1[len] = '\0';                /* Added 12/16/94.gwl   */
            trace(LEVEL3,logbuf1);              /* Changed 12/16/94.gwl */
            token = strtok(NULL,"\r\n");    /* remove \r\n from string  */
       }

     return;
}


#endif


/* Function Specification *********************************************/
/*                                                                    */
/*  Name:  traceStdout()                                              */
/*                                                                    */
/*  Description: This function listens on one end of the remapped     */
/*               stdout pipe and calls functions to log the data.     */
/*                                                                    */
/*  Dependencies:  None                                               */
/*                                                                    */
/*  Input:    none                                                    */
/*  Output:  none                                                     */
/*                                                                    */
/*  Exit Normal:  Return to caller                                    */
/*                                                                    */
/*  Exit Error:   Return to caller                                    */
/*                                                                    */
/* End Function Specification *****************************************/

#ifdef OS2
/* no file trace for now in aix $MED */
static void traceStdout(void *pvoid)
{
     UCHAR buf[PIPESIZE];
     ULONG cbread;

     do {
          DosRead(hpR,buf,sizeof(buf),&cbread);
          buf[(int) cbread - 1] = '\0';             /* NULL terminate */
          traceLogStdout(buf);    /* format & log trace to log file */
     }
     while(cbread);

     return;
}
#elif defined WIN32 
static void traceStdout(void *pvoid)
{
     UCHAR buf[PIPESIZE];
     ULONG cbread;

     do {
          ReadFile(hpR,buf,sizeof(buf),&cbread, NULL);
          buf[(int) cbread - 1] = '\0';             /* NULL terminate */
          traceLogStdout(buf);    /* format & log trace to log file */
     }
     while(cbread);

     return;
}

#endif
#endif

void tracem(int info_trace_level, char *trace_info_ptr)
{
     time_t  time_stamp;
     UCHAR    *time_ptr;

     if ((TraceLevel == 0) && (info_trace_level == LEVEL1))
               syslog(LOG_ERR, trace_info_ptr);
     else {
            if (info_trace_level <= TraceLevel)
               printf("%s\n",trace_info_ptr);
     }
     return;
}
