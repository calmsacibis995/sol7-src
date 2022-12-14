/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)date.c	1.27	97/02/18 SMI"	/* SVr4.0 1.24	*/

/*
 *
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988,1989,1994  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		    All rights reserved.
 */

/*
 *	date - with format capabilities and international flair
 */

#include	<locale.h>
#include	<fcntl.h>
#include	<langinfo.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<time.h>
#include	<unistd.h>
#include	<sys/time.h>
#include	<sys/types.h>
#include	<ctype.h>
#include	<utmpx.h>
#include	"utmp.h"

#define	year_size(A)	(((A) % 4) ? 365 : 366)
static 	char	buf[BUFSIZ];
static	time_t	clock_val;
static  short	month_size[12] =
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
static  struct  utmp wtmp[2] = {
	{"", "", OTIME_MSG, 0, OLD_TIME, 0, 0, 0},
	{"", "", NTIME_MSG, 0, NEW_TIME, 0, 0, 0}
	};
static char *usage =
	"usage:\tdate [-u] mmddHHMM[[cc]yy][.SS]\n\tdate [-u] [+format]\n"
	"\tdate -a [-]sss[.fff]\n";
static int uflag;

static int get_adj(char *, struct timeval *);
static int setdate(struct tm *, char *);

main(argc, argv)
int	argc;
char	**argv;
{
	struct tm *tp, tm;
	struct timeval tv;
	char *fmt;
	int c, aflag = 0, illflag = 0;

	(void) setlocale(LC_ALL, "");

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "a:u")) != EOF)
		switch (c) {
		case 'a':
			aflag++;
			if (get_adj(optarg, &tv) < 0) {
				(void) fprintf(stderr,
				    "date: invalid argument -- %s\n", optarg);
				illflag++;
			}
			break;
		case 'u':
			uflag++;
			break;
		default:
			illflag++;
		}

	argc -= optind;
	argv  = &argv[optind];

	/* -u and -a are mutually exclusive */
	if (uflag && aflag)
		illflag++;

	if (illflag) {
		(void) fprintf(stderr, gettext(usage));
		exit(1);
	}

	(void) time(&clock_val);

	if (aflag) {
		if (adjtime(&tv, 0) < 0) {
			perror(gettext("date: Failed to adjust date"));
			exit(1);
		}
		exit(0);
	}

	if (argc > 0) {
		if (*argv[0] == '+')
			fmt = &argv[0][1];
		else {
			if (setdate(localtime(&clock_val), argv[0])) {
				(void) fprintf(stderr, gettext(usage));
				exit(1);
			}
			fmt = nl_langinfo(_DATE_FMT);
		}
	} else
		fmt = nl_langinfo(_DATE_FMT);

	if (uflag) {
		(void) putenv("TZ=GMT");
		tzset();
		tp = gmtime(&clock_val);
	} else
		tp = localtime(&clock_val);
	(void) memcpy(&tm, tp, sizeof (struct tm));
	(void) strftime(buf, BUFSIZ, fmt, &tm);

	(void) puts(buf);

	return (0);
}

