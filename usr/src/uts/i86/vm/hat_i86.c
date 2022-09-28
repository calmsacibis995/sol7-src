/*
 * Copyright (c) 1989, 1992-1994, 1997, 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)hat_i86.c	1.112	98/02/04 SMI"

/*
 * VM - Hardware Address Translation management.
 *
 * This file implements the machine specific hardware translation
 * needed by the VM system.  The machine independent interface is
 * described in <common/vm/hat.h> while the machine dependent interface
 * and data structures are described in <i86/vm/hat_i86.h>.
 *
 * The hat layer manages the address translation hardware as a cache
 * driven by calls from the higher levels in the VM system.  Nearly
 * all the details of how the hardware is managed shound not be visable
 * about this layer except for miscellanous machine specific functions
 * (e.g. mapin/mapout) that work in conjunction with this code.  Other
 * than a small number of machine specific places, the hat data
 * structures seen by the higher levels in the VM system are opaque
 * and are only operated on by the hat routines.  Each address space
 * contains a struct hat and a page contains an opaque pointer which
 * is used by the hat code to hold a list of active translations to
 * that page.
 */

/*
 * Hardware address translation routines for the Intel 80x86 MMU.
 * Originally based upon sun4m (srmmu) hat code.
 */

#include <sys/machparam.h>
#include <sys/machsystm.h>
#include <sys/mman.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/vtrace.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/thread.h>
#include <sys/proc.h>

#include <sys/kmem.h>
#include <sys/cpu.h>
#include <sys/cmn_err.h>
#include <sys/sysmacros.h>
#include <sys/map.h>
#include <sys/vmmac.h>
#include <sys/vmsystm.h>

#include <vm/hat.h>
#include <vm/hat_i86.h>
#include <vm/as.h>
#include <vm/seg.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg_kp.h>
#include <vm/rm.h>
#include <vm/seg_spt.h>
#include <sys/var.h>
#include <sys/x86_archext.h>
#include <vm/faultcode.h>
#include <sys/atomic.h>


#define	setpte_nosync(a)	(*((pteptr_t)(a)) |= (1 << 9))
#define	clrpte_nosync(a)	(*((pteptr_t)(a)) &= ~(1 << 9))
#define	getpte_nosync(a)	(((*((pteptr_t)(a))) & (1 << 9)) != 0)




struct	hmespace	{
	struct hmespace	*next;
} *hat_hmespace_freelist;

kmutex_t	hat_res_mutex;		/* protect global freehat list */

struct	hat	*hats;
struct	hat	*hatsNHATS;
struct	hat	*hatfree = (struct hat *)NULL;

int		nhats;
int		hat_inited = 0;
/*
 * hat hat layer tunables
 *
 * hat_hwptepages		Default is 3 * maxusers, Normally you would need
 *				two per process.
 *
 * hat_perprocess_pagedir	This by default is 0, If 1 then we would
 *				allocate a page directory per process.
 *
 * hat_numhmespace		Defaults to 4. which consumes 8K.
 *
 * hat_dup_npts		Default is 4, These are 4K pagetables used for
 *				supporting hat_dup()
 *
 * hat_use4kpte		If this is set then we use 4k ptes to map
 *				kernel text and data.
 *
 * hat_dup_enable	Default value is 0. If this variable is set to non
 *			zero value, the child process would share parent's
 *			ptes.
 */

int 	hat_hwptepages = 0;
int	hat_perprocess_pagedir = 0;
int	hat_numhmespace = 0;
int	hat_dup_npts = 0;
int	hat_maps_per_page = 2;
int	hat_hwpps_per_user = 6;
int	hat_use4kpte = 0;
int	hat_dup_enable = 0;

/*
 * End of kernelmap Virtual address for which
 * pagetables have been allocated
 */
extern u_int	phys_syslimit;
extern int	kadb_is_running;

/*
 * i86MMU has several levels of locking.  The locks must be acquired
 * in the correct order to prevent deadlocks.
 *
 * Page mapping lists are locked with the per-page p_inuse bit, which
 * is manipulated by hat_mlist_enter() and hat_mlist_exit(). This
 * bit must be held to look at or change a page's p_mapping list.
 *
 * In the pp structure the following  bits are manipulated by the hat layer
 * mach_pp->p_inuse
 * mach_pp->p_wanted
 * mach_pp->p_impl
 *
 *
 * The hat structs are locked with the per-hat hat_mutex.  This
 * lock must be held to change any resources that are allocated to
 * the hat.
 *
 *
 * hat_res_flock - lowest level mutex lock, protects hwpteage freelist
 * hat_res_lock - mutex lock that protects hme and hmespace freelist
 * hat_mmu_lock - protects the chain of all currently active hat's
 * hat_page_lock - Used as a second level lock when we need to either
 *		sleep/wakeup when operating on p_inuse bit. p_inuse
 *		bit is set/reset using atomic or/and, In case we need to
 *		sleep/wakeup we grab hat_page_lock
 * hat_dup_lock - is used to serialize parent/child exits.
 * hat->hat_mutex  - protects user hat structure.
 *
 *
 * For kernel address space we do not grab kas.a_hat->hat_mutex, since all
 * pagetables are allocated at startup time and the kernel hat does not
 * change in any fashion.
 * Accquire locks in the following order
 *
 * pp's mapping list lock
 * hat_dup_lock
 * hat->hat_mutex lock
 * hat_mmu_lock
 * hat_res_lock
 * hat_res_flock
 *
 *
 * hat_free() - Here we hold the hat_mutex, now we need to get a lock on
 *		pp's mapping list (for all valid mappings). On a 486 and above
 *		we dont drop the hat mutex unless we have unloaded all valid
 *		mappings. But in cases where we could not lock pp's mapping
 *		list we mark those hme's so that they would be dropped when
 *		the guy holding the mapping list lock drops it.
 */
static kmutex_t	hat_res_flock;
static kmutex_t	hat_res_lock;
static kmutex_t	hat_userpgtbl_lock;
static kmutex_t	hat_hat_lock;
static kmutex_t	hat_dup_lock;
extern kmutex_t	hat_page_lock;
kcondvar_t 	hat_cv;
kcondvar_t	hat_userpgtbl_cv;


/*
 * Public function and data referenced by this hat
 */
extern struct seg	*segkmap;
extern struct seg	*segkp;
extern caddr_t		econtig;
extern caddr_t		eecontig;
extern u_int		largepagesize;
extern u_int		largepageshift;

extern void hat_mlist_enter(struct page *);
extern void hat_mlist_exit(struct page *);
extern int hat_mlist_tryenter(struct page *);

/*
 * Private vm_hat data structures.
 */
static hwptepage_t *hwpp_freelist;	/* hardware pte page free list */
static hwptepage_t *khwpp_freelist;
static hwptepage_t *onlyhwpp_freelist;
static	struct i86hme *i86hme_freelist;
static hwptepage_t *hwpp_dupfreelist; /* duphardware pte page free list */
struct pte	   *dup_ptes;
static hwptepage_t	**hwpp_array; /* hwptepage pointers */

static int 		hat_pteunload(struct hment *, pte_t *, int, int);
static void 		hat_ptesync(struct hment *, pte_t *, int);
static pte_t 		*hat_ptealloc(struct hat *, pteval_t, caddr_t,
			    struct hment **, struct page *, int flags);
static u_int 		hat_update_pte(pte_t *, pteval_t, struct hat *,
			    caddr_t, u_int);
static struct hwptepage *hat_hwppalloc(int);
static struct hment	*hat_findhme(struct hwptepage *, u_int);
static int 		hat_hwppsteal(struct hat *);
static void 		hat_hwppfree(struct hat *, struct hwptepage *hwpp,
			    struct i86hme *i86hme, int);
static caddr_t 		hat_hmespace_alloc();
static int		hat_pagetablealloc();
static void 		hat_pagetablefree(struct hwptepage *,
			    struct hwptepage *);
static struct hwptepage *hat_hwppreserve();
static void		hat_hwppunload(struct hwptepage *, struct hat *);
static int		hat_hmesteal_hwppunload(struct hwptepage *,
			    struct hat *);
static void 		hat_update_pde();
static void		hat_hmesteal(int);
static int	hat_do_hmesteal;

/*
 * input values for hat_hwppalloc
 * DUP_PAGETABLE	allocate from dup freelist
 * REGULAR_PAGETABLE	allocate from the regular hwpp freelist
 */
#define	REGULAR_PAGETABLE	0
#define	DUP_PAGETABLE		1


/*
 * Semi-private data
 */
struct pte	*ptes, *eptes;
struct pte	*kptes, *keptes;
struct hment	*hments, *ehments;
struct hment	*khments, *kehments;
pte_t *KERNELmap; /* pointer to ptes mapping kernel */
pte_t *Sysmap1;	 /* pointer to ptes mapping from SYSBASE */
pte_t *Sysmap2;	 /* pointer to ptes mapping from Syslimit */

/*
 * kstat data
 */
struct vmhatstat vmhatstat;
kstat_named_t *vmhatstat_ptr = (kstat_named_t *)&vmhatstat;
ulong_t vmhatstat_ndata = sizeof (vmhatstat) / sizeof (kstat_named_t);





/*
 * Global data
 */
extern struct as kas;		/* kernel's address space */
u_int total_hwptepages = 0;	/* total no. of hwptepages allocated */
/*
 * The x86 hat layer has 3*maxuser pagetables, to be shared by all
 * process. When the pagetable freelist goes empty the hat layer begins
 * to steal these pagetables from other process. If a process keeps its
 * pagetable locked, the hat layer can not steal it. We have to have a
 * high water mark for the number of pagetables that can be locked
 * system wide. This is essential, else no process in the system can run
 * We limit the number of pagetables that can be locked system wide to
 * 30% of the total available pagetables.
 *
 */
int total_userpgtbl, total_resrvpgtbl = 0;
int total_locked_userpgtbl;
struct hwptepage *hwptepage_start, *hwptepage_end;


pte_t *kernel_only_pagedir;
#define	pte_readonly(x)	(!(*((pteptr_t)(x)) & 0x02))
#define	pte_readonly_and_notdirty(x)	(!(*((pteptr_t)(x)) & 0x42))
#define	ptevalue_dirty(a)		((a) & 0x40)

#ifdef	PTE36

/*
 * pde_nzmask[NCPU] is 64 bits wide per cpu. It holds the bit mask for 64
 * index locations in the pde_mask[NCPU][64] array. each of the 64 longs in
 * pde_mask[][64] is again a bitmask for 32 pagedirectory entries.
 */
u_int 	pde_mask[NCPU][64];
uint64_t pde_nzmask[NCPU];
#define	set_pde_nzmask(cp, a)	pde_nzmask[(cp)->cpu_id] |= \
				    (0x01LL << ((a) >> 5))
#define	set_pdemask(cp, a)	pde_mask[(cp)->cpu_id]\
					[(a) >> 5] |= \
					(0x01 << ((a) & 0x1f));
#define	CLEAR_PDE		user_pde64_clear
#define	LOAD_PAGEDIR(hat)	mmu_loadcr3(CPU, hat)
#define	CAS_PTE(a, b, c) 	cas64(((pteptr_t)a), b, c)
#define	BOOT_SIZE	(2 * FOURMB_PAGESIZE)
#define	PAGEDIR_SIZE	(MMU_PAGESIZE * NPDPERAS)
#define	LGPG_PDE	TWOMB_PDE
#define	LARGE_PAGE	two_mb_page
#define	ALIGN_TONEXT_LGPAGE(a) ((u_int)((a) + \
		TWOMB_PAGESIZE) & ~TWOMB_PAGEOFFSET);

#else

/*
 * pde_nzmask[NCPU] is an unsigned long per cpu. It holds the bit mask for 32
 * index locations in the pde_mask[NCPU][32] array. each of the 32 longs in
 * pde_mask[][32] is again a bitmask for 32 pagedirectory entries.
 */
u_int	pde_mask[NCPU][32];
u_int	pde_nzmask[NCPU];
#define	set_pde_nzmask(cp, a)	pde_nzmask[(cp)->cpu_id] |= \
					(0x01 << ((a) >> 5))
#define	set_pdemask(cp, a)	pde_mask[(cp)->cpu_id]\
					[(a) >> 5] |= \
					(0x01 << ((a) & 0x1f));

#define	CLEAR_PDE		user_pde_clear
#define	LOAD_PAGEDIR(hat)	if (hat->hat_pagedir) { \
					setcr3(hat->hat_pdepfn); \
				} else { \
					setcr3(CPU->cpu_cr3); \
				}
#define	CAS_PTE(a, b, c) 	cas32(((pteptr_t)a), b, c)
#define	BOOT_SIZE	(0)
#define	PAGEDIR_SIZE	(MMU_PAGESIZE)
#define	LGPG_PDE	FOURMB_PDE
#define	LARGE_PAGE	four_mb_page
#define	ALIGN_TONEXT_LGPAGE(a) ((u_int)((a) + \
		FOURMB_PAGESIZE) & ~FOURMB_PAGEOFFSET);

#endif	/* PTE36 */

/*
 * flags passed to hat_pteunload(), hat_pteunload() could be called
 * either with mach_pp->p_mapping list lock held/not held.
 */
#define	I86MMU_PPINUSE_NOTSET	0
#define	I86MMU_PPINUSE_SET	1

/*
 * 4 Mb virtual address space that some psm modules use for
 * the purpose of 1-1 mapping
 */
#define	PROM_SIZE	FOURMB_PAGESIZE
#define	KADB_SIZE	FOURMB_PAGESIZE


struct	hat	all_hat;

/*
 * hme's and pte's are not tied 1-1 since hme's are associated with hardware
 * pte. We need explicit pointers froms pte's to hme's and also from hme's
 * to pte's.
 * Each page can hold some hme pointers. When we have a valid pte which is
 * indexed above I86MMU_MAXPTES in a pagetable then we need to allocate space
 * to hold hme pointers.
 * For most applications we could hold the hme pointers in the pagetable.
 * Only when we cross 3Mb in a given segment(not the as->a_seg) do we
 * need to allocate a seperate hme space.
 */


/*
 * Page table
 *	------------------------
 *	|			|
 *	|hme pointers		|
 *	|---------------------	| I86MMU_MAXPTES	^
 *	|			|			|
 *	|			|			|
 *	|			|			|
 *	|valid ptes		|			|
 *	|			|
 *	|-----------------------|
 */



/*
 * number  of 2K (for NUMHME_ALLOCATE of 2) space to hold hme pointers when
 * we can not  hold them in the same pagetable
 */
#define	I86MMU_NUM_HMESPACE	4



static void hat_handle_dup_parentexit(struct hat *);


struct	hat *kernel_hat;

int	hat_protection[] = {MMU_STD_SRX, MMU_STD_SRX, MMU_STD_SRWX,
			MMU_STD_SRWX, MMU_STD_SRX, MMU_STD_SRX, MMU_STD_SRWX,
			MMU_STD_SRWX, MMU_STD_SRX, MMU_STD_SRXURX,
			MMU_STD_SRWXURWX, MMU_STD_SRWXURWX, MMU_STD_SRXURX,
			MMU_STD_SRXURX, MMU_STD_SRWXURWX, MMU_STD_SRWXURWX};


struct	hat	*hat_searchp = &all_hat;
/*
 * these two variables indicate that we are waiting for hme's| hmespace,
 * 'struct hwptepage' resource.
 */
int	hat_hmeres_wait = 0;
int	hat_hwppres_wait = 0;
/*
 * Total dynamically allocated memory by hat i86 layer. This will not exceed
 * 2.5% of the total physical memory
 */
uint64_t	hat_allocated_memory;
uint64_t	hat_max_memalloc;

u_int	kernel_only_cr3;
extern int	ncpus;
extern	void	atomic_dec();
extern	void	atomic_inc();
extern	void	atomic_incw();
extern	void	atomic_decw();
extern	int	atomic_dec_retzflg();
extern	int	atomic_decw_retzflg();


#define	I86MMUHME		1
#define	I86MMUHMESPACE		2
#define	I86MMUHWPP		4

#define	HAT_TO_I86MMUFLAGS(hme)	((hme) ? ((hme)->hme_nosync ? HAT_RMSTAT :\
					HAT_RMSYNC) : HAT_RMSTAT)


#define	HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr) 	\
				hwpp = hat->hat_hwpp;	\
				while (hwpp) {	\
					if (hwpp->hwpp_base == hwpp_addr) \
						break;	\
					hwpp = hwpp->hwpp_next;	\
				}

extern struct cpu cpus[];

/*
 * A macro used to access machpage fields
 */
#define	mach_pp	((machpage_t *)pp)

static int
hat_hmealloc(x)
struct i86hme **x;
{

	mutex_enter(&hat_res_lock);
	if (i86hme_freelist == 0) {
		mutex_exit(&hat_res_lock);
		return (1);
	}
	*(x) = i86hme_freelist;
	i86hme_freelist = (*(x))->i86hme_next;
	mutex_exit(&hat_res_lock);
	return (0);
}


static void
i86hme_alloc_more()
{
	struct i86hme *i86hme, *ei86hme, *i86hme_start;
	int	num_hments = 32;

	mutex_enter(&hat_res_lock);
	if (i86hme_freelist != (struct i86hme *)0) {
		mutex_exit(&hat_res_lock);
		return;
	}
	if (hat_allocated_memory > hat_max_memalloc) {
		hat_do_hmesteal = 1;
		mutex_exit(&hat_res_lock);
		return;
	}
	hat_allocated_memory += (num_hments * sizeof (struct i86hme));
	mutex_exit(&hat_res_lock);
	i86hme = (struct i86hme *)
	    kmem_zalloc(num_hments * sizeof (struct i86hme), KM_NOSLEEP);
	if (i86hme == NULL) {
		hat_do_hmesteal = 1;
		return;
	}
	i86hme_start = i86hme;
	ei86hme = i86hme + num_hments - 1;
	while (i86hme < ei86hme) {
		i86hme->i86hme_next = i86hme + 1;
		i86hme++;
	}
	mutex_enter(&hat_res_lock);
	i86hme->i86hme_next = i86hme_freelist;
	i86hme_freelist = i86hme_start;
	mutex_exit(&hat_res_lock);
}

/*
 * This routine converts virtual page protections to physical ones.
 */
static u_int
hat_vtop_prot(caddr_t addr, register u_int vprot)
{

#ifdef	lint
	addr = addr;
#endif
	ASSERT((vprot & PROT_ALL) == vprot);

	/* User cannot write kernel space */
	ASSERT(addr < (caddr_t)KERNELBASE ||
	    (vprot & (PROT_USER | PROT_WRITE)) != (PROT_USER | PROT_WRITE));

	switch (vprot) {
	case 0:
	case PROT_USER:
		/* Best we can do for these is kernel read-only */
	case PROT_READ:
	case PROT_EXEC:
	case PROT_READ | PROT_EXEC:
		return (MMU_STD_SRX);
	case PROT_WRITE:
	case PROT_WRITE | PROT_EXEC:
	case PROT_READ | PROT_WRITE:
	case PROT_READ | PROT_WRITE | PROT_EXEC:
		return (MMU_STD_SRWX);
	case PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_USER:
	case PROT_READ | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRXURX);
	case PROT_WRITE | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_USER:
	case PROT_WRITE | PROT_EXEC | PROT_USER:
	case PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER:
		return (MMU_STD_SRWXURWX);
	}
	/*NOTREACHED*/
}


/*
 * Construct a pte for a page.
 */
void
hat_mempte(register struct page *pp, u_int vprot,
register struct pte *pte, caddr_t addr)
{


	*(pteptr_t)pte = PTEOF_C(page_pptonum(pp),
	    hat_vtop_prot(addr, vprot));
}


/*
 * Enter a hme on the mapping list for page pp
 */
static void
hme_add(hme, pp)
	register struct hment *hme;
	register page_t *pp;
{
	ASSERT(mach_pp->p_inuse);

	hme->hme_prev = NULL;
	hme->hme_next = mach_pp->p_mapping;
	hme->hme_page = pp;
	if (mach_pp->p_mapping) {
		((struct hment *)mach_pp->p_mapping)->hme_prev = hme;
		ASSERT(mach_pp->p_share > 0);
	} else  {
		ASSERT(mach_pp->p_share == 0);
#ifdef	lint
		mach_pp->p_mapping = hme;
#endif
	}
	mach_pp->p_mapping = hme;
	mach_pp->p_share++;
}

/*
 * remove a hme from the mapping list for page pp
 */
