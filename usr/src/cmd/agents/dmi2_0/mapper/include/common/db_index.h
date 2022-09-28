/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_index.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: db_index.h
    

    Description: MIF Database Index Routines Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
		11/29/93 sfh	Modify for asm translation.

**********************************************************************/

#ifndef DB_INDEX_H_FILE
#define DB_INDEX_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

unsigned short DB_indexGenerate(void);
MIF_Pos_t      DB_indexPos(unsigned short index);
MIF_Status_t   DB_indexTableCreate(unsigned short indexCount);

/*********************************************************************/


#endif
