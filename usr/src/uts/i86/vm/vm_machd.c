/*
 * Copyright (c) 1987, 1992-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)vm_machdep.c 1.46     98/02/04 SMI"

/*
 * UNIX machine dependent virtual memory support.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/map.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/vm.h>
#include <sys/mman.h>
#include <sys/vnode.h>
#include <sys/cred.h>
#include <sys/exec.h>
#include <sys/exechdr.h>
#include <sys/debug.h>

#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/seg_kp.h>
#include <vm/seg_vn.h>
#include <vm/page.h>
#include <vm/seg_kmem.h>
#include <vm/mach_page.h>

#include <sys/cpu.h>
#include <sys/vm_machparam.h>
#include <sys/memlist.h>
#include <sys/bootconf.h> /* XXX the memlist stuff belongs in memlist_plat.h */

#include <vm/hat_i86.h>
#include <sys/x86_archext.h>
#include <sys/cmn_err.h>
#include <sys/machsystm.h>

#include <sys/vtrace.h>
#include <sys/ddidmareq.h>
#include <sys/promif.h>

int	largepagesupport = 0;
#ifdef	PTE36

#define	MMU_HIGHEST_PFN	0x2fffff
#define	MMU_NAME	"mmu36"
static uint64_t	boot_pte0, boot_pte1;
uint64_t *percpu_pttbl;
uint_t	largepagesize = TWOMB_PAGESIZE;
uint_t	largepageshift = TWOMB_PAGESHIFT;
page_t	*kvseg_pplist = NULL;
kmutex_t kvseg_pplist_lock;

#define	cpu_caddr1pte	cpu_caddr1pte64
#define	cpu_caddr2pte	cpu_caddr2pte64
#define	mach_pp	((machpage_t *)pp)
static page_t *page_get_kvseg(void);

#else

#define	MMU_HIGHEST_PFN	0xfffff
#define	MMU_NAME	"mmu32"

uint_t	largepagesize = FOURMB_PAGESIZE;
uint_t	largepageshift = FOURMB_PAGESHIFT;

#define	cpu_caddr1pte	cpu_caddr1pte32
#define	cpu_caddr2pte	cpu_caddr2pte32

#endif

extern u_int page_create_new;
extern u_int page_create_exists;
extern u_int page_create_putbacks;
extern u_int page_create_putbacks;

u_int
page_num_pagesizes()
{
	return ((largepagesupport) ? 2 : 1);
}

size_t
page_get_pagesize(u_int n)
{
	if (n > 1)
		cmn_err(CE_PANIC, "page_get_pagesize: out of range %d", n);
	return ((n == 0) ? PAGESIZE : largepagesize);
}

/*
 * Handle a pagefault.
 */
faultcode_t
pagefault(addr, type, rw, iskernel)
	register caddr_t addr;
	register enum fault_type type;
	register enum seg_rw rw;
	register int iskernel;
{
	register struct as *as;
	register struct proc *p;
	register faultcode_t res;
	caddr_t base;
	size_t len;
	int err;
	int mapped_red;

	mapped_red = segkp_map_red();

	if (iskernel) {
		as = &kas;
	} else {
		p = curproc;
		as = p->p_as;
	}

	/*
	 * Dispatch pagefault.
	 */
	res = as_fault(as->a_hat, as, addr, 1, type, rw);

	/*
	 * If this isn't a potential unmapped hole in the user's
	 * UNIX data or stack segments, just return status info.
	 */
	if (!(res == FC_NOMAP && iskernel == 0))
		goto out;

	/*
	 * Check to see if we happened to faulted on a currently unmapped
	 * part of the UNIX data or stack segments.  If so, create a zfod
	 * mapping there and then try calling the fault routine again.
	 */
	base = p->p_brkbase;
	len = p->p_brksize;

	if (addr < base || addr >= base + len) {		/* data seg? */
		base = (caddr_t)((caddr_t)USRSTACK - p->p_stksize);
		len = p->p_stksize;
		if (addr < base || addr >= (caddr_t)USRSTACK) {	/* stack seg? */
			/* not in either UNIX data or stack segments */
			res = FC_NOMAP;
			goto out;
		}
	}

	/* the rest of this function implements a 3.X 4.X 5.X compatibility */
	/* This code is probably not needed anymore */

	/* expand the gap to the page boundaries on each side */
	len = (((u_int)base + len + PAGEOFFSET) & PAGEMASK) -
	    ((u_int)base & PAGEMASK);
	base = (caddr_t)((u_int)base & PAGEMASK);

	as_rangelock(as);
	if (as_gap(as, NBPG, &base, &len, AH_CONTAIN, addr) != 0) {
		/*
		 * Since we already got an FC_NOMAP return code from
		 * as_fault, there must be a hole at `addr'.  Therefore,
		 * as_gap should never fail here.
		 */
		panic("pagefault as_gap");
	}

	err = as_map(as, base, len, segvn_create, zfod_argsp);
	as_rangeunlock(as);
	if (err) {
		res = FC_MAKE_ERR(err);
		goto out;
	}

	res = as_fault(as->a_hat, as, addr, 1, F_INVAL, rw);

out:
	if (mapped_red)
		segkp_unmap_red();

	return (res);
}

/*ARGSUSED4*/
void
map_addr(caddr_t *addrp, size_t len, offset_t off, int align, u_int flags)
{
	ASSERT(curproc->p_as->a_userlimit == (caddr_t)USERLIMIT);
	map_addr_proc(addrp, len, off, align, (caddr_t)USERLIMIT, curproc);
}

/*
 * map_addr_proc() is the routine called when the system is to
 * choose an address for the user.  We will pick an address
 * range which is the highest available below KERNELBASE.
 *
 * addrp is a value/result parameter.
 *	On input it is a hint from the user to be used in a completely
 *	machine dependent fashion.  We decide to completely ignore this hint.
 *
 *	On output it is NULL if no address can be found in the current
 *	processes address space or else an address that is currently
 *	not mapped for len bytes with a page of red zone on either side.
 *	If align is true, then the selected address will obey the alignment
 *	constraints of a vac machine based on the given off value.
 */
