/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_comp.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_comp.h
    
    Copyright (C) International Business Machines, Corp. 1995
    Copyright (c) Intel, Inc. 1992,1993,1994

    Description: MIF Parser component routines header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_comp.h 1.4 1994/08/08 13:24:57 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        2/28/94  aip    BIF
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_COMP_H_FILE
#define PR_COMP_H_FILE

/************************ INCLUDES ***********************************/

#include "db_int.h"
#include "pr_key.h"
#include "pr_tok.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

void       PR_componentInit(void);
void       PR_pComponent(void);
DMI_STRING *PR_componentDescription(void);
DMI_STRING *PR_componentName(void);

/*********************************************************************/

#endif