static void
hme_sub(hme, pp)
	register struct hment *hme;
	register page_t *pp;
{
	ASSERT(mach_pp->p_inuse);
	ASSERT(hme->hme_page == pp);

	if (mach_pp->p_mapping == NULL)
		panic("hme_remove - no mappings");

	ASSERT(mach_pp->p_share > 0);
	mach_pp->p_share--;

	if (hme->hme_prev) {
		ASSERT(mach_pp->p_mapping != hme);
		ASSERT(hme->hme_prev->hme_page == pp);
		hme->hme_prev->hme_next = hme->hme_next;
	} else {
		ASSERT(mach_pp->p_mapping == hme);
		mach_pp->p_mapping = hme->hme_next;
		ASSERT((mach_pp->p_mapping == NULL) ?
		    (mach_pp->p_share == 0) : 1);
	}

	if (hme->hme_next) {
		ASSERT(hme->hme_next->hme_page == pp);
		hme->hme_next->hme_prev = hme->hme_prev;
	}

	/*
	 * zero out the entry
	 */
	hme->hme_next = NULL;
	hme->hme_prev = NULL;
	hme->hme_hat = NULL;
	hme->hme_page = (page_t *)NULL;
}

/*
 * get an 'hwpp' from the onlyhwpp_freelist.
 */
static struct hwptepage *
hat_hwppreserve()
{
	struct	hwptepage *newhwpp;

	mutex_enter(&hat_res_flock);
	if (onlyhwpp_freelist) {
		newhwpp = onlyhwpp_freelist;
		onlyhwpp_freelist = newhwpp->hwpp_next;
	} else newhwpp = NULL;
	mutex_exit(&hat_res_flock);
	return (newhwpp);
}


static struct	hwptepage *
hat_onlyhwppalloc(struct hat *hat, caddr_t addr)
{
	struct hwptepage *hwpp, *phwpp;

	if ((hwpp = hat_hwppreserve()) == NULL)
		hwpp = kmem_zalloc(sizeof (*hwpp), KM_SLEEP);
	hwpp->hwpp_pte = NULL;
	hwpp->hwpp_hat = hat;
	hwpp->hwpp_numptes = 0;
	hwpp->hwpp_lockcnt = 0;
	hwpp->hwpp_base = addrtohwppbase(addr);
	hwpp->hwpp_next = 0;
	hat->hat_numhwpp++;
	if ((phwpp = hat->hat_hwpp) != NULL) {
		while (phwpp->hwpp_next)
			phwpp = phwpp->hwpp_next;
		phwpp->hwpp_next = hwpp;
	} else {
		hat->hat_hwpp = hwpp;
	}
	return (hwpp);
}
/*ARGSUSED*/
static void
hat_allocate_sharedpgtbl(struct hat *hat, caddr_t addr)
{
	struct	hwptepage  *hwpp;
	pte_t	*pagetablep;

	pagetablep  = kmem_zalloc(PAGESIZE, KM_SLEEP);
	hat->hat_flags |= I86MMU_SPTAS;
	hwpp = hat_onlyhwppalloc(hat, addr);
	hwpp->hwpp_mapping = I86MMU4KLOCKMAP;
	hwpp->hwpp_pte = pagetablep;
	hwpp->hwpp_pfn = hat_getkpfnum((caddr_t)(hwpp->hwpp_pte));
	hwpp->hwpp_pde = PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWXURWX);
}


static void
hat_switch2perprocess_pagedir(hat)
struct hat *hat;
{
	struct		hwptepage  *hwpp;
	int		i;
	pteptr_t	pagedir;
	struct cpu 	*cpup;


	pagedir = (pteptr_t)kmem_alloc(PAGEDIR_SIZE, KM_SLEEP);
#ifdef	PTE36
	{
	caddr_t		addr;
	u_int 		pfn;

	bcopy((caddr_t)kernel_only_pagedir,
		(caddr_t)(pagedir + (NPTEPERPT * (NPDPERAS - 1))),
		MMU_PAGESIZE);
	hat->hat_pgdirpttblent = (pteptr_t)
		kmem_alloc(sizeof (pte_t) * NPDPERAS, KM_SLEEP);
	for (i = 0, addr = (caddr_t)pagedir; i < NPDPERAS;
		i++, addr += MMU_PAGESIZE) {
		pfn = hat_getkpfnum((caddr_t)(addr));
		hat->hat_pgdirpttblent[i] = PTBL_ENT(pfn);
	}
	}
#else

	bcopy((caddr_t)kernel_only_pagedir, (caddr_t)pagedir, MMU_PAGESIZE);
	hat->hat_pdepfn = (hat_getkpfnum((caddr_t)pagedir) <<
				MMU_STD_PAGESHIFT);
#endif
	hat->hat_flags &= ~I86MMU_UNUSUAL;
	/* load all entries in to our own pagedirectory */
	hwpp = hat->hat_hwpp;
	while (hwpp) {
		pagedir[hwpp->hwpp_base] = hwpp->hwpp_pde;
		hwpp = hwpp->hwpp_next;
	}

	kpreempt_disable();
	hat->hat_pagedir = pagedir;
	if (!hat->hat_cpusrunning ||
		(hat->hat_cpusrunning == CPU->cpu_mask)) {
		cpup = CPU;
		pagedir = (pteptr_t)cpup->cpu_pagedir;
		if (pde_nzmask[cpup->cpu_id]) {
			CLEAR_PDE(&pde_nzmask[cpup->cpu_id],
				pagedir, &pde_mask[cpup->cpu_id][0]);
		}
		pagedir[I86MMU_USER_TEXT] = MMU_STD_INVALIDPTE;
		pagedir[I86MMU_USER_SHLIB] = MMU_STD_INVALIDPTE;
	} else {
		/*
		 * This 'as' is currently active on couple of more
		 * cpu's. We have to clear per cpu page directory
		 * entries on all those cpu's.
		 */
		CAPTURE_CPUS(hat);
		for (i = 0; i < NCPU; i++) {
			if ((cpup = cpu[i]) == NULL)
				continue;
			if (cpup->cpu_current_hat == hat) {
				pagedir = (pteptr_t)cpup->cpu_pagedir;
				if (pde_nzmask[cpup->cpu_id]) {
					CLEAR_PDE(
						&pde_nzmask[cpup->cpu_id],
						pagedir,
						&pde_mask[cpup->cpu_id][0]);
				}
				pagedir[I86MMU_USER_TEXT] = MMU_STD_INVALIDPTE;
				pagedir[I86MMU_USER_SHLIB] = MMU_STD_INVALIDPTE;
			}
		}
		RELEASE_CPUS;
	}
	if (CPU->cpu_current_hat == hat)
		LOAD_PAGEDIR(hat);
	kpreempt_enable();
}
int	k_ramdsize;
/*
 * Initialize the hardware address translation structures.
 * Called by startup() after the vm structures have been allocated
 * and mapped in.
 */
static void
hat_init_internal(void)
{
	register hwptepage_t	*hwpp;
	register hwptepage_t	*ehwptepages, *eehwptepages;
	struct	i86hme	*i86hme, *ei86hme;
	struct	hment	*hme;
	uintptr_t a;
	caddr_t	addr;
	extern u_int va_to_pfn(u_int);
	uint_t	nhmespace, num_hments, kernel_pts, only_pts;
	struct hmespace *hmespacep;
	int	kernelsize, npts, our_maxuser;
	extern pgcnt_t physmem;

	mutex_init(&hat_hat_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_page_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_res_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_res_flock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_dup_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&hat_userpgtbl_lock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&hat_cv, NULL, CV_DEFAULT, NULL);
	cv_init(&hat_userpgtbl_cv, NULL, CV_DEFAULT, NULL);

	mutex_enter(&hat_res_lock);



	all_hat.hat_next = all_hat.hat_prev = &all_hat;
	hat_searchp = &all_hat;



	/*
	 * We want all our allocations to be based on maxuser, where maxuser
	 * is calculated as 1 user for every 1MB of physical memory.
	 * Since someone could change maxuser through /etc/system we dont want
	 * to be misled. We use our_maxuser.
	 */
	our_maxuser = ((uint64_t)physmem/1024) * 4;
	/*
	 * Maximium dynamic memory that the hat layer can allocate is 2.5%
	 * of the total available memory.
	 * If we need more memory we begin to steal
	 * from other hat's
	 */
	hat_max_memalloc = ((uint64_t)physmem * MMU_PAGESIZE)/40;
	/*
	 * we allocate  4 2k chunks to hold hmepointers
	 */
	if (hat_numhmespace == 0)
		hat_numhmespace = I86MMU_NUM_HMESPACE;
	hat_allocated_memory = hat_numhmespace * (HMESPACE_SIZE);
	if ((hmespacep  = (struct hmespace *)kmem_zalloc(hat_numhmespace *
		HMESPACE_SIZE, KM_NOSLEEP)) == NULL) {
		cmn_err(CE_PANIC, "hat_init: No memory for hmespace\n");
	}
	hat_hmespace_freelist = hmespacep;

	nhmespace = hat_numhmespace;
	a = (uintptr_t)hmespacep;
	while (--nhmespace) {
		hmespacep->next = (struct hmespace *)(a + HMESPACE_SIZE);
		a += HMESPACE_SIZE;
		hmespacep = (struct hmespace *)a;
	}
	hmespacep->next = (struct hmespace *)0;

	/*
	 * There could be three chunks of kernel virtual address space
	 * 1. KERNELBASE to eecontig:  includes segkp and segmap
	 * 2. SYSBASE to end of phys_syslimit
	 * 3. The last 4MB of the processor virtual address space that maps
	 * the prom
	 * In PTE36 mode,
	 * during the process of booting there is  a time frame when we
	 * allocate memory ourselves (so we need 64 bit pte's) and we
	 * require boot services for I/O. So, we need to map boot code
	 * (of size BOOT_SIZE) using 64 bit pte's.
	 */
	kernelsize = (u_int)eecontig - KERNELBASE +
		(phys_syslimit - SYSBASE) + k_ramdsize + PROM_SIZE + BOOT_SIZE;
	if (kadb_is_running)
		kernelsize += KADB_SIZE;
	kernel_pts = (kernelsize + PTSIZE - 1) / PTSIZE;
	kernel_pts++;
	if (hat_hwptepages == 0)  {
		npts = our_maxuser * 3;
		if (npts > MAX_HWPTEPAGES)
			npts = MAX_HWPTEPAGES -1;
		else if (npts < 60)
			npts = 60;
	} else npts = hat_hwptepages;

	total_userpgtbl = npts;
	/* We canot allow more than 30% of pagetables to be locked */
	if (total_resrvpgtbl == 0)
		total_resrvpgtbl = (total_userpgtbl/10) * 7;
	else if (total_resrvpgtbl == -1)
		total_resrvpgtbl = 0;

	/*
	 * Each i86hme entry holds two struct hment. In general each page
	 * needs two mapping entry, so we allocate physmem * 2 hme's
	 */
	num_hments = ((uint64_t)physmem * hat_maps_per_page)/NUMHME_ALLOCATE;


	/*
	 * It would be nice if we could allocate the number of dup pagetables
	 * based on number of cpus in the system. Its too complicated to delay
	 * allocation until hat_addhwptes() is called, so we base allocation
	 * on physmem. Assuming that on systems that have a lot of memory
	 * 1. its likely to be an mp machine
	 * 2. if its still not an mp machine, since we have enough memory
	 *	its OK if we waste a small amount of memory (64k of 64MB)
	 * The number of dup pagetables allocated is 4 per 16Mb limiting to
	 * a maximum of 16 pagetables
	 */
	if (hat_dup_npts == 0) {
		hat_dup_npts = our_maxuser/128;
		if (hat_dup_npts > 16)
			hat_dup_npts = 16;
		else if (hat_dup_npts == 0)
			hat_dup_npts = 2;
	}
	npts += hat_dup_npts;



	/* Kernel hments are dense and are allocated one-to-one with ptes */
	if ((khments = (struct hment *)
	    kmem_zalloc(kernel_pts * NPTEPERPT *  sizeof (struct hment),
	    KM_NOSLEEP)) == NULL) {
		mutex_exit(&hat_res_lock);
		cmn_err(CE_PANIC, "Cannot allocate memory for hment structs");
	}
	kehments = khments + (kernel_pts * NPTEPERPT);
	hme = khments;
	while (hme < kehments) {
		hme->hme_impl = HME_KERNEL;
		hme++;
	}




	if ((ptes = (pte_t *)kmem_zalloc((npts+kernel_pts) *
			MMU_PAGESIZE, KM_NOSLEEP)) == NULL) {
		mutex_exit(&hat_res_lock);
		cmn_err(CE_PANIC, "Cannot allocate memory for ptes");
	}
	kptes = ptes;
	keptes = kptes + (kernel_pts * NPTEPERPT);
	eptes = ptes + ((npts + kernel_pts) * NPTEPERPT);
	ASSERT(((u_int)ptes & PAGEOFFSET) == 0);
	ASSERT(((u_int)eptes & PAGEOFFSET) == 0);
	if ((hwpp_array =  (struct hwptepage **)kmem_zalloc((npts+kernel_pts)
			 * sizeof (*hwpp_array), KM_NOSLEEP)) == NULL) {
		mutex_exit(&hat_res_lock);
		cmn_err(CE_PANIC, "hat_init: hwpp_array");
	}

	/*
	 * The number of struct hwptepages is npts + only_pts  + kernel_pts.
	 * Out of  this only npts + kernel_pts have real 4K pagetable
	 * associated with them at any point of time.
	 * We have enough hwptepage structs, so
	 * that we dont steal the inexpensive struct hwptepage. 4Mb pages use
	 * only such struct hwptepage.
	 */

	only_pts = our_maxuser * hat_hwpps_per_user;
	if ((hwpp = (hwptepage_t *)
	    kmem_zalloc(sizeof (hwptepage_t) * (npts + only_pts + kernel_pts),
		KM_NOSLEEP)) == NULL) {
		mutex_exit(&hat_res_lock);
		cmn_err(CE_PANIC, "hat_init: no memory for hwpps");
	}
	eehwptepages = hwpp + (npts + only_pts) + kernel_pts;
	ehwptepages = hwpp + (kernel_pts);
	khwpp_freelist = NULL;
	for (addr = (caddr_t)kptes; hwpp < ehwptepages; hwpp++,
	    addr += PAGESIZE) {
		hwpp->hwpp_pte = (struct pte *)addr;
		hwpp->hwpp_pfn = va_to_pfn((u_int)(hwpp->hwpp_pte));
		hwpp->hwpp_next = khwpp_freelist;
		khwpp_freelist = hwpp;
		hwpp_array[total_hwptepages] = hwpp;
		total_hwptepages++;
	}
	ehwptepages = hwpp + (npts - hat_dup_npts);
	hwpp_freelist = NULL;

	hwptepage_start = hwpp;
	for (; hwpp < ehwptepages; hwpp++, addr += PAGESIZE) {
		hwpp->hwpp_pte = (struct pte *)addr;
		hwpp->hwpp_pfn = va_to_pfn((u_int)(hwpp->hwpp_pte));
		hwpp->hwpp_next = hwpp_freelist;
		hwpp_freelist = hwpp;
		hwpp_array[total_hwptepages] = hwpp;
		total_hwptepages++;
	}


	ehwptepages = hwpp + hat_dup_npts;
	hwpp_dupfreelist = NULL;

	for (dup_ptes = (struct pte *)addr; hwpp < ehwptepages; hwpp++,
	    addr += PAGESIZE) {
		hwpp->hwpp_pte = (struct pte *)addr;
		hwpp->hwpp_pfn = va_to_pfn((u_int)(hwpp->hwpp_pte));
		hwpp->hwpp_next = hwpp_dupfreelist;
		hwpp_dupfreelist = hwpp;
		hwpp_array[total_hwptepages] = hwpp;
		total_hwptepages++;
	}

	hwptepage_end = ehwptepages = eehwptepages;
	onlyhwpp_freelist = NULL;

	for (; hwpp < ehwptepages; hwpp++, addr += PAGESIZE) {
		hwpp->hwpp_pte = 0;
		hwpp->hwpp_pfn = 0;
		hwpp->hwpp_next = onlyhwpp_freelist;
		onlyhwpp_freelist = hwpp;
	}


	hat_allocated_memory += (num_hments * sizeof (struct i86hme));
	if ((i86hme = (struct i86hme *)
	    kmem_zalloc(num_hments * sizeof (struct i86hme),
	    KM_NOSLEEP)) == NULL) {
		mutex_exit(&hat_res_lock);
		cmn_err(CE_PANIC, "Cannot allocate memory for hment structs");
	}
	ei86hme = i86hme + num_hments - 1;
	i86hme_freelist = i86hme;
	while (i86hme < ei86hme) {
		i86hme->i86hme_next = i86hme + 1;
		i86hme++;
	}
	i86hme->i86hme_next = (struct i86hme *)0;

	mutex_exit(&hat_res_lock);
}

static u_int
hat_update_pte(pte, value, hat, addr, rmkeep)
pte_t *pte;
pteval_t value;
struct hat *hat;
caddr_t addr;
u_int rmkeep;
{
	register u_int		oldrm = *(pteptr_t)pte & PTE_RM_MASK;
	int 			newvalid = pte_valid((pte_t *)&value);
	int			freeing, caching_changed;
	int 			flush, operms, perm_changed;
	u_int			gen;
	pteval_t		prev_value;
	extern int 		pentiumpro_bug4064495;

	freeing = ((hat->hat_flags & (I86MMU_FREEING|I86MMU_SWAPOUT)) ==
			I86MMU_FREEING);

	ASSERT((rmkeep & ~PTE_RM_MASK) == 0);

	if (*(pteptr_t)pte == value)
		return (oldrm);
	else if (ncpus == 1) {
		flush  = pte_valid(pte) &&
			(addr >= (caddr_t)KERNELBASE || !freeing);
		value |= *(pteptr_t)pte & rmkeep;
		LOAD_PTE(pte, value);
		if (flush)
			mmu_tlbflush_entry(addr);
		return (oldrm);
	}



	/*
	 * We have to flush tlb when any of the following is true
	 * 1. valid bit is being made invalid.
	 * 2. protection bits change.
	 * 3. ref and/or mod bits change.
	 *
	 * If we are dealing with kernel address we need to flush tlb on
	 * cpu's. In the case of user address we have to flush tlb on all
	 * those cpu's where the page directory holds mappings for the
	 * current address space.
	 * We handle  kernel virtual address in the following way
	 * 1. When we make a pte invalid, we broadcast a tlb flush
	 *    message to all cpu's. We note the tlb_flush_gen number
	 *    in the pte placeholder contained in the pagetable.
	 *    tlb_flush_gen is incremented by a value
	 *    of 2 each time we broadcast a tlb flush message.
	 *    We do not wait for the tlbflush to complete on other cpu's.
	 *    We wait for tlbflush only when we load a valid pte.
	 *
	 * 2. When we reset mod bit we broadcast tlbflush message and wait for
	 * tlbflush before we return.
	 *
	 * In the case of user  address we wait for tlbflush to
	 * complete when
	 * 1. We make a pte invalid
	 * 2. We reset mod or change permission bits.
	 */

	operms = *(pteptr_t)pte & PTE_PERMMASK;


	if ((u_int)addr >= KERNELBASE) {
		if (!newvalid) {
			LOAD_PTE(pte, MMU_STD_INVALIDPTE);
			/*
			 * Other cpu's will not refrence this addr until
			 * the pte is valid again, hence we dont need an atomic
			 * update to pte. oldrm contains ref and mod bit.
			 */
			kpreempt_disable();
			TLBFLUSH_BRDCST(hat, addr, gen);
			mmu_tlbflush_entry(addr);
			kpreempt_enable();
			/* gen is an even unsigned number */
			LOAD_PTE(pte, gen);
		} else if (!pte_valid(pte)) {
			/*
			 * We are loading a pte, make sure that the previous
			 * tlbflush for this address is complete.
			 * generation number is a 32 bit quantity stored
			 * in the pte.
			 */
			TLBFLUSH_WAIT(*(u_int *)pte);
			LOAD_PTE(pte, value);
		} else {
			/*
			 * We need an atomic update here. If the pte was
			 * not dirty and we were only changing the ref bit
			 * then we dont wait for the tlbflush to complete.
			 */
			do {
				prev_value = *(pteptr_t)pte;
				value |= prev_value & rmkeep;
			} while (CAS_PTE(pte, prev_value, value) != prev_value);
			kpreempt_disable();
			TLBFLUSH_BRDCST(hat, addr, gen);
			mmu_tlbflush_entry(addr);
			kpreempt_enable();
			oldrm = prev_value & PTE_RM_MASK;
			if (ptevalue_dirty(prev_value))
				TLBFLUSH_WAIT(gen);
		}

	} else {
		kpreempt_disable();

		if (!hat->hat_cpusrunning ||
			(hat->hat_cpusrunning == CPU->cpu_mask)) {
			/*
			 * We are either not running on any cpu or we are
			 * executing only on this cpu. We dont need an
			 * atomic update to pte.
			 */
			prev_value = *(pteptr_t)pte;
			LOAD_PTE(pte, (value|(prev_value & rmkeep)));
			oldrm = prev_value & PTE_RM_MASK;
			mmu_tlbflush_entry(addr);
		} else {
			/*
			 * Check for the conditions to apply the Pentium Pro bug
			 * workaround.
			 */
			if (pentiumpro_bug4064495 &&
			    ((operms & PTE_PERMMASK) == PG_UW) && (!newvalid ||
					((value & PTE_PERMMASK) == PG_UR))) {

				/*
				 * Apply the workaround.
				 */

				CAPTURE_CPUS(hat);
				do {
					prev_value = *(pteptr_t)pte;
					value |= prev_value & rmkeep;
				} while (CAS_PTE(pte, prev_value, value)
					!= prev_value);
				RELEASE_CPUS;
				mmu_tlbflush_entry(addr);
				oldrm |= prev_value & PTE_RM_MASK;
			} else {
				do {
					prev_value = *(pteptr_t)pte;
					value |= prev_value & rmkeep;
				} while (CAS_PTE(pte, prev_value, value) !=
								prev_value);
				TLBFLUSH_BRDCST(hat, addr, gen);
				mmu_tlbflush_entry(addr);
				oldrm = prev_value & PTE_RM_MASK;
				caching_changed =
				    ((prev_value & PTE_NONCACHEABLE(1)) !=
					(value & PTE_NONCACHEABLE(1)));
				perm_changed = ((operms & value) != operms);
				if (!newvalid || ptevalue_dirty(prev_value) ||
					caching_changed || perm_changed)
					TLBFLUSH_WAIT(gen);
			}
		}

		kpreempt_enable();
	}

	return (oldrm);

}


