/*
 * Copyright (c) 1995 by Sun Microsystems, Inc.
 */

#pragma ident "@(#)locore.s	1.14	98/02/02 SMI"

/*
 * Assembly language support for Fusion kadb64
 * This is very similar to the kadb for sun4u. Instead of using #ifdef
 * all over the place, we created a seperate file for kadb64.
 *
 * Fusion differs from other SPARC machines in that the MMU trap handling
 * code is done in software. For this and other reasons, it is simpler to
 * use the PROM's trap table while running in the debugger than to have
 * kadb manage its own trap table. Breakpoint/L1-A traps work by downloading
 * a FORTH 'defer word' to the PROM, which gives it an address to jump to
 * when it gets a trap it doesn't recognize, e.g. a kadb-induced breakpoint
 * (N.B. PROM breakpoints are independent of this scheme and work as before).
 * The PROM saves some of the register state at the time of the breakpoint
 * trap and jumps to 'start' in kadb, where we save off everything else and
 * proceed as usual.
 */

#if defined(lint)
#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/debug/debugger.h>
#endif	/* lint */

#include <sys/param.h>
#include <sys/vmparam.h>
#include <sys/errno.h>
#include <sys/asm_linkage.h>
#include <sys/clock.h>
#include <sys/cpu.h>
#include <sys/intreg.h>
#include <sys/eeprom.h>
#include <sys/debug/debug.h>
#include <sys/mmu.h>
#include <sys/pcb.h>
#include <sys/pte.h>
#include <sys/spitasi.h>
#include <sys/privregs.h>
#include <sys/trap.h>
#include <sys/scb.h>
#include <sys/machparam.h>
#include <sys/machtrap.h>
#include <sys/machthread.h>
#include <sys/spitregs.h>

#if !defined(lint)
#include <sys/xc_impl.h>
#endif	/* !defined(lint) */

#define WINDOWSIZE64    (16*8)          /* size of window save area */
#define V9BIAS64        (2048-1)        /* v9 abi stack bias */

/*
 * Macros for saving/restoring registers.
 */
#ifdef	SAVE_GLOBALS
#undef	SAVE_GLOBALS
#endif
#define	SAVE_GLOBALS(RP) \
	stx	%g1, [RP + R_G1]; \
	stx	%g2, [RP + R_G2]; \
	stx	%g3, [RP + R_G3]; \
	stx	%g4, [RP + R_G4]; \
	stx	%g5, [RP + R_G5]; \
	stx	%g6, [RP + R_G6]; \
	stx	%g7, [RP + R_G7];

#ifdef	RESTORE_GLOBALS
#undef	RESTORE_GLOBALS
#endif
#define	RESTORE_GLOBALS(RP) \
	ld	[RP + R_Y], %g1; \
	mov	%g1, %y; \
	ldx	[RP + R_G1], %g1; \
	ldx	[RP + R_G2], %g2; \
	ldx	[RP + R_G3], %g3; \
	ldx	[RP + R_G4], %g4; \
	ldx	[RP + R_G5], %g5; \
	ldx	[RP + R_G6], %g6; \
	ldx	[RP + R_G7], %g7;

#ifdef	SAVE_WINDOW
#undef	SAVE_WINDOW
#endif
#define	SAVE_WINDOW(SBP) \
	st	%l0, [SBP + 0]; \
	st	%l1, [SBP + 4]; \
	st	%l2, [SBP + 8]; \
	st	%l3, [SBP + 12]; \
	st	%l4, [SBP + 16]; \
	st	%l5, [SBP + 20]; \
	st	%l6, [SBP + 24]; \
	st	%l7, [SBP + 28]; \
	st	%i0, [SBP + 32]; \
	st	%i1, [SBP + 36]; \
	st	%i2, [SBP + 40]; \
	st	%i3, [SBP + 44]; \
	st	%i4, [SBP + 48]; \
	st	%i5, [SBP + 52]; \
	st	%i6, [SBP + 56]; \
	st	%i7, [SBP + 60];

#ifdef	SAVE_V9WINDOW
#undef	SAVE_V9WINDOW
#endif
#define SAVE_V9WINDOW(SBP) \
        stx     %l0, [SBP + (0*8)]; \
        stx     %l1, [SBP + (1*8)]; \
        stx     %l2, [SBP + (2*8)]; \
        stx     %l3, [SBP + (3*8)]; \
        stx     %l4, [SBP + (4*8)]; \
        stx     %l5, [SBP + (5*8)]; \
        stx     %l6, [SBP + (6*8)]; \
        stx     %l7, [SBP + (7*8)]; \
        stx     %i0, [SBP + (8*8)]; \
        stx     %i1, [SBP + (9*8)]; \
        stx     %i2, [SBP + (10*8)]; \
        stx     %i3, [SBP + (11*8)]; \
        stx     %i4, [SBP + (12*8)]; \
        stx     %i5, [SBP + (13*8)]; \
        stx     %i6, [SBP + (14*8)]; \
        stx     %i7, [SBP + (15*8)];
 
#ifdef	RESTORE_V9WINDOW
#undef	RESTORE_V9WINDOW
#endif
#define RESTORE_V9WINDOW(SBP) \
        ldx     [SBP + (0*8)], %l0; \
        ldx     [SBP + (1*8)], %l1; \
        ldx     [SBP + (2*8)], %l2; \
        ldx     [SBP + (3*8)], %l3; \
        ldx     [SBP + (4*8)], %l4; \
        ldx     [SBP + (5*8)], %l5; \
        ldx     [SBP + (6*8)], %l6; \
        ldx     [SBP + (7*8)], %l7; \
        ldx     [SBP + (8*8)], %i0; \
        ldx     [SBP + (9*8)], %i1; \
        ldx     [SBP + (10*8)], %i2; \
        ldx     [SBP + (11*8)], %i3; \
        ldx     [SBP + (12*8)], %i4; \
        ldx     [SBP + (13*8)], %i5; \
        ldx     [SBP + (14*8)], %i6; \
        ldx     [SBP + (15*8)], %i7;

#ifdef	RESTORE_WINDOW
#undef	RESTORE_WINDOW
#endif
#define	RESTORE_WINDOW(SBP) \
	ld	[SBP + 0], %l0; \
	ld	[SBP + 4], %l1; \
	ld	[SBP + 8], %l2; \
	ld	[SBP + 12], %l3; \
	ld	[SBP + 16], %l4; \
	ld	[SBP + 20], %l5; \
	ld	[SBP + 24], %l6; \
	ld	[SBP + 28], %l7; \
	ld	[SBP + 32], %i0; \
	ld	[SBP + 36], %i1; \
	ld	[SBP + 40], %i2; \
	ld	[SBP + 44], %i3; \
	ld	[SBP + 48], %i4; \
	ld	[SBP + 52], %i5; \
	ld	[SBP + 56], %i6; \
	ld	[SBP + 60], %i7;

/* save 64-bit state of current window's out regs */
#ifdef SAVE_OUTS
#undef SAVE_OUTS
#endif
#define	SAVE_OUTS(SBP) \
	stx	%o0, [SBP + 0]; \
	stx	%o1, [SBP + 8]; \
	stx	%o2, [SBP + 16]; \
	stx	%o3, [SBP + 24]; \
	stx	%o4, [SBP + 32]; \
	stx	%o5, [SBP + 40]; \
	stx	%o6, [SBP + 48]; \
	stx	%o7, [SBP + 56];

