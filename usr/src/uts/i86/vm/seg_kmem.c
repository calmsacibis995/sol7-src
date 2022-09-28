/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986, 1987, 1988, 1989, 1990, 1992, 1995  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1997, 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)seg_kmem.c	1.51	98/01/09 SMI"

/*
 * VM - kernel segment routines
 */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vm.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/mman.h>
#include <sys/vtrace.h>
#include <sys/map.h>
#include <sys/tuneable.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dumphdr.h>

#include <sys/mmu.h>
#include <sys/pte.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/anon.h>
#include <vm/rm.h>
#include <vm/page.h>
#include <vm/faultcode.h>
#include <vm/hat.h>

#include <sys/promif.h>
#include <sys/bootconf.h>

#include <sys/ddidmareq.h>

/*
 * Private seg op routines.
 */
static faultcode_t segkmem_fault(struct hat *hat, struct seg *seg,
	caddr_t addr, size_t len, enum fault_type type, enum seg_rw rw);
static faultcode_t segkmem_faulta(struct seg *seg, caddr_t addr);
static int segkmem_setprot(struct seg *seg, caddr_t addr, size_t len,
	u_int prot);
static int segkmem_checkprot(struct seg *seg, caddr_t addr,
	size_t len, u_int prot);
static int segkmem_getprot(struct seg *seg, caddr_t addr,
	size_t len, u_int *protv);
static u_offset_t segkmem_getoffset(struct seg *seg, caddr_t addr);
static int segkmem_gettype(struct seg *seg, caddr_t addr);
static int segkmem_getvp(struct seg *seg, caddr_t addr, struct vnode **vpp);
static void segkmem_dump(struct seg *seg);
static int segkmem_pagelock(struct seg *seg, caddr_t addr, size_t len,
	struct page ***ppp, enum lock_type type, enum seg_rw rw);
static int segkmem_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp);
static int segkmem_badop();

struct as kas;

extern struct bootops *bootops;

/*
 * Machine specific public segments.
 */
struct seg ktextseg;
struct seg kvseg;
struct seg kdvmaseg;

int segkmem_ready = 0;

/*
 * All kmem alloc'ed kernel pages are associated
 * with a special kernel vnode.
 */
struct vnode kvp;

struct	seg_ops segkmem_ops = {
	segkmem_badop,			/* dup */
	segkmem_badop,			/* unmap */
	(void(*)())segkmem_badop,	/* free */
	segkmem_fault,
	segkmem_faulta,
	segkmem_setprot,
	segkmem_checkprot,
	segkmem_badop,			/* kluster */
	(size_t (*)())segkmem_badop,	/* swapout */
	segkmem_badop,			/* sync */
	(size_t (*)())segkmem_badop,	/* incore */
	segkmem_badop,			/* lockop */
	segkmem_getprot,
	segkmem_getoffset,
	segkmem_gettype,
	segkmem_getvp,
	segkmem_badop,			/* advise */
	segkmem_dump,
	segkmem_pagelock,
	segkmem_getmemid,
};

/*
 * The srmmu segkmem driver optionally uses an array of pte's (argsp) to back
 * up the mappings.  It was used only for Sysmap.  Since our implementation
 * uses direct mapping of kernel space, we don't need it.
 */
int
segkmem_create(struct seg *seg, void *argsp)
{
	ASSERT(seg->s_as == &kas && RW_WRITE_HELD(&seg->s_as->a_lock));
	ASSERT(argsp == 0);
	seg->s_ops = &segkmem_ops;
	seg->s_data = argsp;	/* actually a struct pte array */
	kas.a_size += seg->s_size;
	return (0);
}

