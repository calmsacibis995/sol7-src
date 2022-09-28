/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_lock.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_lock.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: Database Component-level locking procedures

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    RCS Revision: $Header: n:/mif/db/rcs/db_lock.c 1.4 1993/12/21 16:14:47 shanraha Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        12/7/93  aip    Creation date.
		12/08/93 sfh	Changed headerLockTableLastComponentIdX
						to headerLockTableLastCompIdX; for h2inc (too long).
		12/10/93 sfh	Make asm compatible.
		12/21/93 sfh	Remove _PASCAL.
        11/16/94 par    Added component checks for ID in excess of DB max
        10/16/95 par    Modifed include structure for new MF_IO stuff

************************* INCLUDES ***********************************/

#include "db_hdr.h"
#include "db_int.h"
#include "db_lock.h"
#include "mif_db.h"
#include "db_api.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

extern MIF_FileHandle_t DB;

/*********************************************************************/

MIF_Status_t DB_lockGet(unsigned short componentId,MIF_MaHandle_t *Handle)
{
    MIF_Pos_t      maHandlePos;
    MIF_MaHandle_t *maHandle;
    unsigned short maHandlesPerPage;

    (*Handle) = (MIF_MaHandle_t)NULL;  /* default to the null handle */
    if ((componentId == 0) || (componentId > DB_DEFAULT_INDEX_COUNT))
        return MIF_BAD_ID;

    maHandlesPerPage = MIF_PAGE_SIZE / sizeof(MIF_MaHandle_t);
    maHandlePos.page = DB_headerLockTablePageStartGet(DB);
    maHandlePos.page += (unsigned short) (componentId / maHandlesPerPage);
    maHandlePos.offset = (unsigned short) ((componentId % maHandlesPerPage) *
                                           sizeof(MIF_MaHandle_t));

    if ((maHandlePos.page == 0) && (maHandlePos.offset == 0))
        return MIF_NOT_FOUND;

    if ((maHandle = (MIF_MaHandle_t *) MIF_resolve(DB, maHandlePos,
                                                   MIF_READ)) ==
        (MIF_MaHandle_t *) 0)
        return MIF_NOT_FOUND;

    (*Handle) = (*maHandle);
    return MIF_OKAY;       /* all went well */
}

MIF_Status_t DB_lockSet(unsigned short componentId, MIF_MaHandle_t maHandle)
{
    MIF_Pos_t      maHandlePos;
    MIF_MaHandle_t *maHandlePtr;
    unsigned short maHandlesPerPage;

    if ((componentId == 0) || (componentId > DB_DEFAULT_INDEX_COUNT))
        return MIF_BAD_ID;

    maHandlesPerPage = MIF_PAGE_SIZE / sizeof(MIF_MaHandle_t);
    maHandlePos.page = DB_headerLockTablePageStartGet(DB);
    maHandlePos.page += (unsigned short) (componentId / maHandlesPerPage);
    maHandlePos.offset = (unsigned short) ((componentId % maHandlesPerPage) *
                                           sizeof(MIF_MaHandle_t));

    if ((maHandlePos.page == 0) && (maHandlePos.offset == 0))
        return MIF_BAD_POS;

    if ((maHandlePtr = (MIF_MaHandle_t *) MIF_resolve(DB, maHandlePos,
                                                      MIF_WRITE)) ==
        (MIF_MaHandle_t *) 0)
        return MIF_FILE_ERROR;

    *maHandlePtr = maHandle;

    return MIF_OKAY;
}

MIF_Status_t DB_lockTableClear(void)
{
    MIF_Pos_t      maHandlePos;
    MIF_MaHandle_t *maHandle;
    unsigned short maHandlesPerPage;
    unsigned short pageStart;
    unsigned short lastComponentId;
    unsigned short i;

    pageStart = DB_headerLockTablePageStartGet(DB);
    lastComponentId = DB_headerLockTableLastCompIdGet(DB);
    maHandlesPerPage = MIF_PAGE_SIZE / sizeof(MIF_MaHandle_t);

    for (i = 0; i < lastComponentId; ++i) {
        maHandlePos.page = pageStart;
        maHandlePos.page += (unsigned short) (i / maHandlesPerPage);
        maHandlePos.offset = (unsigned short)
                             ((i % maHandlesPerPage) * sizeof(MIF_MaHandle_t));
        if ((maHandle = (MIF_MaHandle_t *)MIF_resolve(DB, maHandlePos,
                                                      MIF_WRITE)) ==
            (MIF_MaHandle_t *) 0)
            return MIF_FILE_ERROR;
        *maHandle = 0;
    }

    return MIF_OKAY;
}

#ifndef SL_BUILD

MIF_Status_t DB_lockTableCreate(unsigned short lastComponentId)
{
    MIF_Pos_t      pos;
    unsigned short pageStart;
    unsigned short pageCount;
    short          i;

    MIF_allocAlign(DB, MIF_PAGE_SIZE);
    pos = MIF_headerHighWaterMarkGet(DB);
    pageStart = pos.page;
    pageCount = (unsigned short) ((sizeof(MIF_MaHandle_t) * lastComponentId +
                                  MIF_PAGE_SIZE - 1) / MIF_PAGE_SIZE);
    for (i = 0; i < (short) pageCount; ++i) {
        pos = MIF_allocClear(DB, MIF_PAGE_SIZE);
        if ((pos.page == 0) && (pos.offset == 0))
            return MIF_FILE_ERROR;
    }
    if (DB_headerLockTablePageStartSet(DB, pageStart) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_headerLockTablePageCountSet(DB, pageCount) != MIF_OKAY)
        return MIF_FILE_ERROR;
    if (DB_headerLockTableLastCompIdSet(DB, lastComponentId) != MIF_OKAY)
        return MIF_FILE_ERROR;
    return MIF_OKAY;
}

#endif