/*
 * Returns a pointer to the pte struct for the given virtual address.
 * If the necessary page tables do not exist, return NULL.
 */
static pte_t *
hat_ptefind(struct hat *hat, register caddr_t addr)
{
	register pte_t *pte;
	int	hwpp_addr;
	struct hwptepage *hwpp;
	caddr_t base;


	if (addr >= (caddr_t)KERNELBASE) {
		if (addr >= Syslimit) {
			if (kadb_is_running)
				base = (caddr_t)DEBUGSTART;
			else
				base = (caddr_t)PROMSTART;
			pte = &Sysmap2[mmu_btop(addr - base)];
		} else if (addr >= Sysbase)
			pte = &Sysmap1[mmu_btop(addr - Sysbase)];
		else
			pte = (addr > eecontig ? NULL :
		    &KERNELmap[mmu_btop(addr - (caddr_t)KERNELBASE)]);
		return (pte);
	}


	hwpp_addr = addrtohwppbase(addr);
	HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);
	return ((hwpp && hwpp->hwpp_pte) ?
		(&hwpp->hwpp_pte[PAGETABLE_INDEX((u_int)addr)]) : NULL);
}

/*
 * Project private interface between the kernel and kadb to implement kadb's
 * "@" command to display/modify memory given physical addresses.
 *
 * Map "vaddr" (a kadb virtual address) to the physical page whose 64-bit
 * address is contained in new, saving the old 64-bit address in *old.
 * Return "0" (indicating an error) if the virtual address is not part of the
 * kernel debugger.
 */


int
kadb_map(caddr_t vaddr, uint64_t new, uint64_t *old)
{
	register pte_t *pte;
	pteval_t	value;

#ifdef	lint
	new = new;
#endif
	if (vaddr < (caddr_t)KERNELBASE)
		return (0);
	pte = hat_ptefind(kernel_hat, vaddr);
	if (old)
		*old = *(pteptr_t)pte & ~MMU_STD_PAGEMASK;

	value = PTEOF_C((new >> MMU_STD_PAGESHIFT), MMU_STD_SRWX);
	LOAD_PTE(pte, value);
	mmu_tlbflush_entry(vaddr);
	return (1);
}

/*
 * Loads the pte for the given address with the given pte. Also sets it
 * up as a mapping for page pp, if there is one.
 */
static void
hat_pteload(struct hat *hat, caddr_t addr, struct page *pp,
pte_t *pte, int flags)
{
	register pte_t  *a_pte;
	struct hment *hme;
	int remap = 0;
	struct	hwptepage *hwpp;
	int	true = 1;



	/*
	 * If it's a kernel address, the page table should already exist.
	 * Otherwise we may have to create one.
	 */
	if (addr >= (caddr_t)KERNELBASE) {
		a_pte = hat_ptefind(hat, addr);
		/* Lot of segmap_faults return this way */
		if (*(pteptr_t)pte == (*(pteptr_t)a_pte & ~PTE_RM_MASK))
			return;
		hme = kptetohme(a_pte);
	} else {
		if (((flags & (HAT_LOAD_LOCK|HAT_LOAD_SHARE))
		    == HAT_LOAD_LOCK) && total_resrvpgtbl) {
			struct hwptepage *hwpp;
			u_int	hwpp_addr;

			hwpp_addr = addrtohwppbase(addr);

			do {
				HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);
				if (hwpp && hwpp->hwpp_lockcnt)
					break;
				mutex_enter(&hat_userpgtbl_lock);
				if (total_locked_userpgtbl <
					total_userpgtbl - total_resrvpgtbl) {
					total_locked_userpgtbl++;
					mutex_exit(&hat_userpgtbl_lock);
					break;
				}
				mutex_exit(&hat->hat_mutex);
				if (pp) hat_mlist_exit(pp);
				while (total_locked_userpgtbl >=
					total_userpgtbl - total_resrvpgtbl) {
					cv_wait(&hat_userpgtbl_cv,
						&hat_userpgtbl_lock);
				}
				mutex_exit(&hat_userpgtbl_lock);
				if (pp) hat_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
			} while (true);
		}

		/* hat_ptealloc will return null for a 4Mb page */
		if ((a_pte = hat_ptealloc(hat, *(pteptr_t)pte, addr, &hme,
				pp, flags)) == NULL)
			return;
		else if (hme == NULL) {
			(void) hat_update_pte(a_pte, *(pteptr_t)pte, hat,
					addr, PTE_RM_MASK);
			return;
		}
	}
	hwpp = ptetohwpp(a_pte);




	remap = pte_valid(a_pte);
	if (pp != NULL) {
		pte->NonCacheable = (PP_ISNC(pp));
		if (!mach_pp->p_mapping && pte_ronly(pte)) {
			PP_SETRO(pp);
		} else if (!pte_ronly(pte) && PP_ISRO(pp)) {
			PP_CLRRO(pp);
		}
		if (remap && (MAKE_PFNUM(a_pte) != MAKE_PFNUM(pte))) {
			mutex_exit(&hat->hat_mutex);
			cmn_err(CE_PANIC, "hat_pteload: remap page");
		}
	}



	if (remap) {
		*(pteptr_t)pte |= *(pteptr_t)a_pte & PTE_RM_MASK;
		/*
		 * For a child process this could be a pte inherited from
		 * its parent.
		 * segvn layer could load this translation, due to a fault
		 * some where below this address, in which case we need to
		 * add this entry on pp->mapping list
		 */
		if (hme->hme_valid == 0) {
			remap = 0;
			atomic_incw((short *)&hwpp->hwpp_numptes);
			hat->hat_rss++;
		}
	} else {
		atomic_incw((short *)&hwpp->hwpp_numptes);
		hat->hat_rss++;
	}

	(void) hat_update_pte(a_pte, *(pteptr_t)pte, hat, addr,
		PTE_RM_MASK);

	hme->hme_valid = pte_valid(a_pte);

	/*
	 * If we need to lock, increment the lock count.
	 */
	if (flags & HAT_LOAD_LOCK)
		hwpp->hwpp_lockcnt++;

	if (pp != NULL && !remap) {
		/*
		 * We are loading a new translation. Add the
		 * translation to the list for this page, and
		 * increment the rss.
		 */
		ASSERT(hme->hme_next != hme);
		hme->hme_hat = hat - hats;
		hme_add(hme, pp);
	}

	/*
	 * Note whether this translation will be ghostunloaded or not.
	 */
	hme->hme_nosync = getpte_nosync(pte);
}

/*
 * Returns a pointer to the pte struct for the given virtual address.
 * If the necessary page tables do not exist, return NULL.
 */
static pte_t *
hat_ptealloc(struct hat *hat, pteval_t pte_value, register caddr_t addr,
struct hment **hmep, struct page *pp, int flags)
{
	pte_t *pte, pte1;
	register struct cpu *cp;
	int	hwpp_addr;
	u_int   *hme_start, pfn = PTE_PFN(pte_value);
	pteptr_t pagedir;
	struct	hwptepage *hwpp, *phwpp;
	struct	i86hme *i86hme;
	struct	hmespace *hmespacep;

try_again:
	hwpp_addr = addrtohwppbase(addr);


	phwpp = hwpp = hat->hat_hwpp;
	while (hwpp) {
		if (hwpp->hwpp_base == hwpp_addr)
			break;
		phwpp = hwpp;
		hwpp = hwpp->hwpp_next;
	}
	if (hwpp == 0) {
		if ((flags & (HAT_LOAD_LOCK|HAT_LOAD_SHARE)) ==
			(HAT_LOAD_LOCK|HAT_LOAD_SHARE)) {

			hat_allocate_sharedpgtbl(hat, addr);
			HWPP_FROM_I86MMU(hat, hwpp, addrtohwppbase(addr));
			*hmep = NULL;
			pte = &hwpp->hwpp_pte[PAGETABLE_INDEX((u_int)addr)];
			hwpp->hwpp_numptes = 1;
			hat->hat_rss++;
			goto load_pagedirectory;
		}
		while ((hwpp = hat_hwppalloc(REGULAR_PAGETABLE)) == 0) {
			if (hat_hwppsteal(hat) == 0) {
				mutex_exit(&hat->hat_mutex);
				if (pp) hat_mlist_exit(pp);
				hat_hmesteal(I86MMUHWPP);
				if (pp) hat_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
				goto	try_again;
			}
		}
		hwpp->hwpp_hat = hat;
		hwpp->hwpp_numptes = 0;
		hwpp->hwpp_lockcnt = 0;
		hwpp->hwpp_base = addrtohwppbase(addr);
		hat->hat_numhwpp++;
		if ((hat->hat_numhwpp > 2) && !(hat->hat_pagedir))
			hat->hat_flags |= I86MMU_UNUSUAL;
		if (phwpp) {
			phwpp->hwpp_next = hwpp;
		} else {
			hat->hat_hwpp = hwpp;
		}
		hat->hat_flags &= ~I86MMU_PTEPAGE_STOLEN;
		hwpp->hwpp_pde =
		    PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWXURWX);

	} else if (hwpp->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
		pfn &= ~(NPTEPERPT - 1);
		LOAD_PTE(&pte1, pte_value);
		hwpp->hwpp_pde = LGPG_PDE(pfn, 0, pte1.AccessPermissions, 1);
		if (getpte_nosync(&pte1))
			setpte_nosync(&hwpp->hwpp_pde);
		pte = 0;
		goto load_pagedirectory;
	} else if (hwpp->hwpp_pte == 0) {
		/*
		 * We have to attach a 4K pagtable to this hwpp.
		 * hat_pagetablealloc() returns 0 when it finds that
		 * hwpp_freelist is empty. Then we need to steal
		 * 4K pagetable, this is what hat_hwppsteal() does
		 * hat_hwppsteal() returns 0 when it finds that
		 * onlyhwpp_freelist is empty, now we need to steal
		 * all hat resources through hat_hmesteal()
		 */
		while (hat_pagetablealloc(hwpp) == 0) {
			if (hat_hwppsteal(hat) == 0) {
				mutex_exit(&hat->hat_mutex);
				if (pp) hat_mlist_exit(pp);
				hat_hmesteal(I86MMUHWPP);
				if (pp) hat_mlist_enter(pp);
				mutex_enter(&hat->hat_mutex);
				goto	try_again;
			}
		}
		hat->hat_flags &= ~I86MMU_PTEPAGE_STOLEN;
		hwpp->hwpp_pde = PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWXURWX);
	} else if (hwpp->hwpp_mapping == I86MMU4KLOCKMAP) {
		*hmep = NULL;
		pte = &hwpp->hwpp_pte[PAGETABLE_INDEX((u_int)addr)];
		if (!pte_valid(pte)) {
			atomic_incw((short *)&hwpp->hwpp_numptes);
			hat->hat_rss++;
		}
		goto load_pagedirectory;
	}
	pte = &hwpp->hwpp_pte[PAGETABLE_INDEX((u_int)addr)];
	if ((hme_start = hwpp->hwpp_hme) == 0) {
		hme_start = hwpp->hwpp_hme =
			(u_int *)(hwpp->hwpp_pte + I86MMU_MAXPTES);
	}
	if ((PAGETABLE_INDEX((u_int)addr) > I86MMU_MAXPTES - 1) &&
	(((u_int)hwpp->hwpp_hme & ~(MMU_PAGESIZE - 1)) ==
	(u_int)hwpp->hwpp_pte)) {
		if ((hmespacep = (struct hmespace *)hat_hmespace_alloc())
			== NULL) {
			mutex_exit(&hat->hat_mutex);
			if (pp) hat_mlist_exit(pp);
			hat_hmesteal(I86MMUHMESPACE);
			if (pp) hat_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			goto	try_again;
		}
		bcopy((caddr_t)hme_start, (caddr_t)hmespacep,
			(NPTEPERPT - I86MMU_MAXPTES)*sizeof (u_int));
		bzero((caddr_t)hme_start,
			(NPTEPERPT - I86MMU_MAXPTES) * sizeof (u_int));
		hme_start = hwpp->hwpp_hme = (u_int *)hmespacep;
	}

	i86hme = (struct i86hme *)(*(hme_start + HMETABLE_INDEX(addr)));
	if (i86hme == 0) {
		if (hat_hmealloc(&i86hme)) {
			mutex_exit(&hat->hat_mutex);
			if (pp) hat_mlist_exit(pp);
			if (hat_do_hmesteal)
				hat_hmesteal(I86MMUHME);
			else
				i86hme_alloc_more();
			if (pp) hat_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			goto	try_again;
		}
		i86hme->i86hme_pte = PTE_TO_I86HMEPTE(pte);
		i86hme->i86hme_next = hwpp->hwpp_firsthme;
		hwpp->hwpp_firsthme = i86hme;
		I86HME_TO_FIRSTHME(i86hme)->hme_impl = HME_FIRST;
		I86HME_TO_SECONDHME(i86hme)->hme_impl = HME_SECOND;
		*(hme_start + HMETABLE_INDEX(addr)) = (u_int)i86hme;
		if (hwpp->hwpp_lasthme == 0)
			hwpp->hwpp_lasthme = i86hme;
	}
	*hmep = I86HME_TO_HME(i86hme, addr);


load_pagedirectory:
	/* procfs needs this, see prusrio() */
	if (hat->hat_as == curthread->t_procp->p_as) {
		kpreempt_disable();
		cp = CPU;
		if (I86MMUNUSUAL(hwpp_addr) && !(hat->hat_pagedir)) {
			hat->hat_flags |= I86MMU_UNUSUAL;
			set_pde_nzmask(cp, hwpp_addr);
			set_pdemask(cp, hwpp_addr);
		}
		if (cp->cpu_current_hat != hat)  {
			(void) hat_setup(hat, HAT_ALLOC);
		} else {
			if ((pagedir = hat->hat_pagedir) == NULL)
				pagedir = (pteptr_t)cp->cpu_pagedir;
			LOAD_PTE(&pagedir[hwpp->hwpp_base], hwpp->hwpp_pde);
			LOAD_PAGEDIR(hat);
		}

		kpreempt_enable();
	}
	return (pte);
}



/*
 * Sync the referenced and modified bits of the page struct with the pte.
 * Clears the bits in the pte.  Also, synchronizes the Cacheable bit in
 * the pte with the noncacheable bit in the page struct.
 *
 * Any change to the PTE requires the TLBs to be flushed, so subsequent
 * accesses or modifies will cause the memory image of the PTE to be
 * modified.
 */
static void
hat_ptesync(hme, pte, flags)
register struct hment *hme;
register struct pte *pte;
int flags;
{
	struct	page		*pp = hme->hme_page;
	struct	hwptepage 	*hwpp;
	struct hat 		*hat;
	caddr_t 		vaddr;
	pteval_t		oldpte;
	struct pte 		pte1;
	int 			rm, lrm, skip_hat_update_pte;


	/*
	 * Get the ref/mod bits from the hardware page table,
	 */
	skip_hat_update_pte = 0;
	if (flags & HAT_INVSYNC)
		oldpte = MMU_STD_INVALIDPTE;
	else {
		oldpte = *(pteptr_t)pte;
		if (flags & HAT_RMSYNC)
			oldpte &= ~PTE_RM_MASK;
		else if (flags & HAT_NCSYNC) {
			pte1 = *pte;
			pte1.NonCacheable = PP_ISNC(pp);
			oldpte = *(pteptr_t)&pte1;
		} else {

			/*
			 * we are not changing the ref or mod bit so dont
			 * update just pick those bits from hwpte
			 */
			rm = *(pteptr_t)pte & PTE_RM_MASK;
			/* Most of fsflush sync's should return this way */
			if (rm == 0)
				return;
			skip_hat_update_pte = 1;
		}
	}
	hwpp = ptetohwpp(pte);
	hat = hwpp->hwpp_hat;
	vaddr = hwppbasetoaddr(hwpp->hwpp_base)
	    + (pte - hwpp->hwpp_pte) * MMU_PAGESIZE;


	/*
	 * Get the hardware ref/mod bits, combine them with the software
	 * bits, and use the combination as the current ref/mod bits.
	 */
	if (!skip_hat_update_pte)
		rm = hat_update_pte(pte, oldpte, hat, vaddr, 0);


	if (pp != NULL) {
		if ((flags & (HAT_RMSYNC|HAT_RMSTAT)) && rm) {
			if (rm & PTE_REF_MASK)
				lrm = P_REF;
			else lrm = 0;
			if (rm & PTE_MOD_MASK)
				lrm |= P_MOD;
			if (flags & HAT_RMSYNC && hat->hat_stat)
				hat_setstat(hat->hat_as, vaddr, PAGESIZE, lrm);
			if (!hme->hme_nosync)
				atomic_orb(&((machpage_t *)pp)->p_nrm, lrm);
		}
	}

}

/*
 * Unload the pte. If required, sync the referenced & modified bits.
 * If it's the last pte in the page table, and the table isn't locked,
 * free it up.
 */
