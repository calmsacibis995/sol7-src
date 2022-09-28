/*
 * Copyright (c) 1996-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _MACH_PAGE_H
#define	_MACH_PAGE_H

#pragma ident	"@(#)mach_page.h	1.9	98/01/26 SMI"

#include <vm/page.h>
/*
 * The file contains the platform specific page structure
 */

#ifdef __cplusplus
extern "C" {
#endif


#ifdef	NUMA

/*
 * p_flag definition
 *
 * 31 ........ 16-15 .........4-3.........0
 * |flag values  | reserved    | node id |
 *
 * The lower 4 bits of p_flag contain the nodeid.
 * The upper 16 bits hold the bitmask of nodes on which this
 * page has not been replicated yet. p_flag is protected
 * by the p_inuse bit.
 *
 */

/* p_flag values */
#define	P_NODEMASK	0xf
#define	P_REPLICATE	0xffff0000
					/*
					 * bit mask of nodes that need
					 * replicated page
					 */
#define	P_NODESHIFT	16

#define	p_mapping	p_mapinfo->hmep
#define	p_shadow	p_mapinfo->shadow /* A linked list of local pages */
#define	p_flag		p_mapinfo->flag	/* lower 4 bits hold node-id */

typedef struct ppmap {
	struct hment	*hmep;
	struct page	*shadow;
	uint32_t	flag;
} ppmap_t;
#endif
/*
 * The PSM portion of a page structure
 */
typedef struct machpage {
	page_t	p_paget;		/* PIM portion of page_t */
#ifdef	NUMA
	ppmap_t	*p_mapinfo;
#else
	struct	hment *p_mapping;	/* hat specific translation info */
#endif
	uint_t	p_pagenum;		/* physical page number */
	uint_t	p_share;		/* number of mappings to this page */
	kcondvar_t p_mlistcv;		/* cond var for mapping list lock */
	uint_t	p_inuse: 1,		/* page mapping list in use */
		p_wanted: 1,		/* page mapping list wanted */
		p_impl: 1,		/* hat implementation bits */
		p_kpg: 1,		/* page mapped by kvseg */
		p_noutsed: 4;
	uchar_t	p_nrm;			/* non-cache, ref, mod readonly bits */
} machpage_t;

/*
 * Each segment of physical memory is described by a memseg struct. Within
 * a segment, memory is considered contiguous. The segments form a linked
 * list to describe all of physical memory.
 */
struct memseg {
	machpage_t *pages, *epages;	/* [from, to] in page array */
	uint_t pages_base, pages_end;	/* [from, to] in page numbers */
	struct memseg *next;		/* next segment in list */
};

extern struct memseg *memsegs;		/* list of memory segments */

void build_pfn_hash();

#ifdef __cplusplus
}
#endif

#endif /* _MACH_PAGE_H */
