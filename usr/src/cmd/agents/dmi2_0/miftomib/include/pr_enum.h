/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_enum.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_enum.h
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Enumeration header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_enum.h 1.5 1994/08/08 13:28:40 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        2/28/94  aip    BIF
        6/10/94  aip    Fixed enumeration problem.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        8/2/94   aip    Changed enumerations to use handles.
        10/28/94 aip    Use handles.
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_ENUM_H_FILE
#define PR_ENUM_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "pr_tok.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

void          PR_enumerationInit(void);
void          PR_pEnumeration(PR_Token_t *t, void *groupHandle,
                              unsigned long attributeId);
MIF_Bool_t    PR_enumElementExists(void *enumHandle, DMI_STRING *elementName);
unsigned long PR_enumElementCount(void *enumHandle);
DMI_STRING    *PR_enumElementIndexToName(void *enumHandle,
                                         unsigned long elementIndex);
MIF_Int_t     PR_enumElementValue(void *enumHandle,
                                  DMI_STRING *elementName);
DMI_STRING    *PR_enumHandleToName(void *enumHandle);
void          *PR_enumIdToHandle(void *groupHandle,
                                 unsigned long attributeId);
void          *PR_enumNameToHandle(DMI_STRING *enumName);

/*********************************************************************/

#endif
