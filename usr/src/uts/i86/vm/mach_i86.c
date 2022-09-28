/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mach_i86mmu.c	1.42	98/02/07 SMI"

#include <sys/t_lock.h>
#include <sys/msgbuf.h>
#include <sys/memlist.h>
#include <sys/cpu.h>
#include <sys/cpuvar.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/kmem.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vm_machparam.h>
#include <sys/tss.h>
#include <sys/vnode.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/hat_i86.h>
#include <sys/systm.h>
#include <sys/archsystm.h>
#include <sys/sunddi.h>
#include <sys/ddidmareq.h>

static int lomem_check_limits(ddi_dma_attr_t *attr, u_int lo, u_int hi);

/*
 * External Data
 */
extern caddr_t econtig;		/* end of first block of contiguous kernel */
extern caddr_t eecontig;	/* end of segkp, which is after econtig */
extern struct memlist *virt_avail;
extern int segkmem_ready;
extern u_int	phys_syslimit;
extern	struct hwptepage *hat_khwppalloc(void);



/*
 * This procedure is callable only while the page directory and page
 * tables are mapped with their virtual addresses equal to their
 * physical addresses, i.e., while segkmem_ready == 0.
 * Boot is in control and it uses 32 bit pagetable
 * entries that support pages of size 4K and 4Mb.
 *
 */
u_int
va_to_pfn(u_int vaddr)
{
	register pte32_t *ptep;
	u_int	pdir_entry, _4mb_pfn;

	ASSERT(segkmem_ready == 0);

	ptep = (pte32_t *)((cr3() & MMU_STD_PAGEMASK)) + MMU32_L1_INDEX(vaddr);
	if (!pte32_valid(ptep))
		return ((uint32_t)-1);
	if (four_mb_page(ptep)) {
		pdir_entry = *(uint32_t *)ptep;
		_4mb_pfn = pdir_entry >> MMU_STD_PAGESHIFT;
		_4mb_pfn += MMU32_L2_INDEX(vaddr);
		return (_4mb_pfn);

	} else {
		ptep = ((pte32_t *)
			(ptep->PhysicalPageNumber<<MMU_STD_PAGESHIFT)) +
					MMU32_L2_INDEX(vaddr);
		return (pte32_valid(ptep) ?
			(int)(ptep->PhysicalPageNumber) : (u_int)-1);
	}

}

#ifdef	PTE36

uint64_t *kernel_only_pttbl;
static pte_t *alloc_two_ptepages(u_int, pte_t *, pte32_t *);
static pte_t *alloc_two_ptelgpages(u_int, pte_t *, pte32_t *);

