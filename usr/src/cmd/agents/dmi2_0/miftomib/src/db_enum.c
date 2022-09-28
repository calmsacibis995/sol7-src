/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_enum.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_enum.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Database Enumeration Object Procedures

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_enum.c 1.7 1994/04/07 09:34:00 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        5/24/93  aip    Creation date.
        5/26/93  aip    elementCount maintained by add and delete.
        6/22/93  aip    Added DB_enumCopy and DB_EnumElementCopy.
        6/23/93  aip    Return the original enumPos when asked to copy a
                        global enumeration.
        4/6/94   aip    Added 32-bit string support.

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

/*
    This procedure is called with a zero component ID when adding an
    anonymous enumeration.
*/

#ifndef SL_BUILD

MIF_Pos_t DB_enumAdd(MIF_Id_t componentId, DB_String_t *name)
{
    MIF_Pos_t      nullPos =     {0, 0};
    MIF_Pos_t      componentPos;
    MIF_Pos_t      newEnumPos;
    DB_Component_t *c;
    MIF_Pos_t      enumPos;
    DB_Enum_t      *e;

/*  If the component ID is not zero, check it for correctness.               */

    if (componentId != 0) {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return nullPos;
    }

/*  Create a new enum.                                                       */

    newEnumPos = DB_ElementAdd(name, sizeof(DB_Enum_t));
    if ((newEnumPos.page == 0) && (newEnumPos.offset == 0))
        return nullPos;

/*  If we are not building an anonymous enum, then link it into the component. */

    if (componentId != 0) {

/*      Link the enum into the component's enum chain.                       */

        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
            (DB_Component_t *) 0)
            return nullPos;
        enumPos.page = c -> enumLink.page;
        enumPos.offset = c -> enumLink.offset;
        if ((enumPos.page == 0) && (enumPos.offset == 0)) {

/*          There were no previous enums in the chain.                       */

            if ((c = (DB_Component_t *)
                MIF_resolve(DB, componentPos, MIF_WRITE)) ==
                (DB_Component_t *) 0)
                return nullPos;
            c -> enumLink.page = newEnumPos.page;
            c -> enumLink.offset = newEnumPos.offset;
        } else {

/*          There were previous enums in the chain.                          */

            if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
                (DB_Enum_t *) 0)
                return nullPos;

/*          Walk to the last enum in the chain.                              */

            while ((e -> enumLinkNext.page != 0) ||
                   (e -> enumLinkNext.offset != 0)) {
                enumPos.page = e -> enumLinkNext.page;
                enumPos.offset = e -> enumLinkNext.offset;
                if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
                    (DB_Enum_t *) 0)
                    return nullPos;
            }
            if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_WRITE)) ==
                (DB_Enum_t *) 0)
                return nullPos;
            e -> enumLinkNext.page = newEnumPos.page;
            e -> enumLinkNext.offset = newEnumPos.offset;
        }
    }

/*  Initialize the fields of the enum.                                       */

    if ((e = (DB_Enum_t *) MIF_resolve(DB, newEnumPos, MIF_WRITE)) ==
        (DB_Enum_t *) 0)
        return nullPos;
    e -> name.type = MIF_ENUM;
    e -> dataType = MIF_UNKNOWN_DATA_TYPE;
    e -> elementCount = 0;
    e -> elementLink.page = 0;
    e -> elementLink.offset = 0;
    e -> enumLinkNext.page = 0;
    e -> enumLinkNext.offset = 0;

    return newEnumPos;
}

#endif

/*
    This procedure is called with a zero component ID when deleting an
    anonymous enumeration.
*/

#ifndef SL_BUILD

