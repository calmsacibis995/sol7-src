/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_group.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_group.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines for Groups

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_group.c 1.8 1994/04/07 09:34:04 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
		05/06/93 sfh	Revise DB_groupDelete to call DB_keyDelete.
        5/13/93  aip    Only call DB_keyDelete if the keyPos is non-NIL.
        6/9/93   aip    Static Table support.
        6/22/93  aip    Added DB_groupCopy.
        4/6/94   aip    Added 32-bit string support.

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_groupAdd(MIF_Id_t componentId, MIF_Id_t groupId, DB_String_t *name)
{
    MIF_Pos_t      nullPos      = {0, 0};
    MIF_Pos_t      newGroupPos;
    MIF_Id_t       insertionPoint;
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      groupLinkNext;
    MIF_Pos_t      groupPos;
    DB_Group_t     *g;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;

    groupPos = DB_groupIdToPos(componentId, groupId);
    if ((groupPos.page != 0) || (groupPos.offset != 0))
        return nullPos;

    newGroupPos = DB_ElementAdd(name, sizeof(DB_Group_t));
    if ((newGroupPos.page == 0) && (newGroupPos.offset == 0))
        return nullPos;      
    insertionPoint = DB_groupInsertionPointFind(componentId, groupId);
    if (insertionPoint == 0) {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return nullPos;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
            (DB_Component_t *) 0)
            return nullPos;
        groupLinkNext.page = c -> groupLink.page;
        groupLinkNext.offset = c -> groupLink.offset;
        c -> groupLink.page = newGroupPos.page;
        c -> groupLink.offset = newGroupPos.offset;
    } else {
        groupPos = DB_groupIdToPos(componentId, insertionPoint);
        if ((groupPos.page == 0) && (groupPos.offset == 0))
            return nullPos;
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
            (DB_Group_t *) 0)
            return nullPos;
        groupLinkNext.page = g -> groupLinkNext.page;
        groupLinkNext.offset = g -> groupLinkNext.offset;
        g -> groupLinkNext.page = newGroupPos.page;
        g -> groupLinkNext.offset = newGroupPos.offset;
    }
    if ((g = (DB_Group_t *) MIF_resolve(DB, newGroupPos, MIF_WRITE)) ==
        (DB_Group_t *) 0)
        return nullPos;
    g -> name.type = MIF_GROUP;
    g -> id = groupId;
    g -> classStr.page = 0;
    g -> classStr.offset = 0;
    g -> key.page = 0;
    g -> key.offset = 0;
    g -> description.page = 0;
    g -> description.offset = 0;
    g -> pragma.page = 0;
    g -> pragma.offset = 0;
    g -> groupLinkNext.page = groupLinkNext.page;
    g -> groupLinkNext.offset = groupLinkNext.offset;
    g -> attributeLink.page = 0;
    g -> attributeLink.offset = 0;
    g -> tableLink.page = 0;
    g -> tableLink.offset = 0;
    return newGroupPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_groupDelete(MIF_Id_t componentId, MIF_Pos_t targetPos)
{
    MIF_Id_t       groupId;
    MIF_Pos_t      attributePos;
    MIF_Pos_t      nextAttributePos;
    DB_Group_t     *g;
    MIF_Pos_t      groupLinkNext;
    MIF_Pos_t      groupPos;
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      nextGroupPos;

    if (componentId == 0)
        return MIF_BAD_ID;
    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

    groupId = DB_groupPosToId(targetPos);
    if (groupId == 0)
        return MIF_BAD_ID;
    attributePos = DB_attributeFindFirst(componentId, groupId);
    while ((attributePos.page != 0) || (attributePos.offset != 0)) {
        nextAttributePos = DB_attributeFindNext(attributePos);
        if (DB_attributeDelete(componentId, groupId, attributePos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        attributePos.page = nextAttributePos.page;
        attributePos.offset = nextAttributePos.offset;
    }

    if ((g = (DB_Group_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return MIF_FILE_ERROR;
    groupLinkNext.page = g -> groupLinkNext.page;
    groupLinkNext.offset = g -> groupLinkNext.offset;
    groupPos = DB_groupFindFirst(componentId);
    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return MIF_NOT_FOUND;
    if ((groupPos.page == targetPos.page) &&
        (groupPos.offset == targetPos.offset)) {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return MIF_BAD_POS;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE))
            == (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        c -> groupLink.page = groupLinkNext.page;
        c -> groupLink.offset = groupLinkNext.offset;
    } else {
        nextGroupPos = DB_groupFindNext(groupPos);
        if ((nextGroupPos.page == 0) && (nextGroupPos.offset == 0))
            return MIF_NOT_FOUND;
        while((nextGroupPos.page != targetPos.page) ||
              (nextGroupPos.offset != targetPos.offset)) {
            groupPos.page = nextGroupPos.page;
            groupPos.offset = nextGroupPos.offset;
            nextGroupPos = DB_groupFindNext(groupPos);
            if ((nextGroupPos.page == 0) && (nextGroupPos.offset == 0))
                return MIF_NOT_FOUND;
        }
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
            (DB_Group_t *) 0)
            return MIF_FILE_ERROR;
        g -> groupLinkNext.page = groupLinkNext.page;
        g -> groupLinkNext.offset = groupLinkNext.offset;
    }

    if ((g = (DB_Group_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Group_t *) 0)
        return MIF_FILE_ERROR;

    if ((g -> key.page != 0) || (g -> key.offset != 0))
	    if (DB_keyDelete(g -> key) != MIF_OKAY)
	    	return MIF_FILE_ERROR;

    g -> id = 0;
    g -> classStr.page = 0;
    g -> classStr.offset = 0;
    g -> key.page = 0;
    g -> key.offset = 0;
    g -> description.page = 0;
    g -> description.offset = 0;
    g -> groupLinkNext.page = 0;
    g -> groupLinkNext.offset = 0;
    g -> attributeLink.page = 0;
    g -> attributeLink.offset = 0;
    g -> tableLink.page = 0;
    g -> tableLink.offset = 0;
    return DB_ElementDelete(targetPos);
}

#endif

MIF_Pos_t DB_groupFindFirst(MIF_Id_t componentId)
{
    MIF_Pos_t      nullPos       = {0, 0};
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      groupPos;

    if (componentId == 0)
        return nullPos;

    componentPos = DB_componentIdToPos(componentId);
    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return nullPos;
    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return nullPos;
    groupPos.page = c -> groupLink.page;
    groupPos.offset = c -> groupLink.offset;
    return groupPos;
}

MIF_Pos_t DB_groupFindNext(MIF_Pos_t groupPos)
{
    MIF_Pos_t  nullPos       = {0, 0};
    DB_Group_t *g;
    MIF_Pos_t  nextGroupPos;

    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return nullPos;

    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return nullPos;
    nextGroupPos.page = g -> groupLinkNext.page;
    nextGroupPos.offset = g -> groupLinkNext.offset;
    return nextGroupPos;
}


MIF_Pos_t DB_groupIdToPos(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Pos_t  nullPos   = {0, 0};
    MIF_Pos_t  groupPos;
    DB_Group_t *g;
    MIF_Id_t   id;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;

    groupPos = DB_groupFindFirst(componentId);
    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return nullPos;
    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return nullPos;
    id = g -> id;
    while (id != groupId) {
        groupPos = DB_groupFindNext(groupPos);
        if ((groupPos.page == 0) && (groupPos.offset == 0))
            return nullPos;
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
            (DB_Group_t *) 0)
            return nullPos;
        id = g -> id;
    }
    return groupPos;
}

MIF_Id_t DB_groupInsertionPointFind(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Id_t   id;
    MIF_Pos_t  groupPos;
    DB_Group_t *g;

    id = 0;
    if (componentId == 0)
        return id;
    groupPos = DB_groupFindFirst(componentId);
    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return id;

    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return id;
    while (groupId >= g -> id) {
        id = g -> id;
        groupPos = DB_groupFindNext(groupPos);
        if ((groupPos.page == 0) && (groupPos.offset == 0))
            return id;
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
            (DB_Group_t *) 0)
            return id;
    }
    return id;
}

#ifndef SL_BUILD

MIF_Id_t DB_groupPosToId(MIF_Pos_t groupPos)
{
    DB_Group_t *g;
    MIF_Id_t   id;

    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return 0;

    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return 0;
    id = g -> id;
    return id;
}

#endif
