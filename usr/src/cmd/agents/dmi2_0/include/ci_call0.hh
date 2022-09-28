#ifndef _CI_CALLBACK_SVC_HH
#define _CI_CALLBACK_SVC_HH
/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)ci_callback_svc.hh	1.2 96/10/07 Sun Microsystems"

#ifdef __cplusplus
extern "C" {
#endif 	

extern u_long reg_ci_callback();
extern void *start_svc_run_thread(void *arg); 

#ifdef __cplusplus
}
#endif 	

#endif
