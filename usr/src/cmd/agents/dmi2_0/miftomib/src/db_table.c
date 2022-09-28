/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_table.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_table.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Databse Rotines for Static Tables

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_table.c 1.7 1994/04/07 09:34:12 apivowar Exp $

    RCS Revision: $Header: n:/mif/db/rcs/db_table.c 1.7 1994/04/07 09:34:12 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        6/7/93   aip    Creation date.
		06/24/93 sfh	Removed tableElementRowFind & tableElementColFind
						prototypes and added to db_int.h
        7/29/93  aip    Introduced MIF_Unsigned_t where appropriate.
        4/6/94   aip    Added 32-bit string support.

************************* INCLUDES ***********************************/
/*********************************************************************/

/************************ PROTOTYPES *********************************/

static MIF_Status_t DB_tableInit(MIF_Pos_t groupPos);
static MIF_Pos_t    DB_tableColumnAdd(MIF_Pos_t tablePos,
                                      MIF_Unsigned_t columnNumber);
static MIF_Pos_t	DB_tableColumnFind(MIF_Pos_t tablePos,
                                       MIF_Unsigned_t columnNumber);
static MIF_Pos_t    DB_tableColumnInsertionPointFind(MIF_Pos_t tablePos,
                                                 MIF_Unsigned_t columnNumber);
static MIF_Pos_t    DB_tableElementColumnInsertionPointFind(MIF_Pos_t tablePos,
                        MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber);
static MIF_Pos_t    DB_tableElementRowInsertionPointFind(MIF_Pos_t tablePos,
                        MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber);
static MIF_Pos_t    DB_tableRowAdd(MIF_Pos_t tablePos,
                                   MIF_Unsigned_t rowNumber);
static MIF_Pos_t    DB_tableRowInsertionPointFind(MIF_Pos_t tablePos,
                                                  MIF_Unsigned_t rowNumber);

/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

#ifdef _WIN32
#pragma optimize( "g", off )  // turn off global optimization to avoid MSVC compiler bug
#endif

MIF_Pos_t DB_tableAdd(MIF_Id_t componentId, MIF_Id_t groupId, DB_String_t *name)
{
    MIF_Pos_t      nullPos        = {0, 0};
    MIF_Pos_t      componentPos;
    MIF_Pos_t      groupPos;
    DB_Component_t *c;
    MIF_Pos_t      tablePos;
    DB_Table_t     *t;
    DB_Group_t     *g;
    MIF_Pos_t      newTablePos;

/*  Check the component ID.                                                  */

    if (componentId == 0)
        return nullPos;
    else {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return nullPos;
    }

/*  Check the group ID.                                                      */

    if (groupId == 0)
        return nullPos;
    else {
        groupPos = DB_groupIdToPos(componentId, groupId);
        if ((groupPos.page == 0) && (groupPos.offset == 0))
            return nullPos;
    }

/*  Check the table chain to ensure that this is a unique ID.                */

    tablePos = DB_tableFind(componentId, groupId);
    if ((tablePos.page != 0) || (tablePos.offset != 0))
        return nullPos;

/*  Check the group to ensure that no other table is referenced.             */

    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return nullPos;
    if ((g -> tableLink.page != 0) || (g -> tableLink.offset != 0))
        return nullPos;
 
/*  Create a new Static Table.                                               */

    newTablePos = DB_ElementAdd(name, sizeof(DB_Table_t));
    if ((newTablePos.page == 0) && (newTablePos.offset == 0))
        return nullPos;

/*  Link it into the component.                                              */

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return nullPos;
    tablePos.page = c -> tableLink.page;
    tablePos.offset = c -> tableLink.offset;
    if ((tablePos.page != 0) || (tablePos.offset != 0)) {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
            (DB_Table_t *) 0)
            return nullPos;
        while ((t -> tableLinkNext.page != 0) ||
               (t -> tableLinkNext.offset != 0)) {
            tablePos.page = t -> tableLinkNext.page;
            tablePos.offset = t -> tableLinkNext.offset;
            if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
                (DB_Table_t *) 0)
                return nullPos;
        }
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
            (DB_Table_t *) 0)
            return nullPos;
        t -> tableLinkNext.page = newTablePos.page;
        t -> tableLinkNext.offset = newTablePos.offset;
    } else {
        if ((c = (DB_Component_t *)
            MIF_resolve(DB, componentPos, MIF_WRITE)) == (DB_Component_t *) 0)
            return nullPos;
        c -> tableLink.page = newTablePos.page;
        c -> tableLink.offset = newTablePos.offset;
    }

/*  Link the group to the table.                                             */

    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
        (DB_Group_t *) 0)
        return nullPos;
    g -> tableLink.page = newTablePos.page;
    g -> tableLink.offset = newTablePos.offset;

