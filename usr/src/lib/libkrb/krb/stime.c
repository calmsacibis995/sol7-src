/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/stime.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_stime_c =
"$Header: stime.c,v 4.5 88/11/15 16:58:05 jtkohl Exp $";
#endif /* lint */

#include <kerberos/mit-copyright.h>
#include <sys/time.h>
#include <stdio.h>                      /* for sprintf() */

/*
 * Given a pointer to a time_t containing the number of seconds
 * since the beginning of time (midnight 1 Jan 1970 GMT), return
 * a string containing the local time in the form:
 *
 * "25-Jan-88 10:17:56"
 */

char *
stime(time_t *t)
{
	static char st_data[40];
	static char *st = st_data;
	struct tm *tm;

	tm = localtime(t);
	(void) strftime(st, 40, "%e-%b-%y %T", tm);
	return (st);
}
