#ident	"@(#)klog.c	1.4	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/klog.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_klog_c =
"$Header: klog.c,v 4.6 88/12/01 14:06:05 jtkohl Exp $";
#endif /* lint */

#include <kerberos/mit-copyright.h>
#include <sys/time.h>
#include <stdio.h>

#include <kerberos/krb.h>
#include <kerberos/klog.h>

static char *log_name = KRBLOG;
static int is_open;
static char logtxt[1000];

/*
 * This file contains two logging routines: kset_logfile()
 * to determine the file to which log entries should be written;
 * and klog() to write log entries to the file.
 */

/*
 * klog() is used to add entries to the logfile (see kset_logfile()
 * below).  Note that it is probably not portable since it makes
 * assumptions about what the compiler will do when it is called
 * with less than the correct number of arguments which is the
 * way it is usually called.
 *
 * The log entry consists of a timestamp and the given arguments
 * printed according to the given "format" string.
 *
 * The log file is opened and closed for each log entry.
 *
 * If the given log type "type" is unknown, or if the log file
 * cannot be opened, no entry is made to the log file.
 *
 * The return value is always a pointer to the formatted log
 * text string "logtxt".
 */

char *
klog(int type, char *format, int a1, int a2, int a3, int a4,
	int a5, int a6, int a7, int a8, int a9, int a0)
{
	FILE *logfile;
	time_t now;
	char timestamp[32];
	struct tm *tm;
	static int logtype_array[NLOGTYPE] = {0, 0};
	static int array_initialized;

	if (!(array_initialized++)) {
		logtype_array[L_NET_ERR] = 1;
		logtype_array[L_KRB_PERR] = 1;
		logtype_array[L_KRB_PWARN] = 1;
		logtype_array[L_APPL_REQ] = 1;
		logtype_array[L_INI_REQ] = 1;
		logtype_array[L_DEATH_REQ] = 1;
		logtype_array[L_NTGT_INTK] = 1;
		logtype_array[L_ERR_SEXP] = 1;
		logtype_array[L_ERR_MKV] = 1;
		logtype_array[L_ERR_NKY] = 1;
		logtype_array[L_ERR_NUN] = 1;
		logtype_array[L_ERR_UNK] = 1;
	}

	(void) sprintf(logtxt, format, a1, a2, a3, a4, a5, a6, a7, a8, a9, a0);

	if (!logtype_array[type])
		return (logtxt);

	if ((logfile = fopen(log_name, "a")) == NULL)
		return (logtxt);

	(void) time(&now);
	tm = localtime(&now);

	(void) strftime(timestamp, 32, "%e-%b-%y %T", tm);
	fprintf(logfile, "%s %s\n", timestamp, logtxt);
	(void) fclose(logfile);
	return (logtxt);
}

/*
 * kset_logfile() changes the name of the file to which
 * messages are logged.  If kset_logfile() is not called,
 * the logfile defaults to KRBLOG, defined in "krb.h".
 */

kset_logfile(char *filename)
{
	log_name = filename;
	is_open = 0;
	return(0);
}
