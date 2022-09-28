/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
/*
 *	Copyright (c) 1992-1996 Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)getmntent.c	1.20	96/11/14 SMI"	/* SVr4.0 1.4	*/

/*LINTLIBRARY*/
#pragma weak getmntany = _getmntany
#pragma weak getmntent = _getmntent
#pragma weak hasmntopt = _hasmntopt

#include	"synonyms.h"
#include	<mtlib.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/mnttab.h>
#include 	<string.h>
#include 	<ctype.h>
#include 	<errno.h>
#include	<stdlib.h>
#include	<thread.h>
#include	<synch.h>
#include	<thr_int.h>
#include	<libc.h>

static int	getline(char *, FILE *);

#define	GETTOK(xx, ll)\
	if ((mp->xx = strtok(ll, sepstr)) == NULL)\
		return (MNT_TOOFEW);\
	if (strcmp(mp->xx, dash) == 0)\
		mp->xx = NULL
#define	GETTOK_R(xx, ll, tmp)\
	if ((mp->xx = (char *)strtok_r(ll, sepstr, tmp)) == NULL)\
		return (MNT_TOOFEW);\
	if (strcmp(mp->xx, dash) == 0)\
		mp->xx = NULL
#define	DIFF(xx)\
	(mrefp->xx != NULL && (mgetp->xx == NULL ||\
	    strcmp(mrefp->xx, mgetp->xx) != 0))
#define	SDIFF(xx, typem, typer)\
	((mgetp->xx == NULL) || (stat64(mgetp->xx, &statb) == -1) ||\
	((statb.st_mode & S_IFMT) != typem) ||\
	    (statb.st_rdev != typer))

static char	*statline = NULL;
static const char	sepstr[] = " \t\n";
static const char	dash[] = "-";

int
getmntany(FILE *fd, struct mnttab *mgetp, struct mnttab *mrefp)
{
	int	ret, bstat;
	mode_t	bmode;
	dev_t	brdev;
	struct stat64	statb;

	if (mrefp->mnt_special && stat64(mrefp->mnt_special, &statb) == 0 &&
	    ((bmode = (statb.st_mode & S_IFMT)) == S_IFBLK ||
	    bmode == S_IFCHR)) {
		bstat = 1;
		brdev = statb.st_rdev;
	} else
		bstat = 0;

	while ((ret = getmntent(fd, mgetp)) == 0 &&
		((bstat == 0 && DIFF(mnt_special)) ||
		(bstat == 1 && SDIFF(mnt_special, bmode, brdev)) ||
		DIFF(mnt_mountp) ||
		DIFF(mnt_fstype) ||
		DIFF(mnt_mntopts) ||
		DIFF(mnt_time)))
		;

	return (ret);
}

int
getmntent(FILE *fd, struct mnttab *mp)
{
	int	ret;
	static	thread_key_t	mnt_key = 0;
	char	*tmp, *line;

	if (_thr_main()) {
		if (statline == NULL)
			statline = (char *)malloc(MNT_LINE_MAX);
		line = statline;
	} else {
		line = (char *)_tsdalloc(&mnt_key, MNT_LINE_MAX);
	}

	if (line == 0) {
		errno = ENOMEM;
		return (0);
	}

	/* skip leading spaces and comments */
	if ((ret = getline(line, fd)) != 0)
		return (ret);

	/* split up each field */
	GETTOK_R(mnt_special, line, &tmp);
	GETTOK_R(mnt_mountp, NULL, &tmp);
	GETTOK_R(mnt_fstype, NULL, &tmp);
	GETTOK_R(mnt_mntopts, NULL, &tmp);
	GETTOK_R(mnt_time, NULL, &tmp);

	/* check for too many fields */
	if ((char *)strtok_r(NULL, sepstr, &tmp) != NULL)
		return (MNT_TOOMANY);

	return (0);
}

static int
getline(char *lp, FILE *fd)
{
	char	*cp;

	while ((lp = fgets(lp, MNT_LINE_MAX, fd)) != NULL) {
		if (strlen(lp) == MNT_LINE_MAX-1 && lp[MNT_LINE_MAX-2] != '\n')
			return (MNT_TOOLONG);

		for (cp = lp; *cp == ' ' || *cp == '\t'; cp++)
			;

		if (*cp != '#' && *cp != '\n')
			return (0);
	}
	return (-1);
}

char *
mntopt(char **p)
{
	char *cp = *p;
	char *retstr;

	while (*cp && isspace(*cp))
		cp++;

	retstr = cp;
	while (*cp && *cp != ',')
		cp++;

	if (*cp) {
		*cp = '\0';
		cp++;
	}

	*p = cp;
	return (retstr);
}

char *
hasmntopt(struct mnttab *mnt, char *opt)
{
	char tmpopts[256];
	char *f, *opts = tmpopts;

	(void) strcpy(opts, mnt->mnt_mntopts);
	f = mntopt(&opts);
	for (; *f; f = mntopt(&opts)) {
		if (strncmp(opt, f, strlen(opt)) == 0)
			return (f - tmpopts + mnt->mnt_mntopts);
	}
	return (NULL);
}