static int
hat_pteunload(hme, pte, mode, mlistlock_held)
struct	hment	*hme;
register pte_t *pte;
int	mode;
{
	register caddr_t vaddr;
	struct page *pp;
	struct	hwptepage *hwpp;
	struct hat *hat;
	int	ret = 0;
	extern void set_p_hmeunload();
	struct	i86hme	*i86hme;
	int	freeing;
	u_int	gen;




	/*
	 * Here the pagetable has been stolen, so only remove hme from
	 * the mach_pp->p_mapping list
	 */
	if (pte == 0) {
		if ((pp = hme->hme_page) != NULL) {
			ASSERT(mlistlock_held);
			hme_sub(hme, pp);
		}
		hme->hme_valid = 0;
		hme->hme_nosync = 0;
		return (0);
	}

	hwpp = ptetohwpp(pte);
	vaddr = hwppbasetoaddr(hwpp->hwpp_base);
	hat = hwpp->hwpp_hat;
	if (!pte_valid(pte)) {
		if (vaddr < (caddr_t)KERNELBASE) {
			ASSERT(((hme->hme_valid == 0) && hme->hme_page &&
				mlistlock_held));
			/*
			 * This hme is not valid.
			 * We have marked this hme to be dropped on
			 * mlist_exit(), but we are being called from
			 * hat_pageunload(). This guy wont give up unless we
			 * remove this hme from pp's mapping list. Do all that
			 * we would have done on mlist_exit() right now
			 */
			hme_sub(hme, hme->hme_page);
			hat->hat_rss--;
			if (atomic_decw_retzflg(&hwpp->hwpp_numptes)) {
				hat_hwppfree(hat,
				hwpp, hwpp->hwpp_firsthme, 0);
				if (hat->hat_numhwpp == 0)
					cv_signal(&hat->hat_cv);
			}
		}
		return (0);
	}

	if ((hme == 0) || (hme->hme_valid == 0)) {
		/*
		 * This entry belongs to a child who was duped
		 * We could have a non null hme since we allocate two hme's
		 * on every call to hat_hmealloc()
		 */
		vaddr += ((pte - hwpp->hwpp_pte) * MMU_PAGESIZE);
		(void) hat_update_pte(pte, MMU_STD_INVALIDPTE, hat, vaddr, 0);
		return (0);
	}




	pp = hme->hme_page;

	freeing = ((hat->hat_flags & (I86MMU_FREEING|I86MMU_SWAPOUT)) ==
			I86MMU_FREEING);
	/* Invalidate pte */
	if (freeing) {
		/* We dont have to sync up MOD bits, just clear the entry */
		if (pte_readonly_and_notdirty(pte)) {
			LOAD_PTE(pte, MMU_STD_INVALIDPTE);
			mmu_tlbflush_entry(vaddr);
		}
		else
			hat_ptesync(hme, pte, HAT_INVSYNC|mode);
	} else if (pte_readonly_and_notdirty(pte)) {
		vaddr += ((pte - hwpp->hwpp_pte) * MMU_PAGESIZE);
		if (hat->hat_cpusrunning != CPU->cpu_mask) {
			LOAD_PTE(pte, MMU_STD_INVALIDPTE);
			kpreempt_disable();
			TLBFLUSH_BRDCST(hat, vaddr, gen);
			mmu_tlbflush_entry(vaddr);
			kpreempt_enable();
			TLBFLUSH_WAIT(gen);
		} else {
			LOAD_PTE(pte, MMU_STD_INVALIDPTE);
			mmu_tlbflush_entry(vaddr);
		}
	} else hat_ptesync(hme, pte, HAT_INVSYNC|mode);

	/*
	 * Remove the pte from the list of mappings for the page.
	 */
	hme->hme_valid = 0;
	if (pp != NULL) {
		ASSERT(hme->hme_next != hme);
		if (mlistlock_held) {
			hme_sub(hme, pp);
		} else if (hat_mlist_tryenter(pp))  {
				if (hme->hme_page) {
					mlistlock_held = 1;
					hme_sub(hme, pp);
				}
				hat_mlist_exit(pp);
		} else {
			set_p_hmeunload(pp);
			if (hat_mlist_tryenter(pp)) {
				if (hme->hme_page) {
					mlistlock_held = 1;
					hme_sub(hme, pp);
				}
				hat_mlist_exit(pp);
			}
		}
	} else mlistlock_held = 1;

	hme->hme_nosync = 0;


	/*
	 * if we dont have a hardware pte, ie if the pagetable was stolen
	 * we dont know what hwpp is, hat_free() will handle this
	 * case
	 */
	if (vaddr < (caddr_t)KERNELBASE) {
		if (mlistlock_held) {
			hat->hat_rss--;
			if (atomic_decw_retzflg(&hwpp->hwpp_numptes)) {
				i86hme = HMETOI86HME(hme);
				hat_hwppfree(hat, hwpp,
				i86hme->i86hme_next, 0);
				ret = 1;
			}
		}
	}

	return (ret);
}

/*
 * Unload a hardware translation that maps page `pp'.
 */
static void
hat_pageunload_hme(struct page *pp, struct hat *hat, struct hment *hme)
{
	struct pte *pte;
	struct	hat *hat_c;
	struct	hwptepage *hwpp;
	caddr_t	addr;
	u_int	*hme_start;
	struct i86hme *i86hme;

#ifdef	lint
	pp = pp;
#endif
	ASSERT(pp == NULL || mach_pp->p_inuse);

	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);

	/*
	 * If there is no hardware pte just mark the hme as invalid,
	 */
	if (hme->hme_impl & HME_PTEUNLOADED) {
		hme->hme_valid = 0;
		hme->hme_nosync = 0;
		hme_sub(hme, hme->hme_page);
		hme->hme_impl |= HME_PAGEUNLOADED;
		i86hme = HMETOI86HME(hme);
		addr = hwppbasetoaddr(i86hme->i86hme_hwppbase) + (MMU_PAGESIZE *
		    (i86hme->i86hme_pteindex + HMEPOSITION(hme)));
	} else {
		pte = hmetopte(hme);
		(void) hat_pteunload(hme, pte, HAT_RMSYNC, I86MMU_PPINUSE_SET);
		hwpp = ptetohwpp(pte);
		addr = hwppbasetoaddr(hwpp->hwpp_base) +
		    ((pte - hwpp->hwpp_pte) * MMU_PAGESIZE);
	}


	if (!(hat->hat_flags & I86MMU_DUP_PARENT)) {
		if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
		return;
	}

	/*
	 * We need to invalidate the pte that maps this page in the child.
	 * hat_steal() does not steal 4K pte pages from those process
	 * who have a child sharing the ptes
	 */
	if ((hat_c = hat->hat_dup) == NULL) {
		mutex_exit(&hat->hat_mutex);
		return;
	}
	mutex_enter(&hat_c->hat_mutex);
	mutex_exit(&hat->hat_mutex);
	if ((pte = hat_ptefind(hat_c, addr)) == 0) {
		mutex_exit(&hat_c->hat_mutex);
		return;
	}
	hwpp = ptetohwpp(pte);
	if ((hme_start = (u_int *)hwpp->hwpp_hme) != NULL) {
		i86hme = (struct i86hme *)(*(hme_start + HMETABLE_INDEX(addr)));
		if (i86hme) {
			hme = I86HME_TO_HME(i86hme, addr);
		} else {
			hme = (struct hment *)0;
		}
	} else {
		hme = (struct hment *)0;
	}
	if (pte_valid(pte) && ((!hme) || (hme->hme_valid == 0)))
		(void) hat_pteunload(hme, pte, HAT_RMSTAT, I86MMU_PPINUSE_SET);
	mutex_exit(&hat_c->hat_mutex);
}

/*
 * Get all the hardware dependent attributes for a page struct
 */
static void
hat_pagesync_pp(struct hat *hat, struct page *pp, struct hment *hme,
u_int clearflag)
{
	struct	pte *pte;

#ifdef	lint
	pp = pp;
#endif

	ASSERT(pp == NULL || mach_pp->p_inuse);
	ASSERT(PAGE_LOCKED(pp) || panicstr);


	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);

	/* there is no hardware pte, it has been synced. so return */
	if (hme->hme_impl & HME_PTEUNLOADED) {
		mutex_exit(&hat->hat_mutex);
		return;
	}

	pte = hmetopte(hme);
	hat_ptesync(hme, pte,
	    clearflag ? HAT_RMSYNC : HAT_RMSTAT);
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
}




/*
 * this function get called at strtup time. we dont have to disable
 * kernel preemption when we grab spin lock
 */
struct hwptepage *
hat_khwppalloc(void)
{
	register hwptepage_t *hwpp;

	mutex_enter(&hat_res_flock);

	if ((hwpp = khwpp_freelist) != 0) {
		khwpp_freelist = hwpp->hwpp_next;
	} else {

		mutex_exit(&hat_res_flock);
		cmn_err(CE_PANIC, "hat_kernelhwppalloc: freelist empty\n");
	}

	mutex_exit(&hat_res_flock);
	hwpp->hwpp_next = 0;
	hwpp->hwpp_lockcnt = 0;
	return (hwpp);
}
/*
 * Allocates hardware pte page
 */
static struct hwptepage *
hat_hwppalloc(dup)
int	dup;
{
	register hwptepage_t *hwpp, **freehwpp;

	mutex_enter(&hat_res_flock);

	if (dup)
		freehwpp = &hwpp_dupfreelist;
	else
		freehwpp = &hwpp_freelist;
	if ((hwpp = *freehwpp) == 0) {
		mutex_exit(&hat_res_flock);
		return (0);
	}
	*freehwpp = hwpp->hwpp_next;
	mutex_exit(&hat_res_flock);
	hwpp->hwpp_next = 0;
	hwpp->hwpp_lockcnt = 0;
	return (hwpp);
}

/*
 * steal 4K pte page, We do not drop hat lock once we accquire it.
 * This is the first level in stealing. Here we steal a 4K pte page from
 * a process and attach the 4K pte page to an hwpp pulled off the
 * onlyhwpp_freelist, and put the new hwpp-4K pte page pair on to the
 * hwpp_freelist.
 * If the onlyhwpp_freelist is empty the we return failure. The higher
 * function would then invoke hat_hmesteal().
 */

static int
hat_hwppsteal(my_hat)
struct hat *my_hat;
{
	register hwptepage_t *hwpp;
	struct	hat	*hat;
	hwptepage_t *nexthwpp, *onlyhwpp;
	pte_t *pte, *start_pte;
	struct hment	*hme;
	int	rm;
	u_int	*start_hme;
	pteptr_t pagedir;
	struct	i86hme *i86hme;
	int	ret = 1, clear_pde = 0, update_pde_first;


	/*
	 *	Find an hwpp to steal.
	 */

	mutex_enter(&hat_hat_lock);
	hat = hat_searchp;
	do {
		if (hat == &all_hat)
			continue;
		hat = &hats[hat - hats];
		if (mutex_tryenter(&hat->hat_mutex) == 0) {
			continue;
		}
		if ((hat->hat_flags & I86MMU_HWPPDONTSTEAL) ||
			(hat->hat_cpusrunning != 0)) {
			mutex_exit(&hat->hat_mutex);
			continue;
		} else  {
			hat_searchp = hat->hat_next;
			break;
		}
	} while ((hat = hat->hat_next) != hat_searchp);
	/*
	 * We have failed to find a process that we could steal 4K pte page
	 * from. We hold the hat mutex of the process trying to steal a
	 * 4K pte page. Lets steal from the same process.
	 */
	update_pde_first = 0;
	if (hat == hat_searchp) {
		if (my_hat->hat_flags & I86MMU_HWPPDONTSTEAL) {
			mutex_exit(&hat_hat_lock);
			return (0);
		}
		if (my_hat->hat_cpusrunning &&
			(my_hat->hat_cpusrunning != CPU->cpu_mask))
			update_pde_first = 1;
		hat = my_hat;
		clear_pde = 1;
	}
	mutex_exit(&hat_hat_lock);
	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		if (hwpp->hwpp_lockcnt)
			continue;
		/*
		 * check if this hwpp has already been stolen
		 */
		if ((hwpp->hwpp_pte == 0) || hwpp->hwpp_mapping)
			continue;
		if ((onlyhwpp = hat_hwppreserve()) == NULL) {
			if (!clear_pde)
				mutex_exit(&hat->hat_mutex);
			return (0);
		}
		if (update_pde_first)
			/*
			 * Here we are stealing from a process that is running
			 * on more than one cpu. We have to unload the pde
			 * on all those cpu's and flush their tlb's. Once
			 * this is done we can continue to look at pagetable
			 * entries for ref and mod bits.
			 */
			hat_update_pde(hat, hwpp, MMU_STD_INVALIDPTE);
		i86hme = hwpp->hwpp_firsthme;
		start_hme = hwpp->hwpp_hme;
		start_pte = hwpp->hwpp_pte;
		while (i86hme) {
			hme = I86HME_TO_FIRSTHME(i86hme);
			pte = i86hme->i86hme_pte;
			i86hme->i86hme_pteindex = (pte - start_pte);
			i86hme->i86hme_hwppbase = hwpp->hwpp_base;
			start_hme[(pte - start_pte) >> 1] = 0;
			if (pte_valid(pte)) {
				rm = *(pteptr_t)pte & PTE_RM_MASK;
				I86MMU_PTEHMEPROT(pte, hme);

				LOAD_PTE(pte, MMU_STD_INVALIDPTE);
				hme->hme_impl |= HME_PTEUNLOADED;
				if (!hme->hme_nosync && hme->hme_page) {
					if (rm & PTE_REF_MASK)
						hat_setref(hme->hme_page);
					if (rm & PTE_MOD_MASK)
						hat_setmod(hme->hme_page);
				} else if (!hme->hme_page) {
					hme->hme_valid = 0;
					hme->hme_nosync = 0;
					hme->hme_impl &= ~HME_PTEUNLOADED;
					atomic_decw(
						(short *)&hwpp->hwpp_numptes);
					hat->hat_rss--;
				}
			}
			hme++; pte++;
			if (pte_valid(pte)) {
				rm = *(pteptr_t)pte & PTE_RM_MASK;
				I86MMU_PTEHMEPROT(pte, hme);

				LOAD_PTE(pte, MMU_STD_INVALIDPTE);
				hme->hme_impl |= HME_PTEUNLOADED;
				if (!hme->hme_nosync && hme->hme_page) {
					if (rm & PTE_REF_MASK)
						hat_setref(hme->hme_page);
					if (rm & PTE_MOD_MASK)
						hat_setmod(hme->hme_page);
				} else if (!hme->hme_page) {
					hme->hme_valid = 0;
					hme->hme_nosync = 0;
					hme->hme_impl &= ~HME_PTEUNLOADED;
					atomic_decw(
						(short *)&hwpp->hwpp_numptes);
					hat->hat_rss--;
				}
			}
			i86hme = i86hme->i86hme_next;
		}
		hwpp->hwpp_pde = MMU_STD_INVALIDPTE;
		hat_pagetablefree(hwpp, onlyhwpp);
		if (clear_pde || hat->hat_pagedir) {
			kpreempt_disable();
			if (CPU->cpu_current_hat == hat) {
			    mmu_pdeload_cpu(CPU,
				hwppbasetoaddr(hwpp->hwpp_base),
				MMU_STD_INVALIDPTE);
			} else if ((pagedir = hat->hat_pagedir) != NULL) {
			    LOAD_PTE(&pagedir[hwpp->hwpp_base],
				MMU_STD_INVALIDPTE);
			}
			reload_cr3();
			kpreempt_enable();
		}
	}

	hat->hat_flags |= I86MMU_PTEPAGE_STOLEN;
	if (!clear_pde)
		/*
		 * We accquired the hat mutex of the process that we stole
		 * from. If we were stealing our own pagetable we have to
		 * return with the hat_mutex held.
		 */
		mutex_exit(&hat->hat_mutex);
	return (ret);
}

/*
 * This is the second level of stealing. Here we steal all resources
 * attached to the 'struct hat'. In case we can not find any process to
 * steal from we wait for the type resource, that caused the higher fellow to
 * invoke this function.
 * argument 'what' could be
 * I86MMUHWPP	we have run out of hwpp's on onlyhwpp_freelist
 * I86MMUHME	i86hme freelist is empty and we can not allocate more
 * I86MMUHMESPACE	the hmespace freelist is empty ( this is a freelist
 *			of 2K chunks that hold hme pointers)
 * We would be forced to sleep only on an MP machine with a low ratio of
 * memory to CPU's like a 32Mb machine with 6 CPU's
 */
static void
hat_hmesteal(what)
int	what;
{
	struct	hat	*hat;
	hwptepage_t 	*hwpp, *nexthwpp;
	int		ret;


	/*
	 *	Find an hwpp to steal.
	 */

	mutex_enter(&hat_hat_lock);
	hat = hat_searchp;
	while ((hat = hat->hat_next) != hat_searchp) {
		if (hat == &all_hat)
			continue;
		if (mutex_tryenter(&hat->hat_mutex)) {
			if ((hat->hat_flags &
				(I86MMU_FREEING|I86MMU_HMESTEAL|I86MMU_DUP)) ||
				((hat->hat_cpusrunning) &&
				(hat->hat_cpusrunning !=
					CPU->cpu_mask)) 	||
				(hat->hat_numhwpp == 0)) {
				mutex_exit(&hat->hat_mutex);
				continue;
			} else {
				hat->hat_flags |= I86MMU_HMESTEAL;
				break;
			}
		}
	}
	if (hat == hat_searchp) {
		if (what == I86MMUHME || what == I86MMUHMESPACE) {
			mutex_exit(&hat_hat_lock);
			mutex_enter(&hat_res_lock);
			if (((what == I86MMUHME) &&
				(i86hme_freelist == NULL)) ||
				((what == I86MMUHMESPACE) &&
				(hat_hmespace_freelist == NULL))) {
				hat_hmeres_wait = 1;
				cv_wait(&hat_cv, &hat_res_lock);
			}
			mutex_exit(&hat_res_lock);
		} else if ((what == I86MMUHWPP) &&
				(onlyhwpp_freelist == NULL)) {
			hat_hwppres_wait = 1;
			cv_wait(&hat_cv, &hat_hat_lock);
			mutex_exit(&hat_hat_lock);
		} else mutex_exit(&hat_hat_lock);
		return;
	}
	hat_searchp = hat;
	mutex_exit(&hat_hat_lock);

begin:
	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		if (hwpp->hwpp_lockcnt || hwpp->hwpp_mapping)
			continue;
		if (ret = hat_hmesteal_hwppunload(hwpp, hat)) {
			/*
			 * ret of 0 is normal
			 * ret of 1 exit this function
			 * ret of 2 things changed underneath
			 */
			if (ret == 2)
				goto begin;
			hat->hat_flags &= ~I86MMU_HMESTEAL;
			cv_signal(&hat->hat_cv);
			mutex_exit(&hat->hat_mutex);
			return;
		}
	}
	/* Free all the hwpp's now */
	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		if (!hwpp->hwpp_mapping && (hwpp->hwpp_numptes == 0))
			hat_hwppfree(hat, hwpp, hwpp->hwpp_firsthme, 1);
	}
	hat->hat_flags &= ~I86MMU_HMESTEAL;
	cv_signal(&hat->hat_cv);
	mutex_exit(&hat->hat_mutex);
}

/*
 * free the hwpp back to freelist. If force is not set and if this hat
 * has I86MMU_HMESTEAL set then, we just return. In hat_hmesteal() we
 * would have dropped hat_mutex so we dont want hwpp's to disappear
 * underneath us
 */
