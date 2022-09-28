/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_comp.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_comp.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines for Components

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_comp.c 1.9 1994/04/07 09:33:56 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        5/17/93  aip    Set description pos to 0 in DB_componentAdd.
		05/18/93 sfh	Revise to use MIF_String_t instead of char[]
						for strings;
        5/26/93  aip    Changed Component add and delete to support enums.
        6/9/93   aip    Static Table support.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
        4/6/94   aip    Added 32-bit string support.
        11/16/94 par    Added a check for component IDs in excess of the db max

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_componentAdd(DB_String_t *name)
{
    MIF_Pos_t      nullPos       = {0, 0};
    MIF_Pos_t      componentPos;
    unsigned short index;
    MIF_Pos_t      indexPos;
    MIF_Pos_t      *pos;
    DB_Component_t *c;

    componentPos = DB_ElementAdd(name, sizeof(DB_Component_t));
    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return nullPos;

    index = DB_indexGenerate();
    if (index == 0)
        return nullPos;
    indexPos = DB_indexPos(index);
    if ((indexPos.page == 0) && (indexPos.offset == 0))
        return nullPos;
    if ((pos = (MIF_Pos_t *) MIF_resolve(DB, indexPos, MIF_WRITE)) ==
        (MIF_Pos_t *) 0)
        return nullPos;
    pos -> page = componentPos.page;
    pos -> offset = componentPos.offset;

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
        (DB_Component_t *) 0)
        return nullPos;
    c -> name.type = MIF_COMPONENT;
    c -> id = index;
    c -> description.page = 0;
    c -> description.offset = 0;
    c -> pathLink.page = 0;
    c -> pathLink.offset = 0;
    c -> enumLink.page = 0;
    c -> enumLink.offset = 0;
    c -> tableLink.page = 0;
    c -> tableLink.offset = 0;
    c -> groupLink.page = 0;
    c -> groupLink.offset = 0;
    return componentPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_componentDelete(MIF_Pos_t componentPos)
{
    MIF_Id_t       componentId;
#ifdef ALVIN_DB
    MIF_Pos_t      componentPathPos;
    MIF_Pos_t      nextPathPos;
    MIF_Pos_t      groupPos;
    MIF_Pos_t      nextGroupPos;
    MIF_Pos_t      enumPos;
    MIF_Pos_t      tablePos;
#endif
    MIF_Pos_t      indexPos;
    MIF_Pos_t      *pos;
    DB_Component_t *c;

    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return MIF_BAD_POS;

    if ((componentId = DB_componentPosToId(componentPos)) == 0)
        return MIF_BAD_ID;

#ifdef ALVIN_DB   /* the old DB doesn't work properly in here */

    componentPathPos = DB_componentPathFindFirst(componentId);
    while ((componentPathPos.page != 0) || (componentPathPos.offset != 0)) {
        nextPathPos = DB_componentPathFindNext(componentPathPos);
        if (DB_componentPathDelete(componentId, componentPathPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        componentPathPos.page = nextPathPos.page;
        componentPathPos.offset = nextPathPos.offset;
    }

    groupPos = DB_groupFindFirst(componentId);
    while ((groupPos.page != 0) || (groupPos.offset != 0)) {
        nextGroupPos = DB_groupFindNext(groupPos);
        if (DB_groupDelete(componentId, groupPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        groupPos.page = nextGroupPos.page;
        groupPos.offset = nextGroupPos.offset;
    }

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return MIF_FILE_ERROR;
    enumPos.page = c -> enumLink.page;
    enumPos.offset = c -> enumLink.offset;
    while ((enumPos.page != 0) || (enumPos.offset != 0)) {
        if (DB_enumDelete(componentId, enumPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        enumPos.page = c -> enumLink.page;
        enumPos.offset = c -> enumLink.offset;
    }

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return MIF_FILE_ERROR;
    tablePos.page = c -> tableLink.page;
    tablePos.offset = c -> tableLink.offset;
    while ((tablePos.page != 0) || (tablePos.offset != 0)) {
        if (DB_tableDelete(componentId, tablePos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        tablePos.page = c -> tableLink.page;
        tablePos.offset = c -> tableLink.offset;
    }

#endif
    indexPos = DB_indexPos((unsigned short) componentId);
    if ((indexPos.page == 0) && (indexPos.offset == 0))
        return MIF_BAD_POS;
    if ((pos = (MIF_Pos_t *) MIF_resolve(DB, indexPos, MIF_WRITE)) ==
        (MIF_Pos_t *) 0)
        return MIF_FILE_ERROR;
    pos -> page = 0;
    pos -> offset = 0;

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
        (DB_Component_t *) 0)
        return MIF_FILE_ERROR;
    c -> id = 0;
    c -> pathLink.page = 0;
    c -> pathLink.offset = 0;
    c -> groupLink.page = 0;
    c -> groupLink.offset = 0;
    return DB_ElementDelete(componentPos);
}

#endif


MIF_Pos_t DB_componentIdToPos(MIF_Id_t componentId)
{
    MIF_Pos_t nullPos       = {0, 0};
    MIF_Pos_t indexPos;
    MIF_Pos_t *pos;
    MIF_Pos_t componentPos;

    if ((componentId == 0) || (componentId > DB_DEFAULT_INDEX_COUNT))
        return nullPos;

    indexPos = DB_indexPos((unsigned short) componentId);
    if ((indexPos.page == 0) && (indexPos.offset == 0))
        return nullPos;
    if ((pos = (MIF_Pos_t *) MIF_resolve(DB, indexPos, MIF_READ)) ==
        (MIF_Pos_t *) 0)
        return nullPos;
    componentPos.page = pos -> page;
    componentPos.offset = pos -> offset;
    return componentPos;
}

#ifndef SL_BUILD

MIF_Id_t DB_componentPosToId(MIF_Pos_t componentPos)
{
    DB_Component_t *c;
    MIF_Id_t       id;

    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return 0;

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return 0;
    id = c -> id;
    return id;
}

#endif
