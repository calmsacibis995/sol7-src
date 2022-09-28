/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)startup.c	1.98	98/02/05 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/conf.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>

#include <sys/disp.h>
#include <sys/class.h>
#include <sys/bitmap.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kstat.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>
#include <sys/acct.h>

#include <sys/vfs.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/debug/debug.h>

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/kvtopdata.h>

#include <sys/consdev.h>
#include <sys/frame.h>

#include <sys/sunddi.h>
#include <sys/ddidmareq.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/tss.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/fp.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/mach_page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/swap.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat.h>
#include <sys/instance.h>
#include <sys/smp_impldefs.h>
#include <sys/x86_archext.h>
#include <sys/segment.h>

extern void debug_enter(char *);
extern void lomem_init(void);

/*
 * XXX make declaration below "static" when drivers no longer use this
 * interface.
 */
extern void kadb_uses_kernel(void);
extern caddr_t p0_va;	/* Virtual address for accessing physical page 0 */
extern void get_font_ptrs(void);

static void kvm_init(void);
static int establish_console(void);

int console = CONSOLE_IS_FB;

/*
 * Declare these as initialized data so we can patch them.
 */
pgcnt_t physmem = 0;	/* memory size in pages, patch if you want less */
int kernprot = 1;	/* write protect kernel text */

/* Global variables for MP support. Used in mp_startup */
caddr_t	rm_platter_va;
paddr_t	rm_platter_pa;


/*
 * Configuration parameters set at boot time.
 */

caddr_t econtig;		/* end of first block of contiguous kernel */
caddr_t eecontig;		/* end of segkp, which is after econtig */

struct bootops *bootops = 0;	/* passed in from boot */

char bootblock_fstype[16];

/*
* new memory fragmentations are possible in startup() due to BOP_ALLOCs. this
* depends on number of BOP_ALLOC calls made and requested size, memory size `
*  combination.
*/
#define	POSS_NEW_FRAGMENTS	10

/*
 * VM data structures
 */
long page_hashsz;		/* Size of page hash table (power of two) */
struct machpage *pp_base;	/* Base of system page struct array */
struct	page **page_hash;	/* Page hash table */
struct	seg *segkmap;		/* Kernel generic mapping segment */
struct	seg ktextseg;		/* Segment used for kernel executable image */
struct	seg kvalloc;		/* Segment used for "valloc" mapping */
struct seg *segkp;		/* Segment for pageable kernel virt. memory */
struct memseg *memseg_base;
struct	vnode unused_pages_vp;

extern char Sysbase[];
extern char Syslimit[];


#define	FOURGB	0x100000000LL
/*
 * VM data structures allocated early during boot.
 */
#define	KERNELMAP_SZ(frag)	\
	MAX(MMU_PAGESIZE,	\
	roundup((sizeof (struct map) * SYSPTSIZE/2/(frag)), MMU_PAGESIZE))

struct memlist *memlist;
caddr_t startup_alloc_vaddr = (caddr_t)SYSBASE + MMU_PAGESIZE;
u_int startup_alloc_size;
u_int	startup_alloc_chunk_size = 20 * MMU_PAGESIZE;	/* patchable */

extern caddr_t  dump_addr, cur_dump_addr; /* for dumping purposes */

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */
caddr_t modtext;	/* start of loadable module text reserved */
caddr_t e_modtext;	/* end of loadable module text reserved */
caddr_t extra_et;	/* MMU_PAGESIZE aligned after e_modtext */
uint64_t extra_etpa;	/* extra_et (phys addr) */
caddr_t moddata;	/* start of loadable module data reserved */
caddr_t e_moddata;	/* end of loadable module data reserved */

struct memlist *phys_install;	/* Total installed physical memory */
struct memlist *phys_avail;	/* Available (unreserved) physical memory */
struct memlist *virt_avail;	/* Available (unmapped?) virtual memory */
struct memlist *shadow_ram;	/* Non-dma-able memory XXX needs work */
uint64_t pmeminstall;		/* total physical memory installed */

static void memlist_add(uint64_t, uint64_t, struct memlist **,
	struct memlist **);
static void kphysm_init(machpage_t *, struct memseg *, pgcnt_t);

extern int segkmem_ready;

#define	IO_PROP_SIZE	64	/* device property size */

/*
 * Monitor pages may not be where this says they are.
 * and the debugger may not be there either.
 *
 *		  Physical memory layout
 *		(not necessarily contiguous)
 *
 *    availmem -+-----------------------+
 *		|			|
 *		|	page pool	|
 *		|			|
 *		|-----------------------|
 *		|   configured tables	|
 *		|	buffers		|
 *   firstaddr -|-----------------------|
 *		|   hat data structures |
 *		|-----------------------|
 *		|    kernel data, bss	|
 *		|-----------------------|
 *		|    interrupt stack	| ?????
 *		|-----------------------|
 *		|	kernel text	|
 *		+-----------------------+
 *
 *
 *		  Virtual memory layout.
 *		+-----------------------+
 *		|	psm 1-1 map	|
 *		|	exec args area	|
 * 0xFFC00000  -|-----------------------|- ARGSBASE
 *		|	debugger	|
 * 0xFF800000  -|-----------------------|- DEBUGSTART
 *		|	Hole		|
 * 0xFF000000  -|-----------------------|- Syslimit
 *		|	allocatable	|- phys_syslimit
 *		|    thru kernelmap	|
 * 0xF5400000  -|-----------------------|- Sysbase
 *		|	Ramdisk		|
 *		| (debugging only)	|	On a machine with largepage
 *		|-----------------------|			|
 *		|	segkmap		|			V
 *		|-----------------------|- eecontig	|-------| eecontig
 *		|			|		|	|
 *		|			| 4Mb		|segkp  |
 *		|	segkp		|  aligned ->	|-------| econtig
 *		|			|		| Hole	|
 *		|-----------------------|- econtig	|-------|
 *		|   page structures	|
 *		|	page hash	|
 *		|   memseg structures	|
 *		|-----------------------|- e_data
 *		|	kernel	 data	|
 * 0xE0800000  -|-----------------------|- s_data
 *		|	kernel	 text	|
 * 0xE0400000  -|-----------------------|- start
 *		|  user copy red zone	|
 *		|	(invalid)	|
 * 0xE0000000  -|-----------------------|- KERNELBASE
 *		|	user stack	|
 *		:			:
 *		:			:
 *		|	user data	|
 *		|-----------------------|
 *		|	user text	|
 * 0x00010000  -|-----------------------|
 *		|	invalid		|
 * 0x00000000	+-----------------------+
 */

struct _klwp lwp0;
struct proc p0;

void init_intr_threads(struct cpu *);
void init_clock_thread();
void init_panic_thread();

/* real-time-clock initialization parameters */
long gmt_lag;		/* offset in seconds of gmt to local time */
extern long process_rtc_config_file(void);

/*
 * The default size of segmap segment is 16Mb  this variable can be used
 * to change the size of segmap segment.
 */
int	segmaplen = SEGMAPSIZE;
u_int	phys_syslimit;
int	insert_into_pmemlist(struct memlist **, struct memlist *);
u_int	kernelbase = (u_int)_start;


/*
 * Space for low memory (below 16 meg), contiguous, memory
 * for DMA use (allocated by ddi_iopb_alloc()).  This default,
 * changeable in /etc/system, allows 2 64K buffers, plus space for
 * a few more small buffers for (e.g.) SCSI command blocks.
 */
long lomempages = (2*64*1024/PAGESIZE + 4);
kmutex_t hat_page_lock;

struct mmuinfo mmuinfo;
struct system_hardware system_hardware;

#ifdef  DEBUGGING_MEM
/*
 * Enable some debugging messages concerning memory usage...
 */
static int debugging_mem;
static void
printmemlist(char *title, struct memlist *mp)
{
	if (debugging_mem) {
		prom_printf("%s:\n", title);
		while (mp != 0)  {
			prom_printf("\tAddress 0x%x, size 0x%x\n",
			    mp->address, mp->size);
			mp = mp->next;
		}
	}
}
#endif	DEBUGGING_MEM

#define	BOOT_END	(3*1024*1024)	/* default to 3mb */

/*
 * Our world looks like this at startup time.
 *
 * Boot loads the kernel text at e0400000 and kernel data at e0800000.
 * Those addresses are fixed in the binary at link time. If this
 * machine supports large pages (4MB) then boot allocates a large
 * page for both text and data. If this machine supports global
 * pages, they are used also. If this machine is a 486 then lots of
 * little 4K pages are used.
 *
 * On the text page:
 * unix/genunix/krtld/module text loads.
 *
 * On the data page:
 * unix/genunix/krtld/module data loads and space for page_t's.
 */
