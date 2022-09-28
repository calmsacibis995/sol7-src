/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_src.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_src.h
    
    Copyright (C) International Business Machines, Corp. 1995
    Copyright (c) Intel, Inc. 1992,1993

    Description: MIF Parser Source I/O Routines Header

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        10/13/95 par    Modified to remove dead code
        10/13/95 par    Modified to remove dependencies on MIF_IO

**********************************************************************/

#ifndef PR_SRC_H_FILE
#define PR_SRC_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"

/*********************************************************************/

/************************ PUBLIC *************************************/

int             PR_sourceCharGetNext(void);
int             PR_sourceCharGetPrev(void);
MIF_Status_t    PR_sourceClose(void);
MIF_Status_t    PR_sourceOpen(char *filename);

/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#endif