#ifdef RESTORE_OUTS
#undef RESTORE_OUTS
#endif
#define	RESTORE_OUTS(SBP) \
	ldx	[SBP + 0], %o0; \
	ldx	[SBP + 8], %o1; \
	ldx	[SBP + 16], %o2; \
	ldx	[SBP + 24], %o3; \
	ldx	[SBP + 32], %o4; \
	ldx	[SBP + 40], %o5; \
	ldx	[SBP + 48], %o6; \
	ldx	[SBP + 56], %o7;

/*
 * The debug stack. This must be the first thing in the data
 * segment (other than an sccs string) so that we don't stomp
 * on anything important. We get a red zone below this stack
 * for free when the text is write protected.
 */
#define	STACK_SIZE	0x8000

#if !defined(lint)

	.seg	".data"
	.align	16
	.global estack

	.skip	STACK_SIZE
estack:				! end (top) of debugger stack

	.align	16
	.global nwindows, nwin_minus_one
	.global p1275_cif	! 1275 client interface handler

	.global kadb_tt		! %tt for kadb - need if we switch-cpu
	.global kadb_cpu	! cpu that took original trap
on_v8_stk:
	.word	0
nwindows:
	.word   8
nwin_minus_one:
	.word   7
kadb_tt:
	.word	0
kadb_cpu:
	.word	0
p1275_cif:
	.word	0
	.align	8
tmpreg:				! double for 64 bit
	.word	0, 0

	.seg	".bss"
	.align	16
	.global	bss_start
bss_start:

	.seg	".text"
#include "assym.s"
!
! REGOFF must be aligned to allow double word access to r_tstate.
!
#if (REGOFF & 7) != 0
ERROR - struct regs not aligned
#endif

/*
 * The debugger vector table.
 */
	.global	dvec
dvec:
	b,a	.enterkadb	! dv_enter
	.word	trap		! dv_trap
	.word	pagesused	! dv_pages
	.word	scbsync		! dv_scbsync
	.word	DEBUGVEC_VERSION_1	! dv_version
 	.word	callb_format_wrapper	! dv_format
 	.word	callb_arm_wrapper	! dv_arm
 	.word	callb_cpu_change_wrapper	! dv_cpu_change
	.word	0


	.align	8
	.global	dvec64
dvec64:
	b,a	.enterkadb
	nop
	.word	0, trap
	.word	0, pagesused
	.word	0, scbsync	! was scbsync64
	.word	DEBUGVEC_VERSION_1, 0
	.word	0, callb_format 
	.word 	0, callb_arm
	.word	0, callb_cpu_change
	.word	0, 0
	

#endif	/* !defined(lint) */

/*
 * Debugger entry point.
 *
 * Inputs:
 *	%o0 - p1275cif pointer
 *	%o1 - dvec (null)
 *	%o2 - bootops vector
 *	%o3 - ELF boot vector
 *
 * Our tasks are:
 *
 *	save parameters
 *	initialize CPU state registers
 *	clear kadblock
 *	dive into main()
 */
#ifdef	lint
void
start() {}
#else

	.align	16
	.global	start		! start address of kadb text (sorta)

	ENTRY(start)

	! Save the 1275 cif handler and ELF boot vector for later use.

	set	p1275_cif, %g1
	st	%o0, [%g1]
	set	elfbootvec, %g1
	stx	%o3, [%g1]

	! Miscellaneous initialization.

	!wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV+PSTATE_IE, %pstate
	wrpr	%g0, PSTATE_PEF+PSTATE_PRIV+PSTATE_IE, %pstate

	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1
	set	nwin_minus_one, %g2
	st	%g1, [%g2]
	inc	%g1
	set	nwindows, %g2
	st	%g1, [%g2]

	! early_startup(p1275cif, dvec, bootops, elfbootvec)

	call	early_startup		! non-machdep startup code
	nop
	call	startup			! do the rest of the startup work.
	nop
	call	main			! enter the debugger.
	nop

	t	ST_KADB_TRAP

	! In the unlikely event we get here, return to the monitor.

	call	prom_exit_to_mon
	clr	%o0

	SET_SIZE(start)
#endif	/* lint */

/*
 * Enter the debugger. This goes out to the kernel (who owns the
 * trap table) and down into the PROM (because we patched the
 * kernel's ST_KADB_TRAP vector with the PROM's).
 */
#ifdef lint
void
enterkadb() {}
#else

	ENTRY(.enterkadb)
	t	ST_KADB_TRAP
	nop
	retl
	nop
	SET_SIZE(.enterkadb)

#endif	/* lint */

#ifdef lint
int
kadb_1275_wrapper(void *p)
{
	return(kadb_1275_call(p));
}

