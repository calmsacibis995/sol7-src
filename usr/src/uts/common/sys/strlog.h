/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_STRLOG_H
#define	_SYS_STRLOG_H

#pragma ident	"@(#)strlog.h	1.13	97/08/27 SMI"	/* SVr4.0 11.7	*/

#include <sys/types.h>
#include <sys/types32.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Streams Log Driver Interface Definitions
 */

/*
 * structure of control portion of log message
 */
struct log_ctl {
	short	mid;
	short	sid;
	char 	level;		/* level of message for tracing */
	short	flags;		/* message disposition */
#if defined(_LP64) || defined(_I32LPx)
	clock32_t ltime;	/* time in machine ticks since boot */
	time32_t ttime;		/* time in seconds since 1970 */
#else
	clock_t	ltime;
	time_t	ttime;
#endif
	int	seq_no;		/* sequence number */
	int	pri;		/* priority = (facility|level) */
};

/* Flags for log messages */

#define	SL_FATAL	0x01	/* indicates fatal error */
#define	SL_NOTIFY	0x02	/* logger must notify administrator */
#define	SL_ERROR	0x04	/* include on the error log */
#define	SL_TRACE	0x08	/* include on the trace log */
#define	SL_CONSOLE	0x10	/* log message to console */
#define	SL_WARN		0x20	/* warning message */
#define	SL_NOTE		0x40	/* notice message */

/*
 * Structure defining ids and levels desired by the tracer (I_TRCLOG).
 */
struct trace_ids {
	short ti_mid;
	short ti_sid;
	char  ti_level;
};

/*
 * Log Driver I_STR ioctl commands
 */

#define	LOGCTL		(('L')<<8)
#define	I_TRCLOG	(LOGCTL|1)	/* process is tracer */
#define	I_ERRLOG	(LOGCTL|2)	/* process is error logger */
#define	I_CONSLOG	(LOGCTL|3)	/* process is console logger */

/*
 * Parameter definitions for logger messages
 */
#define	LOGMSGSZ	1024
#define	NLOGARGS	3

#ifdef _KERNEL
/*PRINTFLIKE5*/
extern int strlog(short, short, char, unsigned short, char *, ...);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_STRLOG_H */
