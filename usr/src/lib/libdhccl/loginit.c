/*
 * bugcode.c: "Initialise the debug stream".
 *
 * SYNOPSIS
 *    void loginit(int loglevel, int dbglevel, int isdaemon, char* logfile)
 *    void logclose()
 *
 * DESCRIPTION
 *    Initializes error, warning, log and debug message output.
 *
 *    loglevel, when dbglevel is zero, indicates whether information will be
 *    output:
 *        0 - only error messages
 *        1 - errors and warnings
 *        2 - errors, warnings, and log
 *    It is ignored if dbglevel is non-zero.
 *
 *    dbglevel of non-zero means debug is on, which overrides the
 *    meaning of loglevel: all messages are turned on, and the
 *    buffering  of the stream is set to to line-mode (i.e. the stream is
 *    flushed whenever a new line is seen).
 *
 *    The messages will go to either:
 *
 *        (1) a file defined by environment variable DHCPLOG if defined or
 *        (2) the file named by argument logfile if not NULL, or
 *        (3) stderr otherwise.
 *
 *    If the isdaemon flag is true, errors and warnings will also be
 *    sent to the system logger (SYSLOG(5)) at priority LOG_ERR and
 *    LOG_WARN respectively (UNIX only).
 *
 *    Loginit() generates no messages itself as it cannot be certain
 *    of the disposition of those messages.
 *
 *    Returns 0 if success, -1 if failure.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)loginit.c 1.6 96/12/26 SMI"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <syslog.h>
#include "catype.h"
#include "unixgen.h"
#include "daemon.h"

FILE *stdbug = NULL;
static int logLevel = 0;
static int isDaemon = FALSE;

static void
loglog(char *fmt, ...)
{
	va_list args;

	if (logLevel < 2)
		return;

	va_start(args, fmt);
	if (isDaemon)
		vsyslog(LOG_INFO, fmt, args);
	else
		vfprintf(stdbug, fmt, args);
	va_end(args);
}

static void
logerr(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (isDaemon)
		vsyslog(LOG_ERR, fmt, args);
	else
		vfprintf(stdbug, fmt, args);
	va_end(args);
}

static void
logwarn(char *fmt, ...)
{
	va_list args;

	if (logLevel < 1)
		return;

	va_start(args, fmt);
	if (isDaemon)
		vsyslog(LOG_WARNING, fmt, args);
	else
		vfprintf(stdbug, fmt, args);
	va_end(args);
}

static void
logdbg(char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (isDaemon)
		vsyslog(LOG_DEBUG, fmt, args);
	else
		vfprintf(stdbug, fmt, args);
	va_end(args);
}

void
loginit(int loglevel, int dbglevel, int isdaemon, char *logfile)
{
	register FILE *tstdbug;
	char *logname;

	logLevel = loglevel;
	if (dbglevel > 0)
		logLevel = 2; /* turn on all debug */
	isDaemon = isdaemon;

	if (isDaemon == 0) {
		if (logname = getenv("DHCPLOG")) {
			tstdbug = fopen(logname, "a");
		} else if (isdaemon && logfile != NULL) {
			tstdbug = fopen(logfile, "a");
		} else
			tstdbug = NULL;

		if (tstdbug == NULL)
			stdbug = stderr;
		else
			stdbug = tstdbug;

		if (dbglevel > 0 && stdbug != stderr)
			setvbuf(stdbug, NULL, _IOLBF, BUFSIZ);
	} else {
		/* If daemonized, use syslog exclusively */
		openlog(DAEMON_NAME, LOG_NOWAIT | LOG_PID, LOG_DAEMON);
	}
	logb = (void (*)(const char *, ...))logdbg;
	loge = (void (*)(const char *, ...))logerr;
	logl = (void (*)(const char *, ...))loglog;
	logw = (void (*)(const char *, ...))logwarn;
}

void
logclose(void)
{
	if (isDaemon)
		(void) closelog();
	else {
		if (stdbug != NULL && stdbug != stdout && stdbug != stderr)
			fclose(stdbug);
	}
	stdbug = NULL;
}