/*  Initialize the fields of the Static Table.                               */

    if ((t = (DB_Table_t *) MIF_resolve(DB, newTablePos, MIF_WRITE)) ==
        (DB_Table_t *) 0)
        return nullPos;
    t -> name.type = MIF_TABLE;
    t -> id = groupId;
    t -> columnCount = 0;
    t -> rowCount = 0;
    t -> columnLink.page = 0;
    t -> columnLink.offset = 0;
    t -> rowLink.page = 0;
    t -> rowLink.offset = 0;
    t -> tableLinkNext.page = 0;
    t -> tableLinkNext.offset = 0;

/*  Build the column headers for the table.                                  */

    if (DB_tableInit(groupPos) != MIF_OKAY)
        return nullPos;

    return newTablePos;
}

#ifdef _WIN32
#pragma optimize( "", on )  // return to previous settings
#endif

#endif

#ifndef SL_BUILD

static MIF_Pos_t DB_tableColumnAdd(MIF_Pos_t tablePos,
                                   MIF_Unsigned_t columnNumber)
{
    MIF_Pos_t        nullPos        = {0, 0};
    DB_Table_t       *t;
    MIF_Pos_t        columnPos;
    MIF_Pos_t        columnLink;
    DB_TableHeader_t *h;
    MIF_Pos_t        newHeaderPos;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the column number.                                                 */

    if (columnNumber == 0)
        return nullPos;
    columnPos = DB_tableColumnFind(tablePos, columnNumber);
    if ((columnPos.page != 0) || (columnPos.offset != 0))
        return nullPos;   

/*  Create a new table header object.                                        */

    newHeaderPos = MIF_alloc(DB, sizeof(DB_TableHeader_t));
    if ((newHeaderPos.page == 0) && (newHeaderPos.offset == 0))
        return nullPos;

/*  Link the new header object into the list.                                */

    columnPos = DB_tableColumnInsertionPointFind(tablePos, columnNumber);
    if ((columnPos.page == 0) && (columnPos.offset == 0)) {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
            (DB_Table_t *) 0)
            return nullPos;
        columnLink.page = t -> columnLink.page;
        columnLink.offset = t -> columnLink.offset;
        t -> columnLink.page = newHeaderPos.page;
        t -> columnLink.offset = newHeaderPos.offset;
    } else {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, columnPos, MIF_WRITE)) ==
            (DB_TableHeader_t *) 0)
            return nullPos;
        columnLink.page = h -> linkNext.page;
        columnLink.offset = h -> linkNext.offset;
        h -> linkNext.page = newHeaderPos.page;
        h -> linkNext.offset = newHeaderPos.offset;
    }

/*  Increment the table column count.                                        */

    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
        (DB_Table_t *) 0)
        return nullPos;
    ++ (t -> columnCount);

/*  Initialize the fields of the header object.                              */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, newHeaderPos, MIF_WRITE)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    h -> number = columnNumber;
    h -> link.page = 0;
    h -> link.offset = 0;
    h -> linkNext.page = columnLink.page;
    h -> linkNext.offset = columnLink.offset;

    return newHeaderPos;
}

#endif


static MIF_Pos_t DB_tableColumnFind(MIF_Pos_t tablePos,
                                    MIF_Unsigned_t columnNumber)
{
    MIF_Pos_t        nullPos     = {0, 0};
    DB_Table_t       *t;
    static MIF_Pos_t        headerPos;
    DB_TableHeader_t *h;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Get the column header pos.                                               */

    headerPos.page = t -> columnLink.page;
    headerPos.offset = t -> columnLink.offset;

/*  Find the column in the list.                                             */

    while ((headerPos.page != 0) || (headerPos.offset != 0)) {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, headerPos, MIF_READ)) ==
            (DB_TableHeader_t *) 0)
            return nullPos;
        if (columnNumber == h -> number)
            return headerPos;
        headerPos.page = h -> linkNext.page;
        headerPos.offset = h -> linkNext.offset;
    }

    return nullPos;
}


#ifndef SL_BUILD

