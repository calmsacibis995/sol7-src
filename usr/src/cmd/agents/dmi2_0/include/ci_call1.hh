#ifndef _CI_CALLBACKS_HH
#define _CI_CALLBACKS_HH
/* Copyright 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)ci_callbacks.hh	1.3 96/07/23 Sun Microsystems"

extern void
CiGetAttribute(CiGetAttributeIN *argp, CiGetAttributeOUT *result);

extern void
CiGetNextAttribute(CiGetNextAttributeIN *argp, CiGetNextAttributeOUT *result); 

extern void
CiSetAttribute(CiSetAttributeIN *argp, DmiErrorStatus_t *result); 

extern void
CiReserveAttribute(CiReserveAttributeIN *argp, DmiErrorStatus_t *result); 

extern void
CiReleaseAttribute(CiReleaseAttributeIN *argp, DmiErrorStatus_t *result); 

extern void
CiAddRow(CiAddRowIN *argp, DmiErrorStatus_t *result); 

extern void
CiDeleteRow(CiDeleteRowIN *argp, DmiErrorStatus_t *result); 

#endif
