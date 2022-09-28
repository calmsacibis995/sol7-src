/*
 * Copyright (c) 1996 - 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)grep.c	1.26	98/02/12 SMI" 	/* SVr4.0 1.22 */

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
 * grep -- print lines matching (or not matching) a pattern
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 */

#include <sys/types.h>

#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <memory.h>
#include <regexpr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char * const errstr[] = {
	"Range endpoint too large.",
	"Bad number.",
	"``\\digit'' out of range.",
	"No remembered search string.",
	"\\( \\) imbalance.",
	"Too many \\(.",
	"More than 2 numbers given in \\{ \\}.",
	"} expected after \\.",
	"First number exceeds second in \\{ \\}.",
	"[ ] imbalance.",
	"Regular expression overflow.",
	"Illegal byte sequence.",
	"Unknown regexp error code!!",
	NULL
};

#define	errmsg(msg, arg)	(void) fprintf(stderr, gettext(msg), arg)
#define	BLKSIZE	512

static int	temp;
static long long	lnum;
static char	*linebuf;
static char	*prntbuf = NULL;
static long	fw_lPrntBufLen = 0;
static int	nflag;
static int	bflag;
static int	lflag;
static int	cflag;
static int	vflag;
static int	sflag;
static int	iflag;
static int	wflag;
static int	hflag;
static int	errflg;
static int	nfile;
static long long	tln;
static int	nsucc;
static int	nlflag;
static char	*ptr, *ptrend;
static char	*expbuf;

static void	execute(char *);
static void	regerr(int);
static int	succeed(char *);

int
main(
    int		argc,
    char 	**argv)
{
	int	c;
	char	*arg;
	extern int	optind;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "hblcnsviyw")) != -1)
		switch (c) {
		case 'h':
			hflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'c':
			cflag++;
			break;
		case 'n':
			nflag++;
			break;
		case 'b':
			bflag++;
			break;
		case 's':
			sflag++;
			break;
		case 'l':
			lflag++;
			break;
		case 'y':
		case 'i':
			iflag++;
			break;
		case 'w':
			wflag++;
			break;
		case '?':
			errflg++;
		}

	if (errflg || (optind >= argc)) {
		errmsg("Usage: grep -hblcnsviw pattern file . . .\n",
			(char *)NULL);
		exit(2);
	}

	argv = &argv[optind];
	argc -= optind;
	nfile = argc - 1;

	if (strrchr(*argv, '\n') != NULL)
		regerr(41);

	if (iflag) {
		for (arg = *argv; *arg != NULL; ++arg)
			*arg = (char)tolower((int)((unsigned char)*arg));
	}

	if (wflag) {
		unsigned int	wordlen;
		char		*wordbuf;

		wordlen = strlen(*argv) + 4;
		if ((wordbuf = malloc(wordlen)) == NULL) {
			errmsg("grep: Out of memory for word\n", (char *)NULL);
			exit(2);
		}

		(void) strcpy(wordbuf, "\\<");
		(void) strcat(wordbuf, *argv);
		(void) strcat(wordbuf, "\\>");
		*argv = wordbuf;
	}

	expbuf = compile(*argv, (char *)0, (char *)0);
	if (regerrno)
		regerr(regerrno);

	if (--argc == 0)
		execute(NULL);
	else
		while (argc-- > 0)
			execute(*++argv);

	return (nsucc == 2 ? 2 : (nsucc == 0 ? 1 : 0));
}