static MIF_Pos_t DB_tableColumnInsertionPointFind(MIF_Pos_t tablePos,
                                                  MIF_Unsigned_t columnNumber)
{
    MIF_Pos_t        nullPos     = {0, 0};
    DB_Table_t       *t;
    static MIF_Pos_t        headerPos;
    DB_TableHeader_t *h;
    MIF_Pos_t        link;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Get the head of the column list.                                         */

    headerPos.page = t -> columnLink.page;
    headerPos.offset = t -> columnLink.offset;
    if ((headerPos.page == 0) && (headerPos.offset == 0))
        return nullPos;

/*  Get the successor.                                                       */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, headerPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    if (columnNumber < h -> number)
        return nullPos;
    link.page = h -> linkNext.page;
    link.offset = h -> linkNext.offset;
    if ((link.page == 0) && (link.offset == 0))
        return headerPos;
    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, link, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;

/*  Walk the list until the insertion point is found.                        */

    while ((columnNumber > h -> number) &&
           ((link.page != 0) || (link.offset != 0))) {
        headerPos.page = link.page;
        headerPos.offset = link.offset;
        link.page = h -> linkNext.page;
        link.offset = h -> linkNext.offset;
        if ((link.page != 0) || (link.offset != 0))
            if ((h = (DB_TableHeader_t *) MIF_resolve(DB, link, MIF_READ)) ==
                (DB_TableHeader_t *) 0)
                return nullPos;
    }

    return headerPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_tableDelete(MIF_Id_t componentId, MIF_Pos_t targetPos)
{
    DB_Table_t        *t;
    MIF_Pos_t         columnPos;
    MIF_Pos_t         rowPos;
    MIF_Pos_t         componentPos;
    DB_Component_t    *c;
    MIF_Bool_t        tableFound;
    MIF_Pos_t         tablePos;
    DB_TableHeader_t  *h;
    MIF_Pos_t         elementPos;
    DB_TableElement_t *e;
    MIF_Pos_t         dataPos;
    MIF_Pos_t         next;
    MIF_Pos_t         groupPos;
    DB_Group_t        *g;

/*  Check the tablePos.                                                      */

    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;
    if ((t = (DB_Table_t *) MIF_resolve(DB, targetPos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return MIF_FILE_ERROR;
    if (t -> name.type != MIF_TABLE)
        return MIF_BAD_POS;

/*  Get the column and row header positions.                                 */

    columnPos.page = t -> columnLink.page;
    columnPos.offset = t -> columnLink.offset;
    rowPos.page = t -> rowLink.page;
    rowPos.offset = t -> rowLink.offset;

/*  Check the componentId.                                                   */

    if (componentId == 0)
        return MIF_BAD_ID;
    else {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return MIF_BAD_ID;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
    }

/*  Check to see if the table is on this component's table list.             */

    tableFound = MIF_FALSE;
    tablePos.page = c -> tableLink.page;
    tablePos.offset = c -> tableLink.offset;
    while ((tablePos.page != 0) || (tablePos.offset != 0))
        if ((targetPos.page == tablePos.page) &&
            (targetPos.offset == tablePos.offset)) {
            tableFound = MIF_TRUE;
            break;
        }
    if (! tableFound)
        return MIF_BAD_POS;

/*  Step 1.  Delete the column headers.                                      */

    while ((columnPos.page != 0) || (columnPos.offset != 0)) {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, columnPos, MIF_WRITE)) ==
            (DB_TableHeader_t *) 0)
            return MIF_FILE_ERROR;
        columnPos.page = h -> linkNext.page;
        columnPos.offset = h -> linkNext.offset;
        h -> number = 0;
        h -> link.page = 0;
        h -> link.offset = 0;
        h -> linkNext.page = 0;
        h -> linkNext.offset = 0;
    }

/*  Step 2.  Delete the row headers, rows, elements, and data objects.       */

    while ((rowPos.page != 0) || (rowPos.offset != 0)) {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_WRITE)) ==
            (DB_TableHeader_t *) 0)
            return MIF_FILE_ERROR;
        elementPos.page = h -> link.page;
        elementPos.offset = h -> link.offset;
        rowPos.page = h -> linkNext.page;
        rowPos.offset = h -> linkNext.offset;
        h -> number = 0;
        h -> link.page = 0;
        h -> link.offset = 0;
        h -> linkNext.page = 0;
        h -> linkNext.offset = 0;
        while ((elementPos.page != 0) || (elementPos.offset != 0)) {
            if ((e = (DB_TableElement_t *)
                MIF_resolve(DB, elementPos, MIF_WRITE)) ==
                (DB_TableElement_t *) 0)
                return MIF_FILE_ERROR;
            dataPos.page = e -> dataPos.page;
            dataPos.offset = e -> dataPos.offset;
            elementPos.page = e -> columnLinkNext.page;
            elementPos.offset = e -> columnLinkNext.offset;
            e -> columnNumber = 0;
            e -> rowNumber = 0;
            e -> dataPos.page = 0;
            e -> dataPos.offset = 0;
            e -> columnLinkNext.page = 0;
            e -> columnLinkNext.offset = 0;
            e -> rowLinkNext.page = 0;
            e -> rowLinkNext.offset = 0;
            if (DB_dataDelete(dataPos) != MIF_OKAY)
                return MIF_FILE_ERROR;
        }
    }

