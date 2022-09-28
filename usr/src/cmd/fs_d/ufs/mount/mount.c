/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989,1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#ident	"@(#)mount.c	1.49	97/12/15 SMI"	/* SVr4.0 1.15 */

/*
 * mount
 */
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>
#include <stdlib.h>

#define	bcopy(f, t, n)	memcpy(t, f, n)
#define	bzero(s, n)	memset(s, 0, n)
#define	bcmp(s, d, n)	memcmp(s, d, n)

#define	index(s, r)	strchr(s, r)
#define	rindex(s, r)	strrchr(s, r)

#include <sys/errno.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
#include <sys/vfstab.h>
#include <sys/filio.h>

#include <sys/fs/ufs_mount.h>
#include <sys/fs/ufs_filio.h>

#include <locale.h>
#include <fslib.h>

static int	ro = 0;
static int	fake = 0;
static int	nosuid = 0;
static int	largefiles = 0; /* flag - add default nolargefiles to mnttab */

static int	mflg = 0;
static int 	Oflg = 0;

extern int	optind;
extern char	*optarg;

#define	NAME_MAX	64		/* sizeof "fstype myname" */

#define	MNTOPT_DEV	"dev"

extern int errno;

static int	checkislog(char *);
static void	disable_logging(char *, char *);
static int	eatmntopt(struct mnttab *, char *);
static void	enable_logging(char *, char *);
static void	fixopts(struct mnttab *, char *, int);
static void	mountfs(struct mnttab *);
static void	replace_opts(char *, int, char *, char *);
static void	rmopt(struct mnttab *, char *);
static void	rpterr(char *, char *);
static void	usage(void);

static char	fstype[] = MNTTYPE_UFS;
static char	opts[MNTMAXSTR];
static char	typename[NAME_MAX], *myname;
static char	*fop_subopts[] = { MNTOPT_ONERROR, MNTOPT_TOOSOON, NULL };
#define	NOMATCH	(-1)
#define	ONERROR	(0)		/* index within fop_subopts */
#define	TOOSOON	(1)		/* index within fop_subopts */

static struct fop_subopt {
	char	*str;
	int	 flag;
} fop_subopt_list[] = {
	{ UFSMNT_ONERROR_REPAIR_STR,	UFSMNT_ONERROR_REPAIR	},
	{ UFSMNT_ONERROR_PANIC_STR,	UFSMNT_ONERROR_PANIC	},
	{ UFSMNT_ONERROR_LOCK_STR,	UFSMNT_ONERROR_LOCK	},
	{ UFSMNT_ONERROR_UMOUNT_STR,	UFSMNT_ONERROR_UMOUNT	},
	{ UFSMNT_ONERROR_RDONLY_STR,	UFSMNT_ONERROR_RDONLY	},
	{ NULL,				UFSMNT_ONERROR_DEFAULT	}
};

static struct units_conv {
	char	*str;
	long	 normalize_to_secs;	/* multiplicative factor */
} units_list[] = {
	{"s",	1},
	{"m",	60},
	{"h",	60*60},
	{"d",	24*60*60},
	{"w",	7*24*60*60},
	{"y",	52*7*24*60*60},
	{NULL,	0}
};

/*
 * Find opt in mntopt
 */
static char *
findopt(char *mntopt, char *opt)
{
	int nc, optlen = strlen(opt);

	while (*mntopt) {
		nc = strcspn(mntopt, ", =");
		if (strncmp(mntopt, opt, nc) == 0)
			if (optlen == nc)
				return (mntopt);
		mntopt += nc;
		mntopt += strspn(mntopt, ", =");
	}
	return (NULL);
}

