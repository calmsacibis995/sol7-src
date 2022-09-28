/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 * Copyright (c) 1997 Sun Microsystems, Inc.
 * All rights reserved
 */

#ident	"@(#)mknod.c	1.6	97/08/12 SMI"	/* SVr4.0 1.9	*/

/*
 *	mknod - build special file
 *
 *	mknod  name [ b ] [ c ] major minor
 *	mknod  name m	(shared data)
 *	mknod  name p	(named pipe)
 *	mknod  name s	(semaphore)
 *
 *	MODIFICATION HISTORY
 *	M000	11 Apr 83	andyp	3.0 upgrade
 *	- (Mostly uncommented).  Picked up 3.0 source.
 *	- Added header.  Changed usage message.  Replaced hard-coded
 *	  makedev with one from <sys/types.h>.
 *	- Added mechanism for creating name space files.
 *	- Added some error checks.
 *	- Semi-major reorganition.
 */

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mkdev.h>

#define	ACC	0666

static int domk(const char *path, const mode_t mode, const dev_t arg);
static long number(const char *s);
static void usage(void);

int
main(int argc, char **argv)
{
	mode_t	mode;
	dev_t	arg;
	major_t	majno;
	minor_t	minno;

	if (argc < 3 || argc > 5)
		usage();

	if (argv[2][1] != '\0')
		usage();

	if (argc == 3) {
		switch (argv[2][0]) {
		case 'p':
			mode = S_IFIFO;
			arg = 0;	/* (not used) */
			break;
		case 'm':
			mode = S_IFNAM;
			arg = S_INSHD;
			break;
		case 's':
			mode = S_IFNAM;
			arg = S_INSEM;
			break;
		default:
			usage();
			/* NO RETURN */
		}
	} else if (argc == 5) {
		switch (argv[2][0]) {
		case 'b':
			mode = S_IFBLK;
			break;
		case 'c':
			mode = S_IFCHR;
			break;
		default:
			usage();
		}
		if (getuid()) {
			fprintf(stderr, "mknod: must be super-user\n");
			return (2);
		}
		majno = (major_t)number(argv[3]);
		if (majno == (major_t)-1 || majno > MAXMAJ) {
			fprintf(stderr, "mknod: invalid major number '%s' "
			    "- valid range is 0-%lu\n", argv[3], MAXMAJ);
			return (2);
		}
		minno = (minor_t)number(argv[4]);
		if (minno == (minor_t)-1 || minno > MAXMIN) {
			fprintf(stderr, "mknod: invalid minor number '%s' "
			    "- valid range is 0-%lu\n", argv[4], MAXMIN);
			return (2);
		}
		arg = makedev(majno, minno);
	} else
		usage();

	return (domk(argv[1], (mode | ACC), arg) ? 2 : 0);
}

static int
domk(const char *path, const mode_t mode, const dev_t arg)
{
	int ec;

	if ((ec = mknod(path, mode, arg)) == -1) {
		perror("mknod");
	} else {
		/* chown() return deliberately ignored */
		(void) chown(path, getuid(), getgid());
	}
	return (ec);
}

static long
number(const char *s)
{
	long n;

	errno = 0;
	n = strtol(s, NULL, 0);
	if (errno != 0 || n < 0)
		return (-1);
	return (n);
}

static void
usage(void)
{
	fprintf(stderr, "usage: mknod name [ b/c major minor ] [ p ]\n");
	exit(2);
}