/*  Step 3.  Unlink the table object from the component list.                */

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return MIF_FILE_ERROR;
    tablePos.page = c -> tableLink.page;
    tablePos.offset = c -> tableLink.offset;
    if ((targetPos.page == tablePos.page) &&
        (targetPos.offset == tablePos.offset)) {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
            (DB_Table_t *) 0)
            return MIF_FILE_ERROR;
        tablePos.page = t -> tableLinkNext.page;
        tablePos.offset = t -> tableLinkNext.offset;
        if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_WRITE)) ==
            (DB_Component_t *) 0)
            return MIF_FILE_ERROR;
        c -> tableLink.page = tablePos.page;
        c -> tableLink.offset = tablePos.offset;
    } else {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
            (DB_Table_t *) 0)
            return MIF_FILE_ERROR;
        next.page = t -> tableLinkNext.page;
        next.offset = t -> tableLinkNext.offset;
        while ((targetPos.page != next.page) ||
               (targetPos.offset != next.offset)) {
            tablePos.page = next.page;
            tablePos.offset = next.offset;
            if ((tablePos.page == 0) && (tablePos.offset == 0))
                return MIF_FILE_ERROR;
            if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
                (DB_Table_t *) 0)
                return MIF_FILE_ERROR;
            next.page = t -> tableLinkNext.page;
            next.offset = t -> tableLinkNext.offset;
        }
        if ((t = (DB_Table_t *) MIF_resolve(DB, next, MIF_READ)) ==
            (DB_Table_t *) 0)
            return MIF_FILE_ERROR;
        next.page = t -> tableLinkNext.page;
        next.offset = t -> tableLinkNext.offset;
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
            (DB_Table_t *) 0)
            return MIF_FILE_ERROR;
        t -> tableLinkNext.page = next.page;
        t -> tableLinkNext.offset = next.offset;
    }

/*  Step 4.  Unlink the table object from the groups.                        */

    groupPos = DB_groupFindFirst(componentId);
    while ((groupPos.page != 0) || (groupPos.offset != 0)) {
        if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
            (DB_Group_t *) 0)
            return MIF_FILE_ERROR;
        tablePos.page = g -> tableLink.page;
        tablePos.offset = g -> tableLink.offset;
        if ((targetPos.page == tablePos.page) &&
            (targetPos.offset == tablePos.offset)) {
            if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) ==
                (DB_Group_t *) 0)
                return MIF_FILE_ERROR;
            g -> tableLink.page = 0;
            g -> tableLink.offset = 0;
        }
        groupPos = DB_groupFindNext(groupPos);
    }

/*  Step 5.  Delete the table object.                                        */

    if ((t = (DB_Table_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Table_t *) 0)
    t -> id = 0;
    t -> columnCount = 0;
    t -> rowCount = 0;
    t -> columnLink.page = 0;
    t -> columnLink.offset = 0;
    t -> rowLink.page = 0;
    t -> rowLink.offset = 0;
    t -> tableLinkNext.page = 0;
    t -> tableLinkNext.offset = 0;

    return DB_ElementDelete(targetPos);
}

#endif

#ifndef SL_BUILD

MIF_Pos_t DB_tableElementAdd(MIF_Pos_t tablePos, MIF_Unsigned_t columnNumber,
                             MIF_Unsigned_t rowNumber, MIF_Pos_t dataPos)
{
    MIF_Pos_t         nullPos          = {0, 0};
    DB_Table_t        *t;
    MIF_Pos_t         columnLink;
    MIF_Pos_t         rowLink;
    static MIF_Pos_t  newElementPos;
    MIF_Pos_t         rowPos;
    MIF_Pos_t         elementPos;
    DB_TableHeader_t  *h;
    MIF_Pos_t         columnLinkNext;
    DB_TableElement_t *e;
    MIF_Pos_t         columnPos;
    MIF_Pos_t         rowLinkNext;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Get the header positions.                                                */

    columnLink.page = t -> columnLink.page;
    columnLink.offset = t -> columnLink.offset;
    rowLink.page = t -> rowLink.page;
    rowLink.offset = t -> rowLink.offset;

/*  Create a new table element.                                              */

    newElementPos = MIF_alloc(DB, sizeof(DB_TableElement_t));
    if ((newElementPos.page == 0) && (newElementPos.offset == 0))
        return nullPos;

/*  Link into the row list.                                                  */

    rowPos = DB_tableRowFind(tablePos, rowNumber);
    if ((rowPos.page == 0) && (rowPos.offset == 0)) {
        rowPos = DB_tableRowAdd(tablePos, rowNumber);
        if ((rowPos.page == 0) && (rowPos.offset == 0))
            return nullPos;
    }
    elementPos = DB_tableElementRowFind(tablePos, columnNumber, rowNumber);
    if ((elementPos.page != 0) || (elementPos.offset != 0))
        return nullPos;
    elementPos = DB_tableElementRowInsertionPointFind(tablePos, columnNumber,
                                                      rowNumber);
    if ((elementPos.page == 0) && (elementPos.offset == 0)) {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_WRITE)) ==
            (DB_TableHeader_t *) 0)
            return nullPos;
        columnLinkNext.page = h -> link.page;
        columnLinkNext.offset = h -> link.offset;
        h -> link.page = newElementPos.page;
        h -> link.offset = newElementPos.offset;
    } else {
        if ((e = (DB_TableElement_t *)
            MIF_resolve(DB, elementPos, MIF_WRITE)) == (DB_TableElement_t *) 0)
            return nullPos;
        columnLinkNext.page = e -> columnLinkNext.page;
        columnLinkNext.offset = e -> columnLinkNext.offset;
        e -> columnLinkNext.page = newElementPos.page;
        e -> columnLinkNext.offset = newElementPos.offset;
    }