void
main(int argc, char *argv[])
{
	struct mnttab mnt;
	int	c;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	myname = strrchr(argv[0], '/');
	if (myname)
		myname++;
	else
		myname = argv[0];
	(void) sprintf(typename, "%s %s", fstype, myname);
	argv[0] = typename;

	opts[0] = '\0';

	/*
	 * Set options
	 */
	while ((c = getopt(argc, argv, "mo:prVO")) != EOF) {
		switch (c) {

		case 'o':
			(void) strcpy(opts, optarg);
			break;
		case 'O':
			Oflg++;
			break;

		case 'r':
			ro++;
			break;

		case 'm':
			mflg++;
			break;

		default:
			usage();
		}
	}

	if (geteuid() != 0) {
		(void) fprintf(stderr, gettext("Must be root to use mount\n"));
		exit(32);
	}

	if ((argc - optind) != 2)
		usage();

	mnt.mnt_special = argv[optind];
	mnt.mnt_mountp = argv[optind+1];
	mnt.mnt_fstype = fstype;
	mnt.mnt_mntopts = opts;
	if (findopt(mnt.mnt_mntopts, "f"))
		fake++;
	if (findopt(mnt.mnt_mntopts, "m"))
		mflg++;
	if (findopt(mnt.mnt_mntopts, MNTOPT_NOSUID))
		nosuid++;
	replace_opts(opts, nosuid, MNTOPT_NOSUID, "suid");
	replace_opts(opts, ro, MNTOPT_RO, MNTOPT_RW);
	replace_opts(opts, largefiles, MNTOPT_NOLARGEFILES, MNTOPT_LARGEFILES);

	if (findopt(mnt.mnt_mntopts, MNTOPT_RQ)) {
		rmopt(&mnt, MNTOPT_RQ);
		replace_opts(opts, 1, MNTOPT_QUOTA, MNTOPT_NOQUOTA);
	}

	mountfs(&mnt);
	/* NOTREACHED */
}

static void
reportlogerror(int ret, char *mp, char *special, char *cmd, fiolog_t *flp)
{
	/* No error */
	if ((ret != -1) && (flp->error == FIOLOG_ENONE))
		return;

	/* logging was not enabled/disabled */
	if (ret == -1 || flp->error != FIOLOG_ENONE)
		fprintf(stderr, gettext("Could not %s logging"
				" for %s on %s.\n"), cmd, mp, special);

	/* ioctl returned error */
	if (ret == -1)
		return;

	/* Some more info */
	switch (flp->error) {
	case FIOLOG_ENONE :
		if (flp->nbytes_requested &&
		    (flp->nbytes_requested != flp->nbytes_actual)) {
			fprintf(stderr, gettext("The log has been resized"
					" from %d bytes to %d bytes.\n"),
					flp->nbytes_requested,
					flp->nbytes_actual);
		}
		return;
	case FIOLOG_ETRANS :
		fprintf(stderr, gettext("Solstice DiskSuite Logging"
				" is already enabled.\n"));
		fprintf(stderr, gettext("Please see the DiskSuite"
				" commands metadetach(1M)"
				" or metaclear(1M).\n"));
		break;
	case FIOLOG_EROFS :
		fprintf(stderr, gettext("File system is mounted read only.\n"));
		fprintf(stderr, gettext("Please see the remount"
				" option described in mount_ufs(1M).\n"));
		break;
	case FIOLOG_EULOCK :
		fprintf(stderr, gettext("File system is locked.\n"));
		fprintf(stderr, gettext("Please see the -u option described"
				" in lockfs(1M).\n"));
		break;
	case FIOLOG_EWLOCK :
		fprintf(stderr, gettext("The file system could not be"
					" write locked.\n"));
		fprintf(stderr, gettext("Please see the -w option described"
				" in lockfs(1M).\n"));
		break;
	case FIOLOG_ECLEAN :
		fprintf(stderr, gettext("The file system may not be"
					" stable.\n"));
		fprintf(stderr, gettext("Please see the -n option"
				" for fsck(1M).\n"));
		break;
	case FIOLOG_ENOULOCK :
		fprintf(stderr, gettext("The file system could not be"
					" unlocked.\n"));
		fprintf(stderr, gettext("Please see the -u option described"
				" in lockfs(1M).\n"));
		break;
	default :
		fprintf(stderr, gettext("Unknown internal error"
					" %d.\n"), flp->error);
		break;
	}
}

static int
checkislog(char *mp)
{
	int fd;
	uint32_t islog;

	fd = open(mp, O_RDONLY);
	islog = 0;
	(void) ioctl(fd, _FIOISLOG, &islog);
	close(fd);
	return ((int)islog);
}

