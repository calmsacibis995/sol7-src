/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_index.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_index.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Database Index Routines

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/27/93  aip    Added unsigned short types where appropriate.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
        10/16/95 par    Modifed include structure for use with new MIF_IO

************************* INCLUDES ***********************************/

#include <limits.h>
#include "db_hdr.h"
#include "db_index.h"
#include "mif_db.h"

/*********************************************************************/
/************************ PRIVATE ************************************/

static unsigned short DB_indexWrapGenerate(void);

/*********************************************************************/

/************************ GLOBALS ************************************/

extern MIF_FileHandle_t DB;

/*********************************************************************/

#ifndef SL_BUILD

unsigned short DB_indexGenerate(void)
{
    unsigned short lastIndex;

    lastIndex = DB_headerIndexTableLastIndexGet(DB);
    if (lastIndex == USHRT_MAX) {
        DB_headerIndexTableWrapModeSet(DB, MIF_TRUE);
        return DB_indexWrapGenerate();
    }
    ++lastIndex;
    if (DB_headerIndexTableLastIndexSet(DB, lastIndex) != MIF_OKAY)
        return 0;
    return lastIndex;
}

#endif

MIF_Pos_t DB_indexPos(unsigned short index)
{
    MIF_Pos_t indexPos;
    short     indexesPerPage;

    indexPos.page = DB_headerIndexTablePageStartGet(DB);
    indexesPerPage = MIF_PAGE_SIZE / sizeof(MIF_Pos_t);
    indexPos.page += (unsigned short) (index / indexesPerPage);
    indexPos.offset = (unsigned short)
                      ((index % indexesPerPage) * sizeof(MIF_Pos_t));
    return indexPos;
}

#ifndef SL_BUILD

MIF_Status_t DB_indexTableCreate(unsigned short indexCount)
{
    MIF_Pos_t      pos;
    unsigned short pageStart;
    unsigned short pageCount;
    short          i;

    MIF_allocAlign(DB, MIF_PAGE_SIZE);
    pos = MIF_headerHighWaterMarkGet(DB);
    pageStart = pos.page;
    pageCount = (unsigned short) ((sizeof(MIF_Pos_t) * indexCount +
                                  MIF_PAGE_SIZE - 1) / MIF_PAGE_SIZE);
    for (i = 0; i < (short) pageCount; ++i) {
        pos = MIF_allocClear(DB, MIF_PAGE_SIZE);
        if ((pos.page == 0) && (pos.offset == 0))
            return MIF_FILE_ERROR;
    }
    if (DB_headerIndexTablePageStartSet(DB, pageStart) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_headerIndexTablePageCountSet(DB, pageCount) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_headerIndexTableWrapModeSet(DB, MIF_FALSE) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_headerIndexTableLastIndexSet(DB, 0) != MIF_OKAY)
        return MIF_FILE_ERROR;
    return MIF_OKAY;
}

#endif

#ifndef SL_BUILD

static unsigned short DB_indexWrapGenerate(void)
{
    unsigned  short lastIndex;
    unsigned  short index;
    MIF_Pos_t       indexPos;
    MIF_Pos_t       *pos;

    lastIndex = DB_headerIndexTableLastIndexGet(DB);
    if (lastIndex == 0)
        return 0;
    index = (unsigned short) (lastIndex + 1);
    while (index != lastIndex)
        if (index != 0) {
            indexPos = DB_indexPos(index);
            if ((indexPos.page == 0) && (indexPos.offset == 0))
                return 0;
            if ((pos = (MIF_Pos_t *) MIF_resolve(DB, indexPos, MIF_READ)) ==
                (MIF_Pos_t *) 0)
                return 0;
            if ((pos -> page == 0) && (pos -> offset == 0))
                return index;
            ++index;
        }
    return 0;
}

#endif
