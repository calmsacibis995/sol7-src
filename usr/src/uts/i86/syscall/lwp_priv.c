/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	Copyright (c) 1994 Sun Microsystems, Inc. */
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident "@(#)lwp_private.c 1.5	97/01/16 SMI" /* from SVr4.0 1.83 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/signal.h>
#include <sys/stack.h>
#include <sys/reg.h>
#include <sys/frame.h>
#include <sys/proc.h>
#include <sys/asm_linkage.h>
#include <sys/errno.h>
#include <sys/archsystm.h>
#include <sys/debug.h>

/*
 * Get the LWP's "thread specific data".
 */
long
lwp_getprivate()
{
	/* XXX - needs work.  additional LDT support needed. */
	return (0l);
}

/*
 * Setup the LWP's "thread specific data".
 */
/* ARGSUSED */
int
lwp_setprivate(void *bp)
{
	/* XXX - needs work.  additional LDT support needed. */
	return (0);
}