/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	unsigned int i;
	pgcnt_t npages;
	pfn_t pfn;
	struct segmap_crargs a;
	int memblocks, memory_limit_warning;
	struct memlist *bootlistp, *previous, *current;
	u_int pp_giveback, pp_extra;
	caddr_t va;
	caddr_t memspace;
	caddr_t ndata_base;
	caddr_t ndata_end;
	size_t ndata_space;
	u_int memspace_sz, segmapbase = 0;
	u_int nppstr;
	u_int memseg_sz;
	u_int pagehash_sz;
	u_int memlist_sz;
	u_int kernelmap_sz;
	u_int pp_sz;			/* Size in bytes of page struct array */
	major_t major;
	page_t *lowpages_pp;
	int dbug_mem;
	uint64_t avmem, total_memory, highest_addr;
	uint64_t mmu_limited_physmax;
	int	max_virt_segkp;
	int	max_phys_segkp;
	int	segkp_len;
	int	b_ext;
	static char b_ext_prop[] = "bootops-extensions";
	caddr_t	boot_end;
	u_int	first_free_page;
	static char boot_end_prop[] = "boot-end";

	static void setup_kvpm();
	extern caddr_t mm_map;
	extern int cr4_value;
	extern int kobj_getpagesize();
	extern int ddi_load_driver(char *);
	extern void _db_install(void);
	extern void setup_mtrr(), setup_mca();
	extern void prom_setup(void);
	extern void hat_kern_setup(void);
	extern void memscrub_init(void);
	extern u_int va_to_pfn(u_int vaddr);

	extern void setx86isalist(void);

	kernelbase = MMU_L1_VA(MMU_L1_INDEX(kernelbase));
	(void) check_boot_version(BOP_GETVERSION(bootops));

	/*
	 * This kernel needs bootops extensions to be at least 1
	 * (for the 1275-ish functions).
	 *
	 * jhd: cachefs boot updates boot extensions to rev 2
	 */
	if ((BOP_GETPROPLEN(bootops, b_ext_prop) != sizeof (int)) ||
	    (BOP_GETPROP(bootops, b_ext_prop, &b_ext) < 0) ||
	    (b_ext < 2)) {
		prom_printf("Booting system too old for this kernel.\n");
		prom_panic("halting");
		/*NOTREACHED*/
	}

	/*
	 * BOOT PROTECT. Ask boot for its '_end' symbol - the
	 * first available address above boot. We use this info
	 * to protect boot until it is no longer needed.
	 */
	if ((BOP_GETPROPLEN(bootops, boot_end_prop) != sizeof (caddr_t)) ||
	    (BOP_GETPROP(bootops, boot_end_prop, &boot_end) < 0))
		first_free_page = mmu_btopr(BOOT_END);
	else
		first_free_page = mmu_btopr((u_int)boot_end);

	/*
	 * Initialize the mmu module
	 */
	mmu_init();

	pp_extra = mmuinfo.mmu_extra_pp_sz;

	/*
	 * Collect node, cpu and memory configuration information.
	 */
	get_system_configuration();

	setcputype();	/* mach/io/autoconf.c - cputype needs definition */

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

#if 0
	/*
	 * Add up how much physical memory /boot has passed us.
	 */
	phys_install = (struct memlist *)bootops->boot_mem->physinstalled;

	phys_avail = (struct memlist *)bootops->boot_mem->physavail;

	virt_avail = (struct memlist *)bootops->boot_mem->virtavail;

	/* XXX - what about shadow ram ???????????????? */
