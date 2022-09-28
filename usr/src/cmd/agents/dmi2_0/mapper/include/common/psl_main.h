/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_main.h	1.1 96/08/05 Sun Microsystems"

/*****************************************************************************************************************************************************
    Filename: psl_main.h
    

    Description: Portable Service Layer Common Definitions

    Author(s): Alvin I. Pivowar, Paul A. Ruocchio

    RCS Revision: $Header: n:/sl/rcs/psl_main.h 1.10 1994/05/11 15:34:40 shanraha Exp $

    Revision History
		Date		Author	Description
		----  		------	------------
		11/11/93 	sfh		Initial version.
		02/25/94	sfh		Add parent/child model definitions.
		04/08/94	sfh		Remove environment IDs; these are in db_api.h.
		04/11/94	sfh		Move SPEC_LEVEL and PSL_VERSION from dmi.h.
		04/15/94	sfh		Add name to iCount/iSelector field of DmiInstrumentationVector for ANSI compliance.
		04/21/94	sfh		Move DmiCancelAllCmd from dmi.h.
		05/10/94	sfh		Update versions.
							Change parent to secondary.
        05/19/94    par         Updated versions.
        05/19/94    par         Added DmiKillOverlayCmd.
        06/18/94    par         Changed name to conform to naming rules
        07/23/94    par         Modified for new *.h files
        11/15/94    par         Changed SPEC_LEVEL from "1.0" to "DMI 1.0" to conform to spec.
        02/10/95    par         Removed all references to the Parent/Child stuff - NOT USED.
        02/10/95    par         Changed PSL version to 3.00
        01/24/96    par         modified to conform to DMI Version 1.1

*****************************************************************************************************************************************************/


/****************************************************************** INCLUDES ***********************************************************************/

#include "os_dmi.h"

/***************************************************************************************************************************************************/


/******************************************************************* DEFINES ***********************************************************************/

#ifndef PSL_H_FILE
#define PSL_H_FILE

#ifndef TRUE
#define FALSE 0
#define TRUE 1
#endif

/* Version of portable service layer and spec level implemented              */
#define SPEC_LEVEL	"DMI 1.1"
#define PSL_VERSION	"PSL Ver. 3.01B"

#define DmiCancelAllCmd							DMI_MISCELLANEOUS_COMMANDS + 0x50
#define DmiKillOverlayCmd                       DMI_MISCELLANEOUS_COMMANDS + 0x51


/* To simplify pointer building                                              */
#define BP(type, parts) (type *)((char *)parts)
#define BFP(type, parts) (type _FAR *)((char _FAR *)parts)

/***************************************************************************************************************************************************/


/******************************************************************** TYPEDEFS *********************************************************************/

/* The following are definitions needed for the parent-child model           */

typedef struct DmiInstrumentationDescriptor 
{
	DMI_UNSIGNED iEnvironmentId;
	DMI_OFFSET	 osPathname;
} DMI_InstrumentationDescriptor_t;


typedef struct DmiInstrumentationVector 
{
	DMI_OFFSET						osInstrumentationName;
	union 
	{
		DMI_UNSIGNED 				iCount;
		DMI_UNSIGNED 				iSelector;
	} countSelector;
	DMI_InstrumentationDescriptor_t	DmiInstrumentation[1];
} DMI_InstrumentationVector_t;


/***************************************************************************************************************************************************/


/********************************************************************** DATA ***********************************************************************/

/***************************************************************************************************************************************************/


/******************************************************************** FUNCTIONS ********************************************************************/

/***************************************************************************************************************************************************/

#endif 			/*PSL_H_FILE                                                       */


				
