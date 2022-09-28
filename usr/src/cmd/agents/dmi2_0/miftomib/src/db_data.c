/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_data.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_data.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Database Routines for Data Types

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_data.c 1.17 1994/04/07 09:33:58 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        5/13/93  aip    Added DB_dataTypeSizeGet
        5/14/93  aip    Changed DB_dataAdd to use the maximum of maxSize
                        and currentSize when allocating space.
		05/18/93 sfh	Revise to use MIF_String_t instead of char[]
						for strings;
		05/21/93 sfh	Remove conditional compilation around
						dataTypeSizeGet.
        5/25/93  aip    Changed names of DB string functions to be consistent
                        with other object add/delete interfaces.
        6/22/93  aip    Added DB_dataCopy.
        6/25/93  aip    Made changes to support new data object.
		07/20/93 sfh	Change type of parameter for dataTypewSizeGet to
                        MIF_Int_t.
        7/27/93  aip    Changed MIF_INT to MIF_INTEGER.
        7/29/93  aip    Introduced MIF_Unsigned_t where appropriate.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
		12/21/93 sfh	Remove _PASCAL.
        4/6/94   aip    Added 32-bit string support.
        05/18/94 par    Chagned MIF_String_t to DMI_STRING

************************* INCLUDES ***********************************/

#include <string.h>
#include "db_int.h"
#include "mif_db.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

extern MIF_FileHandle_t DB;

/*********************************************************************/