static void
hat_hwppfree(hat, hwpp, i86hme, force)
struct	hat	*hat;
struct	hwptepage *hwpp;
struct	i86hme	*i86hme;
int	force;
{
	struct	hwptepage *phwpp;
	u_int	*start_hme, addr;
	pteptr_t pagedir, pagetablep;
	struct	pte *start_pte, *pte;
	struct cpu *cp;
	int	freeing, i, need_to_wakeup;



	if ((hat->hat_flags & I86MMU_HMESTEAL) && !force)
		return;

	freeing = ((hat->hat_flags & (I86MMU_FREEING|I86MMU_SWAPOUT)) ==
			I86MMU_FREEING);
	addr = hwpp->hwpp_base;
	hat->hat_numhwpp--;
	phwpp = hat->hat_hwpp;
	if (phwpp == hwpp) {
		hat->hat_hwpp = hwpp->hwpp_next;
	} else {
		while (phwpp->hwpp_next != hwpp)
			phwpp = phwpp->hwpp_next;
		phwpp->hwpp_next = hwpp->hwpp_next;
	}
	start_hme = hwpp->hwpp_hme;
	if (!freeing)
		i86hme = hwpp->hwpp_firsthme;
	/*
	 * we need to clear the i86hme pointers. If the pagetable was
	 * stolen they have been cleared in hat_hwppsteal()
	 */
	if (i86hme && hwpp->hwpp_pte)  {
		start_pte = hwpp->hwpp_pte;
		while (i86hme) {
			pte = i86hme->i86hme_pte;
			start_hme[(pte - start_pte) >> 1] = 0;
			i86hme = i86hme->i86hme_next;
		}
	}
	mutex_enter(&hat_res_lock);

	if (hwpp->hwpp_lasthme) {
		/* put the i86hme's on freelist */
		hwpp->hwpp_lasthme->i86hme_next = i86hme_freelist;
		i86hme_freelist = hwpp->hwpp_firsthme;
	}
	hwpp->hwpp_firsthme = 0;
	hwpp->hwpp_lasthme = 0;

	/* IF this hwpp has an hmespace hanging of it free it now */
	if (hwpp->hwpp_hme && (((u_int)start_hme & ~(MMU_PAGESIZE - 1))
		!= (u_int)hwpp->hwpp_pte)) {

		((struct hmespace *)hwpp->hwpp_hme)->next =
			hat_hmespace_freelist;
		hat_hmespace_freelist = (struct hmespace *)hwpp->hwpp_hme;
	}
	if (hat_hmeres_wait) {
		hat_hmeres_wait = 0;
		cv_broadcast(&hat_cv);
	}
	mutex_exit(&hat_res_lock);

	mutex_enter(&hat_res_flock);

	hwpp->hwpp_hme = 0;


	/*
	 * Only put those pages that came from hwpp_dupfreelist back to it.
	 * We never steal pages from this freelist, so we dont want to free
	 * extra pages in to this list
	 */
	if (hwpp->hwpp_mapping) {
		if ((hat->hat_flags & I86MMU_SPTAS) && (hwpp->hwpp_pte)) {
			pagetablep = (pteptr_t)hwpp->hwpp_pte;
			kmem_free((caddr_t)pagetablep, MMU_PAGESIZE);
		}
		hwpp->hwpp_pte = 0;
		hwpp->hwpp_mapping = 0;
		hwpp->hwpp_next = onlyhwpp_freelist;
		onlyhwpp_freelist = hwpp;
	} else if (hwpp->hwpp_pte >= dup_ptes) {
		hwpp->hwpp_next = hwpp_dupfreelist;
		hwpp_dupfreelist = hwpp;
	} else if (hwpp->hwpp_pte == 0) {
		/*
		 * If this hwpp does not have a pagetable, free it on the
		 * onlyhwpp freelist
		 */
		if (onlyhwpp_freelist == NULL)
			need_to_wakeup = 1;
		else need_to_wakeup = 0;
		hwpp->hwpp_next = onlyhwpp_freelist;
		onlyhwpp_freelist = hwpp;
		mutex_exit(&hat_res_flock);
		if (need_to_wakeup) {
			mutex_enter(&hat_hat_lock);
			if (hat_hwppres_wait) {
				hat_hwppres_wait = 0;
				cv_broadcast(&hat_cv);
			}
			mutex_exit(&hat_hat_lock);
		}
		return;
	} else  {
		hwpp->hwpp_next = hwpp_freelist;
		hwpp_freelist = hwpp;
	}
	mutex_exit(&hat_res_flock);
	kpreempt_disable();
	if (!freeing) {
		if ((pagedir = hat->hat_pagedir) != NULL) {
			LOAD_PTE(&pagedir[addr], MMU_STD_INVALIDPTE);
		} else if (!hat->hat_cpusrunning ||
			(hat->hat_cpusrunning == CPU->cpu_mask)) {
			if (CPU->cpu_current_hat == hat) {
			    mmu_pdeload_cpu(CPU,
				hwppbasetoaddr(addr), MMU_STD_INVALIDPTE);
			}
		} else {
			CAPTURE_CPUS(hat);
			for (i = 0; i < NCPU; i++) {
				if ((cp = cpu[i]) == NULL)
					continue;
				if (cp->cpu_current_hat == hat) {
				    mmu_pdeload_cpu(cp,
					hwppbasetoaddr(addr),
					MMU_STD_INVALIDPTE);
				}
			}
			RELEASE_CPUS;
		}
		reload_cr3();
	}
	kpreempt_enable();

}


/*
 * allocate a 4K pagetable for 'hwpp'. This function reconstructs the pagetable
 * from the i86hme list hanging of 'hwpp'. It also fills in i86hme pointers
 */
static int
hat_pagetablealloc(struct hwptepage *hwpp)
{
	struct	hwptepage *newhwpp;
	struct	i86hme *i86hme;
	struct	pte	*pte, *start_pte;
	struct	hment	*hme;
	int	offset, prot, need_to_wakeup;
	u_int	*start_hme;
	struct hat *hat = hwpp->hwpp_hat;
	pteval_t value;

	mutex_enter(&hat_res_flock);
	if (hwpp_freelist) {
		newhwpp = hwpp_freelist;
		hwpp_freelist = newhwpp->hwpp_next;
		hwpp->hwpp_pfn = newhwpp->hwpp_pfn;
		hwpp->hwpp_pte = newhwpp->hwpp_pte;
		if (onlyhwpp_freelist == NULL)
			need_to_wakeup = 1;
		else  need_to_wakeup = 0;

		newhwpp->hwpp_next = onlyhwpp_freelist;
		onlyhwpp_freelist = newhwpp;
		mutex_exit(&hat_res_flock);
		hwpp_array[(hwpp->hwpp_pte - ptes) >> NPTESHIFT] = hwpp;
		i86hme = hwpp->hwpp_firsthme;
		if (hwpp->hwpp_hme == (u_int *)0)
			hwpp->hwpp_hme = (u_int *)(hwpp->hwpp_pte +
				I86MMU_MAXPTES);
		start_hme = hwpp->hwpp_hme;
		start_pte = hwpp->hwpp_pte;
		while (i86hme) {
			hme = I86HME_TO_FIRSTHME(i86hme);
			offset = i86hme->i86hme_pteindex;
			pte = start_pte + offset;
			i86hme->i86hme_pte = pte;
			hme->hme_impl &= ~HME_PTEUNLOADED;
			if (hme->hme_valid) {
				prot = I86MMU_PROTFROMHME(hme);
				value =
				PTEOF_CS(page_pptonum(hme->hme_page), prot,
				    hme->hme_nosync);
				LOAD_PTE(pte, value);
			} else if (hme->hme_impl & HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
				hme->hme_impl &= ~HME_PAGEUNLOADED;
			}
			I86MMU_ZEROHMEPROT(hme);
			/* repeat the above for the next hme */
			hme++; pte++;
			hme->hme_impl &= ~HME_PTEUNLOADED;
			if (hme->hme_valid) {
				prot = I86MMU_PROTFROMHME(hme);
				value =
				PTEOF_CS(page_pptonum(hme->hme_page), prot,
				    hme->hme_nosync);
				LOAD_PTE(pte, value);
			} else if (hme->hme_impl & HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
				hme->hme_impl &= ~HME_PAGEUNLOADED;
			}
			I86MMU_ZEROHMEPROT(hme);
			start_hme[offset >> 1] = (u_int)i86hme;
			i86hme = i86hme->i86hme_next;
		}
		if (need_to_wakeup) {
			mutex_enter(&hat_hat_lock);
			if (hat_hwppres_wait) {
				hat_hwppres_wait = 0;
				cv_signal(&hat_cv);
			}
			mutex_exit(&hat_hat_lock);
		}
		return (1);
	}
	mutex_exit(&hat_res_flock);
	return (0);
}

/*
 * free 4K pagetable attached to 'hwpp', by linking it to 'newhpp'
 */
static void
hat_pagetablefree(struct hwptepage *hwpp, struct hwptepage *newhwpp)
{

	newhwpp->hwpp_pfn = hwpp->hwpp_pfn;
	newhwpp->hwpp_pte = hwpp->hwpp_pte;
	mutex_enter(&hat_res_flock);
	hwpp_array[(newhwpp->hwpp_pte - ptes) >> NPTESHIFT] = newhwpp;
	newhwpp->hwpp_next = hwpp_freelist;
	hwpp_freelist = newhwpp;
	mutex_exit(&hat_res_flock);
	if (((u_int)hwpp->hwpp_hme & ~(MMU_PAGESIZE - 1)) ==
		(u_int)hwpp->hwpp_pte)
		hwpp->hwpp_hme = 0;
	hwpp->hwpp_pte = 0;
}

#ifdef DEBUG

#define	MLIST_ENTER_STAT() (vmhatstat.vh_mlist_enter.value.ul++)
#define	MLIST_WAIT_STAT() (vmhatstat.vh_mlist_enter_wait.value.ul++)
#define	MLIST_EXIT_STAT() (vmhatstat.vh_mlist_exit.value.ul++)
#define	MLIST_BROADCAST_STAT() (vmhatstat.vh_mlist_exit_broadcast.value.ul++)

#else

#define	MLIST_ENTER_STAT()
#define	MLIST_WAIT_STAT()
#define	MLIST_EXIT_STAT()
#define	MLIST_BROADCAST_STAT()

#endif

void
i486mmu_mlist_exit(struct page *pp)
{
	register struct hment	*hme, *nexthme;
	struct	hwptepage *hwpp;
	struct	hat	*hat;
	struct	i86hme	*i86hme;
	struct	pte *pte;


	if (pp && mach_pp->p_mapping) {
		nexthme = mach_pp->p_mapping;
		while ((hme = nexthme) != NULL) {
			nexthme = hme->hme_next;
			if (hme->hme_valid == 0) {
				ASSERT((hme->hme_impl & HME_KERNEL) == 0);
				i86hme = HMETOI86HME(hme);
				pte = i86hme->i86hme_pte;
				hwpp = ptetohwpp(pte);
				hat = hwpp->hwpp_hat;
				hme_sub(hme, pp);
				if (atomic_decw_retzflg(&hwpp->hwpp_numptes)) {
					mutex_enter(&hat->hat_mutex);
					hat->hat_rss--;
					hat_hwppfree(hat,
						hwpp, hwpp->hwpp_firsthme, 0);
					if (hat->hat_numhwpp == 0)
						cv_signal(&hat->hat_cv);
					mutex_exit(&hat->hat_mutex);
				}
			}
		}
	}
}



/*
 * this function is called from hat_free() when the parent exits. We need
 * to invalidate all pte's that the child inherited from its parent
 */
static void
hat_handle_dup_parentexit(hat)
struct	hat *hat;
{

	int	running;
	struct 	hwptepage *hwpp, *nexthwpp;
	struct	hment	*hme;
	struct	pte	*pte, *start_pte;
	u_int	*hme_start, addr;
	struct	i86hme	*i86hme;

	running = hat->hat_cpusrunning;
	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		if (hwpp->hwpp_mapping)
			continue;
		start_pte = hwpp->hwpp_pte;
		hme_start = hwpp->hwpp_hme;
		for (pte = start_pte, addr = 0; addr < NPTEPERPT * MMU_PAGESIZE;
			addr += MMU_PAGESIZE, pte++) {
			if (!pte_valid(pte))
				continue;
			if (hme_start)
				i86hme = (struct i86hme *)
				(*(hme_start + HMETABLE_INDEX(addr)));
			else
				i86hme = (struct i86hme *)0;
			if (i86hme) {
				hme = I86HME_TO_HME(i86hme, addr);
				if ((hme->hme_valid == 0) && !running) {
					LOAD_PTE(pte, MMU_STD_INVALIDPTE);
				} else if (hme->hme_valid == 0) {
					(void) hat_pteunload(hme, pte, 0, 0);
				}
			} else if (!running) {
				LOAD_PTE(pte, MMU_STD_INVALIDPTE);
			} else {
				(void) hat_pteunload(NULL, pte, 0, 0);
			}
		}
	}
}




static caddr_t
hat_hmespace_alloc()
{
	struct	hmespace *hmespacep;

	mutex_enter(&hat_res_lock);
	if ((hmespacep = hat_hmespace_freelist) != NULL) {
		hat_hmespace_freelist = hmespacep->next;
		mutex_exit(&hat_res_lock);
		hmespacep->next = (struct hmespace *)0;
	} else if (hat_do_hmesteal ||
	    (hat_allocated_memory > hat_max_memalloc)) {
		hat_do_hmesteal = 1;
		mutex_exit(&hat_res_lock);
		return (NULL);
	} else {
		hat_allocated_memory += HMESPACE_SIZE;
		mutex_exit(&hat_res_lock);
		hmespacep = kmem_zalloc(HMESPACE_SIZE, KM_NOSLEEP);
		if (hmespacep == NULL) {
			mutex_enter(&hat_res_lock);
			hat_allocated_memory -= HMESPACE_SIZE;
			hat_do_hmesteal = 1;
			mutex_exit(&hat_res_lock);
		}
	}
	return ((caddr_t)hmespacep);
}

/*
 * entry point to request large mapping for the range addr, to addr+len
 * both addr and len have to be large pagesize aligned.
 */
static int
hat_map_largepage(struct hat *hat, caddr_t addr, size_t len)
{
	struct	hwptepage  *hwpp;
	u_int	hwpp_addr;
	int	i, num_pages;



	num_pages = len >> largepageshift;

	hwpp_addr = addrtohwppbase(addr);

	for (i = 0; i < num_pages; i++, addr += largepagesize) {
		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);
		if (hwpp)
			continue;
		hwpp = hat_onlyhwppalloc(hat, addr);
		hwpp->hwpp_mapping = I86MMU_LGPAGE_LOCKMAP;
		hwpp->hwpp_pde =  MMU_STD_INVALIDPTE;
	}
	if (hat->hat_pagedir == NULL)
		hat_switch2perprocess_pagedir(hat);
	return (0);
}



/*
 * Unload an hwpp, this function is called only from hat_free().
 * If the 4K pte page of this hwpp is not stolen we dont drop the hat lock
 */

static void
hat_hwppunload(hwpp, hat)
struct	hwptepage *hwpp;
struct hat *hat;
{
	u_int *start_hme;
	struct pte	*pte, *start_pte;
	struct page	*pp;
	struct hment	*hme;
	struct i86hme	*i86hme, *i86hmenext;


	if ((hwpp->hwpp_numptes == 0) || (hwpp->hwpp_mapping)) {
		hat_hwppfree(hat, hwpp, hwpp->hwpp_firsthme, 0);
			return;
	}
	i86hme = hwpp->hwpp_firsthme;
	start_hme = hwpp->hwpp_hme;
	start_pte = hwpp->hwpp_pte;
	while (i86hme) {
		i86hmenext = i86hme->i86hme_next;
		hme = I86HME_TO_FIRSTHME(i86hme);
		/*
		 * Hardrware page table has been stolen
		 */
		pte = i86hme->i86hme_pte;
		if (start_hme && start_pte)
			start_hme[(pte - start_pte) >> 1] = 0;


recheck_hme1:
		if (hme->hme_impl & HME_PTEUNLOADED) {
			if (hme->hme_valid) {
				if ((pp = hme->hme_page) != NULL) {
					mutex_exit(&hat->hat_mutex);
					hat_mlist_enter(pp);
					mutex_enter(&hat->hat_mutex);
					/*
					 * We had dropped the lock
					 * Make sure the page this hme
					 * was pointing to is still
					 * mapped. pageunload() could
					 * have made this mapping
					 * invalid. We are here
					 * most likely bescause of
					 * hat_swapout(), in that
					 * case pageout() would be
					 * unloading a lot of pages
					 */
					if (!(hme->hme_page)) {
						hat_mlist_exit(pp);
						goto recheck_hme1;
					}
					hme_sub(hme, pp);
					hme->hme_valid = 0;
					hme->hme_nosync = 0;
					hat_mlist_exit(pp);
				}
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			} else if (hme->hme_impl & HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			}
			/*
			 * if the hme had HME_PTEUNLOADED set but was not valid
			 * nor had HME_PAGEUNLOADED set, its bescause
			 * hathme_steal() was trying to free this hwpp
			 */
			if (hwpp->hwpp_numptes == 0) {
				hat_hwppfree(hat, hwpp,
					i86hme->i86hme_next, 0);
				break;
			}
		} else if (hme->hme_valid && pte_valid(pte) &&
			    hat_pteunload(hme, pte, HAT_RMSYNC,
				I86MMU_PPINUSE_NOTSET)) {
					break;
		}
		hme++;
recheck_hme2:
		if (hme->hme_impl & HME_PTEUNLOADED) {
			if (hme->hme_valid) {
				if ((pp = hme->hme_page) != NULL) {
					mutex_exit(&hat->hat_mutex);
					hat_mlist_enter(pp);
					mutex_enter(&hat->hat_mutex);
					if (!(hme->hme_page)) {
						hat_mlist_exit(pp);
						goto recheck_hme2;
					}
					hme_sub(hme, pp);
					hme->hme_valid = 0;
					hme->hme_nosync = 0;
					hat_mlist_exit(pp);
				}
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			} else if (hme->hme_impl& HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			}
			if (hwpp->hwpp_numptes == 0) {
				hat_hwppfree(hat, hwpp,
					i86hme->i86hme_next, 0);
				break;
			}
		} else if (hme->hme_valid) {
			pte++;
			if (pte_valid(pte) &&
			    hat_pteunload(hme, pte, HAT_RMSYNC,
				I86MMU_PPINUSE_NOTSET)) {
				break;
			}
		}
		i86hme = i86hmenext;
	}
}

/*
 * This function unloads a hwpp and is called from hat_hmesteal(). Things
 * could change when we drop the hat lock. So this is slower than the
 * previous function.
 * return values
 * 0 - All is well
 * 1 - We need to return from hmesteal() as the process we are stealing from is
 *	running
 * 2 - We need to look at this process from scratch
 */
static int
hat_hmesteal_hwppunload(hwpp, hat)
struct	hwptepage *hwpp;
struct	hat	*hat;
{
	pte_t *pte;
	struct hment	*hme;
	int	num_hwpp;
	struct	page *pp;
	struct	i86hme *i86hme, *i86hmenext;

	if (hwpp->hwpp_numptes == 0) {
		hat_hwppfree(hat, hwpp,
			hwpp->hwpp_firsthme, 1);
		return (0);
	}
	num_hwpp = hat->hat_numhwpp;
	i86hme = hwpp->hwpp_firsthme;
	while (i86hme) {
		i86hmenext = i86hme->i86hme_next;

		hme = I86HME_TO_FIRSTHME(i86hme);

recheck_hme1:
		pte = i86hme->i86hme_pte;
		if (hme->hme_impl & HME_PTEUNLOADED) {
			if (hme->hme_valid) {
				if ((pp = hme->hme_page) != NULL) {
					mutex_exit(&hat->hat_mutex);
					hat_mlist_enter(pp);
					mutex_enter(&hat->hat_mutex);
					if (hat->hat_cpusrunning &&
					(hat->hat_cpusrunning !=
						CPU->cpu_mask)) {
						hat_mlist_exit(pp);
						return (1);
					}

					/*
					 * We had dropped the lock
					 * Make sure the page this hme
					 * was pointing to is still
					 * mapped. pageunload() could
					 * have made this mapping
					 * invalid.
					 */
					if (num_hwpp !=
						hat->hat_numhwpp) {
						hat_mlist_exit(pp);
						return (2);
					} else if ((pp != hme->hme_page) ||
					(!(hme->hme_impl &HME_PTEUNLOADED))) {
						hat_mlist_exit(pp);
						goto recheck_hme1;
					}
					hme_sub(hme, pp);
					hat_mlist_exit(pp);
				}
				hme->hme_valid = 0;
				hme->hme_nosync = 0;
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			} else if (hme->hme_impl & HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
				hme->hme_impl &= ~HME_PAGEUNLOADED;
			}
		} else if (hme->hme_valid && pte_valid(pte)) {
			pp = hme->hme_page;
			if (!pp)
				goto unload_pte1;
			mutex_exit(&hat->hat_mutex);
			hat_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			if (hat->hat_cpusrunning &&
				(hat->hat_cpusrunning != CPU->cpu_mask)) {
				hat_mlist_exit(pp);
				return (1);
			}
			/*
			 * hwpp's dont disappear underneath us, but its safe
			 * to have this check
			 */
			if (num_hwpp != hat->hat_numhwpp) {
				hat_mlist_exit(pp);
				return (2);
			/*
			 * this check also ensures that hme->hme_valid is set
			 */
			} else if ((pp != hme->hme_page)) {
				hat_mlist_exit(pp);
				goto recheck_hme1;
			} else if (hme->hme_impl & HME_PTEUNLOADED) {
				hat_mlist_exit(pp);
				goto recheck_hme1;
			}
unload_pte1:
			/*
			 * Reload the pte, since we dropped the hat
			 * lock. The process could now own a different
			 * pte page as hat_hwppsteal() is also
			 * running
			 */
			pte = i86hme->i86hme_pte;
			(void) hat_pteunload(hme, pte, HAT_RMSYNC,
					I86MMU_PPINUSE_SET);
			if (pp)
				hat_mlist_exit(pp);
		}
		hme++;
recheck_hme2:
		pte = i86hme->i86hme_pte;
		pte++;
		if (hme->hme_impl & HME_PTEUNLOADED) {
			if (hme->hme_valid) {
				if ((pp = hme->hme_page) != NULL) {
					mutex_exit(&hat->hat_mutex);
					hat_mlist_enter(pp);
					mutex_enter(&hat->hat_mutex);
					if (hat->hat_cpusrunning &&
					(hat->hat_cpusrunning !=
						CPU->cpu_mask)) {
						hat_mlist_exit(pp);
						return (1);
					}
					if (num_hwpp !=
						hat->hat_numhwpp) {
						hat_mlist_exit(pp);
						return (2);
					} else if ((pp != hme->hme_page) ||
					(!(hme->hme_impl & HME_PTEUNLOADED))) {
						hat_mlist_exit(pp);
						goto recheck_hme2;
					}
					hme_sub(hme, pp);
					hat_mlist_exit(pp);
				}
				hme->hme_valid = 0;
				hme->hme_nosync = 0;
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
			} else if (hme->hme_impl& HME_PAGEUNLOADED) {
				atomic_decw((short *)&hwpp->hwpp_numptes);
				hat->hat_rss--;
				hme->hme_impl &= ~HME_PAGEUNLOADED;
			}
		} else if (hme->hme_valid && pte_valid(pte)) {
			pp = hme->hme_page;
			if (!pp)
				goto unload_pte2;
			mutex_exit(&hat->hat_mutex);
			hat_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			if (hat->hat_cpusrunning &&
				(hat->hat_cpusrunning != CPU->cpu_mask)) {
				hat_mlist_exit(pp);
				return (1);
			}
			if (num_hwpp != hat->hat_numhwpp) {
				hat_mlist_exit(pp);
				return (2);
			} else if ((pp != hme->hme_page)) {
				hat_mlist_exit(pp);
				goto recheck_hme2;
			} else if (hme->hme_impl & HME_PTEUNLOADED) {
				hat_mlist_exit(pp);
				goto recheck_hme2;
			}
unload_pte2:
			pte = i86hme->i86hme_pte;
			pte++;
			(void) hat_pteunload(hme, pte, HAT_RMSYNC,
				I86MMU_PPINUSE_SET);
			if (pp)
				hat_mlist_exit(pp);
		}
		i86hme = i86hmenext;
		if (hwpp->hwpp_numptes == 0) {
			hat_hwppfree(hat, hwpp, hwpp->hwpp_firsthme, 1);
			return (0);
		}
	}
	return (0);
}

