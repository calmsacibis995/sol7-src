/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_stack_iter.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stack.h>
#include "libproc.h"
#include "Pcontrol.h"

/*
 * Iterate over stack frames, call client iterator function for each frame.
 */

/*
 * Utility function to prevent stack loops from running on forever by
 * detecting when there is a stack loop, thst is, when the %fp has been
 * previously encountered.
 */
static int
stack_loop(prgreg_t fp, prgreg_t **prevfpp, int *nfpp, u_int *pfpsizep)
{
	prgreg_t *prevfp = *prevfpp;
	u_int pfpsize = *pfpsizep;
	int nfp = *nfpp;
	int i;

	for (i = 0; i < nfp; i++) {
		if (fp == prevfp[i])
			break;
	}
	if (i < nfp)	/* stack loop detected */
		return (1);
	if (nfp == pfpsize) {
		if (pfpsize == 0)
			pfpsize = 16;
		else
			pfpsize *= 2;
		prevfp = realloc(prevfp, pfpsize * sizeof (prgreg_t));
	}
	prevfp[nfp++] = fp;

	*prevfpp = prevfp;
	*pfpsizep = pfpsize;
	*nfpp = nfp;
	return (0);
}

#if defined(sparc) || defined(__sparc)
int
proc_stack_iter(struct ps_prochandle *P, const prgregset_t regs,
	proc_stack_f *func, void *arg)
{
	prgreg_t *prevfp = NULL;
	u_int pfpsize = 0;
	int nfp = 0;
	prgregset_t gregs;
	long args[6];
	prgreg_t fp;
	int i;
	int rv;
	off_t sp;

	(void) memcpy(gregs, regs, sizeof (gregs));
	for (;;) {
		fp = gregs[R_FP];
		if (stack_loop(fp, &prevfp, &nfp, &pfpsize))
			break;

		for (i = 0; i < 6; i++)
			args[i] = gregs[R_I0 + i];
		if ((rv = func(arg, gregs[R_PC], 6, args)) != 0)
			break;

		gregs[R_PC] = gregs[R_I7];
		gregs[R_nPC] = gregs[R_PC] + 4;
		(void) memcpy(&gregs[R_O0], &gregs[R_I0], 8*sizeof (prgreg_t));
		if ((sp = gregs[R_FP]) == 0)
			break;

#ifdef _LP64
		if (P->status.pr_dmodel == PR_MODEL_LP64) {
			sp += STACK_BIAS;
			if (pread(P->asfd, &gregs[R_L0], 16 * sizeof (long), sp)
			    != 16 * sizeof (long))
				break;
		} else {
			uint32_t rwin[16];

			if (pread(P->asfd, &rwin[0], 16 * sizeof (uint32_t), sp)
			    != 16 * sizeof (uint32_t))
				break;

			for (i = 0; i < 16; i++)
				gregs[R_L0 + i] = rwin[i];
		}
#else	/* _LP64 */
		if (P->status.pr_dmodel == PR_MODEL_LP64 ||
		    pread(P->asfd, &gregs[R_L0], 16 * sizeof (long), sp)
		    != 16 * sizeof (long))
			break;
#endif	/* _LP64 */
	}

	if (prevfp)
		free(prevfp);
	return (rv);
}
#endif	/* sparc */

#if defined(i386) || defined(__i386)

/*
 * Given the return PC, return the number of arguments.
 * (A bit of disassembly of the instruction is required here.)
 */
static u_long
argcount(struct ps_prochandle *P, long pc, ssize_t sz)
{
	u_char instr[6];
	u_long count;

	/*
	 * Read the instruction at the return location.
	 */
	if (Pread(P, instr, sizeof (instr), pc) != sizeof (instr) ||
	    instr[1] != 0xc4)
		return (0);

	switch (instr[0]) {
	case 0x81:	/* count is a longword */
		count = instr[2]+(instr[3]<<8)+(instr[4]<<16)+(instr[5]<<24);
		break;
	case 0x83:	/* count is a byte */
		count = instr[2];
		break;
	default:
		count = 0;
		break;
	}

	if (count > sz)
		count = sz;
	return (count / sizeof (long));
}

int
proc_stack_iter(struct ps_prochandle *P, const prgregset_t regs,
	proc_stack_f *func, void *arg)
{
	prgreg_t *prevfp = NULL;
	u_int pfpsize = 0;
	int nfp = 0;
	struct {
		long	fp;
		long	pc;
		long	args[32];
	} frame;
	u_int argc;
	ssize_t sz;
	prgreg_t fp;
	prgreg_t pc;
	int rv;

	fp = regs[R_FP];
	pc = regs[R_PC];
	while (fp != 0 || pc != 0) {
		if (stack_loop(fp, &prevfp, &nfp, &pfpsize))
			break;

		if (fp != 0 &&
		    (sz = Pread(P, &frame, sizeof (frame), (uintptr_t)fp)
		    >= (ssize_t)(2* sizeof (long)))) {
			sz -= 2* sizeof (long);
			argc = argcount(P, (long)frame.pc, sz);
		} else {
			(void) memset(&frame, 0, sizeof (frame));
			argc = 0;
		}

		if ((rv = func(arg, pc, argc, frame.args)) != 0)
			break;

		fp = frame.fp;
		pc = frame.pc;
	}

	if (prevfp)
		free(prevfp);
	return (rv);
}
#endif	/* i386 */