static int
setdate(current_date, date)
struct tm	*current_date;
char		*date;
{
	int	i;
	int	mm;
	int	hh;
	int	min;
	int	sec = 0;
	char	*secptr;
	int	yy;
	int	dd	= 0;
	int	wf	= 0;
	int	minidx	= 6;
	int	len = 0;

	/*  Parse date string  */
	if ((secptr = strchr(date, '.')) != NULL && strlen(&secptr[1]) == 2 &&
	    isdigit(secptr[1]) && isdigit(secptr[2]) &&
	    (sec = atoi(&secptr[1])) >= 0 && sec < 60)
		secptr[0] = '\0';	/* eat decimal point only on success */

	len = strlen(date);

	for (i = 0; i < len; i++) {
		if (!isdigit(date[i])) {
			(void) fprintf(stderr,
			gettext("date: bad conversion\n"));
			exit(1);
		}
	}
	switch (strlen(date)) {
	case 12:
		yy = atoi(&date[8]);
		date[8] = '\0';
		break;
	case 10:
		/*
		 * The YY format has the following representation:
		 * 00-69 = 2000 thru 2069
		 * 70-99 = 1970 thru 1999 
		 */
		if (atoi(&date[8]) <= 69) {
			yy = 1900 + (atoi(&date[8]) + 100);
		} else {
			yy = 1900 + atoi(&date[8]);
		}
		date[8] = '\0';
		break;
	case 8:
		yy = 1900 + current_date->tm_year;
		break;
	case 4:
		yy = 1900 + current_date->tm_year;
		mm = current_date->tm_mon + 1; 	/* tm_mon goes from 1 to 11 */
		dd = current_date->tm_mday;
		minidx = 2;
		break;
	default:
		(void) fprintf(stderr, gettext("date: bad conversion\n"));
		return (1);
	}

	min = atoi(&date[minidx]);
	date[minidx] = '\0';
	hh = atoi(&date[minidx-2]);
	date[minidx-2] = '\0';

	if (!dd) {
		/*
		 * if dd is 0 (not between 1 and 31), then
		 * read the value supplied by the user.
		 */
		dd = atoi(&date[2]);
		date[2] = '\0';
		mm = atoi(&date[0]);
	}

	if (hh == 24)
		hh = 0, dd++;

	/*  Validate date elements  */
	if (!((mm >= 1 && mm <= 12) && (dd >= 1 && dd <= 31) &&
		(hh >= 0 && hh <= 23) && (min >= 0 && min <= 59))) {
		(void) fprintf(stderr, gettext("date: bad conversion\n"));
		return (1);
	}

	/*  Build date and time number  */
	for (clock_val = 0, i = 1970; i < yy; i++)
		clock_val += year_size(i);
	/*  Adjust for leap year  */
	if (year_size(yy) == 366 && mm >= 3)
		clock_val += 1;
	/*  Adjust for different month lengths  */
	while (--mm)
		clock_val += (time_t)month_size[mm - 1];
	/*  Load up the rest  */
	clock_val += (time_t)(dd - 1);
	clock_val *= 24;
	clock_val += (time_t)hh;
	clock_val *= 60;
	clock_val += (time_t)min;
	clock_val *= 60;
	clock_val += sec;

	if (!uflag) {
		/* convert to GMT assuming standard time */
		/* correction is made in localtime(3C) */

		clock_val += (time_t)timezone;

		/* correct if daylight savings time in effect */

		if (localtime(&clock_val)->tm_isdst)
			clock_val = clock_val - (time_t)(timezone - altzone);
	}

	(void) time(&wtmp[0].ut_time);
	if (stime(&clock_val) < 0) {
		(void) fprintf(stderr, gettext("date: no permission\n"));
		return (1);
	}
#if defined(i386)
	/* correct the kernel's "gmt_lag" and the PC's RTC */
	system("/usr/sbin/rtc -c > /dev/null 2>&1");
#endif
	(void) time(&wtmp[1].ut_time);
	(void) pututline(&wtmp[0]);
	(void) pututline(&wtmp[1]);
	(void) updwtmp(WTMP_FILE, &wtmp[0]);
	(void) updwtmp(WTMP_FILE, &wtmp[1]);
	return (0);
}

static int
get_adj(cp, tp)
char *cp;
struct timeval *tp;
{
	register int mult;
	int sign;

	/* arg must be [-]sss[.fff] */

	tp->tv_sec = tp->tv_usec = 0;
	if (*cp == '-') {
		sign = -1;
		cp++;
	} else {
		sign = 1;
	}

	while (*cp >= '0' && *cp <= '9') {
		tp->tv_sec *= 10;
		tp->tv_sec += *cp++ - '0';
	}
	if (*cp == '.') {
		cp++;
		mult = 100000;
		while (*cp >= '0' && *cp <= '9') {
			tp->tv_usec += (*cp++ - '0') * mult;
			mult /= 10;
		}
	}
	/*
	 * if there's anything left in the string,
	 * the input was invalid.
	 */
	if (*cp) {
		return (-1);
	} else {
		tp->tv_sec *= sign;
		tp->tv_usec *= sign;
		return (0);
	}
}
