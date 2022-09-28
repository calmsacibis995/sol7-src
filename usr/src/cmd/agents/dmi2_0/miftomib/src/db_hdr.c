/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_hdr.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_hdr.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Database Header Get/Set Routines

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/27/93  aip    Added unsigned short types where appropriate.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
        11/2/93  aip    Fixed header caching for multiple database files.
        12/7/93  aip    Added support routines for lock header.
		12/08/93 sfh	Changed headerLockTableLastComponentIdX
						to headerLockTableLastCompIdX; for h2inc (too long).
		12/10/93 sfh	Make asm compatible.
		12/21/93 sfh	Remove _PASCAL.
		04/08/94 sfh	Remove ifdef SL_BUILD around headerFlush.

************************* INCLUDES ***********************************/

#include <string.h>
#include "db_hdr.h"
#include "mif_db.h"

/*********************************************************************/
/************************ PRIVATE ************************************/

static MIF_Status_t DB_headerRead(MIF_FileHandle_t fileHandle);

#ifndef SL_BUILD

static MIF_Status_t DB_headerWrite(MIF_FileHandle_t fileHandle);

#endif

/*********************************************************************/

/************************ GLOBALS ************************************/

static MIF_FileHandle_t    LastHeaderRead;
static DB_DatabaseHeader_t DB_Header;

/*********************************************************************/

MIF_Status_t DB_headerFlush(MIF_FileHandle_t fileHandle)
{
    if (fileHandle == LastHeaderRead)
        LastHeaderRead = 0;
    return MIF_OKAY;
}

unsigned short DB_headerIndexTableLastIndexGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return 0;
    return DB_Header.indexTableHeader.lastIndex;
}

#ifndef SL_BUILD

MIF_Status_t DB_headerIndexTableLastIndexSet(MIF_FileHandle_t fileHandle,
                                              unsigned short index)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.indexTableHeader.lastIndex = index;
    return DB_headerWrite(fileHandle);
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_headerIndexTablePageCountSet(MIF_FileHandle_t fileHandle,
                                              unsigned short count)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.indexTableHeader.pageCount = count;
    return DB_headerWrite(fileHandle);
}

#endif

unsigned short DB_headerIndexTablePageStartGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return 0;
    return DB_Header.indexTableHeader.pageStart;
}

#ifndef SL_BUILD

MIF_Status_t DB_headerIndexTablePageStartSet(MIF_FileHandle_t fileHandle,
                                              unsigned short pageNumber)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.indexTableHeader.pageStart = pageNumber;
    return DB_headerWrite(fileHandle);
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_headerIndexTableWrapModeSet(MIF_FileHandle_t fileHandle,
                                            MIF_Bool_t value)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.indexTableHeader.wrapMode = value;
    return DB_headerWrite(fileHandle);
}

#endif

unsigned short DB_headerLockTableLastCompIdGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FALSE;
    return DB_Header.lockTableHeader.lastComponentId;
}

#ifndef SL_BUILD

MIF_Status_t DB_headerLockTableLastCompIdSet(MIF_FileHandle_t fileHandle,
                 unsigned short lastComponentId)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.lockTableHeader.lastComponentId = lastComponentId;
    return DB_headerWrite(fileHandle);
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_headerLockTablePageCountSet(MIF_FileHandle_t fileHandle,
                                            unsigned short pageCount)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.lockTableHeader.pageCount = pageCount;
    return DB_headerWrite(fileHandle);
}

#endif

unsigned short DB_headerLockTablePageStartGet(MIF_FileHandle_t fileHandle)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FALSE;
    return DB_Header.lockTableHeader.pageStart;
}

#ifndef SL_BUILD

MIF_Status_t DB_headerLockTablePageStartSet(MIF_FileHandle_t fileHandle,
                                            unsigned short pageStart)
{
    if (fileHandle != LastHeaderRead)
        if (DB_headerRead(fileHandle) != MIF_OKAY)
            return MIF_FILE_ERROR;
    DB_Header.lockTableHeader.pageStart = pageStart;
    return DB_headerWrite(fileHandle);
}

#endif

static MIF_Status_t DB_headerRead(MIF_FileHandle_t fileHandle)
{
    MIF_Pos_t           headerPos = {0, 0};
    DB_DatabaseHeader_t *header;

#ifndef SL_BUILD

    if (LastHeaderRead != 0)
        if (DB_headerWrite(LastHeaderRead) != MIF_OKAY)
            return MIF_FILE_ERROR;

#endif

    if ((header = (DB_DatabaseHeader_t *)
         MIF_resolve(fileHandle, headerPos, MIF_READ)) ==
         (DB_DatabaseHeader_t *) 0)
        return MIF_FILE_ERROR;
    DB_Header.indexTableHeader.pageStart =
        header -> indexTableHeader.pageStart;
    DB_Header.indexTableHeader.pageCount =
        header -> indexTableHeader.pageCount;
    DB_Header.indexTableHeader.wrapMode = header -> indexTableHeader.wrapMode;
    DB_Header.indexTableHeader.lastIndex =
        header -> indexTableHeader.lastIndex;
    DB_Header.lockTableHeader.pageStart = header -> lockTableHeader.pageStart;
    DB_Header.lockTableHeader.pageCount = header -> lockTableHeader.pageCount;
    DB_Header.lockTableHeader.lastComponentId =
        header -> lockTableHeader.lastComponentId;
    LastHeaderRead = fileHandle;
    return MIF_OKAY;
}

#ifndef SL_BUILD

static MIF_Status_t DB_headerWrite(MIF_FileHandle_t fileHandle)
{
    MIF_Pos_t           headerPos = {0, 0};
    DB_DatabaseHeader_t *header;

    if (fileHandle != LastHeaderRead)
        return MIF_FILE_ERROR;

    if ((header = (DB_DatabaseHeader_t *)
         MIF_resolve(fileHandle, headerPos, MIF_WRITE)) ==
         (DB_DatabaseHeader_t *) 0)
        return MIF_FILE_ERROR;
    header -> indexTableHeader.pageStart =
        DB_Header.indexTableHeader.pageStart;
    header -> indexTableHeader.pageCount =
        DB_Header.indexTableHeader.pageCount; 
    header -> indexTableHeader.wrapMode = DB_Header.indexTableHeader.wrapMode;
    header -> indexTableHeader.lastIndex =
        DB_Header.indexTableHeader.lastIndex;
    header -> lockTableHeader.pageStart = DB_Header.lockTableHeader.pageStart;
    header -> lockTableHeader.pageCount = DB_Header.lockTableHeader.pageCount;
    header -> lockTableHeader.lastComponentId =
        DB_Header.lockTableHeader.lastComponentId;
    return MIF_OKAY;
}

#endif
