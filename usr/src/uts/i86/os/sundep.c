/*
 * Copyright (c) 1995-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*	All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)sundep.c	1.87	98/02/04 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/archsystm.h>
#include <sys/vmparam.h>
#include <sys/prsystm.h>
#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/msgbuf.h>
#include <sys/session.h>
#include <sys/ucontext.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/debugreg.h>
#include <sys/thread.h>
#include <sys/vtrace.h>
#include <sys/consdev.h>
#include <sys/psw.h>
#include <sys/reg.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/swap.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <sys/exec.h>
#include <sys/acct.h>
#include <sys/core.h>
#include <sys/modctl.h>
#include <sys/tuneable.h>
#include <c2/audit.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>
#include <sys/dumphdr.h>
#include <sys/systeminfo.h>

extern void prom_printf(char *fmt, ...);
extern void prom_enter_mon(void);
extern void prom_panic();

int is_coff_proc(register proc_t *p);

/* platform-specific cpu partition hook for fastbuildstack */
void (*cpupart_exec_hook)() = NULL;

/*
 * Compare the version of boot that boot says it is against
 * the version of boot the kernel expects.
 */
int
check_boot_version(int boots_version)
{
	if (boots_version == BO_VERSION)
		return (0);

	prom_printf("Wrong boot interface - kernel needs v%d found v%d\n",
	    BO_VERSION, boots_version);
	prom_panic("halting");
	/*NOTREACHED*/
}

/*
 * Count the number of available pages and the number of
 * chunks in the list of available memory.
 */
void
size_physavail(
	struct memlist	*physavail,
	pgcnt_t		*npages,
	int		*memblocks,
	pfn_t		last_pfn)
{
	uint64_t mmu_limited_physmax;

	*npages = 0;
	*memblocks = 0;
	mmu_limited_physmax = ptob((uint64_t)last_pfn + 1);
	for (; physavail; physavail = physavail->next) {
		if (physavail->address >= mmu_limited_physmax)
			continue;
		if (physavail->address + physavail->size < mmu_limited_physmax)
		    *npages += (pgcnt_t)btop(physavail->size);
		else
		    *npages += (pgcnt_t)
			btop((mmu_limited_physmax - physavail->address));
		(*memblocks)++;
	}
}

/*
 * Copy boot's physavail list deducting memory at "start"
 * for "size" bytes.
 */
int
copy_physavail(
	struct memlist	*src,
	struct memlist	**dstp,
	u_int		start,
	u_int		size)
{
	struct memlist *dst, *prev;
	u_int end1;
	int deducted = 0;

	dst = *dstp;
	prev = dst;
	end1 = start + size;

	for (; src; src = src->next) {
		u_longlong_t addr, lsize, end2;

		addr = src->address;
		lsize = src->size;
		end2 = addr + lsize;

		if (start >= addr && end1 <= end2) {
			/* deducted range in this chunk */
			deducted = 1;
			if (start == addr) {
				/* abuts start of chunk */
				if (end1 == end2)
					/* is equal to the chunk */
					continue;
				dst->address = end1;
				dst->size = lsize - size;
			} else if (end1 == end2) {
				/* abuts end of chunk */
				dst->address = addr;
				dst->size = lsize - size;
			} else {
				/* in the middle of the chunk */
				dst->address = addr;
				dst->size = start - addr;
				dst->next = 0;
				if (prev == dst) {
					dst->prev = 0;
					dst++;
				} else {
					dst->prev = prev;
					prev->next = dst;
					dst++;
					prev++;
				}
				dst->address = end1;
				dst->size = end2 - end1;
			}
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
			dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		} else {
			dst->address = src->address;
			dst->size = src->size;
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
		}
	}

	*dstp = dst;
	return (deducted);
}

/*
 * Find the page number of the highest installed physical
 * page and the number of pages installed (one cannot be
 * calculated from the other because memory isn't necessarily
 * contiguous).
 */
void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	pfn_t *topp,		/* return ptr for top value */
	pgcnt_t *sumpagesp,	/* return ptr for sum of installed pages */
	pfn_t last_pfn)
{
	pfn_t top = 0;
	pgcnt_t sumpages = 0;
	pfn_t highp;		/* high page in a chunk */
	uint64_t mmu_limited_physmax;

	mmu_limited_physmax = ptob((uint64_t)last_pfn + 1);
	for (; list; list = list->next) {

		if (list->address >= mmu_limited_physmax)
			continue;

		highp = (list->address + list->size - 1) >> PAGESHIFT;
		if (highp > last_pfn)
			highp = last_pfn;
		if (top < highp)
			top = highp;
		if (list->address + list->size < mmu_limited_physmax)
		    sumpages += btop(list->size);
		else
		    sumpages += btop(mmu_limited_physmax - list->address);
	}

	*topp = top;
	*sumpagesp = sumpages;
}

/*
 * Copy a memory list.  Used in startup() to copy boot's
 * memory lists to the kernel.
 */
void
copy_memlist(
	struct memlist *src,
	struct memlist **dstp)
{
	struct memlist *dst, *prev;

	dst = *dstp;
	prev = dst;

	for (; src; src = src->next) {
		dst->address = src->address;
		dst->size = src->size;
		dst->next = 0;
		if (prev == dst) {
			dst->prev = 0;
			dst++;
		} else {
			dst->prev = prev;
			prev->next = dst;
			dst++;
			prev++;
		}
	}

	*dstp = dst;
}

/*
 * Determine if an address is in a memlist...
 */
int
address_in_memlist(struct memlist *mp, uint64_t va, size_t len)
{

	while (mp != 0)	 {
		if ((va >= mp->address) &&
		    (va + len <= (mp->address + mp->size)))
			return (1);	 /* TRUE */
		mp = mp->next;
	}
	return (0);	/* FALSE */
}

/*
 * Kernel setup code, called from startup().
 */
