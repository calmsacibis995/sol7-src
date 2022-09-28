/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)sum.c	1.8	97/07/16 SMI"	/* SVr4.0 1.1	*/
/* LINTLIBRARY */

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 *	$Header: RCS/sum.c,v 1.4 88/04/26 05:56:26 root Exp $
 */

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <std.h>
#include <sum.h>
#include "libcmd.h"

#define	MSW(l)	(((l) >> 16) & 0x0000ffffL)
#define	LSW(l)	((l) & 0x0000ffffL)

/*
 *	sumpro -- prolog
 */
void
sumpro(struct suminfo *sip)
{
	sip->si_sum = sip->si_nbytes = 0L;
}

/*
 *	sumupd -- update
 */
void
sumupd(struct suminfo *sip, char *buf, int cnt)
{
	long sum;

	if (cnt <= 0)
	return;
	sip->si_nbytes += cnt;
	sum = sip->si_sum;
	while (cnt-- > 0)
	sum += *buf++ & 0x00ff;
	sip->si_sum = sum;
}

/*
 *	sumepi -- epilog
 */
void
sumepi(struct suminfo *sip)
{
	long sum;

	sum = sip->si_sum;
	sum = LSW(sum) + MSW(sum);
	sip->si_sum = (ushort) (LSW(sum) + MSW(sum));
}

/*
 *	sumout -- output
 */
void
sumout(FILE *fp, struct suminfo *sip)
{
#ifdef	M_V7
#define	FMT	"%lu\t%D"
#else
#define	FMT	"%lu\t%ld"
#endif
	(void) fprintf(
	fp, FMT,
	(ulong_t) sip->si_sum,
	(sip->si_nbytes + MULBSIZE - 1) / MULBSIZE);
#undef	FMT
}

#ifdef	BLACKBOX
char	*Pgm		= "tstsum";
char	Enoopen[]	= "cannot open %s\n";
char	Ebadread[]	= "read error on %s\n";

int
main(argc, argv)
int argc;
char **argv;
{
	char *pn;
	FILE *fp;
	int cnt;
	struct suminfo si;
	char buf[BUFSIZ];

	--argc; ++argv;
	for (; argc > 0; --argc, ++argv) {
	pn = *argv;
	if ((fp = fopen(pn, "r")) == NULL) {
		error(Enoopen, pn);
		continue;
	}
	sumpro(&si);
	while (((cnt = fread(buf, sizeof (char), BUFSIZ, fp))
		!= 0) && (cnt != EOF))
		sumupd(&si, buf, cnt);
	if (cnt == EOF && ferror(fp))
		error(Ebadread, pn);
	sumepi(&si);
	sumout(stdout, &si);
	printf("\t%s\n", pn);
	fclose(fp);
	}
	exit(0);
}

static
error(char *fmt, char *a1, char *a2, char *a3, char *a4, char *a5)
{
	(void) fprintf(stderr, "%s: ", Pgm);
	(void) fprintf(stderr, fmt, a1, a2, a3, a4, a5);
}
#endif
