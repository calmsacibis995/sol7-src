/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_cpath.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_cpath.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines for Component Paths

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_cpath.c 1.6 1994/04/08 09:06:53 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
		05/18/93 sfh	Revise to use MIF_String_t instead of char[]
						for strings;
        12/6/93  aip    Initialize and clear new pathType member.
                        Changed path block.
        4/6/94   aip    Added 32-bit string support.
        4/7/94   aip    New path structure.

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_componentPathAdd(MIF_Id_t componentId, DB_String_t *name)
{
    MIF_Pos_t          nullPos              = {0, 0};
    MIF_Pos_t          newComponentPathPos;
    MIF_Pos_t          componentPathPos;
    MIF_Pos_t          componentPos;
    DB_Component_t     *c;
    MIF_Pos_t          nextComponentPathPos;
    DB_ComponentPath_t *cp;

    if (componentId == 0)
        return nullPos;

    newComponentPathPos = DB_ElementAdd(name, sizeof(DB_ComponentPath_t));
    if ((newComponentPathPos.page == 0) && (newComponentPathPos.offset == 0))
        return nullPos;
    componentPathPos = DB_componentPathFindFirst(componentId);
    if ((componentPathPos.page == 0) && (componentPathPos.offset == 0)) {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset))
            return nullPos;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
            (DB_Component_t *) 0)
            return nullPos;
        c -> pathLink.page = newComponentPathPos.page;
        c -> pathLink.offset = newComponentPathPos.offset;
    } else {
        nextComponentPathPos = DB_componentPathFindNext(componentPathPos);
        while ((nextComponentPathPos.page != 0) ||
               (nextComponentPathPos.offset != 0)) {
            componentPathPos.page = nextComponentPathPos.page;
            componentPathPos.offset = nextComponentPathPos.offset;
            nextComponentPathPos = DB_componentPathFindNext(componentPathPos);
        }
        if ((cp = (DB_ComponentPath_t *)
            MIF_resolve(DB, componentPathPos, MIF_WRITE)) ==
            (DB_ComponentPath_t *) 0)
            return nullPos;
        cp -> pathLinkNext.page = newComponentPathPos.page;
        cp -> pathLinkNext.offset = newComponentPathPos.offset;
    }

    if ((cp = (DB_ComponentPath_t *)
        MIF_resolve(DB, newComponentPathPos, MIF_WRITE)) ==
        (DB_ComponentPath_t *) 0)
        return nullPos;
    cp -> pathType = MIF_UNKNOWN_PATH_TYPE;
    cp -> name.type = MIF_COMPONENT_PATH;
    cp -> iEnvironmentId = MIF_UNKNOWN_ENVIRONMENT;
    cp -> pathValue.page = 0;
    cp -> pathValue.offset = 0;
    cp -> pathLinkNext.page = 0;
    cp -> pathLinkNext.offset = 0;
    return newComponentPathPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_componentPathDelete(MIF_Id_t componentId,
                                    MIF_Pos_t targetPos)
{
    MIF_Pos_t          nextPathLink;
    MIF_Pos_t          componentPathPos;
    MIF_Pos_t          componentPos;
    DB_Component_t     *c;
    MIF_Pos_t          nextComponentPathPos;
    DB_ComponentPath_t *cp;

    if (componentId == 0)
        return MIF_BAD_ID;
    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

    nextPathLink = DB_componentPathFindNext(targetPos);
    componentPathPos = DB_componentPathFindFirst(componentId);
    if ((componentPathPos.page == 0) && (componentPathPos.offset == 0))
        return MIF_NOT_FOUND;
    if ((componentPathPos.page == targetPos.page) &&
        (componentPathPos.offset == targetPos.offset)) {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return MIF_BAD_POS;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        c -> pathLink.page = nextPathLink.page;
        c -> pathLink.offset = nextPathLink.offset;
    } else {
        nextComponentPathPos = DB_componentPathFindNext(componentPathPos);
        if ((nextComponentPathPos.page == 0) &&
            (nextComponentPathPos.offset == 0))
            return MIF_NOT_FOUND;
        while ((nextComponentPathPos.page != targetPos.page) ||
               (nextComponentPathPos.offset != targetPos.offset)) {
            componentPathPos.page = nextComponentPathPos.page;
            componentPathPos.offset = nextComponentPathPos.offset;
            nextComponentPathPos = DB_componentPathFindNext(componentPathPos);
            if ((nextComponentPathPos.page == 0) &&
                (nextComponentPathPos.offset == 0))
                return MIF_NOT_FOUND;
        }
        if ((cp = (DB_ComponentPath_t *)
            MIF_resolve(DB, componentPathPos, MIF_WRITE)) ==
            (DB_ComponentPath_t *) 0)
            return MIF_FILE_ERROR;
        cp -> pathLinkNext.page = nextPathLink.page;
        cp -> pathLinkNext.offset = nextPathLink.offset;
    }

    if ((cp = (DB_ComponentPath_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_ComponentPath_t *) 0)
        return MIF_FILE_ERROR;
    cp -> pathType = MIF_UNKNOWN_PATH_TYPE;
    cp -> iEnvironmentId = MIF_UNKNOWN_ENVIRONMENT;
    cp -> pathValue.page = 0;
    cp -> pathValue.offset = 0;
    cp -> pathLinkNext.page = 0;
    cp -> pathLinkNext.offset = 0;
    return DB_ElementDelete(targetPos);
}

#endif

MIF_Pos_t DB_componentPathFindFirst(MIF_Id_t componentId)
{
    MIF_Pos_t      nullPos           = {0, 0};
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      componentPathPos;

    if (componentId == 0)
        return nullPos;

    componentPos = DB_componentIdToPos(componentId);
    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return nullPos;
    componentPathPos.page = c -> pathLink.page;
    componentPathPos.offset = c -> pathLink.offset;
    return componentPathPos;
}

MIF_Pos_t DB_componentPathFindNext(MIF_Pos_t componentPathPos)
{
    MIF_Pos_t          nullPos               = {0, 0};
    DB_ComponentPath_t *cp;
    MIF_Pos_t          nextComponentPathPos;

    if ((componentPathPos.page == 0) && (componentPathPos.offset == 0))
        return nullPos;

    if ((cp = (DB_ComponentPath_t *)
        MIF_resolve(DB, componentPathPos, MIF_READ)) ==
        (DB_ComponentPath_t *) 0)
        return nullPos;
    nextComponentPathPos.page = cp -> pathLinkNext.page;
    nextComponentPathPos.offset = cp -> pathLinkNext.offset;
    return nextComponentPathPos;
}