/*ARGSUSED2*/
void
map_addr_proc(
	caddr_t		*addrp,
	size_t		len,
	offset_t	off,
	int		align,
	caddr_t		userlimit,
	struct proc	*p)
{
	register struct as *as = p->p_as;
	register caddr_t addr;
	caddr_t base;
	size_t slen;

	ASSERT(as->a_userlimit == userlimit);

	base = (caddr_t)(((size_t)p->p_brkbase + FOURMB_PAGESIZE)
		& ~FOURMB_PAGEOFFSET);
	slen = (caddr_t)KERNELBASE - base;
	len = (len + PAGEOFFSET) & PAGEMASK;
	if (len >= FOURMB_PAGESIZE) {
		/*
		 * we need to return 4MB aligned address
		 */
		len += FOURMB_PAGESIZE;
		align = 1;
	} else {

		/*
		 * Redzone for each side of the request. This is done to leave
		 * one page unmapped between segments. This is not required, but
		 * it's useful for the user because if their program strays
		 * across a segment boundary, it will catch a fault
		 * immediately making debugging a little easier.
		 */
		len += 2 * PAGESIZE;
		align = 0;
	}

#ifdef NEVER
	if (vac && align)
		len += 2 * shm_alignment;
#endif NEVER

	/*
	 * Look for a large enough hole below KERNELBASE.
	 * After finding it, use the higher part.
	 */
	if (as_gap(as, len, &base, &slen, AH_HI, (caddr_t)NULL) == 0) {
		base = base + slen - len;
		if (align)
			addr = (caddr_t)(((u_int)base + FOURMB_PAGESIZE) &
			    ~FOURMB_PAGEOFFSET);
		else
			addr = base + PAGESIZE;
#ifdef NEVER
		/*
		 * If this remains "non-generic" code, we will not use it
		 * because we do not expect to see a virtual address cache.
		 * This code is being left here in case this module
		 *  becomes a candidate for being generic.
		 */
		if (vac && align) {
			/*
			 * Adjust up to the next shm_alignment boundary, then
			 * up by the pos offset in shm_alignment from there.
			 */
			addr = (caddr_t)roundup((u_int)addr, shm_alignment);
			addr += (int)(off & (shm_alignment - 1));
		}
#endif NEVER
		*addrp = addr;
	} else {
		*addrp = ((caddr_t)NULL);	/* no more virtual space */
	}
}

/*
 * Determine whether [base, base+len] contains a mapable range of
 * addresses at least minlen long. base and len are adjusted if
 * required to provide a mapable range.
 */
/*ARGSUSED3*/
int
valid_va_range(register caddr_t *basep, register size_t *lenp,
	register size_t minlen, register int dir)
{
	register caddr_t hi, lo;

	lo = *basep;
	hi = lo + *lenp;

	/*
	 * If hi rolled over the top, try cutting back.
	 */
	if (hi < lo) {
		if (0 - (u_int)lo + (u_int)hi < minlen)
			return (0);
		if (0 - (u_int)lo < minlen)
			return (0);
		*lenp = 0 - (u_int)lo;
	} else if (hi - lo < minlen)
		return (0);
	return (1);
}

/*
 * Determine whether [addr, addr+len] are valid user addresses.
 */
int
valid_usr_range(caddr_t addr, size_t len, caddr_t userlimit)
{
	caddr_t eaddr = addr + len;

	if (eaddr <= addr || addr >= userlimit || eaddr > userlimit)
		return (0);
	return (1);
}

/*
 * Return 1 if the page frame is onboard memory, else 0.
 */
int
pf_is_memory(pfn_t pf)
{
	return (address_in_memlist(phys_install,
		ptob((unsigned long long)pf), 1));
}


/*
 * initialized by page_coloring_init()
 */
static u_int	page_colors = 1;
static u_int	page_colors_mask;

/*
 * Within a memory range the page freelist and cachelist are hashed
 * into bins based on color. This makes it easiser to search for a page
 * within a specific memory range.
 */
#define	PAGE_COLORS_MAX	256
#define	MAX_MEM_RANGES	4

static	page_t *page_freelists[MAX_MEM_RANGES][PAGE_COLORS_MAX];
static	page_t *page_cachelists[MAX_MEM_RANGES][PAGE_COLORS_MAX];

static	struct	mem_range {
	u_long		pfn_lo;
	u_long		pfn_hi;
} memranges[] = {
	{0x100000, (u_long)0xFFFFFFFF},	/* pfn range for 4G and above */
	{0x80000, 0xFFFFF},	/* pfn range for 2G-4G */
	{0x01000, 0x7FFFF},	/* pfn range for 16M-2G */
	{0x00000, 0x00FFF},	/* pfn range for 0-16M */
};

/* number of memory ranges */
static int nranges = sizeof (memranges)/sizeof (struct mem_range);

/*
 * There are at most 256 colors/bins.  Spread them out under a
 * couple of locks.  There are mutexes for both the page freelist
 * and the page cachelist.
 */

#define	PC_SHIFT	(4)
#define	NPC_MUTEX	(PAGE_COLORS_MAX/(1 << PC_SHIFT))

static kmutex_t	fpc_mutex[NPC_MUTEX];
static kmutex_t	cpc_mutex[NPC_MUTEX];

#if	defined(COLOR_STATS) || defined(DEBUG)

#define	COLOR_STATS_INC(x) (x)++;
#define	COLOR_STATS_DEC(x) (x)--;

static	u_int	pf_size[PAGE_COLORS_MAX];
static	u_int	pc_size[PAGE_COLORS_MAX];

static	u_int	sys_nak_bins[PAGE_COLORS_MAX];
static	u_int	sys_req_bins[PAGE_COLORS_MAX];

#else	COLOR_STATS

#define	COLOR_STATS_INC(x)
#define	COLOR_STATS_DEC(x)

#endif	COLOR_STATS


#define	mach_pp	((machpage_t *)pp)

#define	PP_2_BIN(pp) (((machpage_t *)(pp))->p_pagenum & page_colors_mask)

#define	PC_BIN_MUTEX(bin, list)	((list == PG_FREE_LIST)? \
	&fpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))] :	\
	&cpc_mutex[(((bin)>>PC_SHIFT)&(NPC_MUTEX-1))])

/*
 * hash `as' and `vaddr' to get a bin.
 * sizeof(struct as) is 60.
 * shifting down by 4 bits will cause consecutive as's to be offset by ~3.
 */
