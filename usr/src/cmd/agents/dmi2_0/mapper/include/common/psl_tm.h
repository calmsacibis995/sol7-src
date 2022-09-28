/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_tm.h	1.1 96/08/05 Sun Microsystems"

/****************************************************************************************************
    Filename: psl_tm.h
    

    Description: Task Manager Header

    Author(s): Alvin I. Pivowar, Steve Hanrahan, Paul A. Ruocchio

	RCS Revision: $Header: n:/sl/rcs/psl_tm.h 1.15 1994/03/04 12:33:42 shanraha Exp $

    Revision History:

        Date     	Author Description
        ----	  	---    -----------
        3/12/93  	aip		Initial build and test.
		10/28/93 	sfh		Change dos_dmi.h to os_dmi.h.
		11/11/93 	sfh		Remove include of types.h.
		11/16/93 	sfh		Add TM_EVENT.
		03/04/94	sfh		Add parent/child code.
		04/11/94	sfh		Move TM_UNLOAD to end of TM_Task_t.
        05/19/94    par         Modified for true portability (new MH.C, OM.C, TM.C)
        06/18/94    par         Changed name to conform to new naming rules

 
*****************************************************************************************************/

#ifndef PSL_TM_H_FILE
#define PSL_TM_H_FILE


/********************************* INCLUDES *********************************************************/

#include "os_dmi.h"
#include "psl_main.h"

/****************************************************************************************************/


/************************************** DEFINES *****************************************************/

/****************************************************************************************************/


/************************************* TYPEDEFS *****************************************************/

typedef enum {TM_UNKNOWN_STATUS, TM_DONE, TM_DONE_CALLBACK, TM_NOT_DONE, TM_IDLE} TM_TaskStatus_t;

typedef enum {TM_DB, TM_EXECUTE_INSTRUMENTATION, TM_CALL_COMPONENT, TM_DIRECT, TM_RUN_COMPONENT, 
              TM_CALLBACK, TM_EVENT,TM_INSTALL,TM_UNINSTALL,TM_UNLOAD} TM_Task_t;

typedef TM_TaskStatus_t (*SL_TASK)(DMI_MgmtCommand_t _FAR *, void _FAR *);

typedef struct TM_TaskList {
            SL_TASK             SL_task;         /* next internal task to execute */
            DMI_MgmtCommand_t   *dmiCommand;     /* SL working command block */
            ULONG               iCommand;        /* the DMI command for this packet */
            ULONG               iComponentID;    /* component ID for this request */
#ifdef _WIN32
			ULONG               iMagicValue;
#endif
            void                *TempCnfBuffer;  /* If non-NULL, this is a temp confirm buffer allocated by the SL */
            USHORT              CancelFlag;      /* used to cancel this packet if needed */
            void                *parameter;      /* opaque data, used by the Parent/Child stuff */
            void                *CurrentOverlay; /* the overlay that is currently in use by this request */
            struct TM_TaskList  *next;
} TM_TaskList_t;

/****************************************************************************************************/


/********************************* FUNCTION PROTOTYPES **********************************************/

void           TM_cancel(DMI_UNSIGNED command, DMI_UNSIGNED maHandle, DMI_UNSIGNED cmdHandle);
ULONG          TM_main(void);
boolean        TM_taskAdd(TM_Task_t SL_task, DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter);
TM_TaskList_t *TM_taskSwitch(TM_Task_t SL_task, DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter);
ULONG          TM_init(void);    /* tasker initialization function in TM.C          */
ULONG          TM_term(void);    /* tasker shutdown and cleanup function     */
void           TM_taskRequeue(TM_Task_t SL_task, DMI_MgmtCommand_t _FAR *dmiCommand, void _FAR *parameter);
boolean        TM_validPacket(DMI_MgmtCommand_t *NewPacket);   /* is this packet ID valid */


/****************************************************************************************************/

#endif
