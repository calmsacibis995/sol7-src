/*
 * ident	"@(#)getconf.c	1.9	98/02/04 SMI"
 *
 * Copyright (c) 1994,1996,1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 *
 * getconf -- POSIX.2 compatible utility to query configuration specific
 *	      parameters.
 *         -- XPG4 support added June/93
 *
 *         -- XPG5 support added Dec/97
 *
 * Copyright 1985, 1993 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <locale.h>
#include <libintl.h>
#include <assert.h>

extern size_t confstr(int, char *, size_t);

static int aflag = 0;

#define	INVAL_ARG	"getconf: Invalid argument (%s)\n"
#define	INVAL_PATHARG	"getconf: Invalid argument (%s or %s)\n"

/*
 *  Notes:
 *  The sctab.value field is defined to be a long.
 *  There are some values that are "unsigned long"; these values
 *  can be stored in a "long" field but when output, must be printed
 *  as an unsigned value. Thus, these values must have UNSIGNED_VALUE bit
 *  set in sctab.flag field.
 *
 *  There are 2 different ways to indicate a symbol is undefined:
 *     1) sctab.flag = UNDEFINED
 *     2) or sctab.value = -1 (and if !UNDEFINED and !UNSIGNED_VALUE)
 *
 *  There are a group of symbols (e.g CHAR_MIN, INT_MAX, INT_MIN, LONG_BIT ...)
 *  which we may set to -1 if they are not pre-defined in a system header file.
 *  This is used to indicate that these symbols are "undefined".
 *  We are assuming that these symbols cannot reasonably have a value of -1
 *  if they were defined in a system header file.
 *  (Unless of course -1 can be used to indicate "undefined" for that symbol)
 */