#define	AS_2_BIN(as, vaddr) \
	((((u_int)(vaddr) >> PAGESHIFT) + ((u_int)(as) >> 3)) \
	& page_colors_mask)


#define	VADDR_2_BIN(as, vaddr) AS_2_BIN(as, vaddr)

/* returns the number of the freelist to begin the search */
int
pagelist_num(u_long pfn)
{
	int n = 0;

	while (n < nranges) {
		if (pfn >= memranges[n].pfn_lo)
			return (n);
		n++;
	}

	return (0);
}

static page_t *
is_largepage_free(u_int b_pfn)
{
	int	i;
	page_t  *pplist = NULL, *pp;
	extern page_t *page_numtopp_nolock(pfn_t pfn);

	/*
	 * Make sure we are called with a pfn that is 4MB aligned.
	 */
	if (! IS_P2ALIGNED(b_pfn << PAGESHIFT, largepagesize)) {
		cmn_err(CE_PANIC, "largepage: pfn %d not aligned",
			b_pfn);
	}

	for (i = b_pfn; i < (b_pfn + NPTEPERPT); i++) {

		pp = page_numtopp_nolock(i);

		if ((pp == NULL) || (!page_trylock(pp, SE_EXCL)))
			goto failed;
		if (!PP_ISFREE(pp) || !PP_ISAGED(pp)) {
			page_unlock(pp);
			goto failed;
		}
		page_list_sub(PG_FREE_LIST, pp);
		PP_CLRFREE(pp);
		PP_CLRAGED(pp);
		PP_SETREF(pp);
		page_list_concat(&pplist, &pp);
	}

	ASSERT(IS_P2ALIGNED(((machpage_t *)pplist)->p_pagenum << PAGESHIFT,
	    largepagesize));
	ASSERT(((machpage_t *)pplist->p_prev)->p_pagenum
		== (((machpage_t *)pplist)->p_pagenum + (NPTEPERPT - 1)));

	return (pplist);
failed:
	while (pplist) {
		pp = pplist;
		page_sub(&pplist, pp);
		PP_SETFREE(pp);
		PP_CLRALL(pp);
		PP_SETAGED(pp);
		page_list_add(PG_FREE_LIST, pp, PG_LIST_TAIL);
		page_unlock(pp);
	}
	return (NULL);
}

static page_t *
page_get_largepage(void) {

	u_int	start_pfn, pfn;
	page_t *pplist;

	static u_int last_pfn = 0;

	/*
	 * XXX I assume this is because you know that the
	 * first 4MB of physical memory is not free?
	 */
	start_pfn = FOURMB_PAGESIZE / PAGESIZE;

	if (last_pfn == 0) {
		struct memseg *tseg;

		for (tseg = memsegs; tseg; tseg = tseg->next)
				if (tseg->pages_end > last_pfn)
					last_pfn = tseg->pages_end;
	}

	for (pfn = start_pfn; pfn < last_pfn; pfn += NPTEPERPT) {
		if (pplist = is_largepage_free(pfn)) {
			return (pplist);
		}
	}
	return (NULL);
}

/*
 * Take a particular page off of whatever freelist the page is claimed to be on.
 */
void
page_list_sub(int list, page_t *pp)
{
	u_int		bin;
	kmutex_t	*pcm;
	page_t		**ppp;
	int		n;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	n = pagelist_num(((machpage_t *)pp)->p_pagenum);

	if (list == PG_FREE_LIST) {
		ppp = &page_freelists[n][bin];
		COLOR_STATS_DEC(pf_size[bin]);
		ASSERT(PP_ISAGED(pp));

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ppp = &page_cachelists[n][bin];
		COLOR_STATS_DEC(pc_size[bin]);
		ASSERT(PP_ISAGED(pp) == 0);
	}

	mutex_enter(pcm);
	page_sub(ppp, pp);
	mutex_exit(pcm);
}

void
page_list_add(int list, page_t *pp, int where)
{
	page_t		**ppp;
	kmutex_t	*pcm;
	u_int		bin;
#ifdef	DEBUG
	u_int		*pc_stats;
#endif
	int		n;

	ASSERT(PAGE_EXCL(pp));
	ASSERT(PP_ISFREE(pp));
	ASSERT(!hat_page_is_mapped(pp));

#ifdef	PTE36
	/*
	 * If this page was mapped by kvseg (kmem_alloc area)
	 * return this page to kvseg_pplist pool.
	 */
	if (mach_pp->p_kpg) {
		mutex_enter(&kvseg_pplist_lock);
		page_add(&kvseg_pplist, pp);
		mutex_exit(&kvseg_pplist_lock);
		return;
	}
#endif

	bin = PP_2_BIN(pp);
	pcm = PC_BIN_MUTEX(bin, list);

	n = pagelist_num(((machpage_t *)pp)->p_pagenum);

	if (list == PG_FREE_LIST) {
		ASSERT(PP_ISAGED(pp));
		ASSERT((pc_stats = &pf_size[bin]) != NULL);
		ppp = &page_freelists[n][bin];

		ASSERT(page_pptonum(pp) <= physmax);
	} else {
		ASSERT(pp->p_vnode);
		ASSERT((pp->p_offset & 0xfff) == 0);
		ASSERT((pc_stats = &pc_size[bin]) != NULL);
		ppp = &page_cachelists[n][bin];
	}

	mutex_enter(pcm);
	COLOR_STATS_INC(*pc_stats);
	page_add(ppp, pp);

	if (where == PG_LIST_TAIL) {
		*ppp = (*ppp)->p_next;
	}
	mutex_exit(pcm);

	/*
	 * It is up to the caller to unlock the page!
	 */
	ASSERT(PAGE_EXCL(pp));
}


/*
 * When a bin is empty, and we can't satisfy a color request correctly,
 * we scan.  If we assume that the programs have reasonable spatial
 * behavior, then it will not be a good idea to use the adjacent color.
 * Using the adjacent color would result in virtually adjacent addresses
 * mapping into the same spot in the cache.  So, if we stumble across
 * an empty bin, skip a bunch before looking.  After the first skip,
 * then just look one bin at a time so we don't miss our cache on
 * every look. Be sure to check every bin.  Page_create() will panic
 * if we miss a page.
 *
 * This also explains the `<=' in the for loops in both page_get_freelist()
 * and page_get_cachelist().  Since we checked the target bin, skipped
 * a bunch, then continued one a time, we wind up checking the target bin
 * twice to make sure we get all of them bins.
 */
