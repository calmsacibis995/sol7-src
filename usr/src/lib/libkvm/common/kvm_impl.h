/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _KVM_IMPL_H
#define	_KVM_IMPL_H

#pragma ident	"@(#)kvm_impl.h	2.33	97/08/12 SMI"

#include <kvm.h>
#include <kvm_kbi.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types32.h>
#include <sys/dumphdr.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* libkvm library debugging */
#if defined(_KVM_DEBUG) || defined(lint)
/* no varargs for macros, unfortunately */
#define	KVM_ERROR_1(s)		_kvm_error((s))
#define	KVM_ERROR_2(s, t)	_kvm_error((s), (t))
#define	KVM_PERROR_1(s)		_kvm_perror((s))
#define	KVM_PERROR_2(s, t)	_kvm_perror((s), (t))
extern void _kvm_error(), _kvm_perror();
#else
#define	KVM_ERROR_1(s)		/* do nothing */
#define	KVM_ERROR_2(s, t)	/* do nothing */
#define	KVM_PERROR_1(s)		/* do nothing */
#define	KVM_PERROR_2(s, t)	/* do nothing */
#endif /* _KVM_DEBUG || lint */

#define	GETKVM(a, b, m)							\
	if (kvm_kread(kd,						\
	    (uintptr_t)(a), (b), sizeof (*b)) != sizeof (*b)) {		\
		KVM_ERROR_2("error reading %s", m);			\
		return (-1);						\
	}

#define	GETKVMNULL(a, b, m)						\
	if (kvm_kread(kd,						\
	    (uintptr_t)(a), (b), sizeof (*b)) != sizeof (*b))	 {	\
		KVM_ERROR_2("error reading %s", m);			\
		return (NULL);						\
	}

#define	LIVE_NAMELIST	"/dev/ksyms"
#define	LIVE_COREFILE	"/dev/mem"
#define	LIVE_VIRTMEM	"/dev/kmem"
#define	LIVE_SWAPFILE	"/dev/drum"

#define	pagenum_offset	kvm_param->p_pagenum_offset
#define	pagesz		kvm_param->p_pagesize
#define	pageoffset	kvm_param->p_pageoffset
#define	pageshift	kvm_param->p_pageshift
#define	pagemask	kvm_param->p_pagemask
#define	kernelbase	kvm_param->p_kernelbase

extern int _kvm_physaddr(kvm_t *,
    struct as *, uintptr_t, int *, u_longlong_t *);
extern int _uncondense(kvm_t *, int, u_longlong_t *);

/*
 * Used directly by crash(1m)
 */
extern ssize_t kvm_as_read(kvm_t *, struct as *, uintptr_t, void *, size_t);
extern u_longlong_t kvm_physaddr(kvm_t *, struct as *, uintptr_t);

/*
 * Ensure that the library itself does -not- use kvm_read or kvm_write
 * directly, but uses kvm_{u,k}read and kvm_{u,k}write as appropriate.
 */
#define	kvm_read	error..
#define	kvm_write	error..

/*
 * XXX-Declaration of struct _kvmd moved to kvm.h
 * Necessary to make the contents of the _kvmd structure visible
 * to lkvm_pd.so
 */

#ifdef	__cplusplus
}
#endif

#endif	/* _KVM_IMPL_H */