MIF_Status_t DB_enumDelete(MIF_Id_t componentId, MIF_Pos_t targetPos)
{
    DB_Enum_t      *e;
    MIF_Pos_t      enumLinkNext;
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      enumLink;
    MIF_Pos_t      enumPos;
    MIF_Pos_t      elemPos;

/*  Check for a bad pos.                                                     */

    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

/*  Check to see if the pos references an enum.                              */

    if ((e = (DB_Enum_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_Enum_t *) 0)
        return MIF_FILE_ERROR;
    if (e -> name.type != MIF_ENUM)
        return MIF_BAD_POS;

    enumLinkNext.page = e -> enumLinkNext.page;
    enumLinkNext.offset = e -> enumLinkNext.offset;

/*
    If we are not deleting an anonymous enumeration, then ensure that it
    is on the specified component.
*/

    if (componentId != 0) {
        componentPos = DB_componentIdToPos(componentId);       
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return MIF_NOT_FOUND;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        enumPos.page = enumLink.page = c -> enumLink.page;
        enumPos.offset = enumLink.offset = c -> enumLink.offset;
        while ((enumPos.page != targetPos.page) ||
               (enumPos.offset != targetPos.offset)) {
            if ((enumPos.page == 0) && (enumPos.offset == 0))
                return MIF_NOT_FOUND;
            if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
                (DB_Enum_t *) 0)
                return MIF_FILE_ERROR;
            enumPos.page = e -> enumLinkNext.page;
            enumPos.offset = e -> enumLinkNext.offset;
        }
    }

/*  Delete the elements.                                                     */

    if ((e = (DB_Enum_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_Enum_t *) 0)
        return MIF_FILE_ERROR;
    elemPos.page = e -> elementLink.page;
    elemPos.offset = e -> elementLink.offset;
    while ((elemPos.page != 0) || (elemPos.offset != 0)) {
        if (DB_enumElementDelete(targetPos, elemPos) != MIF_OKAY)
            return MIF_FILE_ERROR;
        if ((e = (DB_Enum_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
            (DB_Enum_t *) 0)
            return MIF_FILE_ERROR;
        elemPos.page = e -> elementLink.page;
        elemPos.offset = e -> elementLink.offset;
    }

/*
    If we are not deleting an anonymous enumeration, then unlink it from
    the component chain.
*/

    if (componentId != 0) {
        if ((enumLink.page == targetPos.page) &&
            (enumLink.offset == targetPos.offset)) {

/*      The enumeration is at the head of the component list.                */

            if ((c = (DB_Component_t *)
                MIF_resolve(DB, componentPos, MIF_WRITE)) ==
                (DB_Component_t *) 0)
                return MIF_FILE_ERROR; 
            c -> enumLink.page = enumLinkNext.page;
            c -> enumLink.offset = enumLinkNext.offset;
        } else {

/*      The enumeration is not at the head of the component list.            */

            enumPos.page = enumLink.page;
            enumPos.offset = enumLink.offset;
            if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
                (DB_Enum_t *) 0)
                return MIF_FILE_ERROR;
            while ((e -> enumLinkNext.page != targetPos.page) ||
                   (e -> enumLinkNext.offset != targetPos.offset)) {
                enumPos.page = e -> enumLinkNext.page;
                enumPos.offset = e -> enumLinkNext.offset;
                if ((enumPos.page == 0) && (enumPos.offset == 0))
                    return MIF_FILE_ERROR;
                if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
                    (DB_Enum_t *) 0)
                    return MIF_FILE_ERROR;
            }
            if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_WRITE)) ==
                (DB_Enum_t *) 0)
                return MIF_FILE_ERROR;
            e -> enumLinkNext.page = enumLinkNext.page;
            e -> enumLinkNext.offset = enumLinkNext.offset;
        }
    }

/*  Zero out the elements of the enumeration.                                */

    if ((e = (DB_Enum_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Enum_t *) 0)
        return MIF_FILE_ERROR;
    e -> dataType = 0;
    e -> elementCount = 0;
    e -> enumLinkNext.page = 0;
    e -> enumLinkNext.offset = 0;

    return DB_ElementDelete(targetPos);
}

#endif

#ifndef SL_BUILD

