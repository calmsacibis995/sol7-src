/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1993, 1995, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strlog.c	1.31	98/01/26 SMI"	/* SVr4.0 1.14	*/

/*
 * Streams log interface routine.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/log.h>
#include <sys/inline.h>
#include <sys/strlog.h>
#include <sys/systm.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/file.h>
#include <sys/ddi.h>
#include <sys/varargs.h>
#include <sys/syslog.h>
#include <sys/cmn_err.h>
#include <sys/msgbuf.h>
#include <sys/stat.h>
#include <sys/strsubr.h>
#include <sys/thread.h>
#include <sys/mutex.h>

#include <sys/conf.h>
#include <sys/sunddi.h>

static void parse_args(mblk_t *, char *, va_list);

/*
 * This lock and queue pointer form the private conduit for strlog messages
 * into the log driver.  Since the log driver is loadable they are defined
 * in space.c
 */
extern kmutex_t	logdq_lock;
extern queue_t	*log_d_q;

/*
 * Kernel logger interface function.  Attempts to construct a log
 * message, put it on the dedicated queue which distributes it to the
 * appropriate logger stream.
 * Delivery will not be done if message blocks cannot be allocated or if the
 * logger is not registered (exception is console logger).
 *
 * Returns 0 if a message is not successfully submitted. Returns 1 otherwise.
 *
 * Remember, strlog cannot be called from this driver itself. It is
 * assumed that the code calling it is outside of the perimeter of this
 * driver.
 */
int
strlog(short mid, short sid, char level, u_short flags, char *fmt, ...)
{
	va_list argp;
	struct log_ctl *lcp;
	mblk_t *dbp, *cbp;
	int rval = 0;

	/*
	 * return if dedicated log Q has not yet been initialized
	 */
	if (log_d_q == NULL)
		return (0);

	ASSERT(flags & (SL_ERROR|SL_TRACE|SL_CONSOLE));

	if (!(flags & (SL_ERROR|SL_TRACE|SL_CONSOLE))) {
		return (0);
	}

	/*
	 * allocate message blocks for log text, log header, and
	 * proto control field.
	 */
	if (!(dbp = allocb(LOGMSGSZ, BPRI_HI)))
		return (0);
	if (!(cbp = allocb(sizeof (struct log_ctl), BPRI_HI))) {
		freeb(dbp);
		return (0);
	}

	/*
	 * expand the string
	 */
	va_start(argp, fmt);
	if (vsprintf_len(LOGMSGSZ, (char *)dbp->b_wptr, fmt, argp) == NULL)
		return (0);
	dbp->b_wptr += strlen((char *)dbp->b_wptr) + 1;
	ASSERT(dbp->b_wptr <= dbp->b_datap->db_lim && dbp->b_wptr[-1] == '\0');
	va_end(argp);

	/*
	 * parse the arguments and save them following the string.
	 */
	va_start(argp, fmt); /* reinitialize */
	parse_args(dbp, fmt, argp);
	va_end(argp);

	/*
	 * set up proto header
	 */
	cbp->b_datap->db_type = M_PROTO;
	cbp->b_cont = dbp;
	cbp->b_wptr += sizeof (struct log_ctl);
	lcp = (struct log_ctl *)cbp->b_rptr;
	lcp->mid = mid;
	lcp->sid = sid;
	{
		time_t	cur_time;
		clock_t	cur_lbolt;

		(void) drv_getparm(LBOLT, &cur_lbolt);
		lcp->ltime = cur_lbolt;
		(void) drv_getparm(TIME, &cur_time);
		lcp->ttime = cur_time;
	}
	lcp->level = level;
	lcp->flags = flags;

	/*
	 * The mutex is here to prevent the race condition in which the
	 * dedicated Q is being closed and strlog is trying to do putq on
	 * the Q.
	 */
	mutex_enter(&logdq_lock);
	if (log_d_q != NULL)
		rval = putq(log_d_q, cbp);
	mutex_exit(&logdq_lock);
	if (rval == 0)
		freemsg(cbp);
	return (rval);
}

/*
 * The following parser is the modified version of that in prf_internal. It
 * will have to been maintained accordingly.
 */
static void
parse_args(mblk_t *dmp, char *fmt, va_list adx)
{
	int		c;
	int		ells;
	int		numargs;
	uint32_t	*argptr;

	argptr = (uint32_t *)logadjust(dmp->b_wptr);

	numargs = min(NLOGARGS, (uint32_t *)dmp->b_datap->db_lim - argptr);

	if (numargs == 0)
		return;

	while (numargs > 0) {
		while ((c = *fmt++) != '%') {
			if (c == '\0')
				return;
		}

		do {
			c = *fmt++;
		} while (c == ' ' || (c >= '0' && c <= '9'));

		for (ells = 0; c == 'l'; c = *fmt++)
			ells++;

		switch (c) {
		case 'd':
		case 'D':
			if (ells == 0)
				*argptr++ = va_arg(adx, int);
			else if (ells == 1)
				*argptr++ = (int32_t)va_arg(adx, long);
			else
				*argptr++ = (int32_t)va_arg(adx, int64_t);
			break;

		case 'p':
			ells = 1;
			/*FALLTHROUGH*/
		case 'x':
		case 'X':
		case 'u':
		case 'o':
		case 'O':
			/*FALLTHROUGH*/
			if (ells == 0)
				*argptr++ = va_arg(adx, u_int);
			else if (ells == 1)
				*argptr++ = (uint32_t)va_arg(adx, u_long);
			else
				*argptr++ = (uint32_t)va_arg(adx, uint64_t);
			break;

		case 'c':
			*argptr++ = va_arg(adx, int);
			break;

		case 's':
			*argptr++ = (int32_t)va_arg(adx, char *);
			break;

		case '\0':
			return;

		default:
			continue;
		}

		dmp->b_wptr = (unsigned char *)argptr;
		--numargs;
	}
}