/*ARGSUSED*/
static faultcode_t
segkmem_fault(struct hat *hat, struct seg *seg, caddr_t addr,
    size_t len, enum fault_type type, enum seg_rw rw)
{
	faultcode_t retval;

	ASSERT(seg->s_as && RW_READ_HELD(&seg->s_as->a_lock));

	/*
	 * For now the only `faults' supported by this driver are
	 * F_SOFTLOCK/F_SOFTUNLOCK for the S_READ/S_WRITE case (caused
	 * during physio by RFS servers).
	 *
	 * These types of faults are used to denote "lock down already
	 * loaded translations".
	 */
	switch (type) {
	case F_SOFTLOCK:
		if (rw == S_READ || rw == S_WRITE || rw == S_OTHER)
			retval = 0;
		else
			retval = FC_NOSUPPORT;
		break;
	case F_SOFTUNLOCK:
		if (rw == S_READ || rw == S_WRITE)
			retval = 0;
		else
			retval = FC_NOSUPPORT;
		break;
	default:
		retval = FC_NOSUPPORT;
		break;
	}
	return (retval);
}

/*ARGSUSED*/
static faultcode_t
segkmem_faulta(struct seg *seg, caddr_t addr)
{
	return (FC_NOSUPPORT);
}

/*
 * XXX - Routines which obtain information directly from the
 * MMU should acquire the hat layer lock.
 */

static int
segkmem_setprot(struct seg *seg, caddr_t addr, size_t len, u_int prot)
{
	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	if (addr < seg->s_base || (addr + len) > (seg->s_base + seg->s_size))
		cmn_err(CE_PANIC, "segkmem_setprot -- out of segment");

	hat_chgattr(seg->s_as->a_hat, addr, len, prot);

	return (0);
}


static int
segkmem_checkprot(struct seg *seg, register caddr_t addr,
    size_t len, u_int prot)
{
	u_int attr;
	caddr_t eaddr;

	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(AS_LOCK_HELD(seg->s_as, &seg->s_as->a_lock));

	/* hat_getattr returns 0 on success */

	for (eaddr = addr + len; addr < eaddr; addr += MMU_PAGESIZE) {
		if (hat_getattr(seg->s_as->a_hat, addr, &attr) ||
		    ((attr & prot) != prot)) {
			return (EACCES);
		}
	}
	return (0);
}

static int
segkmem_getprot(struct seg *seg, caddr_t addr, size_t len, u_int *protv)
{
	u_int pgno = (mmu_btopr((u_int)(addr+len)) - mmu_btop((u_int)addr));
	u_int	attr;
	register int i;

	ASSERT(seg->s_as == &kas);


	for (i = 0; i < pgno; i++, addr += MMU_PAGESIZE) {
		if (hat_getattr(seg->s_as->a_hat, addr, &attr) == 0)
			protv[i] = attr;
		else
			protv[i] = PROT_NONE;
	}
	return (0);
}

/*ARGSUSED*/
static u_offset_t
segkmem_getoffset(struct seg *seg, caddr_t addr)
{
	return ((u_offset_t)0);
}

/*ARGSUSED*/
static int
segkmem_gettype(struct seg *seg, caddr_t addr)
{
	return (MAP_SHARED);
}

/*ARGSUSED*/
static int
segkmem_getvp(register struct seg *seg, caddr_t addr, struct vnode **vpp)
{
	*vpp = NULL;
	return (-1);
}

static int
segkmem_badop()
{

	cmn_err(CE_PANIC, "segkmem_badop");
	return (0);
	/*NOTREACHED*/
}

/*
 * Special public segkmem routines.
 */

/*
 * Allocate physical pages for the given kernel virtual address.
 */
int
segkmem_alloc(struct seg *seg, caddr_t addr, size_t len, int canwait)
{
	page_t *pp;
	register int val = 0;		/* assume failure */
	struct as *as = seg->s_as;

	ASSERT(as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);

	pp = page_create_va(&kvp, (u_offset_t)addr, len,
			PG_EXCL | ((canwait) ? PG_WAIT : 0), &kas, addr);
	if (pp != (page_t *)NULL) {
		page_t *ppcur;

		while (pp != (page_t *)NULL) {
			ppcur = pp;
			page_sub(&pp, ppcur);
			ASSERT(PAGE_LOCKED(ppcur) &&
				page_iolock_assert(ppcur));
			page_io_unlock(ppcur);
			/*
			page_downgrade(ppcur);
			*/

			ASSERT(ppcur->p_offset <= UINT_MAX);
			hat_memload(as->a_hat,
			    (caddr_t)ppcur->p_offset, ppcur,
			    PROT_ALL & ~PROT_USER, HAT_LOAD_LOCK);
		}
		val = 1;		/* success */
	}
	return (val);
}

