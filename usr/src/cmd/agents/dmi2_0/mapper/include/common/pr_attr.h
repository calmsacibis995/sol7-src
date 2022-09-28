/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_attr.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_attr.h
    

    Description: MIF parser attribute routines header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_attr.h 1.10 1994/09/08 11:05:07 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    4.4
        4/21/94  aip    Added date and 64-bit types.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        9/2/94   aip    Moved PR_cDate() from private to public.
        10/28/94 aip    Use group handles.
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_ATTR_H_FILE
#define PR_ATTR_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "pr_tok.h"
#include "pr_todmi.h"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ PUBLIC *************************************/

void                     PR_attributeInit(void);
void                     PR_pAttribute(void *groupHandle);
BIF_AttributeAccess_t    PR_attributeAccess(void *groupHandle,
                                            unsigned long attributeId);
unsigned long            PR_attributeCount(void *groupHandle);
DMI_STRING               *PR_attributeDescription(void *groupHandle,
                                                  unsigned long attributeId);
void                     *PR_attributeEnumHandle(void *groupHandle,
                                                 unsigned long attributeId);
MIF_Bool_t               PR_attributeExists(void *groupHandle,
                                            unsigned long attributeId);
unsigned long            PR_attributeMaxSize(void *groupHandle,
                                             unsigned long attributeId);
DMI_STRING               *PR_attributeName(void *groupHandle,
                                           unsigned long attributeId);
unsigned long            PR_attributeNext(void *groupHandle,
                                          unsigned long attributeId);
BIF_AttributeStorage_t   PR_attributeStorage(void *groupHandle,
                                             unsigned long attributeId);
BIF_AttributeDataType_t  PR_attributeType(void *groupHandle,
                                          unsigned long attributeId);
void                     *PR_attributeValue(void *groupHandle,
                                            unsigned long attributeId);
BIF_AttributeValueType_t PR_attributeValueType(void *groupHandle,
                                               unsigned long attributeId);
MIF_Bool_t               PR_cDate(char *date);

/*********************************************************************/

#endif