/************************ DOCUMENTATION *******************************

    There are three size fields in the DB_Data_t structure:

        objectSize		The total size in bytes of the space referenced
                        by dataPos.

        maxSize         The maximum size in bytes of the value of the
                        attribute to which this data object is associated.

        currentSize     The current size in bytes of the value in the
                        database of the attribute.

    Examples:

        1.  For numeric data objects, all three size fields are equal to
            the size in bytes of the data type size.

        2.  For READ-ONLY string objects:
                maxSize = the maximum specified in the MIF.  (If a value
                          was not specified, then maxSize = PR_LITERAL_MAX.)
                currentSize = the length in bytes of the string value.
                objectSize = the size of a DMI_STRING object containg
                             the current value.

        3.  For READ-WRITE string objects:
                maxSize = the size in bytes of a maximum string specified
                          in the MIF.  (This field is required.)
                currentSize = the size in bytes of the current string.
                objectSize = the size in bytes of a DMI_STRING object that
                             contains maxSize characters.

        4.  For numeric attributes with an overlay data object:
                maxSize = currentSize = the number of bytes of the numeric
                                        type.
                objectSize = the size of the DMI_STRING object containing
                             the overlay symbol name.

        5.  For READ-ONLY string attributes with an overlay data object:
                maxSize = PR_LITERAL_MAX (unless specified in the MIF).
                currentSize = 0.
                objectSize = the size of the DMI_STRING object containing
                             the overaly symbol name.

        6.  For READ-WRITE string attributes with an overlay data object:
                maxSize = (the size specified in the MIF).
                currentSize = 0.
                objectSize = the size of the DMI_STRING object containing
                             the overaly symbol name.
              
**********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_dataAdd(MIF_DataType_t type, MIF_Unsigned_t objectSize,
                     MIF_Unsigned_t maxSize, MIF_Unsigned_t currentSize,
                     MIF_Unsigned_t valueSize, void *value)
{
    MIF_Pos_t nullPos      = {0, 0};
    MIF_Pos_t newDataPos;
    MIF_Pos_t valuePos;
    DB_Data_t *d;
    void      *v;

    newDataPos = MIF_alloc(DB, sizeof(DB_Data_t));
    if ((newDataPos.page == 0) && (newDataPos.offset == 0))
        return nullPos;
    if(objectSize != 0){
        valuePos = MIF_alloc(DB, (unsigned short) objectSize);
        if ((valuePos.page == 0) && (valuePos.offset == 0))
            return nullPos;
    }
    else valuePos.page = valuePos.offset = 0;

    if ((d = (DB_Data_t *) MIF_resolve(DB, newDataPos, MIF_WRITE)) ==
        (DB_Data_t *) 0)
        return nullPos;   
    d -> dataType = type;
    d -> objectSize = objectSize;
    d -> maxSize = maxSize;
    d -> currentSize = currentSize;
    d -> valuePos.page = valuePos.page;
    d -> valuePos.offset = valuePos.offset;

    if(objectSize != 0){
        if ((v = (void *) MIF_resolve(DB, valuePos, MIF_WRITE)) == (void *) 0)
            return nullPos;

    	memcpy(v, value, (size_t) valueSize);
    }
    
	return newDataPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_dataDelete(MIF_Pos_t targetPos)
{
    DB_Data_t      *d;
    MIF_Unsigned_t objectSize;
    MIF_Pos_t      valuePos;
    char           *c;
    short          i;

    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

    if ((d = (DB_Data_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Data_t *) 0)
        return MIF_FILE_ERROR;
    objectSize = d -> objectSize;
    valuePos.page = d -> valuePos.page;
    valuePos.offset = d -> valuePos.offset;
    d -> dataType = MIF_UNKNOWN_DATA_TYPE;
    d -> objectSize = 0;
    d -> maxSize = 0;
    d -> currentSize = 0;
    d -> valuePos.page = 0;
    d -> valuePos.offset = 0;

    if ((valuePos.page == 0) && (valuePos.offset == 0))
       return MIF_OKAY;
    if ((c = (char *) MIF_resolve(DB, valuePos, MIF_WRITE)) == (char *) 0)
        return MIF_FILE_ERROR;
    for (i = 0; i < (short) objectSize; ++i)
        c[i] = 0;
    return MIF_OKAY;
}

#endif

short DB_dataTypeSizeGet(MIF_Unsigned_t dataType)
{
    switch (dataType) {
        case MIF_UNKNOWN_DATA_TYPE:
        case MIF_COMPONENT_PATH_NAME:
            return -1;
        case MIF_DISPLAYSTRING:
        case MIF_OCTETSTRING:
            return 0;
        case MIF_COUNTER:
        case MIF_GAUGE:
        case MIF_INTEGER:
            return 4;
        case MIF_INTEGER64:
        case MIF_COUNTER64:
            return 8;
        case MIF_DATE:
            return 28;
    }
}

#ifndef SL_BUILD

MIF_Pos_t DB_stringAdd(DB_String_t *string)
{
    MIF_Pos_t	nullPos        = {0, 0};
    MIF_Pos_t	newStringPos;
    DB_String_t	*c;

    newStringPos = MIF_alloc(DB, (unsigned short) DB_stringSize(string));
    if ((newStringPos.page == 0) && (newStringPos.offset == 0))
        return nullPos;

    if ((c = (DB_String_t *) MIF_resolve(DB, newStringPos, MIF_WRITE)) ==
        (DB_String_t *) 0)
        return nullPos;
    DB_stringCpy(c, string);
    return newStringPos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_stringDelete(MIF_Pos_t strPos)
{
    short       limit;
    DB_String_t *c;

    if ((strPos.page == 0) && (strPos.offset == 0))
        return MIF_BAD_POS;
    
    limit = (short) (MIF_PAGE_SIZE - strPos.offset);
    if ((c = (DB_String_t *) MIF_resolve(DB, strPos, MIF_WRITE)) ==
        (DB_String_t *) 0)
        return MIF_FILE_ERROR;
	DB_stringSet(c, '\0');
    c -> length = 0;
    return MIF_OKAY;
}

#endif

#ifndef SL_BUILD

MIF_Pos_t DB_ElementAdd(DB_String_t *name, short size)
{
    MIF_Pos_t      nullPos    = {0, 0};
    MIF_Pos_t      namePos;
    DB_Name_t      *n;
    MIF_Pos_t      strPos;
    DB_String_t    *s;

    if (name == NULL || name->length == 0) return nullPos;
    if (size < sizeof(DB_Name_t)) size = sizeof(DB_Name_t);

    namePos = MIF_alloc(DB, size);
    if ((namePos.page == 0) && (namePos.offset == 0)) return nullPos;
        
    strPos = MIF_alloc(DB, (unsigned short) DB_stringSize(name));
    if ((strPos.page == 0) && (strPos.offset == 0)) return nullPos;


    if ((n = (DB_Name_t *) MIF_resolve(DB, namePos, MIF_WRITE)) ==
        (DB_Name_t *) 0) return nullPos;
    n -> type = MIF_UNKNOWN_TYPE;
    n -> name.page = strPos.page;
    n -> name.offset = strPos.offset;
    n -> nameLinkNext.page = 0;
    n -> nameLinkNext.offset = 0;
    if ((s = (DB_String_t *) MIF_resolve(DB, strPos, MIF_WRITE)) ==
        (DB_String_t *) 0) return nullPos;
    DB_stringCpy(s, name);
    return namePos;
}

#endif

#ifndef SL_BUILD

MIF_Status_t DB_ElementDelete(MIF_Pos_t targetPos)
{
DB_Name_t       *n;
MIF_Pos_t       strPos;

    if ((targetPos.page == 0) && (targetPos.offset == 0))
        return MIF_BAD_POS;

    if ((n = (DB_Name_t *) MIF_resolve(DB, targetPos, MIF_WRITE)) ==
        (DB_Name_t *) 0) return MIF_FILE_ERROR;

    strPos.page = n -> name.page;
    strPos.offset = n -> name.offset;
    n -> type = MIF_UNKNOWN_TYPE;
    n -> name.page = 0;
    n -> name.offset = 0;
    n -> nameLinkNext.page = 0;
    n -> nameLinkNext.offset = 0;
    if (DB_stringDelete(strPos) != MIF_OKAY) return MIF_FILE_ERROR;
    return MIF_OKAY;
}

#endif