void
hat_kern_setup()
{
	struct	hat	*hat;
	pte32_t *bootpte;
	pte_t	*kernpte, *pte;
	register int i;
	u_int vaddr, enable_pae_phyaddr;
	extern struct tss386 dftss;
	extern int kadb_is_running;
	extern void setup_121_andcall();
	extern void enable_pae();
	pteptr_t pagedir, pagetbl;
	pteval_t pagedirvalue;

	/*
	 * Initialize the rest of the kas i86mmu hat.
	 */
	hat = kas.a_hat;
	hat->hat_cpusrunning = 1;


	/*
	 * Copy boot page tables that map the kernel into
	 * the kernel's hat structures.
	 * we convert 32 bit ptes to 64 bit ptes.
	 * All kernel segments are 4Mb aligned.
	 */
	bootpte = (pte32_t *)(cr3() & MMU_STD_PAGEMASK) + MMU32_NPTE_ONE - 1;
	kernpte = (pte_t *)(CPU->cpu_pagedir) + MMU_NPTE_ONE - 1;
	for (i = MMU32_NPTE_ONE; --i >= 0; --bootpte, kernpte -= 2) {
		vaddr = MMU32_L1_VA(i);

		if (vaddr < (u_int)KERNELBASE) {
			/*
			 * Invalidate all level-1 entries below KERNELBASE and
			 * those which are otherwise unused except boot code
			 */
			if (!pte32_valid(bootpte)) {
				*(uint64_t *)kernpte = MMU_STD_INVALIDPTE;
				*(uint64_t *)(kernpte - 1) = MMU_STD_INVALIDPTE;
			} else {
				/*
				 * This should be boot code, we need to map this
				 */
				pte = alloc_two_ptepages(vaddr, kernpte,
				    (pte32_t *)(bootpte->PhysicalPageNumber *
				    MMU_STD_PAGESIZE));
			}
			continue;
		}
		if (pte32_valid(bootpte)) {
			/*
			 * If this is a 4Mb page, then we map this
			 * as two 2Mb page.
			 */
			if (four_mb_page(bootpte)) {
				pte = alloc_two_ptelgpages(vaddr, kernpte,
						bootpte);
			} else {
				pte = alloc_two_ptepages(vaddr, kernpte,
				    (pte32_t *)(bootpte->PhysicalPageNumber *
				    MMU_STD_PAGESIZE));
			}
		} else if (vaddr < (u_int)eecontig		||
		    ((vaddr > (u_int)(SYSBASE - PTSIZE)) &&
		    (vaddr < phys_syslimit)) 			||
		    (vaddr == (u_int)PROMSTART)			||
		    ((vaddr == (u_int)DEBUGSTART) && kadb_is_running))
			/* Just allocate ptes for used virtual space */
			pte = alloc_two_ptepages(vaddr, kernpte, NULL);
		if (vaddr == (u_int)KERNELBASE)
			KERNELmap = pte;
		else if (vaddr == (u_int)Sysbase)
			Sysmap1 = pte;
		else if ((vaddr == (u_int)PROMSTART)	||
		    ((vaddr == (u_int)DEBUGSTART) && kadb_is_running))
			Sysmap2 = pte;
	}

	for (i = 0, vaddr = (u_int)CPU->cpu_pagedir; i < NPDPERAS;
	    i++, vaddr += MMU_PAGESIZE) {
		CPU->cpu_pgdirpttbl[i] = CPU->cpu_pgdirpttblent[i] =
		    PTBL_ENT(va_to_pfn(vaddr));
		kernel_only_pttbl[i] = MMU_STD_INVALIDPTE;
	}
	kernel_only_pttbl[NPDPERAS - 1] =
	    PTBL_ENT(va_to_pfn((u_int)kernel_only_pagedir));

	/*
	 * enable_pae() is called to switch from 32 bit pte's to 64 bit pte's.
	 * We need to setup one-to-one mapping for this function in
	 * the 64 bit pte  pagetable.
	 * The function setup_121_andcall() does the same in the 32 bit pte
	 * pagetable. When we disable paging in enable_pae(), we would be using
	 * 32 bit pte's. When we enable paging after having set PAE bit
	 * we would be using 64 bit pte's.
	 * we use kernel_only_pagedir as second level pagetable
	 * There is an assumption here that the physical address of the
	 * function enable_pae (enable_pae_phyaddr) is not above KERNELBASE.
	 * The system will page fault, if the following is true
	 * MMU_L1_INDEX(enable_pae_phyaddr) == MMU_L1_INDEX(CPU->cpu_pagedir)
	 * The problem is that the kernel picks the virtual address and
	 * boot picks the physical address. It would be nice to request
	 * boot to allocate a given physical page.
	 */
	enable_pae_phyaddr = (va_to_pfn((u_int)enable_pae) << MMU_STD_PAGESHIFT)
				+ ((u_int)enable_pae & PAGEOFFSET);
	pagedir = CPU->cpu_pagedir;
	pagetbl = (pteptr_t)kernel_only_pagedir;
	pagedirvalue = pagedir[MMU_L1_INDEX(enable_pae_phyaddr)];
	pagedir[MMU_L1_INDEX(enable_pae_phyaddr)] =
	    PTEOF_C(va_to_pfn((u_int)kernel_only_pagedir), MMU_STD_SRWXURWX);
	pagetbl[MMU_L2_INDEX(enable_pae_phyaddr)] =
	    PTEOF_C(va_to_pfn((u_int)enable_pae), MMU_STD_SRWXURWX);



	CPU->cpu_cr3 = (u_int)(va_to_pfn((u_int)CPU->cpu_pgdirpttbl) <<
			    MMU_STD_PAGESHIFT) +
			    ((u_int)CPU->cpu_pgdirpttbl & PAGEOFFSET);
	kernel_only_cr3 = (u_int)(va_to_pfn((u_int)kernel_only_pttbl) <<
			    MMU_STD_PAGESHIFT) +
			    ((u_int)kernel_only_pttbl & PAGEOFFSET);
	dftss.t_cr3 = kernel_only_cr3;
	CPU->cpu_tss->t_cr3 = CPU->cpu_cr3;

	/*
	 * Enable Physical Address Extension (PAE)
	 */
	setup_121_andcall(enable_pae, CPU->cpu_cr3);
	/* destroy the one-to-one mapping that we setup before */
	pagedir[MMU_L1_INDEX(enable_pae_phyaddr)] = pagedirvalue;
	pagetbl[MMU_L2_INDEX(enable_pae_phyaddr)] = MMU_STD_INVALIDPTE;
	bcopy((caddr_t)CPU->cpu_pagedir + ((NPDPERAS - 1) * MMU_PAGESIZE),
	    kernel_only_pagedir, MMU_STD_PAGESIZE);

}

