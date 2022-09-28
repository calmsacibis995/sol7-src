/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)localedef.c 1.19	97/11/19  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: localedef.c,v $ $Revision: 1.5.6.3 $"
	" (OSF) $Date: 1992/09/14 15:20:09 $";
#endif
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS:
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.8  com/cmd/nls/localedef.c, cmdnls, bos320, 9130320 7/17/91 17:39:41
 */

#include <stdio.h>
#include "err.h"
#include <sys/types.h>
#include <fcntl.h>
#include "method.h"
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/localedef.h>

static char	*tpath;
static char	*ccopts;
static char	*ldopts;

int	Charmap_pass;

extern library_t lib_array[];
	/* Array of libraries required for extensible methods */

char *yyfilenm;			/* global file name pointer */

int warn = FALSE;		/* Suppress warnings flag */

int	lp64p = FALSE;		/* TRUE if generating 64-bit obj */

extern int	Euc_filecode;
extern int	user_specified_libc;	/* from sem_method.c */

#define	CCPATH  "cc"
#define	CCFLAGS_COM	" -v -K PIC -DPIC -G -z defs -D_REENTRANT"
#define	CCFLAGS_SPARC	" -xO3 -xcg89 -Wa,-cg92 -xregs=no%appl"
#define	CCFLAGS_SPARCV9	" -xO3 -xarch=v9 -dalign -xregs=no%appl"
#define	CCFLAGS_I386	" -O"
#if defined(__sparc)
#define	CCFLAGS		CCFLAGS_COM CCFLAGS_SPARC
#define	CCFLAGS64	CCFLAGS_COM CCFLAGS_SPARCV9
#else
#define	CCFLAGS		CCFLAGS_COM CCFLAGS_I386
#define	CCFLAGS64	CCFLAGS_COM CCFLAGS_I386
#endif

const char *options = {
#ifdef DEBUG
			"scdvwx:f:i:C:L:P:"
#else /* DEBUG */
			"cvwx:f:i:C:L:P:m:W:"
#endif /* DEBUG */
};

extern void	initlex(void);
extern void	initparse(void);
extern void	check_methods(void);
extern void	initgram(void);
extern void	init_symbol_tbl(int);
extern void	define_all_wchars(void);
extern void	gen(FILE *);
extern int	yyparse(void);

char *
get_tpath(char *tpath)
{
	size_t	len;
	char	*s;

	if (tpath == NULL)
		return ("");

	len = strlen(tpath);
	if (*(tpath + len - 1) == '/')
		return (tpath);

	s = MALLOC(char, len + 2);
	(void) strcpy(s, tpath);
	(void) strcat(s, "/");
	return (s);
}

char *
get_ccarg(char *optstr)
{
#define	ARGSTR	"cc,"

	char	*s, *ms, *cs, *tmps;
	int	n, len;

	s = optstr;
	n = strlen(ARGSTR);

	/* check if the first 3 characters are "cc," */
	if (strncmp(s, ARGSTR, n) != 0) {
		return (NULL);			/* error */
	}
	s += n;
	if (!*s) {
		return (NULL);			/* no argument */
	}

	len = strlen(s);
	ms = MALLOC(char, len + 1);
	tmps = ms;
	while ((cs = strchr(s, ',')) != NULL) {
		if (cs == s) {
			s++;
			continue;
		}
		if (*(cs - 1) == '\\') {
			if (cs == (s + 1)) {
				*tmps++ = ',';
				s += 2;
			} else {
				len = cs - s - 1;
				(void) strncpy(tmps, s, len);
				tmps += len;
				*tmps++ = ',';
				s = cs + 1;
			}
		} else {
			len = cs - s;
			(void) strncpy(tmps, s, len);
			tmps += len;
			*tmps++ = ' ';
			s = cs + 1;
		}
	}
	len = strlen(s);
	(void) strncpy(tmps, s, len);
	tmps += len;
	*tmps = '\0';
	return (ms);
}

void
initparse(void)
{
	initlex();
	initgram();
}

