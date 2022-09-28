/* Copyright 08/20/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_shm.h	1.2 96/08/20 Sun Microsystems"

/*****************************************************************************
 *                                                                           *
 *  File Name: os_shm.h                                                      *
 *                                                                           *
 *  Description:  This file contains the definitions of shared memory        *
 *                functions for the SL and API code.                         *
 *                                                                           *
 *  Table of Contents:                                                       *
 *                                                                           *
 *  Notes:                                                                   *
 *                                                                           *
 *****************************************************************************/

#ifndef os_shm_h
#define os_shm_h

#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */

#include <stdlib.h>
#include "stdio.h"
#include "string.h"
#include "setjmp.h"
#include "error.h"

#include <errno.h>
#include <thread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#include "os_sys.h"     /* pull in the trace vector information              */
#include "os_lib.h"

/*#define PACKET_DEBUG*/

#define SHMSEGID '6'
#define SHMSEMID SHMSEGID + 1

#define SHMSERVER 1
#define SHMCLIENT 2

#define SHMSIG 630617
#define SHMDEL 811020
#define SHMUPD 611220
#define SHMINI 791206

/*#define SHMMAXSIZE 256*1024*1024*/ /* 256Meg */
#define SHMMAXSIZE 1024*256

void *os_shmAlloc(ULONG);    /* base Service layer memory management routines */
void os_shmFree(void *);
int  getSharedSegment(int type);
void releaseSharedSegment(int type);
#ifdef PACKET_DEBUG
DataPtr debug_PacketAlloc(ULONG Size, ULONG Type, char *filename, int linenum);
void debug_PacketFree(void *Buffer, int Flags, char *filename, int linenum);
#define OS_PacketAlloc(x, y) debug_PacketAlloc((x), (y), __FILE__, __LINE__)
#define OS_PacketFree(x, y) debug_PacketFree((x), (y), __FILE__, __LINE__)
#else
DataPtr OS_PacketAlloc(ULONG Size, ULONG Type);
void OS_PacketFree(void *Buffer, int Flags);
#endif
#endif
