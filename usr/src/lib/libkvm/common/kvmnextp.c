/*
 * Copyright (c) 1987, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)kvmnextproc.c	2.12	97/05/24 SMI"

#include "kvm_impl.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/proc.h>

struct proc *
kvm_nextproc(kvm_t *kd)
{
	/* if no proc buf, allocate one */
	if (kd->pbuf == NULL) {
		kd->pbuf = malloc(sizeof (struct proc));
		if (kd->pbuf == NULL) {
			KVM_PERROR_1("can't allocate proc cache");
			return ((struct proc *)-1);
		}
	}

	if (kd->pnext != NULL) {
		if (kvm_kread(kd, (uintptr_t)kd->pnext, kd->pbuf,
		    sizeof (struct proc)) != sizeof (struct proc))
			return (NULL);
		kd->pnext = kd->pbuf->p_next;
		return (kd->pbuf);
	}
	return (NULL);
}

struct proc *
kvm_getproc(kvm_t *kd, pid_t pid)
{
	int n;
	struct pid pidbuf;
	struct proc *procp;

	/* if no proc buf, allocate one */
	if (kd->pbuf == NULL) {
		kd->pbuf = malloc(sizeof (struct proc));
		if (kd->pbuf == NULL) {
			KVM_PERROR_1("can't allocate proc cache");
			return ((struct proc *)-1);
		}
	}

	for (n = 0, procp = kd->proc; n < kd->nproc && procp != NULL;
	    procp = kd->pbuf->p_next) {
		if (kvm_kread(kd, (uintptr_t)procp, kd->pbuf,
		    sizeof (*procp)) != sizeof (*procp))
			return (NULL);
		/*
		 * pid is now in separate struct.
		 */
		if (kvm_kread(kd, (uintptr_t)kd->pbuf->p_pidp,
		    &pidbuf, sizeof (pidbuf)) != sizeof (pidbuf))
			return (NULL);
		if ((kd->pbuf->p_stat != 0) && (pidbuf.pid_id == pid))
			return (kd->pbuf);
	}
	return (NULL);
}
