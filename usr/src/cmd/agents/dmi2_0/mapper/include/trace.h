/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)trace.h	1.1 96/08/05 Sun Microsystems"

#ifndef traceH
#define traceH

/***********************************************************************/
/*   IBM Confidential                                                  */
/*   (IBM Confidential-Restricted when aggregated                      */
/*    with all OCO source modules of this product)                     */
/*                                                                     */
/*   COPYRIGHT:  (C) IBM Corporation 1994, Property of IBM.            */
/*                                                                     */
/*   Header file for trace.c                                           */
/*                                                                     */
/***********************************************************************/
/*                                                                     */
/* Change Log **********************************************************/
/*                                                                     */
/*  Flag  Reason    Date      Userid    Description                    */
/*  ----  --------  --------  --------  -----------                    */
/*                  940509    KOBEL     New header                     */
/*                                                                     */
/* End Change Log ******************************************************/
/*                                                                     */
/***********************************************************************/
/* USAGE NOTES                                                         */
/* ===========                                                         */
/*      1. Include trace.h in all source modules that reference        */
/*         trace functions.                                            */
/*      2. Set trace variables:                                        */
/*                      TraceLevel    = LEVEL5;                        */
/*                      TracetoScreen = FALSE;                         */
/*                      strcpy(TraceFileMain,"xyzMAIN.LOG");           */
/*                      strcpy(TraceFileOverflow,"xyzOVER.LOG");       */
/*      3. Call traceInit().                                           */
/*      4. Use trace(...) within program to log trace data:            */
/*                   trace(LEVEL2,"Trace data");                       */
/*                                or                                   */
/*                   sprintf(buf,"Trace Level set to %d.",TraceLevel); */
/*                   trace(LEVEL1,buf);                                */
/*           NOTE: Special care must be taken to ensure the entire     */
/*                 string fits in the buffer; otherwise, sprintf will  */
/*                 overwrite other portions of your memory!  -GWL      */
/*      5. Make sure traceFlush(...) is called before your             */
/*         program terminates so that the buffer is saved to the       */
/*         trace file.                                                 */
/*                                                                     */
/***********************************************************************/


/***********************************************************************/
/* TRACE CONSTANTS                                                     */
/***********************************************************************/

#define MAX_BUF_LINES        1000          /* size of trace buffer     */
#define BUF_LINE_LENGTH      132           /* max length of line entry */
#define MAX_LOG_FILE_SIZE    500000        /* max log file size (bytes)*/
#define DEFAULT_MAIN_LOG     "MAIN.LOG"    /* default main log name    */
#define DEFAULT_OVERFLOW_LOG "OVERFLOW.LOG"/* default main log name    */

/* available trace levels   */
#define LEVEL0  0              /* trace off                            */
#define LEVEL1  1
#define LEVEL2  2
#define LEVEL3  3
#define LEVEL4  4
#define LEVEL5  5             /* most detailed (includes screen trace) */

#ifdef DMISA_TRACE
/***********************************************************************/
/* TRACE VARIABLES                                                     */
/* These variables must be set BEFORE calling trace_init(...)          */
/***********************************************************************/

int  TraceLevel;                      /* current trace level           */
int  TracetoScreen;                   /* dump trace to screen          */
unsigned char TraceFileMain[15];      /* Main trace file               */
unsigned char TraceFileOverflow[15];  /* Overflow trace file           */

/***********************************************************************/
/* TRACE FUNCTION PROTOTYPES                                           */
/***********************************************************************/

void traceInit(void);                     /* initialize trace function */
void trace(int, unsigned char *);         /* trace function            */
void APIENTRY traceFlush(void);           /* flush trace buffer        */
#endif
#endif /* traceH */

