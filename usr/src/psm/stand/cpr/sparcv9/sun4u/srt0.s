/*
 * Copyright (c) 1986 - 1996, Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)srt0.s	1.12	98/01/07 SMI"

/*
 * srt0.s - cprbooter startup code
 */

#include <sys/asm_linkage.h>
#include <sys/debug/debug.h>
#include <sys/cpu.h>
#include <sys/privregs.h>
#include <sys/stack.h>


#if defined(lint)

char _end[1];

/*ARGSUSED*/
void
_start(void *a, void *b, void *c, void *cif_handler)
{}

#else

	.seg	".data"
_local_p1275cif:
	.xword	0

#define	STACK_SIZE	0x14000
	.align	16
	.skip	STACK_SIZE
.ebootstack:			! top of boot stack

/*
 * The following variables are more or less machine-independent
 * (or are set outside of fiximp).
 */

	.seg	".text"
	.global	prom_exit_to_mon
	.type	prom_exit_to_mon, #function


!
! Careful: don't touch %o4 until the save, since it contains the
! address of the IEEE 1275 SPARC v9 CIF handler.
!
! We cannot write to any symbols until we are relocated.
! Note that with the advent of 5.x boot, we no longer have to
! relocate ourselves, but this code is kept around cuz we *know*
! someone would scream if we did the obvious.
!

	ENTRY(_start)
	setn	.ebootstack - STACK_BIAS, %g1, %o5
	save	%o5, -SA(MINFRAME), %sp

	!
	! zero the bss
	!
	setn	edata, %g1, %o0
	setn	end, %g1, %i2
	call	cpr_bzero
	sub	%i2, %o0, %o1			! end - edata = size of bss

	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, PSTATE_PEF|PSTATE_PRIV|PSTATE_IE, %pstate

	setn	_local_p1275cif, %g1, %o1
	stx	%i4, [%o1]
	call	main			! main(prom-cookie)
	mov	%i4, %o0		! SPARCV9/CIF

	call	prom_exit_to_mon	! can't happen .. :-)
	nop
	SET_SIZE(_start)

#endif	/* lint */

/*
 *	exitto is called from main() and it jumps directly to the
 *	just-loaded standalone.  There is NO RETURN from exitto().
 */

#if defined(lint)

/* ARGSUSED */
void
exitto(func_t entrypoint, char *loadbase)
{}

#else	/* lint */

	ENTRY(exitto)
	save	%sp, -SA(MINFRAME), %sp

	setn	_local_p1275cif, %g1, %o0
	setn	cpr_statefile, %g1, %o1	  ! pass state file name to cprboot
	setn	cpr_filesystem, %g1, %o2  ! pass file system path to cprboot
	mov	%i1, %o3		  ! pass loadbase to cprboot
	jmpl	%i0, %o7		  ! call thru register to the standalone
	ldx	[%o0], %o0		  ! pass the 1275 CIF handler to callee
	unimp	0
	/* there is no return from here */
	SET_SIZE(exitto)

#endif	/* lint */

/*
 * The interface for a 64-bit client program
 * calling the 64-bit romvec OBP.
 */

#if defined(lint)
#include <sys/promif.h>

/* ARGSUSED */
int
client_handler(void *cif_handler, void *arg_array)
{ return (0); }

#else	/* lint */

	ENTRY(client_handler)
	mov	%o7, %g1
	mov	%o0, %g5
	mov	%o1, %o0
	jmp	%g5
	mov	%g1, %o7
	SET_SIZE(client_handler)

#endif	/* lint */