static pte_t *
allocptepage(u_int vaddr, pte_t *kernpte, pte32_t *bootpte)
{
	pte_t 			*pte;
	int 			i;
	struct	hwptepage 	*hwpp;
	struct	hment 		*hmep;
	extern	struct hat 	*kernel_hat;

	hwpp = hat_khwppalloc();
	hwpp->hwpp_hat = kernel_hat;
	hwpp->hwpp_numptes = 0;
	hwpp->hwpp_lockcnt = 1;
	hwpp->hwpp_base = addrtohwppbase(vaddr);

	pte = hwpp->hwpp_pte;

	/* Set kernel page directory entry */
	/* LINTED constant operand to op: "!" */
	*(uint64_t *)kernpte = PTEOF(va_to_pfn((u_int)pte), 1, MMU_STD_SRWX);

	/* Copy boot ptes, if provided, to just-allocated page table */
	if (bootpte) {
		hmep = kptetohme(pte);
		for (i = NPTEPERPT; --i >= 0; hmep++, pte++, bootpte++) {
			MOVPTE32_2_PTE64(bootpte, pte);
			if (pte_valid(pte)) {
				hwpp->hwpp_numptes++;
				hmep->hme_valid = 1;
			}
		}
	}
	return (hwpp->hwpp_pte);
}
/*
 * Allocate and copy in a page of kernel ptes.
 * The pagetables allocated are contiguous and
 * grow towards lower virtual address.
 */
static pte_t *
alloc_two_ptepages(u_int vaddr, pte_t *kernpte, register pte32_t *bootpte)
{
	pte_t *pte;

	/*
	 * setup the upper 2 MB of the 4MB chunk
	 */
	pte = allocptepage(vaddr + TWOMB_PAGESIZE, kernpte,
		(bootpte) ? bootpte + NPTEPERPT : NULL);

	/*
	 * Setup the lower 2Mb.
	 */
	pte = allocptepage(vaddr, (kernpte - 1), bootpte);

	return (pte);
}

static pte_t *
allocptelgpage(u_int vaddr, pte_t *kernpte, register pte32_t *bootpte)
{
	pte_t 			*pte;
	uint32_t		pfnum;
	struct	hwptepage 	*hwpp;
	int 			i;
	extern	struct hat 	*kernel_hat;
	struct	hment 		*hmep;

	hwpp = hat_khwppalloc();
	hwpp->hwpp_hat = kernel_hat;
	hwpp->hwpp_numptes = NPTEPERPT;
	hwpp->hwpp_lockcnt = 1;
	hwpp->hwpp_base = addrtohwppbase(vaddr+TWOMB_PAGESIZE);

	pte = hwpp->hwpp_pte;
	MOV4MBPTE32_2_2MBPTE64(bootpte, kernpte);
	pfnum = bootpte->PhysicalPageNumber;
	hmep = kptetohme(pte);
	for (i = 0; i < NPTEPERPT; i++, pfnum++, hmep++) {
	    *(uint64_t *)pte++ = PTEOF_C(pfnum, MMU_STD_SRWX);
	    hmep->hme_valid = 1;
	}

	return (hwpp->hwpp_pte);
}
static pte_t *
alloc_two_ptelgpages(u_int vaddr, pte_t *kernpte, register pte32_t *bootpte)
{
	pte_t 	*pte;
	pte32_t	pte32;

	/*
	 * Setup the upper 2Mb page
	 */
	pte32 = *bootpte;
	pte32.PhysicalPageNumber += NPTEPERPT;
	pte = allocptelgpage(vaddr + TWOMB_PAGESIZE, kernpte, &pte32);

	/*
	 * Setup the lower 2Mb page
	 */
	pte32 = *bootpte;
	pte = allocptelgpage(vaddr, (kernpte - 1), &pte32);

	return (pte);
}