void
main(int argc, char *argv[])
{
#ifdef DEBUG
	extern int yylineno, yycharno;
#endif
	extern int err_flag;
#ifdef DEBUG
	extern symtab_t cm_symtab;
#endif /* DEBUG */
	extern int yydebug;
	extern int optind;
	extern char *optarg;
	extern int	Euc_filecode;

	int	c;
	int	verbose;
	int	force;
#ifdef DEBUG
	int	sym_dump;
#endif
	int	fd;
	FILE	*fp;
	char	*cmdstr;
	char	*locname;
	char	*methsrc;
	char	*cmapsrc;
	char 	*locsrc;
	char	*tmpfilenm;
	char	*s;

	size_t	cmdlen;
	int	i;

	yydebug = verbose = force = FALSE;
#ifdef DEBUG
	sym_dump = FALSE;
#endif
	ldopts = ccopts = tpath = cmapsrc = locsrc = methsrc  = NULL;

	while ((c = getopt(argc, argv, (char *)options)) != -1)
		switch (c) {
#ifdef DEBUG
		case 's':
			sym_dump = TRUE;
			break;
		case 'd':	/* parser debug */
			yydebug = TRUE;
			break;
#endif /* DEBUG */

		case 'w':	/* display duplicate definition warnings */
			warn = TRUE;
			break;

		case 'c':	/* generate a locale even if warnings */
			force = TRUE;
			break;

		case 'v':	/* verbose table dump */
			verbose = TRUE;
			break;

		case 'x':	/* specify method file name */
			methsrc = optarg;
			break;

		case 'f':	/* specify charmap file name */
			cmapsrc = optarg;
			break;

		case 'i':	/* specify locale source file */
			locsrc = optarg;
			break;

		case 'P':	/* tool path */
			tpath = optarg;
			break;

		case 'C':	/* special compiler options */
			ccopts = optarg;
			break;

		case 'L':	/* special linker options */
			ldopts = optarg;
			break;

		case 'm':	/* -m lp64 or -m ilp32 */
			if (strcmp(optarg, "lp64") == 0) {
				lp64p = TRUE;
			} else if (strcmp(optarg, "ilp32") == 0) {
				lp64p = FALSE;
			} else {
				usage(4);	/* Never returns */
			}
			break;

		case 'W':	/* -W cc,arg: cc opts */
			if ((ccopts = get_ccarg(optarg)) == NULL) {
				usage(4);
			}
			break;

		default:	/* Bad option or invalid flags */
			usage(4);		/* Never returns! */
		}

	if (optind < argc) {
		/*
		 * Create the locale name
		 */
		locname = MALLOC(char, strlen(argv[optind]) + 1);
		strcpy(locname, argv[optind]);
	} else {
		usage(4);			/* NEVER returns */
	}

	/*
	 * seed symbol table with default values for mb_cur_max, mb_cur_min,
	 * and codeset
	 */
	if (cmapsrc != NULL)
		init_symbol_tbl(FALSE);	/* don't seed the symbol table */
	else
		init_symbol_tbl(TRUE);
			/* seed the symbol table with POSIX PCS */

	/* if there is a method source file, process it */

	if (methsrc != NULL) {
		int fd0;		/* file descriptor to save stdin */
		fd0 = dup(0);	/* dup current stdin */
		close(0);		/* close stdin, i.e. fd 0 */
		yyfilenm = methsrc;	/* set filename begin parsed for */
				/* error reporting.  */
		fd = open(methsrc, O_RDONLY);
		if (fd < 0)
			error(ERR_OPEN_READ, methsrc);

		initparse();
		yyparse();		/* parse the methods file */

		close(fd);

		/* restore stdin */
		close(0);		/* close methods file */
		dup(fd0);		/* restore saved descriptor to 0 */
	}

	/* process charmap if present */
	if (cmapsrc != NULL) {
		int fd0;		/* file descriptor to save stdin */

		fd0 = dup(0);	/* dup current stdin */
		close(0);		/* close stdin, i.e. fd 0 */

		if (Euc_filecode == TRUE) {		/* first charmap pass */
			/* only do if we are euc filecode */
			yyfilenm = cmapsrc;
				/* set filename begin parsed for */
				/* error reporting.  */
			fd = open(cmapsrc, O_RDONLY);	/* new open gets fd 0 */
			if (fd != 0)
				error(ERR_OPEN_READ, cmapsrc);

			initparse();
			Charmap_pass = 1;
			yyparse();			/* parse charmap file */

			close(0);			/* close charmap file */
		}

		/* second charmap pass */

		yyfilenm = cmapsrc;	/* set filename begin parsed for */
			/* error reporting.  */
		fd = open(cmapsrc, O_RDONLY);    /* new open gets fd 0 */
		if (fd != 0)
			error(ERR_OPEN_READ, cmapsrc);

		initparse();
		Charmap_pass = 2;
		yyparse();		/* parse charmap file */

		/* restore stdin */
		close(0);		/* close charmap file */
		dup(fd0);		/* restore saved descriptor to 0 */
	} else
		define_all_wchars();	/* Act like all code points legal */


	/*
	 * process locale source file.  if locsrc specified use it,
	 * otherwise process input from standard input
	 */

	check_methods();
		/*
		 * if no extension file and no charmap then we
		 * need this.
		 */
	{
		int fd0 = 0;

		if (locsrc != NULL) {
			fd0 = dup(0);
			close(0);
			yyfilenm = locsrc;
				/* set file name being parsed for */
				/* error reporting. */
			fd = open(locsrc, O_RDONLY);
			if (fd != 0)
				error(ERR_OPEN_READ, locsrc);
		} else {
			yyfilenm = "stdin";
		}

		initparse();
		yyparse();

		if (fd0 != 0) {
			close(0);
			dup(fd0);
		}
	}

#ifdef DEBUG
	if (sym_dump) {
		/* dump symbol table statistics */
		int	i, j;
		symbol_t *p;

		for (i = 0; i < HASH_TBL_SIZE; i++) {
			j = 0;
			for (p = &(cm_symtab.symbols[i]);
				p->next != NULL; p = p->next)
				j = j + 1;
			printf("bucket #%d - %d\n", i, j);
		}
	}
#endif /* DEBUG */

	if (!force && err_flag)	/* Errors or Warnings without -c present */
		exit(4);

	/* Open temporary file for locale source.  */
	s = tempnam("./", "locale");
	tmpfilenm = MALLOC(char, strlen(s) + 3); /* Space for ".[co]\0" */
	strcpy(tmpfilenm, s);
	strcat(tmpfilenm, ".c");
	fp = fopen(tmpfilenm, "w");
	if (fp == NULL) {
		error(ERR_WRT_PERM, tmpfilenm);
	}

	/* generate the C code which implements the locale */
	gen(fp);
	fclose(fp);

	/* check and initialize if necessary */
	/* linker/compiler opts and tool paths. */
	if (ldopts == NULL)
		if (user_specified_libc == FALSE)
			ldopts = " -lc";
		else
			ldopts = "";

	tpath = get_tpath(tpath);

	if (ccopts == NULL) {
		if (lp64p == TRUE) {
			ccopts = CCFLAGS64;
		} else {
			ccopts = CCFLAGS;
		}
	}

	/* compile the C file created */

	cmdlen = strlen(tpath) + strlen(CCPATH) + strlen(ccopts) +
		strlen(locname) + 10 + 7 +
		4 + strlen(locname) + 10 +
		strlen(tmpfilenm) + 2 +
		10 + strlen(ldopts);

	for (i = 0; i <= LAST_METHOD; i++) {
		if (lp64p) {
			if (!lib_array[i].library64)
				break;	/* No more 64-bit libraries */
			cmdlen += strlen(lib_array[i].library64) + 1;
		} else {
			if (!lib_array[i].library)
				break;	/* No more libraries */
			cmdlen += strlen(lib_array[i].library) + 1;
		}
	}

	cmdstr = MALLOC(char, cmdlen + 1); /* Space for trailing NUL */

	s = cmdstr + sprintf(cmdstr, "%s" CCPATH " %s " "-h %s.so.%d "
		"-o %s.so.%d" " %s",
		tpath, ccopts, locname, _LC_VERSION_MAJOR,
		locname, _LC_VERSION_MAJOR, tmpfilenm);

	for (i = 0; i <= LAST_METHOD; i++) {
		if (lp64p) {
			if (!lib_array[i].library64)
				break;
			s = s + sprintf(s, " %s", lib_array[i]. library64);
		} else {
			if (!lib_array[i].library)
				break;
			s = s + sprintf(s, " %s", lib_array[i]. library);
		}
	}

	(void) sprintf(s, " %s", ldopts);

	if (verbose) printf("%s\n", cmdstr);
	c = system(cmdstr);
	free(cmdstr);

	/* delete the C file after compiling */
	if (!verbose)
		unlink(tmpfilenm);
	else {
		/* rename to localename.c */
		char *s;

		s = MALLOC(char, strlen(locname) + 3);
		strcpy(s, locname);
		strcat(s, ".c");
		rename(tmpfilenm, s);
		free(s);
	}

	if (WIFEXITED(c))
		switch (WEXITSTATUS(c)) {
		case 0:	break;			/* Successful compilation */

		case -1:	perror("localedef");	/* system() problems? */
			exit(4);
			/* NOTREACHED */
		case 127:	error(ERR_NOSHELL);
			/* cannot exec /usr/bin/sh */
			/* NOTREACHED */
		default:	error(ERR_BAD_CHDR);	/* take a guess.. */
		}
	else
		error(ERR_INTERNAL, tmpfilenm, 0);

	/*
	 * create a text file with 'locname' to keep VSC happy
	 */
	cmdlen = strlen(locname);
	cmdlen += 4 + 4;	/* .so. + VERSION */
	cmdstr = (char *) malloc(cmdlen);
	(void) sprintf(cmdstr, "%s.so.%d", locname, _LC_VERSION_MAJOR);
	fp = fopen(locname, "w");
	if (fp == NULL) {
		error(ERR_WRT_PERM, locname);
	}
	(void) fprintf(fp,
"The locale shared object was created with the name: %s\n",
		cmdstr);
	(void) fclose(fp);
	exit(err_flag != 0);	/* 1=>created with warnings */
				/* 0=>no problems */
}