static struct sctab {
	long value;
	char *name;
	enum { SELFCONF, SYSCONF, PATHCONF, CONFSTR } type;
	int flag;
/* bit fields for sctab.flag member */
#define	NOFLAGS		0	/* no special indicators */
#define	UNDEFINED	1	/* value is known undefined at compile time */
#define	UNSIGNED_VALUE	2	/* value is an unsigned */
} sctab[] = {
	/* POSIX.2 table 2-16 */
	_POSIX2_BC_BASE_MAX,	"POSIX2_BC_BASE_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_BC_DIM_MAX,	"POSIX2_BC_DIM_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_BC_SCALE_MAX,	"POSIX2_BC_SCALE_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_BC_STRING_MAX,	"POSIX2_BC_STRING_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_COLL_WEIGHTS_MAX, "POSIX2_COLL_WEIGHTS_MAX", SELFCONF,  NOFLAGS,
	_POSIX2_EXPR_NEST_MAX,	"POSIX2_EXPR_NEST_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_LINE_MAX,	"POSIX2_LINE_MAX",	SELFCONF,   NOFLAGS,
	_POSIX2_RE_DUP_MAX,	"POSIX2_RE_DUP_MAX",	SELFCONF,   NOFLAGS,
	_SC_2_VERSION,		"POSIX2_VERSION",	SYSCONF,   NOFLAGS,

	/* POSIX.2 table 2-17 */
	_SC_BC_BASE_MAX,	"BC_BASE_MAX",		SYSCONF,   NOFLAGS,
	_SC_BC_DIM_MAX,		"BC_DIM_MAX",		SYSCONF,   NOFLAGS,
	_SC_BC_SCALE_MAX,	"BC_SCALE_MAX",		SYSCONF,   NOFLAGS,
	_SC_BC_STRING_MAX,	"BC_STRING_MAX",	SYSCONF,   NOFLAGS,
	_SC_COLL_WEIGHTS_MAX,	"COLL_WEIGHTS_MAX",	SYSCONF,   NOFLAGS,
	_SC_EXPR_NEST_MAX,	"EXPR_NEST_MAX",	SYSCONF,   NOFLAGS,
	_SC_LINE_MAX,		"LINE_MAX",		SYSCONF,   NOFLAGS,
	_SC_RE_DUP_MAX,		"RE_DUP_MAX",		SYSCONF,   NOFLAGS,

	/* POSIX.2 table 2-18 */
#ifdef _SC_2_C_BIND   /* d11.2 not defined - but should be */
	_SC_2_C_BIND,		"POSIX2_C_BIND",	SYSCONF,   NOFLAGS,
#else
	_POSIX2_C_BIND,		"POSIX2_C_BIND",	SELFCONF,   NOFLAGS,
#endif /* _SC_2_C_BIND */
	_SC_2_C_DEV,		"POSIX2_C_DEV",		SYSCONF,   NOFLAGS,
	_SC_2_FORT_DEV,		"POSIX2_FORT_DEV",	SYSCONF,   NOFLAGS,
	_SC_2_FORT_RUN,		"POSIX2_FORT_RUN",	SYSCONF,   NOFLAGS,
	_SC_2_LOCALEDEF,	"POSIX2_LOCALEDEF",	SYSCONF,   NOFLAGS,
	_SC_2_SW_DEV,		"POSIX2_SW_DEV",	SYSCONF,   NOFLAGS,

	/* POSIX.2a modifications to POSIX.2 table 2-18 */
#ifdef _SC_2_CHAR_TERM
	_SC_2_CHAR_TERM,	"POSIX2_CHAR_TERM",	SYSCONF,   NOFLAGS,
#endif
	_SC_2_UPE,		"POSIX2_UPE",		SYSCONF,   NOFLAGS,

	/* ISO/IEC 9945-1:1990 table 2-3 */
	_POSIX_ARG_MAX,		"_POSIX_ARG_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_CHILD_MAX,	"_POSIX_CHILD_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_LINK_MAX,	"_POSIX_LINK_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_MAX_CANON,	"_POSIX_MAX_CANON",	SELFCONF,   NOFLAGS,
	_POSIX_MAX_INPUT,	"_POSIX_MAX_INPUT",	SELFCONF,   NOFLAGS,
	_POSIX_NAME_MAX,	"_POSIX_NAME_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_NGROUPS_MAX,	"_POSIX_NGROUPS_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_OPEN_MAX,	"_POSIX_OPEN_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_PATH_MAX,	"_POSIX_PATH_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_PIPE_BUF,	"_POSIX_PIPE_BUF",	SELFCONF,   NOFLAGS,
#if _POSIX_VERSION == 198809L

/*
 * these symbols not defined in P1003.1-1988
 * but P1003.2-1992 specifies them,
 * so on .1-1988 systems we need to indicate that they
 * are "undefined" here.
 *
 */

	0,			"_POSIX_SSIZE_MAX",	SELFCONF,   UNDEFINED,
	0,			"_POSIX_STREAM_MAX",	SELFCONF,   UNDEFINED,
	0,			"_POSIX_TZNAME_MAX",	SELFCONF,   UNDEFINED,
#else
	_POSIX_SSIZE_MAX,	"_POSIX_SSIZE_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_STREAM_MAX,	"_POSIX_STREAM_MAX",	SELFCONF,   NOFLAGS,
	_POSIX_TZNAME_MAX,	"_POSIX_TZNAME_MAX",	SELFCONF,   NOFLAGS,
#endif /* _POSIX_VERSION == 198809L */

	/* ISO/IEC 9945-1:1990 table 4-2 */
	_SC_ARG_MAX,		"ARG_MAX",		SYSCONF,   NOFLAGS,
	_SC_CHILD_MAX,		"CHILD_MAX",		SYSCONF,   NOFLAGS,
#ifdef	_SC_CLK_TCK	/* this need not be supported - .2d11.2 pg 443 */
	_SC_CLK_TCK,		"CLK_TCK",		SYSCONF,   NOFLAGS,
#endif /* _SC_CLK_TCK */
	_SC_NGROUPS_MAX,	"NGROUPS_MAX",		SYSCONF,   NOFLAGS,
	_SC_OPEN_MAX,		"OPEN_MAX",		SYSCONF,   NOFLAGS,

#if _POSIX_VERSION == 198809L

	/*
	 * these symbols not defined in P1003.1-1988
	 * but P1003.2-1992 specifies them,
	 * so on .1-1988 systems we need to indicate that they
	 * are "undefined" here.
	 */

	0,			"STREAM_MAX",		SYSCONF,   UNDEFINED,
	0,			"TZNAME_MAX",		SYSCONF,   UNDEFINED,
#else
	_SC_STREAM_MAX,		"STREAM_MAX",		SYSCONF,   NOFLAGS,
	_SC_TZNAME_MAX,		"TZNAME_MAX",		SYSCONF,   NOFLAGS,

#endif /* _POSIX_VERSION == 198809L */

	_SC_JOB_CONTROL,	"_POSIX_JOB_CONTROL",	SYSCONF,   NOFLAGS,
	_SC_SAVED_IDS,		"_POSIX_SAVED_IDS",	SYSCONF,   NOFLAGS,
	_SC_VERSION,		"_POSIX_VERSION",	SYSCONF,   NOFLAGS,

	/* ISO/IEC 9945-1:1990 table 5-2 */

	_PC_LINK_MAX,		"LINK_MAX",		PATHCONF,  NOFLAGS,
	_PC_MAX_CANON,		"MAX_CANON",		PATHCONF,  NOFLAGS,
	_PC_MAX_INPUT,		"MAX_INPUT",		PATHCONF,  NOFLAGS,
	_PC_NAME_MAX,		"NAME_MAX",		PATHCONF,  NOFLAGS,
	_PC_PATH_MAX,		"PATH_MAX",		PATHCONF,  NOFLAGS,
	_PC_PIPE_BUF,		"PIPE_BUF",		PATHCONF,  NOFLAGS,
	_PC_CHOWN_RESTRICTED,	"_POSIX_CHOWN_RESTRICTED", PATHCONF,  NOFLAGS,
	_PC_NO_TRUNC,		"_POSIX_NO_TRUNC",	PATHCONF,  NOFLAGS,
	_PC_VDISABLE,		"_POSIX_VDISABLE",	PATHCONF,  NOFLAGS,

	/* Large File Summit name */

	_PC_FILESIZEBITS,	"FILESIZEBITS",		PATHCONF,  NOFLAGS,

	/* POSIX.2/D11.2 pg 443 - PATH to represent confstr(_CS_PATH) */

	_CS_PATH,		"PATH",			CONFSTR,   NOFLAGS,

	/* POSIX.2/D11.2 table B-18 */

	_CS_PATH,		"_CS_PATH",		CONFSTR,   NOFLAGS,

	/* VSC test suite considers CS_PATH valid system variable */

	_CS_PATH,		"CS_PATH",		CONFSTR,   NOFLAGS,

	/* command names for large file configuration information */
	/* large file compilation environment configuration */

	_CS_LFS_CFLAGS,		"LFS_CFLAGS",		CONFSTR,   NOFLAGS,
	_CS_LFS_LDFLAGS,	"LFS_LDFLAGS",		CONFSTR,   NOFLAGS,
	_CS_LFS_LIBS,		"LFS_LIBS",		CONFSTR,   NOFLAGS,
	_CS_LFS_LINTFLAGS,	"LFS_LINTFLAGS",	CONFSTR,   NOFLAGS,

	/* transitional large file interface configuration */

	_CS_LFS64_CFLAGS,	"LFS64_CFLAGS",		CONFSTR,   NOFLAGS,
	_CS_LFS64_LDFLAGS,	"LFS64_LDFLAGS",	CONFSTR,   NOFLAGS,
	_CS_LFS64_LIBS,		"LFS64_LIBS",		CONFSTR,   NOFLAGS,
	_CS_LFS64_LINTFLAGS,	"LFS64_LINTFLAGS",	CONFSTR,   NOFLAGS,

	/*
	**  UNIX98:
	**  confstr additions, these support
	**  programming specifications/environments.
	**
	** Unfortunately there is some uncertainty with
	** the standard at this point about the constant
	** name, thus we need to recognize BOTH with and
	** without the initial underscore.
	*/
	_CS_XBS5_ILP32_OFF32_CFLAGS,	"XBS5_ILP32_OFF32_CFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFF32_CFLAGS,	"_XBS5_ILP32_OFF32_CFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFF32_LDFLAGS,	"XBS5_ILP32_OFF32_LDFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFF32_LDFLAGS,	"_XBS5_ILP32_OFF32_LDFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFF32_LIBS,	"XBS5_ILP32_OFF32_LIBS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFF32_LIBS,	"_XBS5_ILP32_OFF32_LIBS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFF32_LINTFLAGS,	"XBS5_ILP32_OFF32_LINTFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFF32_LINTFLAGS,	"_XBS5_ILP32_OFF32_LINTFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFFBIG_CFLAGS,	"XBS5_ILP32_OFFBIG_CFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFFBIG_CFLAGS,	"_XBS5_ILP32_OFFBIG_CFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFFBIG_LDFLAGS,	"XBS5_ILP32_OFFBIG_LDFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFFBIG_LDFLAGS,	"_XBS5_ILP32_OFFBIG_LDFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFFBIG_LIBS,	"XBS5_ILP32_OFFBIG_LIBS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFFBIG_LIBS,	"_XBS5_ILP32_OFFBIG_LIBS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_ILP32_OFFBIG_LINTFLAGS, "XBS5_ILP32_OFFBIG_LINTFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_ILP32_OFFBIG_LINTFLAGS, "_XBS5_ILP32_OFFBIG_LINTFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LP64_OFF64_CFLAGS,	"XBS5_LP64_OFF64_CFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LP64_OFF64_CFLAGS,	"_XBS5_LP64_OFF64_CFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LP64_OFF64_LDFLAGS,	"XBS5_LP64_OFF64_LDFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LP64_OFF64_LDFLAGS,	"_XBS5_LP64_OFF64_LDFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LP64_OFF64_LIBS,	"XBS5_LP64_OFF64_LIBS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LP64_OFF64_LIBS,	"_XBS5_LP64_OFF64_LIBS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LP64_OFF64_LINTFLAGS,	"XBS5_LP64_OFF64_LINTFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LP64_OFF64_LINTFLAGS,	"_XBS5_LP64_OFF64_LINTFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LPBIG_OFFBIG_CFLAGS,	"XBS5_LPBIG_OFFBIG_CFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LPBIG_OFFBIG_CFLAGS,	"_XBS5_LPBIG_OFFBIG_CFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LPBIG_OFFBIG_LDFLAGS,	"XBS5_LPBIG_OFFBIG_LDFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LPBIG_OFFBIG_LDFLAGS,	"_XBS5_LPBIG_OFFBIG_LDFLAGS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LPBIG_OFFBIG_LIBS,	"XBS5_LPBIG_OFFBIG_LIBS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LPBIG_OFFBIG_LIBS,	"_XBS5_LPBIG_OFFBIG_LIBS",
		CONFSTR,	NOFLAGS,

	_CS_XBS5_LPBIG_OFFBIG_LINTFLAGS, "XBS5_LPBIG_OFFBIG_LINTFLAGS",
		CONFSTR,	NOFLAGS,
	_CS_XBS5_LPBIG_OFFBIG_LINTFLAGS, "_XBS5_LPBIG_OFFBIG_LINTFLAGS",
		CONFSTR,	NOFLAGS,


#ifdef  _SC_2_C_VERSION		/* POSIX.2 table B-4 */
	_SC_2_C_VERSION,	"_POSIX2_C_VERSION",	SYSCONF,   NOFLAGS,
#else
	_POSIX2_C_VERSION,	"_POSIX2_C_VERSION",	SELFCONF,   NOFLAGS,
#endif

/*
 * XPG4 support BEGINS
 */

#ifndef CHARCLASS_NAME_MAX
#define	CHARCLASS_NAME_MAX	-1
#endif
#ifndef CHAR_BIT
#define	CHAR_BIT	-1
#endif
#ifndef CHAR_MAX
#define	CHAR_MAX	-1
#endif
#ifndef CHAR_MIN
#define	CHAR_MIN	-1
#endif
#ifndef INT_MAX
#define	INT_MAX	-1
#endif
#ifndef INT_MIN
#define	INT_MIN	-1
#endif
#ifndef LONG_BIT
#define	LONG_BIT	-1
#endif
#ifndef LONG_MAX
#define	LONG_MAX	-1
#endif
#ifndef LONG_MIN
#define	LONG_MIN	-1
#endif
#ifndef MB_LEN_MAX
#define	MB_LEN_MAX	-1
#endif
#ifndef NL_NMAX
#define	NL_NMAX	-1
#endif
#ifndef NL_ARGMAX
#define	NL_ARGMAX	-1
#endif
#ifndef NL_LANGMAX
#define	NL_LANGMAX	-1
#endif
#ifndef NL_MSGMAX
#define	NL_MSGMAX	-1
#endif
#ifndef NL_SETMAX
#define	NL_SETMAX	-1
#endif
#ifndef NL_TEXTMAX
#define	NL_TEXTMAX	-1
#endif
#ifndef NZERO
#define	NZERO	-1
#endif
#ifndef SCHAR_MAX
#define	SCHAR_MAX	-1
#endif
#ifndef SCHAR_MIN
#define	SCHAR_MIN	-1
#endif
#ifndef SHRT_MAX
#define	SHRT_MAX	-1
#endif
#ifndef SHRT_MIN
#define	SHRT_MIN	-1
#endif
#ifndef TMP_MAX
#define	TMP_MAX	-1
#endif
#ifndef WORD_BIT
#define	WORD_BIT	-1
#endif


#ifndef	_XOPEN_XPG2
#define	_XOPEN_XPG2	-1
#endif
#ifndef	_XOPEN_XPG3
#define	_XOPEN_XPG3	-1
#endif
#ifndef	_XOPEN_XPG4
#define	_XOPEN_XPG4	-1
#endif

	/*
	 * the following are values that we should find in <limits.h>
	 * so we use SELFCONF here.
	 *
	 */
	CHARCLASS_NAME_MAX,	"CHARCLASS_NAME_MAX",	SELFCONF,   NOFLAGS,
	CHAR_BIT,		"CHAR_BIT",	SELFCONF,   NOFLAGS,
	CHAR_MAX,		"CHAR_MAX",	SELFCONF,   NOFLAGS,
	CHAR_MIN,		"CHAR_MIN",	SELFCONF,   NOFLAGS,
	INT_MAX,		"INT_MAX",	SELFCONF,   NOFLAGS,
	INT_MIN,		"INT_MIN",	SELFCONF,   NOFLAGS,
	LONG_BIT,		"LONG_BIT",	SELFCONF,   NOFLAGS,
	LONG_MAX,		"LONG_MAX",	SELFCONF,   NOFLAGS,
	LONG_MIN,		"LONG_MIN",	SELFCONF,   NOFLAGS,
	MB_LEN_MAX,		"MB_LEN_MAX",	SELFCONF,   NOFLAGS,
	NL_NMAX,		"NL_NMAX",	SELFCONF,   NOFLAGS,
	NL_ARGMAX,		"NL_ARGMAX",	SELFCONF,   NOFLAGS,
	NL_LANGMAX,		"NL_LANGMAX",	SELFCONF,   NOFLAGS,
	NL_MSGMAX,		"NL_MSGMAX",	SELFCONF,   NOFLAGS,
	NL_SETMAX,		"NL_SETMAX",	SELFCONF,   NOFLAGS,
	NL_TEXTMAX,		"NL_TEXTMAX",	SELFCONF,   NOFLAGS,
	NZERO,			"NZERO",	SELFCONF,   NOFLAGS,
	SCHAR_MAX,		"SCHAR_MAX",	SELFCONF,   NOFLAGS,
	SCHAR_MIN,		"SCHAR_MIN",	SELFCONF,   NOFLAGS,
	SHRT_MAX,		"SHRT_MAX",	SELFCONF,   NOFLAGS,
	SHRT_MIN,		"SHRT_MIN",	SELFCONF,   NOFLAGS,
	_POSIX_SSIZE_MAX,	"SSIZE_MAX",	SELFCONF,   NOFLAGS,
	TMP_MAX,		"TMP_MAX",	SELFCONF,   NOFLAGS,

	/*
	 * for the unsigned maximums, we cannot rely on the value -1
	 * to indicate "undefined".
	 */
#ifndef UCHAR_MAX
	0,			"UCHAR_MAX",	SELFCONF,   UNDEFINED,
#else
	(long)UCHAR_MAX,	"UCHAR_MAX",	SELFCONF,   UNSIGNED_VALUE,
#endif /* UCHAR_MAX */
#ifndef UINT_MAX
	0,			"UINT_MAX",	SELFCONF,   UNDEFINED,
#else
	(long)UINT_MAX,		"UINT_MAX",	SELFCONF,   UNSIGNED_VALUE,
#endif /* UINT_MAX */
#ifndef ULONG_MAX
	0,			"ULONG_MAX",	SELFCONF,   UNDEFINED,
#else
	(long)ULONG_MAX,	"ULONG_MAX",	SELFCONF,   UNSIGNED_VALUE,
#endif /* ULONG_MAX */
#ifndef USHRT_MAX
	0,			"USHRT_MAX",	SELFCONF,   UNDEFINED,
#else
	(long)USHRT_MAX,	"USHRT_MAX",	SELFCONF,   UNSIGNED_VALUE,
#endif /* USHRT_MAX */
#ifndef SIZE_MAX
	0,			"SIZE_MAX",	SELFCONF,   UNDEFINED,
#else
	(long)SIZE_MAX,		"SIZE_MAX",	SELFCONF,   UNSIGNED_VALUE,
#endif /* SIZE_MAX */

	WORD_BIT,		"WORD_BIT",	SELFCONF,   NOFLAGS,

	/*
	 *   The following are defined via sysconf().  These are considered
	 *   an extension to sysconf().
	 */
	_XOPEN_XPG2-0,		"_XOPEN_XPG2",	SELFCONF,    NOFLAGS,
	_XOPEN_XPG3-0,		"_XOPEN_XPG3",	SELFCONF,    NOFLAGS,
	_XOPEN_XPG4-0,		"_XOPEN_XPG4",	SELFCONF,    NOFLAGS,

	/*
	 * The following should be provided by sysconf() (e.g use SYSCONF),
	 * so we  look for the appropriate _SC_* symbol in <unistd.h>.
	 * If it is not defined, then we use SELFCONF with the value of -1.
	 */
#ifdef _SC_XOPEN_VERSION
	_SC_XOPEN_VERSION,	"_XOPEN_VERSION",	SYSCONF,   NOFLAGS,
#else
	-1,			"_XOPEN_VERSION",	SELFCONF,   UNDEFINED,
#endif /* _SC_XOPEN_VERSION */
#ifdef _SC_XOPEN_XCU_VERSION
	_SC_XOPEN_XCU_VERSION,	"_XOPEN_XCU_VERSION",	SYSCONF,   NOFLAGS,
#else
	-1,			"_XOPEN_XCU_VERSION",	SELFCONF,   UNDEFINED,
#endif /* _SC_XOPEN_XCU_VERSION */
#ifdef _SC_XOPEN_CRYPT
	_SC_XOPEN_CRYPT,	"_XOPEN_CRYPT",		SYSCONF,   NOFLAGS,
#else
	-1,			"_XOPEN_CRYPT",		SELFCONF,   UNDEFINED,
#endif /* _SC_XOPEN_CRYPT */
#ifdef _SC_XOPEN_ENH_I18N
	_SC_XOPEN_ENH_I18N,	"_XOPEN_ENH_I18N",	SYSCONF,   NOFLAGS,
#else
	-1,			"_XOPEN_ENH_I18N",	SELFCONF,   UNDEFINED,
#endif /* _SC_XOPEN_ENH_I18N */
#ifdef _SC_XOPEN_SHM
	_SC_XOPEN_SHM,		"_XOPEN_SHM",		SYSCONF,   NOFLAGS,
#else
	-1,			"_XOPEN_SHM",		SELFCONF,   UNDEFINED,
#endif /* _SC_XOPEN_SHM */

/*
 * XPG4 support ends
 */

/* XPG4, Version 2 support */

	_SC_ATEXIT_MAX, 	"ATEXIT_MAX",		SYSCONF,	NOFLAGS,
	_SC_PAGESIZE,		"PAGESIZE",		SYSCONF,	NOFLAGS,
	_SC_PAGE_SIZE,		"PAGE_SIZE",		SYSCONF,	NOFLAGS,
	_SC_XOPEN_UNIX,		"_XOPEN_UNIX",		SYSCONF,	NOFLAGS,
	_SC_IOV_MAX,		"IOV_MAX",		SYSCONF, 	NOFLAGS,

/*
** UNIX98 - XPG5
** additions
**
** As noted above, due to ambiguity in the standard
** we need to recognize both the underscored name
** and the non-underscored version. This could
** be corrected down the line...
*/
	_SC_XOPEN_LEGACY,	"_XOPEN_LEGACY",	SYSCONF, 	NOFLAGS,

	_SC_XBS5_ILP32_OFF32,	"XBS5_ILP32_OFF32",	SYSCONF, 	NOFLAGS,
	_SC_XBS5_ILP32_OFF32,	"_XBS5_ILP32_OFF32",	SYSCONF, 	NOFLAGS,

	_SC_XBS5_ILP32_OFFBIG,	"XBS5_ILP32_OFFBIG",	SYSCONF, 	NOFLAGS,
	_SC_XBS5_ILP32_OFFBIG,	"_XBS5_ILP32_OFFBIG",	SYSCONF, 	NOFLAGS,

	_SC_XBS5_LP64_OFF64,	"XBS5_LP64_OFF64",	SYSCONF, 	NOFLAGS,
	_SC_XBS5_LP64_OFF64,	"_XBS5_LP64_OFF64",	SYSCONF, 	NOFLAGS,

	_SC_XBS5_LPBIG_OFFBIG,	"XBS5_LPBIG_OFFBIG",	SYSCONF, 	NOFLAGS,
	_SC_XBS5_LPBIG_OFFBIG,	"_XBS5_LPBIG_OFFBIG",	SYSCONF, 	NOFLAGS,
	0,		NULL,	0,	0		/* end of table */
};