#define	BIN_STEP	19

/*
 * Find the `best' page on the freelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking and accounting.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 *
 * Finds a page, removes it, THEN locks it.
 */
/*ARGSUSED*/
page_t *
page_get_freelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	size_t size,
	u_int flags)
{
	u_int		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;
	int		n;

	if (size == largepagesize)
		return (page_get_largepage());
	else if (size != MMU_PAGESIZE)
		cmn_err(CE_PANIC,
		"page_get_freelist: illegal size request for i86 platform");

	/*
	 * In PAE mode, pages allocated to kmem_alloc come from
	 * kvseg_pplist pool. This is a pool of pages below 4Gb, big
	 * enough to satisfy kmem_alloc requests.
	 */
#ifdef	PTE36
	if (kvseg_pplist && (vp == &kvp) && (vaddr >= Sysbase) &&
	    (vaddr < Syslimit))
		return (page_get_kvseg());
#endif

	/*
	 * Only hold one freelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);


	for (n = 0; n < nranges; n++) {
	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_freelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[n][bin]) {
				/*
				 * These were set before the page
				 * was put on the free list,
				 * they must still be set.
				 */
				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				first_pp = pp;

				/*
				 * Walk down the hash chain
				 */

				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;

					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp));
					ASSERT(pp->p_vnode == NULL);
					ASSERT(pp->p_hash == NULL);
					ASSERT(pp->p_offset == -1);

					if (pp == first_pp) {
						pp = NULL;
						break;
					}
				}

				if (pp != NULL) {
				    COLOR_STATS_DEC(pf_size[bin]);
				    page_sub(&page_freelists[n][bin], pp);

				    ASSERT(page_pptonum(pp) <= physmax);

				    if ((PP_ISFREE(pp) == 0) ||
					(PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC,
					    "free page is not. pp %x", (int)pp);
				    }
				    mutex_exit(pcm);
				    return (pp);
				}
			}
			mutex_exit(pcm);
		}

		/*
		 * Wow! The bin was empty.
		 */
		COLOR_STATS_INC(sys_nak_bins[bin]);
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* try the next memory range */
	}
	return (NULL);
}

/*
 * Find the `best' page on the cachelist for this (vp,off) (as,vaddr) pair.
 *
 * Does its own locking.
 * If PG_MATCH_COLOR is set, then NULL will be returned if there are no
 * pages of the proper color even if there are pages of a different color.
 * Otherwise, scan the bins for ones with pages.  For each bin with pages,
 * try to lock one of them.  If no page can be locked, try the
 * next bin.  Return NULL if a page can not be found and locked.
 *
 * Finds a pages, TRYs to lock it, then removes it.
 */
/*ARGSUSED*/
page_t *
page_get_cachelist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	u_int flags)
{
	kmutex_t	*pcm;
	int		i;
	page_t		*pp;
	page_t		*first_pp;
	int		bin;
	int		n;


#ifdef	PTE36
	if (kvseg_pplist && (vp == &kvp) && (vaddr >= Sysbase) &&
	    (vaddr < Syslimit))
		return (page_get_kvseg());
#endif

	/*
	 * Only hold one cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */
	bin = VADDR_2_BIN(as, vaddr);

	for (n = 0; n < nranges; n++) {
	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[n][bin]) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);

				while (!page_trylock(pp, SE_EXCL)) {
					pp = pp->p_next;
					if (pp == first_pp) {
						/*
						 * We have searched the
						 * complete list!
						 * And all of them (might
						 * only be one) are locked.
						 * This can happen since
						 * these pages can also be
						 * found via the hash list.
						 * When found via the hash
						 * list, they are locked
						 * first, then removed.
						 * We give up to let the
						 * other thread run.
						 */
						pp = NULL;
						break;
					}
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISFREE(pp));
					ASSERT(PP_ISAGED(pp) == 0);
				}

				if (pp) {
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					COLOR_STATS_DEC(pc_size[bin]);
					page_sub(&page_cachelists[n][bin], pp);
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		COLOR_STATS_INC(sys_nak_bins[bin]);
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* try the next memory range */
	}


	return (NULL);
}

int	x86_l2cache = 512 * 1024;

/*
 * page_coloring_init()
 * called once at startup from kphysm_init() -- before memialloc()
 * is invoked to do the 1st page_free()/page_freelist_add().
 *
 * initializes page_colors and page_colors_mask
 */
void
page_coloring_init()
{
	u_int colors;


	colors = x86_l2cache/ PAGESIZE;	/* 256 */
	if (colors > PAGE_COLORS_MAX - 1)
		colors = PAGE_COLORS_MAX - 1;
	page_colors = colors;
	page_colors_mask = colors - 1;
}

/*
 * This function is similar to page_get_freelist()/page_get_cachelist()
 * but it searches both the lists to find a page with the specified
 * color (or no color) and DMA attributes. The search is done in the
 * freelist first and then in the cache list within the highest memory
 * range (based on DMA attributes) before searching in the lower
 * memory ranges.
 *
 * Note: This function is called only by page_create_io().
 */