#else
	ENTRY(kadb_1275_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
        setn    kadb_1275_call, %g1, %o1       	! Call boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     kadb_1275_call, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(kadb_1275_wrapper)
#endif

#ifdef lint
void
kobj_notify_kadb_wrapper(unsigned int u, struct modctl *modp)
{
	/* fixed stack */
	kobj_notify_kadb_deferred_bkpt();
}
#else
	
	ENTRY(kobj_notify_kadb_wrapper)

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
	srl	%i1, 0, %o1

	rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    kobj_notify_kadb_deferred_bkpt, %g1, %o2
        jmpl    %o2, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0

	SET_SIZE(kobj_notify_kadb_wrapper)
#endif
	
#ifdef lint
void
callb_format_wrapper(void *arg)
{
	callb_format(arg);
}

#else
	ENTRY(callb_format_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0

	rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_format, %g1, %o1       	! Call boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_format, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(callb_format_wrapper)
#endif

#ifdef lint
void
callb_arm_wrapper(void)
{
	callb_arm();
}

#else
	ENTRY(callb_arm_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_arm, %g1, %o1       	! Call boot service with ...
        jmpl    %o1, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_arm, %o1
        jmp     %o1
        nop
        /* NOTREACHED */

	SET_SIZE(callb_arm_wrapper)
#endif

#ifdef lint
void
callb_cpu_change_wrapper(int cpuid, kadb_cpu_attr_t what, int arg)
{
	callb_cpu_change(cpuid, what, arg);
}

#else
	ENTRY(callb_cpu_change_wrapper)
	
	andcc    %sp, 0x1, %g0
        bnz	6f
        nop

	save    %sp, -SA(MINFRAME), %sp
	srl     %i0, 0, %o0
	srl	%i1, 0, %o1
	srl	%i2, 0, %o2
1:      rdpr    %pstate, %l1                    ! Get the present pstate value
        andn    %l1, PSTATE_AM, %l2
        wrpr    %l2, 0, %pstate                 ! Set PSTATE.AM to zero
	rdpr	%wstate, %l5
	andn	%l5, WSTATE_MASK, %l6
	wrpr	%l6, WSTATE_KMIX, %wstate	
        setn    callb_cpu_change, %g1, %o4     	! Call boot service with ...
        jmpl    %o4, %o7                        ! ... arg array ptr in %o0
        sub     %sp, STACK_BIAS, %sp            ! delay: Now a 64-bit frame
        add     %sp, STACK_BIAS, %sp            ! back to a 32-bit frame
        wrpr    %l1, 0, %pstate                 ! Set PSTATE.AM to one.
	wrpr	%l5, 0, %wstate
        ret                                     ! Return ...
        restore %o0, %g0, %o0                   ! ... result in %o0
 
6:
        /*
         * 64-bit caller:
         * Just jump to boot-services with the arg array in %o0.
         * boot-services will return to the original 64-bit caller
         */
        set     callb_cpu_change, %o4
        jmp     %o4
        nop
        /* NOTREACHED */

	SET_SIZE(callb_cpu_change_wrapper)
#endif
/*
 * Pass control to the client program.
 */
#ifdef	lint
/*ARGSUSED*/
void
_exitto(int (*addr)())
{}
#else

	ENTRY(_exitto)
	sub	%g0, SA(MINFRAME) - STACK_BIAS, %g1
	save	%sp, %g1, %sp
	set	bootops, %o0		! pass bootops to callee
	ldx	[%o0], %o2
	set	dvec, %o1		! pass dvec address to callee
	set	elfbootvec, %o3		! pass elf bootstrap vector
	ldx	[%o3], %o3
	set	p1275_cif, %o0		! pass CIF to callee
	ld	[%o0], %o0
	rdpr	%pstate, %g1		! This is a 32 bit client
	or	%g1, PSTATE_AM, %g1	! Set address mask
	wrpr	%g0, %g1, %pstate
	jmpl	%i0, %o7		! register-indirect call
	mov	%o0, %o4		! 1210378: Pass cif in %o0 and %o4
	ret
	restore
	SET_SIZE(_exitto)

#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
_exitto64(int (*addr)())
{}
#else

	ENTRY(_exitto64)
	save	%sp, -SA(MINFRAME64), %sp
	set	bootops, %o0		! pass bootops to callee
	ldx	[%o0], %o2
	set	dvec64, %o1		! pass dvec address to callee
	set	elfbootvecELF64, %o3		! pass elf bootstrap vector
	ldx	[%o3], %o3
	set	p1275_cif, %o0		! pass CIF to callee
	ld	[%o0], %o0
	jmpl	%i0, %o7		! register-indirect call
	mov	%o0, %o4		! 1210378: Pass cif in %o0 and %o4
	ret
	restore
	SET_SIZE(_exitto64)

#endif	/* lint */

#ifdef lint
u_int
turn_off_interrupts(void)
{ return(0); }
#else
	ENTRY(turn_off_interrupts)
	rdpr	%pstate, %o0
	andn	%o0, PSTATE_IE, %g1
	retl
	wrpr	%g0, %g1, %pstate
	SET_SIZE(turn_off_interrupts)
#endif /* lint */

#ifdef lint
void
turn_on_interrupts(u_int pstate_save)	
{}
#else
	ENTRY(turn_on_interrupts)
	retl
	wrpr	%g0, %o0, %pstate
	SET_SIZE(turn_on_interrupts)
#endif


/*
 * Kadb's breakpoint/L1-A trap handler. The mechanism differs from that
 * used on other architectures, since the PROM owns the trap table while
 * the debugger is running.
 *
 * The kernel's trap handler is patched with the PROM's 0x17d and 0x17e
 * trap handlers at kernel startup time. Breakpoints and L1-A traps therefore
 * go into the PROM, which bounces to here (because of the defer word we
 * downloaded at kadb startup time). On entry, the trap table is owned by
 * the kernel, since the PROM restored everything to what it was at the time
 * of the trap. We are called at TL=0, and must only use the normal globals
 * until everything is saved (we can use the globals because they were
 * saved in the defer word).
 *
 *	- acquire the lock
 *	- use the PROM's %tba
 *	- save CPU state, register windows
 *	- idle the other CPUs
 *	- call cmd()
 *	- resume the other CPUs
 *	- restore CPU state, register windows
 *	- release lock
 *	- fake return from trap
 */
#ifdef lint
int
trap(void) { return (0); }
#else

	ENTRY(trap)

	! disable interrupts
	!wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV, %pstate
	wrpr	%g0, PSTATE_PEF+PSTATE_PRIV, %pstate
	wrpr	%g0, 0, %tl

	! Try to acquire the mutex.

	set	kadblock, %g1
	ldstub	[%g1], %g2
1:
	tst	%g2			! Is the lock held?
	bnz	2f			! Yes - spin until it's released
	nop
	b,a	4f			! No - onward...
2:
	ldub	[%g1], %g2
3:
	tst	%g2			! Is the lock held?
	bz,a	1b			! lock appears to be free, try again
	ldstub	[%g1], %g2		! delay slot - try to set lock
	ba	3b			! otherwise, spin
	ldub	[%g1], %g2		! delay - reload
4:
	CPU_INDEX(%g1, %g2)		! get CPU id
	set	cur_cpuid, %g2
	st	%g1, [%g2]

	! If nofault is set, we've taken a trap within the debugger itself
	! (e.g. peeking at an invalid address). We don't save things in that
	! case, since we would overwrite the save area, which has valid
	! kernel state in it.

	set	nofault, %g1
#ifdef KADB64 
	ldx	[%g1], %g2
#else
	ld	[%g1], %g2
#endif
	tst	%g2
	bz,a	5f
	nop

	set	kadblock, %g1
	clrb	[%g1]

	! Figure out what kind of stack we are on
	! We need to do this before we call any C routine
	
	and	%sp, 0x1, %g1
	brnz	%g1, 1f 
	nop
	set	on_v8_stk, %g2
	mov	1, %g1
	st	%g1, [%g2]		! remember it
	save	%sp, -SA(MINFRAME64), %sp
	sub	%sp, STACK_BIAS, %sp
	wrpr	%g0, PSTATE_AM+PSTATE_PEF+PSTATE_PRIV, %pstate

1:
	! Reinstall the PROM's callback mechanism.


	call	reload_prom_callback
	nop
	
	set	saved_tt, %g3
	ld	[%g3], %g3
	mov	%g3, %o0
	set	saved_pc, %g3		! X64X saved_pc, saved_npc
	ldx	[%g3], %g3		! are u_long, compiled as 64 bit
	mov	%g3, %o1
	set	saved_npc, %g3
	ldx	[%g3], %g3
	mov	%g3, %o2

	call	fault			! fault(tt, pc, npc)
	nop
	/* NOTREACHED */		! we longjmp() out of fault()
5:
	! mark which cpu we came in on - needed if we switch cpus.

	CPU_INDEX(%g1, %g2)
	set	kadb_cpu, %g2
	st	%g1, [%g2]

	! need to switch to the PROM's %tba before we call any C code.

	set	regsave, %g2
	rdpr	%tba, %g3
	stx	%g3, [%g2 + R_TBA]
	set	mon_tba, %g3
#ifdef KADB64
	ldx	[%g3], %g3
#else
	ld	[%g3], %g3
#endif
	wrpr	%g0, %g3, %tba

	! Save the whole cpu state (all regs, windows) to the
	! register save area. Various privregs were saved in
	! the defer word, so copy them from the saved locations.

	set	saved_pc, %g3			! u_long, now 64 bit
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_PC]
	set	saved_npc, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_NPC]		! u_long, now 64 bit
	set	saved_g1, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G1]
	set	saved_g2, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G2]
	set	saved_g3, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G3]
	set	saved_g4, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G4]
	set	saved_g5, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G5]
	set	saved_g6, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G6]
	set	saved_g7, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_G7]

	set	saved_tt, %g3
	ld	[%g3], %g3
	stx	%g3, [%g2 + R_TT]

	! save the trap type - used for switch_cpu
	set	kadb_tt, %g4
	st	%g3, [%g4]

	set	saved_pil, %g3
	ld	[%g3], %g3
	stx	%g3, [%g2 + R_PIL]
	rdpr	%cwp, %g3
	stx	%g3, [%g2 + R_CWP]
	rdpr	%otherwin, %g3
	stx	%g3, [%g2 + R_OTHERWIN]
	rdpr	%cansave, %g3
	stx	%g3, [%g2 + R_CANSAVE]
	rdpr	%canrestore, %g3
	stx	%g3, [%g2 + R_CANRESTORE]
	rdpr	%cleanwin, %g3
	stx	%g3, [%g2 + R_CLEANWIN]
	rdpr	%wstate, %g3
	stx	%g3, [%g2 + R_WSTATE]
	set	saved_tstate, %g3
	ldx	[%g3], %g3
	stx	%g3, [%g2 + R_TSTATE]

	set	MMU_PCONTEXT, %g4
	ldxa	[%g4]ASI_DMMU, %g3
	stx	%g3, [%g2 + R_MMU_PCONTEXT]

	stxa	%g0, [%g4]ASI_DMMU	! guarantee kernel address space
	membar	#Sync

	! Save the 64-bit state of the current window's out regs,
	! since v8+ programs can make use of them.

	!add	%g2, R_OUTS, %g7
	set	R_OUTS, %g7		! R_OUTS exceeds 13 bits
	add	%g2, %g7, %g7
	SAVE_OUTS(%g7)

	! Now save all the register windows.

	add	%g2, R_WINDOW, %g7

	! go to window 0 and blast everything to the save area.

	wrpr	%g0, 0, %cwp
	wrpr	%g0, 0, %otherwin
	set	nwin_minus_one, %g5
	ld	[%g5], %g5
	wrpr	%g5, %cleanwin
	sub	%g5, 1, %g5		! cansave+canrestore == nwindows-2
	wrpr	%g5, %cansave
	wrpr	%g0, %canrestore
