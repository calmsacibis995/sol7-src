/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_util.h	1.1 96/08/05 Sun Microsystems"

/*****************************************************************************************************************************************************

    Filename: psl_util.h
    

    Description: Service Layer C-code Utility Header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: n:/sl/rcs/psl_util.h 1.31 1994/05/18 09:02:36 shanraha Exp $

    Revision History
		Date     Author
		-------  ---
		3/30/93  	aip    	Creation date
		10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
		11/02/93 	sfh		Move unsignedToAscii back to slutils.asm.  Not used in PSL.
		11/11/93 	sfh		Remove include of types.h.
		12/21/93 	sfh		Add _cdecl modifier to buildResponse to can build with pascal linkage.
		03/18/94	sfh		Modify buildCiCommand for asm version.
		04/07/94	sfh		Add DMI_STRING function prototypes.
		04/19/94	sfh		Change ifdef of MSC_VER in ResponseTable to PS_BUILD.
		05/18/94	sfh		Change return type of buidCiCommand to GetAttributeCnf.
        06/18/94    par         Changed name to conform to new naming rules
		 
****************************************************************************************************************************************************/

#ifndef PSL_UTILC_H_FILE
#define PSL_UTILC_H_FILE
						  				

/****************************************************************** INCLUDES ***********************************************************************/

#include "error.h"
#include "os_dmi.h"
#include "psl_main.h"
#include "db_api.h"

/***************************************************************************************************************************************************/


/******************************************************************* DEFINES ***********************************************************************/


/***************************************************************************************************************************************************/


/******************************************************************** TYPEDEFS *********************************************************************/

typedef struct
{
	unsigned short	structSize;
	unsigned short	offsetCount;
	unsigned short	offsets[3];
} SL_ResponseTable_t;

/***************************************************************************************************************************************************/

		
/********************************************************************** DATA ***********************************************************************/


/***************************************************************************************************************************************************/


/**************************************************************** FUNCTION PROTOTYPES **************************************************************/

DMI_GetAttributeCnf_t _FAR	*SL_buildCiCommand(void _FAR *dmiCommand);

/* This function is supplied by the OS-dependent code.                       */
void 						SL_buildError(void _FAR *dmiCommand, SL_ErrorCode_t errorCode);
DMI_GetAttributeCnf_t _FAR	*SL_buildGroupResponse(DMI_GetRowReq_t _FAR *dmiCommand, DMI_UNSIGNED attributeSize, void _FAR *attributeValue);
DMI_GroupKeyData_t _FAR	 	*SL_buildGroupKey(DMI_GetRowReq_t _FAR *dmiCommand, DMI_UNSIGNED keySize, void _FAR *keyValue);
void _FAR *      			SL_buildResponse(void _FAR *dmiCommand, ...);
DMI_EnumData_t _FAR			*SL_buildEnumList(DMI_ListAttributeReq_t _FAR *cmd, DMI_STRING *enumValue);
void 						SL_packResponse(DMI_Confirm_t _FAR *dmiCommand);

/***************************************************************************************************************************************************

    Function:	SL_buildPathname

  Description:	Mallocs and builds a string containing Service Layer path and given filename.  Caller must free the memory returned.

   Parameters:	filename		Pointer to filename to add to the Service Layer path.

      Returns:	Complete pathname for given file.

***************************************************************************************************************************************************/

char *SL_buildPathname(char *filename);


/***************************************************************************************************************************************************

    Function:	stringCmp

  Description:	Compares 2 DMI_STRINGs.

   Parameters:	str1, str2		Pointer to strings to compare.

      Returns:	< 0 if str1 is lexically less than str2
				= 0 if str1 is lexically equal to str2.
      			> 0 if str1 is lexically greater than str2.

***************************************************************************************************************************************************/

short stringCmp(DMI_STRING _FAR *str1, DMI_STRING _FAR *str2);

/***************************************************************************************************************************************************

    Function:	stringCpy

  Description:	Copys one DMI_STRING into another DMI_STRING, including the length descriptor.

   Parameters:	dest		Pointer to destination string.
				source		Pointer to source string.

      Returns:	Pointer to destination string.

***************************************************************************************************************************************************/

DMI_STRING _FAR *stringCpy(DMI_STRING _FAR *dest, DMI_STRING _FAR *source);

/***************************************************************************************************************************************************

    Function:	stringLen

  Description:	Returns the length in bytes of string, including the size of the length descriptor.  In other words, the return value is 4 greater
				than the length of the body of string.

   Parameters:	string		Pointer to DMI_STRING.

      Returns:	Length of string.

***************************************************************************************************************************************************/

unsigned short stringLen(DMI_STRING _FAR *string);


/***************************************************************************************************************************************************/


/***************************************************************************************************************************************************

    Function:	stringCat

  Description:	Appends src DMI_STRING to dest DMI_STRING.

   Parameters:	dest			Pointer to DMI_STRING to append to.
				source			Pointer to DMI_STRING to be appended to dest.

      Returns:	dest/

***************************************************************************************************************************************************/

DMI_STRING _FAR *stringCat(DMI_STRING _FAR *dest, DMI_STRING _FAR *source);


/* This macro builds an initialized DMI_STRING object initialized with       */
/* the string passed into it.                                                */

#define DMI_iSTRING(n, s) DMI_STRING n = { sizeof(s) - 1, s }

#endif