#else		/* PTE36 */

static pte_t *allocptepage(u_int, pte_t *, pte_t *);
extern int	hat_use4kpte;

void
hat_kern_setup(void)
{
	struct	hat	*hat;
	pte_t *bootpte, *kernpte;
	int i, _4K_pfnum, j;
	pte_t *pte1, *_4kpte;
	u_int vaddr, pde;
	extern struct tss386 dftss;
	extern int kadb_is_running;

	/*
	 * Initialize the rest of the kas i86mmu hat.
	 */
	hat = kas.a_hat;
	hat->hat_cpusrunning = 1;


	/*
	 * Copy the boot page tables that map the kernel into
	 * the kernel's hat structures.
	 */
	bootpte = (pte_t *)(cr3() & MMU_STD_PAGEMASK) + MMU_NPTE_ONE - 1;
	kernpte = (pte_t *)(CPU->cpu_pagedir) + MMU_NPTE_ONE - 1;
	for (i = MMU_NPTE_ONE; --i >= 0; --bootpte, --kernpte) {
		vaddr = MMU_L1_VA(i);

#define	XXX_temp	/* XXX - DO WE STILL NEED THIS? */
#ifdef XXX_temp
		/* Leave map of lower 4 meg for bootops -- VERY temporary */
		if (vaddr >= (u_int)KERNELBASE || vaddr == 0) {
#else
		if (vaddr >= (u_int)KERNELBASE) {
#endif /* XXX_temp */
			if (pte_valid(bootpte)) {
				/*
				 * If this is a 4Mb page, then we need to
				 * retain the pagedirectory entry and fill in
				 * the pagetable with incremental pfnum
				 * i86mmu_ptefind() will return 4k pfnum
				 * even though they are really backed by a
				 * 4Mb page
				 */
				if (four_mb_page(bootpte)) {
					_4kpte = pte1 =
					allocptepage(vaddr, kernpte, NULL);
					if (hat_use4kpte &&
						vaddr < (u_int)SYSBASE) {
						/*LINTED*/
						pde = PTEOF(
						    va_to_pfn((u_int)pte1),
						    MMU_STD_SRWX, 1);
						*kernpte = *(pte_t *)&pde;
					} else *kernpte = *bootpte;
					_4K_pfnum =
					bootpte->PhysicalPageNumber;
					for (j = 0; j < NPTEPERPT; j++,
						_4K_pfnum++) {

						*(u_int *)_4kpte++ = (u_int)
						(MMU_L2_VA(_4K_pfnum)|
						PTE_PERMS(MMU_STD_SRWX)|PG_V);
					}
				} else {
				/*
				 * Copy maps for boot-allocated pages.
				 * If there's a ramdisk below SYSBASE,
				 * its ptes will be copied, even though
				 * it won't be in the ranges that
				 * i86mmu_ptefind() can locate (i.e.,
				 * the range between KERNELBASE and
				 * eecontig).
				 */
					pte1 =
					allocptepage(vaddr, kernpte, (pte_t *)
						(bootpte->PhysicalPageNumber *
						MMU_STD_PAGESIZE));
				}
			} else if (vaddr < (u_int)eecontig		||
				((vaddr > (u_int)(SYSBASE - PTSIZE)) &&
				    (vaddr < phys_syslimit)) 	||
				(vaddr == (u_int)PROMSTART)		||
				((vaddr == (u_int)DEBUGSTART) &&
				    kadb_is_running))
				/* Just allocate ptes for used virtual space */
				pte1 = allocptepage(vaddr, kernpte, NULL);
			if (vaddr == (u_int)KERNELBASE)
				KERNELmap = pte1;
			else if (vaddr == (u_int)Sysbase)
				Sysmap1 = pte1;
			else if ((vaddr == (u_int)PROMSTART)	||
				((vaddr == (u_int)DEBUGSTART) &&
				    kadb_is_running))
				Sysmap2 = pte1;
			continue;
		}

		/*
		 * Invalidate all level-1 entries below KERNELBASE and
		 * those which are otherwise unused.
		 */
		*(u_int *)kernpte = MMU_STD_INVALIDPTE;
	}

	/*
	 * Load CR3 to start mapping with the kernel page directory.
	 * Loading CR3 flushes the entire TLB.
	 */
	CPU->cpu_cr3 = (u_int)(va_to_pfn((u_int)CPU->cpu_pagedir) <<
							MMU_STD_PAGESHIFT);
	setcr3(CPU->cpu_cr3);
	dftss.t_cr3 = CPU->cpu_cr3;
	CPU->cpu_tss->t_cr3 = CPU->cpu_cr3;


	(void) hat_setup(kas.a_hat, HAT_ALLOC);
	(void) bcopy((caddr_t)CPU->cpu_pagedir,
		kernel_only_pagedir, MMU_STD_PAGESIZE);
}

/*
 * Allocate and copy in a page of kernel ptes.
 * The pagetables allocated are contiguous and
 * grow towards lower virtual address.
 */
static pte_t *
allocptepage(u_int vaddr, pte_t *kernpte, pte_t *bootpte)
{
	pte_t *ptep, *ptep1;
	struct	hwptepage *hwpp;
	int i;
	extern	struct hat *kernel_hat;

	hwpp = hat_khwppalloc();
	hwpp->hwpp_hat = kernel_hat;
	hwpp->hwpp_numptes = 0;
	hwpp->hwpp_lockcnt = 1;
	hwpp->hwpp_base = addrtohwppbase(vaddr);

	ptep1 = ptep = hwpp->hwpp_pte;

	/* Set kernel page directory entry */
	/* LINTED constant operand to op: "!" */
	*(u_int *)kernpte = PTEOF(va_to_pfn((u_int)ptep), 1, MMU_STD_SRWX);

	/* Copy boot ptes, if provided, to just-allocated page table */
	if (bootpte) {
		struct hment *hmep;
		hmep = kptetohme(ptep);
		for (i = NPTEPERPT; --i >= 0; hmep++) {
			if (pte_valid(ptep)) {
				hwpp->hwpp_numptes++;
				hmep->hme_valid = 1;
			}
			*ptep++ = *bootpte++;
		}
	}
	return (ptep1);
}

#endif


/*
 * Kernel lomem memory allocation/freeing
 */

static struct lomemlist {
	struct lomemlist	*lomem_next;	/* next in a list */
	u_long		lomem_paddr;	/* base kernel virtual */
	u_long		lomem_size;	/* size of space (bytes) */
} lomemusedlist, lomemfreelist;

static kmutex_t	lomem_lock;		/* mutex protecting lomemlist data */
static int lomemused, lomemmaxused;
static caddr_t lomem_startva;
static caddr_t lomem_endva;
static u_long  lomem_startpa;
static kcondvar_t lomem_cv;
static u_long  lomem_wanted;

extern long lomempages;

#ifdef DEBUG
static int lomem_debug = 0;
#endif /* DEBUG */

void
lomem_init(void)
{
	pgcnt_t nfound;
	pfn_t pfn;
	struct lomemlist *dlp;
	int biggest_group = 0;
	caddr_t addr;
	extern struct vnode kvp;
	int i;

	/*
	 * Try to find lomempages pages of contiguous memory below 16meg.
	 * If we can't find lomempages, find the biggest group.
	 * With 64K alignment being needed for devices that have
	 * only 16 bit address counters (next byte fixed), make sure
	 * that the first physical page is 64K aligned; if lomempages
	 * is >= 16, we have at least one 64K segment that doesn't
	 * span the address counter's range.
	 */

again:
	if (lomempages <= 0)
		return;

	for (nfound = 0, pfn = 0; pfn < btop(16*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) == NULL) {
			/* Encountered an unallocated page. Back out.  */
			if (nfound > biggest_group)
				biggest_group = nfound;
			for (; nfound; --nfound)
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			/*
			 * nfound is back to zero to continue search.
			 * Bump pfn so next pfn is on a 64Kb boundary.
			 */
			pfn |= (btop(64*1024) - 1);
		} else {
			if (++nfound >= lomempages)
				break;
		}
	}

	if (nfound < lomempages) {

		/*
		 * Ran beyond 16 meg.  pfn is last in group + 1.
		 * This is *highly* unlikely, as this search happens
		 *   during startup, so there should be plenty of
		 *   pages below 16mb.
		 */

		if (nfound > biggest_group)
			biggest_group = nfound;

		cmn_err(CE_WARN, "lomem_init: obtained only %d of %d pages.\n",
				biggest_group, (int)lomempages);

		if (nfound != biggest_group) {
			/*
			 * The last group isn't the biggest.
			 * Free it and try again for biggest_group.
			 */
			for (; nfound; --nfound) {
				page_free(page_numtopp_nolock(pfn-nfound), 1);
			}
			lomempages = biggest_group;
			goto again;
		}

		--pfn;	/* Adjust to be pfn of last in group */
	}


	/* pfn is last page frame number; compute  first */
	pfn -= (nfound - 1);
	lomem_startva = i86devmap(pfn, nfound, PROT_READ|PROT_WRITE);
	lomem_endva = lomem_startva + ptob(lomempages);

	/* Set up first free block */
	lomemfreelist.lomem_next = dlp =
		(struct lomemlist *)kmem_alloc(sizeof (struct lomemlist), 0);
	dlp->lomem_next = NULL;
	dlp->lomem_paddr = lomem_startpa = ptob(pfn);
	dlp->lomem_size  = ptob(nfound);

	/*
	 * crash dump, libkvm support:
	 *
	 * Add the allocated pages to the page-hash list so that
	 * libkvm can find these pages when looking at the crash
	 * dumps.
	 */
	for (addr = lomem_startva, i = lomempages; i; --i) {
		page_t *pp;

		pp = page_numtopp_nolock(pfn);
		(void) page_hashin(pp, &kvp, (u_offset_t)(u_int)addr, NULL);
		addr += PAGESIZE;
		pfn++;
	}

#ifdef DEBUG
	if (lomem_debug)
		printf("lomem_init: %d pages, phys=%x virt=%x\n",
		    (int)lomempages, (int)dlp->lomem_paddr,
		    (int)lomem_startva);
#endif /* DEBUG */

	mutex_init(&lomem_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&lomem_cv, NULL, CV_DEFAULT, NULL);
}

