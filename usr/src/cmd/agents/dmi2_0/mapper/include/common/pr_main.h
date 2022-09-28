/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_main.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: PR_MAIN.H
    

    Description: MIF Installer

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/install.h 1.5 1994/06/27 11:41:29 shanraha Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        4/7/94   aip    32-bit string support.
        5/20/94  aip    Changed extensions.
		06/27/94 sfh	MIF_String_t -> DMI_STRING.
        10/10/95 par    Modified for the removal of the BIF step.

**********************************************************************/

#ifndef PR_MAIN_H_FILE
#define PR_MAIN_H_FILE

/************************ INCLUDES ***********************************/

#include "os_dmi.h"
#include "mif_db.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_SOURCE_EXTENSION   "mif"
#define PR_DATABASE_EXTENSION "dmi"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/
/*********************************************************************/

/************************ MACROS *************************************/

#define PR_uString(name, size) char name[(size) + sizeof(unsigned long)]

/*********************************************************************/

/************************ PUBLIC *************************************/

DMI_STRING   *PR_stringInit(DMI_STRING *destination, char *source);
unsigned long PR_stringSize(DMI_STRING *string);
short         PR_stringCmp(DMI_STRING *string1, DMI_STRING *string2);
unsigned long PR_main(char *sourcefile,DMI_UNSIGNED *iComponentID,
                    DMI_UNSIGNED *iIndicationType,void *OS_Context);

/*********************************************************************/

#endif
