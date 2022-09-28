/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_group.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_group.h
    

    Description: MIF parser group routine header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_group.h 1.8 1994/09/14 13:25:01 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        12/3/93  aip    Added PR_cClass prototype.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        4/7/94   aip    4.4
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        9/14/94  aip    Added PR_INSTALLATION_ATTRIBUTE definition.
        10/28/94 aip    Fixed class string checking.
        10/10/95 par    Modified to remove dead code
        01/24/96 par    Modified for Version 1.1

**********************************************************************/

#ifndef PR_GROUP_H_FILE
#define PR_GROUP_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "pr_tok.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_COMPONENT_ID_GROUP 1
#define PR_INSTALLATION_ATTRIBUTE         5
#define PR_COMPONENT_ID_GROUP_BODY        "DMTF"
#define PR_COMPONENT_ID_GROUP_CLASS       "ComponentID"
#define PR_COMPONENT_ID_GROUP_CLASS_STRING \
    PR_COMPONENT_ID_GROUP_BODY "|" PR_COMPONENT_ID_GROUP_CLASS "|"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef struct PR_Key {
            MIF_Id_t      id;
            struct PR_Key *next;
        } PR_Key_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

void          PR_groupInit(void);
void          PR_pGroup(void);
DMI_STRING    *PR_groupClass(void *groupHandle);
void          *PR_groupClassToHandle(DMI_STRING *class);
void          *PR_groupCopy(unsigned long targetId, void *groupHandle);
unsigned long PR_groupCount(void);
DMI_STRING    *PR_groupDescription(void *groupHandle);
DMI_STRING    *PR_groupPragma(void *groupHandle);
MIF_Bool_t    PR_groupExists(unsigned long groupId);
unsigned long PR_groupHandleToId(void *groupHandle);
unsigned long PR_groupId(void *groupHandle);
void          PR_groupIdStore(void *groupHandle, unsigned long groupId);
void          *PR_groupIdToHandle(unsigned long groupId);
unsigned long PR_groupKeyCount(void *groupHandle);
MIF_Bool_t    PR_groupKeyExists(void *groupHandle, unsigned long attributeId);
unsigned long PR_groupKeyNext(void *groupHandle, unsigned long attributeId);
DMI_STRING    *PR_groupName(void *groupHandle);
void          PR_groupNameStore(void *groupHandle, DMI_STRING *groupName);
unsigned long PR_groupNameToId(DMI_STRING *groupName);
unsigned long PR_groupNext(void *groupHandle);
void          *PR_groupTemplateHandle(void *groupHandle);
void          PR_groupTemplateHandleStore(void *groupHandle,
                                          void *templateHandle);
unsigned long PR_unusedTemplateCount(void);

/*********************************************************************/

#endif