static void
enable_logging(char *mp, char *special)
{
	int fd, ret, islog;
	fiolog_t fl;

	fd = open(mp, O_RDONLY);
	if (fd == -1) {
		perror(mp);
		return;
	}
	fl.nbytes_requested = 0;
	fl.nbytes_actual = 0;
	fl.error = FIOLOG_ENONE;
	ret = ioctl(fd, _FIOLOGENABLE, &fl);
	if (ret == -1)
		perror(mp);
	close(fd);

	/* is logging enabled? */
	islog = checkislog(mp);

	/* report errors, if any */
	if (ret == -1 || !islog)
		reportlogerror(ret, mp, special, "enable", &fl);
}

static void
disable_logging(char *mp, char *special)
{
	int fd, ret, islog;
	fiolog_t fl;

	fd = open(mp, O_RDONLY);
	if (fd == -1) {
		perror(mp);
		return;
	}
	fl.error = FIOLOG_ENONE;
	ret = ioctl(fd, _FIOLOGDISABLE, &fl);
	if (ret == -1)
		perror(mp);
	close(fd);

	/* is logging enabled? */
	islog = checkislog(mp);

	/* report errors, if any */
	if (ret == -1 || islog)
		reportlogerror(ret, mp, special, "disable", &fl);
}


/*
 * attempt to mount file system, return errno or 0
 */
