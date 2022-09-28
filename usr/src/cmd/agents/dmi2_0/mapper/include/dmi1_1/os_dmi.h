/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_dmi.h	1.1 96/08/05 Sun Microsystems"

/*    Filename: OS_DMI.H                                                     */


/*    Description: Include file for using the OS/2 DMTF Service Layer        */

/*    Author(s): Paul A. Ruocchio                                            */

#ifndef OS_DMI_H_FILE
#define OS_DMI_H_FILE

/**************************************  INCLUDES   ********************************************** */

#define _FAR
#include "os_default_msg.h"
#define OS_SL_VERSION OS_SL_VER_D
#define SL_ENVIRONMENT_ID MIF_UNIX_ENVIRONMENT    /* ID of this service layer environment */
#define PR_TARGET_OS "AIX"                       /* target OS used in the parser functions */
#define DMI_FUNC_ENTRY                           /* define the system calling conventions */
#define DMI_FUNC_CALLBACK

#define MIF_DIRECTORY  "/home/yiru/sva/mif/"
#define BACKUP_DIRECTORY  "/home/yiru/sva/backup/"
#define SL_MIF "namedir.mif"

#pragma pack(2)     /* this will keep the old DOS code happy                 */
#include "dmi.h"     /* the Management Interface primitives                  */
#include "error.h"  /* include the service layer error file                  */
#pragma pack()      /* return to default allignment                          */

/************** some typedefs to make AIX happy *************/
typedef unsigned long  int ULONG;
typedef unsigned short int USHORT;
typedef void               VOID;
#ifdef AIX_SL_BUILD
typedef int                BOOL;
#endif
typedef int                boolean; 
/*typedef unsigned char      BYTE;*/   /* $MED */
typedef void              *PVOID;  /* $MED */
typedef unsigned char      UCHAR;  /* $MED */
typedef unsigned long      APIRET; /* $MED */
typedef short              SHORT;  /*$MED  */
typedef long int           LONG;   /*$MED  */
/**************************************  PROTOTYPES ********************************************** */


/* Entry points that are unique to the AIX Service Layer implementation     */
#if defined(CPLUSPLUS) || defined(__cplusplus)
extern "C"
#endif
DMI_UNSIGNED DMI_FUNC_ENTRY sDmiInvoke(DMI_MgmtCommand_t *dmiCommand);   /* syncronout invoke entry point */
int DmiMainLoop();

#endif
