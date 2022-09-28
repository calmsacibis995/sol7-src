/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_strng.c	1.3 96/09/24 Sun Microsystems"


/******************************************************************************

    Filename: strng.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Database String object procedures

    Author(s): Steve Hanrahan
               Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_strng.c 1.11 1994/04/07 09:34:10 apivowar Exp $

    Revision History:

        Date     	Author Description
        -------  	---    -----------
        5/24/93  	aip    	Moved procedures from db_int.c to here.
		12/14/93 	sfh		Change SL prototypes to _PASCAL linkage.
		12/21/93 	sfh		Remove _PASCAL.
		03/08/94	sfh		Uncomment fstringCpy.
		03/22/94	sfh		Remove fstring versions; all functions are now _FAR;
                            for OS independence.
		03/23/94	sfh		Make stringInit like above.
        4/6/94      aip     Add 32-bit string support.

******************************************************************************/

/**************************** INCLUDES ***************************************/
/*****************************************************************************/

/**************************** DEFINES ****************************************/
/*****************************************************************************/

/**************************** TYPEDEFS ***************************************/
/*****************************************************************************/

#ifndef SL_BUILD

/* This function compares two DB_String_t objects                            */

short DB_stringCmp(DB_String_t *string1, DB_String_t *string2)
{
	return (short) (memcmp(string1, string2, (size_t) DB_stringSize(string1)));
}

/* This function copies a DB_String_t                                        */

DB_String_t *DB_stringCpy(DB_String_t *destination, DB_String_t *source)
{
	memcpy(destination, source, (size_t) DB_stringSize(source));
	return destination;
}

/* This function copies a null-terminated C string into a DB_String_t.       */

DB_String_t *DB_stringInit(DB_String_t *destination, char *source)
{
    destination -> length = strlen(source);
    memcpy(&destination -> body, source, (size_t) destination -> length);
    return destination;
}

/* This function fills in a DB_String_t by repeating a given character.      */

DB_String_t *DB_stringSet(DB_String_t *destination, char c)
{
	memset(&destination -> body, c, (size_t) destination -> length);
    return destination;
}

/* This function returns the size of a DB_String_t (length + data).          */

unsigned long DB_stringSize(DB_String_t *string)
{
	return alignmem(string -> length) + sizeof(string -> length);
}

#endif