static void
execute(
    char	*file)
{
	char	*lbuf, *p;
	long	count;
	long	offset = 0;
	char	*next_ptr = NULL;
	long	next_count = 0;

	tln = 0;

	if (prntbuf == NULL) {
		fw_lPrntBufLen = BUFSIZ + 1;
		if ((prntbuf = malloc(fw_lPrntBufLen)) == NULL) {
			exit(2); /* out of memory - BAIL */
		}
		if ((linebuf = malloc(fw_lPrntBufLen)) == NULL) {
			exit(2); /* out of memory - BAIL */
		}
	}

	if (file == NULL)
		temp = 0;
	else if ((temp = open(file, O_RDONLY)) == -1) {
		if (!sflag)
			errmsg("grep: can't open %s\n", file);
		nsucc = 2;
		return;
	}

	/* read in first block of bytes */
	if ((count = read(temp, prntbuf, BUFSIZ)) <= 0) {
		(void) close(temp);

		if (cflag) {
			if (nfile > 1 && !hflag && file)
				(void) fprintf(stdout, "%s:", file);
			(void) fprintf(stdout, "%lld\n", tln);
		}
		return;
	}

	lnum = 0;
	ptr = prntbuf;
	for (;;) {
		/* look for next newline */
		if ((ptrend = memchr(ptr + offset, '\n', count)) == NULL) {
			offset += count;

			/*
			 * shift unused data to the beginning of the buffer
			 */
			if (ptr > prntbuf) {
				(void) memmove(prntbuf, ptr, offset);
				ptr = prntbuf;
			}

			/*
			 * re-allocate a larger buffer if this one is full
			 */
			if (offset + BUFSIZ > fw_lPrntBufLen) {
				/*
				 * allocate a new buffer and preserve the
				 * contents...
				 */
				fw_lPrntBufLen += BUFSIZ;
				if ((prntbuf = realloc(prntbuf,
				    fw_lPrntBufLen)) == NULL)
					exit(2);

				/*
				 * set up a bigger linebuffer (this is only used
				 * for case insensitive operations). Contents do
				 * not have to be preserved.
				 */
				free(linebuf);
				if ((linebuf = malloc(fw_lPrntBufLen)) == NULL)
					exit(2);

				ptr = prntbuf;
			}

			p = prntbuf + offset;
			if ((count = read(temp, p, BUFSIZ)) > 0)
				continue;

			if (offset == 0)
				/* end of file already reached */
				break;

			/* last line of file has no newline */
			ptrend = ptr + offset;
			nlflag = 0;
		} else {
			next_ptr = ptrend + 1;
			next_count = offset + count - (next_ptr - ptr);
			nlflag = 1;
		}
		lnum++;
		*ptrend = '\0';

		if (iflag) {
			/*
			 * Make a lower case copy of the record
			 */
			p = ptr;
			for (lbuf = linebuf; p < ptrend; )
				*lbuf++ = (char)tolower((int)
				    (unsigned char)*p++);
			*lbuf = '\0';
			lbuf = linebuf;
		} else
			/*
			 * Use record as is
			 */
			lbuf = ptr;

		/* lflag only once */
		if ((step(lbuf, expbuf) ^ vflag) && succeed(file) == 1)
			break;

		if (!nlflag)
			break;

		ptr = next_ptr;
		count = next_count;
		offset = 0;
	}
	(void) close(temp);

	if (cflag) {
		if (nfile > 1 && !hflag && file)
			(void) fprintf(stdout, "%s:", file);
		(void) fprintf(stdout, "%lld\n", tln);
	}
}

static int
succeed(
    char	*f)
{
	int nchars;
	nsucc = (nsucc == 2) ? 2 : 1;

	if (f == NULL)
		f = "<stdin>";

	if (cflag) {
		tln++;
		return (0);
	}

	if (lflag) {
		(void) fprintf(stdout, "%s\n", f);
		return (1);
	}

	if (nfile > 1 && !hflag)
		/* print filename */
		(void) fprintf(stdout, "%s:", f);

	if (bflag)
		/* print block number */
		(void) fprintf(stdout, "%lld:", (lseek(temp, (off_t)0,
			SEEK_CUR)-1)/BLKSIZE);

	if (nflag)
		/* print line number */
		(void) fprintf(stdout, "%lld:", lnum);

	if (nlflag) {
		/* newline at end of line */
		*ptrend = '\n';
		nchars = ptrend - ptr + 1;
	} else {
		/* don't write sentinel \0 */
		nchars = ptrend - ptr;
	}

	(void) fwrite(ptr, 1, nchars, stdout);
	return (0);
}

static void
regerr(
    int	err)
{
	errmsg("grep: RE error %d: ", err);
	switch (err) {
		case 11:
			err = 0;
			break;
		case 16:
			err = 1;
			break;
		case 25:
			err = 2;
			break;
		case 41:
			err = 3;
			break;
		case 42:
			err = 4;
			break;
		case 43:
			err = 5;
			break;
		case 44:
			err = 6;
			break;
		case 45:
			err = 7;
			break;
		case 46:
			err = 8;
			break;
		case 49:
			err = 9;
			break;
		case 50:
			err = 10;
			break;
		case 67:
			err = 11;
			break;
		default:
			err = 12;
			break;
	}

	errmsg("%s\n", gettext(errstr[err]));
	exit(2);
}
