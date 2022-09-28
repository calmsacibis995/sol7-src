/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SORT_H
#define	_SORT_H

#pragma ident	"@(#)sort.h	1.3	97/10/01	SMI"

#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <limits.h>
#include <locale.h>
#include <widec.h>
#include <wctype.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ulimit.h>
#include <errno.h>
#include <note.h>
#include <ulimit.h>

#ifdef DEBUG
#include <assert.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if	!defined(TEXT_DOMAIN)		/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"		/* Use this only if it weren't */
#endif

#define	N		16
					/*
					 * max num files open at one time and
					 * can be stored in core memory
					 */
#define	INIT_NUM_FIELDS	10
#define	MTHRESH		8
					/*
					 * threshhold for doing median of 3
					 * qksort selection
					 */
#define	TREEZ		32		/* TREEZ >= N and best if power of 2 */

#define	INIT_MAXREC	256		/* Max # of chars in each record */
#define	INC_MAXREC	128
#define	MAX_MON_LEN	20		/* Max. # of chars of month names. */
#define	INIT_COLL_LEN	128

/*
 * Memory administration
 *
 * Using a lot of memory is great when sorting a lot of data.
 * Using a megabyte to sort the output of `who' loses big.
 * MAXMEM, MINMEM and DEFMEM define the absolute maximum,
 * minimum and default memory requirements.  Administrators
 * can override any or all of these via defines at compile time.
 * Users can override the amount allocated (within the limits
 * of MAXMEM and MINMEM) on the command line.
 */

#ifndef MAXMEM
#define	MAXMEM	1048576			/* Megabyte maximum */
#endif

#ifndef MINMEM
#define	MINMEM	16384			/* 16K minimum */
#endif

#ifndef DEFMEM
#define	DEFMEM	32768			/* Same as old sort */
#endif


enum comp_type {ASC = 0, NUM = 1, MON = 2};

#define	blank(c)		(iswspace(c) && ((c) != L'\n'))

#define	qsexc(p, q)		t = *p; *p = *q; *q = t
#define	qstexc(p, q, r)		t = *p; *p = *r; *r = *q; *q = t


static struct   field {
	wchar_t	(*code)(wchar_t);
					/*
					 * (-f) function that determines if
					 * lower-case letters should be folded
					 * into upper case letters
					 */
	int	(*ignore)(wchar_t);
					/*
					 * (-i) function that determines what
					 * characters to ignore in comparisons
					 */
	enum comp_type	fcmp;
					/*
					 * Compare data as months (MON) (-M),
					 * numeric (NUM) (-n), or ascii (ASC)
					 */
	int	rflg;
					/*
					 * Reverse the result of the compare
					 * (-r) == -1, default (no reverse) = 1
					 */
	int	bflg[2];
					/*
					 * Indicates when to ignore white space
					 * in comparisons.
					 * bflg[1] is incremented when keys
					 * are verified for correctness,
					 * bflg[0] is incremented otherwise
					 * for -b.
					 * bflg[0] is incremented with
					 * the -M, -n, and -t options.
					 */
	int	m[2];
	int	n[2];
					/*
					 * m and n are used to determine start
					 * and end characters for sort keys
					 * m[0] and n[0] are the first and
					 * last character of the starting
					 * sort key, m[1] and n[1] are the
					 * first and last character of the
					 * ending sort key.
					 */
};


static struct btree {
	wchar_t *rp;		/* data to sort */
	int	rn;		/* record/file number */
	int	recsz;		/* size of data */
	int	allocflag;	/* true if data is malloc, 0 otherwise */
};

static void	sort(void);
static void	msort(wchar_t **, wchar_t **);
static void	insert(struct btree **, int);
static void	merge(int, int);
static void	copy_line(wchar_t *, wchar_t *);
static void	write_line(wchar_t *);
static void	checksort(void);
static void	disorder(char *, wchar_t *);
static void	newfile(void);
static void	oldfile(void);
static void	safeoutfil(void);
static void	cant(char *);
static void	diag1(const char *, int);
static void	diag2(const char *f, const char *, int);
static void	term(void);
static void	initree(void);
static void	qksort(wchar_t **, wchar_t **);
static void	month_init(void);
static void	rderror(char *);
static void	wterror(char *);
static void	initdecpnt(void);
static void	newline_warning(void);
static void	usage(void);
static int	grow_core(unsigned int, unsigned int);
static int	xrline(FILE *, struct btree *);
static int	yrline(FILE *, int);
static int	getsign(wchar_t *, wchar_t *);
static int	cmp(wchar_t *, wchar_t *);
static int	cmpa(wchar_t *, wchar_t *);
static int	cmpsave(int);
static int	field(char *, int, int, int);
static int	number(char **);
static int	month(wchar_t *);
static int	nonprint(wchar_t);
static int	dict(wchar_t);
static int	zero(wchar_t);
static wchar_t	*skip(wchar_t *, struct field *, int);
static wchar_t	*skip_to_eol(wchar_t *);
static wchar_t	nofold(wchar_t);
static wchar_t	fold(wchar_t);
static char	*setfil(int);
static char	*get_subopt(int, char **, char);

#ifdef __cplusplus
}
#endif

#endif	/* _SORT_H */