2:
	SAVE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE64, %g7
	subcc	%g5, 1, %g5
	bnz	2b
	save

	! We have now exhausted all of cansave's windows and the next
	! save will cause a spill trap. Since there may not be a valid
	! %sp/%o6 in the next window, we reset the winregs by hand.

	SAVE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE64, %g7
	rdpr	%canrestore, %g4
	sub	%g4, 1, %g4
	wrpr	%g0, %g4, %canrestore	! %canrestore--
	wrpr	%g0, 1, %cansave	! %cansave++
	save
	SAVE_V9WINDOW(%g7)

	! Now flush the active (i.e. restoreable) window contents to memory.
	! Add %otherwin to %cansave, since those windows have no meaning in
	! this context, and %otherwin+%cansave+%canrestore MUST equal nwin-2.

	ldx	[%g2 + R_CWP], %g3	! back into saved cwp
	wrpr	%g0, %g3, %cwp
	ldx	[%g2 + R_OTHERWIN], %g3
	mov	%g3, %g4
	ldx	[%g2 + R_CANSAVE], %g3
	add	%g3, %g4, %g3
	wrpr	%g3, %cansave
	ldx	[%g2 + R_CANRESTORE], %g3
	wrpr	%g3, %canrestore
	and	%sp, 0x1, %g1
	brnz	%g1, 3f
	nop
	set	on_v8_stk, %g2
	mov	1, %g1
	st	%g1, [%g2]		! remember it
	save    %sp, -SA(MINFRAME64), %sp
	sub	%sp, STACK_BIAS, %sp
	wrpr	%g0, PSTATE_AM+PSTATE_PEF+PSTATE_PRIV, %pstate
	ba	2f
	nop
3:
	ldx	[%fp + V9BIAS64], %g0	! We get a Fast Data Access MMU
	ldx	[%sp + V9BIAS64], %g0	! miss if we don't do this
2:
	mov	%sp, %g4
	mov	%fp, %g5
	flushw				! flush all active windows to memory
	mov	%g4, %sp
	mov	%g5, %fp

#ifdef out
	and	%sp, 0x1, %g1
	brnz	%g1, 1f 
	nop
	set	on_v8_stk, %g2
	mov	1, %g1
	st	%g1, [%g2]		! remember it
	save    %sp, -SA(MINFRAME64), %sp
	sub	%sp, STACK_BIAS, %sp
	wrpr	%g0, PSTATE_AM+PSTATE_PEF+PSTATE_PRIV, %pstate
#endif

1:

	! Initialize window registers for the debugger.

	wrpr	%g0, 0, %otherwin
	set	nwin_minus_one, %g5
	ld	[%g5], %g5
	wrpr	%g5, %cleanwin
	sub	%g5, 1, %g5
	wrpr	%g5, %cansave
	wrpr	%g0, %canrestore

	! Check to see if we need to save the floating-point state. Note
	! that we need to enable FP regardless, since printf uses it.

	set	fpuregs, %g7
	rd	%fprs, %g4
	st	%g4, [%g7 + FPU_FPRS]	! save original
	andcc	%g4, FPRS_FEF, %g4	! fp enabled?
	bz,pt	%icc, 1f		! nope, drive on...
	wr	%g0, FPRS_FEF, %fprs	! enable it anyway

	! Save the FP registers and status. On Fusion, FP traps are
	! precise, so no need to worry about outstanding FP operations.

	STORE_FPREGS(%g7)
	stx	%fsr, [%g7 + FPU_FSR]
1:
	!rdpr	%pstate, %g1
	!wrpr	%g1, PSTATE_AM, %pstate
	call	idle_other_cpus
	nop

	! Clear the lock, now that we know nobody can take it from us.
	! We need to do this before doing anything that could take a
	! trap while we're still holding the lock.

	sethi	%hi(kadblock), %g1
	clrb	[%g1 + %lo(kadblock)]

	call	reload_prom_callback	! reload callback mechanism
	nop

restart_cmd_loop:
	call	cmd			! enter main cmd loop
	nop

	call	resume_other_cpus	! restart everybody
	nop

	! Restore the register state

	!rdpr	%pstate, %g1
	!wrpr	%g1, PSTATE_AM, %pstate


	set	fpuregs, %g7
	ld	[%g7 + FPU_FPRS], %g4
	wr	%g4, %fprs
	andcc	%g4, FPRS_FEF, %g0	! was fp enabled?
	bz,pt	%icc, 1f		! nope, drive on...
	nop
	LOAD_FPREGS(%g7)
	ldx	[%g7 + FPU_FSR], %fsr