/*
 * function returns a pointer to hme that maps the given virtual address
 */
static struct hment *
hat_findhme(hwpp, addr)
struct	hwptepage *hwpp;
u_int	addr;
{
	u_int	base_addr;
	struct	i86hme *i86hme;
	struct	hment	*hme;
	int	offset;

	base_addr = (u_int)hwppbasetoaddr(hwpp->hwpp_base);
	i86hme = hwpp->hwpp_firsthme;
	hme = (struct hment *)0;
	while (i86hme) {
		offset = (u_int)i86hme->i86hme_pteindex;
		if (addr == (base_addr + offset * MMU_PAGESIZE)) {
			hme = I86HME_TO_FIRSTHME(i86hme);
			break;
		} else if (addr == (base_addr + (offset+1) * MMU_PAGESIZE)) {
			hme = I86HME_TO_SECONDHME(i86hme);
			break;
		}
		i86hme = i86hme->i86hme_next;
	}
	return (hme);
}

/*
 * A very expensive function called only from hat_hwppsteal()
 * We need to clear a page directory entry on all those cpu's that 'as'
 * is currently active.
 */
static void
hat_update_pde(struct hat *hat, struct hwptepage *hwpp, pteval_t value)
{
	int i;
	struct cpu *cpup;
	pteptr_t pagedir;

	kpreempt_disable();
	/*
	 * We expect the following CAPTURE_CPUS() to force all cpu's on
	 * which as is currently running to invalidate all of its tlb.
	 */
	CAPTURE_CPUS(hat);
	for (i = 0; i < NCPU; i++) {
		if ((cpup = cpu[i]) == NULL)
			continue;
		/*
		 * We have captured this CPU, we can clear the pde
		 * now. We dont care if this cpu has kernel_only_cr3
		 * loaded. As long as the cpu_current_hat is pointing
		 * to the argument 'hat' its safe to clear the pde.
		 */
		if ((pagedir = hwpp->hwpp_hat->hat_pagedir) != NULL) {
			LOAD_PTE(&pagedir[hwpp->hwpp_base], value);
			break;
		} else if (cpup->cpu_current_hat == hat) {
			pagedir = (pteptr_t)cpup->cpu_pagedir;
			LOAD_PTE(&pagedir[hwpp->hwpp_base], value);
		}
	}
	reload_cr3();
	RELEASE_CPUS;
	kpreempt_enable();
}


static u_long
hat_getpfnum_nolock(struct hat *hat, caddr_t addr)
{
	pte_t	*pte;
	pteval_t pte_lgpage;
	pteptr_t pagedir;
	u_int	pfn;

	if (mutex_tryenter(&hat->hat_mutex) == 0)
		return ((u_long)PFN_INVALID);
	if ((addr < (caddr_t)KERNELBASE) && (pagedir = hat->hat_pagedir)) {
		pte_lgpage = pagedir[addrtohwppbase(addr)];
		if (LARGE_PAGE((pte_t *)&pte_lgpage)) {
			if (pte_valid((pte_t *)&pte_lgpage))
				pfn =  (PTE_PFN(pte_lgpage) +
				PAGETABLE_INDEX((u_int)addr));
			else
				pfn = (u_int)PFN_INVALID;
			mutex_exit(&hat->hat_mutex);
			return ((u_long)pfn);
		}
	}
	pte = hat_ptefind(hat, addr);
	pfn = (pte == NULL || !pte_valid(pte))
	    ? PFN_INVALID : MAKE_PFNUM(pte);
	mutex_exit(&hat->hat_mutex);
	return (pfn);
}


/*
 * Get a hat structure from the freelist
 */
static struct hat *
hat_gethat()
{
	struct hat *hat;

	mutex_enter(&hat_res_mutex);
	if ((hat = hatfree) == NULL)	/* "shouldn't happen" */
		panic("out of hats");

	hatfree = hat->hat_next;
	hat->hat_next = NULL;

	mutex_exit(&hat_res_mutex);
	return (hat);
}

static void
hat_freehat(hat)
register struct hat *hat;
{

	mutex_enter(&hat_res_mutex);
	hat->hat_next = hatfree;
	hatfree = hat;
	mutex_exit(&hat_res_mutex);
}

static int
hat_is_pp_largepage(page_t **ppa)
{
	int pfn = ((machpage_t *)ppa[0])->p_pagenum;
	int i;

	for (i = 0; i < NPTEPERPT; i++, pfn++) {
		if (((machpage_t *)ppa[i])->p_pagenum != pfn) {
			return (0);
		}
	}
	return (1);
}


/*
 * Call the init routines for every configured hat.
 */
void
hat_init()
{
	register struct hat *hat;


	/*
	 * Allocate mmu independent hat data structures.
	 */
	nhats = v.v_proc + (v.v_proc/2);
	if ((hats = (struct hat *)kmem_zalloc(sizeof (struct hat) * nhats,
	    KM_NOSLEEP)) == NULL)
		panic("Cannot allocate memory for hat structs");
	hatsNHATS = hats + nhats;

	for (hat = hatsNHATS - 1; hat >= hats; hat--) {
		mutex_init(&hat->hat_mutex, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&hat->hat_cv, NULL, CV_DEFAULT, NULL);
		hat_freehat(hat);
	}

	hat_init_internal();

	/*
	 * We grab the first hat for the kernel,
	 * the above initialization loop initialized sys_hatops and kctx.
	 */
	kas.a_hat = hat_alloc(&kas);
}

/*
 * Allocate a hat structure.
 * Called when an address space first uses a hat.
 */
struct hat *
hat_alloc(struct as *as)
{
	register struct hat *hat;

	if ((hat = hat_gethat()) == NULL)
		panic("no hats");
	hat->hat_as = as;
	hat->hat_flags = 0;
	hat->hat_cpusrunning = 0;
	hat->hat_rss = 0;
	if (as == &kas) {
		kernel_hat = hat;
		return (hat);
	}
	mutex_enter(&hat_hat_lock);
	hat->hat_next = all_hat.hat_next;
	all_hat.hat_next = hat;
	hat->hat_next->hat_prev = hat;
	hat->hat_prev = &all_hat;
	mutex_exit(&hat_hat_lock);
	return (hat);
}

void
hat_free_start(hat)
register struct hat *hat;
{
	struct hat *hat_c, *hat_p;
	u_int i, flags;
	struct cpu *cpup;
	struct hwptepage *hwpp, *nexthwpp;
	pteptr_t pagedir;
	int	swap = 0;

	if (hat->hat_flags & I86MMU_DUP) {
		mutex_enter(&hat_dup_lock);
		if (hat->hat_flags & I86MMU_DUP_CHILD) {
			hat_p = hat->hat_dup;
			mutex_enter(&hat_p->hat_mutex);
			mutex_enter(&hat->hat_mutex);
			hat_p->hat_dup = (struct hat *)0;
			hat_p->hat_flags &= ~(I86MMU_DUP_PARENT);
			mutex_exit(&hat_p->hat_mutex);
		} else if (hat->hat_flags & I86MMU_DUP_PARENT) {
			hat_c = hat->hat_dup;
			mutex_enter(&hat->hat_mutex);
			mutex_enter(&hat_c->hat_mutex);
			hat_c->hat_dup = (struct hat *)0;
			hat_c->hat_flags &= ~(I86MMU_DUP_CHILD);
			hat_handle_dup_parentexit(hat_c);
			mutex_exit(&hat_c->hat_mutex);
		} else mutex_enter(&hat->hat_mutex);
		hat->hat_dup = (struct hat *)0;
		hat->hat_flags &= ~I86MMU_DUP;
		mutex_exit(&hat_dup_lock);
	} else mutex_enter(&hat->hat_mutex);

	while (hat->hat_flags & I86MMU_HMESTEAL)
		cv_wait(&hat->hat_cv, &hat->hat_mutex);

	hat->hat_flags |= I86MMU_FREEING;
	if (hat->hat_flags & I86MMU_SWAPOUT)
		swap = 1;

	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		hat_hwppunload(hwpp, hat);
	}

	while (hat->hat_numhwpp)
		cv_wait(&hat->hat_cv, &hat->hat_mutex);
	kpreempt_disable();
	for (i = 0; i < NCPU; i++) {
		if ((cpup = cpu[i]) != NULL) {
			if (cpup->cpu_current_hat != hat)
				continue;
			flags = intr_clear();
			lock_set(&cpup->cpu_pt_lock);
			/*
			 * check again, since the 1st check was without lock
			 */
			if (cpup->cpu_current_hat == hat) {
				cpup->cpu_current_hat = NULL;
			}
			lock_clear(&cpup->cpu_pt_lock);
			intr_restore((u_int)flags);
		}
	}
	if (((pagedir = hat->hat_pagedir) != NULL) && !swap) {
		/*
		 * The CAPTURE_CPUS(), RELEASE_CPU sequence would
		 * cause all those cpu's who had their cr3 set to
		 * hat_pdepfn to reload their cr3 with kernel_only_cr3
		 */
		CAPTURE_CPUS(hat);
		RELEASE_CPUS;
		hat->hat_pagedir = NULL;
		setcr3(kernel_only_cr3);
	}
	kpreempt_enable();
	if (!swap) {
		mutex_enter(&hat_hat_lock);
		hat->hat_prev->hat_next = hat->hat_next;
		hat->hat_next->hat_prev = hat->hat_prev;
		if (hat == hat_searchp)
			hat_searchp = hat->hat_next;
		mutex_exit(&hat_hat_lock);
		if (pagedir)
			kmem_free(pagedir, PAGEDIR_SIZE);
	} else {
		/*
		 * VM layer never calls hat_swapin() so reset
		 * the flags right now
		 */
		hat->hat_flags &= ~(I86MMU_SWAPOUT|I86MMU_FREEING);
		hat->hat_rss = 0;
	}
	mutex_exit(&hat->hat_mutex);
}

void
hat_free_end(struct hat *hat)
{
	mutex_enter(&hat_res_mutex);
	hat->hat_next = hatfree;
	hatfree = hat;
	mutex_exit(&hat_res_mutex);
}

/*
 * Duplicate the translations of an as into another newas
 */
int
hat_dup(struct hat *hat, struct hat *hat_c, caddr_t addr,
    size_t len, u_int flags)
{
	struct	hwptepage *hwpp, *hwpp_c, *nexthwpp, *phwpp;
	int	duphwpp_allocated = 0;

#ifdef	lint
	addr = addr;
	len = len;
#endif

	if (hat_dup_enable == 0)
		return (0);
	else if (flags != HAT_DUP_ALL) {
		if (flags == HAT_DUP_COW)
			cmn_err(CE_PANIC, "hat_dup: HAT_DUP_COW not supported");
		else
			return (0);
	}
	mutex_enter(&hat->hat_mutex);

	/*
	 * We only have 4 dup pagetables. hat_flags could be set bescause of
	 * I86MMU_UNUSUAL - this guy could have more than 2 pagetables.
	 * I86MMU_HMESTEAL, I86MMU_PTEPAGE_STOLEN - too complex
	 * so in these cases we ignore the hat_dup() call
	 */

	if (hat->hat_flags) {
		mutex_exit(&hat->hat_mutex);
		return (0);
	}
	mutex_enter(&hat_c->hat_mutex);


	nexthwpp = hat->hat_hwpp;
	while ((hwpp = nexthwpp) != NULL) {
		nexthwpp = hwpp->hwpp_next;
		/*
		 * If pagetable has been stolen
		 * dont duplicate this hwpp
		 */
		if ((hwpp->hwpp_pte == 0) || hwpp->hwpp_mapping)
			continue;
		/*
		 * If we need to allocate hmespace skip this
		 * hwpp
		 */
		if (((u_int)hwpp->hwpp_hme & ~(MMU_PAGESIZE-1))
			!= (u_int)hwpp->hwpp_pte)
			continue;

		if ((hwpp_c = hat_hwppalloc(DUP_PAGETABLE)) == 0) {
			goto hat_dup_failed;
		}
		duphwpp_allocated++;
		hat_c->hat_numhwpp++;
		hwpp_c->hwpp_hat = hat_c;
		hwpp_c->hwpp_numptes = 0;
		hwpp_c->hwpp_lockcnt = 0;
		hwpp_c->hwpp_base = hwpp->hwpp_base;
		hwpp_c->hwpp_hme = 0;
		hwpp_c->hwpp_firsthme = (struct i86hme *)0;
		hwpp_c->hwpp_lasthme = (struct i86hme *)0;
		if ((phwpp = hat_c->hat_hwpp) != NULL) {
			while (phwpp->hwpp_next)
				phwpp = phwpp->hwpp_next;
			phwpp->hwpp_next = hwpp_c;
		} else {
			hat_c->hat_hwpp = hwpp_c;
		}
		hwpp_c->hwpp_pde = PTEOF_C(hwpp_c->hwpp_pfn, MMU_STD_SRWXURWX);
		bcopy((caddr_t)hwpp->hwpp_pte, (caddr_t)hwpp_c->hwpp_pte,
			(I86MMU_MAXPTES - 1) * sizeof (struct pte));
	}
hat_dup_failed:
	if (duphwpp_allocated) {
		hat->hat_flags |= I86MMU_DUP_PARENT;
		hat_c->hat_flags |= I86MMU_DUP_CHILD;
		hat->hat_dup = hat_c;
		hat_c->hat_dup = hat;
	}
	mutex_exit(&hat->hat_mutex);
	mutex_exit(&hat_c->hat_mutex);
	return (0);

}


/*
 * VM layer does not make this call
 */
void
hat_swapin(struct hat *hat)
{
#ifdef	lint
	hat = hat;
#endif
}


/*
 * Free all of the translation resources, for the specified address space,
 * that can be freed while the process is swapped out. Called from as_swapout.
 */
void
hat_swapout(struct hat *hat)
{
	mutex_enter(&hat->hat_mutex);
	hat->hat_flags |= I86MMU_SWAPOUT;
	mutex_exit(&hat->hat_mutex);
	hat_free_start(hat);
}