void
kern_setup1(void)
{
	proc_t *pp;

	pp = &p0;

	proc_sched = pp;

	/*
	 * Initialize process 0 data structures
	 */
	pp->p_stat = SRUN;
	pp->p_flag = SLOAD | SSYS | SLOCK | SULOAD;

	pp->p_pidp = &pid0;
	pp->p_pgidp = &pid0;
	pp->p_sessp = &session0;
	pp->p_tlist = &t0;
	pid0.pid_pglink = pp;


	/*
	 * XXX - we asssume that the u-area is zeroed out except for
	 * ttolwp(curthread)->lwp_regs.
	 */
	u.u_cmask = (mode_t)CMASK;
	mutex_init(&u.u_flock, NULL, MUTEX_DEFAULT, NULL);

	/*
	 * Set up default resource limits.
	 */
	bcopy(rlimits, u.u_rlimit, sizeof (struct rlimit64) * RLIM_NLIMITS);

	thread_init();		/* init thread_free list */
#ifdef LATER
	hrtinit();		/* init hires timer free list */
	itinit();		/* init interval timer free list */
#endif
	pid_init();		/* initialize pid (proc) table */

	/*
	 * Determine pages_pp_maximum, the number of currently available
	 * pages (availrmem) that can't be `locked'. If not set by
	 * the user, we set it to 10% of the currently available memory.
	 * But we also insist that it be greater than tune.t_minarmem;
	 * otherwise a process could lock down a lot of memory, get swapped
	 * out, and never have enough to get swapped back in.
	 */
	if (pages_pp_maximum <= MAX(tune.t_minarmem+100, availrmem/10))
		pages_pp_maximum = MAX(tune.t_minarmem+100, availrmem/10);
}

/*
 * Second stage setup - some startup-like things cannot
 * be done until after the root has been mounted.
 */
int
kern_setup2(void)
{
	consconfig();
	return (0);
}

static char *initname = "/etc/init";

static struct  bootcode {
	char    letter;
	u_int   bit;
} bootcode[] = {	/* See reboot.h */
	'a',	RB_ASKNAME,
	's',	RB_SINGLE,
	'i',	RB_INITNAME,
	'h',	RB_HALT,
	'b',	RB_NOBOOTRC,
	'd',	RB_DEBUG,
	'w',	RB_WRITABLE,
	'G',    RB_GDB,
	'r',    RB_RECONFIG,
	'c',    RB_CONFIG,
	'v',	RB_VERBOSE,
	'k',	RB_KDBX,
	'f',	RB_FLUSHCACHE,
	0,	0,
};

char kern_bootargs[256];

/*
 * Parse the boot line to determine boot flags .
 */
void
bootflags(void)
{
	char *cp;
	int i;
	extern struct debugvec *dvec;

	if (BOP_GETPROP(bootops, "boot-args", kern_bootargs) < 0) {
		cp = NULL;
		boothowto |= RB_ASKNAME;
	} else {
		cp = kern_bootargs;
		while (*cp && *cp != '-')
			cp++;

		if (*cp && *cp++ == '-')
			do {
				for (i = 0; bootcode[i].letter; i++) {
					if (*cp == bootcode[i].letter) {
						boothowto |= bootcode[i].bit;
						break;
					}
				}
				cp++;
			} while (bootcode[i].letter && *cp);
	}

	if (boothowto & RB_INITNAME) {
		/*
		 * XXX	This is a bit broken - shouldn't we
		 *	really be using the initpath[] above?
		 */
		while (*cp && *cp != ' ' && *cp != '\t')
			cp++;
		initname = cp;
	}

	if (boothowto & RB_HALT) {
		prom_printf("kernel halted by -h flag\n");
		prom_enter_mon();
	}

	if (boothowto & RB_GDB) {
		extern int gdbon;
		gdbon = 1;
	}

#ifdef KDBX
	if (boothowto & RB_KDBX) {
		extern int kdbx_useme;
		kdbx_useme = 1;
	}
#endif KDBX

	/*
	 * If the boot flags say that kadb is there,
	 * test and see if it really is by peeking at DVEC.
	 * If is isn't, we turn off the RB_DEBUG flag else
	 * we call the debugger scbsync() routine.
	 * The kdbx debugger agent does the dvec and scb sync stuff,
	 * and sets RB_DEBUG for debug_enter() later on.
	 */
	if ((boothowto & RB_DEBUG) != 0) {
		if (dvec == NULL || ddi_peeks((dev_info_t *)0,
		    (short *)dvec, (short *)0) != DDI_SUCCESS)
			boothowto &= ~RB_DEBUG;
		else {
			prom_printf("bootflags() - KADB is running\n");
		}
	}
}

/*
 * MP initialization.
 */
void
mp_init(void)
{
	/*
	 * Start other CPUs, if any, now.
	 */
	extern void start_other_cpus(int);	/* should be in a header file */

	start_other_cpus(0);
}

/*
 * Start the initial user process.
 * The program [initname] is invoked with one argument
 * containing the boot flags.
 */
void
icode(void)
{
	char *ucp;
	caddr_t *uap;
	char *arg0, *arg1;
	proc_t *p = ttoproc(curthread);
	static char pathbuf[128];
	int i, error = 0;
	klwp_t *lwp = ttolwp(curthread);
	struct segvn_crargs crargs = STACK_ZFOD_ARGS;

	/*
	 * Allocate user address space and stack segment
	 */
	p->p_cstime = p->p_stime = p->p_cutime = p->p_utime = 0;
	p->p_usrstack = (caddr_t)USRSTACK32;
	p->p_model = DATAMODEL_ILP32;
	proc_init = p;
	p->p_as = as_alloc();
	p->p_as->a_userlimit = (caddr_t)USERLIMIT32;
	(void) as_map(p->p_as, (caddr_t)(p->p_usrstack - PAGESIZE),
	    PAGESIZE, segvn_create, (caddr_t)&crargs);
	(void) hat_setup(p->p_as->a_hat, 0);

	/*
	 * Construct the boot flag argument.
	 */
	ucp = (char *)p->p_usrstack;
	(void) subyte(--ucp, '\0');		/* trailing null byte */

	/*
	 * XXX - should we also handle "-i" ?
	 */
	if (boothowto & RB_SINGLE)
		(void) subyte(--ucp, 's');
	if (boothowto & RB_NOBOOTRC)
		(void) subyte(--ucp, 'b');
	if (boothowto & RB_RECONFIG)
		(void) subyte(--ucp, 'r');
	if (boothowto & RB_VERBOSE)
		(void) subyte(--ucp, 'v');
	if (boothowto & RB_FLUSHCACHE)
		(void) subyte(--ucp, 'f');
	(void) subyte(--ucp, '-');		/* leading hyphen */
	arg1 = ucp;

	/*
	 * Build a pathname.
	 */
	(void) strcpy(pathbuf, initname);

	/*
	 * Move out the file name (also arg 0).
	 */
	for (i = 0; pathbuf[i]; i++);		/* size the name */
	for (; i >= 0; i--)
		(void) subyte(--ucp, pathbuf[i]);
	arg0 = ucp;

	/*
	 * Move out the arg pointers.
	 */
	uap = (char **)((intptr_t)ucp & ~(NBPW-1));
	(void) suword32(--uap, 0);	/* terminator */
	(void) suword32(--uap, (u_int)arg1);
	(void) suword32(--uap, (u_int)arg0);

	/*
	 * Point at the arguments.
	 */
	lwp->lwp_ap = lwp->lwp_arg;
	lwp->lwp_arg[0] = (uintptr_t)arg0;
	lwp->lwp_arg[1] = (uintptr_t)uap;
	lwp->lwp_arg[2] = NULL;
	curthread->t_sysnum = SYS_execve;

	init_mstate(curthread, LMS_SYSTEM);

	/*
	 * Now let exec do the hard work.
	 */
	if (error = exec_common((const char *)arg0,
	    (const char **)uap, NULL)) {
		printf("Can't invoke %s, error %d\n", initname, error);
		cmn_err(CE_PANIC, "icode");
	}

	lwp_rtt();
}

