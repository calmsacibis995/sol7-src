/* Copyright 08/20/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_lib.h	1.2 96/08/20 Sun Microsystems"

/********************************************************************************************************

    Filename: os_lib.h


    Description: Include Information for all the porting concerns

*********************************************************************************************************/

#ifndef LIB_H_FILE
#define LIB_H_FILE

#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */

#include <stdlib.h>
#include "stdio.h"
#include "string.h"
#include "setjmp.h"
#include "error.h"

#include <errno.h>
#include <thread.h>

#include "os_sys.h"     /* pull in the trace vector information              */
#include "os_skt.h"

#include "os_shm.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include <fcntl.h>
#include <sys/mode.h>

#include "dmisl_msg.h"
#include "os_default_msg.h"

/* ******************************** defines ****************************************************** */
/*#define MEM_DEBUG*/    /* this is here so everybody who needs this recompiles */
/* memory macro */
/*#define memcat(string) ((string)##" at line "##__LINE__##" of "##__FILE__)*/
#define memcat(string) memdiag(string, __LINE__, __FILE__)

#define PKTID         SHMSEMID + 1
#define DISEMID       PKTID + 1
#define DMISL_MSG_CAT "dmisl.cat"

/* ********************************* Structure definitions **************************************** */
typedef struct _critSecInfo {
   thread_t   *threadId;          /* which thread this belongs to */
/* int         priority;*/         /* old priority */
   int         counter;            /* priority counter */
/* struct      _critSecInfo *prev;*/ /* previous one */
   struct      _critSecInfo *next; /* next one */
   } CritSecInfo;

/*CritSecInfo *critSecBase = NULL; */

/* *********************************** Function Prototypes ***************************************** */
/*int os_initLib(int type);
int os_termLib(int type);*/
int os_initLib(); 
int os_termLib(); 
#ifdef MEM_DEBUG
void *debug_Alloc(ULONG Size, char *filename, int linenum);
void debug_Free(void *Buffer, char *filename, int linenum);
#define OS_Alloc(x) debug_Alloc((x), __FILE__, __LINE__)
#define OS_Free(x) debug_Free((x), __FILE__, __LINE__)
#else
void *OS_Alloc(ULONG);    /* base Service layer memory management routines */
void OS_Free(void *);
#endif
void OS_EnterCritSec(void);
void OS_ExitCritSec(void);
char *os_itos(long);
/*void os_critSecInit();*/
int  os_pushPrio(thread_t thisThread);
int  os_popPrio(thread_t thisThread);
void checkCritSec();
int os_errorMessage(char *where, int errorNumber);
char *memdiag(char *whoItIs, int line, char *file);
void setupSignalMask();
void addSignalToMask(int signalName, sigset_t *signalMask);
int setupDISem(int type);
int lockDISem();
int unlockDISem();
void openMsgCatalog();
void closeMsgCatalog();
char *msgDmi(int msgnum, char *defaultMsg);
char *msgComp(int msgnum, char *defaultMsg);
char *msgApi(int msgnum, char *defaultMsg);
char *dmiMsg(int msgset, int msgnum, char *defaultMsg);

#endif