void
hat_map(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{

#ifdef	lint
	hat = hat;
	addr = addr;
	len = len;
	flags = flags;
#endif
}

int
hat_pageunload(struct page *pp, u_int forceflag)
{
	register struct hat *hat;
	struct	hment	*hme;

#ifdef	lint
	forceflag = forceflag;
#endif
	ASSERT(PAGE_LOCKED(pp));

	hat_mlist_enter(pp);
	while ((hme = (struct hment *)mach_pp->p_mapping) != NULL) {
		hat = &hats[hme->hme_hat];
		hat_pageunload_hme(pp, hat, hme);
	}
	hat_mlist_exit(pp);
	return (0);
}


u_int
hat_pagesync(struct page *pp, u_int clearflag)
{
	struct hment	*hme;
	struct	hat	*hat;
	extern	u_long	po_share;


	if (PP_ISRO(pp) && (clearflag & HAT_SYNC_STOPON_MOD)) {
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	if (mach_pp->p_share > po_share &&
	    !(clearflag & HAT_SYNC_ZERORM)) {
		if (PP_ISRO(pp))
			atomic_orb(&mach_pp->p_nrm, P_REF);
		return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
	}

	hat_mlist_enter(pp);
	for (hme = (struct hment *)mach_pp->p_mapping; hme;
	    hme = hme->hme_next) {
		hat = &hats[hme->hme_hat];
		hat_pagesync_pp(hat, pp, hme, clearflag & ~HAT_SYNC_STOPON_RM);
		/*
		 * If clearflag is HAT_DONTZERO, break out as soon
		 * as the "ref" or "mod" is set.
		 */
		if ((clearflag & ~HAT_SYNC_STOPON_RM) == HAT_SYNC_DONTZERO &&
		    ((clearflag & HAT_SYNC_STOPON_MOD) && PP_ISMOD(pp)) ||
		    ((clearflag & HAT_SYNC_STOPON_REF) && PP_ISREF(pp)))
			break;
	}
	hat_mlist_exit(pp);

	return (mach_pp->p_nrm & (P_REF|P_MOD|P_RO));
}

void
hat_sync(struct hat *hat, caddr_t addr, size_t len, u_int clearflag)
{
	register caddr_t	a, ea;
	register struct hment	*hme;
	register pte_t		*pte;
	struct	hwptepage	*hwpp;
	struct	i86hme		*i86hme;
	u_int			*hme_start;

	mutex_enter(&hat->hat_mutex);

	for (a = addr, ea = addr + len; a < ea; a += MMU_PAGESIZE) {
		pte = hat_ptefind(hat, a);
		if ((pte == NULL) || !pte_valid(pte))
			continue;
		hwpp = ptetohwpp(pte);
		if ((hme_start = (u_int *)hwpp->hwpp_hme) == NULL)
			continue;
		i86hme = (struct i86hme *)(*(hme_start + HMETABLE_INDEX(a)));
		if (i86hme == NULL)
			continue;
		hme = I86HME_TO_HME(i86hme, addr);
		if (hme->hme_valid)
			hat_ptesync(hme, pte,
			    clearflag ? HAT_RMSYNC : HAT_RMSTAT);
	}

	mutex_exit(&hat->hat_mutex);
}

/*
 * Make a mapping at addr to map page pp with protection prot.
 *
 * No locking at this level.  Hat_memload and hat_devload
 * are wrappers for xxx_pteload, where xxx is instance of
 * a hat module like the sun mmu.  Xxx_pteload is called from
 * machine dependent code directly.  The locking (acquiring
 * the per hat hat_mutex) that is done in xxx_pteload allows both the
 * hat layer and the machine dependent code to work properly.
 */
void
hat_memload(struct hat *hat, caddr_t addr, page_t *pp,
    u_int attr, u_int flags)
{
	pte_t pte;
	short prot = attr & HAT_PROT_MASK;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	ASSERT(PAGE_LOCKED(pp));
	if (PP_ISFREE(pp))
		cmn_err(CE_PANIC,
		    "hat_memload: loading a mapping to free page %x", (int)pp);
	if (hat->hat_stat)
		hat_resvstat(MMU_PAGESIZE, hat->hat_as, addr);

	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE,
		    "hat_memload: called with unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	*(pteptr_t)&pte = PTEOF_CS(page_pptonum(pp), hat_protection[prot],
		((attr & HAT_NOSYNC) ? 1 : 0));
	if (pp) hat_mlist_enter(pp);
	if (hat != kernel_hat) {
		mutex_enter(&hat->hat_mutex);
		hat_pteload(hat, addr, pp, &pte, flags);
		mutex_exit(&hat->hat_mutex);
	} else {
		hat_pteload(hat, addr, pp, &pte, flags);
	}

	if (pp) hat_mlist_exit(pp);
}

/*
 * Cons up a struct pte using the device's pf bits and protection
 * prot to load into the hardware for address addr; treat as minflt.
 */
void
hat_devload(struct hat *hat, caddr_t addr, size_t len, u_long pf,
    u_int attr, int flags)
{
	short 		prot = attr & HAT_PROT_MASK;
	pte_t 		pte;
	page_t		*pp;
	int		pf_mem;
	int		pagesize, pp_incr;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	if (len == 0)
		cmn_err(CE_PANIC, "hat_devload: zero len");
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_devload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	if (hat->hat_stat)
		hat_resvstat(len, hat->hat_as, addr);

	if (IS_P2ALIGNED(addr, largepagesize) &&
	    IS_P2ALIGNED(len, largepagesize) &&
	    IS_P2ALIGNED(pf, NPTEPERPT)) {
		pagesize = largepagesize;
		pp_incr = NPTEPERPT;
	} else {
		pagesize = MMU_PAGESIZE;
		pp_incr = 1;
	}

	/*
	 * Device memory not backed by devpage structs.
	 */
	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
	while (len) {
		if (((pf_mem = pf_is_memory(pf)) != 0) &&
		    !(flags & HAT_LOAD_NOCONSIST)) {
			pp = page_numtopp_nolock(pf);
			if (pp) {
				if (hat != kernel_hat)
					mutex_exit(&hat->hat_mutex);
				hat_mlist_enter(pp);
				if (hat != kernel_hat)
					mutex_enter(&hat->hat_mutex);
			} else flags |= HAT_LOAD_NOCONSIST;
		} else pp = NULL;
		LOAD_PTE(&pte, PTEOF(pf, hat_vtop_prot(addr, prot), !pf_mem));
		if (attr & HAT_NOSYNC)
			setpte_nosync(&pte);
		if (pagesize == largepagesize)
			(void) hat_map_largepage(hat, addr, largepagesize);
		(void) hat_pteload(hat, addr, pp, &pte, flags);
		if (pp) hat_mlist_exit(pp);
		pf += pp_incr;
		addr += pagesize;
		len -= pagesize;
	}
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
}

/*
 * Set up translations to a range of virtual addresses backed by
 * physically contiguous memory. The MMU driver can take advantage
 * of underlying hardware to set up translations using larger than
 * 4K bytes size pages. The caller must ensure that the pages are
 * locked down and that their identity will not change.
 */
void
hat_memload_array(struct hat *hat, caddr_t addr, size_t len,
    page_t **ppa, u_int attr, u_int flags)
{

	page_t	*pp;
	short 	prot = attr & HAT_PROT_MASK;
	pte_t 	apte;
	int	pagesize, pp_incr;

	ASSERT((hat->hat_as == &kas) || AS_LOCK_HELD(hat->hat_as,
	    &hat->hat_as->a_lock));
	if (flags & ~HAT_SUPPORTED_LOAD_FLAGS)
		cmn_err(CE_NOTE, "hat_memload: unsupported flags %d",
		    flags & ~HAT_SUPPORTED_LOAD_FLAGS);

	if (hat->hat_stat)
		hat_resvstat(len, hat->hat_as, addr);

	ASSERT((len & MMU_PAGEOFFSET) == 0);
	while (len) {
		pp = *ppa;
		if ((len > largepagesize) 			&&
		    IS_P2ALIGNED(addr, largepagesize) 	&&
		    IS_P2ALIGNED(mach_pp->p_pagenum, NPTEPERPT)	&&
		    (flags & HAT_LOAD_CONTIG || hat_is_pp_largepage(ppa))) {
			ASSERT((flags & (HAT_LOAD_LOCK|HAT_LOAD_SHARE)) ==
			(HAT_LOAD_LOCK|HAT_LOAD_SHARE));
			pagesize = largepagesize;
			pp_incr = NPTEPERPT;
		} else {
			pagesize = MMU_PAGESIZE;
			pp_incr = 1;
		}

		LOAD_PTE(&apte,
		    PTEOF_CS(page_pptonum(pp),
			hat_protection[prot], ((attr & HAT_NOSYNC) ? 1 : 0)));
		if (pp) hat_mlist_enter(pp);
		if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
		if (pagesize == largepagesize)
			(void) hat_map_largepage(hat, addr, largepagesize);
		(void) hat_pteload(hat, addr, pp, &apte, flags);
		if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
		if (pp) hat_mlist_exit(pp);
		ppa += pp_incr;
		addr += pagesize;
		len -= pagesize;
	}
}

/*
 * Release one hardware address translation lock on the given address.
 */
void
hat_unlock(struct hat *hat, caddr_t addr, size_t len)
{

	register pte_t 		*pte;
	register u_int 		a, newa;
	int			hwpp_addr;
	struct	hwptepage 	*hwpp;
	int			span;


	ASSERT((len & MMU_PAGEOFFSET) == 0);

	if (hat == kernel_hat)
		return;
	mutex_enter(&hat->hat_mutex);

	hwpp_addr = 0;
#ifdef	lint
	span = MMU_PAGESIZE;
#endif
	for (a = (u_int)addr; a < (u_int)addr + len; a += span) {

		span = MMU_PAGESIZE;
		hwpp_addr = addrtohwppbase(a);

		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);

		if (hwpp->hwpp_mapping) {
			newa = ALIGN_TONEXT_LGPAGE(a);
			span = newa - a;
			continue;
		}
		pte = &hwpp->hwpp_pte[PAGETABLE_INDEX(a)];
		/*
		 * If the address was mapped, we need to unlock
		 * the page table it resides in.
		 */
		if (pte_valid(pte))
			hwpp->hwpp_lockcnt--;
		if (hwpp->hwpp_lockcnt == 0) {
			mutex_enter(&hat_userpgtbl_lock);
			total_locked_userpgtbl--;
			cv_broadcast(&hat_userpgtbl_cv);
			mutex_exit(&hat_userpgtbl_lock);
		}

	}

	mutex_exit(&hat->hat_mutex);
}

#define	HAT_LOAD_ATTR		1
#define	HAT_SET_ATTR		2
#define	HAT_CLR_ATTR		4
#define	HAT_LOAD_ATTR_PGDIR	8


static void
hat_updateattr(struct hat *hat, caddr_t addr, size_t len, u_int attr, int what)
{
	short 			vprot = attr & HAT_PROT_MASK;
	register caddr_t 	ea;
	pte_t 			*pte = NULL, oldpte, *pde;
	register u_int 		pprot, ppprot;
	int			hwpp_addr, span;
	u_int   		a, newa;
	struct	hwptepage 	*hwpp = NULL;
	u_int			*hme_start;
	pteptr_t		pagedir;
	struct	hment		*hme;
	struct	i86hme		*i86hme;


	mutex_enter(&hat->hat_mutex);

	ppprot = MMU_STD_SRX;
	if (vprot & PROT_WRITE)
		ppprot |= MMU_STD_SRWX;
	if (vprot & PROT_USER)
		ppprot |= MMU_STD_SRXURX;

#ifdef	lint
	span = MMU_PAGESIZE;
#endif
	for (a = (u_int)addr, ea = addr + len; a < (u_int)ea; a += span) {
		pprot = ppprot;
		span = MMU_PAGESIZE;
		hme = NULL;
		if (a >= (u_int)KERNELBASE) {
			if (a >= phys_syslimit && a < (u_int)Syslimit)
				/*
				 * On machines with memory less than 64MB
				 * we do not allocate pagetables for the entire
				 * range of space between SYSBASE to Syslimit
				 */
				continue;
			pagedir = (pteptr_t)kernel_only_pagedir;
			pde = (struct pte  *)
				&pagedir[MMU_L1_INDEX((u_int)a)];
			if (LARGE_PAGE(pde)) {
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;
				continue;
			}
			if (what == HAT_LOAD_ATTR_PGDIR) {
			    kpreempt_disable();
			    pagedir = (pteptr_t)CPU->cpu_pagedir;
			    pde = (struct pte  *)
				&pagedir[MMU_L1_INDEX((u_int)a)];
				if (pde->AccessPermissions != pprot)
				    pde->AccessPermissions = pprot;
				reload_cr3();
			    kpreempt_enable();
			    newa = ALIGN_TONEXT_LGPAGE(a);
			    span = newa - a;
			    continue;
			}
			pte = hat_ptefind(kernel_hat, (caddr_t)a);
			hme = kptetohme(pte);
		} else {
			hwpp_addr = addrtohwppbase(a);

			HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);

			/* skip this 4 MB if no hardware page allocated */
			if (!hwpp) {
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;
				continue;
			} else  if (hwpp->hwpp_mapping ==
			    I86MMU_LGPAGE_LOCKMAP) {
				/*
				 * These guys have a per process page directory
				 */
				pagedir = hat->hat_pagedir;
				pte = (struct pte *)&pagedir[hwpp->hwpp_base];
				newa = ALIGN_TONEXT_LGPAGE(a);
				span = newa - a;

			} else if (hwpp->hwpp_pte != 0) {
				pte = &hwpp->hwpp_pte[PAGETABLE_INDEX(a)];
				if (!pte_valid(pte))
					continue;
				if ((attr & HAT_NOSYNC) &&
				    (hme_start = (u_int *)hwpp->hwpp_hme)
				    != NULL) {
					i86hme = (struct i86hme *)
					(*(hme_start + HMETABLE_INDEX(a)));
					if (i86hme)
						hme = I86HME_TO_HME(i86hme, a);
					else
						hme = (struct hment *)0;
				} else {
					hme = (struct hment *)0;
				}
			} else {

				hme = hat_findhme(hwpp, a);
				if (!hme || hme->hme_valid == 0)
					continue;
				switch (what) {
				case HAT_SET_ATTR:
					pprot |= I86MMU_PROTFROMHME(hme);
					if (attr & HAT_NOSYNC)
						hme->hme_nosync = 1;
					break;
				case HAT_CLR_ATTR:
					pprot = ~pprot&I86MMU_PROTFROMHME(hme);
					if (attr & HAT_NOSYNC)
						hme->hme_nosync = 0;
					break;
				}
				if (pprot != I86MMU_PROTFROMHME(hme)) {
					I86MMU_ZEROHMEPROT(hme);
					I86MMU_HMEPROT(hme, pprot);
				}
				continue;
			}
		}
		if (!pte_valid(pte))
			continue;
		oldpte = *pte;
		switch (what) {
		case HAT_SET_ATTR:
			pprot |= pte->AccessPermissions;
			if (attr & HAT_NOSYNC) {
				if (hme) hme->hme_nosync = 1;
				setpte_nosync(&oldpte);
			}
			break;
		case HAT_CLR_ATTR:
			pprot = ~pprot & pte->AccessPermissions;
			if (attr & HAT_NOSYNC) {
				if (hme) hme->hme_nosync = 0;
				clrpte_nosync(&oldpte);
			}
			break;
		}

		if ((pte->AccessPermissions != pprot) || (attr & HAT_NOSYNC)) {
			oldpte.AccessPermissions = pprot;
			if (hwpp && (hwpp->hwpp_mapping ==
			    I86MMU_LGPAGE_LOCKMAP))
				hat_update_pde(hat, hwpp, *(pteptr_t)&oldpte);
			else
				(void) hat_update_pte(pte, *(pteptr_t)&oldpte,
				    hat, (caddr_t)a, PTE_RM_MASK);
		}
	}

	mutex_exit(&hat->hat_mutex);
}
/*
 * Change the protections in the virtual address range
 * given to the specified virtual protection.  If
 * vprot == ~PROT_WRITE, then all the write permission
 * is taken away for the current translations, else if
 * vprot == ~PROT_USER, then all the user permissions
 * are taken away for the current translations, otherwise
 * vprot gives the new virtual protections to load up.
 *
 * addr and len must be MMU_PAGESIZE aligned.
 */
void
hat_chgprot(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	hat_updateattr(hat, addr, len, attr & HAT_PROT_MASK, HAT_LOAD_ATTR);
}
void
hat_chgattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_LOAD_ATTR);
}
void
hat_setattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_SET_ATTR);
}
void
hat_clrattr(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_CLR_ATTR);
}

void
hat_chgattr_pagedir(struct hat *hat, caddr_t addr, size_t len, u_int attr)
{
	hat_updateattr(hat, addr, len, attr, HAT_LOAD_ATTR_PGDIR);
}
u_int
hat_getattr(struct hat *hat, caddr_t addr, u_int *attr)
{
	pte_t			*pte;
	int			prot, hwpp_addr;
	struct	hwptepage	*hwpp;
	struct	hment		*hme;

	mutex_enter(&hat->hat_mutex);
	pte = hat_ptefind(hat, addr);
	*attr = 0;
	if (pte == NULL || !pte_valid(pte)) {
		ASSERT((u_int)addr < KERNELBASE);
		hwpp_addr = addrtohwppbase(addr);

		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);
		if (!hwpp) {
			mutex_exit(&hat->hat_mutex);
			return ((u_int)0xffffffff);
		}
		if (hwpp->hwpp_mapping) {
			pte = (pte_t *)&hat->hat_pagedir[hwpp_addr];
			prot = pte->AccessPermissions;
			if (getpte_nosync(pte))
				*attr = HAT_NOSYNC;
		} else {
			hme = hat_findhme(hwpp, (u_int)addr);
			if ((!hme) || (hme->hme_valid == 0)) {
				mutex_exit(&hat->hat_mutex);
				return ((u_int)0xffffffff);
			}
			prot = I86MMU_PROTFROMHME(hme);
			if (hme->hme_nosync)
				*attr = HAT_NOSYNC;
		}
	} else {
		prot = pte->AccessPermissions;
		if (getpte_nosync(pte))
			*attr = HAT_NOSYNC;
	}
	mutex_exit(&hat->hat_mutex);
	switch (prot) {
	case MMU_STD_SRX:
		*attr |=  PROT_READ | PROT_EXEC;
		break;
	case MMU_STD_SRWX:
		*attr |=  PROT_READ | PROT_WRITE | PROT_EXEC;
		break;
	case MMU_STD_SRXURX:
		*attr |=  PROT_READ | PROT_EXEC | PROT_USER;
		break;
	case MMU_STD_SRWXURWX:
		*attr |=  PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER;
		break;
	}
	return (0);
}

ssize_t
hat_getpagesize(struct hat *hat, caddr_t addr)
{
	pte_t	*pte;
	pteval_t pte_lgpage;
	pteptr_t pagedir;
	ssize_t	ret;

	mutex_enter(&hat->hat_mutex);
	if ((addr < (caddr_t)KERNELBASE) &&
	    (pagedir = hat->hat_pagedir)) {
		pte_lgpage = pagedir[addrtohwppbase(addr)];
		if (LARGE_PAGE((pte_t *)&pte_lgpage)) {
			mutex_exit(&hat->hat_mutex);
			if (pte_valid((pte_t *)&pte_lgpage))
				return (largepagesize);
			else
				return (-1);
		}
	}
	pte = hat_ptefind(hat, addr);
	ret  = ((pte == NULL || !pte_valid(pte)) ? -1 : MMU_PAGESIZE);
	mutex_exit(&hat->hat_mutex);
	return (ret);
}

void
hat_page_setattr(page_t *pp, u_int flag)
{
	atomic_orb(&mach_pp->p_nrm, flag);
}
void
hat_page_clrattr(page_t *pp, u_int flag)
{
	atomic_andb(&mach_pp->p_nrm, ~flag);
}
u_int
hat_page_getattr(page_t *pp, u_int flag)
{
	return (mach_pp->p_nrm & flag);
}

/*
 * Unload all the mappings in the range [addr..addr+len).
 */
void
hat_unload(struct hat *hat, caddr_t addr, size_t len, u_int flags)
{

	register 		pte_t *pte;
	register u_int 		span = 0;
	struct hat 		*hat_c;
	int			hwpp_addr, length = len;
	u_int   		a, newa, *hme_start;
	struct	hwptepage	*hwpp;
	struct	hment		*hme;
	struct	i86hme		*i86hme;
	struct	page		*pp;
	int			llen;


	ASSERT(hat == kernel_hat || (flags & HAT_UNLOAD_OTHER) || \
	    AS_LOCK_HELD(hat->hat_as, &hat->hat_as->a_lock));

#ifdef HAT_DEBUG
	if (unload_debug)
		cmn_err(CE_CONT, "hat_unload: as %x addr %x len %x",
		    (int)as, (int)addr, len);
#endif

	if (flags & HAT_UNLOAD_UNMAP)
		flags = (flags & ~HAT_UNLOAD_UNMAP) | HAT_UNLOAD;

	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
	ASSERT((len & MMU_PAGEOFFSET) == 0);

begin:
	for (a = (u_int)addr, llen = len; llen; llen -= span, a += span) {
		span = MMU_PAGESIZE;
		if (addr >= (caddr_t)KERNELBASE) {
			pte = hat_ptefind(kas.a_hat, (caddr_t)a);
			hme = kptetohme(pte);
			pp = hme->hme_page;
			if (pp)
				hat_mlist_enter(pp);
			(void) hat_pteunload(hme, pte, HAT_TO_I86MMUFLAGS(hme),
				I86MMU_PPINUSE_SET);
			if (pp)
				hat_mlist_exit(pp);
			continue;
		}
		hwpp_addr = addrtohwppbase(a);

		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);

		/* skip this 4 MB if no hardware page allocated */
		if (!hwpp) {
			newa = ALIGN_TONEXT_LGPAGE(a);
			if (llen <= newa - a)
				break;
			span = newa - a;
			continue;
		}
		if (hwpp->hwpp_mapping) {
			if (hat->hat_pagedir != NULL) {
				hat_update_pde(hat, hwpp, MMU_STD_INVALIDPTE);
			}
			hat->hat_rss -= hwpp->hwpp_numptes;
			hat_hwppfree(hat, hwpp, 0, 0);
			newa = ALIGN_TONEXT_LGPAGE(a);
			if (llen <= newa - a)
				break;
			span = newa - a;
			continue;
		}
		if (hwpp->hwpp_pte == 0) {
			/*
			 * We should not go through the following code
			 * often
			 */
			hme = hat_findhme(hwpp, a);
			if ((!hme) || (hme->hme_valid == 0))
				continue;
			pp = hme->hme_page;
			ASSERT(pp);
			if (hat_mlist_tryenter(pp)) {
				hme_sub(hme, pp);
				hme->hme_valid = 0;
				hme->hme_nosync = 0;
				hme->hme_impl |= HME_PAGEUNLOADED;
				hat_mlist_exit(pp);
				continue;
			}
			mutex_exit(&hat->hat_mutex);
			hat_mlist_enter(pp);
			mutex_enter(&hat->hat_mutex);
			if ((hwpp->hwpp_hat != hat) ||
				(hme->hme_page != pp)) {
				hat_mlist_exit(pp);
				goto begin;
			}
			if (!(hme->hme_impl & HME_PTEUNLOADED))  {
				pte = &hwpp->hwpp_pte[PAGETABLE_INDEX(a)];
				(void) hat_pteunload(hme, pte,
					HAT_TO_I86MMUFLAGS(hme),
					I86MMU_PPINUSE_SET);
				hat_mlist_exit(pp);
				continue;
			}
			hme_sub(hme, pp);
			hme->hme_valid = 0;
			hme->hme_nosync = 0;
			hme->hme_impl |= HME_PAGEUNLOADED;
			hat_mlist_exit(pp);
			continue;
		}

		pte = &hwpp->hwpp_pte[PAGETABLE_INDEX(a)];
		if (!pte_valid(pte))
			continue;

		if (flags & HAT_UNLOAD_UNLOCK) {
			/*
			 * If the address was mapped, we need to unlock
			 * the page table it resides in.
			 */
			hwpp->hwpp_lockcnt--;
			if (hwpp->hwpp_lockcnt == 0) {
				mutex_enter(&hat_userpgtbl_lock);
				total_locked_userpgtbl--;
				cv_broadcast(&hat_userpgtbl_cv);
				mutex_exit(&hat_userpgtbl_lock);
			}
		}
		if ((hme_start = (u_int *)hwpp->hwpp_hme) != NULL) {
			i86hme = (struct i86hme *)
				(*(hme_start + HMETABLE_INDEX(a)));
			if (i86hme) {
				hme = I86HME_TO_HME(i86hme, a);
				pp = hme->hme_page;
			} else {
				hme = (struct hment *)0;
				pp = (struct page *)0;
			}

		} else {
			hme = (struct hment *)0;
			pp = (struct page *)0;
		}
		if (pp && hat_mlist_tryenter(pp)) {
			(void) hat_pteunload(hme, pte, HAT_TO_I86MMUFLAGS(hme),
				I86MMU_PPINUSE_SET);
			hat_mlist_exit(pp);
			continue;
		} else if (!pp) {
			(void) hat_pteunload(hme, pte, HAT_TO_I86MMUFLAGS(hme),
				I86MMU_PPINUSE_SET);
			continue;
		}
		/*
		 * we failed to get mapping list lock. We need to accquire
		 * locks in the right order
		 */
		mutex_exit(&hat->hat_mutex);
		hat_mlist_enter(pp);
		mutex_enter(&hat->hat_mutex);
		if ((hwpp->hwpp_hat != hat) || (hme->hme_page != pp)) {
			hat_mlist_exit(pp);
			goto begin;
		}
		if (hme->hme_impl & HME_PTEUNLOADED) {
			hme_sub(hme, pp);
			hme->hme_valid = 0;
			hme->hme_nosync = 0;
			hme->hme_impl |= HME_PAGEUNLOADED;
			hat_mlist_exit(pp);
			continue;
		}
		(void) hat_pteunload(hme, pte, HAT_TO_I86MMUFLAGS(hme),
			I86MMU_PPINUSE_SET);
		hat_mlist_exit(pp);
	}

	if (!(hat->hat_flags & I86MMU_DUP_PARENT)) {
		if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
		return;
	}
	/*
	 * we need to invalidate those ptes that the child process
	 * inherited from its parent
	 */
	hat_c = hat->hat_dup;

	if ((hat_c == NULL) || (hat_c->hat_flags & I86MMU_FREEING)) {
		mutex_exit(&hat->hat_mutex);
		return;
	}
	mutex_enter(&hat_c->hat_mutex);
	mutex_exit(&hat->hat_mutex);
	hat = hat_c;

	for (a = (u_int)addr, len = length; len; len -= span, a += span) {
		span = MMU_PAGESIZE;
		hwpp_addr = addrtohwppbase(a);

		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);

		/* skip this 4 MB if no hardware page allocated */
		if (!hwpp || hwpp->hwpp_mapping) {
			newa = ALIGN_TONEXT_LGPAGE(a);
			if (len <= newa - a)
				break;
			span = newa - a;
			continue;
		}

		pte = &hwpp->hwpp_pte[PAGETABLE_INDEX(a)];
		if ((hme_start = (u_int *)hwpp->hwpp_hme) != NULL) {
			i86hme = (struct i86hme *)
				(*(hme_start + HMETABLE_INDEX(a)));
			if (i86hme) {
				hme = I86HME_TO_HME(i86hme, a);
			} else {
				hme = (struct hment *)0;
			}
		} else {
			hme = (struct hment *)0;
		}
		if (pte_valid(pte) && ((!hme) || (hme->hme_valid == 0))) {
			(void) hat_pteunload(hme, pte, HAT_RMSTAT,
				I86MMU_PPINUSE_SET);
		}
	}
	mutex_exit(&hat_c->hat_mutex);
}

