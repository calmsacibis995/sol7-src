/*
 * Copyright (c) 1992-1993,1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)machdep.c	1.136	98/02/07 SMI"

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

#include <sys/disp.h>
#include <sys/class.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/callb.h>

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
#include <sys/tss.h>
#include <sys/cpu.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <sys/pic.h>
#include <sys/mmu.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
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
#include <sys/machlock.h>
#include <sys/x_call.h>
#include <sys/instance.h>
#include <sys/msgbuf.h>

#include <sys/time.h>
#include <sys/smp_impldefs.h>
#include <sys/psm_types.h>
#include <sys/atomic.h>

#include <sys/dumphdr.h>
extern struct dumphdr *dumphdr;

/*
 * Declare these as initialized data so we can patch them.
 */
int noprintf = 0;	/* patch to non-zero to suppress kernel printf's */
int msgbufinit = 1;	/* message buffer has been initialized, ok to printf */
int nopanicdebug = 0;	/* 0 = call debugger (if any) on panic, 1 = reboot */

u_int phys_msgbuf;	/* physical address of msgbuf for libkvm */
struct msgbuf msgbuf;

/*
 * Initialize variables containing the beginning and end addresses of the
 * virtual space reserved for the kernel heap.
 */
char *kernelheap = Sysbase;
char *ekernelheap = Syslimit;

/*
 * maxphys - used during physio
 * klustsize - used for klustering by swapfs and specfs
 */
int maxphys = 56 * 1024;    /* XXX See vm_subr.c - max b_count in physio */
int klustsize = 56 * 1024;

caddr_t	p0_va;		/* Virtual address for accessing physical page 0 */
int default_panic_timeout;
int	pokefault = 0;

/*
 * defined here, though unused on x86,
 * to make kstat_fr.c happy.
 */
int vac;

struct debugvec *dvec = (struct debugvec *)0;

#define	PANIC_TIMEOUT (default_panic_timeout)
#define	BOOT_CPU	0 /* replace with getbootcpuid in future */

extern struct memlist *phys_install; /* Total installed physical memory */
extern struct memlist *phys_avail;  /* Available (unreserved) physical memory */
extern struct memlist *virt_avail;  /* Available (unmapped?) virtual memory */
extern struct memlist *shadow_ram;  /* Non-dma-able memory XXX needs work */
extern struct kvtopdata kvtopdata;
extern struct bootops *bootops;	/* passed in from boot */


int (*getcharptr)(void);
int (*putcharptr)(char);
int (*ischarptr)(void);

void stop_other_cpus();
void debug_enter(char *);

static void complete_panic(void);

kthread_id_t clock_thread;	/* clock interrupt thread pointer */
kmutex_t memseg_lock;		/* lock for searching memory segments */

#ifdef MP
int	procset = 1;
#endif /* MP */

/*
 * Panic is called on unresolvable fatal errors.
 * It prints "panic: mesg", and then reboots.
 * If we are called twice, then we avoid trying to
 * sync the disks as this often leads to recursive panics.
 */

void    do_panic();

int panic_timeout = 0;

/*
 * In case console is off, panicstr contains argument
 * to last call to panic.
 */
char	*panicstr = 0;
va_list  panicargs;

/*
 * If do_polled_io is set, the scsi transport system will do polled I/O instead
 * of interrupt I/O when called from xx_dump().
 * This is placed here because this behavior used to be indicated by setting
 * panicstr.  (See bug id 1256334).
 */
int	do_polled_io;

/*
 * This is the state of the world before the file system are sync'd
 * and system state is dumped. Should be put in panic data structure.
 */
label_t	panic_regs;	/* adb looks at these */
kthread_id_t panic_thread;
kthread_id_t panic_clock_thread;
clock_t panic_lbolt = 0;
struct cpu panic_cpu;
kmutex_t paniclock;
int setup_panic_done = 0;

/* The cpus on which panic_hook has to enable interrupts */
cpuset_t	cpus_to_enable = CPUSET_ALL;
volatile int	cpu_intr_enabled = -1;
volatile int	cpu_reboot = -1;