/*  Link into the column list.                                               */

    rowLinkNext.page = 0;
    rowLinkNext.offset = 0;
    if ((columnLink.page != 0) || (columnLink.offset != 0)) {
        columnPos = DB_tableColumnFind(tablePos, columnNumber);
        if ((columnPos.page != 0) || (columnPos.offset != 0)) {
            elementPos = DB_tableElementColumnFind(tablePos, columnNumber,
                                                   rowNumber);
            if ((elementPos.page != 0) || (elementPos.offset != 0))
                return nullPos;
            elementPos = DB_tableElementColumnInsertionPointFind(tablePos,
                             columnNumber, rowNumber);
            if ((elementPos.page == 0) && (elementPos.offset == 0)) {
                if ((h = (DB_TableHeader_t *)
                    MIF_resolve(DB, columnPos, MIF_WRITE)) ==
                    (DB_TableHeader_t *) 0)
                    return nullPos;
                rowLinkNext.page = h -> link.page;
                rowLinkNext.offset = h -> link.offset;
                h -> link.page = newElementPos.page;
                h -> link.offset = newElementPos.offset;
            } else {
                if ((e = (DB_TableElement_t *)
                    MIF_resolve(DB, elementPos, MIF_WRITE)) ==
                    (DB_TableElement_t *) 0)
                    return nullPos;
                rowLinkNext.page = e -> rowLinkNext.page;
                rowLinkNext.offset = e -> rowLinkNext.offset;
                e -> rowLinkNext.page = newElementPos.page;
                e -> rowLinkNext.offset = newElementPos.offset;
            }
        }
    }

/*  Initialize the fields of the table element.                              */

    if ((e = (DB_TableElement_t *) MIF_resolve(DB, newElementPos, MIF_WRITE)) ==
        (DB_TableElement_t *) 0)
        return nullPos;
    e -> columnNumber = columnNumber;
    e -> rowNumber = rowNumber;
    e -> dataPos.page = dataPos.page;
    e -> dataPos.offset = dataPos.offset;
    e -> columnLinkNext.page = columnLinkNext.page;
    e -> columnLinkNext.offset = columnLinkNext.offset;
    e -> rowLinkNext.page = rowLinkNext.page;
    e -> rowLinkNext.offset = rowLinkNext.offset;

    return newElementPos;
}

#endif

MIF_Pos_t DB_tableElementColumnFind(MIF_Pos_t tablePos,
                     MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t         nullPos      = {0, 0};
    DB_Table_t        *t;
    MIF_Pos_t         columnPos;
    DB_TableHeader_t  *h;
    static MIF_Pos_t  elementPos;
    DB_TableElement_t *e;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the column number.                                                 */

    if (columnNumber == 0)
        return nullPos;
    else {
        columnPos = DB_tableColumnFind(tablePos, columnNumber);
        if ((columnPos.page == 0) && (columnPos.offset == 0))
            return nullPos;
    }

/*  Get the head of the list.                                                */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, columnPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    elementPos.page = h -> link.page;
    elementPos.offset = h -> link.offset;

/*  Find the row number in the list.                                         */

    while ((elementPos.page != 0) || (elementPos.offset != 0)) {
        if ((e = (DB_TableElement_t *) MIF_resolve(DB, elementPos, MIF_READ)) ==
            (DB_TableElement_t *) 0)
            return nullPos;
        if (rowNumber == e -> rowNumber)
            return elementPos;
        elementPos.page = e -> rowLinkNext.page;
        elementPos.offset = e -> rowLinkNext.offset;
    }

    return nullPos;
}


#ifndef SL_BUILD

