/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MACH_KOBJ_H
#define	_MACH_KOBJ_H

#pragma ident	"@(#)machkobj.h	1.1	98/01/06 SMI"

#include <sys/kobj.h>
#include <sys/kobj_impl.h>

/*
 * This file contains the interface prototypes between the machine
 * specific implementation for module loading and the generic kobj
 * loading mechanism.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/kobj.h>

#if defined(_KERNEL)

typedef struct _kobj_free {
	struct _kobj_free *next;
	struct _kobj_free *prev;
	caddr_t		mem;
	size_t		size;
} kobj_free_t;

/*
 * The machine specific side
 */
void mach_init_kobj();
caddr_t mach_mod_alloc(size_t, int, reloc_dest_t *);
void mach_mod_free(caddr_t, size_t, int);
void mach_mod_epilogue(size_t, reloc_dest_t);

/*
 * The krtld side
 */
kobj_free_t *kobj_create_managed();
void kobj_manage_mem(caddr_t, size_t, kobj_free_t *);
caddr_t kobj_find_managed_mem(size_t, kobj_free_t *);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _MACH_KOBJ_H */
