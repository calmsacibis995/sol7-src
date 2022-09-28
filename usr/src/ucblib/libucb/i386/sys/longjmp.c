/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF		*/
/*	UNIX System Laboratories, Inc.				*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * NOTE!
 * This code was copied from usr/src/lib/libc/i386/gen/siglongjmp.c
 */

#ident	"@(#)longjmp.c	1.2	96/12/19 SMI"

#include <ucontext.h>
#include <setjmp.h>

void
longjmp(env, val)
jmp_buf env;
int val;
{
	register ucontext_t *ucp = (ucontext_t *)env;
	if (val)
		ucp->uc_mcontext.gregs[ EAX ] = val;
	setcontext(ucp);
}