/*
 * Load a procedure into a thread.
 */
int
thread_load(
	kthread_id_t	t,
	void		(*start)(),
	caddr_t		arg,
	size_t		len)
{
	caddr_t sp;
	size_t framesz;
	caddr_t argp;
	label_t ljb;
	long *p;
	extern void hat_map_kernel_stack_local(caddr_t, caddr_t);

	/*
	 * Push a "c" call frame onto the stack to represent
	 * the caller of "start".
	 */
	sp = t->t_stk;
	if (len != 0) {
		/*
		 * the object that arg points at is copied into the
		 * caller's frame.
		 */
		framesz = SA(len);
		sp -= framesz;
		if (sp < t->t_swap)	/* stack grows down */
			return (-1);
		argp = sp + SA(MINFRAME);
		if (on_fault(&ljb)) {
			no_fault();
			return (-1);
		}
		bcopy(arg, argp, len);
		no_fault();
		arg = (void *)argp;
	}
	/*
	 * Set up arguments (arg and len) on the caller's stack frame.
	 */
	p = (long *)sp;
	*--p = (long)len;
	*--p = (intptr_t)arg;
	*--p = (intptr_t)thread_exit;	/* threads that return, should exit */
	/*
	 * initialize thread to resume at (*start)().
	 */
	t->t_pc = (uintptr_t)start;
	t->t_sp = (uintptr_t)p;

	/*
	 * If necessary remap the kernel stack to node local pages.
	 */
	(void) hat_map_kernel_stack_local((caddr_t)t->t_stkbase,
		(caddr_t)t->t_stk);
	return (0);
}

/*
 * load user registers into lwp.
 */
