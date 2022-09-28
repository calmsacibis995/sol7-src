#ident	"@(#)misc.c	1.15	97/03/27	SMI"

/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <varargs.h>
#include <syslog.h>
#include <locale.h>

/*
 * Miscellaneous support routines for in.dhcpd.
 */

/*
 * Error message function. If debugging off, then logging goes to
 * syslog.
 *
 * Must be MT SAFE - called by various threads as well as the main thread.
 */

extern int debug;
extern int errno;

/* VARARGS */
void
dhcpmsg(va_alist)
	va_dcl
{
	va_list		ap;
	register int	errlevel;
	char		*fmtp;
	char		buff[512], errbuf[80];

	va_start(ap);

	errlevel = (int) va_arg(ap, int);
	fmtp = (char *) va_arg(ap, char *);

	if (debug)  {
		if (errlevel != LOG_ERR)
			*errbuf = '\0';
		else
			(void) sprintf(errbuf, "(errno: %d)", errno);
		(void) sprintf(buff, "%s %s", errbuf, gettext(fmtp));
		(void) vfprintf(stderr, buff, ap);
	} else {
		if (errlevel != LOG_ERR)
			(void) sprintf(buff, "%s", gettext(fmtp));
		else
			(void) sprintf(buff, "(%%m) %s", gettext(fmtp));
		(void) vsyslog(errlevel, buff, ap);
	}

	va_end(ap);
}