/*
 * Print usage message.
 */
static int
usage()
{

	(void) fputs(gettext(
		"usage:\tgetconf [-v specification] system_var\n"
		"\tgetconf [-v specification] path_var pathname\n"
		"\tgetconf -a\n"), stderr);
	return (2);
}


static int
namecmp(const void *a, const void *b)
{
	return (strcoll(((const struct sctab *)a)->name,
	    ((const struct sctab *)b)->name));
}


static int
getconf(struct sctab *scp, int argc, char *name, char *file)
{
	register size_t len;
	register char *buffer;
	long value;

	switch (scp->type) {
	case SELFCONF:
		if (argc > 2)
			return (usage());
		if (scp->flag & UNDEFINED ||
		    (!(scp->flag&UNSIGNED_VALUE) && scp->value == -1)) {
			/* DO NOT TRANSLATE */
			(void) printf("undefined\n");
		} else
			if (scp->flag & UNSIGNED_VALUE)
				(void) printf("%lu\n", scp->value);
			else
				(void) printf("%ld\n", scp->value);
		break;

	case SYSCONF:
		if (argc > 2)
			return (usage());
		errno = 0;
		if (scp->flag & UNDEFINED ||
		    (value = sysconf((int)scp->value)) == -1) {
			if (errno == EINVAL) {
				(void) fprintf(stderr,
				    gettext(INVAL_ARG), name);
				return (1);
			} else {
				/* DO NOT TRANSLATE */
				(void) printf("undefined\n");
				return (0);
			}
		} else
			(void) printf("%ld\n", value);
		break;

	case CONFSTR:
		if (argc > 2)
			return (usage());
		errno = 0;
		len = confstr((int)scp->value, NULL, (size_t)0);
		if (len == 0)
			if (errno == EINVAL) {
				(void) fprintf(stderr, gettext(INVAL_ARG),
				    name);
				return (1);
			} else {
				/* DO NOT TRANSLATE */
				(void) printf("undefined\n");
				return (0);
			}
		/*
		 * allocate space to store result of constr() into
		 */
		if ((buffer = malloc(len)) == NULL) {
			(void) fprintf(stderr,
			    gettext("insufficient memory available"));
			return (1);
		}

		assert(confstr((int)scp->value, buffer, len) != 0);

		(void) printf("%s\n", buffer);
		free(buffer);
		break;

	case PATHCONF:
		if (argc != 3)
			return (usage());
		errno = 0;
		if ((value = pathconf(file, (int)scp->value)) == -1) {
			if (errno == EINVAL) {
				/* Invalid pathconf variable */
				if (aflag) {
					(void) printf(gettext(INVAL_PATHARG),
						name, file);
					(void) printf("\n");
				} else {
					(void) fprintf(stderr,
					    gettext(INVAL_PATHARG), name, file);
				}
				return (1);
			} else if (errno != 0) {
				/* Probably problems with the pathname itself */
				if (aflag) {
					(void) printf("%s\n", file);
				} else {
					(void) printf("%s", file);
				}
				return (1);
			} else {
				/* Does not support the association */
				/* DO NOT TRANSLATE */
				(void) printf("undefined\n");
				return (0);
			}
		}
		(void) printf("%ld\n", value);
		break;
	}
	return (0);
}