#endif
	/*
	 * the assumption here is, if we have large pages, we used
	 * them to load the kernel. So for x86 boxes that do not support
	 * large pages we have to be more circumspect in how we lay out
	 * the kernel address space.
	 */
	if (kobj_getpagesize()) {
		x86_feature |= X86_LARGEPAGE;
	}

	/*
	 * For MP machines cr4_value must be set or the other
	 * cpus will not be able to start.
	 */
	if (x86_feature & X86_LARGEPAGE)
		cr4_value = cr4();

	/*
	 * Initialize enough of the system to allow kmem_alloc
	 * to work by calling boot to allocate its memory until
	 * the time that kvm_init is completed.  The page structs
	 * are allocated after rounding up end to the nearest page
	 * boundary; kernelmap, and the memsegs are intialized and
	 * the space they use comes from the area managed by kernelmap.
	 * With appropriate initialization, they can be reallocated
	 * later to a size appropriate for the machine's configuration.
	 *
	 * At this point, memory is allocated for things that will never
	 * need to be freed, this used to be "valloced".  This allows a
	 * savings as the pages don't need page structures to describe
	 * them because them will not be managed by the vm system.
	 */
	ndata_base = (caddr_t)roundup((uintptr_t)e_data, MMU_PAGESIZE);
	if (x86_feature & X86_LARGEPAGE) {
		ndata_end = (caddr_t)roundup((uintptr_t)ndata_base,
			FOURMB_PAGESIZE);
		ndata_space = ndata_end - ndata_base;
	/*
	 * Reserve space for loadable modules.
	 */
		modtext = (caddr_t)roundup((uintptr_t)e_text, MMU_PAGESIZE);
		e_modtext = modtext + MODTEXT;
		extra_et = (caddr_t)roundup((uintptr_t)e_modtext, MMU_PAGESIZE);
		extra_etpa = va_to_pfn((u_int)extra_et);
		extra_etpa = extra_etpa << MMU_STD_PAGESHIFT;
		if (ndata_base + MODDATA < ndata_end) {
			moddata = ndata_base;
			e_moddata = moddata + MODDATA;
			ndata_base = e_moddata;
			ndata_space = ndata_end - ndata_base;
		}
	} else {
		/*
		 * No large pages so don't bother loading
		 * modules on them.
		 */
		modtext = e_modtext = e_text;
		moddata = e_moddata = e_data;
		extra_et = NULL;
		ndata_space = 0;
	}
	/*
	 * Remember what the physically available highest page is
	 * so that dumpsys works properly, and find out how much
	 * memory is installed.
	 */

	installed_top_size(bootops->boot_mem->physinstalled, &physmax,
	    &physinstalled, mmuinfo.mmu_highest_pfn);

	/*
	 * physinstalled is of type pgcnt_t. The macro ptob() relies
	 * on the type of argument passed.
	 * #define ptob(x)	((x) << PAGESHIFT)
	 */
	pmeminstall = ptob((unsigned long long)physinstalled);

	if (extra_et) {
		struct memlist *tmp;
		uint64_t seglen;

		seglen = (uint64_t)((caddr_t)roundup((uintptr_t)extra_et,
			FOURMB_PAGESIZE) - extra_et);

		if (ndata_space) {
			tmp = (struct memlist *)ndata_base;
			ndata_base += sizeof (*tmp);
			ndata_space = ndata_end - ndata_base;

			memlist_add(extra_etpa, seglen, &tmp,
				&bootops->boot_mem->physavail);
		}
#ifdef DEBUGGING_MEM
		if (debugging_mem)
			printmemlist("Add nucleus",
				bootops->boot_mem->physavail);
#endif	/* DEBUGGING_MEM */
	}

	/*
	 * Get the list of physically available memory to size
	 * the number of page structures needed.
	 * This is something of an overestimate, as it include
	 * the boot and any DMA (low continguous) reserved space.
	 */
	size_physavail(bootops->boot_mem->physavail, &npages, &memblocks,
	    mmuinfo.mmu_highest_pfn);

	/*
	 * If physmem is patched to be non-zero, use it instead of
	 * the monitor value unless physmem is larger than the total
	 * amount of memory on hand.
	 */
	if (physmem == 0 || physmem > npages)
		physmem = npages;
	else {
		npages = physmem;
	}


	/*
	 * The kernel address space has two contiguous regions of
	 * virtual space, KERNELBASE to eecontig and SYSBASE to 4G
	 * The resource map "kernelmap" starts from SYSBASE and goes
	 * up to Syslimit. Since we can not allocate more than physmem
	 * of memory, we limit Syslimit to SYSBASE + (physmem * 2)
	 * So on machines with less than 64Mb of memory we have hole
	 * between phys_syslimit and Syslimit
	 */


	/*
	 * total memory rounded to multiple of 4MB.
	 * physmem is of type pgcnt_t.
	 */

	total_memory = roundup(ptob((unsigned long long)physmem), PTSIZE);

	if (total_memory < 64 * 1024 * 1024) {
		phys_syslimit = SYSBASE + (total_memory * 2);
		if (phys_syslimit > (u_int)Syslimit)
			phys_syslimit = (u_int)Syslimit;
	} else phys_syslimit = (u_int)Syslimit;

	/*
	 * The page structure hash table size is a power of 2
	 * such that the average hash chain length is PAGE_HASHAVELEN.
	 */
	page_hashsz = npages / PAGE_HASHAVELEN;
	page_hashsz = 1 << highbit(page_hashsz);
	pagehash_sz = sizeof (struct page *) * page_hashsz;

	/*
	 * Some of the locks depend on page_hashsz being set!
	 */
	page_lock_init();

	/*
	 * The memseg list is for the chunks of physical memory that
	 * will be managed by the vm system.  The number calculated is
	 * a guess as boot may fragment it more when memory allocations
	 * are made before kvm_init(); twice as many are allocated
	 * as are currently needed.
	 */
	memseg_sz = sizeof (struct memseg) * (memblocks + POSS_NEW_FRAGMENTS);
	pp_sz = sizeof (struct machpage) * npages;
	memspace_sz = roundup(pagehash_sz + memseg_sz + pp_sz, MMU_PAGESIZE);

	/*
	 * We don't need page structs for the memory we are allocating
	 * so we subtract an appropriate amount.
	 */
	nppstr = btop(memspace_sz -
	    (btop(memspace_sz) * sizeof (struct machpage)));
	pp_giveback = nppstr * sizeof (struct machpage);

	memspace_sz -= pp_giveback;
	npages -= btopr(memspace_sz);
	pp_sz -= pp_giveback;

	if (x86_feature & X86_LARGEPAGE) {
		memspace = ndata_base;

		if (memspace_sz > ndata_space) {
			caddr_t tmp;
			tmp = (caddr_t)BOP_ALLOC(bootops, ndata_end,
				memspace_sz - ndata_space, BO_NO_ALIGN);
			if (tmp == NULL)
				cmn_err(CE_PANIC, "no space for page structs");
			/*
			 * We should have contiguous memory for our page structs
			 * now. From e_moddata out to the end of whatever
			 * BOP_ALLOC returned to us.
			 */
			ndata_space = 0;
		} else {
			ndata_base += memspace_sz;
			ndata_space = ndata_end - ndata_base;
		}
	} else {
		memspace = BOP_ALLOC(bootops, ndata_base, memspace_sz,
			BO_NO_ALIGN);
	}


	memspace_sz += (pp_extra * npages);
	page_hash = (struct page **)memspace;
	memseg_base = (struct memseg *)((u_int)page_hash + pagehash_sz);
	pp_base = (struct machpage *)((u_int)memseg_base + memseg_sz);
	mmuinfo.mmu_extra_ppp = (caddr_t)((u_int)pp_base + pp_sz);

	bzero(memspace, memspace_sz);

	/*
	 * econtig is the end of contiguous memory allocations. This
	 * comprises the area from _start() to e_moddata.
	 */
	if (x86_feature & X86_LARGEPAGE)
		econtig = (caddr_t)roundup((u_int)memspace + memspace_sz,
			FOURMB_PAGESIZE);
	else
		econtig = (caddr_t)roundup((u_int)memspace + memspace_sz,
			MMU_PAGESIZE);
	/*
	 * the memory lists from boot, and early versions of the kernelmap
	 * is allocated from the virtual address region managed by kernelmap
	 * so that later they can be freed and/or reallocated.
	 */
	memlist_sz = bootops->boot_mem->extent;
	/*
	 * Between now and when we finish copying in the memory lists,
	 * allocations happen so the space gets fragmented and the
	 * lists longer.  Leave enough space for lists twice as long
	 * as what boot says it has now; roundup to a pagesize.
	 */
	memlist_sz *= 2;
	memlist_sz = roundup(memlist_sz, MMU_PAGESIZE);
	kernelmap_sz = KERNELMAP_SZ(4);
	memspace_sz =  memlist_sz + kernelmap_sz;
	memspace = (caddr_t)BOP_ALLOC(bootops, startup_alloc_vaddr,
	    memspace_sz, BO_NO_ALIGN);
	startup_alloc_vaddr += memspace_sz;
	startup_alloc_size += memspace_sz;
	if (memspace == NULL)
		halt("Boot allocation failed.");
	bzero(memspace, memspace_sz);

	memlist = (struct memlist *)((u_int)memspace);
	kernelmap = (struct map *)((u_int)memlist + memlist_sz);

	mapinit(kernelmap, (size_t)btopr(phys_syslimit - (size_t)Sysbase) - 1,
		(u_long)1, "kernel map", kernelmap_sz / sizeof (struct map));

	mutex_enter(&maplock(kernelmap));
	if (rmget(kernelmap, btop(startup_alloc_size), 1) == 0)
		panic("can't make initial kernelmap allocation");
	mutex_exit(&maplock(kernelmap));

	/*
	 * take the most current snapshot we can by calling mem-update
	 */
	if (BOP_GETPROPLEN(bootops, "memory-update") == 0)
		BOP_GETPROP(bootops, "memory-update", NULL);

	phys_avail = current = previous = memlist;

	/*
	 * This block is used to copy the memory lists from boot's
	 * address space into the kernel's address space.  The lists
	 * represent the actual state of memory including boot and its
	 * resources.  kvm_init will use these lists to initialize the
	 * vm system.
	 */

	highest_addr = 0x1000000;
	bootlistp = bootops->boot_mem->physavail;
	for (; bootlistp; bootlistp = bootlistp->next) {

		if (bootlistp->address + bootlistp->size > FOURGB)
			continue;

		if (bootlistp->address + bootlistp->size > highest_addr)
			highest_addr = bootlistp->address + bootlistp->size;
	}

	/*
	 * Now copy the memlist into kernel space.
	 */
	mmu_limited_physmax = ptob((uint64_t)mmuinfo.mmu_highest_pfn + 1);
	bootlistp = bootops->boot_mem->physavail;
	memory_limit_warning = 0;
	for (; bootlistp; bootlistp = bootlistp->next) {
		/*
		 * Reserve page zero - see use of 'p0_va'
		 */
		if (bootlistp->address == 0) {
			if (bootlistp->size > PAGESIZE) {
				bootlistp->address += PAGESIZE;
				bootlistp->size -= PAGESIZE;
			} else
				continue;
		}

		if ((previous != current) && (bootlistp->address ==
		    previous->address + previous->size)) {
			/* coalesce */
			previous->size += bootlistp->size;
			if ((previous->address + previous->size) >
			    mmu_limited_physmax) {
				previous->size = mmu_limited_physmax -
				    previous->address;
				memory_limit_warning = 1;
			}
			continue;
		}
		if (bootlistp->address >= mmu_limited_physmax) {
			memory_limit_warning = 1;
			break;
		}
		current->address = bootlistp->address;
		current->size = bootlistp->size;
		current->next = (struct memlist *)0;
		if ((current->address + current->size) >
		    mmu_limited_physmax) {
			current->size = mmu_limited_physmax - current->address;
			memory_limit_warning = 1;
		}
		if (previous == current) {
			current->prev = (struct memlist *)0;
			current++;
		} else {
			current->prev = previous;
			previous->next = current;
			current++;
			previous++;
		}
	}

	/*
	 * Initialize the page structures from the memory lists.
	 */
	kphysm_init(pp_base, memseg_base, npages);

	availrmem_initial = availrmem = freemem;

	/*
	 * BOOT PROTECT. All of boots pages are now in the available
	 * page list, but we still have a few boot chores remaining.
	 * Acquire locks on boot pages before they get can get consumed
	 * by kmem_allocs after kvm_init() is called. Unlock the
	 * pages once boot is really gone.
	 */
	lowpages_pp = (page_t *)NULL;
	for (pfn = 0; pfn < first_free_page; pfn++) {
		page_t *pp;
		if ((pp = page_numtopp_alloc(pfn)) != NULL) {
			pp->p_next = lowpages_pp;
			lowpages_pp = pp;
		}
	}

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Initialize the kstat framework.
	 */
	kstat_init();

#if XXX_NEEDED
	/*
	 * we scale panic timeout by allowing 30 seconds for every
	 * 64 of memory.
	 */
	default_panic_timeout = 3000 * (physmax/0x4000 + 1);
#endif

	/*
	 * Lets display the banner early so the user has some idea that
	 * UNIX is taking over the system.
	 */
	cmn_err(CE_CONT,
	    "\rSunOS Release %s Version %s [UNIX(R) System V Release 4.0]\n",
	    utsname.release, utsname.version);
	cmn_err(CE_CONT, "Copyright (c) 1983-1998, Sun Microsystems, Inc.\n");
#ifdef DEBUG
	cmn_err(CE_CONT, "DEBUG enabled\n");