void
panic(const char *fmt, ...)
{
	void do_panic(const char *, va_list);
	va_list adx;

	va_start(adx, fmt);
	do_panic(fmt, adx);
	va_end(adx);
}

extern void	panic_halt();

void
do_panic(const char *fmt, va_list adx)
{
	(void) callb_execute_class(CB_CL_PANIC, NULL);
	(*psm_notifyf)(PSM_PANIC_ENTER);
	if (setup_panic(fmt, adx) == 0)
		complete_panic();

	if (CPU->cpu_id == panic_cpu.cpu_id) {
		if (CPU->cpu_id == BOOT_CPU)
			mdboot(A_REBOOT, AD_BOOT, NULL);
		else {
			cpu_reboot = BOOT_CPU;
			membar_producer();
		}
		/*CONSTANTCONDITION*/
		while (1);
	}
	else
		panic_halt();
}

int
setup_panic(const char *fmt, va_list adx)
{
	extern int conslogging;
	int s;
	kthread_id_t tp;
	int i;

#ifndef LOCKNEST	/* No telling what locks we hold when we call panic */
	noprintf = 1;
	s = splzs();

	/*
	 * Allow only threads on panicking cpu to blow through locks.
	 * Use counter to avoid endless looping if code should panic
	 * before panicstr is set.
	 */
	if (mutex_owner(&paniclock) != curthread)
		mutex_enter(&paniclock);
	if (panicstr) {
		panicstr = (char *)fmt;
		panicargs = adx;
		splx(s);
		return (0);
	}

	curthread->t_preempt++;

	conslogging = 0;
	panic_cpu = *curthread->t_cpu;
	panicstr = (char *)fmt;
	panicargs = adx;
	do_polled_io = 1;	/* cause xx_dump to do polled I/O */
	if (dumphdr) {
		dumphdr->dump_thread = (long)curthread;
		dumphdr->dump_cpu = (long)CPU;
	}
	/*
	 * Insure that we are neither preempted nor swapped out
	 * while setting up the panic state.
	 */
	thread_lock(curthread);
	curthread->t_bound_cpu = CPU;
	curthread->t_schedflag |= TS_DONT_SWAP;
	thread_unlock(curthread);
	panic_thread = curthread;

	if (panic_lbolt == 0)
		panic_lbolt = lbolt;

	/*
	 * Order is important here. We need the cross-call to set
	 * the basespl. Then all interrupts below level 9 are taken on
	 * the panic cpu. clock_thread is the exception.
	 */
#ifdef MP
	xc_capture_cpus(CPUSET_ALL_BUT(panic_cpu.cpu_id));
	xc_release_cpus();
	for (i = 0; i < NCPU; i++) {
		if (i != panic_cpu.cpu_id &&
		    cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS)) {
			cpu[i]->cpu_flags |= CPU_QUIESCED;
		}
	}
#endif MP

	if (clock_thread)
		clock_thread->t_bound_cpu = CPU;

#ifdef MP
	/*
	 * panic_halt() will lower priority to 1 to let most interrupts
	 * come in. The panic cpu will set pri to 0 so that the focal
	 * cpu for interrupt processing will become the panic cpu (for
	 * those machines that use focal cpus
	 */
	CPUSET_DEL(cpus_to_enable, panic_cpu.cpu_id);
	xc_call_debug(NULL, NULL, NULL, X_CALL_HIPRI,
	    CPUSET_ALL_BUT(panic_cpu.cpu_id), (int (*)())panic_halt);
#endif MP

	/*
	 * Pass this point if the clockthread thread runnable but
	 * not ON_PROC then it will take interrupts on panic_cpu.
	 * Otherwise we create new clock thread to take interrupts
	 * on panic_cpu
	 */
	if (clock_thread &&
	    (clock_thread->t_state == TS_SLEEP || clock_thread->t_lock)) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			tp->t_pri = clock_thread->t_pri;
			tp->t_cpu = CPU;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			panic_clock_thread = clock_thread;
			clock_thread = tp;
		} else {
			splx(s);
			return (-1);
		}
	}

	/*
	 * If on interrupt stack, allocate new interrupt thread
	 * stack
	 */
	if ((CPU->cpu_on_intr)) {
		if (CPU->cpu_intr_thread) {
			tp = CPU->cpu_intr_thread;
			CPU->cpu_intr_thread = tp->t_link;
			CPU->cpu_intr_stack = tp->t_stk -= SA(MINFRAME);
			CPU->cpu_on_intr = 0;
		} else {
			splx(s);
			return (-1);
		}
	}

	splx(s);
	setup_panic_done = 1;
	return (0);