1:
	set	regsave, %g7
	add	%g7, R_WINDOW, %g7

	! Go to window 0 and restore everything from the save area.

	wrpr	%g0, 0, %cwp
	wrpr	%g0, 0, %otherwin
	set	nwin_minus_one, %g5
	ld	[%g5], %g5
	wrpr	%g5, %cleanwin
	sub	%g5, 1, %g5		! cansave+canrestore == nwindows-2
	wrpr	%g5, %cansave
	wrpr	%g0, 0, %canrestore

2:
	RESTORE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE64, %g7
	subcc	%g5, 1, %g5
	bnz	2b
	save

	! same trick as above
	RESTORE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE64, %g7
	rdpr	%canrestore, %g4
	sub	%g4, 1, %g4
	wrpr	%g0, %g4, %canrestore	! %canrestore--
	wrpr	%g0, 1, %cansave	! %cansave++
	save
	RESTORE_V9WINDOW(%g7)

	! Reset window registers to values on trap entry.

	set	regsave, %g2
	ldx	[%g2 + R_CWP], %g3
	wrpr	%g3, %cwp
	ldx	[%g2 + R_OTHERWIN], %g3
	wrpr	%g3, %otherwin
	ldx	[%g2 + R_CANSAVE], %g3
	wrpr	%g3, %cansave
	ldx	[%g2 + R_CANRESTORE], %g3
	wrpr	%g3, %canrestore
	ldx	[%g2 + R_CLEANWIN], %g3
	wrpr	%g3, %cleanwin
	ldx	[%g2 + R_WSTATE], %g3
	wrpr	%g3, %wstate

	! Restore the 64-bit state of the current window's out regs,
	! since v8+ programs can make use of them.

	!add	%g2, R_OUTS, %g7
	set	R_OUTS, %g7		! R_OUTS exceeds 13 bits
	add	%g2, %g7, %g7

	RESTORE_OUTS(%g7)

	set	on_v8_stk, %g7
	ld	[%g7], %g4	
	brz	%g4, 1f 	
	nop
	! Since the sp was saved before we muck around
	! with it, we have already restored it. 
	!add	%sp, STACK_BIAS, %sp
	st	%g0, [%g7] 
	wrpr    %g0, PSTATE_AM+PSTATE_PEF+PSTATE_PRIV, %pstate
	
1:

	! Figure out which type of trap we're returning from (i.e. either
	! some type of breakpoint or an L1-A/break from the user) and fake
	! a return from TL=1.

	wrpr	%g0, 1, %tl		! TL = 1

	ldx	[%g2 + R_MMU_PCONTEXT], %g3
	set	MMU_PCONTEXT, %g4
	stxa	%g3, [%g4]ASI_DMMU	! Can't do this until TL>0
	membar	#Sync

	ldx	[%g2 + R_TSTATE], %g3
	wrpr	%g0, %g3, %tstate
	ldx	[%g2 + R_PC], %g3	! setup return pc, npc
	wrpr	%g0, %g3, %tpc
	ldx	[%g2 + R_NPC], %g3
	wrpr	%g0, %g3, %tnpc
	ldx	[%g2 + R_TBA], %g3	! restore kernel's trap table
	wrpr	%g0, %g3, %tba
	ldx	[%g2 + R_PIL], %g3
	wrpr	%g0, %g3, %pil
	ldx	[%g2 + R_TT], %g3
	wrpr	%g0, %g3, %tt

	set	cur_cpuid, %g1
	ld	[%g1], %g1
	set	kadb_cpu, %g3
	ld	[%g3], %g3
	cmp	%g1, %g3		! did we switch cpus?
	bne	2f			! yes, need to redo trap inst.
	nop

	set	kadb_tt, %g3		! saved trap type
	ld	[%g3], %g3
1:
	cmp	%g3, ST_KADB_TRAP|T_SOFTWARE_TRAP
	beq	debugtrap
	nop
2:
	set	tmpreg, %g4
	stx	%l3, [%g4]		! use %l3 to restore globals
	set	regsave, %l3
	RESTORE_GLOBALS(%l3)
	set	tmpreg, %l3
	ldx	[%l3], %l3
	retry				! redo instruction
	/* NOTREACHED */
debugtrap:
	set	tmpreg, %g4
	stx	%l3, [%g4]
	set	regsave, %l3
	RESTORE_GLOBALS(%l3)
	set	tmpreg, %l3
	ldx	[%l3], %l3
	done				! skip over trap
	/* NOTREACHED */

	SET_SIZE(trap)

#endif	/* lint */

/*
 * Perform a trap on the behalf of our (C) caller.
 */
#ifdef	lint
/*ARGSUSED*/
void
asm_trap(int x) {}
#else

	ENTRY(asm_trap)
	retl
	t	%o0
	SET_SIZE(asm_trap)

#endif	/* lint */

/*
 * Flush all active (i.e. restoreable) windows to memory.
 */
#if defined(lint)
void
flush_windows(void) {}
#else	/* lint */

	ENTRY_NP(flush_windows)
	retl
	flushw
	SET_SIZE(flush_windows)

#endif	/* lint */

/*
 * The interface for a 32-bit client program calling the 64-bit 1275 OBP.
 */
#if defined(lint)
/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array) {return (0);}
#else	/* lint */

	ENTRY(client_handler)
	save	%sp, -SA64(MINFRAME64), %sp	! 32 bit frame, 64 bit sized
#ifdef __sparcv9
	mov	%i1, %o0
#else
	srl	%i0, 0, %i0			! zero extend handler addr.
	srl	%i1, 0, %o0			! zero extend first argument.
	set	1f, %o1				! zero extend pc
	jmp	%o1
	srl	%sp, 0, %sp			! zero extend sp
#endif
1:
	rdpr	%pstate, %l4			! Get the present pstate value
	andn	%l4, PSTATE_AM, %l6
	wrpr	%l6, 0, %pstate			! Set PSTATE_AM = 0
	jmpl	%i0, %o7			! Call cif handler
#ifdef __sparcv9
	nop
#else
	sub	%sp, V9BIAS64, %sp		! delay; Now a 64 bit frame
	add	%sp, V9BIAS64, %sp		! back to a 32-bit frame
#endif
	wrpr	%l4, 0, %pstate			! Just restore 
	ret					! Return result ...
	restore	%o0, %g0, %o0			! delay; result in %o0
	SET_SIZE(client_handler)

#endif	/* lint */

/*
 * Misc subroutines.
 */

#ifdef	lint
char *
gettba() {return ((char *)0);}
#else

	ENTRY(gettba)
	rdpr	%tba, %o0
	srl	%o0, 14, %o0
	retl
	sll	%o0, 14, %o0
	SET_SIZE(gettba)

#endif	/* lint */

#ifdef	lint
/*ARGSUSED*/
void
settba(struct scb *t) {}
#else

	ENTRY(settba)
	srl	%o0, 14, %o0
	sll	%o0, 14, %o0
	retl
	wrpr	%o0, %tba
	SET_SIZE(settba)

#endif	/* lint */

#ifdef	lint
char *
getsp(void) {return ((char *)0);}
#else

	ENTRY(getsp)
	retl
	mov	%sp, %o0
	SET_SIZE(getsp)

#endif	/* lint */

/*
 * Flush page containing <addr> from the spitfire I-cache.
 */
