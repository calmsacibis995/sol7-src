/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986, 1987, 1988, 1989, 1992  Sun Microsystems, Inc
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

#ifndef _VM_SEG_KMEM_H
#define	_VM_SEG_KMEM_H

#pragma ident	"@(#)seg_kmem.h	1.7	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * VM - Kernel Segment Driver
 */

#if defined(_KERNEL)
/*
 * These variables should be put in a place which
 * is guaranteed not to get paged out of memory.
 */
extern struct as kas;		/* kernel's address space */
extern struct seg ktextseg;	/* kernel's "most everything else" segment */
extern struct seg kvseg;	/* kernel's "sptalloc" segment */
extern struct seg E_kvseg;	/* version of the above useable by ethernet */
extern struct seg kvalloc;	/* kernel's "valloc" segment */
extern struct seg kdvmaseg;	/* kernel's "DVMA" segment */

extern struct vnode kvp;	/* kernel's vnode */

/*
 * For segkmem_create, the 2nd argument is actually a pointer to the
 * optional array of pte's used to map the given segment.
 */
extern int segkmem_create(struct seg *, void *);

/*
 * Special kernel segment operations.  NOTE: mapin() and mapout() are
 * supported for compatibility only.  Callers should be using
 * segkmem_mapin() and segkmem_mapout().
 */
extern int segkmem_alloc(struct seg *seg, caddr_t addr, size_t len,
		int canwait);
extern void segkmem_free(struct seg *seg, caddr_t addr, size_t len);
extern void segkmem_mapin(struct seg *seg, caddr_t addr, size_t len,
		uint_t vprot, uint_t pfn, int flags);
extern void segkmem_mapout(struct seg *seg, caddr_t addr, size_t len);
extern void *kmem_getpages(pgcnt_t npages, uint_t flag);
extern void kmem_freepages(void *addr, pgcnt_t npages);
extern void kmem_gc(void);
extern size_t kmem_maxvirt(void);

struct ddi_dma_attr;
extern void *kmem_getpages_io(size_t, int, int, struct ddi_dma_attr *);

#endif	/* _KERNEL */

/*
 * Usage codes.  The p_offset fields of page structs allocated through
 * segkmem_alloc() are set to one of these values to indicate what
 * the page is being used for.
 */
#define	SEGKMEM_HEAP    1
#define	SEGKMEM_STREAMS 2
#define	SEGKMEM_MBUF    3
#define	SEGKMEM_UFSBUF  4
#define	SEGKMEM_USTRUCT 5
#define	SEGKMEM_LOADMOD 6

#ifdef	__cplusplus
}
#endif

#endif	/* _VM_SEG_KMEM_H */
