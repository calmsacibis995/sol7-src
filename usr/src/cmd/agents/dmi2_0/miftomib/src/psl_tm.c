/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)psl_tm.c	1.2 96/09/24 Sun Microsystems"


#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */


/******************************** Task List funcitons ***********************************************/

SL_TASK tasks[] = { (SL_TASK)DH_main, (SL_TASK)OM_executeInstrumentation, (SL_TASK)OM_loader,
                    (SL_TASK)OM_direct, (SL_TASK)OM_run, (SL_TASK)MH_completionCallback,
                    (SL_TASK)MH_handleEvent,(SL_TASK)MH_install,(SL_TASK)MH_uninstall};


/****************************************************************************************************/