/*
 * Mark the page as cached or non-cached (depending on flag). Make all mappings
 * to page 'pp' cached or non-cached. This is permanent as long as the page
 * identity remains the same.
 */

void
hat_pagecachectl(struct page *pp, int flag)
{
	register struct hment *hme;
	struct hat *hat;
	struct	pte *pte;

	ASSERT(PAGE_LOCKED(pp));
	if (flag & HAT_CACHE)
		PP_CLRPNC(pp);
	else if (flag & HAT_UNCACHE)
		PP_SETPNC(pp);

	hat_mlist_enter(pp);
	for (hme = (struct hment *)mach_pp->p_mapping; hme;
	    hme = hme->hme_next) {
		hat = &hats[hme->hme_hat];
		mutex_enter(&hat->hat_mutex);
		pte = hmetopte(hme);
		hat_ptesync(hme, pte, HAT_NCSYNC);
		mutex_exit(&hat->hat_mutex);
	}
	hat_mlist_exit(pp);
}

/*
 * Get the page frame number for a particular user virtual address.
 * Walk the hat list for the address space and call the getpfnum
 * op for each one; the first one to return a non-zero value is used
 * since they should all point to the same page.
 */
u_long
hat_getpfnum(struct hat *hat, caddr_t addr)
{
	pte_t	*pte;
	pteval_t pte_lgpage;
	pteptr_t pagedir;
	u_long	pfn;

	if (hat != kernel_hat) mutex_enter(&hat->hat_mutex);
	if ((addr < (caddr_t)KERNELBASE) &&
	    (pagedir = hat->hat_pagedir)) {
		pte_lgpage = pagedir[addrtohwppbase(addr)];
		if (LARGE_PAGE((pte_t *)&pte_lgpage)) {
			if (pte_valid((pte_t *)&pte_lgpage))
				pfn = PTE_PFN(pte_lgpage) +
				PAGETABLE_INDEX((u_int)addr);
			else
				pfn = (u_long)PFN_INVALID;
		}
		mutex_exit(&hat->hat_mutex);
		return ((u_long)pfn);
	}
	pte = hat_ptefind(hat, addr);
	pfn = ((pte == NULL || !pte_valid(pte))
		? PFN_INVALID : MAKE_PFNUM(pte));
	if (hat != kernel_hat) mutex_exit(&hat->hat_mutex);
	return (pfn);
}

/*
 * Return the number of mappings to a particular page.
 * This number is an approximation of the number of
 * number of people sharing the page.
 */
u_long
hat_page_getshare(pp)
	page_t *pp;
{
	return (mach_pp->p_share);
}

/*
 * hat_probe returns 1 if the translation for the address 'addr' is
 * loaded, zero otherwise.
 *
 * hat_probe should be used only for advisorary purposes because it may
 * occasionally return the wrong value. The implementation must guarantee that
 * returning the wrong value is a very rare event. hat_probe is used
 * to implement optimizations in the segment drivers.
 *
 * hat_probe doesn't acquire hat_mutex.
 */
int
hat_probe(struct hat *hat, caddr_t addr)
{
	return ((hat_getpfnum_nolock(hat, addr) == -1) ? 0 : 1);
}

/*
 * Copy top level mapping elements (L1 ptes or whatever)
 * that map from saddr to (saddr + len) in sas
 * to top level mapping elements from daddr in das.
 *
 * Hat_share()/unshare() return an (non-zero) error
 * when saddr and daddr are not properly aligned.
 *
 * The top level mapping element determines the alignment
 * requirement for saddr and daddr, depending on different
 * architectures.
 *
 * When hat_share()/unshare() are not supported,
 * HATOP_SHARE()/UNSHARE() return 0.
 */

/*ARGSUSED*/
int
hat_share(struct hat *hat, caddr_t addr, struct hat *hat_s,
caddr_t srcaddr, size_t size)
{
	struct	hwptepage  *hwpp, *hwpp_s;
	u_int	hwpp_addr;
	int	i, numpgtbl;
	pteptr_t pagedir;

	if ((u_int)addr & (largepagesize - 1))
		return (-1);
	mutex_enter(&hat->hat_mutex);
	mutex_enter(&hat_s->hat_mutex);

	numpgtbl = (size + largepagesize - 1) >> largepageshift;


	for (i = 0; i < numpgtbl; i++, addr += largepagesize,
		srcaddr += largepagesize) {


		hwpp_s = hat_s->hat_hwpp;
		hwpp_addr = addrtohwppbase(srcaddr);
		while (hwpp_s) {
			if (hwpp_s->hwpp_base == hwpp_addr)
				break;
			hwpp_s = hwpp_s->hwpp_next;
		}
		if (!hwpp_s)
			continue;

		hwpp_addr = addrtohwppbase(addr);

		HWPP_FROM_I86MMU(hat, hwpp, hwpp_addr);

		if (hwpp)
			continue;
		hwpp = hat_onlyhwppalloc(hat, addr);
		if (hwpp_s->hwpp_mapping == I86MMU_LGPAGE_LOCKMAP) {
			hwpp->hwpp_mapping = I86MMU_LGPAGE_LOCKMAP;
			hwpp->hwpp_pte = NULL;
			hwpp->hwpp_pde = hwpp_s->hwpp_pde;
			hwpp->hwpp_numptes = NPTEPERPT;
			hat->hat_rss += NPTEPERPT;
		} else {
			hwpp->hwpp_pte = (pte_t *)hwpp_s->hwpp_pte;
			hwpp->hwpp_pfn =
				hat_getkpfnum((caddr_t)(hwpp->hwpp_pte));
			hwpp->hwpp_pde =
				PTEOF_C(hwpp->hwpp_pfn, MMU_STD_SRWXURWX);
			hwpp->hwpp_mapping = I86MMU4KLOCKMAP;
			hwpp->hwpp_numptes = hwpp_s->hwpp_numptes;
			hat->hat_rss += hwpp_s->hwpp_numptes;
		}
	}
	mutex_exit(&hat_s->hat_mutex);
	if ((pagedir = hat->hat_pagedir) == NULL)
		hat_switch2perprocess_pagedir(hat);
	else {
		hwpp = hat->hat_hwpp;
		while (hwpp) {
			LOAD_PTE(&pagedir[hwpp->hwpp_base], hwpp->hwpp_pde);
			hwpp = hwpp->hwpp_next;
		}
		/*
		 * We could be called as a result of pagefault, so
		 * we have to load cr3
		 */
		kpreempt_disable();
		if (CPU->cpu_current_hat != hat)
			(void) hat_setup(hat, HAT_ALLOC);
		else
			LOAD_PAGEDIR(hat);
		kpreempt_enable();
	}
	mutex_exit(&hat->hat_mutex);
	return (0);
}

/*ARGSUSED*/
void
hat_unshare(struct hat *hat, caddr_t addr, size_t size)
{
	hat_unload(hat, addr, size, HAT_UNLOAD_UNMAP);
}


size_t
hat_get_mapped_size(struct hat *hat)
{
#ifdef	DEBUG
	struct hwptepage 	*hwpp;
	u_int			rss = 0;


	mutex_enter(&hat->hat_mutex);
	hwpp = hat->hat_hwpp;
	while (hwpp) {
		rss += hwpp->hwpp_numptes;
		hwpp = hwpp->hwpp_next;
	}
	/*
	 * The assert is not valid under all cases and is being removed.
	 * hat->hat_rss is protected by hat->hat_mutex
	 * hwpp->hwpp_numptes is operated by atomic_instructions and so
	 * rss could be different from hat->hat_rss.
	 * ASSERT(hat->hat_rss == rss);
	 */
	mutex_exit(&hat->hat_mutex);
	return ((size_t)ptob(rss));
#else
	return ((size_t)ptob(hat->hat_rss));

#endif
}

int
hat_stats_enable(struct hat *hat)
{
	atomic_incw(&hat->hat_stat);
	return (1);
}

void
hat_stats_disable(struct hat *hat)
{
	atomic_decw((short *)&hat->hat_stat);
}


void
hat_setup_nolock(struct hat *hat)
{
	if ((hat == NULL) || (mutex_tryenter(&hat->hat_mutex) == 0)) {
		setcr3(kernel_only_cr3);
		return;
	}
	hat_setup(hat, 0);
	mutex_exit(&hat->hat_mutex);
}

/*
 * Called from resume() and hat_ptealloc(). This function is called with
 * kpreempt_disable(),  to setup mmu for a user thread. This
 * function does not get called when we swtch to a kernel thread
 */
/*ARGSUSED1*/
void
hat_setup(struct hat *hat, int allocflag)
{
	struct cpu *cpup;
	struct	hwptepage *hwpp;
	register struct hat  *oldhat;
	u_int	index, flags;
	pteptr_t pagedir;


	if (hat == kernel_hat)
		return;
	if (hat == CPU->cpu_current_hat) {
		if ((ncpus == 1) || (hat->hat_pagedir))
			return;
		cpup = CPU;
		pagedir = (pteptr_t)cpup->cpu_pagedir;
		if (hat->hat_flags & I86MMU_UNUSUAL) {
			hwpp = hat->hat_hwpp;
			while (hwpp) {
				if (hwpp->hwpp_pte) {
					index = hwpp->hwpp_base;
					pagedir[index] = hwpp->hwpp_pde;
					if (I86MMUNUSUAL(index)) {
						set_pde_nzmask(cpup, index);
						set_pdemask(cpup, index);
					}
				}
				hwpp = hwpp->hwpp_next;
			}
		} else {
			if (((hwpp = hat->hat_hwpp) != NULL) && hwpp->hwpp_pte)
				pagedir[hwpp->hwpp_base] = hwpp->hwpp_pde;

			if (hwpp && (hwpp = hwpp->hwpp_next) &&
				hwpp->hwpp_pte)
				pagedir[hwpp->hwpp_base] = hwpp->hwpp_pde;
		}
		LOAD_PAGEDIR(hat);
		return;
	}

	cpup = CPU;
	flags = intr_clear();
	lock_set(&cpup->cpu_pt_lock);
	/*
	 * we hold the cpup->cpu_pt_lock so that we could serialize
	 * hat_free() clearing cpup->cpu_current_hat
	 */
	if ((oldhat = cpup->cpu_current_hat) != (struct hat *)0) {
		atomic_andl((unsigned long *)&oldhat->hat_cpusrunning,
		~(unsigned long)cpup->cpu_mask);
	}
	lock_clear(&cpup->cpu_pt_lock);
	intr_restore((u_int)flags);
	pagedir = (pteptr_t)cpup->cpu_pagedir;
	if (pde_nzmask[cpup->cpu_id]) {
		CLEAR_PDE(&pde_nzmask[cpup->cpu_id],
			pagedir, &pde_mask[cpup->cpu_id][0]);
	}
	pagedir[I86MMU_USER_TEXT] = MMU_STD_INVALIDPTE;
	pagedir[I86MMU_USER_SHLIB] = MMU_STD_INVALIDPTE;
	if (hat->hat_pagedir == NULL) {
		if (hat->hat_flags & I86MMU_UNUSUAL) {
			hwpp = hat->hat_hwpp;
			while (hwpp) {
				if (hwpp->hwpp_pte) {
					index = hwpp->hwpp_base;
					pagedir[index] = hwpp->hwpp_pde;
					if (I86MMUNUSUAL(index)) {
						set_pde_nzmask(cpup, index);
						set_pdemask(cpup, index);
					}
				}
				hwpp = hwpp->hwpp_next;
			}
		} else {
			if (((hwpp = hat->hat_hwpp) != NULL) && hwpp->hwpp_pte)
				pagedir[hwpp->hwpp_base] = hwpp->hwpp_pde;

			if (hwpp && (hwpp = hwpp->hwpp_next) &&
				hwpp->hwpp_pte)
				pagedir[hwpp->hwpp_base] = hwpp->hwpp_pde;
		}
	}
	cpup->cpu_current_hat = hat;
	LOAD_PAGEDIR(hat);
	atomic_orl((unsigned long *)&hat->hat_cpusrunning,
	    (unsigned long)cpup->cpu_mask);
}

void
hat_enter(struct hat *hat)
{
	mutex_enter(&hat->hat_mutex);
}
void
hat_exit(struct hat *hat)
{
	mutex_exit(&hat->hat_mutex);
}
/*
 * return supported features
 */
/* ARGSUSED */
int
hat_supported(enum hat_features feature, void *arg)
{
	switch (feature) {
	case    HAT_SHARED_PT:
		return (1);
	default:
		return (0);
	}
}


/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_softlock(hat, addr, lenp, ppp, flags)
	struct	hat *hat;
	caddr_t	addr;
	size_t	*lenp;
	page_t	**ppp;
	u_int	flags;
{
	return (FC_NOSUPPORT);
}

/*
 * This function is currently not supported on this platform. For what
 * it's supposed to do, see hat.c and hat_srmmu.c
 */
/*ARGSUSED*/
faultcode_t
hat_pageflip(hat, addr_to, kaddr, lenp, pp_to, pp_from)
	struct hat *hat;
	caddr_t addr_to, kaddr;
	size_t   *lenp;
	page_t  **pp_to, **pp_from;
{
	return (FC_NOSUPPORT);
}
u_int
mmu_ptov_prot(struct pte *pte)
{
	switch (pte->AccessPermissions) {
	case MMU_STD_SRX:
		return (PROT_READ | PROT_EXEC);
	case MMU_STD_SRWX:
		return (PROT_READ | PROT_WRITE | PROT_EXEC);
	case MMU_STD_SRXURX:
		return (PROT_READ | PROT_EXEC | PROT_USER);
	case MMU_STD_SRWXURWX:
		return (PROT_READ | PROT_WRITE | PROT_EXEC | PROT_USER);
	}
	/*NOTREACHED*/
}

u_long
hat_getkpfnum(caddr_t addr)
{
	return (hat_getpfnum(kas.a_hat, addr));
}

#define	ASCHUNK	64

/*
 * Kill process(es) that use the given page. (Used for parity recovery)
 * If we encounter the kernel's address space, give up (return -1).
 * Otherwise, we return 0.
 */
hat_kill_procs(page_t *pp, caddr_t addr)
{
	struct hment *hme;
	struct hat *hat;
	struct	as	*as;
	struct	proc	*p;
	struct	as	*as_array[ASCHUNK];
	int	loop	= 0;
	int	opid	= -1;
	int	i;

	hat_mlist_enter(pp);
again:
	if (mach_pp->p_mapping) {
		bzero((caddr_t)&as_array[0], ASCHUNK * sizeof (int));
		for (i = 0; i < ASCHUNK; i++) {
			hme = mach_pp->p_mapping;
			hat = &hats[hme->hme_hat];
			as = hat->hat_as;

			/*
			 * If the address space is the kernel's, then fail.
			 * The only thing to do with corrupted kernel memory
			 * is die.  The caller is expected to panic if this
			 * is true.
			 */
			if (as == &kas) {
				hat_mlist_exit(pp);
				printf("parity error: ");
				printf("kernel address space\n");
				return (-1);
			}
			as_array[i] = as;

			if (hme->hme_next)
				hme = hme->hme_next;
			else
				break;
		}
	}

	for (i = 0; i < ASCHUNK; i++) {

		as = as_array[i];
		if (as == NULL)
			break;

		/*
		 * Note that more than one process can share the
		 * same address space, if vfork() was used to create it.
		 * This means that we have to look through the entire
		 * process table and not stop at the first match.
		 */
		mutex_enter(&pidlock);
		for (p = practive; p; p = p->p_next) {
			k_siginfo_t siginfo;
			kthread_t *t = NULL;

			if (p->p_as == as) {
				/* limit messages about same process */
				if (opid != p->p_pid) {
					printf("parity error: killing pid %d\n",
					    (int)p->p_pid);
					opid =  p->p_pid;
					uprintf("pid %d killed: parity error\n",
					    (int)p->p_pid);
				}

				bzero((caddr_t)&siginfo, sizeof (siginfo));
				siginfo.si_addr = addr;
				siginfo.si_signo = SIGBUS;
				/*
				 * the following code should probably be
				 * something from siginfo.h
				 */
				siginfo.si_code = FC_HWERR;

				mutex_enter(&p->p_lock);
				sigaddq(p, t, &siginfo, KM_NOSLEEP);
				mutex_exit(&p->p_lock);
			}
		}
		mutex_exit(&pidlock);
	}

	/*
	 * Wait for previously signaled processes to die off,
	 * thus removing their mappings from the mapping list.
	 */
	delay(hz);

	/*
	 * More than ASCHUNK mappings on the list for the page,
	 * loop to kill off the rest of them.  This will terminate
	 * with failure if there are more than ASCHUNK*20 mappings
	 * or a process will not die.
	 */
	if (mach_pp->p_mapping) {
		loop++;
		if (loop > 20) {
			hat_mlist_exit(pp);
			return (-1);
		}
		goto again;
	}
	hat_mlist_exit(pp);

	return (0);
}

#ifdef	PTE36
void
mmu_loadcr3(struct cpu *cpup, void *arg)
{
	struct hat 	*hat = (struct hat *)arg;
	int		i;
	uint64_t	*pttbl;

	if (cpup->cpu_current_hat == NULL) {
		setcr3(kernel_only_cr3);
		return;
	} else if (hat == NULL) {
		reload_cr3();
		return;
	} else if ((pttbl = hat->hat_pgdirpttblent) == NULL) {
		pttbl = &cpup->cpu_pgdirpttblent[0];
	}
	for (i = 0; i < NPDPERAS; i++)
		mmu_pgdirptbl_load(cpup, i, pttbl[i]);
	setcr3(cpup->cpu_cr3);
}

#else

/* ARGSUSED */
void
mmu_loadcr3(struct cpu *cpup, void *arg)
{

	if (cpup->cpu_current_hat == NULL)
		setcr3(kernel_only_cr3);
	else
		reload_cr3();
}

#endif

/* ARGSUSED */
void
hat_map_kernel_stack_local(caddr_t saddr, caddr_t eaddr) {}
