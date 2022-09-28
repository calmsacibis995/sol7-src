/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_attr.c	1.2 96/09/24 Sun Microsystems"

/**********************************************************************
    Filename: db_attr.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines for Attributes

    Author(s): Alvin I. Pivowar
  
    RCS Revision: $Header: j:/mif/db/rcs/db_attr.c 1.10 1994/05/25 13:22:49 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        5/17/93  aip    Set description pos to 0 in DB_attributeAdd.
        5/26/93  aip    Added support for anonymous enumerations.
        6/22/93  aip    Added DB_attributeCopy.
        3/22/94  aip    Fixed DB_attributeAdd.
        4/6/94   aip    Added 32-bit string support.
        5/25/94  aip    Fixed DB_attributeDelete().

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_attributeAdd(MIF_Id_t componentId, MIF_Id_t groupId,
                          MIF_Id_t attributeId, DB_String_t *name)
{
    MIF_Pos_t      nullPos         = {0, 0};
    MIF_Pos_t      attributePos;
    MIF_Pos_t      newAttributePos;
    MIF_Id_t       insertionPoint;
    MIF_Pos_t      groupPos;
    DB_Group_t     *g;
    MIF_Pos_t      attributeLinkNext;
    DB_Attribute_t *a;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;
    if (attributeId == 0)
        return nullPos;

    attributePos = DB_attributeIdToPos(componentId, groupId, attributeId);
    if ((attributePos.page != 0) || (attributePos.offset != 0))
        return nullPos;

    newAttributePos = DB_ElementAdd(name, sizeof(DB_Attribute_t));
    if ((newAttributePos.page == 0) && (newAttributePos.offset == 0))
        return nullPos;
    insertionPoint = DB_attributeInsertionPointFind(componentId, groupId,
                                                    attributeId);
    if (insertionPoint == 0) {
        groupPos = DB_groupIdToPos(componentId, groupId);
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
            (DB_Group_t *) 0)
            return nullPos;
        attributeLinkNext.page = g -> attributeLink.page;
        attributeLinkNext.offset = g -> attributeLink.offset;
        g -> attributeLink.page = newAttributePos.page;
        g -> attributeLink.offset = newAttributePos.offset;
    } else {
        attributePos = DB_attributeIdToPos(componentId, groupId,
                                           insertionPoint);
        if ((attributePos.page == 0) && (attributePos.offset == 0))
            return nullPos;
        if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_WRITE))
            == (DB_Attribute_t *) 0)
            return nullPos;
        attributeLinkNext.page = a -> attributeLinkNext.page;
        attributeLinkNext.offset = a -> attributeLinkNext.offset;
        a -> attributeLinkNext.page = newAttributePos.page;
        a -> attributeLinkNext.offset = newAttributePos.offset;
    }

    if ((a = (DB_Attribute_t *) MIF_resolve(DB, newAttributePos, MIF_WRITE)) ==
        (DB_Attribute_t *) 0)
        return nullPos;
    a -> name.type = MIF_ATTRIBUTE;
    a -> id = attributeId;
    a -> accessType = MIF_UNKNOWN_ACCESS;
    a -> dataType = MIF_UNKNOWN_DATA_TYPE;
    a -> enumLink.page = 0;
    a -> enumLink.offset = 0;
    a -> dataPos.page = 0;
    a -> dataPos.offset = 0;
    a -> description.page = 0;
    a -> description.offset = 0;
    a -> attributeLinkNext.page = attributeLinkNext.page;
    a -> attributeLinkNext.offset = attributeLinkNext.offset;
    return newAttributePos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_attributeDelete(MIF_Id_t componentId, MIF_Id_t groupId,
                                 MIF_Pos_t targetPos)
{
    DB_Attribute_t *a;
    MIF_Pos_t      attributeLinkNext;
    MIF_Pos_t      attributePos;
    MIF_Pos_t      groupPos;
    DB_Group_t     *g;
    MIF_Pos_t      nextAttributePos;
    MIF_Pos_t      enumPos;
    MIF_Pos_t      dataPos;
    MIF_Pos_t      descriptionPos;
    DB_Enum_t      *e;
    DB_String_t    *s;
    MIF_Pos_t      globalEnumPos;

    if (componentId == 0)
        return MIF_BAD_ID;
    if (groupId == 0)
        return MIF_BAD_ID;
    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

    if ((a = (DB_Attribute_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_Attribute_t *) 0)
        return MIF_FILE_ERROR;
    attributeLinkNext.page = a -> attributeLinkNext.page;
    attributeLinkNext.offset = a -> attributeLinkNext.offset;

    attributePos = DB_attributeFindFirst(componentId, groupId);
    if ((attributePos.page == 0) && (attributePos.offset == 0))
        return MIF_NOT_FOUND;
    if ((attributePos.page == targetPos.page) &&
        (attributePos.offset == targetPos.offset)) {
        groupPos = DB_groupIdToPos(componentId, groupId);
        if ((groupPos.page == 0) && (groupPos.offset == 0))
            return MIF_BAD_POS;
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
            (DB_Group_t *) 0)
            return MIF_FILE_ERROR;
        g -> attributeLink.page = attributeLinkNext.page;
        g -> attributeLink.offset = attributeLinkNext.offset;
    } else {
        nextAttributePos = DB_attributeFindNext(attributePos);
        if ((nextAttributePos.page == 0) && (nextAttributePos.offset == 0))
            return MIF_NOT_FOUND;
        while ((nextAttributePos.page != targetPos.page) ||
               (nextAttributePos.offset != targetPos.offset)) {
            attributePos.page = nextAttributePos.page;
            attributePos.offset = nextAttributePos.offset;
            nextAttributePos = DB_attributeFindNext(attributePos);
            if ((nextAttributePos.page == 0) && (nextAttributePos.offset == 0))
                return MIF_NOT_FOUND;
        }
        if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_WRITE))
            == (DB_Attribute_t *) 0)
            return MIF_FILE_ERROR;
        a -> attributeLinkNext.page = attributeLinkNext.page;
        a -> attributeLinkNext.offset = attributeLinkNext.offset;
    }

    if ((a = (DB_Attribute_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Attribute_t *) 0)
        return MIF_FILE_ERROR;
    enumPos.page = a -> enumLink.page;
    enumPos.offset = a -> enumLink.offset;
    dataPos.page = a -> dataPos.page;
    dataPos.offset = a -> dataPos.offset;
    descriptionPos.page = a -> description.page;
    descriptionPos.offset = a -> description.offset;
    a -> id = 0;
    a -> accessType = MIF_UNKNOWN_ACCESS;
    a -> dataType = MIF_UNKNOWN_DATA_TYPE;
    a -> enumLink.page = 0;
    a -> enumLink.offset = 0;
    a -> dataPos.page = 0;
    a -> dataPos.offset = 0;
    a -> description.page = 0;
    a -> description.offset = 0;
    a -> attributeLinkNext.page = 0;
    a -> attributeLinkNext.offset = 0;
    if ((enumPos.page != 0) || (enumPos.offset != 0)) {
        if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
            (DB_Enum_t *) 0)
            return MIF_FILE_ERROR;
        if ((s = (DB_String_t *) MIF_resolve(DB, e -> name.name, MIF_READ)) ==
            (DB_String_t *) 0)
            return MIF_FILE_ERROR;
        globalEnumPos = DB_enumFind(componentId, s);
        if ((globalEnumPos.page == 0) && (globalEnumPos.offset == 0))
            if (DB_enumDelete(0, enumPos) != MIF_OKAY)
                return MIF_FILE_ERROR;
    }
    if ((dataPos.page != 0) || (dataPos.offset != 0))
        if (DB_dataDelete(dataPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
    if ((descriptionPos.page != 0) || (descriptionPos.offset != 0)) 
        if (DB_stringDelete(descriptionPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
    return DB_ElementDelete(targetPos);
}

#endif

MIF_Pos_t DB_attributeFindFirst(MIF_Id_t componentId, MIF_Id_t groupId)
{
    MIF_Pos_t  nullPos       = {0, 0};
    MIF_Pos_t  groupPos;
    DB_Group_t *g;
    MIF_Pos_t  attributePos;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;

    groupPos = DB_groupIdToPos(componentId, groupId);
    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return nullPos;
    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return nullPos;
    attributePos.page= g -> attributeLink.page;
    attributePos.offset = g -> attributeLink.offset;
    return attributePos;
}

MIF_Pos_t DB_attributeFindNext(MIF_Pos_t attributePos)
{
    MIF_Pos_t      nullPos           = {0, 0};
    DB_Attribute_t *a;
    MIF_Pos_t      nextAttributePos;

    if ((attributePos.page == 0) && (attributePos.offset == 0))
        return nullPos;
    
    if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
        (DB_Attribute_t *) 0)
        return nullPos;
    nextAttributePos.page = a -> attributeLinkNext.page;
    nextAttributePos.offset = a -> attributeLinkNext.offset;
    return nextAttributePos;
}

MIF_Pos_t DB_attributeIdToPos(MIF_Id_t componentId, MIF_Id_t groupId,
                              MIF_Id_t attributeId)
{
    MIF_Pos_t      nullPos       = {0, 0};
    MIF_Pos_t      attributePos;
    DB_Attribute_t *a;
    MIF_Id_t       id;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;
    if (attributeId == 0)
        return nullPos;

    attributePos = DB_attributeFindFirst(componentId, groupId);
    if ((attributePos.page == 0) && (attributePos.offset == 0))
        return nullPos;
    if (( a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
        (DB_Attribute_t *) 0)
        return nullPos;
    id = a -> id;
    while (id != attributeId) {
        attributePos = DB_attributeFindNext(attributePos);
        if ((attributePos.page == 0) && (attributePos.offset == 0))
            return nullPos;
        if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
            (DB_Attribute_t *) 0)
            return nullPos;
        id = a -> id;
    }
    return attributePos;
}

MIF_Id_t DB_attributeInsertionPointFind(MIF_Id_t componentId, MIF_Id_t groupId,
                                        MIF_Id_t attributeId)
{
    MIF_Id_t       id;
    MIF_Pos_t      attributePos;
    DB_Attribute_t *a;

    id = 0;
    if (componentId == 0)
        return id;
    if (groupId == 0)
        return id;
    attributePos = DB_attributeFindFirst(componentId, groupId);
    if ((attributePos.page == 0) && (attributePos.offset == 0))
        return id;

    if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
        (DB_Attribute_t *) 0)
        return id;
    while (attributeId >= a -> id) {
        id = a -> id;
        attributePos = DB_attributeFindNext(attributePos);
        if ((attributePos.page == 0) && (attributePos.offset == 0))
            return id;
        if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
            (DB_Attribute_t *) 0)
            return id;
    }
    return id;
}


