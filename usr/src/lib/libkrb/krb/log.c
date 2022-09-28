#ident	"@(#)log.c	1.4	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/log.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_log_c =
"$Header: log.c,v 4.7 88/12/01 14:15:14 jtkohl Exp $";
#endif /* lint */

#include <kerberos/mit-copyright.h>
#include <sys/time.h>
#include <stdio.h>

#include <kerberos/krb.h>
#include <kerberos/klog.h>

static char *log_name = KRBLOG;
static is_open;

/*
 * This file contains three logging routines: set_logfile()
 * to determine the file that log entries should be written to;
 * and log() and new_log() to write log entries to the file.
 */

/*
 * log() is used to add entries to the logfile (see set_logfile()
 * below).  Note that it is probably not portable since it makes
 * assumptions about what the compiler will do when it is called
 * with less than the correct number of arguments which is the
 * way it is usually called.
 *
 * The log entry consists of a timestamp and the given arguments
 * printed according to the given "format".
 *
 * The log file is opened and closed for each log entry.
 *
 * The return value is undefined.
 */

extern char *month_sname();

/*VARARGS1*/
void
log(char *format, int a1, int a2, int a3, int a4, int a5, int a6,
	int a7, int a8, int a9, int a0)
{
	FILE *logfile;
	time_t now;
	struct tm *tm;
	char timestamp[32];

	if ((logfile = fopen(log_name, "a")) == NULL)
		return;

	(void) time(&now);
	tm = localtime(&now);

	(void) strftime(timestamp, 32, "%e-%b-%y %T ", tm);
	fprintf(logfile, timestamp);
	fprintf(logfile, format, a1, a2, a3, a4, a5, a6, a7, a8, a9, a0);
	fprintf(logfile, "\n");
	(void) fclose(logfile);
}

/*
 * set_logfile() changes the name of the file to which
 * messages are logged.  If set_logfile() is not called,
 * the logfile defaults to KRBLOG, defined in "krb.h".
 */

set_logfile(char *filename)
{
	log_name = filename;
	is_open = 0;
	return(0);
}

/*
 * new_log() appends a log entry containing the give time "t" and the
 * string "string" to the logfile (see set_logfile() above).  The file
 * is opened once and left open.  The routine returns 1 on failure, 0
 * on success.
 */

new_log(time_t t, char *string)
{
	static FILE *logfile;

	struct tm *tm;
	char timestamp[32];

	if (!is_open) {
		if ((logfile = fopen(log_name, "a")) == NULL)
			return (1);
		is_open = 1;
	}

	if (t) {
		tm = localtime(&t);

		(void) strftime(timestamp, 32, "%e-%b-%y %T", tm);
		fprintf(logfile, "\n%s  %s", timestamp, string);
	} else {
		fprintf(logfile, "\n%20s%s", "", string);
	}

	(void) fflush(logfile);
	return (0);
}
