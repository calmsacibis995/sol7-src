/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)machkobj.c	1.2	98/02/20 SMI"

/*
 * This file contains the machine specific parts necessary for
 * module loading.
 */
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/machparam.h>
#include <sys/machkobj.h>
#include <sys/kobj.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/vmmac.h>
#include <sys/debug.h>
#include <vm/as.h>

/*
 * Return pages for module loading code
 *
 */
static caddr_t text_pool;
static caddr_t data_pool;
static kobj_free_t *textobjs;
static kobj_free_t *dataobjs;

extern caddr_t modtext;		/* start of loadable module text reserved */
extern caddr_t e_modtext;	/* end of loadable module text reserved */

extern caddr_t moddata;		/* start of loadable module data reserved */
extern caddr_t e_moddata;	/* end of loadable module data reserved */

static caddr_t mod_remap(caddr_t, size_t);

/*
 * set module pool pointer to start of managed segment
 */
void
mach_init_kobj()
{
	text_pool = modtext;
	data_pool = moddata;
	textobjs = kobj_create_managed();
	dataobjs = kobj_create_managed();
}

/* ARGSUSED */
void
mach_mod_free(caddr_t addr, size_t size, int flags)
{
	size = roundup(size, sizeof (int32_t));
	if (flags & KOBJ_TEXT) {
		ASSERT(addr > modtext);
		ASSERT(addr < e_modtext);
		kobj_manage_mem(addr, size, textobjs);
	} else {		/* must be data or bss */
		ASSERT(addr > moddata);
		ASSERT(addr < e_moddata);
		kobj_manage_mem(addr, size, dataobjs);
	}
}

/* ARGSUSED */
caddr_t
mach_mod_alloc(size_t size, int flags, reloc_dest_t *private)
{
	caddr_t		modptr = NULL;
	caddr_t		*ptr;
	kobj_free_t	*cookie;
	size_t		end;
	extern int	segkmem_ready;

	if (flags & KOBJ_TEXT) {
		ptr = &text_pool;
		cookie = textobjs;
		end = (size_t)e_modtext;
	} else {		/* must be data or bss */
		ptr = &data_pool;
		cookie = dataobjs;
		end = (size_t)e_moddata;
	}

	size = roundup(size, sizeof (int32_t));

	if (modptr = (caddr_t)kobj_find_managed_mem(size, cookie)) {
		if (segkmem_ready && (flags & KOBJ_TEXT))
			*private = (reloc_dest_t)mod_remap(modptr, size);
		if (!segkmem_ready && (flags & KOBJ_TEXT))
			*private = NULL;
		return (modptr);
	}

	if (((size_t)*ptr + size) < end) {
		modptr = *ptr;
		*ptr = *ptr + size;
		/* if this is text, do the remap dance */
		if (segkmem_ready && (flags & KOBJ_TEXT))
			*private = (reloc_dest_t)mod_remap(modptr, size);
		if (!segkmem_ready && (flags & KOBJ_TEXT))
			*private = NULL;
		return (modptr);
	}
	else
		return (NULL);  /* out of space */
}

/* ARGSUSED */
static caddr_t
mod_remap(caddr_t ptr, size_t len)
{
	return (NULL);
}

/* ARGSUSED */
void
mach_mod_epilogue(size_t size, reloc_dest_t mapped_at)
{
}