/*ARGSUSED*/
page_t *
page_get_anylist(
	struct vnode *vp,
	u_offset_t off,
	struct as *as,
	caddr_t vaddr,
	size_t size,
	u_int flags,
	ddi_dma_attr_t *dma_attr)
{
	u_int		bin;
	kmutex_t	*pcm;
	int		i;
	page_t		*pp, *first_pp;
	int		n;

	/* currently we support only 4k pages */
	if (size != PAGESIZE)
		return (NULL);

	/*
	 * Only hold one freelist or cachelist lock at a time, that way we
	 * can start anywhere and not have to worry about lock
	 * ordering.
	 *
	 * color = (vpage % cache_pages) + constant.
	 */

	if (dma_attr == NULL)
		n = 0;	/* start with the highest memory range */
	else {
		/*
		 * We can gaurantee alignment only for page boundary.
		 */
		if (dma_attr->dma_attr_align > MMU_PAGESIZE)
			return (NULL);
		n = pagelist_num(btop(dma_attr->dma_attr_addr_hi));
	}

	while (n < nranges) {
	    /* start with the bin of matching color */
	    bin = VADDR_2_BIN(as, vaddr);

	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);

		/* try the freelist first */
		if (page_freelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_FREE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_freelists[n][bin]) {
				/*
				 * These were set before the page
				 * was put on the free list,
				 * they must still be set.
				 */
				ASSERT(PP_ISFREE(pp));
				ASSERT(PP_ISAGED(pp));
				ASSERT(pp->p_vnode == NULL);
				ASSERT(pp->p_hash == NULL);
				ASSERT(pp->p_offset == (u_offset_t)-1);
				first_pp = pp;

				/*
				 * Walk down the hash chain
				 */

				while (pp) {
				    if (page_trylock(pp, SE_EXCL)) {
					if (dma_attr == NULL)
						break;
					/*
					 * Check for the page within the
					 * specified DMA attributes.
					 *
					 */
					if (ptob((unsigned long long)
					    (mach_pp->p_pagenum + 1)) <=
						dma_attr->dma_attr_addr_hi)
							break;
					page_unlock(pp);
					/* continue looking */
				    }
				    pp = pp->p_next;

				    ASSERT(PP_ISFREE(pp));
				    ASSERT(PP_ISAGED(pp));
				    ASSERT(pp->p_vnode == NULL);
				    ASSERT(pp->p_hash == NULL);
				    ASSERT(pp->p_offset == -1);

				    if (pp == first_pp) {
					pp = NULL;
					break;
				    }
				}

				if (pp != NULL) {
				    COLOR_STATS_DEC(pf_size[bin]);
				    page_sub(&page_freelists[n][bin], pp);

				    ASSERT(page_pptonum(pp) <= physmax);

				    if ((PP_ISFREE(pp) == 0) ||
					(PP_ISAGED(pp) == 0)) {
					cmn_err(CE_PANIC,
					    "free page is not. pp %x", (int)pp);
				    }
				    mutex_exit(pcm);
				    return (pp);
				}
			}
			mutex_exit(pcm);
		}


		/*
		 * Wow! The bin was empty.
		 */
		COLOR_STATS_INC(sys_nak_bins[bin]);
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    /* failed to find a page in the freelist; try it in the cachelist */

	    /* start with the bin of matching color */
	    bin = VADDR_2_BIN(as, vaddr);

	    for (i = 0; i <= page_colors; i++) {
		COLOR_STATS_INC(sys_req_bins[bin]);
		if (page_cachelists[n][bin]) {
			pcm = PC_BIN_MUTEX(bin, PG_CACHE_LIST);
			mutex_enter(pcm);
			/* LINTED */
			if (pp = page_cachelists[n][bin]) {
				first_pp = pp;
				ASSERT(pp->p_vnode);
				ASSERT(PP_ISAGED(pp) == 0);
				while (pp) {
				    if (page_trylock(pp, SE_EXCL)) {
					if (dma_attr == NULL)
						break;
					/*
					 * Check for the page within the
					 * specified DMA attributes.
					 *
					 */
					if (ptob((unsigned long long)
					    (mach_pp->p_pagenum + 1)) <=
						dma_attr->dma_attr_addr_hi)
							break;
					page_unlock(pp);
					/* continue looking */
				    }

				    pp = pp->p_next;
				    if (pp == first_pp) {
					/*
					 * We have searched the
					 * complete list!
					 * And all of them (might
					 * only be one) are locked.
					 * This can happen since
					 * these pages can also be
					 * found via the hash list.
					 * When found via the hash
					 * list, they are locked
					 * first, then removed.
					 * We give up to let the
					 * other thread run.
					 */
					pp = NULL;
					break;
				    }
				    ASSERT(pp->p_vnode);
				    ASSERT(PP_ISFREE(pp));
				    ASSERT(PP_ISAGED(pp) == 0);
				}

				if (pp) {
					/*
					 * Found and locked a page.
					 * Pull it off the list.
					 */
					COLOR_STATS_DEC(pc_size[bin]);
					page_sub(&page_cachelists[n][bin], pp);
					mutex_exit(pcm);
					ASSERT(pp->p_vnode);
					ASSERT(PP_ISAGED(pp) == 0);
					return (pp);
				}
			}
			mutex_exit(pcm);
		}
		COLOR_STATS_INC(sys_nak_bins[bin]);
		/*
		 * Wow! The bin was empty or no page could be locked.
		 * If only the proper bin is to be checked, get out
		 * now.
		 */
		if (flags & PG_MATCH_COLOR) {
			break;
		}
		bin += (i == 0) ? BIN_STEP : 1;
		bin &= page_colors_mask;
	    }

	    n++; /* try the next freelist */
	}
	return (NULL);
}


/*
 * page_create_io()
 *
 * This function is a copy of page_create_va() with an additional
 * argument 'mattr' that specifies DMA memory requirements to
 * the page list functions. This function is used by the segkmem
 * allocator so it is only to create new pages (i.e PG_EXCL is
 * set).
 *
 * Note: This interface is currently used by x86 PSM only and is
 *	 not fully specified so the commitment level is only for
 *	 private interface specific to x86. This interface uses PSM
 *	 specific page_get_anylist() interface.
 */

extern kmutex_t	ph_mutex[];
extern u_int	ph_mutex_shift;

#define	PAGE_HASH_MUTEX(index)	&ph_mutex[(index) >> ph_mutex_shift]

#define	PAGE_HASH_SEARCH(index, pp, vp, off) { \
	for ((pp) = page_hash[(index)]; (pp); (pp) = (pp)->p_hash) { \
		if ((pp)->p_vnode == (vp) && (pp)->p_offset == (off)) \
			break; \
	} \
}