MIF_Pos_t DB_enumElementAdd(MIF_Pos_t enumPos, DB_String_t *symbol, void *value)
{
    MIF_Pos_t        nullPos =         {0, 0};
    DB_Enum_t        *en;
    MIF_DataType_t   dataType;
    MIF_Pos_t        elementLink;
    MIF_Pos_t        newElemPos;
    MIF_Pos_t        symbolPos;
    MIF_Int_t        dataSize;
    MIF_Pos_t        dataPos;
    DB_EnumElement_t *el;
    MIF_Pos_t        elemPos;
    
/*  Check for a bad pos.                                                     */

    if ((enumPos.page == 0) && (enumPos.offset == 0))
        return nullPos;

/*  Check to see if the pos references an enum.                              */

    if ((en = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
        (DB_Enum_t *) 0)
        return nullPos;
    if (en -> name.type != MIF_ENUM)
        return nullPos;

    dataType = en -> dataType;
    elementLink.page = en -> elementLink.page;
    elementLink.offset = en -> elementLink.offset;

/*  Build a new element.                                                     */

    newElemPos = MIF_alloc(DB, sizeof(DB_EnumElement_t));
    if ((newElemPos.page == 0) && (newElemPos.offset == 0))
        return nullPos;

/*  Create the symbol string.                                                */

    symbolPos = DB_stringAdd(symbol);
    if ((symbolPos.page == 0) && (symbolPos.offset == 0))
        return nullPos;

/*  Create a data object with the value.                                     */

    dataSize = DB_dataTypeSizeGet(dataType);
    switch (dataSize) {
        case -1:
            return nullPos;
        case 0:
            dataSize = DB_stringSize((DB_String_t *) value); 
            break;
    }

    dataPos = DB_dataAdd(dataType, dataSize, dataSize, dataSize,
                         dataSize, value);
    if ((dataPos.page == 0) && (dataPos.offset == 0))
        return nullPos;

/*  Link the element to the symbol and data object.                          */

    if ((el = (DB_EnumElement_t *) MIF_resolve(DB, newElemPos, MIF_WRITE)) ==
        (DB_EnumElement_t *) 0)
        return nullPos;
    el -> symbolPos.page = symbolPos.page;
    el -> symbolPos.offset = symbolPos.offset;
    el -> dataPos.page = dataPos.page;
    el -> dataPos.offset = dataPos.offset;
    el -> elementLinkNext.page = 0;
    el -> elementLinkNext.offset = 0;

/*  Link the element into the chain.                                         */

    if ((elementLink.page == 0) && (elementLink.offset == 0)) {

/*      This the first element added to the enum.                            */

        if ((en = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_WRITE)) ==
            (DB_Enum_t *) 0)
            return nullPos;
        ++en -> elementCount;
        en -> elementLink.page = newElemPos.page;
        en -> elementLink.offset = newElemPos.offset;
    } else {

/*      There are already elements in this enum.                             */

        elemPos.page = elementLink.page;
        elemPos.offset = elementLink.offset;
        if ((el = (DB_EnumElement_t *) MIF_resolve(DB, elemPos, MIF_READ)) ==
            (DB_EnumElement_t *) 0)
            return nullPos;

/*      Walk the element chain to the end.                                   */

        while ((el -> elementLinkNext.page != 0) ||
               (el -> elementLinkNext.offset != 0)) {
            elemPos.page = el -> elementLinkNext.page;
            elemPos.offset = el -> elementLinkNext.offset;
            if ((el = (DB_EnumElement_t *)
                MIF_resolve(DB, elemPos, MIF_READ)) == (DB_EnumElement_t *) 0)
                return nullPos;
        }

/*      Link the new element to the end of the list.                         */

        if ((el = (DB_EnumElement_t *) MIF_resolve(DB, elemPos, MIF_WRITE)) ==
            (DB_EnumElement_t *) 0)
            return nullPos;
        el -> elementLinkNext.page = newElemPos.page;
        el -> elementLinkNext.offset = newElemPos.offset;

/*      Increment the element count.                                         */

        if ((en = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_WRITE)) ==
            (DB_Enum_t *) 0)
            return nullPos;
        ++en -> elementCount;
    }

    return newElemPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_enumElementDelete(MIF_Pos_t enumPos, MIF_Pos_t targetPos)
{
    DB_Enum_t        *en;
    MIF_Pos_t        elementLink;
    MIF_Pos_t        elemPos;
    DB_EnumElement_t *el;
    MIF_Pos_t        symbolPos;
    MIF_Pos_t        dataPos;
    MIF_Pos_t        elementLinkNext;

/*  Check for a bad pos.                                                     */

    if ((enumPos.page == 0) && (enumPos.offset == 0))
        return MIF_BAD_POS;

/*  Check to see if the pos references an enum.                              */

    if ((en = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
        (DB_Enum_t *) 0)
        return MIF_FILE_ERROR;
    if (en -> name.type != MIF_ENUM)
        return MIF_BAD_POS;
    elementLink.page = en -> elementLink.page;
    elementLink.offset = en -> elementLink.offset;

/*  Find the element in the enum's chain.                                    */

    elemPos.page = elementLink.page;
    elemPos.offset = elementLink.offset;
    while ((elemPos.page != 0) || (elemPos.offset != 0)) {
        if ((elemPos.page == targetPos.page) &&
            (elemPos.offset == targetPos.offset))
            break;
        if ((el = (DB_EnumElement_t *) MIF_resolve(DB, elemPos, MIF_READ)) ==
            (DB_EnumElement_t *) 0)
            return MIF_FILE_ERROR;
        elemPos.page = el -> elementLinkNext.page;
        elemPos.offset = el -> elementLinkNext.offset;
    }
    if ((elemPos.page == 0) && (elemPos.offset == 0))
        return MIF_NOT_FOUND;

/*  Get the contents of the element to be deleted.                           */

    if ((el = (DB_EnumElement_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_EnumElement_t *) 0)
        return MIF_FILE_ERROR;
    symbolPos.page = el -> symbolPos.page;
    symbolPos.offset = el -> symbolPos.offset;
    dataPos.page = el -> dataPos.page;
    dataPos.offset = el -> dataPos.offset;
    elementLinkNext.page = el -> elementLinkNext.page;
    elementLinkNext.offset = el -> elementLinkNext.offset;

/*  Delete the symbol.                                                       */

    if ((symbolPos.page != 0) || (symbolPos.offset != 0))
        if (DB_stringDelete(symbolPos) != MIF_OKAY)
            return MIF_FILE_ERROR;

/*  Delete the data object.                                                  */

    if ((dataPos.page != 0) || (dataPos.offset != 0))
        if (DB_dataDelete(dataPos) != MIF_OKAY)
            return MIF_FILE_ERROR;

/*  Remove the element from the chain.                                       */

    if ((elementLink.page == targetPos.page) &&
        (elementLink.offset == targetPos.offset)) {

/*      The element to be deleted is at the head of the list.                */

        if ((en = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_WRITE)) ==
            (DB_Enum_t *) 0)
            return MIF_FILE_ERROR;
        en -> elementLink.page = elementLinkNext.page;
        en -> elementLink.offset = elementLinkNext.offset;
    } else {

/*      The element is not at the head of the list.                          */

        elemPos.page = elementLink.page;
        elemPos.offset = elementLink.offset;
        if ((el = (DB_EnumElement_t *) MIF_resolve(DB, elemPos, MIF_READ)) ==
            (DB_EnumElement_t *) 0)
            return MIF_FILE_ERROR;
        while ((el -> elementLinkNext.page != targetPos.page) ||
               (el -> elementLinkNext.offset != targetPos.offset)) {
            
            elemPos.page = el -> elementLinkNext.page;
            elemPos.offset = el -> elementLinkNext.offset;
            if ((elemPos.page == 0) && (elemPos.offset == 0))
                return MIF_FILE_ERROR;
            if ((el = (DB_EnumElement_t *)
                MIF_resolve(DB, elemPos, MIF_READ)) == (DB_EnumElement_t *) 0)
                return MIF_FILE_ERROR;
        }
        if ((el = (DB_EnumElement_t *) MIF_resolve(DB, elemPos, MIF_WRITE)) ==
            (DB_EnumElement_t *) 0)
            return MIF_FILE_ERROR;
        el -> elementLinkNext.page = elementLinkNext.page;
        el -> elementLinkNext.offset = elementLinkNext.offset;
    }

/*  Zero out the members of the element.                                     */

    if ((el = (DB_EnumElement_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_EnumElement_t *) 0)
        return MIF_FILE_ERROR;
    el -> symbolPos.page = 0;
    el -> symbolPos.offset = 0;
    el -> dataPos.page = 0;
    el -> dataPos.offset = 0;
    el -> elementLinkNext.page = 0;
    el -> elementLinkNext.offset = 0;

    return MIF_OKAY;
}

#endif


/*
    This procedure is only used to find non-anonymous enumerations, since
    anonymous enumerations are linked to attributes.
*/

#ifndef SL_BUILD

MIF_Pos_t DB_enumFind(MIF_Id_t componentId, DB_String_t *name)
{
    MIF_Pos_t      nullPos        = {0, 0};
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    MIF_Pos_t      enumPos;
    DB_Enum_t      *e;
    MIF_Pos_t      enumLinkNext;
    DB_String_t    *s;

/*  Check the component ID.                                                  */

    if (componentId == 0)
        return nullPos;
    componentPos = DB_componentIdToPos(componentId);
    if ((componentPos.page == 0) && (componentPos.offset == 0))
        return nullPos;

/*  Get the pos of the first enumeration in the chain.                       */

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return nullPos;
    enumPos.page = c -> enumLink.page;
    enumPos.offset = c -> enumLink.offset;
    
/*  Walk the enumeration chain to find the first match.                      */

    while ((enumPos.page != 0) || (enumPos.offset != 0)) {
        if ((e = (DB_Enum_t *) MIF_resolve(DB, enumPos, MIF_READ)) ==
            (DB_Enum_t *) 0)
            return nullPos;
        enumLinkNext.page = e -> enumLinkNext.page;
        enumLinkNext.offset = e -> enumLinkNext.offset;
        if ((s = (DB_String_t *) MIF_resolve(DB, e -> name.name, MIF_READ)) ==
            (DB_String_t *) 0)
            return nullPos;
        if (DB_stringCmp(s, name) == 0)
            return enumPos;
        enumPos.page = enumLinkNext.page;
        enumPos.offset = enumLinkNext.offset;
    }

    return nullPos;
}

#endif
