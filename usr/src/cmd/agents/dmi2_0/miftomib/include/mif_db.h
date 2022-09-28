/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)mif_db.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: mif_defs.h

    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: Common MIF Definitions

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
                05/21/93 sfh    Put #ifdef _H2INC around definition of offset.
                                                This is not allowed in MASM; since it's not being
                                                used (included only because other files include
                                                mif_defs.h), it shouldn't be a problem.
        7/27/93  aip    Added short to prototypes.
        8/5/93   aip    Changed unsigned to unsigned short and
                        int to short.
                11/22/93 sfh    Add definition of _PASCAL.
                12/08/93 sfh    Remove MIF_Pos_t macro.
                12/21/93 sfh    Remove _PASCAL.
        10/16/95 par    Modified for new MIF_IO include information

**********************************************************************/

#ifndef MIF_DEFS_H_FILE
#define MIF_DEFS_H_FILE

/************************ INCLUDES ***********************************/

#include <time.h>
#ifdef OS2_SL_BUILD
#define INCL_DOSFILEMGR
#define INCL_DOSDATETIME
#include <os2.h>
#endif

/*********************************************************************/

/************************ DEFINES ************************************/

#define MIF_CACHE_SLOT_COUNT 32
#define MIF_UNUSED_SLOT      0xFF
#define MIF_PAGE_SIZE        512


/*********************************************************************/

/************************ TYPEDEFS ***********************************/

#ifdef OS2_SL_BUILD
typedef HFILE MIF_FileHandle_t;
#elif defined(_WIN32)
typedef int MIF_FileHandle_t;
#elif AIX
#include <stdio.h>
FILE *aixFileStream;
typedef FILE *MIF_FileHandle_t;
#else
typedef unsigned short MIF_FileHandle_t;
#endif

typedef enum {MIF_READ = 1, MIF_WRITE} MIF_IoMode_t;

typedef enum {MIF_FALSE, MIF_TRUE} MIF_Bool_t;

typedef unsigned short MIF_Semaphore_t;

typedef enum {MIF_OKAY, MIF_NOT_FOUND, MIF_FILE_ERROR, MIF_BAD_FORMAT,
              MIF_NOT_OPEN, MIF_BAD_POS, MIF_BAD_ID, MIF_INTERNAL_ERROR}
              MIF_Status_t;

typedef struct {
            unsigned long page;
            unsigned long offset;
        } MIF_Pos_t;

typedef struct {
            unsigned short magic;
            char           version[80];
            time_t         ltime;
            MIF_Pos_t      highWaterMark;
        } MIF_Header_t;

typedef enum {MIF_CACHE_FLUSH = 1, MIF_CACHE_WRITETHRU} MIF_cacheMode_t;


/************************ PUBLIC *************************************/

MIF_Status_t            MIF_close(MIF_FileHandle_t fileHandle);
MIF_FileHandle_t        MIF_create(char *filename, unsigned short magicNumber,char *version);
unsigned long           MIF_length(MIF_FileHandle_t fileHandle);
MIF_FileHandle_t        MIF_open(char *filename, MIF_IoMode_t ioMode,unsigned short magicNumber);
MIF_Status_t            MIF_remove(char *filename);
MIF_Status_t            MIF_pageRead(MIF_FileHandle_t fileHandle, unsigned long pageNumber, char *buffer, unsigned long bufferLength);
MIF_Status_t            MIF_cacheFlush(MIF_FileHandle_t fileHandle,MIF_cacheMode_t Mode);
void *                  MIF_resolve(MIF_FileHandle_t fileHandle,MIF_Pos_t pos, MIF_IoMode_t mode);
MIF_Pos_t               MIF_alloc(MIF_FileHandle_t fileHandle, unsigned short size);
void                    MIF_allocAlign(MIF_FileHandle_t fileHandle, unsigned short size);
MIF_Pos_t               MIF_allocClear(MIF_FileHandle_t fileHandle, unsigned short size);
MIF_Pos_t               MIF_headerHighWaterMarkGet(MIF_FileHandle_t fileHandle);
time_t                  MIF_headerTimeGet(MIF_FileHandle_t fileHandle);
char *                  MIF_headerVersionGet(MIF_FileHandle_t fileHandle);


/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#endif