/*ARGSUSED*/
void
segkmem_free(struct seg *seg, caddr_t addr, size_t len)
{
	page_t *pp;
	struct as *as = seg->s_as;

	ASSERT(as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((uintptr_t)addr & PAGEOFFSET) == 0);

	for (; (int)len > 0; len -= PAGESIZE, addr += PAGESIZE) {
		/*
		 * Use page_find() instead of page_lookup() to
		 * find the page since we know that it is hashed
		 * and has an "exclusive" lock.
		 */
		pp = page_find(&kvp, (u_offset_t)addr);
		if (pp == NULL)
			cmn_err(CE_PANIC, "segkmem_free");

#ifdef	sparc
		if (! page_tryupgrade(pp)) {
			/*
			* Other thread has it locked shared too. Most
			* likely is some /dev/mem reader threads.
			*/
			page_unlock(pp);

			/* We should be the only one to free this page! */
			pp = page_lookup(&kvp, (off_t)addr, SE_EXCL);
			if (pp == NULL) {
				cmn_err(CE_PANIC, "segkmem_free: page freed");
			}
		}
#endif	/* sparc */

		hat_unload(as->a_hat, addr, PAGESIZE,
		    HAT_UNLOAD_UNLOCK);
		/*
		 * Destroy identity of the page and put it
		 * back on the free list.
		 */
		/*LINTED: constant in conditional context */
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}
}


/*
 * segkmem_mapin() and segkmem_mapout() are for manipulating kernel
 * addresses only. Since some users of segkmem_mapin() forget to unmap,
 * this is done implicitly.
 * NOTE: addr and len must always be multiples of the mmu page size.
 * Also, this routine cannot be used to set invalid translations.
 */
void
segkmem_mapin(struct seg *seg, caddr_t addr, size_t len, u_int vprot,
    u_int pfn, int flags)
{

	page_t *pp;

#ifdef	lint

	seg = seg;

#endif

	ASSERT(seg->s_as == &kas);
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);
	ASSERT(((uintptr_t)addr & MMU_PAGEOFFSET) == 0);

	/*
	 * Always lock the mapin'd translations.
	 */
	flags |= HAT_LOAD_LOCK;

	for (; len != 0; addr += MMU_PAGESIZE, len -= MMU_PAGESIZE, pfn++) {
		if (hat_getpfnum(kas.a_hat, addr) != PFN_INVALID)
			hat_unload(kas.a_hat, addr,
			    MMU_PAGESIZE, HAT_UNLOAD_UNLOCK);

		/*
		 * check for the need to attempt to acquire the
		 * "shared" lock on the page
		 */
		if ((flags & HAT_LOAD_NOCONSIST) ||
		    ((pp = page_numtopp_nowait(pfn, SE_SHARED)) == NULL)) {
			hat_devload(kas.a_hat,  addr,
			    MMU_PAGESIZE, pfn, vprot, flags);

		} else {
			hat_memload(kas.a_hat, addr, pp, vprot,
			    flags);
			page_unlock(pp);
		}
	}
}

/*
 * Release mapping for the kernel.  The pages specified are only freed
 * if they are valid. This allows callers to clear out virtual space
 * without knowing if it's mapped or not.
 * NOTE: addr and len must always be multiples of the page size.
 */
void
segkmem_mapout(struct seg *seg, caddr_t addr, size_t len)
{
	ASSERT(len <= seg->s_size);
	ASSERT(addr <= addr + len);
	ASSERT(addr >= seg->s_base && addr + len <= seg->s_base + seg->s_size);
	ASSERT(seg->s_ops == &segkmem_ops);

	ASSERT(((uintptr_t)addr & MMU_PAGEOFFSET) == 0);
	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (seg->s_as != &kas)
		cmn_err(CE_PANIC, "segkmem_mapout: bad as");

	hat_unload(kas.a_hat, addr, len, HAT_UNLOAD_UNLOCK);
}