/*
 * Allocate contiguous, memory below 16meg.
 * Only used for ddi_iopb_alloc (and ddi_memm_alloc) - os/ddi_impl.c.
 */
caddr_t
lomem_alloc(u_int nbytes, ddi_dma_attr_t *attr, int align, int cansleep)
{
	struct lomemlist *dlp;	/* lomem list ptr scans free list */
	struct lomemlist *dlpu; /* New entry for used list if needed */
	struct lomemlist *dlpf;	/* New entry for free list if needed */
	struct lomemlist *pred;	/* Predecessor of dlp */
	struct lomemlist *bestpred = NULL;
	u_long left, right;
	u_long leftrounded, rightrounded;

#ifdef lint
	align = align;
#endif

	/* make sure lomem_init() has been called */
	ASSERT(lomem_endva != 0);

	if ((dlpu = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL)
			return (NULL);

	/* In case we need a second lomem list element ... */
	if ((dlpf = (struct lomemlist *)kmem_alloc(sizeof (struct lomemlist),
		cansleep ? 0 : KM_NOSLEEP)) == NULL) {
			kmem_free(dlpu, sizeof (struct lomemlist));
			return (NULL);
	}

	/* Force 16-byte multiples and alignment; great simplification. */
	nbytes = (nbytes + 15) & (~15);

	mutex_enter(&lomem_lock);

again:
	for (pred = &lomemfreelist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp) {
		/*
		 * The criteria for choosing lomem space are:
		 *   1. Leave largest possible free block after allocation.
		 *	From this follows:
		 *		a. Use space in smallest usable block.
		 *		b. Avoid fragments (i.e., take from end).
		 *	Note: This may mean that we fragment a smaller
		 *	block when we could have allocated from the end
		 *	of a larger one, but c'est la vie.
		 *
		 *   2. Prefer taking from right (high) end.  We start
		 *	with 64Kb aligned space, so prefer not to break
		 *	up the first chunk until we have to.  In any event,
		 *	reduce fragmentation by being consistent.
		 */
		if (dlp->lomem_size < nbytes ||
			(bestpred &&
			dlp->lomem_size > bestpred->lomem_next->lomem_size))
				continue;

		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + attr->dma_attr_seg - 1) &
						~attr->dma_attr_seg);
		rightrounded = right & ~attr->dma_attr_seg;

		/*
		 * See if this block will work, either from left, from
		 * right, or after rounding up left to be on an "address
		 * increment" (dlim_adreg_max) boundary.
		 */
		if (lomem_check_limits(attr, right - nbytes, right - 1) ||
		    lomem_check_limits(attr, left, left + nbytes - 1) ||
		    (leftrounded + nbytes <= right &&
			lomem_check_limits(attr, leftrounded,
						leftrounded+nbytes-1))) {
			bestpred = pred;
		}
	}

	if (bestpred == NULL) {
		if (cansleep) {
			if (lomem_wanted == 0 || nbytes < lomem_wanted)
				lomem_wanted = nbytes;
			cv_wait(&lomem_cv, &lomem_lock);
			goto again;
		}
		mutex_exit(&lomem_lock);
		kmem_free(dlpu, sizeof (struct lomemlist));
		kmem_free(dlpf, sizeof (struct lomemlist));
		return (NULL);
	}

	/* bestpred is predecessor of block we're going to take from */
	dlp = bestpred->lomem_next;

	if (dlp->lomem_size == nbytes) {
		/* Perfect fit.  Just use whole block. */
		ASSERT(lomem_check_limits(attr,  dlp->lomem_paddr,
				dlp->lomem_paddr + dlp->lomem_size - 1));
		bestpred->lomem_next = dlp->lomem_next;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	} else {
		left = dlp->lomem_paddr;
		right = dlp->lomem_paddr + dlp->lomem_size;
		leftrounded = ((left + attr->dma_attr_seg - 1) &
						~attr->dma_attr_seg);
		rightrounded = right & ~attr->dma_attr_seg;

		if (lomem_check_limits(attr, right - nbytes, right - 1)) {
			/* Take from right end */
			dlpu->lomem_paddr = right - nbytes;
			dlp->lomem_size -= nbytes;
		} else if (lomem_check_limits(attr, left, left+nbytes-1)) {
			/* Take from left end */
			dlpu->lomem_paddr = left;
			dlp->lomem_paddr += nbytes;
			dlp->lomem_size -= nbytes;
		} else if (rightrounded - nbytes >= left &&
			lomem_check_limits(attr, rightrounded - nbytes,
							rightrounded - 1)) {
			/* Take from right after rounding down */
			dlpu->lomem_paddr = rightrounded - nbytes;
			dlpf->lomem_paddr = rightrounded;
			dlpf->lomem_size  = right - rightrounded;
			dlp->lomem_size -= (nbytes + dlpf->lomem_size);
			dlpf->lomem_next = dlp->lomem_next;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		} else {
			ASSERT(leftrounded + nbytes <= right &&
				lomem_check_limits(attr, leftrounded,
						leftrounded + nbytes - 1));
			/* Take from left after rounding up */
			dlpu->lomem_paddr = leftrounded;
			dlpf->lomem_paddr = leftrounded + nbytes;
			dlpf->lomem_size  = right - dlpf->lomem_paddr;
			dlpf->lomem_next  = dlp->lomem_next;
			dlp->lomem_size = leftrounded - dlp->lomem_paddr;
			dlp->lomem_next  = dlpf;
			dlpf = NULL;	/* Don't free it */
		}
		dlp = dlpu;
		dlpu = NULL;	/* Don't free it */
		dlp->lomem_size = nbytes;
		dlp->lomem_next = lomemusedlist.lomem_next;
		lomemusedlist.lomem_next = dlp;
	}

	if ((lomemused += nbytes) > lomemmaxused)
		lomemmaxused = lomemused;

	mutex_exit(&lomem_lock);

	if (dlpu) kmem_free(dlpu, sizeof (struct lomemlist));
	if (dlpf) kmem_free(dlpf, sizeof (struct lomemlist));

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_alloc: alloc paddr 0x%x size %d\n",
		    (int)dlp->lomem_paddr, (int)dlp->lomem_size);
	}