#endif	/* LOCKNEST */
}


static void
complete_panic(void)
{
	extern void prf(const char *, va_list, vnode_t *, int);
	int s;
	static int in_sync = 0;
	static int in_dumpsys = 0;
	extern int sync_timeout;

	s = splzs();

	noprintf = 0;   /* turn printfs on */

	if (curthread->t_cred == NULL)
		curthread->t_cred = kcred;

	printf("panic: ");
	prf(panicstr, panicargs, (vnode_t *)NULL, PRW_CONS | PRW_BUF);
	printf("\n");

	if ((boothowto & RB_DEBUG) && (nopanicdebug == 0))
		debug_enter((char *)NULL);

	if (!in_sync) {
		in_sync = 1;
		vfs_syncall();
	} else {
		/* Assume we paniced while syncing and avoid timeout */
		sync_timeout = 0;
	}

	(void) setjmp(&panic_thread->t_pcb);	/* save stack ptr for dump */
	panic_regs = panic_thread->t_pcb;

	if (!in_dumpsys) {
		in_dumpsys = 1;
		dumpsys();
	}
	splx(s);
}

/*
 * Allow interrupt threads to run only don't allow them to nest
 * save the current interrupt count
 */
void
panic_hook()
{
	int s, i, j;

	if (!setup_panic_done)
		return;

	if (CPU->cpu_id != panic_cpu.cpu_id &&
	    !(CPU->cpu_on_intr || curthread->t_flag & T_INTR_THREAD)) {
		for (;;);
	}

	if (panic_thread != curthread ||
	    CPU->cpu_on_intr > panic_cpu.cpu_on_intr)
		return;

	s = spl0();
	return_instr();
	splx(s);

	/* Let each CPU enable interrupts for a short while in turn */
	for (i = 0; i < NCPU; i++) {
		if (CPU_IN_SET(cpus_to_enable, i) &&
		    cpu[i] != NULL &&
		    (cpu[i]->cpu_flags & CPU_EXISTS)) {
			cpu_intr_enabled = i;
			membar_producer();
			/* Wait for a maximum of 10 ticks */
			for (j = 0; j < TICK_TO_USEC(1); j++) {
				if (cpu_intr_enabled == -1)
					break;
				tenmicrosec();
			}
			if (j == TICK_TO_USEC(1))
				/* Don't attempt this CPU again */
				CPUSET_DEL(cpus_to_enable, i);
		}
	}
}

/*
 * Machine dependent code to reboot.
 * "mdep" is interpreted as a character pointer; if non-null, it is a pointer
 * to a string to be used as the argument string when rebooting.
 */
/*ARGSUSED*/
void
mdboot(int cmd, int fcn, char *mdep)
{
	extern void reset_leaves(void);

#ifdef lint
	mdep = mdep;
	cmd = cmd;
#endif

	if (!panicstr) {
		kpreempt_disable();
		affinity_set(0);
	}

	/*
	 * XXX - rconsvp is set to NULL to ensure that output messages
	 * are sent to the underlying "hardware" device using the
	 * monitor's printf routine since we are in the process of
	 * either rebooting or halting the machine.
	 */
	rconsvp = NULL;

	/*
	 * Print the reboot message now, before pausing other cpus.
	 * There is a race condition in the printing support that
	 * can deadlock multiprocessor machines.
	 */
	if (!(fcn == AD_HALT || fcn == AD_POWEROFF))
		prom_printf("rebooting...\n");

	(void) spl6();
	reset_leaves();		/* call all drivers reset entry point	*/

#ifdef MP
	if (!panicstr) {
		mutex_enter(&cpu_lock);
		pause_cpus(NULL);
		mutex_exit(&cpu_lock);
	}
#endif
	(void) spl8();
	(*psm_shutdownf)(cmd, fcn);

	if (fcn == AD_HALT || fcn == AD_POWEROFF)
		halt((char *)NULL);
	else
		prom_reboot("");
	/*NOTREACHED*/
}