/*
 * Dump the pages belonging to this segkmem segment.
 */
static void
segkmem_dump(struct seg *seg)
{
	caddr_t addr, eaddr;
	pfn_t pfn;

	addr = seg->s_base;
	eaddr = addr + seg->s_size;
	for (; addr < eaddr; addr += PAGESIZE) {
		if (((pfn = hat_getpfnum(seg->s_as->a_hat,
		    addr)) != PFN_INVALID) &&
		    (pfn <= physmax) && pf_is_memory(pfn))
			dump_addpage(pfn);
	}
}

/*ARGSUSED*/
static int
segkmem_pagelock(struct seg *seg, caddr_t addr, size_t len,
    struct page ***ppp, enum lock_type type, enum seg_rw rw)
{
	return (ENOTSUP);
}

/*
 * Allocate "npages" (MMU) pages worth of system virtual address space, and
 * wired-down page frames to back them.
 * If "flag" is KM_NOSLEEP, block until address space and page frames are
 * available.
 */
void *
kmem_getpages(size_t npages, u_int flag)
{
	register u_long a;
	caddr_t base;
	extern u_int va_to_pfn(u_int);

	/*
	 * Make sure that the number of pages requested isn't bigger
	 * than segkmem itself.
	 */
	if (segkmem_ready && (npages > (int)mmu_btop(kvseg.s_size))) {
		if (flag & KM_NOSLEEP)
			return (NULL);
		cmn_err(CE_PANIC, "kmem_getpages: request too big");
	}

	/*
	 * Allocate kernel virtual address space.
	 */
	if (flag & KM_NOSLEEP) {
		if ((a = rmalloc(kernelmap, (long)npages)) == 0)
			return (NULL);
	} else {
		a = rmalloc_wait(kernelmap, (long)npages);
	}

	base = kmxtob(a);

	/*
	 * Allocate physical pages to back the address allocated.
	 */
	if (segkmem_ready) {
		if (segkmem_alloc(&kvseg, base,
		    (size_t)mmu_ptob(npages),
		    !(flag & KM_NOSLEEP)) == 0) {
			rmfree(kernelmap, (long)npages, a);
			return (NULL);
		}
	} else {
		u_int pfn;
		struct page *pp;
		caddr_t addr;
		u_int n;

		/*
		 * Below we simulate segkmem before the segkmem
		 * is completely initilaized by using the boot/prom
		 * memory allocator and the parts of the vm system
		 * that are operational early on.  The required parts
		 * are: initialization of locking, kernelmap,
		 * the memseg list, and the page structs.
		 * Notable parts that are not initialized:
		 * any kernel segments and the hat layer.
		 */
		addr = BOP_ALLOC(bootops, base, (u_int)mmu_ptob(npages),
		    BO_NO_ALIGN);
		if (addr != base)
			cmn_err(CE_PANIC, "boot alloc failed");

		for (n = npages; n; addr += MMU_PAGESIZE, n--) {
			pfn = va_to_pfn((uintptr_t)addr);
			ASSERT(pfn != (unsigned)(-1));
			pp = page_numtopp(pfn, SE_EXCL);
			ASSERT(pp != NULL);
			(void) page_hashin(pp, &kvp, (u_offset_t)addr,
			    (kmutex_t *)NULL);
			/*
			page_downgrade(pp);
			*/
		}
	}
	return ((void *)base);
}