#ifdef	lint
/*ARGSUSED*/
void
sf_iflush(int *addr) {}
#else

	ENTRY(sf_iflush)
	set	MMU_PAGESIZE, %o1
	srlx	%o0, MMU_PAGESHIFT, %o0		! Go to begining of page
	sllx	%o0, MMU_PAGESHIFT, %o0
	membar	#StoreStore			! make stores globally visible
1:
	flush	%o0
	sub	%o1, ICACHE_FLUSHSZ, %o1	! bytes = bytes-8 
	brnz,a,pt %o1, 1b
	add	%o0, ICACHE_FLUSHSZ, %o0	! vaddr = vaddr+8
	retl
	nop
	SET_SIZE(sf_iflush)

#endif

#if defined(lint)
/* ARGSUSED */
void
kadb_send_mondo(u_int cpuid, func_t func, u_int arg1, u_int arg2,
    u_int arg3, u_int arg4) {}
#else

/*
 * Send a mondo interrupt to the specified CPU. 
 *
 *	%o0: CPU number (UPA port id)
 *	%o1: PC of mondo handler
 *	%o2-%o5: arguments
 */

	ENTRY(kadb_send_mondo)

	! IDSR should not be busy at the moment

	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %g1
	btst	IDSR_BUSY, %g1
	bz,pt	%xcc, 1f
	mov	ASI_INTR_DISPATCH, %asi
	sethi	%hi(_send_mondo_busy), %o0
	call	prom_panic
	or	%o0, %lo(_send_mondo_busy), %o0

	! interrupt vector dispatch data reg 0
	! 	low  32bit: %o1
	! 	high 32bit: 0
1:
	mov	%o1, %g1
	mov	ASI_INTR_DISPATCH, %asi
	stxa	%g1, [IDDR_0]%asi	! pc of handler

	! interrupt vector dispatch data reg 1
	! 	low  32bit: %o2
	! 	high 32bit: %o4

	sllx	%o4, 32, %g1
	or	%o2, %g1, %g1
	stxa	%g1, [IDDR_1]%asi

	! interrupt vector dispatch data reg 2
	! 	low  32bit: %o3
	! 	high 32bit: %o5

	sllx	%o5, 32, %g1
	or	%o3, %g1, %g1
	stxa	%g1, [IDDR_2]%asi
	membar	#Sync

	! construct the interrupt dispatch command register in %g1

	sll	%o0, IDCR_PID_SHIFT, %g1	! IDCR<18:14> = upa port id
	or	%g1, IDCR_OFFSET, %g1		! IDCR<13:0> = 0x70
	set	XC_NACK_COUNT, %o1		! reset NACK debugging counter
1:
	set	XC_BUSY_COUNT, %o2		! reset BUSY debugging counter

	! dispatch the command

	stxa	%g0, [%g1]ASI_INTR_DISPATCH	! interrupt vector dispatch
	membar	#Sync
2:
	ldxa	[%g0]ASI_INTR_DISPATCH_STATUS, %g5
	tst	%g5
	bnz,pn	%xcc, 3f
	deccc	%o2
	retl
	nop

	! update BUSY debugging counter; quit if beyond the limit
3:
	bnz,a,pt %xcc, 4f
	btst	IDSR_BUSY, %g5
	sethi	%hi(_send_mondo_busy), %o0
	call	prom_panic
	or	%o0, %lo(_send_mondo_busy), %o0

	! wait if BUSY
4:
	bnz,pn	%xcc, 2b
	nop

	! update NACK debugging counter; quit if beyond the limit

	deccc	%o1
	bnz,a,pt %xcc, 5f
	btst	IDSR_NACK, %g5
	sethi	%hi(_send_mondo_nack), %o0
	call	prom_panic
	or	%o0, %lo(_send_mondo_nack), %o0

	! repeat the dispatch if NACK
5:
	bz,pn	%xcc, 7f
	nop

	! nacked so pause for 1 usec_delay and retry

	sethi	%hi(Cpudelay), %g2
	ld	[%g2 + %lo(Cpudelay)], %g2 ! microsecond countdown counter
	orcc	%g2, 0, %g2		! set cc bits to nz
6:
	bnz	6b			! microsecond countdown loop
	subcc	%g2, 1, %g2		! 2 instructions in loop
	ba,pt	%xcc, 1b
	nop
7:
	retl
	nop

	SET_SIZE(kadb_send_mondo)

_send_mondo_busy:
	.asciz	"send_mondo: BUSY"
	.align	4

_send_mondo_nack:
	.asciz	"send_mondo: NACK"
	.align	4
#endif	/* lint */


#ifdef lint
/*
 * We are entered when 'this' CPU receives a mondo vector telling it
 * to save its state. First, raise the PIL to prevent device interrupts
 * (just disabling ints is no good, since the first time we trap the PROM
 * will turn them back on when it finishes servicing the trap); next,
 * save all the register state; finally, manipulate %tpc and %tnpc to
 * have "done" go to a routine that spins in a loop waiting until we
 * receive another mondo telling us to restore the state and jump back
 * to where we were when first idled by the CPU that took the breakpoint.
 * This routine runs at TL = 0.
 */
/*ARGSUSED*/
void
save_cpu_state(caddr_t savep, caddr_t tmp) {}
#else	/* lint */

	ENTRY(save_cpu_state)

	! Register Usage:
	!	g1: save area ptr
	!	g2: fp save area ptr
	!	g3: arg3
	!	g4: arg4

	! Go to PIL 14. Note that we're using the vector (interrupt)
	! globals.

	rdpr	%pil, %g3
	cmp	%g3, 14
	bge	1f
	nop
	wrpr	%g0, 14, %pil
1:
	! %g2 == mpfpregs[this_cpuid]

	rd	%fprs, %g4
	st	%g4, [%g2 + FPU_FPRS]	! save original
	andcc	%g4, FPRS_FEF, %g4	! fp enabled?
	bz,pt	%icc, 2f		! nope, drive on...
	nop
	STORE_FPREGS(%g2)
	stx	%fsr, [%g2 + FPU_FSR]
2:
	mov	%g1, %g2	! save area ptr
	stx	%g3, [%g2 + R_PIL]
	rdpr	%tpc, %g3
	stx	%g3, [%g2 + R_PC]
	rdpr	%tnpc, %g3
	stx	%g3, [%g2 + R_NPC]
	rdpr	%tt, %g3
	stx	%g3, [%g2 + R_TT]
	rdpr	%tba, %g3
	stx	%g3, [%g2 + R_TBA]
	rdpr	%cwp, %g3
	stx	%g3, [%g2 + R_CWP]
	rdpr	%otherwin, %g3
	stx	%g3, [%g2 + R_OTHERWIN]
	rdpr	%cansave, %g3
	stx	%g3, [%g2 + R_CANSAVE]
	rdpr	%canrestore, %g3
	stx	%g3, [%g2 + R_CANRESTORE]
	rdpr	%cleanwin, %g3
	stx	%g3, [%g2 + R_CLEANWIN]
	rdpr	%wstate, %g3
	stx	%g3, [%g2 + R_WSTATE]
	rdpr	%tstate, %g3
	stx	%g3, [%g2 + R_TSTATE]

	set	MMU_PCONTEXT, %g4
	ldxa	[%g4]ASI_DMMU, %g3
	stx	%g3, [%g2 + R_MMU_PCONTEXT]

	! Save the 64-bit state of the current window's out regs,
	! since v8+ programs can make use of them.

	!add	%g2, R_OUTS, %g7
	set	R_OUTS, %g7		! R_OUTS exceeds 13 bits
	add	%g2, %g7, %g7

	SAVE_OUTS(%g7)

	! Now save all the register windows.

	add	%g2, R_WINDOW, %g7

	! go to window 0 and blast everything to the save area.

	wrpr	%g0, 0, %cwp
	wrpr	%g0, 0, %otherwin
	set	nwin_minus_one, %g5
	ld	[%g5], %g5
	wrpr	%g5, %cleanwin
	sub	%g5, 1, %g5		! cansave+canrestore == nwindows-2
	wrpr	%g5, %cansave
	wrpr	%g0, %canrestore
