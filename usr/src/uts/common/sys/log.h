/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_LOG_H
#define	_SYS_LOG_H

#pragma ident	"@(#)log.h	1.14	97/09/24 SMI"	/* SVr4.0 11.8	*/

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Header file for the Streams Log Driver
 */

/*
 * Minor devices for the log driver.
 * 0 through 5 are reserved for well-known entry points.
 * Minors above CLONEMIN are given out on clone opens.
 */
#define	CONSWMIN	0	/* minor device to write to console log */
#define	CLONEMIN	5	/* minor device of clone interface */
#define	DEDICATED_Q	1	/* used by strlog; performance optimization */

struct log {
	unsigned log_state;
	queue_t *log_rdq;
	mblk_t	*log_tracemp;
	unsigned log_refcnt; /* number of instances of send message */
	kcondvar_t log_cv;
};

/*
 * Driver state values.
 */
#define	LOGOPEN 	0x01
#define	LOGERR		0x02
#define	LOGTRC		0x04
#define	LOGCONS		0x08
#define	LOGCLOSE	0x10

/*
 * Module information structure fields
 */
#define	LOG_MID		44
#define	LOG_NAME	"LOG"
#define	LOG_MINPS	0
#define	LOG_MAXPS	1024
#define	LOG_HIWAT	8192
#define	LOG_LOWAT	2048

extern struct log log_log[];	/* log device state table */
extern int log_cnt;		/* number of configured minor devices */

/*
 * STRLOG(mid,sid,level,flags,fmt,args) should be used for those trace
 * calls that are only to be made during debugging.
 */
#if defined(DEBUG) || defined(lint)
#define	STRLOG	strlog
#else
#define	STRLOG	0 && strlog
#endif


/*
 * Utility macros for strlog.
 */

/*
 * logadjust - move a character pointer up to the next int boundary after
 * its current value.  Assumes sizeof(int) is 2**n bytes for some integer n.
 */
#define	logadjust(wp) \
	(unsigned char *)(((uintptr_t)wp + (sizeof (int32_t) - 1)) & \
			~(sizeof (int32_t) - 1))

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LOG_H */