void
mountfs(struct mnttab *mnt)
{
	char			 opt[1024];
	char			 opt2[1024];
	char			*opts =	opt;
	int			 flags = 0;
	struct ufs_args_fop	 args;
	struct ufs_args		 old_args;
	int			 need_separator = 0;

	(void) bzero((char *)&args, sizeof (args));
	(void) strcpy(opts, mnt->mnt_mntopts);
	opt2[0] = '\0';

	flags |= Oflg ? MS_OVERLAY : 0;
	flags |= eatmntopt(mnt, MNTOPT_RO) ? MS_RDONLY : 0;
	flags |= eatmntopt(mnt, MNTOPT_NOSUID) ? MS_NOSUID : 0;
	flags |= eatmntopt(mnt, MNTOPT_REMOUNT) ? MS_REMOUNT : 0;

	if (eatmntopt(mnt, MNTOPT_NOINTR))
		args.flags |= UFSMNT_NOINTR;
	if (eatmntopt(mnt, MNTOPT_INTR))
		args.flags &= ~UFSMNT_NOINTR;
	if (eatmntopt(mnt, MNTOPT_SYNCDIR))
		args.flags |= UFSMNT_SYNCDIR;
	if (eatmntopt(mnt, MNTOPT_FORCEDIRECTIO)) {
		args.flags |= UFSMNT_FORCEDIRECTIO;
		args.flags &= ~UFSMNT_NOFORCEDIRECTIO;
	}
	if (eatmntopt(mnt, MNTOPT_NOFORCEDIRECTIO)) {
		args.flags |= UFSMNT_NOFORCEDIRECTIO;
		args.flags &= ~UFSMNT_FORCEDIRECTIO;
	}
	if (eatmntopt(mnt, MNTOPT_NOSETSEC))
		args.flags |= (UFSMNT_NOSETSEC);
	if (eatmntopt(mnt, MNTOPT_LARGEFILES))
		args.flags |= (UFSMNT_LARGEFILES);
	if (eatmntopt(mnt, MNTOPT_NOLARGEFILES))
		args.flags &= ~UFSMNT_LARGEFILES;
	if (eatmntopt(mnt, MNTOPT_LOGGING))
		args.flags |= UFSMNT_LOGGING;
	if (eatmntopt(mnt, MNTOPT_NOLOGGING))
		args.flags &= ~UFSMNT_LOGGING;

	while (*opts != '\0') {
		char	*argval;

		switch (getsubopt(&opts, fop_subopts, &argval)) {
		case ONERROR:
			if (argval) {
				struct fop_subopt	*s;
				int			 found = 0;

				for (s = fop_subopt_list;
				    s->str && !found;
				    s++) {
					if (strcmp(argval, s->str) == 0) {
						args.flags |= s->flag;
						found = 1;
					}
				}
				if (!found) {
					usage();
				}

				if (need_separator)
					(void) strcat(opt2, ",");
				(void) strcat(opt2, MNTOPT_ONERROR);
				(void) strcat(opt2, "=");
				(void) strcat(opt2, argval);
				need_separator = 1;

			} else {
				args.flags |= UFSMNT_ONERROR_DEFAULT;
			}
			break;

		case TOOSOON:
		{
			char			*units_str;
			long			 val;
			struct units_conv	*u;
			int			 found = 0;

			if (argval == NULL || (int)strlen(argval) <= 0)
				usage();

			units_str = strdup(argval);
			for (; isdigit(*units_str); units_str++)
				/* NULL */;

			if (strlen(units_str) != 1)
				usage();

			val = atol(argval);

			for (u = units_list; u->str && !found; u++) {
				if (strcasecmp(units_str, u->str) == 0) {
					found = 1;
					val *= u->normalize_to_secs;
				}
			}

			if (val <= 0 || !found)
				usage();

			args.toosoon = val;

			if (need_separator)
				(void) strcat(opt2, ",");
			(void) strcat(opt2, MNTOPT_TOOSOON);
			(void) strcat(opt2, "=");
			(void) strcat(opt2, argval);
			need_separator = 1;

			break;
		}
		case NOMATCH:
		default:
			if (argval) {
				if (need_separator)
					(void) strcat(opt2, ",");
				(void) strcat(opt2, argval);
				need_separator = 1;
			}
			break;

		}
	}

	if (*opt2 != '\0')
		strcpy(opt, opt2);
	opts = opt;
	if ((args.flags & UFSMNT_ONERROR_FLGMASK) == 0)
		args.flags |= UFSMNT_ONERROR_DEFAULT;

	if (fake)
		goto itworked;

	old_args.flags = args.flags & ~UFSMNT_ONERROR_FLGMASK;

	(void) signal(SIGHUP,  SIG_IGN);
	(void) signal(SIGQUIT, SIG_IGN);
	(void) signal(SIGINT,  SIG_IGN);

	/*
	 * try both forms, just in case this is a pre-fop kernel/fs/ufs
	 * with a post-fop ufs_mount command
	 */
	errno = 0;
	if (mount(mnt->mnt_special, mnt->mnt_mountp, flags | MS_DATA, "ufs",
		(char *)&args, sizeof (args)) < 0) {
		if (errno != EINVAL) {
			rpterr(mnt->mnt_special, mnt->mnt_mountp);
			exit(32);
		} else if (mount(mnt->mnt_special, mnt->mnt_mountp,
			    flags | MS_DATA, "ufs", (char *)&old_args,
			    sizeof (old_args)) < 0) {
				rpterr(mnt->mnt_special, mnt->mnt_mountp);
				exit(32);
		}
	}

	if (args.flags & UFSMNT_LOGGING)
		enable_logging(mnt->mnt_mountp, mnt->mnt_special);
	else if (!(flags & MS_RDONLY))
		disable_logging(mnt->mnt_mountp, mnt->mnt_special);

	if (mflg)
		exit(0);

itworked:
	if (flags & MS_REMOUNT) {
		if (!mflg)
			fsrmfrommtab(mnt);
		replace_opts(mnt->mnt_mntopts, 1, MNTOPT_RW, MNTOPT_RO);
	}

	fixopts(mnt, opts, checkislog(mnt->mnt_mountp));
	if (*opts) {
		(void) fprintf(stderr, gettext(
		    "mount: %s on %s - WARNING unknown options \"%s\"\n"),
		    mnt->mnt_special, mnt->mnt_mountp, opts);
	}
	if (!mflg)
		(void) fsaddtomtab(mnt);
	exit(0);
}

/*
 * same as findopt but remove the option from the option string and return
 * true or false
 */
static int
eatmntopt(struct mnttab *mnt, char *opt)
{
	int has;

	has = (findopt(mnt->mnt_mntopts, opt) != NULL);
	rmopt(mnt, opt);
	return (has);
}

