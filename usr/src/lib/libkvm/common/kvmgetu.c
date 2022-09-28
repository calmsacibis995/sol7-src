/*
 * Copyright (c) 1988, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)kvmgetu.c	2.17	97/05/24 SMI"

#include "kvm_impl.h"
#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>

struct user *
kvm_getu(kvm_t *kd, struct proc *proc)
{
	if (proc->p_stat == SZOMB)	/* zombies don't have u-areas */
		return (NULL);
	kd->uarea = &proc->p_user;

	/*
	 * As a side-effect for adb -k, initialize the user address space
	 * description (if there is one; proc 0 and proc 2 don't have
	 * address spaces).
	 */
	if (proc->p_as) {
		GETKVMNULL(proc->p_as, &kd->Uas,
			"user address space descriptor");
	}

	return (kd->uarea);
}

/*
 * XXX - do we also need a kvm_getkernelstack now that it is not part of the
 * u-area proper any more?
 */
