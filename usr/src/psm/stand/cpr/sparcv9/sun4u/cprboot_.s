/*
 * Copyright (c) 1986-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprboot_srt0.s	1.19	98/01/13 SMI"

/*
 * cprboot_srt0.s - standalone startup code
 */

#include <sys/asm_linkage.h>
#include <sys/cpu.h>
#include <sys/privregs.h>
#include <sys/stack.h>
#include <sys/spitasi.h>
#include <sys/machparam.h>
#include <sys/pte.h>
#include <sys/mmu.h>
#include <sys/cpr_impl.h>

#if defined(lint)

char _etext[1];
char _end[1];

extern int main(void *, char *, char *, char *);

/*ARGSUSED*/
void
_start(void *cif_handler, char *pathname, char *bpath, char *base)
{}

#else
	.seg	".text"
	.align	8
	.global	end
	.global edata
	.global	main

/*
 *
 */
	.seg	".data"
_local_p1275cif:
	.xword	0

#define	STACK_SIZE	0x14000
	.align	16
	.skip	STACK_SIZE
.estack:			! top of boot stack

/*
 * The following variables are more or less machine-independent
 * (or are set outside of fiximp).
 */

	.seg	".text"
	.global	prom_exit_to_mon
	.type	prom_exit_to_mon, #function


!
! Careful, do not loose value of the SPARC v9 P1275 CIF handler in %o4
!

	ENTRY(_start)
	setn	.estack - STACK_BIAS, %g1, %o5
	save	%o5, -SA(MINFRAME), %sp

	/*
	 * Set the psr into a known state:
	 * Set supervisor mode, interrupt level >= 13, traps enabled
	 */
	wrpr	%g0, PSTATE_PEF|PSTATE_PRIV|PSTATE_IE, %pstate

	setn	_local_p1275cif, %g1, %o1
	stx	%i4, [%o1]

	mov	%i3, %o3		! loadbase
	mov	%i2, %o2		! bpath
	mov	%i1, %o1		! pathname
	call	main			! main(prom-cookie)
	mov	%i0, %o0		! SPARCV9/CIF

	call	prom_exit_to_mon	! can't happen .. :-)
	nop
	SET_SIZE(_start)

#endif	/* lint */


#if defined(lint)

/*
 * registers setup for i_cpr_resume_setup()
 *
 * args from cprboot main():
 * 	%o0	prom cookie
 *	%o1	struct sun4u_machdep *mdp
 *
 * args setup here:
 *      %i1     start of cprboot text pages
 *      %i2     end   of cprboot text pages
 *      %i3     start of cprboot data pages
 *      %i4     end   of cprboot data pages
 *
 * Any change to this register assignment requires
 * changes to uts/common/cpr/cpr_resume_setup.s
 */

/* ARGSUSED */
void
exit_to_kernel(void *cookie, csu_md_t *mdp)
{}

#else	/* lint */

	.seg	".data"

	ENTRY(exit_to_kernel)
#if 1
	rdpr	%ver, %g1
	and	%g1, VER_MAXWIN, %g1
	wrpr	%g0, %g1, %cleanwin
	wrpr	%g0, %g0, %canrestore
	wrpr	%g0, %g0, %otherwin
	dec	%g1
	wrpr	%g0, %g1, %cansave
#endif

	!
	! setup %i registers
	!
	setn	starttext, %g2, %g1
	ldn	[%g1], %i1
	setn	endtext, %g2, %g1
	ldn	[%g1], %i2
	setn	startdata, %g2, %g1
	ldn	[%g1], %i3
	setn	enddata, %g2, %g1
	ldn	[%g1], %i4

	!
	! setup temporary stack and adjust
	! by the saved kernel stack bias
	!
	setn	newstack, %g2, %g1		! g1 = &newstack
	ldn	[%g1], %l2			! l2 =  newstack
	sub	%l2, SA(MINFRAME), %l2
	ld	[%o1 + CPR_MD_KSB], %l4		! mdp->ksb
	sub	%l2, %l4, %sp

	!
	! set pstate and wstate from saved values
	!
	lduh	[%o1 + CPR_MD_KPSTATE], %l4	! l4 = mdp->kpstate
	wrpr	%g0, %l4, %pstate
	lduh	[%o1 + CPR_MD_KWSTATE], %l4	! l4 = mdp->kwstate
	wrpr	%g0, %l4, %wstate

	!
	! jump to kernel with %o0 and %o1 unchanged
	!
	ldx	[%o1 + CPR_MD_FUNC], %l3	! l3 = mdp->func
	jmpl	%l3, %g0
	nop

	/*  there is no return from here */
	unimp	0
	SET_SIZE(exit_to_kernel)

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

#if defined(lint)
/* ARGSUSED */
void
cpr_dtlb_wr_entry(int index, caddr_t tag, int ctx, tte_t *tte)
{}

/* ARGSUSED */
void
cpr_itlb_wr_entry(int index, caddr_t tag, int ctx, tte_t *tte)
{}

#else	/* lint */

	ENTRY(cpr_dtlb_wr_entry)
	srl	%o1, MMU_PAGESHIFT, %o1
	sll	%o1, MMU_PAGESHIFT, %o1
	sllx    %o0, 3, %o0
	or	%o1, %o2, %o1
	ldx	[%o3], %o3
	set	MMU_TAG_ACCESS, %o5
	stxa	%o1, [%o5]ASI_DMMU
	stxa	%o3, [%o0]ASI_DTLB_ACCESS
	membar	#Sync
	retl
	nop
	SET_SIZE(cpr_dtlb_wr_entry)

	ENTRY(cpr_itlb_wr_entry)
	srl	%o1, MMU_PAGESHIFT, %o1
	sll	%o1, MMU_PAGESHIFT, %o1
	sllx	%o0, 3, %o0
	or	%o1, %o2, %o1
	ldx	[%o3], %o3
	set	MMU_TAG_ACCESS, %o5
	stxa    %o1, [%o5]ASI_IMMU
	stxa	%o3, [%o0]ASI_ITLB_ACCESS
	retl
	nop
	SET_SIZE(cpr_itlb_wr_entry)

#endif	/* lint */
