/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_path.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_path.h
    

    Description: MIF Parser path routine header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_path.h 1.3 1994/08/08 15:04:37 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        12/6/93  aip    Added prototype for Direct Interface checking.
        2/28/94  aip    BIF
        3/22/94  aip    4.3
        6/21/94  par    Moved PR_TARGET_OS to OS_DMI.H.  This way all
                        OS dependant stuff is in one place.
        06/27/94 sfh	MIF_String_t -> DMI_STRING.
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        10/10/95 par    Modified for the removal of dead code

**********************************************************************/

#ifndef PR_PATH_H_FILE
#define PR_PATH_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "pr_key.h"

/*********************************************************************/

/************************ DEFINE *************************************/

/*********************************************************************/

/************************ PUBLIC *************************************/

void          PR_pathsInit(void);
void          PR_pPaths(void);
unsigned long PR_pathCount(void);
unsigned long PR_pathEnvironmentCount(unsigned long pathIndex);
unsigned long PR_pathEnvironmentId(unsigned long pathIndex,
                                   unsigned long environmentIndex);
DMI_STRING    *PR_pathEnvironmentPathName(unsigned long pathIndex,
                                          unsigned long environmentIndex);
MIF_Bool_t    PR_pathExists(DMI_STRING *symbolicName,
                            DMI_STRING *environment);
DMI_STRING    *PR_pathSymbolName(unsigned long pathIndex);
DMI_STRING    *PR_pathTargetOs(void);

/*********************************************************************/

#endif
