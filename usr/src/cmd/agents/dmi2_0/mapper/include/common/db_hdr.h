/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_hdr.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: db_hdr.h
    

    Description: MIF Database Header Get/Set Routines Header

    Author(s):

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/27/93  aip    Added unsigned short to prototypes.
        8/5/93   aip    Changed unsigned to unsigned short.
                        Changed int to short.
		11/29/93 sfh	Revised for asm translation.
        12/7/93  aip    Added locking table header.
                        Added prototypes for header access.
		12/08/93 sfh	Changed headerLockTableLastComponentIdX
						to headerLockTableLastCompIdX; for h2inc (too long).
		12/21/93 sfh	Remove _PASCAL.

**********************************************************************/

#ifndef DB_HDR_H_FILE
#define DB_HDR_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef struct {
            unsigned short pageStart;
            unsigned short pageCount;
            MIF_Bool_t     wrapMode;
            unsigned short lastIndex;
        } DB_IndexHeader_t;

typedef struct {
            unsigned short pageStart;
            unsigned short pageCount;
            unsigned short lastComponentId;
        } DB_LockTableHeader_t;

typedef struct {
            MIF_Header_t         mifHeader;
            DB_IndexHeader_t     indexTableHeader;
            DB_LockTableHeader_t lockTableHeader;
        } DB_DatabaseHeader_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

MIF_Status_t    DB_headerFlush(MIF_FileHandle_t fileHandle);

unsigned short  DB_headerIndexTableLastIndexGet(MIF_FileHandle_t
                                                       fileHandle);
MIF_Status_t    DB_headerIndexTableLastIndexSet(MIF_FileHandle_t fileHandle,
                                                unsigned short index);
MIF_Status_t    DB_headerIndexTablePageCountSet(MIF_FileHandle_t fileHandle,
                                                unsigned short count);
unsigned short  DB_headerIndexTablePageStartGet(MIF_FileHandle_t
                                                       fileHandle);
MIF_Status_t    DB_headerIndexTablePageStartSet(MIF_FileHandle_t fileHandle,
                                                unsigned short pageNumber);
MIF_Status_t    DB_headerIndexTableWrapModeSet(MIF_FileHandle_t fileHandle,
                                               MIF_Bool_t value);

unsigned short  DB_headerLockTableLastCompIdGet(MIF_FileHandle_t
                                                     fileHandle);
MIF_Status_t    DB_headerLockTableLastCompIdSet(MIF_FileHandle_t
                    fileHandle, unsigned short lastComponentId);
unsigned short  DB_headerLockTablePageStartGet(MIF_FileHandle_t fileHandle);
MIF_Status_t    DB_headerLockTablePageStartSet(MIF_FileHandle_t fileHandle,
                                               unsigned short pageStart);
MIF_Status_t    DB_headerLockTablePageCountSet(MIF_FileHandle_t fileHandle,
                                               unsigned short pageCount);

/*********************************************************************/


#endif