3:
	SAVE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE, %g7
	subcc	%g5, 1, %g5
	bnz	3b
	save

	! We have now exhausted all of cansave's windows and the next
	! save will cause a spill trap. Since there may not be a valid
	! %sp/%o6 in the next window, we reset the winregs by hand.

	SAVE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE, %g7
	rdpr	%canrestore, %g4
	sub	%g4, 1, %g4
	wrpr	%g0, %g4, %canrestore	! %canrestore--
	wrpr	%g0, 1, %cansave	! %cansave++
	save
	SAVE_V9WINDOW(%g7)

	! Now flush the active (i.e. restoreable) window contents to memory.
	! Add %otherwin to %cansave, since those windows have no meaning in
	! this context, and %otherwin+%cansave+%canrestore MUST equal nwin-2.

	ldx	[%g2 + R_CWP], %g3	! back into saved cwp
	wrpr	%g0, %g3, %cwp
	ldx	[%g2 + R_OTHERWIN], %g3
	mov	%g3, %g4
	ldx	[%g2 + R_CANSAVE], %g3
	add	%g3, %g4, %g3
	wrpr	%g3, %cansave
	ldx	[%g2 + R_CANRESTORE], %g3
	wrpr	%g3, %canrestore
	mov	%sp, %g4
	mov	%fp, %g5
	flushw				! flush all active windows to memory
	mov	%g4, %sp
	mov	%g5, %fp

	! We've been running on the vector globals to this point;
	! now we finally save the regular globals.

	mov	%g2, %l3
	rdpr	%pstate, %g5
	andn	%g5, PSTATE_IG, %g5	! switch to regular globals
	wrpr	%g5, %g0, %pstate
	SAVE_GLOBALS(%l3)
	rdpr	%pstate, %l3
	andn	%l3, PSTATE_IG, %l3	! switch back to vector globals
	wrpr	%l3, %g0, %pstate

	! Arrange for "retry" to go to kadb's idle routine, instead of
	! back to the kernel. We'll spin there until the CPU that owns
	! kadb sends us another mondo when it's time to go back to userland. 

	set	kadb_idle, %g3
	wrpr	%g0, %g3, %tpc
	add	%g3, 4, %g3
	wrpr	%g0, %g3, %tnpc

	! Since we were cross interrupted, we may have entered from
	! user space.  Force our state to be kernel state on return
	! from the trap, regardless of where we came from.

	rdpr	%tstate, %g3
	or	TSTATE_PRIV, %g3, %g3
	wrpr	%g0, %g3, %tstate
	set	MMU_PCONTEXT, %g3
	stxa	%g0, [%g3]ASI_DMMU

	retry

	SET_SIZE(save_cpu_state)

#endif	/* lint */

#ifdef lint
/*
 * Restore the register state and jump back to where we
 * were before being idled. Note that on entry we are using
 * the vector (interrupt) globals.
 */
/*ARGSUSED*/
void
restore_cpu_state(caddr_t regsp, caddr_t tmp) {}
#else	/* lint */

	ENTRY(restore_cpu_state)

	! Register Usage:
	!	g1: save area ptr
	!	g2: fp save area ptr
	!	g3: arg3
	!	g4: arg4

	! Restore the register state

	ldx	[%g2 + FPU_FPRS], %g4
	wr	%g4, %fprs
	andcc	%g4, FPRS_FEF, %g0	! was fp enabled?
	bz,pt	%icc, 1f		! nope, drive on...
	nop
	LOAD_FPREGS(%g2)
	ldx	[%g2 + FPU_FSR], %fsr
1:
	mov	%g1, %g2
	add	%g2, R_WINDOW, %g7

	! Go to window 0 and restore everything from the save area.

	wrpr	%g0, 0, %cwp
	wrpr	%g0, 0, %otherwin
	set	nwin_minus_one, %g5
	ld	[%g5], %g5
	wrpr	%g5, %cleanwin
	sub	%g5, 1, %g5		! cansave+canrestore == nwindows-2
	wrpr	%g5, %cansave
	wrpr	%g0, 0, %canrestore

2:
	RESTORE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE, %g7
	subcc	%g5, 1, %g5
	bnz	2b
	save

	! same trick as above

	RESTORE_V9WINDOW(%g7)
	add	%g7, WINDOWSIZE, %g7
	rdpr	%canrestore, %g4
	sub	%g4, 1, %g4
	wrpr	%g0, %g4, %canrestore	! %canrestore--
	wrpr	%g0, 1, %cansave	! %cansave++
	save
	RESTORE_V9WINDOW(%g7)

	! Reset window registers to values on trap entry.

	ldx	[%g2 + R_CWP], %g3
	wrpr	%g3, %cwp
	ldx	[%g2 + R_OTHERWIN], %g3
	wrpr	%g3, %otherwin
	ldx	[%g2 + R_CANSAVE], %g3
	wrpr	%g3, %cansave
	ldx	[%g2 + R_CANRESTORE], %g3
	wrpr	%g3, %canrestore
	ldx	[%g2 + R_CLEANWIN], %g3
	wrpr	%g3, %cleanwin
	ldx	[%g2 + R_WSTATE], %g3
	wrpr	%g3, %wstate

	! Restore the 64-bit state of the current window's out regs,
	! since v8+ programs can make use of them.

	!add	%g2, R_OUTS, %g7
	set	R_OUTS, %g7		! R_OUTS exceeds 13 bits
	add	%g2, %g7, %g7
	RESTORE_OUTS(%g7)

	ldx	[%g2 + R_TSTATE], %g3
	wrpr	%g0, %g3, %tstate
	ldx	[%g2 + R_PC], %g3	! setup return pc, npc
	wrpr	%g0, %g3, %tpc
	ldx	[%g2 + R_NPC], %g3
	wrpr	%g0, %g3, %tnpc
	ldx	[%g2 + R_PIL], %g3
	wrpr	%g0, %g3, %pil
	ldx	[%g2 + R_TBA], %g3
	wrpr	%g0, %g3, %tba
	ldx	[%g2 + R_TT], %g3
	wrpr	%g0, %g3, %tt

	! We need to check the trap type for the switch-cpu case:
	! this CPU may have entered kadb via an L1-A trap, which
	! requires a 'done', not a 'retry'.

	cmp	%g3, ST_KADB_TRAP|T_SOFTWARE_TRAP
		! set %xcc now, don't use until later
	stx	%l3, [%g2 + R_TT]	! save %l3 for later
	mov	%g2, %l3		! save-area ptr

	wrpr	%g0, 1, %tl		! go to TL=1 to simulate trap ret

	ldx	[%g2 + R_MMU_PCONTEXT], %g3
	set	MMU_PCONTEXT, %g4	! Restore MMU Primary Context
	stxa	%g3, [%g4]ASI_DMMU	! Can't do this until TL>0

	rdpr	%pstate, %g3
	andn	%g3, PSTATE_IG, %g3	! set for regular globals

	be,pn	%xcc, 1f		! Test for L1-A trap
	wrpr	%g3, %g0, %pstate	! switch globals

	RESTORE_GLOBALS(%l3)
	ldx	[%l3 + R_TT], %l3
	retry
	/* NOTREACHED */