/*
 * Machine-dependent portion of dump-checking;
 * verify that a physical address is valid.
 */
int
dump_checksetbit_machdep(addr)
	u_longlong_t	addr;
{
	struct memlist	*pmem;

	for (pmem = phys_install; pmem; pmem = pmem->next) {
		if (pmem->address <= addr &&
		    addr < (pmem->address + pmem->size))
			return (1);
	}
	return (0);
}

/*
 * Machine-dependent portion of dump-checking;
 * mark all pages for dumping.
 */
void
dump_allpages_machdep(void)
{
	u_int	i, j;
	struct memlist	*pmem;

	for (pmem = bootops->boot_mem->physavail; pmem; pmem = pmem->next) {
		i = mmu_btop(pmem->address);
		j = i + mmu_btop(pmem->size);
		for (; i < j; i++)
			dump_addpage(i);
	}
}

/*
 * Temporary address we use to map things when dumping them.
 */
caddr_t dump_addr;
caddr_t cur_dump_addr;

static	int	npgs = 0;
static	int	dbn = 0;

int
dump_page(vp, pg, bn)
	struct vnode *vp;
	int pg;
	int bn;
{
	register caddr_t addr;
	register int err = 0;

	addr = cur_dump_addr;
	hat_devload(kas.a_hat, addr, MMU_PAGESIZE, pg, PROT_READ, 0);
	cur_dump_addr += MMU_PAGESIZE;

	if (npgs == 0) {
		/*
		 * Remember were we have to start writing on the disk.
		 */
		dbn = bn;
	}
	npgs++;

	if (npgs == DUMPPAGES) {

		/*
		 * Write to the disk only if we have enough pages to flush.
		 * The dumpsys_doit() procedure will call dump_final() at the
		 * end where we will write out the last remaining set of pages
		 * to the disk
		 */

		addr = dump_addr;
		err = VOP_DUMP(vp, addr, dbn, ptod(npgs));

		/*
		 * Destroy the page mappings that we had setup.
		 */
		hat_unload(kas.a_hat, addr, MMU_PAGESIZE * npgs, 0);

		/*
		 * Restore the virtual address and pages that we have cached.
		 */

		npgs = 0;
		cur_dump_addr = dump_addr;
	}
	return (err);
}

int
dump_final(struct vnode *vp)
{
	int err;
	caddr_t addr = dump_addr;

	if (npgs == 0)
		return (0);
	err = VOP_DUMP(vp, addr, dbn, ptod(npgs));
	hat_unload(kas.a_hat, addr, MMU_PAGESIZE * npgs, 0);
	npgs = 0;
	cur_dump_addr = dump_addr;
	return (err);
}

int
dump_kaddr(vp, kaddr, bn, count)
	struct vnode *vp;
	caddr_t kaddr;
	int bn;
	int count;
{
	register int err = 0;


	while (count > 0 && !err) {
		err = VOP_DUMP(vp, kaddr, bn, ptod(1));
		bn += ptod(1);
		count -= ptod(1);
		kaddr += MMU_PAGESIZE;
	}

	return (err);
}

#if	MP
void
idle_other_cpus()
{
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	xc_capture_cpus(CPUSET_ALL_BUT(cpuid));
}

void
resume_other_cpus()
{
	ASSERT(CPU->cpu_id <= NCPU);

	xc_release_cpus();
}

extern void	mp_halt(char *);

void
stop_other_cpus()
{
	int cpuid = CPU->cpu_id;

	ASSERT(cpuid <= NCPU);

	/*
	 * xc_call_debug will make all other cpus to execute mp_halt and
	 * then return immediately without waiting for acknowledgment
	 */
	xc_call_debug(NULL, NULL, NULL, X_CALL_HIPRI, CPUSET_ALL_BUT(cpuid),
	    (int (*)())mp_halt);
}

#else MP
void idle_other_cpus() {}
void resume_other_cpus() {}
void stop_other_cpus() {}
#endif	MP