/*
 * remove an option string from the option list
 */
static void
rmopt(struct mnttab *mnt, char *opt)
{
	char *str;
	char *optstart;

	while (optstart = findopt(mnt->mnt_mntopts, opt)) {
		for (str = optstart;
		    *str != ','	&& *str != '\0' && *str != ' ';
		    str++)
			/* NULL */;
		if (*str == ',') {
			str++;
		} else if (optstart != mnt->mnt_mntopts) {
			optstart--;
		}
		while (*optstart++ = *str++)
			;
	}
}

/*
 * mnt->mnt_ops has un-eaten opts, opts is the original opts list.
 * Set mnt->mnt_opts to the original list minus the un-eaten opts.
 * Set "opts" to the un-eaten opts minus the "default" options ("rw",
 * "hard", "noquota", "noauto", "bg", and "largefiles".  If there are any
 * options left after this, they are uneaten because they are unknown; our
 * caller will print a warning message.
 */
static void
fixopts(struct mnttab *mnt, char *opts, int islog)
{
	char *comma;
	char *ue;
	char uneaten[1024];

	/*
	 * Remove the options we know about
	 */
	rmopt(mnt, MNTOPT_RW);
	rmopt(mnt, MNTOPT_REMOUNT);
	rmopt(mnt, MNTOPT_RO);
	rmopt(mnt, MNTOPT_NOQUOTA);
	rmopt(mnt, MNTOPT_QUOTA);
	rmopt(mnt, MNTOPT_ONERROR);
	rmopt(mnt, MNTOPT_TOOSOON);
/*	rmopt(mnt, MNTOPT_NOAUTO); */
	rmopt(mnt, MNTOPT_NOLOGGING);
	rmopt(mnt, MNTOPT_LOGGING);
	rmopt(mnt, "suid");
	rmopt(mnt, "f");
	rmopt(mnt, "n");
	rmopt(mnt, MNTOPT_NOLARGEFILES);
	rmopt(mnt, MNTOPT_LARGEFILES);
	rmopt(mnt, MNTOPT_SYNCDIR);
	rmopt(mnt, MNTOPT_FORCEDIRECTIO);
	rmopt(mnt, MNTOPT_NOFORCEDIRECTIO);
	rmopt(mnt, MNTOPT_INTR);
	rmopt(mnt, MNTOPT_NOINTR);
	rmopt(mnt, MNTOPT_NOSETSEC);

	/*
	 * Save the uneaten options
	 */
	(void) strcpy(uneaten, mnt->mnt_mntopts);

	/*
	 * Set the options for ``/etc/mnttab'' to be the original
	 * options from main(); except for the option "f" and "remount".
	 */
	(void) strcpy(mnt->mnt_mntopts, opts);
	rmopt(mnt, "f");
	rmopt(mnt, MNTOPT_REMOUNT);

	/*
	 * opts now points to the uneaten options.
	 *
	 * The caller of this function will report the uneaten options as
	 * errors.
	 */

	(void) strcpy(opts, uneaten);

	/*
	 * Remove uneaten options from the /etc/mnttab options
	 */
	for (ue = uneaten; *ue; ) {
		for (comma = ue; *comma != '\0' && *comma != ','; comma++)
			;
		if (*comma == ',') {
			*comma = '\0';
			rmopt(mnt, ue);
			ue = comma+1;
		} else {
			rmopt(mnt, ue);
			ue = comma;
		}
	}

	/*
	 * Woops, something is wrong, there are no more options! At the very
	 * least, set options to the minimal defaults.
	 */
	if (*mnt->mnt_mntopts == '\0') {
		if (islog) {
			(void) sprintf(mnt->mnt_mntopts, "%s,%s,%s",
			    MNTOPT_RW, MNTOPT_LARGEFILES, MNTOPT_LOGGING);
		} else {
			(void) sprintf(mnt->mnt_mntopts, "%s,%s",
			    MNTOPT_RW, MNTOPT_LARGEFILES);
		}
	} else {
		/*
		 * Adjust logging option to the actual state of the fs
		 */
		rmopt(mnt, MNTOPT_LOGGING);
		rmopt(mnt, MNTOPT_NOLOGGING);
		if (islog) {
			replace_opts(mnt->mnt_mntopts, 0,
			    MNTOPT_NOLOGGING, MNTOPT_LOGGING);
		}
	}
}