static MIF_Pos_t DB_tableElementColumnInsertionPointFind(MIF_Pos_t tablePos,
                     MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t         nullPos       = {0, 0};
    DB_Table_t        *t;
    MIF_Pos_t         columnPos;
    DB_TableHeader_t  *h;
    static MIF_Pos_t  elementPos;
    DB_TableElement_t *e;
    MIF_Pos_t         rowLinkNext;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the column number.                                                 */

    if (columnNumber == 0)
        return nullPos;
    else {
        columnPos = DB_tableColumnFind(tablePos, columnNumber);
        if ((columnPos.page == 0) && (columnPos.offset == 0))
            return nullPos;
    }

/*  Get the head of the column list.                                         */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, columnPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    elementPos.page = h -> link.page;
    elementPos.offset = h -> link.offset;
    if ((elementPos.page == 0) && (elementPos.offset == 0))
        return nullPos;

/*  Get the successor.                                                       */

    if ((e = (DB_TableElement_t *) MIF_resolve(DB, elementPos, MIF_READ)) ==
        (DB_TableElement_t *) 0)
        return nullPos;
    if (rowNumber < e -> rowNumber)
        return nullPos;
    rowLinkNext.page = e -> rowLinkNext.page;
    rowLinkNext.offset = e -> rowLinkNext.offset;
    if ((rowLinkNext.page == 0) && (rowLinkNext.offset == 0))
        return elementPos;
    if ((e = (DB_TableElement_t *) MIF_resolve(DB, rowLinkNext, MIF_READ)) ==
        (DB_TableElement_t *) 0)
        return nullPos;

/*  Walk the list until the insertion point is found.                        */

    while ((rowNumber > e -> rowNumber) &&
           ((rowLinkNext.page != 0) || (rowLinkNext.offset != 0))) {
        elementPos.page = rowLinkNext.page;
        elementPos.offset = rowLinkNext.offset;
        rowLinkNext.page = e -> rowLinkNext.page;
        rowLinkNext.offset = e -> rowLinkNext.offset;
        if ((rowLinkNext.page != 0) || (rowLinkNext.offset != 0))
            if ((e = (DB_TableElement_t *)
                MIF_resolve(DB, rowLinkNext, MIF_READ)) ==
                (DB_TableElement_t *) 0)
                return nullPos;
    }

    return elementPos;
}

#endif


MIF_Pos_t DB_tableElementRowFind(MIF_Pos_t tablePos,
                     MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t         nullPos      = {0, 0};
    DB_Table_t        *t;
    MIF_Pos_t         rowPos;
    DB_TableHeader_t  *h;
    static MIF_Pos_t  elementPos;
    DB_TableElement_t *e;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the row number.                                                    */

    if (rowNumber == 0)
        return nullPos;
    else {
        rowPos = DB_tableRowFind(tablePos, rowNumber);
        if ((rowPos.page == 0) && (rowPos.offset == 0))
            return nullPos;
    }

/*  Get the head of the list.                                                */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    elementPos.page = h -> link.page;
    elementPos.offset = h -> link.offset;

/*  Find the column number in the list.                                      */

    while ((elementPos.page != 0) || (elementPos.offset != 0)) {
        if ((e = (DB_TableElement_t *) MIF_resolve(DB, elementPos, MIF_READ)) ==
            (DB_TableElement_t *) 0)
            return nullPos;
        if (columnNumber == e -> columnNumber)
            return elementPos;
        elementPos.page = e -> columnLinkNext.page;
        elementPos.offset = e -> columnLinkNext.offset;
    }

    return nullPos;
}


#ifndef SL_BUILD

static MIF_Pos_t DB_tableElementRowInsertionPointFind(MIF_Pos_t tablePos,
                     MIF_Unsigned_t columnNumber, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t         nullPos          = {0, 0};
    DB_Table_t        *t;
    MIF_Pos_t         rowPos;
    DB_TableHeader_t  *h;
    static MIF_Pos_t  elementPos;
    DB_TableElement_t *e;
    MIF_Pos_t         columnLinkNext;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the row number.                                                    */

    if (rowNumber == 0)
        return nullPos;
    else {
        rowPos = DB_tableRowFind(tablePos, rowNumber);
        if ((rowPos.page == 0) && (rowPos.offset == 0))
            return nullPos;
    }

/*  Get the head of the row list.                                            */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    elementPos.page = h -> link.page;
    elementPos.offset = h -> link.offset;
    if ((elementPos.page == 0) && (elementPos.offset == 0))
        return nullPos;

/*  Get the successor.                                                       */

    if ((e = (DB_TableElement_t *) MIF_resolve(DB, elementPos, MIF_READ)) ==
        (DB_TableElement_t *) 0)
        return nullPos;
    if (columnNumber < e -> columnNumber)
        return nullPos;
    columnLinkNext.page = e -> columnLinkNext.page;
    columnLinkNext.offset = e -> columnLinkNext.offset;
    if ((columnLinkNext.page == 0) && (columnLinkNext.offset == 0))
        return elementPos;
    if ((e = (DB_TableElement_t *) MIF_resolve(DB, columnLinkNext, MIF_READ)) ==
        (DB_TableElement_t *) 0)
        return nullPos;

/*  Walk the list until the insertion point is found.                        */

    while ((columnNumber > e -> columnNumber) &&
           ((columnLinkNext.page != 0) || (columnLinkNext.offset != 0))) {
        elementPos.page = columnLinkNext.page;
        elementPos.offset = columnLinkNext.offset;
        columnLinkNext.page = e -> columnLinkNext.page;
        columnLinkNext.offset = e -> columnLinkNext.offset;
        if ((columnLinkNext.page != 0) || (columnLinkNext.offset != 0))
            if ((e = (DB_TableElement_t *)
                MIF_resolve(DB, columnLinkNext, MIF_READ)) ==
                (DB_TableElement_t *) 0)
                return nullPos;
    }

    return elementPos;
}