1:
	RESTORE_GLOBALS(%l3)
	ldx	[%l3 + R_TT], %l3
	done
	/* NOTREACHED */

	SET_SIZE(restore_cpu_state)

#endif	/* lint */

#ifdef lint
void
start_cmd_loop(void) {}
#else	/* lint */

	ENTRY(start_cmd_loop)

	wrpr	%g0, 1, %tl		! return from TL=1
	set	1f, %g3
	wrpr	%g0, %g3, %tpc
	add	%g3, 4, %g3
	wrpr	%g0, %g3, %tnpc
	retry
1:
	! now running on normal globals, but we need to restore the
	! original windows, etc.

	set	mon_tba, %g3		! get onto PROM's trap table
#ifdef KADB64 
	ldx	[%g3], %g3
#else
	ld	[%g3], %g3
#endif
	wrpr	%g0, %g3, %tba
	wr	%g0, FPRS_FEF, %fprs	! enable floating point
	wrpr	%g0, PSTATE_PEF+PSTATE_PRIV, %pstate
	!wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV, %pstate
	ba	restart_cmd_loop
	nop
	/*NOTREACHED*/

	SET_SIZE(start_cmd_loop)

#endif	/* lint */

#ifdef lint
/*ARGSUSED*/
void
kadb_idle_self(int tba) {}
#else	/* lint */

	ENTRY(kadb_idle_self)

	! Being the active CPU, we currently have interrupts disabled;
	! reenable them so we can receive the mondo when it's time to
	! wake up, but go to PIL 14, so we don't get interrupted.

	wrpr	%g0, %o0, %tba
	wrpr	%g0, 14, %pil
	wrpr	%g0, PSTATE_PEF+PSTATE_PRIV+PSTATE_IE, %pstate
	!wrpr	%g0, PSTATE_PEF+PSTATE_AM+PSTATE_PRIV+PSTATE_IE, %pstate
	b,a	kadb_idle
	nop

	SET_SIZE(kadb_idle_self)
#endif	/* lint */

#ifdef lint
void
kadb_idle(void) {}
#else	/* lint */

	ENTRY(kadb_idle)
1:
	tst	%g0
	beq	1b
	nop

	SET_SIZE(kadb_idle)

_kadb_idle_str:
	.asciz	"kadb: idled..."
	.align	4
_kadb_mondo_str:
	.asciz	"kadb: mondo..."
	.align	4

#endif	/* lint */


/*
 * _setjmp(buf_ptr)
 * buf_ptr points to a five word array (jmp_buf). In the first is our
 * return address, the second, is the callers SP.
 * The rest is cleared by _setjmp
 *
 *		+----------------+
 *   %i0->	|      pc        |
 *		+----------------+
 *		|      sp        |
 *		+----------------+
 *		|    sigmask     |
 *		+----------------+
 *		|   stagstack    |
 *		|   structure    |
 *		+----------------+
 */

#if defined(lint)

/* ARGSUSED */
int
_setjmp(jmp_buf_ptr buf_ptr)
{ return (0); }

#else	/* lint */


#ifdef KADB64 
	PCVAL	=	0	! offsets in buf structure
	SPVAL	=	8
	SIGMASK	=	16
	SIGSTACK =	24	

	SS_SP	   =	0	! offset in sigstack structure
	SS_ONSTACK =	8
#else

	PCVAL	=	0	! offsets in buf structure
	SPVAL	=	4
	SIGMASK	=	8
	SIGSTACK =	12

	SS_SP	   =	0	! offset in sigstack structure
	SS_ONSTACK =	4
#endif

	ENTRY(_setjmp)
#ifdef KADB64
	stx	%o7, [%o0 + PCVAL] 	! return pc
	stx	%sp, [%o0 + SPVAL] 	! save caller's sp
#else
	st	%o7, [%o0 + PCVAL] 	! return pc
	st	%sp, [%o0 + SPVAL] 	! save caller's sp
#endif
	clr	[%o0 + SIGMASK]		! clear the remainder of the jmp_buf
	clr	[%o0 + SIGSTACK + SS_SP]
	clr	[%o0 + SIGSTACK + SS_ONSTACK]
	retl
	clr	%o0
	SET_SIZE(_setjmp)

#endif	/* lint */

/*
 * _longjmp(buf_ptr, val)
 * buf_ptr points to an array which has been initialized by _setjmp.
 * val is the value we wish to return to _setjmp's caller
 *
 * We will flush our registers by doing (nwindows-1) save instructions.
 * This could be better done as a kernel call. This is necessary to
 * ensure that the registers we want to pick up are stored in the stack.
 * Then, we set fp from the saved fp and make ourselves a stack frame.
 */

#if defined(lint)

/* ARGSUSED */
void
_longjmp(jmp_buf_ptr buf_ptr, int val)
{
	return;
}

#else	/* lint */

	ENTRY(_longjmp)
#ifdef KADB64
	save	%sp, -WINDOWSIZE64, %sp
#else
	save	%sp, -WINDOWSIZE64, %sp
#endif
	!
	! flush all register windows to the stack.
	! 
	set	nwindows, %g7
	ld	[%g7], %g7
	sub	%g7, 2, %g6
1:
!ldx	[%sp + V9BIAS64], %g0
	deccc	%g6			! all windows done?
	bnz	1b
#ifdef KADB64
	save	%sp, -WINDOWSIZE64, %sp
#else
	save	%sp, -WINDOWSIZE, %sp
#endif
	ldx	[%sp + V9BIAS64], %g0
	sub	%g7, 2, %g6
2:
	deccc	%g6			! all windows done?
	bnz	2b
	restore				! delay slot, increment CWP

#ifdef KADB64
	ldx	[%i0 + SPVAL], %fp	! build new stack frame
	sub	%fp, -SA64(MINFRAME64), %sp	 !establish new save area
	ldx	[%i0 + PCVAL], %i7	! get new return pc
	ldx	[%fp + V9BIAS64], %g0	! We get a Fast Data Access MMU
	ldx	[%sp + V9BIAS64], %g0	! miss if we don't do this
					! when we do the restore
#else
	ld	[%i0 + SPVAL], %fp	! build new stack frame
	sub	%fp, -SA64(MINFRAME), %sp	! establish new save area
	ld	[%i0 + PCVAL], %i7	! get new return pc
#endif
	ret
	restore	%i1, 0, %o0		! return (val)
	SET_SIZE(_longjmp)

#endif	/* lint */

