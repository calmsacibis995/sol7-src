
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)archdep.c	1.44	98/02/12 SMI"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/vmparam.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/psw.h>
#include <sys/siginfo.h>
#include <sys/cpuvar.h>
#include <sys/asm_linkage.h>
#include <sys/kmem.h>
#include <sys/errno.h>
#include <sys/bootconf.h>
#include <sys/archsystm.h>
#include <sys/debug.h>
#include <sys/elf.h>
#include <sys/spl.h>
#include <sys/time.h>
#include <sys/atomic.h>
#include <sys/lockstat.h>

extern struct bootops *bootops;
extern void fp_save();
extern int fperr_reset(void);
extern void fpdisable(void);
extern void fp_free();


/*
 * Advertised via /etc/system.
 * Does execution of coff binaries bring in /usr/lib/cbcp.
 */
int enable_cbcp = 1;

/*
 * Set floating-point registers.
 */
void
setfpregs(klwp_id_t lwp, fpregset_t *fp)
{

	register struct pcb *pcb;
	register fpregset_t *pfp;

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		if (!(pcb->pcb_fpu.fpu_flags & FPU_VALID)) {
			/*
			 * FPU context is still active, release the
			 * ownership.
			 */
			fp_free(&pcb->pcb_fpu);
		}
		(void) kcopy(fp, pfp, sizeof (struct fpu));
		pcb->pcb_fpu.fpu_flags |= FPU_VALID;
		/*
		 * If we are changing the fpu_flags in the current context,
		 * disable floating point (turn on CR0_TS bit) to track
		 * FPU_VALID after clearing any errors (frstor chokes
		 * otherwise)
		 */
		if (lwp == ttolwp(curthread)) {
			(void) fperr_reset();
			fpdisable();
		}
	}
}

/*
 * Get floating-point registers.  The u-block is mapped in here (not by
 * the caller).
 */
void
getfpregs(klwp_id_t lwp, fpregset_t *fp)
{
	register fpregset_t *pfp;
	register struct pcb *pcb;
	extern int fpu_exists;

	pcb = &lwp->lwp_pcb;
	pfp = &pcb->pcb_fpu.fpu_regs;

	kpreempt_disable();
	if (pcb->pcb_fpu.fpu_flags & FPU_EN) {
		/*
		 * If we have FPU hw and the thread's pcb doesn't have
		 * a valid FPU state then get the state from the hw.
		 */
		if (fpu_exists && ttolwp(curthread) == lwp &&
		    !(pcb->pcb_fpu.fpu_flags & FPU_VALID))
			fp_save(&pcb->pcb_fpu); /* get the current FPU state */
		(void) kcopy(pfp, fp, sizeof (struct fpu));
	}
	kpreempt_enable();
}

/*
 * Return the general registers
 */
void
getgregs(klwp_id_t lwp, gregset_t rp)
{
	register greg_t *reg;

	reg = (greg_t *)lwp->lwp_regs;
	bcopy(reg, rp, sizeof (gregset_t));
}

/*
 * Return the user-level PC.
 * If in a system call, return the address of the syscall trap.
 */
greg_t
getuserpc()
{
	greg_t eip = lwptoregs(ttolwp(curthread))->r_eip;
	if (curthread->t_sysnum)
		eip -= 7;	/* size of an lcall instruction */
	return (eip);
}

/*
 * Set general registers.
 */
void
setgregs(klwp_id_t lwp, gregset_t rp)
{
	register struct regs *reg;
	void chksegregval();

	reg = lwptoregs(lwp);

	/*
	 * Only certain bits of the EFL can be modified.
	 */
	rp[EFL] = (reg->r_ps & ~PSL_USERMASK) | (rp[EFL] & PSL_USERMASK);

	/* copy saved registers from user stack */
	bcopy(rp, reg, sizeof (gregset_t));

	/*
	 * protect segment registers from non-user privilege levels,
	 * and GDT selectors other than FPESEL
	 */
	chksegregval(&(reg->r_fs));
	chksegregval(&(reg->r_gs));
	chksegregval(&(reg->r_ss));
	chksegregval(&(reg->r_cs));
	chksegregval(&(reg->r_ds));
	chksegregval(&(reg->r_es));
	/*
	 * Set the flag lwp_gpfault to catch GP faults when going back
	 * to user mode.
	 */
	lwp->lwp_gpfault = 1;
}

