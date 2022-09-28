/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)db_int.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: db_int.c
    
    Copyright (c) Intel, Inc. 1992,1993
    Copyright (c) International Business Machines, Corp. 1994

    Description: MIF Internal Database Routines

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
		05/05/93 sfh	Add db_key.c include.
		05/21/93 sfh	Add MIF_String functions.
		05/21/93 sfh	Revise StringInit to copy the string before
						setting the length, in case strings overlap.
		05/21/93 sfh	Remove conditional compilation around stringLen,
						stringCmp and stringCpy.
        5/24/93  aip    Removed String functions.
        5/25/93  aip    db_int.c now includes all the other intrinsic files.
		12/09/93 sfh	Don't include some files based on ASM_BUILD.
		02/17/94 sfh	Undo above change.
        11/16/94 par    Added the inlcude file DB_API.H to get the max component value in
        10/16/95 par    Modified includes for new MIF_IO stuff
		
************************* INCLUDES ***********************************/

#include <string.h>
#include "db_index.h"
#include "db_int.h"
#include "mif_db.h"
#include "db_api.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

extern MIF_FileHandle_t DB;

/*********************************************************************/

#include "db_attr.c"
#include "db_comp.c"
#include "db_group.c"
#include "db_cpath.c"
#include "db_data.c"
#include "db_strng.c"
#include "db_table.c"
#include "db_enum.c"
#include "db_key.c"