page_t *
page_create_io(
	struct vnode	*vp,
	register u_offset_t off,
	u_int		bytes,
	u_int		flags,
	struct as	*as,
	caddr_t		vaddr,
	ddi_dma_attr_t	*mattr)	/* DMA memory attributes if any */
{
	page_t		*plist = NULL;
	register int	npages;
	page_t		*npp = NULL;
	u_int		pages_req;

	TRACE_5(TR_FAC_VM, TR_PAGE_CREATE_START,
		"page_create_start:vp %x off %llx bytes %u flags %x freemem %d",
		vp, off, bytes, flags, freemem);

	ASSERT((flags & ~(PG_EXCL | PG_WAIT)) == 0);	/* but no others */

	pages_req = npages = btopr(bytes);

	/*
	 * Do the freemem and pcf accounting.
	 */
	if (!page_create_wait(npages, flags)) {
		return (NULL);
	}

	TRACE_3(TR_FAC_VM, TR_PAGE_CREATE_SUCCESS,
		"page_create_success:vp %x off %llx freemem %d",
		vp, off, freemem);

	/*
	 * If satisfying this request has left us with too little
	 * memory, start the wheels turning to get some back.  The
	 * first clause of the test prevents waking up the pageout
	 * daemon in situations where it would decide that there's
	 * nothing to do.
	 */
	if (nscan < desscan && freemem < minfree) {
		TRACE_1(TR_FAC_VM, TR_PAGEOUT_CV_SIGNAL,
			"pageout_cv_signal:freemem %ld", freemem);
		cv_signal(&proc_pageout->p_cv);
	}

	/*
	 * Loop around collecting the requested number of pages.
	 * Most of the time, we have to `create' a new page. With
	 * this in mind, pull the page off the free list before
	 * getting the hash lock.  This will minimize the hash
	 * lock hold time, nesting, and the like.  If it turns
	 * out we don't need the page, we put it back at the end.
	 */
	while (npages--) {
		register page_t	*pp;
		kmutex_t	*phm = NULL;
		u_int		index;

		index = PAGE_HASH_FUNC(vp, off);
top:
		ASSERT(phm == NULL);
		ASSERT(index == PAGE_HASH_FUNC(vp, off));
		ASSERT(MUTEX_NOT_HELD(page_vnode_mutex(vp)));

		if (npp == NULL) {
			/*
			 * Try to get the page of any color either from
			 * the freelist or from the cache list.
			 */
			npp = page_get_anylist(vp, off, as, vaddr,
				PAGESIZE, flags & ~PG_MATCH_COLOR, mattr);
			if (npp == NULL) {
				if (mattr == NULL) {
					/*
					 * Not looking for a special page;
					 * panic!
					 */
					cmn_err(CE_PANIC,
						"no page found %d", npages);
				}
				/*
				 * No page found! This can happen
				 * if we are looking for a page
				 * within a specific memory range
				 * for DMA purposes. If PG_WAIT is
				 * specified then we wait for a
				 * while and then try again. The
				 * wait could be forever if we
				 * don't get the page(s) we need.
				 *
				 * Note: XXX We really need a mechanism
				 * to wait for pages in the desired
				 * range. For now, we wait for any
				 * pages and see if we can use it.
				 */

				if ((mattr != NULL) && (flags & PG_WAIT)) {
					delay(10);
					goto top;
				}

				goto fail; /* undo accounting stuff */
			}

			if (PP_ISAGED(npp) == 0) {
				/*
				 * Since this page came from the
				 * cachelist, we must destroy the
				 * old vnode association.
				 */
				page_hashout(npp, (kmutex_t *)NULL);
			}
		}

		/*
		 * We own this page!
		 */
		ASSERT(PAGE_EXCL(npp));
		ASSERT(npp->p_vnode == NULL);
		ASSERT(!hat_page_is_mapped(npp));
		PP_CLRFREE(npp);
		PP_CLRAGED(npp);

		/*
		 * Here we have a page in our hot little mits and are
		 * just waiting to stuff it on the appropriate lists.
		 * Get the mutex and check to see if it really does
		 * not exist.
		 */
		phm = PAGE_HASH_MUTEX(index);
		mutex_enter(phm);
		PAGE_HASH_SEARCH(index, pp, vp, off);
		if (pp == NULL) {
			VM_STAT_ADD(page_create_new);
			pp = npp;
			npp = NULL;
			if (!page_hashin(pp, vp, off, phm)) {
				/*
				 * Since we hold the page hash mutex and
				 * just searched for this page, page_hashin
				 * had better not fail.  If it does, that
				 * means somethread did not follow the
				 * page hash mutex rules.  Panic now and
				 * get it over with.  As usual, go down
				 * holding all the locks.
				 */
				ASSERT(MUTEX_NOT_HELD(phm));
				cmn_err(CE_PANIC,
				    "page_create: hashin failed %p %p %llx %p",
				    (void *)pp, (void *)vp, off, (void *)phm);

			}
			ASSERT(MUTEX_NOT_HELD(phm));	/* hashin dropped it */
			phm = NULL;

			/*
			 * Hat layer locking need not be done to set
			 * the following bits since the page is not hashed
			 * and was on the free list (i.e., had no mappings).
			 *
			 * Set the reference bit to protect
			 * against immediate pageout
			 *
			 * XXXmh modify freelist code to set reference
			 * bit so we don't have to do it here.
			 */
			page_set_props(pp, P_REF);
		} else {
			/*
			 * NOTE: This should not happen for pages associated
			 *	 with kernel vnode 'kvp'.
			 */
			if (vp == &kvp)
			    cmn_err(CE_NOTE, "page_create: page not expected "
				"in hash list for kernel vnode - pp 0x%p",
				(void *)pp);
			VM_STAT_ADD(page_create_exists);
			goto fail;
		}

		/*
		 * Got a page!  It is locked.  Acquire the i/o
		 * lock since we are going to use the p_next and
		 * p_prev fields to link the requested pages together.
		 */
		page_io_lock(pp);
		page_add(&plist, pp);
		plist = plist->p_next;
		off += PAGESIZE;
		vaddr += PAGESIZE;
	}

	return (plist);

fail:
	if (npp != NULL) {
		/*
		 * Did not need this page after all.
		 * Put it back on the free list.
		 */
		VM_STAT_ADD(page_create_putbacks);
		PP_SETFREE(npp);
		PP_SETAGED(npp);
		npp->p_offset = (u_offset_t)-1;
		page_list_add(PG_FREE_LIST, npp, PG_LIST_TAIL);
		page_unlock(npp);
	}

	/*
	 * Give up the pages we already got.
	 */
	while (plist != NULL) {
		register page_t	*pp;

		pp = plist;
		page_sub(&plist, pp);
		page_io_unlock(pp);
		/*LINTED: constant in conditional ctx*/
		VN_DISPOSE(pp, B_INVAL, 0, kcred);
	}

	/* undo the accounting of freemem */
	VM_STAT_ADD(page_create_putbacks);
	page_create_putback(pages_req);

	return (NULL);
}


