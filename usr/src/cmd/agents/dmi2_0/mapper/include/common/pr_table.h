/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_table.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_table.h
    

    Description: MIF Parser Static Table routines header

    Author(s): Alvin I. PIvowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_table.h 1.3 1994/08/08 15:30:55 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        2/28/94  aip    BIF
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c

**********************************************************************/

#ifndef PR_TABLE_H_FILE
#define PR_TABLE_H_FILE

/************************ INCLUDES ***********************************/

#include "pr_todmi.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

void                     PR_tableInit(void);
void                     PR_pTable(void);
unsigned long            PR_tableElementCount(unsigned long tableId);
unsigned long            PR_tableElementId(unsigned long tableId,
                                           unsigned long elementIndex);
void                     *PR_tableElementValue(unsigned long tableId,
                                               unsigned long elementIndex);
BIF_AttributeValueType_t PR_tableElementValueType(unsigned long tableId,
                                                  unsigned long elementIndex);

/*********************************************************************/

#endif
