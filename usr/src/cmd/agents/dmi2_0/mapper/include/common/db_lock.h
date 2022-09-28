/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_lock.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: db_lock.h
    

    Description: Database Component-level locking header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/mif/db/rcs/db_lock.h 1.4 1993/12/21 16:14:49 shanraha Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        12/7/93  aip    Creation date.
		12/08/93 sfh	Include db_int.h (for MaHandle_t).
		12/21/93 sfh	Remove _PASCAL.

**********************************************************************/

#ifndef DB_LOCK_H_FILE
#define DB_LOCK_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "db_int.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

MIF_Status_t    DB_lockGet(unsigned short componentId,MIF_MaHandle_t *Handle);
MIF_Status_t 	DB_lockSet(unsigned short componentId, MIF_MaHandle_t maHandle);
MIF_Status_t 	DB_lockTableClear(void);
MIF_Status_t	DB_lockTableCreate(unsigned short lastComponentId);

/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#endif
