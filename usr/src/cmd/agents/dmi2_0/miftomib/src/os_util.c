/* Copyright 09/30/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_util.c	1.5 96/09/30 Sun Microsystems"


/* ************************************************************************************************** */
/*                                 OS_SVC.C                                                           */
/*                                                                                                    */
/*     Copyright (c) International Business Machines Corp., 1993-94                                   */
/*                                                                                                    */
/*     Description:  This file is a template for all of the OS specific function required to build    */
/*                   a working DMI service layer based on the PSL code.                               */
/*                                                                                                    */
/* ************************************************************************************************** */

#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mode.h>
#include "psl_mh.h"
#include "os_sys.h"
#include "os_lib.h"
#include "os_shm.h"
/*#include <sys/ldr.h>*/

/* ******************************** defines ****************************************************** */

/* ********************************* Structure definitions **************************************** */

/* *********************************** Function Prototypes ***************************************** */

/* *********************************** Globals ***************************************************** */

extern MH_Registry_t *registry;   /* included ONLY if you need access to the App registry table in the SL */
extern CI_Registry_t *direct;     /* included ONLY if you need access to the CI registry table in the SL */

extern int acceptSocket;

extern char Environment[];    /* the directory information from the environment variable */
nl_catd msgCatalog;

/* NOTE: this is really messy, but it is to map from the PSL error codes     */
/*       to the ones required by the AIX Message stuff.                      */
#define ERROR_COUNT 124

static ULONG ErrorArray[ERROR_COUNT] = {
              1530,1643,1703,2101,2533,2641,2691,2741,3105,3106,3201,3202,3203,3204,
              3301,3306,3307,3401,3417,3503,3509,3510,3511,3512,3513,3514,3515,3518,
              3519,3520,3522,3523,3524,3525,3528,3529,3537,3553,3559,3562,3563,3565,
              3569,3571,3578,3579,3580,3603,3608,3609,3611,3612,3613,3615,3616,3626,
              3629,3653,3659,3661,3662,3663,3664,3665,3668,3669,3670,3671,3674,3678,
              3679,3684,3685,3686,3689,3694,3695,3696,3703,3709,3710,3711,3712,3713,
              3714,3715,3716,3727,3728,3729,3738,3753,3760,3762,3774,3778,3803,3809,
              3811,3812,3813,3815,3817,3819,3821,3824,3828,3829,3840,3878,3892,4406,
              4531,4532,3581,4631,4681,4731,4732,4781,4782,4831,4931,4900
};

/******************************** OS FUNCTION *********************************************/
void OS_GetParserMessage(PR_ErrorNumber_t errorNumber,
                         unsigned long line,
                         unsigned long col,
                         char *buffer,
                         unsigned long length)
{

/*
    This function is called by the install code when an error/wraning/info message is
    required.  By providing this function here, any translation can be handled in a
    manner that is native to the OS environment that you are running in.
    When called, a message corresponding to the errorNumber passed in should be
    formatted into the provided buffer.

    errorNumber - the Errorcode
    line        - The line in the MIF file that generated this error
    col         - The column on that line that generated this error
    buffer      - The buffer to write the message into
    length      - The length of the provided buffer
*/
unsigned long msglen;
unsigned long RC,i;
char *temp;

    for(i = 0;i != ERROR_COUNT;i++) if(ErrorArray[i] == errorNumber) break;
    i++;   /* array starts at 0, message catalog starts at 1 */
    temp = msgComp(i, compDefault[i-1]);
    msglen = strlen(temp);

    if ( (int)msgCatalog == -1 || (int)(msgCatalog -> __content) == -1 )
        strcpy(buffer, "DMI9999: Unable to open message catalog dmisl.cat.");
    else if ( msglen == 0 )
        sprintf(buffer, "DMI9998: Requested message %i not found.", errorNumber);
    else{
        sprintf(buffer, temp, line, col);
        if ( msglen < length - 1 )
           strcat(buffer, ".");
    }
}


/******************************** OS FUNCTION *********************************************/
void OS_getTime(DMI_TimeStamp_t _FAR *ts)  /* called to build up the time-stamp used in events */
{
   struct timeval Tp;
   struct timezone Tzp;
   struct tm *tmTime,
             curTime;

/*
    This function is responsible for getting the system time and date information, and
    formatting it into the DMI_TimeStamp_t buffer that is provided.
*/
   gettimeofday(&Tp, &Tzp);

   tmTime = (struct tm*)localtime_r((long *)&(Tp.tv_sec), &curTime);
   memset((void *)ts,0,sizeof(DMI_TimeStamp_t));
   sprintf((char *)ts, "%04d%02d%02d%02d%02d%02d.%06d%04d",
               tmTime -> tm_year + 1900,
               tmTime -> tm_mon + 1,
               tmTime -> tm_mday,
               tmTime -> tm_hour,
               tmTime -> tm_min,
               tmTime -> tm_sec,
               Tp.tv_usec,
               Tzp.tz_minuteswest / 60);
   if ( Tzp.tz_minuteswest > 0 ) ts -> cPlusOrMinus = '+';
   else                          ts -> cPlusOrMinus = '-';
}


/******************************** OS FUNCTION *********************************************/
void OS_InstallNotice(char *Message,void *OS_Context,DMI_MgmtCommand_t *dmiCommand)   /* ask the OS code to tell the user about this error */
{
   printf("%s", Message);
}

/*******************************************************************************/
char *msgComp(int msgnum, char *defaultMsg)
{
return(dmiMsg(COMPONENT, msgnum, defaultMsg));
}
/*******************************************************************************/

char *dmiMsg(int msgset, int msgnum, char *defaultMsg)
{
   if ( !msgCatalog )
      msgCatalog = catopen(MF_DMISL, 0);
   if ( (int)msgCatalog == -1 )
      msgCatalog = catopen(MF_DMISL, 0);
   return(catgets(msgCatalog, msgset, msgnum, defaultMsg));
}

void *OS_Alloc(ULONG Size)
{
   return(malloc(Size));
}

void OS_Free(void *Buffer)
{
   free( Buffer );
}