#endif
#ifdef TRACE
	cmn_err(CE_CONT, "TRACE enabled\n");
#endif
#ifdef GPROF
	cmn_err(CE_CONT, "GPROF enabled\n");
#endif

	if (memory_limit_warning)
		cmn_err(CE_WARN, "%s: limiting physical memory to %llX\n",
		    mmuinfo.mmu_name, mmu_limited_physmax);
	/*
	 * Initialize ten-micro second timer so that drivers will
	 * not get short changed in their init phase. This was
	 * not getting called until clkinit which, on fast cpu's
	 * caused the drv_usecwait to be way too short.
	 */
	microfind();

	/*
	 * Read system file, (to set maxusers, lomempages.......)
	 *
	 * Variables that can be set by the system file shouldn't be
	 * used until after the following initialization!
	 */
	mod_read_system_file(boothowto & RB_ASKNAME);

	/*
	 * Read the GMT lag from /etc/rtc_config.
	 */
	gmt_lag = process_rtc_config_file();

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers, yet allow to be overridden via the /etc/system file.
	 */
	param_calc(maxusers, 0);

	/*
	 * Initialize loadable module system, and apply the `set' commands
	 * gleaned from /etc/system.
	 */
	mod_setup();


	/*
	 * Setup MTRR registers in P6
	 */
	setup_mtrr();

	/*
	 * Setup machine check architecture on P6
	 */
	setup_mca();

	/*
	 * Initialize system parameters.
	 */
	param_init();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	setup_kernel_page_directory(CPU);


	/*
	 * For the purpose of setting up the kernel's page tables, which
	 * is done in hat_kern_setup(), we have to determine the size of
	 * the segkp segment, even though it hasn't been created yet.
	 * Later on seg_alloc() will creat the segkp segment starting at
	 * econtig.
	 */
	max_virt_segkp = btop(Sysbase - econtig);
	max_phys_segkp = (physmem * 2);
	/*
	 * We need to allocate segmap just after segkp
	 * segment. segmap has to be MAXBSIZE aligned. We also
	 * leave a hole between eecontig and Sysbase.
	 */
	max_virt_segkp -= (btop(segmaplen) + 4);

	segkp_len = ptob(MIN(max_virt_segkp, max_phys_segkp));
	eecontig = (caddr_t)((u_int)econtig + segkp_len);

	/* align across MAXBSIZE boundary */
	segmapbase = ((u_int)eecontig & ~(MAXBSIZE - 1)) + MAXBSIZE;
	eecontig = (caddr_t)(segmapbase + segmaplen);


	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

#if	NM_DEBUG
	add_nmintr(100, nmfunc1, "debug nmi 1", 99);
	add_nmintr(200, nmfunc1, "debug nmi 2", 199);
	add_nmintr(150, nmfunc1, "debug nmi 3", 149);
	add_nmintr(50, nmfunc1, "debug nmi 4", 49);
#endif

	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modloadonly("misc", "swapgeneric") == -1)
		halt("Can't load swapgeneric");

	dispinit();

#if 0
	/*
	 * Initialize the instance number data base--this must be done
	 * after mod_setup and before the bootops are given up
	 */

	e_ddi_instance_init();
#endif
	/*
	 * Make the in core copy of the prom tree - used for
	 * emulation of ieee 1275 boot environment
	 */
	prom_setup();
	setup_ddi();

	get_font_ptrs();

	console = establish_console();
	if (ddi_load_driver("kd") == DDI_FAILURE)
		halt("Can't load KD");
	if (console != CONSOLE_IS_FB)
		if (ddi_load_driver("asy") == DDI_FAILURE)
			halt("Can't load ASY");

	/*
	 * Lets take this opportunity to load the the root device.
	 */
	if (loadrootmodules() != 0)
		halt("Can't load the root filesystem");

	/*
	 * Load all platform specific modules
	 */
	psm_modload();

	/*
	 * Call back into boot and release boots resources.
	 */
	BOP_QUIESCE_IO(bootops);

	/*
	 * Copy physinstalled list into kernel space.
	 */
	phys_install = current;
	copy_memlist(bootops->boot_mem->physinstalled, &current);

	/*
	 * Virtual available next.
	 */
	virt_avail = current;
	copy_memlist(bootops->boot_mem->virtavail, &current);

	/*
	 * Copy in boot's page tables,
	 * set up extra page tables for the kernel,
	 * and switch to the kernel's context.
	 */
	hat_kern_setup();

	/*
	 * If we have more than 4Gb of memory, kmem_alloc's have
	 * to be restricted to less than 4Gb. Setup pp freelist
	 * for kvseg.
	 */
	mmu_setup_kvseg(physmax);

	/*
	 * Initialize VM system, and map kernel address space.
	 */
	kvm_init();


	if (x86_feature & X86_P5) {
		extern void setup_idt_base(caddr_t);
		extern void pentium_pftrap(void);
		extern struct gate_desc idt[];
		struct gdscr *pf_idtp;
		caddr_t new_idt_addr;

		new_idt_addr = kmem_zalloc(MMU_PAGESIZE, KM_NOSLEEP);
		if (new_idt_addr == NULL) {
			cmn_err(CE_PANIC, "failed to allocate a page"
				"for pentium bug workaround\n");
		}
		bcopy((caddr_t)&idt[0], (caddr_t)new_idt_addr,
			IDTSZ * sizeof (struct gate_desc));
		pf_idtp = (struct gdscr *)new_idt_addr;
		pf_idtp += T_PGFLT;
		pf_idtp->gd_off0015 = ((u_int)pentium_pftrap) & 0xffff;
		pf_idtp->gd_off1631 = ((u_int)pentium_pftrap >> 16);
		(void) as_setprot(&kas, new_idt_addr, MMU_PAGESIZE,
			(u_int)(PROT_READ|PROT_EXEC));
		(void) setup_idt_base((caddr_t)new_idt_addr);
		CPU->cpu_idt = (struct gate_desc *)new_idt_addr;
	}

	/*
	 * Map page 0 for drivers, such as kd, that need to pick up
	 * parameters left there by controllers/BIOS.
	 */
	p0_va = i86devmap(btop(0x0), 1, (PROT_READ));  /* 4K */

	/*
	 * Set up a map for translating kernel vitual addresses;
	 * used by dump and libkvm.
	 */
	setup_kvpm();

	/*
	 * Allocate a vm slot for the dev mem driver.
	 */
	i = rmalloc(kernelmap, (long)CLSIZE);
	mm_map = (caddr_t)kmxtob(i);

	i = rmalloc(kernelmap, DUMPPAGES);
	cur_dump_addr = dump_addr = kmxtob(i);

	/*
	 * If the following is true, someone has patched
	 * phsymem to be less than the number of pages that
	 * the system actually has.  Remove pages until system
	 * memory is limited to the requested amount.  Since we
	 * have allocated page structures for all pages, we
	 * correct the amount of memory we want to remove
	 * by the size of the memory used to hold page structures
	 * for the non-used pages.
	 */
	if (physmem < npages) {
		u_int diff;
		offset_t off;
		struct page *pp;
		caddr_t rand_vaddr;

		cmn_err(CE_WARN, "limiting physmem to %lu pages", physmem);

		off = 0;
		diff = npages - physmem;
		diff -= mmu_btopr(diff * sizeof (struct machpage));
		while (diff--) {
			rand_vaddr = (caddr_t)(((u_int)&unused_pages_vp >> 7) ^
						((u_offset_t)off >> PAGESHIFT));
			pp = page_create_va(&unused_pages_vp, off, MMU_PAGESIZE,
				PG_WAIT | PG_EXCL, &kas, rand_vaddr);
			if (pp == NULL)
				cmn_err(CE_PANIC, "limited physmem too much!");
			page_io_unlock(pp);
			page_downgrade(pp);
			availrmem--;
			off += MMU_PAGESIZE;
		}
	}

	/*
	 * When printing memory, show the total as physmem less
	 * that stolen by a debugger.
	 * XXX - do we know how much memory kadb uses?
	 */
	dbug_mem = 0;	/* XXX */
	cmn_err(CE_CONT, "?mem = %luK (0x%lx)\n",
	    (physinstalled - dbug_mem) << (PAGESHIFT - 10),
	    ptob(physinstalled - dbug_mem));

	avmem = ptob((unsigned long long)freemem);
	cmn_err(CE_CONT, "?avail mem = %lld\n", (unsigned long long)avmem);

	/*
	 * Initialize the segkp segment type.  We position it
	 * after the configured tables and buffers (whose end
	 * is given by econtig) and before Sysbase.
	 */

	va = econtig;

	rw_enter(&kas.a_lock, RW_WRITER);
	segkp = seg_alloc(&kas, va, segkp_len);
	if (segkp == NULL)
		cmn_err(CE_PANIC, "startup: cannot allocate segkp");
	if (segkp_create(segkp) != 0)
		cmn_err(CE_PANIC, "startup: segkp_create failed");
	rw_exit(&kas.a_lock);

	/*
	 * Now create generic mapping segment.  This mapping
	 * goes NCARGS beyond Syslimit up to DEBUGSTART.
	 * But if the total virtual address is greater than the
	 * amount of free memory that is available, then we trim
	 * back the segment size to that amount.
	 */
	va = (caddr_t)segmapbase;
	i = segmaplen;
	/*
	 * 1201049: segkmap base address must be MAXBSIZE aligned
	 */
	ASSERT(((u_int)va & MAXBOFFSET) == 0);