#endif /* DEBUG */
	return (lomem_startva + (dlp->lomem_paddr - lomem_startpa));
}

static int
lomem_check_limits(ddi_dma_attr_t *attr, u_int lo, u_int hi)
{
	return (lo >= attr->dma_attr_addr_lo && hi <= attr->dma_attr_addr_hi &&
		((hi & ~(attr->dma_attr_seg)) == (lo & ~(attr->dma_attr_seg))));
}

void
lomem_free(caddr_t kaddr)
{
	struct lomemlist *dlp, *pred, *dlpf;
	u_long paddr;

	/* make sure lomem_init() has been called */
	ASSERT(lomem_endva != 0);


	/* Convert kaddr from virtual to physical */
	paddr = (kaddr - lomem_startva) + lomem_startpa;

	mutex_enter(&lomem_lock);

	/* Find the allocated block in the used list */
	for (pred = &lomemusedlist; (dlp = pred->lomem_next) != NULL;
	    pred = dlp)
		if (dlp->lomem_paddr == paddr)
			break;

	if ((dlp == NULL) || (dlp->lomem_paddr != paddr)) {
		cmn_err(CE_WARN, "lomem_free: bad addr=0x%x paddr=0x%x\n",
			(int)kaddr, (int)paddr);
		mutex_exit(&lomem_lock);
		return;
	}

	lomemused -= dlp->lomem_size;

	/* Remove from used list */
	pred->lomem_next = dlp->lomem_next;

	/* Insert/merge into free list */
	for (pred = &lomemfreelist; (dlpf = pred->lomem_next) != NULL;
	    pred = dlpf) {
		if (paddr <= dlpf->lomem_paddr)
			break;
	}

	/* Insert after pred; dlpf may be NULL */
	if (pred->lomem_paddr + pred->lomem_size == dlp->lomem_paddr) {
		/* Merge into pred */
		pred->lomem_size += dlp->lomem_size;
		kmem_free(dlp, sizeof (struct lomemlist));
	} else {
		/* Insert after pred */
		dlp->lomem_next = dlpf;
		pred->lomem_next = dlp;
		pred = dlp;
	}

	if (dlpf &&
		pred->lomem_paddr + pred->lomem_size == dlpf->lomem_paddr) {
		pred->lomem_next = dlpf->lomem_next;
		pred->lomem_size += dlpf->lomem_size;
		kmem_free(dlpf, sizeof (struct lomemlist));
	}

	if (pred->lomem_size >= lomem_wanted) {
		lomem_wanted = 0;
		cv_broadcast(&lomem_cv);
	}

	mutex_exit(&lomem_lock);

#ifdef DEBUG
	if (lomem_debug) {
		printf("lomem_free: freeing addr 0x%x -> addr=0x%x, size=%d\n",
		    (int)paddr, (int)pred->lomem_paddr, (int)pred->lomem_size);
	}
#endif /* DEBUG */
}

