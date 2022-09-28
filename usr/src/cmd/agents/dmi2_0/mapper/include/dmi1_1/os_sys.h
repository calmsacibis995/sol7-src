/* Copyright 08/20/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_sys.h	1.2 96/08/20 Sun Microsystems"

/*****************************************************************************
 *                                                                           *
 *  File Name: os_sys.h                                                      *
 *                                                                           *
 *  Description:  This file contains the definitions of data and macros for  *
 *                the DMI SL to use the OS/2 System Trace functions.         *
 *                                                                           *
 *  Table of Contents:                                                       *
 *                                                                           *
 *  Notes:                                                                   *
 *                                                                           *
 *****************************************************************************/

#ifndef os_sys_h
#define os_sys_h

#include <sys/select.h>

#define SOCK_SIG 102030
#define SOCK_DEL 987654

#define MEM_DONE_SL 0x001        /* Tasker finished with packet */
#define MEM_DONE_DI 0x010        /* DI component done with packet */
#define MEM_DONE_MA 0x100        /* Management application done */
#define MEM_DONE_ALL 0x111       /* Everybody's done */
#define MEM_NOTDONE_SL 0x110        /* Tasker finished with packet */
#define MEM_NOTDONE_DI 0x101        /* DI component done with packet */
#define MEM_NOTDONE_MA 0x011        /* Management application done */

#define MEM_PACKET_DELETED 0xEEE

struct timeval *_timeOut;             /* select timeout value */

#define APP_INITIALIZE  0x0001    /* the process API dll is being initialized */
#define APP_TERMINATE   0x0002    /* the process API dll has terminated      */
#define REQUEST         0x0003    /* identifies the packet as a request      */
#define REQUEST_SYNC    0x0004    /* identifies the packet as a SYNCRONOUS request  (OS/2 unique) */
#define INDICATE        0x0005    /* identifies the packet as an indication  */
#define DIRECT_CANCEL   0x0010    /* direct interface component cancel call  */
#define DIRECT_CALL     0x0020    /* direct interface component call         */
#define INSTALL_STATUS  0x0009    /* installation status information         */
#define TYPE_NORMAL     0x000f    /* mask for the normal requests            */
#define TYPE_DIRECT     0x00f0    /* mask for direct interface commands      */

typedef struct {
    ULONG             Size;            /* number of bytes in this block             */
    ULONG             Sig;             /* signature, used to verify that this is a block we are interested in */
    ULONG             Type;            /* REQUEST, INDICATE, ....                   */
    ULONG             Flags;           /* flags used to control this memory block   */
    mutex_t   *SyncSem;        /* semaphore used for the syncronous call API */
    cond_t    *SyncCond;       /* condition used for the syncronous call API */
    int               Requestor;       /* Process ID of the requestor, used to create the correct pipe for the confirm */
    ULONG             ReqReturnVal;    /* the return value of this request packet   */
    ULONG             DIReturnVal;     /* return value for a direct interface component */
    DMI_FUNC3_OUT     pConfirmFunc;    /* Confirm fucntion ptr, passed back on every request call */
    DMI_FUNC3_OUT     pIndicationFunc; /* Indication function ptr, passed back on every indication callback */
    DMI_FUNC_OUT      pDirectFunc;     /* the direct call entry point              */
    DMI_MgmtCommand_t *OrgDmiCommand;  /* ptr to the original command        */
    void              *OrgCnfBuffer;   /* ptr to the original confirm buffer       */
    DMI_Confirm_t     DirectCnf;       /* direct callback confirm information */
} DataStr, *DataPtr;

typedef DMI_UNSIGNED DMI_FUNC_ENTRY (*pEP)(void *);
typedef int (*pIntFunc)();

typedef struct _Overlay{            /* structure used to hold the OS specific overlay information -- DLL */
    pEP             DmiCiEntryPoint; /* the handle of this DLL */
    DMI_FUNC_OUT    CiInvoke;       /* the entry point of the invoke function    */
    DMI_FUNC0_OUT   CiCancel;       /* the entry point of the cancle function    */
} DllStr, *DllPtr;

/* API and DMISTRUCT are used to check the contents of the messages being passed
   around.  mostly these are here for debug */
typedef struct {
    DMI_MgmtCommand_t NewCmd;   /* point to the dmi_command */
    char              cnfBuf[1];  /* point to the new conf buffer */
    } DMISTRUCT;

typedef struct {
    DataStr           New;
    DMISTRUCT         NewCmd;   /* point to the dmi_command */
    } APISTRUCT;
#endif