#ifdef XXX
	/*
	 * If there's a debugging ramdisk, we want to replace DEBUGSTART to
	 * the start of the ramdisk.
	 */
#endif XXX
	if (i > mmu_ptob(freemem))
		i = mmu_ptob(freemem);
	i &= MAXBMASK;	/* 1201049: segkmap size must be MAXBSIZE aligned */

	rw_enter(&kas.a_lock, RW_WRITER);
	segkmap = seg_alloc(&kas, va, i);
	if (segkmap == NULL)
		cmn_err(CE_PANIC, "cannot allocate segkmap");

	a.prot = PROT_READ | PROT_WRITE;
	a.shmsize = 0;
	a.nfreelist = 2;

	if (segmap_create(segkmap, (caddr_t)&a) != 0)
		panic("segmap_create segkmap");
	rw_exit(&kas.a_lock);

	setup_vaddr_for_ppcopy(CPU);


	/*
	 * DO NOT MOVE THIS BEFORE mod_setup() is called since
	 * _db_install() is in a loadable module that will be
	 * loaded on demand.
	 */
	{ extern int gdbon; if (gdbon) _db_install(); }

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Garbage collect any kmem-freed memory that really came from
	 * boot but was allocated before kvseg was initialized, and send
	 * it back into segkmem.
	 */

	kmem_gc();

	/*
	 * Startup memory scrubber.
	 */
	(void) memscrub_init();

	/*
	 * Configure the system.
	 */
	configure();		/* set up devices */

	/*
	 * Set the isa_list string to the defined instruction sets we
	 * support. Default to i386
	 */

	setx86isalist();

	init_intr_threads(CPU);

	{

		psm_install();

		init_clock_thread();

		if (console != CONSOLE_IS_FB) {
			struct dev_ops *ops;
			dev_info_t	*devi;
			major_t		maj;

			if ((major = ddi_name_to_major("asy")) == -1)
				goto asy_err;
			devi = devnamesp[major].dn_head;
			ops = devopsp[major];
			if (ops && devi && ddi_get_parent(devi)) {
				maj = ddi_name_to_major(
				    ddi_get_name(ddi_get_parent(devi)));
				if (maj != (major_t)-1) {
				    (void) ddi_hold_installed_driver(maj);
				}
			}

			if (ddi_hold_installed_driver(major) == NULL)
asy_err:
				halt("Can't hold installed driver for ASY");
		} else {
			if (((major = ddi_name_to_major("kd")) == -1) ||
			    (ddi_hold_installed_driver(major) == NULL))
				halt("Can't hold installed driver for KD");
		}

		kadb_uses_kernel();

		(*picinitf)();
		sti();

		(void) add_avsoftintr((void *)NULL, 1, softlevel1,
			"softlevel1", NULL); /* XXX to be moved later */
	}

	/*
	 * BOOT PROTECT. We are really done with boot
	 * now - unlock its pages.
	 */
	while (lowpages_pp) {
		page_t *pp;
		pp = lowpages_pp;
		lowpages_pp = pp->p_next;
		pp->p_next = (struct page *)0;
		page_free(pp, 1);
	}

	/* Boot pages available - allocate any needed low phys pages */

	/*
	 * Get 1 page below 1 MB so that other processors can boot up.
	 */
	for (pfn = 1; pfn < btop(1*1024*1024); pfn++) {
		if (page_numtopp_alloc(pfn) != NULL) {
			rm_platter_va = i86devmap(pfn, 1, PROT_READ|PROT_WRITE);
			rm_platter_pa = ptob(pfn);
			break;
		}
	}
	if (pfn == btop(1*1024*1024)) {
		cmn_err(CE_WARN,
		    "No page available for starting up auxiliary processors\n");
	}

	/*
	 * Allocate contiguous, memory below 16 mb
	 * with corresponding data structures to control its use.
	 */
	lomem_init();
}


#define	TBUF	1024

void
setx86isalist(void)
{
	char *tp;
	char *rp;
	size_t len;
	extern char *isa_list;

	tp = kmem_alloc(TBUF, KM_SLEEP);

	*tp = '\0';

	switch (cputype & CPU_ARCH) {
	/* The order of these case statements is very important! */

	case I86_P5_ARCH:
		if (x86_feature & X86_CMOV) {
			/* PentiumPro */
			(void) strcat(tp, "pentium_pro");
			(void) strcat(tp, (x86_feature & X86_MMX) ?
				"+mmx pentium_pro " : " ");
		}
		/* fall through to plain Pentium */
		(void) strcat(tp, "pentium");
		(void) strcat(tp, (x86_feature & X86_MMX) ?
			"+mmx pentium " : " ");
		/* FALLTHROUGH */

	case I86_486_ARCH:
		(void) strcat(tp, "i486 ");

		/* FALLTHROUGH */
	case I86_386_ARCH:
	default:
		(void) strcat(tp, "i386 ");

		/*
		 * We need this completely generic one to avoid
		 * confusion between a subdirectory of e.g. /usr/bin
		 * and the contents of /usr/bin e.g. /usr/bin/i386
		 */
		(void) strcat(tp, "i86");
	}

	/*
	 * Allocate right-sized buffer, copy temporary buf to it,
	 * and free temporary buffer.
	 */
	len = strlen(tp) + 1;   /* account for NULL at end of string */
	rp = kmem_alloc(len, KM_SLEEP);
	if (rp == NULL)
		return;
	isa_list = strcpy(rp, tp);
	kmem_free(tp, TBUF);

}

extern char hw_serial[];
char *_hs1107 = hw_serial;
ulong_t  _bdhs34;

void
post_startup(void)
{
	int sysinitid;

	if ((sysinitid = modload("misc", "sysinit")) != -1)
		(void) modunload(sysinitid);
	else
		cmn_err(CE_CONT, "sysinit load failed");

	/*
	 * Perform forceloading tasks for /etc/system.
	 */
	(void) mod_sysctl(SYS_FORCELOAD, NULL);
	/*
	 * complete mmu initialization, now that kernel and critical
	 * modules have been loaded.
	 */
	(void) post_startup_mmu_initialization();

	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	/*
	 * Load the floating point emulator if necessary.
	 */
	if (fp_kind == FP_NO) {
		if (modload("misc", "emul_80387") == -1)
			cmn_err(CE_CONT, "No FP emulator found\n");
		cmn_err(CE_CONT, "FP hardware will %sbe emulated by software\n",
			fp_kind == FP_SW ? "" : "not ");
	}

	maxmem = freemem;

	(void) spl0();		/* allow interrupts */
}

/*
 * kphysm_init() initializes physical memory.
 */
static void
kphysm_init(machpage_t *pp, struct memseg *memsegp, pgcnt_t npages)
{
	struct memlist *pmem;
	struct memseg *cur_memseg;
	struct memseg *tmp_memseg;
	struct memseg **prev_memsegp;
	u_long pnum;
	extern void page_coloring_init(void);

	reload_cr3();
	ASSERT(page_hash != NULL && page_hashsz != 0);

	page_coloring_init();

	cur_memseg = memsegp;
	for (pmem = phys_avail; pmem && npages;
	    pmem = pmem->next, cur_memseg++) {
		u_long base;
		u_int num;

		/*
		 * Build the memsegs entry
		 */
		num = btop(pmem->size);
		if (num > npages)
			num = npages;

		npages -= num;
		base = btop(pmem->address);

		cur_memseg->pages = pp;
		cur_memseg->epages = pp + num;
		cur_memseg->pages_base = base;
		cur_memseg->pages_end = base + num;

		/* insert in memseg list, decreasing number of pages order */
		for (prev_memsegp = &memsegs, tmp_memseg = memsegs;
		    tmp_memseg;
		    prev_memsegp = &(tmp_memseg->next),
		    tmp_memseg = tmp_memseg->next) {
			if (num > tmp_memseg->pages_end -
			    tmp_memseg->pages_base)
				break;
		}
		cur_memseg->next = *prev_memsegp;
		*prev_memsegp = cur_memseg;

		/*
		 * Initialize the PSM part of the page struct
		 */
		pnum = cur_memseg->pages_base;
		for (pp = cur_memseg->pages; pp < cur_memseg->epages; pp++) {
			psm_pageinit(pp, pnum);
			pnum++;
		}

		/*
		 * have the PIM initialize things for this
		 * chunk of physical memory.
		 */
		add_physmem((page_t *)cur_memseg->pages, num);
	}

	build_pfn_hash();
}

