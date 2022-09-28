/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_EPM_H
#define	_SYS_EPM_H

#pragma ident	"@(#)epm.h	1.6	96/10/11 SMI"	/* SVr4.0 */

#include <sys/sunddi.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

/*
 * epm.h:	Function prototypes and data structs for kernel pm functions.
 */

int
e_pm_props(dev_info_t *dip);

/*
 * Values used by e_pm_props and friends, found in devi_comp_flags
 */
#define	PMC_NEEDS_SR	0x400		/* do suspend/resume despite no "reg" */
#define	PMC_NO_SR	0x800		/* don't suspend/resume despite "reg" */
#define	PMC_PARENTAL_SR	0x1000		/* call up tree to suspend/resume */
#define	PMC_TSPROP	0x2000		/* uses old pm_timestamp prop */
#define	PMC_NPPROP	0x4000		/* uses old pm_norm_pwr prop */

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_EPM_H */