/*
 *	Machine dependent abort sequence handling
 */
void
abort_sequence_enter(char *msg)
{
	if (abort_enable != 0)
		debug_enter(msg);
}

/*
 * Enter debugger.  Called when the user types ctrl-alt-d or whenever
 * code wants to enter the debugger and possibly resume later.
 */
void
debug_enter(msg)
	char	*msg;		/* message to print, possibly NULL */
{
	int s;

	if (msg)
		prom_printf("%s\n", msg);
	s = splzs();

	if (boothowto & RB_DEBUG) {
		int20();
	}
	splx(s);

}

void
reset(void)
{
	extern	void pc_reset(void);
	ushort 	*bios_memchk;

	if (bios_memchk = (ushort *)psm_map_phys(0x472, sizeof (ushort),
		PROT_READ|PROT_WRITE))
		*bios_memchk = 0x1234;	/* bios memmory check disable */

	pc_reset();
}

/*
 * Halt the machine and return to the monitor
 */
void
halt(char *s)
{
#ifdef	MP
	stop_other_cpus();	/* send stop signal to other CPUs */
#endif
	if (s)
		prom_printf("(%s) \n", s);
	prom_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Called via cross-call from setup_panic().
 */
void
panic_halt()
{
	splx(ipltospl(CLOCK_LEVEL - 1));
	/*CONSTANTCONDITION*/
	while (1) {
		while ((cpu_intr_enabled != CPU->cpu_id) &&
		    (cpu_reboot != CPU->cpu_id))
			;
		if (cpu_reboot == CPU->cpu_id)
			mdboot(A_REBOOT, AD_BOOT, NULL);
		splx(0);
		return_instr();
		splx(ipltospl(CLOCK_LEVEL - 1));
		cpu_intr_enabled = -1;
		membar_producer();
	}
}
/*
 * Enter monitor.  Called via cross-call from stop_other_cpus().
 */
void
mp_halt(char *msg)
{
	if (msg)
		prom_printf("%s\n", msg);

	/*CONSTANTCONDITION*/
	while (1);
}

#ifndef SAS
/*
 * Console put and get character routines.
 * XXX	NEEDS REVIEW w.r.t MP (Sherif?)
 *	(Also fix all the calls to this routine in this file.)
 */
/*ARGSUSED1*/
void
cnputc(register int c, int device_in_use)
{
	register int s;

	s = splzs();
	if (c == '\n')
		prom_putchar('\r');
	prom_putchar(c);
	splx(s);
}

int
cngetc()
{
	return ((int)prom_getchar());
}

/*
 * Get a character from the console.
 */
getchar()
{
	register c;

	c = cngetc();
	if (c == '\r')
		c = '\n';
	cnputc(c, 0);
	return (c);
}

/*
 * Get a line from the console.
 */
void
gets(char *cp)
{
	register char *lp;
	register int c;

	lp = cp;
	for (;;) {
		c = getchar() & 0177;
		switch (c) {

		case '\n':
			*lp++ = '\0';
			return;

		case 0177:
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '\b':
			cnputc(' ', 0);
			cnputc('\b', 0);
			/*FALLTHROUGH*/
		case '#':
			lp--;
			if (lp < cp)
				lp = cp;
			continue;

		case 'u'&037:
			lp = cp;
			cnputc('\n', 0);
			continue;

		default:
			*lp++ = (char)c;
			break;
		}
	}
}

#endif !SAS

#ifdef XXX
/*
 * Called by assembler files that want to debug_enter
 */
call_debug_from_asm()
{
	int	s;

	s = splzs();		/* protect against re-entrancy */
	(void) setjmp(&curthread->t_pcb); /* for kadb */
	debug_enter((char *)NULL);
	splx(s);
}
#endif /* XXX */

/*
 * Allocate threads and stacks for interrupt handling.
 */
#define	NINTR_THREADS	(LOCK_LEVEL-1)	/* number of interrupt threads */
#ifdef REDCHECK
#define	INTR_STACK_SIZE	(roundup(8192 + PAGESIZE, PAGESIZE))
#else
#define	INTR_STACK_SIZE	(roundup(8192, PAGESIZE))
#endif /* REDCHECK */

void
init_intr_threads(cp)
	struct cpu *cp;
{
	int i;

	for (i = 0; i < NINTR_THREADS; i++)
		(void) thread_create_intr(cp);

	cp->cpu_intr_stack = (caddr_t)segkp_get(segkp, INTR_STACK_SIZE,
		KPD_HASREDZONE | KPD_NO_ANON | KPD_LOCKED) +
		INTR_STACK_SIZE - SA(MINFRAME);
}

void
init_clock_thread()
{
	kthread_id_t tp;

	/*
	 * Create clock interrupt thread.
	 * The state is initially TS_FREE.  Think of this thread on
	 * a private free list until it runs.
	 */
	tp = thread_create(NULL, INTR_STACK_SIZE, NULL, NULL, 0,
		&p0, TS_FREE, 0);
	/*LINTED: statement has null effect */
	tp->t_stk -= SA(MINFRAME);
	tp->t_flag |= T_INTR_THREAD;	/* for clock()'s tick checking */
	clock_thread = tp;
}

/*
 * Called from dumpsys() to ensure the kvtopdata is in the dump.
 *
 * XXX  Not entirely convinced we need to do this specially ..
 */
void
dump_kvtopdata(void)
{
	caddr_t		i, j;
	u_int		pfn;

	i = (caddr_t)(((u_int)&kvtopdata) & MMU_PAGEMASK);
	for (j = (caddr_t)&kvtopdata + sizeof (kvtopdata); i < j;
	    i += MMU_PAGESIZE) {
		pfn = hat_getpfnum(kas.a_hat, i);
		dump_addpage(pfn);
	}
}

/*
 * XXX These probably ought to live somewhere else
 * XXX They are called from mem.c
 */

/*
 * Convert page frame number to an OBMEM page frame number
 * (i.e. put in the type bits -- zero for this implementation)
 */
pfn_t
impl_obmem_pfnum(pfn_t pf)
{
	return (pf);
}

#if	NM_DEBUG
int nmi_test = 0;	/* checked in intentry.s during clock int */
int nmtest = -1;
nmfunc1(arg, rp)
int	arg;
struct regs *rp;
{
	printf("nmi called with arg = %x, regs = %x\n", arg, rp);
	nmtest += 50;
	if (arg == nmtest) {
		printf("ip = %x\n", rp->r_pc);
		return (1);
	}
	return (0);
}

#endif

/*
 * Return the number of ticks per second of the highest
 * resolution clock or free running timer in the system.
 * XXX - if/when we provide higher resolution data, update this.
 */
getclkfreq()
{
	return (hz);
}

/* XXX stubbed stuff that is mainly here for debugging */
/* XXX   BOOTOPS definition until we get the new boot */
/* XXX   Things we cannot afford to forget! */



#include <sys/bootsvcs.h>

/* Hacked up initialization for initial kernel check out is HERE. */
/* The basic steps are: */
/*	kernel bootfuncs definition/initialization for KADB */
/*	kadb bootfuncs pointer initialization */
/*	kd putchar/getchar (interrupts disabled) */

/* KD font info provided by the boot */

unsigned char *egafont_p[5];

void
get_font_ptrs()
{
	unsigned char **k_fontptr;
	int i;

	k_fontptr = (unsigned char **)(get_fonts)();
	if ((int)k_fontptr != 0) {
		for (i = 0; i < 5; i++) {
			egafont_p[i] = (unsigned char *)
				((int)(*k_fontptr++) & 0xfffff);
		}
	}
}

/* kadb bootfuncs pointer initialization */

/*
 *	Redirect KADB's pointer to bootfuncs into the kernel,
 *	when the boot is about to go away.
 *	Make the kernel's pointer to boot services NULL.
 */

static void kadb_error(int n);

static char *
kadb_error1()
{
	kadb_error(1);
	return (NULL);
}

static char *
kadb_error2()
{
	kadb_error(2);
	return (NULL);
}

static char *
kadb_error3()
{
	kadb_error(3);
	return (NULL);
}

static size_t
kadb_error4()
{
	kadb_error(4);
	return (0);
}

static void *
kadb_error5()
{
	kadb_error(5);
	return (NULL);
}

static int
kadb_error6()
{
	kadb_error(6);
	return (0);
}

static int
kadb_error10()
{
	kadb_error(10);
	return (0);
}

static int
kadb_error11()
{
	kadb_error(11);
	return (0);
}

static void *
kadb_error12()
{
	kadb_error(12);
	return (0);
}

static int
kadb_error13()
{
	kadb_error(13);
	return (0);
}

static ssize_t
kadb_error14()
{
	kadb_error(14);
	return (0);
}

static off_t
kadb_error15()
{
	kadb_error(15);
	return (0);
}

static int
kadb_error16()
{
	kadb_error(16);
	return (0);
}

static int
kadb_error17()
{
	kadb_error(17);
	return (0);
}

static char *
kadb_error18()
{
	kadb_error(18);
	return (NULL);
}

static paddr_t
kadb_error19()
{
	kadb_error(19);
	return (NULL);
}

static u_int
kadb_error20()
{
	kadb_error(20);
	return (0);
}

static void
kadb_error(int n)
{
	prom_printf("Kadb service routine number %d called in ERROR!\n", n);
}

#ifdef GPROF

extern int sysp_getchar();
extern void sysp_putchar(int);
extern int sysp_ischar();

#else

int
sysp_getchar()
{
	int i;
	int s;

	s = clear_int_flag();
	i = (*getcharptr)();
	restore_int_flag(s);
	return (i);
}

void
sysp_putchar(int arg)
{
	int s;

	s = clear_int_flag();
	(void) (*putcharptr)(arg);
	restore_int_flag(s);
}

int
sysp_ischar()
{
	int i;
	int s;

	s = clear_int_flag();
	i = (*ischarptr)();
	restore_int_flag(s);
	return (i);
}

#endif /* !GPROF */

static void
sysp_printf(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	prom_printf(fmt, ap);
	va_end(ap);
}

int
goany(void)
{
	prom_printf("Type any key to continue ");
	(void) prom_getchar();
	prom_printf("\n");
	return (1);
}

static struct boot_syscalls kern_sysp = {
	sysp_printf,	/*	int	(*printf)();	0  */
	kadb_error1,	/*	char	*(*strcpy)();	1  */
	kadb_error2,	/*	char	*(*strncpy)();	2  */
	kadb_error3,	/*	char	*(*strcat)();	3  */
	kadb_error4,	/*	size_t	(*strlen)();	4  */
	kadb_error5,	/*	char	*(*memcpy)();	5  */
	kadb_error6,	/*	char	*(*memcmp)();	6  */
	sysp_getchar,	/*	unchar	(*getchar)();	7  */
	sysp_putchar,	/*	int	(*putchar)();	8  */
	sysp_ischar,	/*	int	(*ischar)();	9  */
	kadb_error10,	/*	int	(*goany)();	10 */
	kadb_error11,	/*	int	(*gets)();	11 */
	kadb_error12,	/*	int	(*memset)();	12 */
	kadb_error13,	/*	int	(*open)();	13 */
	kadb_error14,	/*	int	(*read)();	14 */
	kadb_error15,	/*	int	(*lseek)();	15 */
	kadb_error16,	/*	int	(*close)();	16 */
	kadb_error17,	/*	int	(*fstat)();	17 */
	kadb_error18,	/*	char	*(*malloc)();	18 */
	kadb_error19,	/*	char	*(*get_fonts)();19 */
	kadb_error20,	/*	u_int (*vlimit)();	20 */
};

void
kadb_uses_kernel()
{
	extern struct boot_syscalls **syspp;
	extern struct bootops **bootopsp;

	*syspp = sysp = &kern_sysp;

	/*
	 * We're done with boot.
	 */
	*bootopsp = (struct bootops *)0;
	bootops = (struct bootops *)NULL;
	(void) clear_bootpde(CPU);
}

/*
 *	the interface to the outside world
 */

/*
 * poll_port -- wait for a register to achieve a
 *		specific state.  Arguments are a mask of bits we care about,
 *		and two sub-masks.  To return normally, all the bits in the
 *		first sub-mask must be ON, all the bits in the second sub-
 *		mask must be OFF.  If about seconds pass without the register
 *		achieving the desired bit configuration, we return 1, else
 *		0.
 */
int
poll_port(register ushort port, ushort mask, ushort onbits, ushort offbits)
{
	register int i;
	register ushort maskval;

	for (i = 500000; i; i--) {
		maskval = inb(port) & mask;
		if (((maskval & onbits) == onbits) &&
			((maskval & offbits) == 0))
			return (0);
		drv_usecwait(10);
	}
	return (1);
}

int
getlongprop(id, prop)
char *prop;
{
	if (id == 0) {
		if (strcmp(prop, "name") == 0)
			return ((int)"i86pc");
		prom_printf("getlongprop: root property '%s' not defined.\n",
			prop);
	}
	return (0);
}

int ticks_til_clock;
int unix_tick;

#ifdef  MP
/*
 * set_idle_cpu is called from idle() when a CPU becomes idle.
 */
/*LINTED: static unused */
static u_int last_idle_cpu;

/*ARGSUSED*/
void
set_idle_cpu(int cpun)
{
	last_idle_cpu = cpun;
	(*psm_set_idle_cpuf)(cpun);
}

/*
 * unset_idle_cpu is called from idle() when a CPU is no longer idle.
 */
/*ARGSUSED*/
void
unset_idle_cpu(int cpun)
{
	(*psm_unset_idle_cpuf)(cpun);
}
#endif /* MP */

/*
 * This routine is almost correct now, but not quite.  It still needs the
 * equivalent concept of "hres_last_tick", just like on the sparc side.
 * The idea is to take a snapshot of the hi-res timer while doing the
 * hrestime_adj updates under hres_lock in locore, so that the small
 * interval between interrupt assertion and interrupt processing is
 * accounted for correctly.  Once we have this, the code below should
 * be modified to subtract off hres_last_tick rather than hrtime_base.
 *
 * I'd have done this myself, but I don't have source to all of the
 * vendor-specific hi-res timer routines (grrr...).  The generic hook I
 * need is something like "gethrtime_unlocked()", which would be just like
 * gethrtime() but would assume that you're already holding CLOCK_LOCK().
 * This is what the GET_HRTIME() macro is for on sparc (although it also
 * serves the function of making time available without a function call
 * so you don't take a register window overflow while traps are diasbled).
 */
extern volatile hrtime_t hres_last_tick;
void
pc_gethrestime(timestruc_t *tp)
{
	int lock_prev;
	timestruc_t now;
	int nslt;		/* nsec since last tick */
	int adj;		/* amount of adjustment to apply */

loop:
	lock_prev = hres_lock;
	now = hrestime;
	nslt = (int)(gethrtime() - hres_last_tick);
	if (nslt < 0)
		nslt += nsec_per_tick;
	now.tv_nsec += nslt;
	if (hrestime_adj != 0) {
		if (hrestime_adj > 0) {
			adj = (nslt >> ADJ_SHIFT);
			if (adj > hrestime_adj)
				adj = (int)hrestime_adj;
		} else {
			adj = -(nslt >> ADJ_SHIFT);
			if (adj < hrestime_adj)
				adj = (int)hrestime_adj;
		}
		now.tv_nsec += adj;
	}
	if (now.tv_nsec >= NANOSEC) {
		now.tv_nsec -= NANOSEC;
		now.tv_sec++;
	}
	if ((hres_lock & ~1) != lock_prev)
		goto loop;

	*tp = now;
}

/*
 * Initialize kernel thread's stack
 */

caddr_t
thread_stk_init(caddr_t stk)
{
	return (stk - SA(MINFRAME));
}

/*
 * Initialize lwp's kernel stack.
 */
caddr_t
lwp_stk_init(klwp_t *lwp, caddr_t stk)
{
	caddr_t oldstk;

	oldstk = stk;
	stk -= SA(sizeof (struct regs) + MINFRAME);
	bzero((caddr_t)stk, (size_t)(oldstk - stk));
	lwp->lwp_regs = (void *)(stk + REGOFF);

	return (stk);

}

/*ARGSUSED*/
void
lwp_stk_fini(klwp_t *lwp)
{}
