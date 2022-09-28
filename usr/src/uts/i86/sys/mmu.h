/*
 * Copyright (c) 1990,1992,1997-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MMU_H
#define	_SYS_MMU_H

#pragma ident	"@(#)mmu.h	1.22	98/01/14 SMI"


#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions for the Intel 80x86 MMU
 */

/*
 * Page fault error code, pushed onto stack on page fault exception
 */
#define	MMU_PFEC_P		0x1	/* Page present */
#define	MMU_PFEC_WRITE		0x2	/* Write access */
#define	MMU_PFEC_USER		0x4	/* User mode access */

/* Access types based on above error codes */
#define	MMU_PFEC_AT_MASK	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_UREAD	MMU_PFEC_USER
#define	MMU_PFEC_AT_UWRITE	(MMU_PFEC_USER|MMU_PFEC_WRITE)
#define	MMU_PFEC_AT_SREAD	0
#define	MMU_PFEC_AT_SWRITE	MMU_PFEC_WRITE

#define	MMU_STD_SRX		0
#define	MMU_STD_SRWX		1
#define	MMU_STD_SRXURX		2
#define	MMU_STD_SRWXURWX	3

#if defined(_KERNEL) && !defined(_ASM)

extern int valid_va_range(caddr_t *, size_t *, size_t, int);

#endif /* defined(_KERNEL) && !defined(_ASM) */

/*
 * Page directory and physical page parameters
 */
#ifndef MMU_PAGESIZE
#define	MMU_PAGESIZE	4096
#endif



#define	MMU_STD_PAGESIZE	MMU_PAGESIZE
#define	MMU_STD_PAGEMASK	0xFFFFF000
#define	MMU_STD_PAGESHIFT	12
#define	MMU_STD_SEGSHIFT	22



#define	TWOMB_PAGESIZE		0x200000
#define	TWOMB_PAGEOFFSET	(TWOMB_PAGESIZE - 1)
#define	TWOMB_PAGESHIFT		21
#define	FOURMB_PAGESIZE		0x400000
#define	FOURMB_PAGEOFFSET	(FOURMB_PAGESIZE - 1)
#define	FOURMB_PAGESHIFT	22
#define	FOURMB_PAGEMASK		(~FOURMB_PAGEOFFSET)

#define	HAT_INVLDPFNUM		0xffffffff

#ifndef	PTE36

#define	PTE_VALID	0x01
#define	PTE_LARGEPAGE	0x80
#define	PTE_SRWX	0x02
/*
 * The following defines MMU constants in 32 bit pte mode
 */
#define	NPTEPERPT	1024	/* entries in page table */
#define	NPTESHIFT	10
#define	PTSIZE		(NPTEPERPT * MMU_PAGESIZE)	/* bytes mapped */
#define	MMU_NPTE_ONE	1024 /* 1024 PTE's in first level table */
#define	MMU_NPTE_TWO	1024 /* 1024 PTE's in second level table */
#define	MMU_L1_VA(a)	(((uint_t)(a)) << 22)
#define	MMU_L1_INDEX(a) (((uint_t)(a)) >> 22)
#define	MMU_L2_INDEX(a) ((((uint_t)(a)) >> 12) & 0x3ff)
#define	PAGETABLE_INDEX(a)	MMU_L2_INDEX(a)
#define	PAGEDIR_INDEX(a)	MMU_L1_INDEX(a)
#define	PTE_PFN(p)	(((uint32_t)(p)) >> 12)

#endif		/* PTE36 */

#define	IN_SAME_4MB_PAGE(a, b)	(MMU_L1_INDEX(a)  ==  MMU_L1_INDEX(b))
#define	FOURMB_PDE(a, g,  b, c) \
	((((uint32_t)((uint_t)(a))) << MMU_STD_PAGESHIFT) |\
	((g) << 8) | PTE_LARGEPAGE |(((b) & 0x03) << 1) | (c))

#ifndef _ASM
#define	mmu_tlbflush_all()	reload_cr3()

/* Low-level functions */
extern void mmu_tlbflush_entry(caddr_t);
extern uint_t cr3(void);
extern void reload_cr3(void);
extern void setcr3(uint_t);
extern void mmu_loadcr3(struct cpu *, void *);
extern void setup_kernel_page_directory(struct cpu *);
extern void setup_vaddr_for_ppcopy(struct cpu *);
extern void clear_bootpde(struct cpu *);
#endif /* !_ASM */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_MMU_H */