/*
 * Copy the data from the physical page represented by "frompp" to
 * that represented by "topp". ppcopy uses CPU->cpu_caddr1 and
 * CPU->cpu_caddr2.  It assumes that no one uses either map at interrupt
 * level and no one sleeps with an active mapping there.
 */
void
ppcopy(page_t *frompp, page_t *topp)
{
	caddr_t caddr1;
	caddr_t caddr2;
	kmutex_t *ppaddr_mutex;
	struct	pte	pte;
	pte_t	*caddr1_pte, *caddr2_pte;
	extern void 	hat_mempte();

	ASSERT(PAGE_LOCKED(frompp));
	ASSERT(PAGE_LOCKED(topp));

	kpreempt_disable(); /* can't preempt if holding caddr1, caddr2 */

	caddr1 = CPU->cpu_caddr1;
	caddr2 = CPU->cpu_caddr2;
	caddr1_pte = (pte_t *)CPU->cpu_caddr1pte;
	caddr2_pte = (pte_t *)CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;

	mutex_enter(ppaddr_mutex);

	hat_mempte(frompp, PROT_READ, &pte, caddr1);
	LOAD_PTE(caddr1_pte, *(pteptr_t)&pte);
	hat_mempte(topp, PROT_READ|PROT_WRITE, &pte, caddr2);
	LOAD_PTE(caddr2_pte, *(pteptr_t)&pte);
	mmu_tlbflush_entry(caddr1);
	mmu_tlbflush_entry(caddr2);
	bcopy(caddr1, caddr2, PAGESIZE);


	mutex_exit(ppaddr_mutex);

	kpreempt_enable();
}

/*
 * Zero the physical page from off to off + len given by `pp'
 * without changing the reference and modified bits of page.
 * pagezero uses CPU->cpu_caddr2 and assumes that no one uses this
 * map at interrupt level and no one sleeps with an active mapping there.
 *
 * pagezero() must not be called at interrupt level.
 */
void
pagezero(page_t *pp, u_int off, u_int len)
{
	caddr_t caddr2;
	pte_t 	*caddr2_pte;
	kmutex_t *ppaddr_mutex;
	struct	pte pte;
	extern void 	hat_mempte();

	ASSERT((int)len > 0 && (int)off >= 0 && off + len <= PAGESIZE);
	ASSERT(PAGE_LOCKED(pp));

	kpreempt_disable(); /* can't preempt if holding caddr2 */

	caddr2 = CPU->cpu_caddr2;
	caddr2_pte = (pte_t *)CPU->cpu_caddr2pte;

	ppaddr_mutex = &CPU->cpu_ppaddr_mutex;
	mutex_enter(ppaddr_mutex);

	hat_mempte(pp, PROT_READ|PROT_WRITE, &pte, caddr2);
	LOAD_PTE(caddr2_pte, *(pteptr_t)&pte);
	mmu_tlbflush_entry(caddr2);

	bzero(caddr2 + off, len);

	mutex_exit(ppaddr_mutex);
	kpreempt_enable();
}


void
setup_vaddr_for_ppcopy(struct cpu *cpup)
{
	uint32_t addr;

	if ((addr = rmalloc(kernelmap, (long)(2 * CLSIZE))) == NULL) {
		cmn_err(CE_PANIC,
		    "Couldn't rmalloc pages for cpu_caddr");
	}
	cpup->cpu_caddr1 = Sysbase + mmu_ptob(addr);
	cpup->cpu_caddr2 = cpup->cpu_caddr1 + PAGESIZE;
	cpup->cpu_caddr1pte =
		(pteptr_t)&Sysmap1[mmu_btop(cpup->cpu_caddr1 - Sysbase)];
	cpup->cpu_caddr2pte =
		(pteptr_t)&Sysmap1[mmu_btop(cpup->cpu_caddr2 - Sysbase)];

	mutex_init(&cpup->cpu_ppaddr_mutex, NULL, MUTEX_DEFAULT, NULL);
}

void
psm_pageinit(machpage_t *pp, uint32_t pnum)
{
	pp->p_pagenum = pnum;
	cv_init(&(pp->p_mlistcv), NULL, CV_DEFAULT, NULL);
}


void
mmu_init()
{
	extern struct mmuinfo mmuinfo;

	if (largepagesize == TWOMB_PAGESIZE) {
	    if ((x86_feature & X86_PAE) == 0) {
		cmn_err(CE_PANIC, "Processor does not support"
		    "Physical Address Extension");
	    }
	    if ((x86_feature & X86_CXS) == 0) {
		cmn_err(CE_PANIC, "Processor does not support"
		    "cmpxchg8b instruction");
	    }
	}
	if (x86_feature & X86_LARGEPAGE)
		largepagesupport = 1;
	mmuinfo.mmu_highest_pfn = MMU_HIGHEST_PFN;
	mmuinfo.mmu_name = MMU_NAME;
}

/* ARGSUSED */
int
cpuid2nodeid(int cpun)
{
	return (0);
}

/* ARGSUSED */
void *
kmem_node_alloc(size_t size, int flags, int node)
{
	return (kmem_alloc(size, flags));
}


#ifdef	PTE36