static void
usage()
{
	(void) fprintf(stdout, gettext(
"ufs usage:\n"
"mount [-F ufs] [generic options] [-o suboptions] {special | mount_point}\n"));
	(void) fprintf(stdout, gettext(
			"\tsuboptions are: \n"
			"\t	ro,rw,nosuid,remount,f,m,\n"
			"\t	largefiles,nolargefiles,\n"
			"\t	forcedirectio,noforcedirectio\n"
			"\t	logging,nologging,\n"
			"\t	onerror[={panic | lock | umount | repair}],\n,"
			"\t	toosoon=<val>[s|m|h|d|w|y]\n"));

	exit(32);
}

/*
 * Returns the next option in the option string.
 */
static char *
getnextopt(char **p)
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

/*
 * "trueopt" and "falseopt" are two settings of a Boolean option.
 * If "flag" is true, forcibly set the option to the "true" setting; otherwise,
 * if the option isn't present, set it to the false setting.
 */
static void
replace_opts(char *options, int flag, char *trueopt, char *falseopt)
{
	char *f;
	char *tmpoptsp;
	int found;
	char tmptopts[MNTMAXSTR];

	(void) strcpy(tmptopts, options);
	tmpoptsp = tmptopts;
	(void) strcpy(options, "");

	found = 0;
	for (f = getnextopt(&tmpoptsp); *f; f = getnextopt(&tmpoptsp)) {
		if (options[0] != '\0')
			(void) strcat(options, ",");
		if (strcmp(f, trueopt) == 0) {
			(void) strcat(options, f);
			found++;
		} else if (strcmp(f, falseopt) == 0) {
			if (flag)
				(void) strcat(options, trueopt);
			else
				(void) strcat(options, f);
			found++;
		} else
			(void) strcat(options, f);
	}
	if (!found) {
		if (options[0] != '\0')
			(void) strcat(options, ",");
		(void) strcat(options, flag ? trueopt : falseopt);
	}
}

static void
rpterr(char *bs, char *mp)
{
	switch (errno) {
	case EPERM:
		(void) fprintf(stderr, gettext("%s: not super user\n"), myname);
		break;
	case ENXIO:
		(void) fprintf(stderr, gettext("%s: %s no such device\n"),
								myname, bs);
		break;
	case ENOTDIR:
		(void) fprintf(stderr,
			gettext(
	"%s: %s not a directory\n\tor a component of %s is not a directory\n"),
			myname, mp, bs);
		break;
	case ENOENT:
		(void) fprintf(stderr, gettext(
			"%s: %s or %s, no such file or directory\n"),
			myname, bs, mp);
		break;
	case EINVAL:
		(void) fprintf(stderr, gettext("%s: %s is not this fstype.\n"),
			myname, bs);
		break;
	case EBUSY:
		(void) fprintf(stderr, gettext(
	"%s: %s is already mounted, %s is busy,\n"
	"\tor the allowable number of mount points has been exceeded\n"),
		myname, bs, mp);
		break;
	case ENOTBLK:
		(void) fprintf(stderr, gettext(
			"%s: %s not a block device\n"), myname, bs);
		break;
	case EROFS:
		(void) fprintf(stderr, gettext("%s: %s write-protected\n"),
			myname, bs);
		break;
	case ENOSPC:
		(void) fprintf(stderr, gettext(
			"%s: the state of %s is not okay\n"
			"\tand it was attempted to be mounted read/write\n"),
			myname, bs);
		(void) printf(gettext(
			"mount: Please run fsck and try again\n"));
		break;
	case EFBIG:
		(void) fprintf(stderr, gettext(
			"%s: large files may be present on %s,\n"
			"\tand it was attempted to be mounted nolargefiles\n"),
		    myname, bs);
		break;
	default:
		perror(myname);
		(void) fprintf(stderr, gettext("%s: cannot mount %s\n"),
				myname, bs);
	}
}