int
main(int argc, char **argv)
{
	register struct sctab *scp;
	int c;
	int exstat = 0;

	(void) setlocale(LC_ALL, "");
#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "av:")) != -1)
		switch (c) {
		case 'a':
			aflag = 1;
			break;
		case 'v':
			/*
			** Unix98 adds the -v option to allow
			** programming 'specifications' to be
			** indicated, for present purposes
			** the specification isn't really
			** doing anything of value, so for
			** the moment getopt just processes the
			** option value and argv[1] is adjusted.
			**
			** At some later date we might want to
			** do specification processing at this
			** point.
			*/
			break;
		case '?':
			return (usage());
		}
	argc -= optind-1;
	argv += optind-1;

	if ((aflag && argc >= 2) || (!aflag && argc < 2))
		return (usage());
	if (aflag) {

#define	TabStop		8
#define	RightColumn	32
#define	DefPathName	"."

		/*
		 * sort the table by the "name" field
		 * so we print it in sorted order
		 */
	qsort(&sctab[0], (sizeof (sctab)/sizeof (struct sctab))-1,
	    sizeof (struct sctab), namecmp);

		/*
		 * print all the known symbols and their values
		 */
		for (scp = &sctab[0]; scp->name != NULL; ++scp) {
			int stat;

			/*
			 * create 2 columns:
			 *   variable name in the left column,
			 *   value in the right column.
			 * The right column starts at a tab stop.
			 */
			(void) printf("%s:\t", scp->name);

			c = strlen(scp->name) + 1;
			c = (c+TabStop) / TabStop, c *= TabStop;
			for (; c < RightColumn; c += TabStop)
				(void) putchar('\t');

			/*
			 * for pathconf() related variables that require
			 * a pathname, use "."
			 */
			stat = getconf(scp, scp->type == PATHCONF ? 3 : 2,
			    scp->name, DefPathName);

			exstat |= stat;

			/*
			 * if stat != 0, then an error message was already
			 * printed in getconf(),
			 * so don't need to print one here
			 */
		}
	} else {

		/*
		 * find the name of the specified variable (argv[1])
		 * and print its value.
		 */
		for (scp = &sctab[0]; scp->name != NULL; ++scp)
			if (strcmp(argv[1], scp->name) == 0) {
				exstat = getconf(scp, argc, argv[1], argv[2]);
				break;
			}

		/*
		 * if at last entry in table, then the user specified
		 * variable is invalid
		 */
		if (scp->name == NULL) {
			errno = EINVAL;
			(void) fprintf(stderr, gettext(INVAL_ARG), argv[1]);
			return (1);
		}
	}
	return (exstat);
}