void
setup_kernel_page_directory(struct cpu *cpup)
{
	caddr_t	va;
	uint32_t addr;
	int i, copy = 1;

	if (kernel_only_pagedir == NULL) {
		kernel_only_pagedir = kmem_zalloc(ptob(1), KM_NOSLEEP);
		if (kernel_only_pagedir == NULL)
		    prom_panic("Cannot allocate memory for page directory");
		va =
		    (caddr_t)kmem_zalloc(MMU_PTTBL_SIZE * (NCPU+2),
			KM_NOSLEEP);
		if (va == NULL)
		    prom_panic("Cannot allocate memory for page directories");
		if ((u_int)va & (MMU_PTTBL_SIZE - 1)) {
			va = (caddr_t)((u_int)(va  + MMU_PTTBL_SIZE) &
				~(MMU_PTTBL_SIZE - 1));
		}
		kernel_only_pttbl = (uint64_t *)va;
		percpu_pttbl = kernel_only_pttbl + NPDPERAS;
		copy = 0;
	}

	cpup->cpu_pagedir = (uint64_t *)kmem_zalloc(ptob(NPDPERAS), KM_NOSLEEP);
	if (cpup->cpu_pagedir == NULL)
		prom_panic("Cannot allocate memory for page directory");


	ASSERT((((u_int)cpup->cpu_pagedir) & PAGEOFFSET) == 0);

	cpup->cpu_pgdirpttbl = percpu_pttbl;
	percpu_pttbl += NPDPERAS;

	if (!copy)
		return;

	kas.a_hat->hat_cpusrunning |= 1 << cpup->cpu_id;
	bcopy((caddr_t)kernel_only_pagedir,
	    (caddr_t)cpup->cpu_pagedir + ((NPDPERAS - 1) * MMU_PAGESIZE),
	    ptob(1));
	/*
	 * We need to map the startup code in the first 4Mb chunk 1-1
	 */
	cpup->cpu_pagedir[0] = boot_pte0;
	cpup->cpu_pagedir[1] = boot_pte1;

	for (i = 0, addr = (u_int)cpup->cpu_pagedir; i < NPDPERAS;
	    i++, addr += MMU_PAGESIZE) {
		cpup->cpu_pgdirpttbl[i] = cpup->cpu_pgdirpttblent[i] =
		    PTBL_ENT(hat_getkpfnum((caddr_t)addr));
	}
	cpup->cpu_cr3 = (u_int)(hat_getkpfnum((caddr_t)cpup->cpu_pgdirpttbl) <<
			MMU_STD_PAGESHIFT) +
			((u_int)cpup->cpu_pgdirpttbl & PAGEOFFSET);

}

void
clear_bootpde(struct cpu *cpup)
{
	boot_pte0 = cpup->cpu_pagedir[0];
	boot_pte1 = cpup->cpu_pagedir[1];
	cpup->cpu_pagedir[0] = MMU_STD_INVALIDPTE;
	cpup->cpu_pagedir[1] = MMU_STD_INVALIDPTE;
}

page_t *
page_get_kvseg(void)
{
	page_t	*pp, *first_pp;


	mutex_enter(&kvseg_pplist_lock);
	pp = kvseg_pplist;
	ASSERT(pp);
	ASSERT(PP_ISFREE(pp));
	first_pp = pp;

	/*
	 * Walk down the hash chain
	 */

	while (!page_trylock(pp, SE_EXCL)) {
		pp = pp->p_next;

		ASSERT(PP_ISFREE(pp));
		if (pp == first_pp) {
			pp = NULL;
			break;
		}
	}
	if (pp)
		page_sub(&kvseg_pplist, pp);
	mutex_exit(&kvseg_pplist_lock);
	return (pp);
}
void
mmu_setup_kvseg(pfn_t highest_pfn)
{
	int 		i;
	size_t		npages, pages_mapped;
	caddr_t		addr;
	page_t		*pp;
	ddi_dma_attr_t	attr;
	pfn_t		pfn;

	/*
	 * If the highest memory to be mapped is less than 4Gb,
	 * just return
	 */
	if (highest_pfn < 0x100000)
		return;

	/*
	 * pre allocate physical pages for kvseg
	 */
	mutex_enter(&kvseg_pplist_lock);
	pages_mapped = 0;
	for (addr = Sysbase; addr < Syslimit; addr += MMU_PAGESIZE) {
		if ((pfn = hat_getkpfnum(addr)) != PFN_INVALID) {
			pp = page_numtopp_nolock(pfn);
			if (pp)
				mach_pp->p_kpg = 1;
			pages_mapped++;
		}
	}

	npages = btop(Syslimit - Sysbase) - pages_mapped;

	if (!page_create_wait(npages, 0)) {
		cmn_err(CE_PANIC, "mmu_setup_kvseg: "
		    "Can not pre allocate pages for kvseg\n");
	}
	bzero((caddr_t)&attr, sizeof (attr));
	attr.dma_attr_addr_hi = 0x0FFFFFFFFULL;
	for (i = 0, addr = Sysbase; i < npages; i++, addr += MMU_PAGESIZE) {
		pp = page_get_anylist(&kvp, 0, &kas, addr,
			PAGESIZE, 0, &attr);
		if (pp == NULL)
			cmn_err(CE_PANIC, "mmu_setup_kvseg: "
			    "Can not pre allocate pages for kvseg\n");
		mach_pp->p_kpg = 1;

		page_add(&kvseg_pplist, pp);
		page_unlock(pp);
	}
	mutex_exit(&kvseg_pplist_lock);
}
#else	/* PTE36 */


void
setup_kernel_page_directory(struct cpu *cpup)
{
	extern u_int va_to_pfn();
	int copy = 1;

	if (kernel_only_pagedir == NULL) {
		kernel_only_pagedir = kmem_zalloc(ptob(1), KM_NOSLEEP);
		if (kernel_only_pagedir == NULL)
		    prom_panic("Cannot allocate memory for page directory");
		kernel_only_cr3 = (u_int)(va_to_pfn((u_int)kernel_only_pagedir)
				<<MMU_STD_PAGESHIFT);
		copy = 0;
	}
	cpup->cpu_pagedir =
		(uint32_t *)kmem_zalloc(ptob(1), KM_NOSLEEP);
	if (cpup->cpu_pagedir == NULL)
		prom_panic("Cannot allocate memory for page directories");
	if (copy) {
	    kas.a_hat->hat_cpusrunning |= 1 << cpup->cpu_id;
	    bcopy((caddr_t)kernel_only_pagedir, (caddr_t)cpup->cpu_pagedir,
		ptob(1));
	    cpup->cpu_cr3 = ptob(hat_getkpfnum((caddr_t)cpup->cpu_pagedir));
	}
}

void
clear_bootpde(struct cpu *cpup)
{
	*(uint32_t *)(cpup->cpu_pagedir) = 0;
}

/*ARGSUSED*/
void 	mmu_setup_kvseg(pfn_t pfn) {}
#endif
void post_startup_mmu_initialization(void) {}