void *
kmem_getpages_io(size_t npages, int flag, int physcontig, ddi_dma_attr_t *mattr)
{
	u_long a;
	caddr_t base;
	page_t *pp;
	page_t	*page_create_io(struct vnode *, u_offset_t, u_int, u_int,
	    struct as *, caddr_t, ddi_dma_attr_t *);

	if (!segkmem_ready)
		return (NULL);

	/*
	 * For now, we can't allocate physically contiguous memory for
	 * more than one page.
	 */
	if (physcontig && (npages > 1))
		return (NULL);

	/*
	 * Make sure that the number of pages requested isn't bigger
	 * than segkmem itself.
	 */
	if (npages > (int)mmu_btop(kvseg.s_size))
		cmn_err(CE_PANIC, "kmem_getpages_io: request too big");

	/*
	 * Allocate kernel virtual address space.
	 */
	if (flag & KM_NOSLEEP) {
		if ((a = rmalloc(kernelmap, (long)npages)) == 0)
			return (NULL);
	} else {
		a = rmalloc_wait(kernelmap, (long)npages);
	}

	base = kmxtob(a);

	/*
	 * Allocate physical pages to back the address allocated.
	 */
	pp = page_create_io(&kvp, (u_offset_t)base, (u_int)mmu_ptob(npages),
	    PG_EXCL | (!(flag & KM_NOSLEEP) ? PG_WAIT : 0), &kas,
	    base, mattr);

	if (pp != (page_t *)NULL) {
		page_t *ppcur;

		while (pp != (page_t *)NULL) {
			ppcur = pp;
			page_sub(&pp, ppcur);
			ASSERT(PAGE_LOCKED(ppcur) && page_iolock_assert(ppcur));
			page_io_unlock(ppcur);
			/*
			page_downgrade(ppcur);
			*/

			ASSERT(ppcur->p_offset <= UINT_MAX);
			hat_memload(kas.a_hat,
			    (caddr_t)ppcur->p_offset, ppcur,
			    PROT_ALL & ~PROT_USER, HAT_LOAD_LOCK);
		}

		return ((void *)base);
	}

	/* failed to allocate pages */
	rmfree(kernelmap, (long)npages, a);
	return (NULL);
}


/*
 * This is the memlist we'll keep around until segkmem is ready
 * and then we'll come through and free it.
 */
static struct memlist *kmem_garbage_list;

/*
 * Free "npages" (MMU) pages gotten with "kmem_getpages".
 */
void
kmem_freepages(void *addr, pgcnt_t npages)
{
	if (!segkmem_ready) {
		struct memlist *this;
		/*
		 * If someone attempts to free memory here we must delay
		 * the action until kvseg is ready.  So lets just put
		 * the request on a list and delete it at a later time.
		 */
		this = kmem_zalloc(sizeof (struct memlist), KM_NOSLEEP);
		if (!this) {
			cmn_err(CE_PANIC,
			    "can't allocate kmem gc list element");
		}
		this->address = (uintptr_t)addr;
		this->size = (u_int)ptob(npages);
		this->next = kmem_garbage_list;
		kmem_garbage_list = this;
	} else {

		/*
		 * Free the physical memory behind the pages.
		 */
		segkmem_free(&kvseg, addr, (size_t)mmu_ptob(npages));

		/*
		 * Free the virtual addresses behind which they resided.
		 */
		rmfree(kernelmap, (size_t)npages, (u_long)btokmx(addr));
	}
}

/*
 * Collect and free all the unfreed garbage on the delayed free list
 * Called at the end of startup().
 */
void
kmem_gc(void)
{
	struct memlist	*this, *next;

	ASSERT(segkmem_ready);

	for (this = kmem_garbage_list; this; this = next) {
		kmem_freepages((void *)this->address,
		    (size_t)mmu_btop(this->size));
		next = this->next;
		kmem_free(this, sizeof (struct memlist));
	}

	kmem_garbage_list = NULL;
}

/*
 * Return the total amount of virtual space available for kmem_alloc
 */
size_t
kmem_maxvirt()
{
	struct map *bp;
	size_t amount = 0;

	mutex_enter(&maplock(kernelmap));
	for (bp = mapstart(kernelmap); bp->m_size; bp++)
		amount += bp->m_size;
	mutex_exit(&maplock(kernelmap));
	return (amount);
}

/*ARGSUSED*/
static int
segkmem_getmemid(struct seg *seg, caddr_t addr, memid_t *memidp)
{
	return (ENODEV);
}
