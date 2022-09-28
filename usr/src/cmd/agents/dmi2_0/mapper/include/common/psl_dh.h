/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_dh.h	1.1 96/08/05 Sun Microsystems"

/*****************************************************************************************************************************************************

    Filename: psl_dh.h
    

    Description: Database Handler Header

    Author(s): Alvin I. Pivowar, Steve Hanrahan

	RCS Revision: $Header: n:/sl/rcs/psl_dh.h 1.34 1994/04/14 18:09:00 shanraha Exp $

    Revision History:

        Date    	Author 	Description
        ----  		------ 	-----------
        3/17/93  	aip    	Creation Date.
		10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
		11/17/93 	sfh		Move private definitions to dh.c.
		03/01/94	sfh		Include dmi.h, not os_dmi.h.
		03/10/94	sfh		Move PSL_VERSION to dmi.h.
		04/06/94	sfh		Add DH_listComponent.
		04/08/94	sfh		Change type	of DB_mifDbName.
        06/18/94    par         Changed name to conform to new naming rules

***************************************************************************************************************************************************/

#ifndef PSL_DH_H_FILE
#define PSL_DH_H_FILE

/********************************************************************* INCLUDES *********************************************************************/

#include "db_api.h"
#include "os_dmi.h"
#include "psl_tm.h"

/**************************************************************************************************************************************************/


/******************************************************************* DEFINES ***********************************************************************/

/**************************************************************************************************************************************************/


/********************************************************************* TYPEDEFS *********************************************************************/

extern char 				DH_mifDbName[];
void            			DH_fini(void);
boolean _FAR            	DH_init(char _FAR *SL_PathName);
TM_TaskStatus_t 			DH_main(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter);
TM_Task_t					DH_listComponent(DMI_ListComponentReq_t _FAR *dmiCommand);


/**************************************************************************************************************************************************/

#endif