void
lwp_load(klwp_t *lwp, gregset_t gregs)
{
	setgregs(lwp, gregs);
	lwptoregs(lwp)->r_ps = PSL_USER;
	lwp->lwp_eosys = JUSTRETURN;
	lwptot(lwp)->t_post_sys = 1;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setrval(klwp_id_t lwp, int v1, int v2)
{
	lwptoregs(lwp)->r_ps &= ~PS_C;
	lwptoregs(lwp)->r_r0 = v1;
	lwptoregs(lwp)->r_r1 = v2;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setsp(klwp_id_t lwp, caddr_t sp)
{
	lwptoregs(lwp)->r_usp = (int)sp;
}

/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	bcopy(lwp->lwp_regs, clwp->lwp_regs, sizeof (struct regs));
}

/*
 * This function is unused on x86.
 */
/* ARGSUSED */
void
lwp_freeregs(klwp_t *lwp)
{}

/*
 * Clear registers on exec(2).
 */
void
setregs(void)
{
	register u_long entry;
	register struct regs *rp;
	klwp_id_t lwp = ttolwp(curthread);

	entry = (u_long)u.u_exdata.ux_entloc;

	/*
	 * Initialize user registers.
	 * (Note: User stack pointer is already initialized by buildstack()
	 *	  or fastbuildstack()).
	 */
	rp = lwptoregs(lwp);
	rp->r_cs = USER_CS;
	rp->r_ds = rp->r_ss = rp->r_es = USER_DS;
	rp->r_gs = rp->r_fs = 0;
	rp->r_pc = entry;
	rp->r_ps = PSL_USER;	/* initial user EFLAGS */
	rp->r_eax = rp->r_ebx = rp->r_ecx = rp->r_edx = rp->r_edi =
	rp->r_esi = rp->r_ebp = 0;
	lwp->lwp_eosys = JUSTRETURN;
	curthread->t_post_sys = 1;

	/*
	 * Here we initialize minimal fpu state.
	 * The rest is done at the first floating
	 * point instruction that a process executes.
	 */
	lwp->lwp_pcb.pcb_fpu.fpu_flags = 0;
}

/*
 * Condition variable used while waiting for virtual address
 * to become available in the kernel's address space during exec.
 */
static kcondvar_t args_cv;

/*
 * Allocate a chunk of anon memory and create an arg_hunk struct that
 * points at it.  This arg_hunk will eventually be remapped to USRSTACK.
 * Size is rounded up to be a multiple of PAGESIZE.
 */
static int
alloc_hunk(struct as *as, size_t size, struct uarg *args)
{
	struct segvn_crargs crargs;
	struct anon_map *amp;
	caddr_t base;
	size_t len;
	size_t nsize;
	int error;
	size_t mapsize;

	base = (caddr_t)ARGSBASE;	/* start of virtual arg space area */
	len = NCARGS;

	nsize = roundup(size, PAGESIZE);

	/*
	 * Reserve swap space, allocate and intialize the anon_map.
	 *
	 * Note:  anonmap_alloc() is not used as a performance optimization
	 * since it initializes the the various mutexes in the anon_map
	 * structure.  This also assumes that adaptive mutexes are of type
	 * 0 (yuk!).
	 *
	 * Allocate bigger anon map than needed to preload extra stack pages.
	 */
	if (anon_resv(nsize) == 0)
		return (ENOMEM);
	amp = kmem_zalloc(sizeof (*amp), KM_SLEEP);
	mapsize = nsize + roundup(SSIZE, PAGESIZE);
	amp->refcnt = 1;
	amp->size = mapsize;
	amp->ahp = anon_create(btop(mapsize), ANON_SLEEP);

	as_rangelock(as);		/* serialize access to as ranges */

	while (as_gap(as, nsize, &base, &len, AH_LO, NULL)) {
		/*
		 * Need to wait for virtual address space to be freed,
		 * so release hold on "claimgap" and wakeup any other
		 * blocked threads.
		 */
		as_rangewait(as, &args_cv);
	}

	/*
	 * Now setup the arguments for segvn_create and map the segment
	 * into the specified address space.
	 */
	crargs.vp = NULL;
	crargs.offset = mapsize - nsize;
	crargs.cred = NULL;
	crargs.type = MAP_SHARED;
	crargs.maxprot = PROT_ALL;
	crargs.prot = PROT_ALL;
	if (as == &kas) {
		crargs.maxprot &= ~PROT_USER;
		crargs.prot &= ~PROT_USER;
	}
	crargs.flags = 0;
	crargs.amp = amp;
	if (error = as_map(as, base, nsize, segvn_create, &crargs)) {
		anon_unresv(nsize);
		as_rangeunlock(as);
		anon_release(amp->ahp, btop(mapsize));
		kmem_free(amp, sizeof (*amp));
		return (error);
	}
	as_rangeunlock(as);
	(void) as_fault(as->a_hat, as, base, nsize, F_INVAL, S_WRITE);

	/*
	 * Initialize arg structure.
	 */
	args->as = as;
	args->hunk_base = base;
	args->amp = amp;
	args->hunk_size = nsize;
	return (0);
}

/*
 * Discard a hunk of memory that was allocated by alloc_hunk().
 */
static void
free_hunk(struct anon_map *amp, size_t size)
{
	ASSERT(amp->refcnt == 1);

	anon_free(amp->ahp, 0, amp->size);
	anon_unresv(size);
	anon_release(amp->ahp, btop(amp->size));
	kmem_free(amp, sizeof (*amp));
}

/*
 * Map a hunk of anonymous memory to another address.
 */
static int
map_hunk(struct as *as, struct anon_map *amp, caddr_t addr, size_t size)
{
	struct segvn_crargs crargs = STACK_ZFOD_ARGS;
	crargs.amp = amp;
	crargs.prot = curproc->p_stkprot;

	/*
	 * Remap anon pages containing the arguments.
	 */
	return (as_map(as, addr, size, segvn_create, (caddr_t)&crargs));
}

/*
 * Calculate the amount of space used by auxiliary vector strings.
 */
static void
exec_auxsize(
	struct execa	*uap,
	struct uarg	*args,
	struct intpdata	*idata,
	void		**aux,
	model_t		to_model)
{
	size_t	l;

#ifdef lint
	uap = uap;
	idata = idata;
	to_model = to_model;
#endif
	if ((aux == NULL) || (*aux == NULL))
		return;

	/*
	 * for AT_SUN_PLATFORM and AT_SUN_EXECNAME
	 */
	ASSERT(to_model == DATAMODEL_NATIVE);
	{
		auxv_t *a = *aux;
		ADDAUX(a, AT_SUN_PLATFORM, 0);
		ADDAUX(a, AT_SUN_EXECNAME, 0);
	}

	/*
	 * "nc" is the size of all the information block info, i.e.
	 * argv[] and environ[] strings and auxiliary information.
	 *
	 * preserve "nc" size previously calculated by exec_argsize()
	 */
	l = strlen(platform) + 1 + strlen(args->pathname) + 1;
	l = (l + NBPW - 1) & ~(NBPW - 1);
	args->nc += l;
}

/*
 * Given an array of arguments (pointers to strings),
 * calculate the number of args and the total number of characters needed for
 * the strings for all the args.
 */
/*ARGSUSED*/
static void
calc_argsize(
	model_t	model,
	caddr_t	argptr,
	size_t	*numargs,
	size_t	*numchars)
{
	size_t	na, nc;
	caddr_t	strptr;

	na = nc = 0;

	{
		do {
			fulword_noerr(argptr, (u_long *)&strptr);
			argptr += sizeof (char *);
			if (strptr == NULL)
				break;
			nc += ustrlen(strptr);
			na++;
		/*CONSTCOND*/
		} while (1);
	}

	*numargs += na;
	*numchars += nc;
}

/*
 * Calculate the amount of space used by *argv[], and *environ[].
 */
/*ARGSUSED4*/
static void
exec_argsize(
	struct execa	*uap,
	struct uarg	*args,
	struct intpdata	*idata,
	model_t		from_model,
	model_t		to_model)
{
	caddr_t argvp;
	caddr_t envp;
	size_t nc;
	size_t na, ne;

	argvp = (caddr_t)uap->argp;
	envp = (caddr_t)uap->envp;

	na = 0;
	nc = 0;
	/*
	 * Calculate size of the program interpreting the file
	 * referenced by argv[0].
	 */
	if (idata && idata->intp_name) {
		argvp += sizeof (caddr_t);	/* ignore argv[0] */
		nc += strlen(idata->intp_name);
		if (idata->intp_arg) {
			nc += strlen(idata->intp_arg);
			na++;
		}
		if (args->fname != NULL)
			nc += strlen(args->fname);
		else
			nc += ustrlen(uap->fname);
		na += 2;
	}

	/*
	 * Calculate the size of "argvp".
	 */
	calc_argsize(from_model, argvp, &na, &nc);

	/*
	 * Calculate the size of "envp".
	 */
	ne = 0;
	if (envp != NULL)
		calc_argsize(from_model, envp, &ne, &nc);

	args->na = na + ne;
	args->ne = ne;

	/*
	 * "na" is the number of argv[] and environ[] pointers while
	 * "ne" is the just the number of environ[] pointers.  "nc" is
	 * the number of characters in argv[] and environ[] strings.
	 *
	 * "na + ne" is the number of bytes used to terminate the strings
	 * since they are not included by strlen.
	 */
	nc = nc + (na + ne);
	nc = (nc + sizeof (caddr_t) - 1) & ~(sizeof (caddr_t) - 1);
	args->nc = nc;
}

static int fastbuildstack(struct execa *, struct uarg *,
    struct intpdata *idata, size_t size, void **aux);

/*
 * Move exec arguments and environment variables to the new user's
 * stack.
 *
 * NOTE:  If a -1 is returned as an error, it implies that the old
 * address space is either partially or completely destroyed and
 * it is the callers responsiblity to send a SIGKILL to the process.
 */
int
exec_args(
	struct execa	*uap,
	struct uarg	*args,
	struct intpdata	*idata,
	void		**aux)
{
	int error;
	label_t ljb;
	size_t size;
	volatile int hunk_allocated = 0;
	proc_t *p = ttoproc(curthread);
	uint32_t ui;

	args->committed = 0;
	if (on_fault(&ljb)) {
		/*
		 * Free hunk, if allocated.
		 * XXX - Will this work?
		 */
		if (hunk_allocated) {
			(void) as_unmap(args->as, args->hunk_base,
			    args->hunk_size);
		}
		no_fault();
		if (args->committed == 1) {
			/*
			 * fastbuildstack has relvm'ed the old address
			 * space.  return -1 so that e.g. elfexec will
			 * SIGKILL us instead of returning ENOEXEC.
			 */
			return (-1);
		}
		return (EFAULT);
	}

	exec_argsize(uap, args, idata, p->p_model, args->to_model);
	exec_auxsize(uap, args, idata, aux, args->to_model);

	fuword32_noerr(&(args->auxsize), &ui);	/* huh? why from userland? */
	size = GET_NARGC(args->nc, args->na) + ui;

	if (size > NCARGS) {
		no_fault();
		return (E2BIG);
	}

	/*
	 * Allocate one chunk of anon memory that'll hold
	 * argv[], environ[], and their strings in the kernel's
	 * address space.
	 */
	error = alloc_hunk(&kas, size, args);
	if (error == 0) {
		hunk_allocated = 1;
		error = fastbuildstack(uap, args, idata, size, aux);
		if (error)
			error = -1;
	}
	no_fault();
	return (error);
}

/*
 * Put the given pointer value at addr and return the size
 */
static void
putptr(model_t model, caddr_t addr, uintptr_t value, size_t *size)
{
	ASSERT(model == DATAMODEL_NATIVE);
	{
		*(caddr_t *)addr = (caddr_t)value;
		*size = sizeof (caddr_t);
	}
}

/*
 * Copy an array of argument pointers and the strings that they point at.
 */
static void
copy_args(
	model_t	old_model,	/* data model of process being copied from */
	model_t	new_model,	/* data model of process being copied to */
	int	argc,		/* arg count */
	caddr_t	old_argvp,	/* address of arg ptrs to copy from */
	caddr_t	*buf_argvp,	/* address of arg ptrs to copy to */
	caddr_t	*buf_strp,	/* ptr to where to copy arg string to */
	caddr_t	*new_strp)	/* new ptr address of string */
{
	caddr_t	buf_argvptr;	/* address of arg ptrs to copy to */
	caddr_t	buf_strptr;	/* ptr to where to copy arg string to */
	caddr_t	new_strptr;	/* new ptr address of string */
	caddr_t	old_strp;	/* ptr to arg string to copy from */
	size_t	strlen;		/* length of arg string */

	buf_argvptr = *buf_argvp;
	buf_strptr = *buf_strp;
	new_strptr = *new_strp;

#ifdef _ILP32
	/*
	 * The 32-bit kernel only handles native 32-bit processes
	 */
	ASSERT(old_model == new_model && old_model == DATAMODEL_NATIVE);
#endif
	{
		while (argc-- > 0) {
			/*
			 * Fetch the pointer to the argument string
			 * and copy the string into the buffer
			 */
			fulword_noerr(old_argvp, (u_long *)&old_strp);
			(void) copyinstr_noerr(buf_strptr, old_strp, &strlen);

			/*
			 * Set the argument pointer to point at where the
			 * string will be (after the buffer gets mapped into
			 * the user's address space) and increment the
			 * pointers.
			 */
			ASSERT(new_model == DATAMODEL_NATIVE);
			{
				*(caddr_t *)buf_argvptr = new_strptr;
				buf_argvptr += sizeof (caddr_t);
			}

			buf_strptr += strlen;
			new_strptr += strlen;
			old_argvp += sizeof (caddr_t);
		}
	}

	/*
	 * NULL terminate arg ptrs
	 */
	{
		*(caddr_t *)buf_argvptr = NULL;
		buf_argvptr += sizeof (caddr_t);
	}

	*buf_argvp = buf_argvptr;
	*buf_strp = buf_strptr;
	*new_strp = new_strptr;
}

/*
 * Copy the user arguments into a temporary area in the old user's
 * address space before invalidating the remaining segments.
 *
 * 			User Stack
 *	argvp_end ---->	-----------------	-  HIGH ADDRS ----
 *			|	NULL	|	|		|
 *			-----------------	|		|
 *			|		|	|		|
 * 			|   		|	|		|
 *			|    ARGUMENT	|	|		|
 *			|    STRINGS	|	|		|
 *			|		|	strbuflen	|
 *  			|		|	|		|
 *  			|		|	|		|
 *  			|		|	|		|
 * 			|		|	|		|
 * 			|		|	|		|
 *			----------------- <----- strp		|
 *			|    AUX	|			size
 *			|    VECTOR	|			|
 *			-----------------			|
 *			|    NULL	|    ^			|
 *			-----------------    |			|
 *			|		|-----			|
 *	env[] -------->	-----------------			|
 *			|    NULL	|	^		|
 *			-----------------	|		|
 *			|		|--------   ^		|
 *			-----------------	    |		|
 *			|		|------------		|
 *	argv[] ------->	-----------------			|
 *			|    argc	|			|
 *	argvp -------->	-----------------	   LOW ADDRS	--
 *
 */
static int
fastbuildstack(
	struct execa	*uap,
	struct uarg	*args,
	struct intpdata	*idata,
	size_t		size,
	void		**aux)
{
	caddr_t argvp;
	uintptr_t argvp_end, usrsp;
	size_t len;
	uintptr_t strp, ostrp;
	caddr_t ps_strp, addr;
	ssize_t ps_len;
	ssize_t argc;
	ssize_t strbuflen;
	int error;
	struct as *as;
	struct anon_map *amp;
	size_t args_size;
	proc_t *pp = ttoproc(curthread);
	klwp_id_t lwp = ttolwp(curthread);
	struct user *up = PTOU(pp);
	size_t mapsize;
	size_t sz;
	caddr_t usrstack;
	void (*funcp)();

	argc = args->na - args->ne;
	ASSERT(args->to_model == DATAMODEL_NATIVE);
	{
		usrstack = (caddr_t)USRSTACK;
		strbuflen = args->nc + sizeof (caddr_t);
		/*
		 * Calculate final resting place for argv
		 * as user stack address - size of argc, argv, env, aux, and
		 * string table + the size of argc + padding so argv will be
		 * properly aligned.
		 */
		up->u_argv = ((uintptr_t)usrstack - size + sizeof (caddr_t));
		up->u_envp = up->u_argv + (argc + 1) * sizeof (caddr_t);
	}

	argvp_end = (uintptr_t)(args->hunk_base + args->hunk_size);
	strp = argvp_end - strbuflen;

	/* for /proc */
	up->u_argc = argc;

	argvp = (caddr_t)(argvp_end - size);
	usrsp = (uintptr_t)usrstack - strbuflen;

	*(int *)argvp = argc;
	argvp += sizeof (int);
	if (args->to_model == DATAMODEL_LP64) {
		argvp += sizeof (int);	/* need to align for argv pointers */
		ASSERT(((uintptr_t)argvp & 7) == 0);
	}

	ps_strp = (caddr_t)strp;	/* remember for u_psargs */
	ostrp = strp;

	/*
	 * Copy interpreter's name and argument to argv[0] and argv[1].
	 */
	if (idata && idata->intp_name) {
		(void) knstrcpy((caddr_t)strp, idata->intp_name, &len);
		uap->argp++;			/* ignore argv[0] */
		putptr(args->to_model, argvp, usrsp, &sz);
		argvp += sz;
		strp += len;
		usrsp += len;

		/* argv[1] or argv[2] */
		if (idata->intp_arg != NULL) {
			(void) knstrcpy((caddr_t)strp, idata->intp_arg, &len);
			putptr(args->to_model, argvp, usrsp, &sz);
			argvp += sz;
			strp += len;
			usrsp += len;
			argc--;
		}
		if (args->fname != NULL)
			(void) knstrcpy((caddr_t)strp, args->fname, &len);
		else
			(void) copyinstr_noerr((caddr_t)strp, uap->fname, &len);
		putptr(args->to_model, argvp, usrsp, &sz);
		argvp += sz;
		strp += len;
		usrsp += len;
		argc -= 2;
	}

	/*
	 * Copy exec arguments to stack.
	 */
	copy_args(pp->p_model, args->to_model, argc, (caddr_t)uap->argp,
	    (caddr_t *)&argvp, (caddr_t *)&strp, (caddr_t *)&usrsp);

	/*
	 * Copy arguments to u.u_psargs.
	 */
	ps_len = MIN((caddr_t)strp - ps_strp, PSARGSZ);
	bzero(up->u_psargs, PSARGSZ);
	if (ps_len > 0) {
		bcopy(ps_strp, up->u_psargs, ps_len);
		ps_strp = &up->u_psargs[--ps_len];
		*ps_strp = '\0';		/* NULL terminate the string */
		while (--ps_strp >= up->u_psargs) {
			if (*ps_strp == '\0')
				*ps_strp = ' ';
		}
	}

	/*
	 * Copy environ variables to stack.
	 */
	copy_args(pp->p_model, args->to_model, args->ne, (caddr_t)uap->envp,
	    (caddr_t *)&argvp, (caddr_t *)&strp, (caddr_t *)&usrsp);

	/*
	 * copy auxiliary vector information to stack
	 */
	if (aux && *aux) {
		ASSERT(args->to_model == DATAMODEL_NATIVE);
		{
			auxv_t **a = (auxv_t **)aux;
			if ((*a)->a_type == AT_SUN_PLATFORM) {
				(void) knstrcpy((caddr_t)strp, platform, &len);
				(*a)->a_un.a_val = usrsp;
				(*a)++;
				strp += len;
				usrsp += len;
			}
			if ((*a)->a_type == AT_SUN_EXECNAME) {
				(void) knstrcpy((caddr_t)strp,
				    args->pathname, &len);
				(*a)->a_un.a_val = usrsp;
				(*a)++;
				strp += len;
				usrsp += len;
			}
		}
	}

	/*
	 * Determine the location of the "aux" vector.
	 */
	args->stackend = (caddr_t)(usrstack - (argvp_end - (uintptr_t)argvp));

	if (ostrp - (uintptr_t)argvp < args->auxsize) {
		cmn_err(CE_PANIC,
		    "fastbuildstack: arg strings may be overwritten");
	}

#ifdef  C2_AUDIT
	if (audit_active)
		audit_exec((uintptr_t)usrstack, argvp_end, size, args->na,
		    args->ne);
#endif

	/*
	 * At this point, we are committed to the new image!
	 * Release virtual memory resource of old process, and
	 * initialize the virtual memory of the new process.
	 */
	relvm();

	args->committed = 1;

	/*
	 * Update the execsw pointer in the uarea.  Now that we've committed
	 * to exec, we want the appropriate core dump function available.
	 * If we fail after this, exec_args will return -1 and the exec
	 * module will deliver SIGKILL to the process, and no core dump
	 * will be required.
	 */
	up->u_execsw = args->execswp;

	pp->p_brkbase = NULL;
	pp->p_brksize = 0;
	pp->p_stksize = 0;
	pp->p_model = args->to_model;
	pp->p_usrstack = usrstack;

	pp->p_stkprot = PROT_ZFOD;

	if (noexec_user_stack)
		pp->p_stkprot &= ~PROT_EXEC;

	/*
	 * Call platform-specific cpu partition hook (if set).  We may
	 * want to move to another partition now that our old address
	 * space is gone.
	 */
	if ((funcp = cpupart_exec_hook) != NULL)
		(*funcp)();

	lwptoregs(lwp)->r_usp = (uintptr_t)usrstack - size;

	/*
	 * Allocate an address space and setup its context.
	 */
	as = pp->p_as = as_alloc();
	ASSERT(pp->p_model == DATAMODEL_NATIVE);
	(void) hat_setup(as->a_hat, HAT_ALLOC);

	amp = args->amp;
	args_size = args->hunk_size;
	mapsize = amp->size;

	/*
	 * Now unmap the segment which contains the arguments.  This
	 * will only free up the segment since the anon_map reference
	 * count is 2.
	 *
	 * NOTE:  The segment has to be unmapped before mapping in the
	 * stack in order to prevent overlapping segments if both are
	 * in the same address space.
	 */
	ASSERT(amp->refcnt == 2);

	error = as_unmap(args->as, args->hunk_base, args_size);
	as_rangebroadcast(args->as, &args_cv);

	ASSERT(amp->refcnt == 1);

	/*
	 * Map args into the user's stack.
	 */
	if (error == 0) {
		addr = (caddr_t)pp->p_usrstack - mapsize;
		error = map_hunk(as, amp, addr, mapsize);
		pp->p_stksize = mapsize;
	}

	/*
	 * Now, discard anon map and its associated structures.
	 */
	free_hunk(amp, args_size);

	/*
	 * Ensure that stack pages have writable translations by
	 * forcing a minor fault.
	 */
	if (error == 0) {
		(void) as_fault(as->a_hat, as, addr, mapsize,
		    F_INVAL, S_WRITE);
	}
	return (error);
}

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 *
 * i86:
 *	For COFF Binary Compatibility we assume that COFF executables use
 *	SVR532 libraries and therefore we need sigret() to clean up
 *	after signal handler.
 */

/* These structure defines what is pushd on the stack */

/* SVR4/ABI signal frame */
struct argpframe {
	void		(*retadr)();
	u_int		signo;
	siginfo_t	*sip;
	ucontext_t	*ucp;
};

/* SVR532 compatible frame */
struct compat_frame {
	void		(*retadr)();
	u_int		signo;
	gregset_t	gregs;
	char		*fpsp;
	char		*wsp;
};

int
sendsig(
	int		sig,
	k_siginfo_t	*sip,
	register void	(*hdlr)())
{
	/*
	 * 'volatile' is needed to ensure that values are
	 * correct on the error return from on_fault().
	 */
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack = 0;	/* if true, switching to altstack */
	label_t ljb;
	volatile u_int sp;
	register u_int fp;
	register struct regs *regs;
	proc_t *volatile p = ttoproc(curthread);
	klwp_t *lwp = ttolwp(curthread);
	ucontext_t *volatile tuc = NULL;
	ucontext_t *uc;
	siginfo_t *sip_addr;
	int	old_style = 0;	/* flag to indicate SVR532 signal frame */
	int setsegregs = 0; /* flag to set segment registers */
	volatile int mapped;

	regs = lwptoregs(lwp);

	/*
	 * Determine if we need to generate SVR532 compatible
	 * signal frame.
	 */

	old_style = is_coff_proc(p);

	if (old_style) {
		minstacksz = sizeof (struct compat_frame) +
				sizeof (ucontext_t);
		sip = NULL;
	} else
		minstacksz = sizeof (struct argpframe) +
				sizeof (ucontext_t);

	if (sip != NULL)
		minstacksz += sizeof (siginfo_t);

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&u.u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	/* check if we need to reload the selector registers */
	if (((regs->r_ss & 0xffff) != USER_DS) ||
	    ((regs->r_cs & 0xffff) != USER_CS))
		setsegregs++;

	if (newstack != 0) {
		sp = (u_int)(SA((int)lwp->lwp_sigaltstack.ss_sp) +
		    SA((int)lwp->lwp_sigaltstack.ss_size) - STACK_ALIGN -
			SA(minstacksz));
	} else {
		/*
		 * If the stack segment selector is not the 386 user data
		 * data selector, convert the SS:SP to the equivalent 386
		 * virtual address and set the flag to load SS and DS with
		 * correct values after saving the current context.
		 */
		if ((regs->r_ss & 0xffff) != USER_DS) {
			char *dp;
			struct dscr *ldt = (struct dscr *)p->p_ldt;

			if (ldt == NULL)
				return (0); /* can't setup signal frame */
			dp = (char *)(ldt + seltoi(regs->r_ss));
			sp = regs->r_usp & 0xFFFF;
			sp += (dp[7] << 24) | (*(int *)&dp[2] & 0x00FFFFFF);
			sp -= SA(minstacksz);
		} else
			sp = (u_int)((caddr_t)regs->r_usp - SA(minstacksz));
	}

	fp = sp + SA(minstacksz);

	/*
	 * Make sure process hasn't trashed its stack.
	 */
	if (((int)sp & (STACK_ALIGN - 1)) != 0 ||
	    (caddr_t)sp >= (caddr_t)KERNELBASE ||
	    (caddr_t)sp + SA(minstacksz) >= (caddr_t)KERNELBASE) {
#ifdef DEBUG
		printf("sendsig: bad signal stack pid=%d, sig=%d\n",
		    (int)curproc->p_pid, sig);
		printf("sigsp = 0x%x, action = 0x%x, upc = 0x%x\n",
		    sp, (int)hdlr, regs->r_pc);

		if (((int)sp & (STACK_ALIGN - 1)) != 0)
		    printf("bad stack alignment\n");
		else
		    printf("sp above KERNELBASE\n");
#endif
		return (0);
	}

	mapped = 0;
	if (p->p_warea)
		mapped = pr_mappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);

	if (on_fault(&ljb))
		goto badstack;

	if (sip != NULL) {
		fp -= sizeof (siginfo_t);
		uzero((caddr_t)fp, sizeof (siginfo_t));
		copyout_noerr((caddr_t)sip, (caddr_t)fp, sizeof (*sip));
		sip_addr = (siginfo_t *)fp;

		if (sig == SIGPROF &&
		    curthread->t_rprof != NULL &&
		    curthread->t_rprof->rp_anystate) {
			/*
			 * We stand on our head to deal with
			 * the real time profiling signal.
			 * Fill in the stuff that doesn't fit
			 * in a normal k_siginfo structure.
			 */
			register int i = sip->si_nsysarg;

			while (--i >= 0)
				suword32_noerr(&(sip_addr->si_sysarg[i]),
						lwp->lwp_arg[i]);
			copyout_noerr((caddr_t)curthread->t_rprof->rp_state,
			    (caddr_t)sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else
		sip_addr = (siginfo_t *)NULL;

	fp -= sizeof (ucontext_t);
	uc = (ucontext_t *)fp;

	tuc = kmem_alloc((size_t)(sizeof (ucontext_t)), KM_SLEEP);

	/* save the current context on the user stack */

	savecontext(tuc, lwp->lwp_sigoldmask);
	copyout_noerr((caddr_t)tuc, (caddr_t)uc, sizeof (ucontext_t));
	kmem_free(tuc, sizeof (ucontext_t));
	tuc = NULL;
	lwp->lwp_oldcontext = (uintptr_t)uc;

	if (newstack != 0)
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

	/*
	 * Set up user registers for execution of signal handler.
	 */
	if (old_style) { /* SVR532 compatible signal frame */
		register struct compat_frame *cframe;

		cframe = (struct compat_frame *)sp;
		cframe->retadr = u.u_sigreturn;
		cframe->signo = sig;
		ucopy((caddr_t)uc->uc_mcontext.gregs, (caddr_t)cframe->gregs,
		    sizeof (gregset_t));
		cframe->fpsp = (char *)&uc->uc_mcontext.fpregs.fp_reg_set;
		cframe->wsp = (char *)&uc->uc_mcontext.fpregs.f_wregs[0];
	} else {
		register struct argpframe *aframe;

		aframe = (struct argpframe *)sp;
		aframe->sip = sip_addr;
		aframe->ucp = uc;
		aframe->signo = sig;
		/* Shouldn't return via this; if they do, fault. */
		aframe->retadr = (void (*)())0xFFFFFFFF;
	}

	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);

	regs->r_usp = (greg_t)sp;
	regs->r_pc = (greg_t)hdlr;
	regs->r_ps = PSL_USER | (regs->r_ps & PS_IOPL);

	if (setsegregs) { /* we need to correct the segment registers */
		regs->r_cs = USER_CS;
		regs->r_ds = regs->r_es = regs->r_ss = USER_DS;
	}

	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (mapped)
		pr_unmappage((caddr_t)sp, SA(minstacksz), S_WRITE, 1);
	if (tuc)
		kmem_free(tuc, sizeof (ucontext_t));
	return (0);
}

/*
 * Restore user's context after execution of user signal handler
 * This code restores all registers to what they were at the time
 * signal occured. So any changes made to things like flags will
 * disappear.
 *
 * The saved context is assumed to be at esp+xxx address on the user's
 * stack. If user has mucked with his stack, he will suffer.
 * Called from the sig_clean.
 *
 * On entry, assume all registers are pushed.  r0ptr points to registers
 * on stack.
 * This function returns like other system calls.
 *
 * Note:
 *	It is assumed that this routine is needed only to support SVR532
 *	compatible signal handling mechanism. And the SVR4 compatible
 *	signal handling uses setcontext(2) system call to cleanup the
 *	signal frame.
 */

void
sigclean(
    register int *r0ptr)		/* registers on stack */
{
	register struct compat_frame *cframe;
	ucontext_t	uc, *ucp;

	/*
	 * The user's stack pointer currently points into compat_frame
	 * on the user stack.  Adjust it to the base of compat_frame.
	 */
	cframe = (struct compat_frame *)(r0ptr[UESP] - 2 * sizeof (int));

	ucp = (ucontext_t *)(cframe + 1);

	if (copyin(ucp, &uc, sizeof (ucontext_t))) {
		int v;
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_start(SIGSEGV);
#endif
		v = core("core", ttoproc(curthread), CRED(),
			(rlim_t)U_CURLIMIT(&u, RLIMIT_CORE), SIGSEGV);
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_finish((v)?CLD_KILLED:CLD_DUMPED);
#endif
		exit((v ? CLD_KILLED : CLD_DUMPED), SIGSEGV);
		return;
	}

	/*
	 * Old stack frame has gregs in a different place.
	 * Copy it into the ucontext structure.
	 */
	if (copyin((caddr_t)&cframe->gregs, (caddr_t)&uc.uc_mcontext.gregs,
	    sizeof (gregset_t))) {
		int v;
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_start(SIGSEGV);
#endif
		v = core("core", ttoproc(curthread), CRED(),
			(rlim_t)U_CURLIMIT(&u, RLIMIT_CORE), SIGSEGV);
#ifdef C2_AUDIT
		if (audit_active)		/* audit core dump */
			audit_core_finish((v)?CLD_KILLED:CLD_DUMPED);
#endif
		exit((v ? CLD_KILLED : CLD_DUMPED), SIGSEGV);
		return;
	}

	(void) restorecontext(&uc);
}

/*
 * Determine if the process is from COFF executable (532 signal compatibility).
 * Returns true for COFF. THIS COULD BE A MACRO.
 * (Note: Currently we don't have support for running COFF binaries yet.)
 */

int
is_coff_proc(register proc_t *p)
{
	extern short elfmagic;

#ifdef XXX_COFF
	extern short coffmagic;

	if (p->p_user.u_execid == coffmagic) /* if COFF then return TRUE */
		return (1);
	else
		return (0);
#else
	if (p->p_user.u_execid == elfmagic) /* if ELF then return FALSE */
		return (0);
	else
		return (1);
#endif XXX_COFF
}

#if !defined(lwp_getdatamodel)

/*
 * Return the datamodel of the given lwp.
 */
/*ARGSUSED*/
model_t
lwp_getdatamodel(klwp_id_t lwp)
{
	return (DATAMODEL_ILP32);
}

#endif	/* !lwp_getdatamodel */

#if !defined(get_udatamodel)

model_t
get_udatamodel(void)
{
	return (lwp_getdatamodel(curthread->t_lwp));
}

#endif	/* !get_udatamodel */
