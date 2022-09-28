/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_key.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************

	Filename: db_key.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Internal Database Routines for Keys

    Author(s): Steve Hanrahan

    RCS Revision: $Header: n:/mif/db/rcs/db_key.c 1.6 1993/08/05 14:45:17 apivowar Exp $

    Revision History:

        Date	 Author	Description
        -------	 ---	-----------------------------------------------
		05/05/93 sfh	Initial build.
        5/18/93  aip    Fixed keySize;
        6/10/93  aip    Added keyFind procedure.
        6/22/93  aip    Added DB_keyCopy.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.

**********************************************************************/

/************************ INCLUDES ***********************************/

/*********************************************************************/

/************************ GLOBALS ************************************/
/*********************************************************************/

#ifndef SL_BUILD

MIF_Pos_t DB_keyAdd(MIF_Id_t componentId, MIF_Id_t groupId, DB_Key_t *key)
{
    MIF_Pos_t	nullPos = {0, 0};
    MIF_Pos_t	keyPos;
    MIF_Pos_t	groupPos;
    DB_Group_t	*g;
    DB_Key_t 	*k;
	MIF_Int_t   keySize;

    if (componentId == 0)
        return nullPos;
    if (groupId == 0)
        return nullPos;
    if (key == 0)
        return nullPos;

	keySize = sizeof(DB_Key_t) + (key -> keyCount - 1) * sizeof(key->keys);
    keyPos = MIF_alloc(DB, (unsigned short) keySize);
    if ((keyPos.page | keyPos.offset) == 0)
        return nullPos;

	if ((k = (DB_Key_t *)MIF_resolve(DB, keyPos, MIF_WRITE)) == NULL)
		return nullPos;
	memcpy(k, key, (size_t) keySize);

	groupPos = DB_groupIdToPos(componentId, groupId);
	if ((g = (DB_Group_t *) MIF_resolve(DB, groupPos, MIF_WRITE)) == NULL)
	    return nullPos;
	g->key.page = keyPos.page;
	g->key.offset = keyPos.offset;

	return keyPos;
}

#endif


#ifndef SL_BUILD

MIF_Status_t DB_keyDelete(MIF_Pos_t keyPos)
{
    DB_Key_t 	*k;

	if ((k = (DB_Key_t *)MIF_resolve(DB, keyPos, MIF_WRITE)) == NULL)
		return MIF_FILE_ERROR;

	memset(k, 0, (size_t) (sizeof(DB_Key_t) + ((k->keyCount) - 1) * sizeof(k->keys)));
	return MIF_OKAY;
}

#endif

#ifndef SL_BUILD

MIF_Bool_t DB_keyFind(MIF_Pos_t keyPos, MIF_Id_t keyValue)
{
    DB_Key_t  *k;
    MIF_Int_t keyCount;
    MIF_Int_t i;

/*  Check the keyPos.                                                        */

    if ((keyPos.page == 0) && (keyPos.offset == 0))
        return MIF_FALSE;
    if ((k = (DB_Key_t *) MIF_resolve(DB, keyPos, MIF_READ)) ==
        (DB_Key_t *) 0)
        return MIF_FALSE;

/*  Get the key count.                                                       */

    keyCount = k -> keyCount;

/*  Find the key.                                                            */

    for (i = 0; i < keyCount; ++i)
        if (keyValue == k -> keys[i])
            return MIF_TRUE;

    return MIF_FALSE;
}

#endif