caddr_t
i86devmap(pfn_t pf, pgcnt_t npf, u_int prot)
{
	caddr_t addr;
	page_t *pp;
	caddr_t addr1;

	addr1 = addr = (caddr_t)kmxtob(rmalloc(kernelmap, npf));

	for (; npf != 0; addr += MMU_PAGESIZE, ++pf, npf--) {
		if ((pp = page_numtopp_nolock(pf)) == NULL)
			hat_devload(kas.a_hat,  addr,
			    MMU_PAGESIZE, pf, prot|HAT_NOSYNC, HAT_LOAD_LOCK);
		else
			hat_memload(kas.a_hat, addr, pp,
			    prot|HAT_NOSYNC, HAT_LOAD_LOCK);
	}

	return (addr1);
}

/*
 * This routine is like page_numtopp, but accepts only free pages, which
 * it allocates (unfrees) and returns with the exclusive lock held.
 * It is used by machdep.c/dma_init() to find contiguous free pages.
 */
page_t *
page_numtopp_alloc(pfn_t pfnum)
{
	page_t *pp;

	pp = page_numtopp_nolock(pfnum);
	if (pp == NULL)
		return (NULL);

	if (!page_trylock(pp, SE_EXCL))
		return (NULL);

	if (!PP_ISFREE(pp)) {
		page_unlock(pp);
		return (NULL);
	}

	/* If associated with a vnode, destroy mappings */

	if (pp->p_vnode) {

		page_destroy_free(pp);

		if (!page_lock(pp, SE_EXCL, (kmutex_t *)NULL, P_NO_RECLAIM))
			return (NULL);
	}

	if (!PP_ISFREE(pp) || !page_reclaim(pp, (kmutex_t *)NULL)) {
		page_unlock(pp);
		return (NULL);
	}

	return (pp);
}

/* check if the buffer is allocated from lomem pages */
int
islomembuf(void *buf)
{
	if (((caddr_t)buf < lomem_endva) && ((caddr_t)buf >= lomem_startva))
		return (1);

	return (0);
}