#endif

#ifndef SL_BUILD

MIF_Pos_t DB_tableFind(MIF_Id_t componentId, MIF_Id_t tableId)
{
    MIF_Pos_t      nullPos        = {0, 0};
    MIF_Pos_t      componentPos;
    DB_Component_t *c;
    static MIF_Pos_t      tablePos;
    DB_Table_t     *t;

/*  Check the component ID.                                                  */

    if (componentId == 0)
        return nullPos;
    else {
        componentPos = DB_componentIdToPos(componentId);
        if ((componentPos.page == 0) && (componentPos.offset == 0))
            return nullPos;
    }

/*  Get the head of the table list.                                          */

    if ((c = (DB_Component_t *) MIF_resolve(DB, componentPos, MIF_READ)) ==
        (DB_Component_t *) 0)
        return nullPos;
    tablePos.page = c -> tableLink.page;
    tablePos.offset = c -> tableLink.offset;

/*  Find the table in the list.                                              */

    while ((tablePos.page != 0) || (tablePos.offset != 0)) {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
            (DB_Table_t *) 0)
            return nullPos;
        if (tableId == t -> id)
            return tablePos;
        tablePos.page = t -> tableLinkNext.page;
        tablePos.offset = t -> tableLinkNext.offset;
    }

    return nullPos;
}

#endif

#ifndef SL_BUILD

static MIF_Status_t DB_tableInit(MIF_Pos_t groupPos)
{
    DB_Group_t      *g;
    MIF_Pos_t       attributePos;
    MIF_Pos_t       keyPos;
    MIF_Pos_t       tablePos;
    DB_Attribute_t  *a;
    MIF_Id_t        id;
    MIF_Pos_t       columnPos;

/*  Check the groupPos.                                                      */

    if ((groupPos.page == 0) && (groupPos.offset == 0))
        return MIF_BAD_POS;
    if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_READ)) ==
        (DB_Group_t *) 0)
        return MIF_FILE_ERROR;

/*  If this group has no associated Static Table then something went wrong.  */

    tablePos.page = g -> tableLink.page;
    tablePos.offset = g -> tableLink.offset;
    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return MIF_BAD_POS;

/*  If there are no keyed attributes, then there are no columns in the table. */

    keyPos.page = g -> key.page;
    keyPos.offset = g -> key.offset;
    if ((keyPos.page == 0) && (keyPos.offset == 0))
        return MIF_OKAY;

/*
    Walk the attribute list, adding a column to the table for each keyed
    attribute.
*/

    attributePos.page = g -> attributeLink.page;
    attributePos.offset = g -> attributeLink.offset;
    while (attributePos.page != 0){    /** && (attributePos.offset != 0)) {    PAUL ***/
        if ((a = (DB_Attribute_t *) MIF_resolve(DB, attributePos, MIF_READ)) ==
            (DB_Attribute_t *) 0)
            return MIF_FILE_ERROR;
        id = a -> id;
        if (DB_keyFind(keyPos, id)) {
            columnPos = DB_tableColumnAdd(tablePos, id);
            if ((columnPos.page == 0) && (columnPos.offset == 0))
                return MIF_FILE_ERROR;
        }
        attributePos = DB_attributeFindNext(attributePos);
    }

    return MIF_OKAY;
}

#endif

#ifndef SL_BUILD

