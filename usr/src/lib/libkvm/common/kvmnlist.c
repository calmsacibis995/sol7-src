/*
 * Copyright (c) 1987, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)kvmnlist.c	2.14	97/05/24 SMI"

#include "kvm_impl.h"
#include <nlist.h>

/*
 * Look up a set of symbols in the running system namelist (default: ksyms)
 */
int
kvm_nlist(kvm_t *kd, struct nlist nl[])
{
	int e;

	if (kd->namefd == -1) {
		KVM_ERROR_1("kvm_nlist: no namelist descriptor");
		return (-1);
	}

	e = nlist(kd->name, nl);

	if (e == -1) {
		KVM_ERROR_2("bad namelist file %s", kd->name);
	}

	return (e);
}
