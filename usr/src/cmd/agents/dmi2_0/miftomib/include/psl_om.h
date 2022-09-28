/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_om.h	1.2 96/09/24 Sun Microsystems"


/*****************************************************************************************************************************************************

    Filename: psl_om.h

    Copyright (c) Intel, Inc. 1992-1994
    Copyright (c) International Business Machines, Corp. 1994

    Description: Overlay Manager Header

    Author(s): Alvin I. Pivowar, Steve Hanrahan, Paul A. Ruocchio

    RCS Revision: $Header: n:/sl/rcs/om.h 1.24 1994/04/18 11:13:20 shanraha Exp $

    Revision History:

        Date     	Author	Description
        -------  	---		-----------------------------------------------
		3/19/93  	aip		Creation date.
		10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
		11/11/93 	sfh		Remove include of types.h.
		1/12/94  	sfh		Redo names within OverlayTable_t and function names.
		03/03/94	sfh		Add parent/child code.
		03/16/94	sfh		Add return value to OM_executeInstrumentation.
		04/18/94	sfh		Remove _pascal.
        05/19/94    par         Modified for the OS Portability
        06/18/94    par         Changed name to conform to new naming rules.

****************************************************************************************************************************************************/

#ifndef OM_H_FILE
#define OM_H_FILE

/************************************ INCLUDES **********************************************************/

#include "psl_tm.h"
#include "db_api.h"
#include "os_dmi.h"
#include "psl_mh.h"

/********************************************************************************************************/


/************************************** DEFINES *********************************************************/

#define OM_DIRECT  0    /* overlay block type                                */
#define OM_OVERLAY 1

#define OM_OVL_BUSY    1    /* status of the overlay element                     */
#define OM_OVL_IDLE    0

/********************************************************************************************************/


/************************************* TYPEDEFS *********************************************************/

enum {OM_CANCEL, OM_RUN};

typedef struct OM_PendingChain{          /* used if more then one command is requesting an overlay */
    DMI_MgmtCommand_t *Command;          /* current working command for this overlay */
    struct OM_PendingChain *Next;        /* incase there is more then one of these   */
} OM_OverlayPending_t;

typedef struct OM_OverlayTable {         /* overlay control structure        */
    struct OM_OverlayTable *next;        /* ptr to the next element in the overlay control table */
    DMI_UNSIGNED Type;                   /* either OM_DIRECT or OM_OVERLAY   */
    DMI_UNSIGNED OVStatus;               /* either OM_OVL_BUSY or OM_OVL_IDLE        */
    DMI_UNSIGNED UseCount;               /* a count of the number of commands waiting for this overlay */
    OM_OverlayPending_t *Pending;        /* a list of the pending commands for this overlay */
    MIF_Id_t     componentId;            /* component ID of this overlay     */
    DMI_STRING   *symbolicName;          /* the symbolic name for this overlay */
    DMI_STRING   *pathName;              /* the actual pathname of this overlay (os unique) */
    void         *OSdata;                /* anything that is unique for this OS environment */
} OM_OverlayTable_t;

/********************************************************************************************************/

/************************************** DATA ************************************************************/


/********************************************************************************************************/

/******************************* FUNCTION PROTOTYPES ****************************************************/

TM_TaskStatus_t             OM_loader(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter);
TM_TaskStatus_t             OM_direct(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *junk);
TM_TaskStatus_t             OM_run(DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *h);
TM_TaskStatus_t             OM_executeInstrumentation(DMI_MgmtCommand_t _FAR *dmiCommand, DMI_InstrumentationVector_t  _FAR *instrumentationVector);
void                        OM_cancel(DMI_UNSIGNED command, DMI_UNSIGNED maHandle, DMI_UNSIGNED cmdHandle);
void                        OM_FreeOverlay(OM_OverlayTable_t *Overlay);
void                        OM_OverlayCleanup(void);
DMI_UNSIGNED DMI_FUNC_ENTRY OM_responseDone(DMI_Confirm_t _FAR *dmiCommand);
/*********************************************************************************************************/

#endif
