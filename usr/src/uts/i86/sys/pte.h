/*
 * Copyright (c) 1991,1992,1997-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PTE_H
#define	_SYS_PTE_H

#pragma ident	"@(#)pte.h	1.15	98/01/09 SMI"

/*
 * Copyright (c) 1991, 1992, Sun Microsystems, Inc. All Rights Reserved.
 * Sun considers its source code as an unpublished, proprietary
 * trade secret, and it is available only under strict license
 * provisions. This copyright notice is placed here only to protect
 * Sun in the event the source is deemed a published work. Disassembly,
 * decompilation, or other means of reducing the object code to human
 * readable form is prohibited by the license agreement under which
 * this code is provided to the user or company in possession of this
 * copy
 *
 * RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the
 * Government is subject to restrictions as set forth in subparagraph
 * (c) (1) (ii) of the Rights in Technical Data and Computeer Software
 * clause at DFARS 52.227-7013 and in similar clauses in the FAR and
 * NASA FAR Supplement
 */

#ifndef _ASM
#include <sys/types.h>
#include <sys/mmu.h>
#endif /* _ASM */

#ifdef	__cplusplus
extern "C" {
#endif


#define	PTE_RM_MASK		0x60
#define	PTE_REF_MASK		0x20
#define	PTE_MOD_MASK		0x40
#define	PTE_RM_SHIFT		5
#define	PTE_PERMS(b)		(((b) & 0x3) << 1)
#define	PTE_PERMMASK		((0x3 << 1))
#define	PTE_PERMSHIFT		(1)
#define	PTE_WRITETHRU(w)	(((w) & 0x1) << 3)
#define	PTE_NONCACHEABLE(d)	(((d) & 0x1) << 4)
#define	PTE_REF(r)		(((r) & 0x1) << 5)
#define	PTE_MOD(m)		(((m) & 0x1) << 6)
#define	PTE_OSRESERVED(o)	(((o) & 0x7) << 9)
#define	pte_valid(pte) ((pte)->Present)


#define	MAKE_PROT(v)		PTE_PERMS(v)
#define	PG_PROT			MAKE_PROT(0x3)
#define	PG_KW			MAKE_PROT(MMU_STD_SRWX)
#define	PG_KR			MAKE_PROT(MMU_STD_SRX)
#define	PG_UW			MAKE_PROT(MMU_STD_SRWXURWX)
#define	PG_URKR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UR			MAKE_PROT(MMU_STD_SRXURX)
#define	PG_UPAGE		PG_KW	/* Intel u pages not user readable */



#define	PG_V			1

#ifndef _ASM

#ifdef	PTE36

typedef	uint64_t *	pteptr_t;
typedef uint64_t	pteval_t;
#define	cpu_pagedir	cpu_pagedir64

/*
 * generate a pte for the specified page frame, with the
 * specified permissions, possibly cacheable.
 * (unsigned long long ((unsigned int)(ppn))) ensures that
 * the sign bit of ppn is not propagated during the shift
 * operation even if ppn is defined to be of type int.
 */

#define	MRPTEOF(ppn, os, m, r, c, w, a, p)	\
	((((uint64_t)((uint32_t)(ppn)))<<12)|\
	((os)<<9)|((m)<<6)|((r)<<5)|	\
	((!(c))<<4)|((w)<<3)|((a)<<1)|p)

#define	PTEOF(p, a, c)	MRPTEOF(p, 0, 0, 0, c, 0, a, 1)

#define	PTBL_ENT(p) (((uint64_t)((uint32_t)(p))<<12)|0x01)
#define	PTEOF_C(p, a)\
	(((uint64_t)((uint32_t)(p))<<12)|((a)<<1)|1)
#define	PTEOF_CS(p, a, s)\
	(((uint64_t)((uint32_t)(p))<<12)|((a)<<1)|((s) << 9)|1)
typedef struct pte32 {
	uint32_t Present:1;
	uint32_t AccessPermissions:2;
	uint32_t WriteThru:1;
	uint32_t NonCacheable:1;
	uint32_t Referenced:1;
	uint32_t Modified:1;
	uint32_t MustBeZero:2;
	uint32_t OSReserved:3;
	uint32_t PhysicalPageNumber:20;
} pte32_t;


typedef struct pte {
	uint32_t Present:1;
	uint32_t AccessPermissions:2;
	uint32_t WriteThru:1;
	uint32_t NonCacheable:1;
	uint32_t Referenced:1;
	uint32_t Modified:1;
	uint32_t MustBeZero:2;
	uint32_t OSReserved:3;
	uint32_t PhysicalPageNumberL:20;
	uint32_t PhysicalPageNumberH;
					/*
					 * An easy way to ensure that
					 * reserved bits are zero.
					 */
} pte_t;
struct  pte64 {
	uint32_t	pte64_0_31;
	uint32_t	pte64_32_64;
};

#define	LOAD_PTE(a, b) \
	((struct pte64 *)(a))->pte64_32_64 = (((uint64_t)(b)) >> 32); \
		((struct pte64 *)(a))->pte64_0_31 = (b)
#define	PTE_PFN(p)		(((uint64_t)(p)) >> 12)
#define	PTE32_PFN(p)		(((uint32_t)(p)) >> 12)
#define	pte_valid(pte) ((pte)->Present)
#define	pte32_valid(pte) ((pte)->Present)

#define	pte_konly(pte)		(((*(uint64_t *)(pte)) & 0x4) == 0)
#define	pte_ronly(pte)		(((*(uint64_t *)(pte)) & 0x2) == 0)
#define	pte_cacheable(pte)	(((*(uint64_t *)(pte)) & 0x10) == 0)
#define	pte_writethru(pte)	(((*(uint64_t *)(pte)) & 0x8) == 0)
#define	pte_accessed(pte)	((*(uint64_t *)(pte)) & 0x20)
#define	pte_dirty(pte)		((*(uint64_t *)(pte)) & 0x40)
#define	pte_accdir(pte)		((*(uint64_t *)(pte)) & 0x60)
#define	MAKE_PFNUM(a)	(*((uint64_t *)(a)) >> 12)
#define	MMU_PTTBL_SIZE		32
#define	MMU_L1_INDEX(a) (((uint_t)(a)) >> 21)
#define	MMU_L1_KINDEX(a) (((uint_t)((a) - 0xc0000000)) >> 21)
#define	MMU_L2_INDEX(a) ((((uint_t)(a)) >> 12) & 0x1ff)
#define	MMU32_L1_VA(a)	(((uint_t)(a)) << 22)
#define	MMU32_L1_INDEX(a) (((uint_t)(a)) >> 22)
#define	MMU32_L2_INDEX(a) ((((uint_t)(a)) >> 12) & 0x3ff)
#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)
#define	PAGETABLE32_INDEX(a)	MMU32_L2_INDEX(a)
#define	PAGEDIR32_INDEX(a)	MMU32_L1_INDEX(a)
#define	two_mb_page(pte)		(*((uint64_t *)(pte)) & 0x80)
#define	four_mb_page(pte)		(*((uint32_t *)(pte)) & 0x80)
#define	IN_SAME_2MB_PAGE(a, b)	(MMU_L1_INDEX(a) == MMU_L1_INDEX(b))
#define	TWOMB_PDE(a, g,  b, c) \
	((((uint64_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | 0x80 |(((b) & 0x03) << 1) | (c))

#define	PTE_GRDA	0x1e0
#define	MOVPTE32_2_PTE64(a, b) ((*((uint64_t *)(b))) =\
			    ((uint64_t)(*((uint32_t *)(a)) & ~PTE_GRDA)))
#define	MOV4MBPTE32_2_2MBPTE64(a, b) ((*((uint64_t *)(b))) =\
			    (((uint64_t)(*((uint32_t *)(a)) & ~PTE_GRDA))|\
			    0x80))
#define	MMU_STD_INVALIDPTE	((uint64_t)0)
#define	NPDPERAS	4
#define	NPTEPERPT	512	/* entries in page table */
#define	NPTESHIFT	9
#define	PTSIZE		(NPTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	PTOFFSET	(PTSIZE - 1)
#define	NPTEPERPT32	1024	/* entries in page table */
#define	NPTESHIFT32	10
#define	PTSIZE32	(NPTEPERPT32 * MMU_PAGESIZE)	/* bytes mapped */
#define	MMU_NPTE_ONE	2048 /* 2048 PTE's in first level table */
#define	MMU_NPTE_TWO	512  /* 512 PTE's in second level table */
#define	MMU32_NPTE_ONE	1024 /* 1024 PTE's in first level table */
#define	MMU32_NPTE_TWO	1024 /* 1024 PTE's in second level table */
#define	TWOMB_PAGESIZE		0x200000
#define	TWOMB_PAGEOFFSET	(TWOMB_PAGESIZE - 1)
#define	TWOMB_PAGESHIFT		21
#define	LARGE_PAGESHIFT		21

extern uint64_t *kernel_only_pttbl;
#define	mmu_pgdirptbl_load(cpu, index, value)\
	((cpu)->cpu_pgdirpttbl[(index)] = (uint64_t)(value))
#define	mmu_pdeptr_cpu(cpu, addr) \
	((uint64_t *)(&((cpu)->cpu_pagedir[MMU_L1_INDEX(addr)])))
#define	mmu_pdeload_cpu(cpu, addr, value) \
	(*(uint64_t *)(mmu_pdeptr_cpu(cpu, addr)) = (uint64_t)(value))
#define	mmu_pdeptr(addr)		mmu_pdeptr_cpu(CPU, addr)
#define	mmu_pdeload(addr, value)	mmu_pdeload_cpu(CPU, addr, value)

extern void user_pde64_clear(uint64_t *, uint64_t *, uint_t *);


#else		/* PTE36 */



typedef	uint_t *	pteptr_t;
typedef uint_t		pteval_t;
#define	cpu_pagedir	cpu_pagedir32

/*
 * generate a pte for the specified page frame, with the
 * specified permissions, possibly cacheable.
 */

#define	MRPTEOF(ppn, os, m, r, c, w, a, p)	\
	(((ppn)<<12)|((os)<<9)|((m)<<6)|((r)<<5)|	\
	((!(c))<<4)|((w)<<3)|((a)<<1)|p)

#define	PTEOF(p, a, c)	MRPTEOF(p, 0, 0, 0, c, 0, a, 1)
#define	PTEOF_C(p, a)		(((p)<<12)|((a)<<1)|1)
#define	PTEOF_CS(p, a, s)	(((p)<<12)|((a)<<1)|((s) << 9)|1)
typedef struct pte {
	uint_t Present:1;
	uint_t AccessPermissions:2;
	uint_t WriteThru:1;
	uint_t NonCacheable:1;
	uint_t Referenced:1;
	uint_t Modified:1;
	uint_t MustBeZero:2;
	uint_t OSReserved:3;
	uint_t PhysicalPageNumber:20;
} pte_t;

#define	pte32_t	pte_t

#define	LOAD_PTE(a, b)		(*(pteptr_t)(a) = b)
#define	pte_konly(pte)		(((*(uint_t *)(pte)) & 0x4) == 0)
#define	pte_ronly(pte)		(((*(uint_t *)(pte)) & 0x2) == 0)
#define	pte_cacheable(pte)	(((*(uint_t *)(pte)) & 0x10) == 0)
#define	pte_writethru(pte)	(((*(uint_t *)(pte)) & 0x8) == 0)
#define	pte_accessed(pte)	((*(uint_t *)(pte)) & 0x20)
#define	pte_dirty(pte)		((*(uint_t *)(pte)) & 0x40)
#define	pte_accdir(pte)		((*(uint_t *)(pte)) & 0x60)

#define	MAKE_PFNUM(a)	\
	(((struct pte *)(a))->PhysicalPageNumber)

#define	MMU_L2_VA(a)	((a) << 12)
#define	four_mb_page(pte)		(*((uint_t *)(pte)) & 0x80)
#define	MMU_STD_INVALIDPTE	(0)
#define	PTOFFSET	(PTSIZE - 1)
#define	MMU32_L1_INDEX 		MMU_L1_INDEX
#define	MMU32_L2_INDEX 		MMU_L2_INDEX
#define	pte32_valid		pte_valid
#define	mmu_pdeptr_cpu(cpu, addr) \
	&((cpu)->cpu_pagedir[(uint_t)(addr) >> 22])
#define	mmu_pdeload_cpu(cpu, addr, value) \
	(*(uint_t *)(mmu_pdeptr_cpu(cpu, addr)) = (uint_t)(value))
#define	mmu_pdeptr(addr)		mmu_pdeptr_cpu(CPU, addr)
#define	mmu_pdeload(addr, value)	mmu_pdeload_cpu(CPU, addr, value)

#define	LARGE_PAGESHIFT		22

extern void user_pde_clear(uint_t *, uint_t *, uint_t *);

#endif	/* PTE36 */

#endif /* !_ASM */


#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_PTE_H */