/*
 * Kernel VM initialization.
 */
static void
kvm_init(void)
{
	register caddr_t va;
	u_int range_size, range_base, range_end;
	u_int prot;
	struct memlist *cur, *prev;
	caddr_t valloc_base = (caddr_t)roundup((uintptr_t)e_data, MMU_PAGESIZE);

	extern void _start();

	/*
	 * Put the kernel segments in kernel address space.  Make it a
	 * "kernel memory" segment objects.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

	(void) seg_attach(&kas, (caddr_t)kernelbase,
	    (uintptr_t)e_data - (uintptr_t)kernelbase, &ktextseg);
	(void) segkmem_create(&ktextseg, (caddr_t)NULL);

	(void) seg_attach(&kas, valloc_base, (uintptr_t)econtig -
	    (uintptr_t)valloc_base, &kvalloc);
	(void) segkmem_create(&kvalloc, (caddr_t)NULL);

	/*
	 * We're about to map out /boot.  This is the beginning of the
	 * system resource management transition. We can no longer
	 * call into /boot for I/O or memory allocations.
	 */
	(void) seg_attach(&kas, (caddr_t)SYSBASE,
	    (u_int)(phys_syslimit - SYSBASE), &kvseg);
	(void) segkmem_create(&kvseg, (caddr_t)NULL);

	rw_exit(&kas.a_lock);

	rw_enter(&kas.a_lock, RW_READER);

	/*
	 * Now we can ask segkmem for memory instead of boot.
	 */
	segkmem_ready = 1;

	/*
	 * All level 1 entries other than the kernel were set invalid
	 * when our prototype level 1 table was created.  Thus, we only need
	 * to deal with addresses above kernelbase here.  Also, all ptes
	 * for this region have been allocated and locked, or they are not
	 * used.  Thus, all we need to do is set protections.  Invalid until
	 * start.
	 */
	ASSERT((((u_int)_start) & PAGEOFFSET) == 0);
	for (va = (caddr_t)kernelbase; va < (caddr_t)_start; va += PAGESIZE) {
		/* user copy red zone */
		(void) as_setprot(&kas, va, PAGESIZE, 0);
	}

	prot = PROT_READ | PROT_EXEC;
	prot |= (kernprot) ? 0 : PROT_WRITE;

	/*
	 * (Normally) Read-only until end of text.
	 */
	(void) as_setprot(&kas, va, (size_t)(e_modtext - kernelbase), prot);

	va = s_data;

	/*
	 * Writable until end.
	 */
	(void) as_setprot(&kas, va, (u_int)(econtig - va),
	    PROT_READ | PROT_WRITE | PROT_EXEC);

	/*
	 * Validate to Syslimit.  Memory allocations done early on
	 * by boot are in this region.
	 */
	for (cur = virt_avail; cur->address < (u_int)startup_alloc_vaddr;
		cur = cur->next)
		prev = cur;

	range_base = prev->address + prev->size;
	range_size = cur->address - range_base;

	(void) as_setprot(&kas, (caddr_t)range_base, range_size,
	    PROT_READ | PROT_WRITE | PROT_EXEC);

	range_end = range_base + range_size;
	va = (caddr_t)roundup(range_end, PAGESIZE);

	/*
	 * Invalidate unused portion of the region managed by kernelmap.
	 */
	(void) as_setprot(&kas, va, phys_syslimit - (u_int)va, 0);

	rw_exit(&kas.a_lock);

	/*
	 * Flush the PDC of any old mappings.
	 */
	mmu_tlbflush_all();

}

/*
 * crash dump, libkvm support - see sys/kvtopdata.h for details
 */
struct kvtopdata kvtopdata;

