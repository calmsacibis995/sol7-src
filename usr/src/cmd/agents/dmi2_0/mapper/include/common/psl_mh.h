/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_mh.h	1.1 96/08/05 Sun Microsystems"

/****************************************************************************************************************************************************

    Filename: psl_mh.h
    

    Description: DMI Handler Header

    Author(s): Alvin I. Pivowar
               Paul A. Ruocchio

	RCS Revision: $Header: n:/sl/rcs/psl_mh.h 1.39 1994/03/16 21:41:10 shanraha Exp $

    Revision History:

        Date     	Author	Description
        -------  	---   	-----------------------------------------------
        3/12/93  	aip   	Initial build and test.
		10/28/93 	sfh		Add MH_getTime.
		11/16/93 	sfh		Change handleEvent to a TM_Task_t.
		12/08/93 	sfh		Move private prototypes to mh.c.
							Update SPEC_LEVEL.
		12/29/93 	sfh		MaContext -> maContext; Registry -> registry.
							Redo some prototypes for DOS asm version.
		01/14/94 	sfh		Change SPEC_LEVEL to 4.2.
		02/18/94 	sfh		Exchange PSL_BUILD for ASM_BUILD.
 		02/25/94	sfh		Add parent/child model changes.
		03/10/94	sfh		Move SPEC_LEVEL to dmi.h.
        05/19/94    par         Modified to be OS independant and portable
        06/18/94    par         Changed name to conform to new naming rules
        07/23/94    par         Modified for new *.h files
				
*****************************************************************************************************************************************************/

#ifndef PSL_MH_H_FILE
#define PSL_MH_H_FILE


/********************************************************************* INCLUDES *********************************************************************/

#include "os_dmi.h"
#include "psl_tm.h"

/****************************************************************************************************************************************************/


/********************************************************************* DEFINES *********************************************************************/

/****************************************************************************************************************************************************/


/********************************************************************* TYPEDEFS *********************************************************************/

typedef struct MH_Registry 
{
    struct MH_Registry  *next;
    DMI_FUNC3_OUT       completionCallback;
    DMI_FUNC3_OUT        eventCallback;
    DMI_FUNC_IN         dmiInvoke;
    DMI_UNSIGNED        environmentId;
    void                *maContext;          /* OS dependant and supplied context information */
} MH_Registry_t;


typedef struct CI_Registry
{
    struct CI_Registry      *next;
    DMI_RegisterCiInd_t     *ciRegistry;
    DMI_UNSIGNED            environmentId;
    void                    *maContext;      /* OS dependant and supplied context information */
} CI_Registry_t;


/****************************************************************************************************************************************************/


/********************************************************************** DATA ***********************************************************************/


/****************************************************************************************************************************************************/


/*************************************************************** FUNCTION PROTOTYPES ***************************************************************/

ULONG                       MH_init(void);
ULONG                       MH_term(void);
TM_TaskStatus_t             MH_completionCallback(DMI_MgmtCommand_t _FAR *dmiCommand,void _FAR *parameter);
TM_TaskStatus_t             MH_handleEvent(DMI_Indicate_t _FAR *event,void _FAR *OS_Context);
TM_TaskStatus_t             MH_install(DMI_CiInstallData_t _FAR *dmiCommand,void _FAR *OS_Context);
TM_TaskStatus_t             MH_uninstall(DMI_CiUninstallData_t _FAR *dmiCommand,void *OS_Context);
DMI_UNSIGNED                MH_main(DMI_MgmtCommand_t _FAR *dmiCommand,void *OS_Context);
void                        MH_doEventCalls(DMI_Indicate_t _FAR *event);
DMI_RegisterCiInd_t _FAR    *MH_directLookup(DMI_UNSIGNED componentID, DMI_UNSIGNED groupID, DMI_UNSIGNED attribID, DMI_UNSIGNED environmentId);


/****************************************************************************************************************************************************/


#endif