/*
 * Get a pc-only stacktrace.  Used for kmem_alloc() buffer ownership tracking.
 * Returns MIN(current stack depth, pcstack_limit).
 */
int
getpcstack(uintptr_t *pcstack, int pcstack_limit)
{
	struct frame *stacktop = (struct frame *)curthread->t_stk;
	struct frame *prevfp = (struct frame *)KERNELBASE;
	struct frame *fp = (struct frame *)getfp();
	int depth = 0;

	while (fp > prevfp && fp < stacktop && depth < pcstack_limit) {
		pcstack[depth++] = (u_int)fp->fr_savpc;
		prevfp = fp;
		fp = (struct frame *)fp->fr_savfp;
	}
	return (depth);
}

/*
 * Check segment register value that will be restored when going to
 * user mode.
 */

void
chksegregval(int *srp)
{
	register int sr;

	sr = *srp;
	if ((sr & 0x0000FFFF) != 0 && (sr & 0x0000FFFF) != FPESEL)
		*srp |= 7;	/* LDT and RPL 3 */
	/* else null selector or FPESEL OK */
}

/*
 * The following ELF header fields are defined as processor-specific
 * in the V8 ABI:
 *
 *	e_ident[EI_DATA]	encoding of the processor-specific
 *				data in the object file
 *	e_machine		processor identification
 *	e_flags			processor-specific flags associated
 *				with the file
 */

/*
 * The value of at_flags reflects a platform's cpu module support.
 * at_flags is used to check for allowing a binary to execute and
 * is passed as the value of the AT_FLAGS auxiliary vector.
 */
int at_flags = 0;

/*
 * Check the processor-specific fields of an ELF header.
 *
 * returns 1 if the fields are valid, 0 otherwise
 */
/*ARGSUSED2*/
int
elfheadcheck(
	unsigned char e_data,
	Elf32_Half e_machine,
	Elf32_Word e_flags)
{
	if ((e_data != ELFDATA2LSB) || (e_machine != EM_386))
		return (0);
	return (1);
}

/*
 * Set the processor-specific fields of an ELF header.
 */
void
elfheadset(
	unsigned char *e_data,
	Elf32_Half *e_machine,
	Elf32_Word *e_flags)
{
	*e_data = ELFDATA2LSB;
	*e_machine = EM_386;
	*e_flags = 0;
}

/*
 *	sync_icache() - this is called
 *	in proc/fs/prusrio.c. x86 has an unified cache and therefore
 *	this is a nop.
 */
/* ARGSUSED */
void
sync_icache(caddr_t addr, u_int len)
{
	/* Do nothing for now */
}

int
__ipltospl(int ipl)
{
	return (ipltospl(ipl));
}

u_int
get_profile_pc(void *p)
{
	struct regs *rp = (struct regs *)p;

	if (USERMODE(rp->r_cs))
		return (0);
	return (rp->r_pc);
}

/*
 * Start and end events on behalf of the lockstat driver.
 * Until we have a cheap, reliable, lock-free timing source
 * on *all* x86 machines we'll have to make do with a
 * software approximation based on lbolt.
 */
int
lockstat_event_start(uintptr_t lp, ls_pend_t *lpp)
{
	if (casptr((void **)&lpp->lp_lock, NULL, (void *)lp) == NULL) {
		lpp->lp_start_time = (hrtime_t)lbolt;
		return (0);
	}
	return (-1);
}

hrtime_t
lockstat_event_end(ls_pend_t *lpp)
{
	clock_t ticks = lbolt - (clock_t)lpp->lp_start_time;

	lpp->lp_lock = 0;
	if (ticks == 0)
		return (0);
	return (TICK_TO_NSEC((hrtime_t)ticks));
}


/*
 * These functions are not used on this architecture, but are
 * declared in common/sys/copyops.h.  Definitions are provided here
 * but they should never be called.
 */
/*ARGSUSED*/
int
default_fuword64(const void *addr, uint64_t *valuep)
{
	ASSERT(0);
	return (-1);
}

/*ARGSUSED*/
int
default_suword64(void *addr, uint64_t value)
{
	ASSERT(0);
	return (-1);
}