static MIF_Pos_t DB_tableRowAdd(MIF_Pos_t tablePos, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t        nullPos        = {0, 0};
    DB_Table_t       *t;
    MIF_Pos_t        rowPos;
    MIF_Pos_t        rowLink;
    DB_TableHeader_t *h;
    static MIF_Pos_t newHeaderPos;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Check the row number.                                                    */

    if (rowNumber == 0)
        return nullPos;
    else {
        rowPos = DB_tableRowFind(tablePos, rowNumber);
        if ((rowPos.page != 0) || (rowPos.offset != 0))
            return nullPos;
    }

/*  Create a new table header object.                                        */

    newHeaderPos = MIF_alloc(DB, sizeof(DB_TableHeader_t));
    if ((newHeaderPos.page == 0) && (newHeaderPos.offset == 0))
        return nullPos;

/*  Link the new header object into the list.                                */

    rowPos = DB_tableRowInsertionPointFind(tablePos, rowNumber);
    if ((rowPos.page == 0) && (rowPos.offset == 0)) {
        if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
            (DB_Table_t *) 0)
            return nullPos;
        rowLink.page = t -> rowLink.page;
        rowLink.offset = t -> rowLink.offset;
        t -> rowLink.page = newHeaderPos.page;
        t -> rowLink.offset = newHeaderPos.offset;
    } else {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_WRITE)) ==
            (DB_TableHeader_t *) 0)
            return nullPos;
        rowLink.page = h -> linkNext.page;
        rowLink.offset = h -> linkNext.offset;
        h -> linkNext.page = newHeaderPos.page;
        h -> linkNext.offset = newHeaderPos.offset;
    }

/*  Increment the table row count.                                           */

    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_WRITE)) ==
        (DB_Table_t *) 0)
        return nullPos;
    ++ (t -> rowCount);

/*  Initialize the fields of the header object.                              */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, newHeaderPos, MIF_WRITE)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    h -> number = rowNumber;
    h -> link.page = 0;
    h -> link.offset = 0;
    h -> linkNext.page = rowLink.page;
    h -> linkNext.offset = rowLink.offset;

    return newHeaderPos;
}

#endif

#ifndef SL_BUILD

static MIF_Pos_t DB_tableRowInsertionPointFind(MIF_Pos_t tablePos,
                                               MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t        nullPos     = {0, 0};
    DB_Table_t       *t;
    static MIF_Pos_t headerPos;
    DB_TableHeader_t *h;
    MIF_Pos_t        link;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Get the head of the row list.                                            */

    headerPos.page = t -> rowLink.page;
    headerPos.offset = t -> rowLink.offset;
    if ((headerPos.page == 0) && (headerPos.offset == 0))
        return nullPos;

/*  Get the successor.                                                       */

    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, headerPos, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;
    if (rowNumber < h -> number)
        return nullPos;
    link.page = h -> linkNext.page;
    link.offset = h -> linkNext.offset;
    if ((link.page == 0) && (link.offset == 0))
        return headerPos;
    if ((h = (DB_TableHeader_t *) MIF_resolve(DB, link, MIF_READ)) ==
        (DB_TableHeader_t *) 0)
        return nullPos;

/*  Walk the list until the insertion point is found.                        */

    while ((rowNumber > h -> number) &&
           ((link.page != 0) || (link.offset != 0))) {
        headerPos.page = link.page;
        headerPos.offset = link.offset;
        link.page = h -> linkNext.page;
        link.offset = h -> linkNext.offset;
        if ((link.page != 0) || (link.offset != 0))
            if ((h = (DB_TableHeader_t *) MIF_resolve(DB, link, MIF_READ)) ==
                (DB_TableHeader_t *) 0)
                return nullPos;
    }

    return headerPos;
}

#endif

MIF_Pos_t DB_tableRowFind(MIF_Pos_t tablePos, MIF_Unsigned_t rowNumber)
{
    MIF_Pos_t        nullPos  = {0, 0};
    DB_Table_t       *t;
    static MIF_Pos_t rowPos;
    DB_TableHeader_t *h;

/*  Check the tablePos.                                                      */

    if ((tablePos.page == 0) && (tablePos.offset == 0))
        return nullPos;
    if ((t = (DB_Table_t *) MIF_resolve(DB, tablePos, MIF_READ)) ==
        (DB_Table_t *) 0)
        return nullPos;
    if (t -> name.type != MIF_TABLE)
        return nullPos;

/*  Get the head of the row list.                                            */

    rowPos.page = t -> rowLink.page;
    rowPos.offset = t -> rowLink.offset;

/*  Find the row in the list.                                                */

    while ((rowPos.page != 0) || (rowPos.offset != 0)) {
        if ((h = (DB_TableHeader_t *) MIF_resolve(DB, rowPos, MIF_READ)) ==
            (DB_TableHeader_t *) 0)
            return nullPos;
        if (rowNumber == h -> number)
            return rowPos;
        rowPos.page = h -> linkNext.page;
        rowPos.offset = h -> linkNext.offset;
    }

    return nullPos;
}