static void
setup_kvpm(void)
{
	caddr_t va;
	pfn_t pfn;
	int i = 0;
	u_int pages = 0;
	pfn_t lastpfnum;

	lastpfnum = PFN_INVALID;

	for (va = (caddr_t)kernelbase; va < econtig; va += PAGESIZE) {
		pfn = hat_getpfnum(kas.a_hat, va);
		if (pfn != PFN_INVALID) {
			if (lastpfnum == PFN_INVALID) {
				lastpfnum = pfn;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfn) {
				lastpfnum = pfn;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfn;
				pages = 1;
				if (++i > NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != PFN_INVALID) {
			lastpfnum = PFN_INVALID;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			if (++i > NKVTOPENTS) {
				cmn_err(CE_WARN, "out of kvtopents");
				break;
			}
		}
	}
	/*
	 * Pages allocated early from sysmap region that don't
	 * have page structures and need to be entered in to the
	 * kvtop array for libkvm.  The rule for memory pages is:
	 * it is either covered by a page structure or included in
	 * kvtopdata.
	 */
	for (va = Sysbase; va < startup_alloc_vaddr; va += PAGESIZE) {
		pfn = hat_getpfnum(kas.a_hat, va);
		if (pfn != PFN_INVALID) {
			if (lastpfnum == PFN_INVALID) {
				lastpfnum = pfn;
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
				pages = 1;
			} else if (lastpfnum+1 == pfn) {
				lastpfnum = pfn;
				pages++;
			} else {
				kvtopdata.kvtopmap[i].kvpm_len = pages;
				lastpfnum = pfn;
				pages = 1;
				if (++i > NKVTOPENTS) {
					cmn_err(CE_WARN, "out of kvtopents");
					break;
				}
				kvtopdata.kvtopmap[i].kvpm_vaddr = va;
				kvtopdata.kvtopmap[i].kvpm_pfnum = lastpfnum;
			}
		} else if (lastpfnum != PFN_INVALID) {
			lastpfnum = PFN_INVALID;
			kvtopdata.kvtopmap[i].kvpm_len = pages;
			pages = 0;
			if (++i > NKVTOPENTS) {
				cmn_err(CE_WARN, "out of kvtopents");
				break;
			}
		}
	}

	if (pages)
		kvtopdata.kvtopmap[i].kvpm_len = pages;
	else
		i--;

	kvtopdata.hdr.version = KVTOPD_VER;
	kvtopdata.hdr.nentries = i + 1;
	kvtopdata.hdr.pagesize = MMU_PAGESIZE;


#ifdef XXX
	pfn = hat_getpfnum(kas.a_hat, (caddr_t)&kvtopdata);
/* msg_map?  msgbuf has been redefined. */
	msgbuf.msg_map = (tpte->PhysicalPageNumber << MMU_PAGESHIFT) |
		((u_int)(&kvtopdata) & MMU_PAGEOFFSET);
#endif XXX
}

static int
establish_console(void)
{
	char idbuf[IO_PROP_SIZE], odbuf[IO_PROP_SIZE];
	int inkey, outfb;
	int cons = CONSOLE_IS_FB;

	/*
	 * Because of the problems matching instance numbers to
	 * ports (there's no guarantee of the device order) no
	 * longer allow the console variable to be set via
	 * /etc/system
	 */
	if (console != CONSOLE_IS_FB && console <= 2) {
		cmn_err(CE_WARN,
		    "Setting the console variable is no "
		    "longer valid. Use the eeprom(1M)\n"
		    "command to change the input-device "
		    "and output-device");
		console = 0;
		/*
		 * fall through here so that if the user has also
		 * set the input/output-device properties everything
		 * will still work.
		 */
	}

	/*
	 * We need to return CONSOLE_IS_FB, 1, or 2;
	 * (for the physical console, or COM1,2 respectively).
	 *
	 * If stdin is already coming from the keyboard or stdout is
	 * already going to the screen, then we assume no console
	 * redirection is desired.  If one of in/out is set for
	 * redirection but not the other, we warn the user that we
	 * aren't going to redirect.
	 */
	inkey = prom_stdin_is_keyboard();
	outfb = prom_stdout_is_framebuffer();
	if (inkey || outfb) {
		if (!inkey || !outfb) {
			cmn_err(CE_WARN, "Console NOT redirected, "
			    "input-device and output-device do not match.");
		}
		return (CONSOLE_IS_FB);
	}
	/*
	 * Get "output-device" and "input-device".  Set console to 1 or 2
	 * if the output-device is set to "tty[ab]" or "com[12]", AND if
	 * the input-device matches the output device.
	 */
	if (prom_getprop(prom_optionsnode(), "output-device", odbuf) > 0 &&
	    prom_getprop(prom_optionsnode(), "input-device", idbuf) > 0) {
		if (strncmp(odbuf, idbuf, 4) == 0) {
			if (strncmp(odbuf, "ttya", 4) == 0 ||
			    strncmp(odbuf, "com1", 4) == 0) {
				cons = CONSOLE_IS_COM1;
			} else if (strncmp(odbuf, "ttyb", 4) == 0 ||
				strncmp(odbuf, "com2", 4) == 0) {
				cons = CONSOLE_IS_COM2;
			} else {
				cmn_err(CE_WARN, "Console NOT redirected.");
				cmn_err(CE_CONT, "In order to redirect "
				    "console, output-device and input-device "
				    "must be identical\n");
				cmn_err(CE_CONT, "and one of: ttya, ttyb, "
				    "com1, or com2.\n");
			}
		} else {
			cmn_err(CE_WARN, "Console NOT redirected, "
			    "input-device and output-device do not match.");
		}
	}
	return (cons);
}

int
insert_into_pmemlist(head, cur)
struct 	memlist **head;
struct	memlist	*cur;
{
	struct memlist *tmemlist, *ptmemlist;

	if (*head == (struct memlist *)0) {
		*head = cur;
		cur->next = (struct memlist *)0;
	} else {
		tmemlist = *head;
		ptmemlist = (struct memlist *)0;
		while (tmemlist) {
			if (cur->address < tmemlist->address) {
				if (((u_int)cur->address + cur->size) ==
				    tmemlist->address) {
					tmemlist->address = cur->address;
					tmemlist->size += cur->size;
					return (0);
				} else break;
			} else if (cur->address == ((u_int)tmemlist->address +
				tmemlist->size)) {
				tmemlist->size += cur->size;
				return (0);
			}
			ptmemlist = tmemlist;
			tmemlist = tmemlist->next;
		}
		if (tmemlist == (struct memlist *)0) {
			/* get to the tail of the list */
			ptmemlist->next = cur;
			cur->next = (struct memlist *)0;
		} else if (ptmemlist == (struct memlist *)0) {
			/* get to the head of the list */
			cur->next = *head;
			*head = cur;
		} else {
			/* insert in between */
			cur->next = ptmemlist->next;
			ptmemlist->next = cur;
		}
	}
	return (1);
}

/*
 * These are MTTR registers supported by P6
 */
static struct	mtrrvar	mtrrphys_arr[MAX_MTRRVAR];
static uint64_t mtrr64k, mtrr16k1, mtrr16k2;
static uint64_t mtrr4k1, mtrr4k2, mtrr4k3;
static uint64_t mtrr4k4, mtrr4k5, mtrr4k6;
static uint64_t mtrr4k7, mtrr4k8, mtrrcap;
static uint64_t mtrrdef;

static uint64_t mtrr_size1, mtrr_base1;
static uint64_t mtrr_size2, mtrr_base2;
static uint32_t mtrr_type1, mtrr_type2;

static u_int	mci_ctl[] = {REG_MC0_CTL, REG_MC1_CTL, REG_MC2_CTL,
		    REG_MC3_CTL, REG_MC4_CTL};
static u_int	mci_status[] = {REG_MC0_STATUS, REG_MC1_STATUS, REG_MC2_STATUS,
		    REG_MC3_STATUS, REG_MC4_STATUS};
static u_int	mci_addr[] = {REG_MC0_ADDR, REG_MC1_ADDR, REG_MC2_ADDR,
		    REG_MC3_ADDR, REG_MC4_ADDR};
static int	mca_cnt;


void
setup_mca()
{
	int 		i;
	uint64_t	allzeros;
	uint64_t	allones;
	uint64_t	buf;
	long long	mca_cap;

	if (!(x86_feature & X86_MCA))
		return;
	(void) rdmsr(REG_MCG_CAP, &buf);
	mca_cap = *(long long *)buf;
	allones = 0xffffffffffffffffULL;
	if (mca_cap & MCG_CAP_CTL_P)
		(void) wrmsr(REG_MCG_CTL, &allones);
	mca_cnt = mca_cap & MCG_CAP_COUNT_MASK;
	if (mca_cnt > P6_MCG_CAP_COUNT)
		mca_cnt = P6_MCG_CAP_COUNT;
	for (i = 1; i < mca_cnt; i++)
		(void) wrmsr(mci_ctl[i], &allones);
	allzeros = 0;
	for (i = 0; i < mca_cnt; i++)
		(void) wrmsr(mci_status[i], &allzeros);
	setcr4(cr4()|CR4_MCE);

}
int
mca_exception(struct regs *rp)
{
	uint64_t	status, addr;
	uint64_t	allzeros;
	uint64_t	buf;
	int		i, ret = 1, errcode, mserrcode;

	allzeros = 0;
	(void) rdmsr(REG_MCG_STATUS, &buf);
	status = buf;
	if (status & MCG_STATUS_RIPV)
		ret = 0;
	if (status & MCG_STATUS_EIPV)
		cmn_err(CE_NOTE, "MCE at %x\n", rp->r_eip);
	(void) wrmsr(REG_MCG_STATUS, &allzeros);
	for (i = 0; i < mca_cnt; i++) {
		(void) rdmsr(mci_status[i], &buf);
		status = buf;
		/*
		 * If status register not valid skip this bank
		 */
		if (!(status & MCI_STATUS_VAL))
			continue;
		errcode = status & MCI_STATUS_ERRCODE;
		mserrcode = (status  >> MSERRCODE_SHFT) & MCI_STATUS_ERRCODE;
		if (status & MCI_STATUS_ADDRV) {
			/*
			 * If mci_addr contains the address where
			 * error occurred, display the address
			 */
			(void) rdmsr(mci_addr[i], &buf);
			addr = buf;
			cmn_err(CE_NOTE, "MCE: Bank %d: error code %x:"\
			    "addr = %llx, model errcode = %x\n", i,
			    errcode, addr, mserrcode);
		} else {
			cmn_err(CE_NOTE,
			    "MCE: Bank %d: error code %x, mserrcode = %x\n",
			    i, errcode, mserrcode);
		}
		(void) wrmsr(mci_status[i], &allzeros);
	}
	return (ret);
}


void
setup_mtrr()
{
	int i, ecx;
	int empty_slot, vcnt;
	struct	mtrrvar	*mtrrphys;
	int	mtrr_fix();

	if (!(x86_feature & X86_MTRR))
		return;

	(void) rdmsr(REG_MTRRCAP, &mtrrcap);
	(void) rdmsr(REG_MTRRDEF, &mtrrdef);
	if (mtrrcap & MTRRCAP_FIX) {
		(void) rdmsr(REG_MTRR64K, &mtrr64k);
		(void) rdmsr(REG_MTRR16K1, &mtrr16k1);
		(void) rdmsr(REG_MTRR16K2, &mtrr16k2);
		(void) rdmsr(REG_MTRR4K1, &mtrr4k1);
		(void) rdmsr(REG_MTRR4K2, &mtrr4k2);
		(void) rdmsr(REG_MTRR4K3, &mtrr4k3);
		(void) rdmsr(REG_MTRR4K4, &mtrr4k4);
		(void) rdmsr(REG_MTRR4K5, &mtrr4k5);
		(void) rdmsr(REG_MTRR4K6, &mtrr4k6);
		(void) rdmsr(REG_MTRR4K7, &mtrr4k7);
		(void) rdmsr(REG_MTRR4K8, &mtrr4k8);
	}
	if ((vcnt = (mtrrcap & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;

	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		(void) rdmsr(ecx, &mtrrphys->mtrrphys_base);
		(void) rdmsr(ecx + 1, &mtrrphys->mtrrphys_mask);

	}

	if (!mtrr_size1 && !mtrr_size2)
		return;
	else if (!mtrr_size1) {
		mtrr_size1 = mtrr_size2;
		mtrr_base1 = mtrr_base2;
		mtrr_type1 = mtrr_type2;
		mtrr_size2 = 0;
	}

	empty_slot = 0;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		if (!(mtrrphys->mtrrphys_mask & MTRRPHYSMASK_V)) {
			empty_slot = 1;
			break;
		}
	}
	if (!empty_slot) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, no free slot\n");
		return;
	}
	if ((mtrr_base1 & PAGEOFFSET) || (mtrr_size1 & PAGEOFFSET)) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, Unaligned address");
		return;
	}
	MTRR_SETVBASE(mtrrphys->mtrrphys_base, mtrr_base1, mtrr_type1);
	MTRR_SETVMASK(mtrrphys->mtrrphys_mask, mtrr_size1, 1);

	while (mtrr_fix(vcnt));

	if (!mtrr_size2)
		goto load_mtrr;

	empty_slot = 0;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		if (!(mtrrphys->mtrrphys_mask & MTRRPHYSMASK_V)) {
			empty_slot = 1;
			break;
		}
	}
	if (!empty_slot) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, no free slot\n");
		return;
	}
	if ((mtrr_base2 & PAGEOFFSET) || (mtrr_size2 & PAGEOFFSET)) {
		cmn_err(CE_WARN, "Cannot Pragram MTRR, Unaligned address");
		return;
	}
	MTRR_SETVBASE(mtrrphys->mtrrphys_base, mtrr_base2, mtrr_type2);
	MTRR_SETVMASK(mtrrphys->mtrrphys_mask, mtrr_size2, 1);

	while (mtrr_fix(vcnt));
load_mtrr:

	mtrr_sync();
}


void
mtrr_warning(struct mtrrvar *mtrrphys, int otype)
{
	uint64_t	base, size;
	uint32_t	ntype;

	base = MTRR_GETVBASE(mtrrphys->mtrrphys_base);
	ntype = MTRR_GETVTYPE(mtrrphys->mtrrphys_base);
	size = MTRR_GETVSIZE(mtrrphys->mtrrphys_mask);


	cmn_err(CE_WARN, "MTRR for range %lld to %lld changing from %d to %d\n",
		base, base + size, otype, ntype);
}
int
mtrr_fix(int vcnt)
{
	int 	i, j,  ecx, type, ttype;
	int 	tecx;
	struct	mtrrvar	*mt, *tmt;
	uint64_t base, size;
	uint64_t tbase, tsize;


	for (i = 0, ecx = REG_MTRRPHYSBASE0, mt = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mt++) {
		if (!(mt->mtrrphys_mask & MTRRPHYSMASK_V))
			continue;
		base = MTRR_GETVBASE(mt->mtrrphys_base);
		size = MTRR_GETVSIZE(mt->mtrrphys_mask);
		type = MTRR_GETVTYPE(mt->mtrrphys_base);
		for (j = 0, tecx = REG_MTRRPHYSBASE0, tmt = mtrrphys_arr;
			j <  vcnt - 1; j++, tecx += 2, tmt++) {

			if (mt == tmt)
				continue;
			else if (!(tmt->mtrrphys_mask & MTRRPHYSMASK_V))
				continue;
			tbase = MTRR_GETVBASE(tmt->mtrrphys_base);
			tsize = MTRR_GETVSIZE(tmt->mtrrphys_mask);
			ttype = MTRR_GETVTYPE(tmt->mtrrphys_base);

			if ((base < tbase && base + size < tbase) ||
				base > tbase + tsize)
				continue;
			if (base > tbase) {
				if (base + size < tbase + tsize) {
					if (type < ttype) {
						MTRR_SETTYPE(
						tmt->mtrrphys_base, type);
						mtrr_warning(tmt, ttype);
					}
					MTRR_SETVINVALID(mt->mtrrphys_mask);
					return (1);
				}
				tsize -= (base - tbase);
				MTRR_SETVMASK(tmt->mtrrphys_mask, tsize, 1);
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(tmt, type);
				}
				return (1);
			} else if (base + size > tbase + tsize) {
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(mt, type);
				}
				MTRR_SETVINVALID(tmt->mtrrphys_mask);
				return (1);
			} else {
				tsize -= (base + size - tbase);
				tbase = base + size;
				MTRR_SETVBASE(tmt->mtrrphys_base, tbase, ttype);
				MTRR_SETVMASK(tmt->mtrrphys_mask, tsize, 1);
				if (ttype < type) {
					MTRR_SETTYPE(mt->mtrrphys_base, ttype);
					mtrr_warning(mt, type);
				}
				return (1);
			}
		}
	}
	return (0);
}
/*
 * Sync current cpu mtrr with the incore copy of mtrr.
 * This function hat to be invoked with interrupts disabled
 * Currently we do not capture other cpu's. This is invoked on cpu0
 * just after reading /etc/system.
 * On other cpu's its invoked from mp_startup().
 */
void
mtrr_sync()
{
	uint64_t my_mtrrdef;
	u_int	crvalue, cr0_orig;
	extern	invalidate_cache();
	int	vcnt, i, ecx;
	struct	mtrrvar	*mtrrphys;

	cr0_orig = crvalue = cr0();
	crvalue |= CR0_CD;
	crvalue &= ~CR0_NW;
	setcr0(crvalue);
	invalidate_cache();
	setcr3(cr3());

	(void) rdmsr(REG_MTRRDEF, &my_mtrrdef);
	my_mtrrdef &= ~MTRRDEF_E;
	(void) wrmsr(REG_MTRRDEF, &my_mtrrdef);
	if (mtrrcap & MTRRCAP_FIX) {
		(void) wrmsr(REG_MTRR64K, &mtrr64k);
		(void) wrmsr(REG_MTRR16K1, &mtrr16k1);
		(void) wrmsr(REG_MTRR16K2, &mtrr16k2);
		(void) wrmsr(REG_MTRR4K1, &mtrr4k1);
		(void) wrmsr(REG_MTRR4K2, &mtrr4k2);
		(void) wrmsr(REG_MTRR4K3, &mtrr4k3);
		(void) wrmsr(REG_MTRR4K4, &mtrr4k4);
		(void) wrmsr(REG_MTRR4K5, &mtrr4k5);
		(void) wrmsr(REG_MTRR4K6, &mtrr4k6);
		(void) wrmsr(REG_MTRR4K7, &mtrr4k7);
		(void) wrmsr(REG_MTRR4K8, &mtrr4k8);
	}
	if ((vcnt = (mtrrcap & MTRRCAP_VCNTMASK)) > MAX_MTRRVAR)
		vcnt = MAX_MTRRVAR;
	for (i = 0, ecx = REG_MTRRPHYSBASE0, mtrrphys = mtrrphys_arr;
		i <  vcnt - 1; i++, ecx += 2, mtrrphys++) {
		(void) wrmsr(ecx, &mtrrphys->mtrrphys_base);
		(void) wrmsr(ecx + 1, &mtrrphys->mtrrphys_mask);
	}
	(void) wrmsr(REG_MTRRDEF, &mtrrdef);
	setcr3(cr3());
	invalidate_cache();
	setcr0(cr0_orig);
}

void
get_system_configuration()
{
	char	prop[32];
	uint64_t nodes_ll, cpus_pernode_ll;
	extern int getvalue(char *token, uint64_t *valuep);


	if (((BOP_GETPROPLEN(bootops, "nodes") > sizeof (prop)) ||
		(BOP_GETPROP(bootops, "nodes", prop) < 0) 	||
		(getvalue(prop, &nodes_ll) == -1) ||
		(nodes_ll > MAXNODES))			   ||
	    ((BOP_GETPROPLEN(bootops, "cpus_pernode") > sizeof (prop)) ||
		(BOP_GETPROP(bootops, "cpus_pernode", prop) < 0) ||
		(getvalue(prop, &cpus_pernode_ll) == -1))) {

		system_hardware.hd_nodes = 1;
		system_hardware.hd_cpus_per_node = 0;
	} else {
		system_hardware.hd_nodes = (int)nodes_ll;
		system_hardware.hd_cpus_per_node = (int)cpus_pernode_ll;
	}
}

/*
 * Add to a memory list.
 * start = start of new memory segment
 * len = length of new memory segment in bytes
 * memlistp = pointer to array of available memory segment structures
 * curmemlistp = memory list to which to add segment.
 */
static void
memlist_add(uint64_t start, uint64_t len, struct memlist **memlistp,
	struct memlist **curmemlistp)
{
	struct memlist *cur, *new, *last;
	uint64_t end = start + len;

	new = *memlistp;

	new->address = start;
	new->size = len;
	*memlistp = new + 1;

	for (cur = *curmemlistp; cur; cur = cur->next) {
		last = cur;
		if (cur->address >= end) {
			new->next = cur;
			new->prev = cur->prev;
			cur->prev = new;
			if (cur == *curmemlistp)
				*curmemlistp = new;
			else
				new->prev->next = new;
			return;
		}
		if (cur->address + cur->size > start)
			panic("munged memory list = 0x%x\n", curmemlistp);
	}
	new->next = NULL;
	new->prev = last;
	last->next = new;
}
