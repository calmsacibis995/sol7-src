#pragma ident	"@(#)mount.c	1.11	97/02/10 SMI"

/*
 * Copyright (c) 1988 Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <locale.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mnttab.h>
#include <sys/mount.h>
#include <sys/fs/pc_fs.h>
#include <fslib.h>

#define	MNTTYPE_PCFS	"pcfs"

extern int	daylight;

extern int errno;

/*
 * The "hidden/nohidden" option is private. It is not approved for
 * use (see PSARC/1996/443), which is why it is not in the usage message.
 */
#define	RO		0
#define	RW		1
#define	HIDDEN		2
#define	NOHIDDEN	3
#define	FOLDCASE	4
#define	NOFOLDCASE	5

#define	HIDDEN_STR	"hidden"
#define	NOHIDDEN_STR	"nohidden"
#define	FOLDCASE_STR	"foldcase"
#define	NOFOLDCASE_STR	"nofoldcase"

int roflag = 0;
static char *myopts[] = {
	"ro",
	"rw",
	HIDDEN_STR,
	NOHIDDEN_STR,
	FOLDCASE_STR,
	NOFOLDCASE_STR,
	NULL
};

struct pcfs_args tz;

main(int argc, char *argv[])
{
	struct mnttab mnt;
	int c;
	char *myname;
	char typename[64];
	char buf[256];
	char *options, *value;
	extern int optind;
	extern char *optarg;
	int error = 0;
	int verbose = 0;
	int mflg = 0;
	int rflg = 0;
	int hidden = 0;
	int foldcase = 0;
	int optcnt = 0;

	myname = strrchr(argv[0], '/');
	myname = myname ? myname + 1 : argv[0];
	(void) sprintf(typename, "%s_%s", MNTTYPE_PCFS, myname);
	argv[0] = typename;

	while ((c = getopt(argc, argv, "Vvmr?o:O")) != EOF) {
		switch (c) {
		case 'V':
		case 'v':
			verbose++;
			break;
		case '?':
			error++;
			break;
		case 'r':
			roflag++;
			break;
		case 'm':
			rflg++;
			break;
		case 'o':
			options = optarg;
			while (*options != '\0') {
				switch (getsubopt(&options, myopts, &value)) {
				case RO:
					roflag++;
					break;
				case RW:
					break;
				case HIDDEN:
					hidden++;
					break;
				case NOHIDDEN:
					hidden = 0;
					break;
				case FOLDCASE:
					foldcase++;
					break;
				case NOFOLDCASE:
					foldcase = 0;
					break;
				default:
					(void) fprintf(stderr,
				    gettext("%s: illegal -o suboption -- %s\n"),
					    typename, value);
					error++;
					break;
				}
			}
			break;
		case 'O':
			mflg |= MS_OVERLAY;
			break;
		}
	}

	if (verbose && !error) {
		char *optptr;

		(void) fprintf(stderr, "%s", typename);
		for (optcnt = 1; optcnt < argc; optcnt++) {
			optptr = argv[optcnt];
			if (optptr)
				(void) fprintf(stderr, " %s", optptr);
		}
		(void) fprintf(stderr, "\n");
	}

	if (argc - optind != 2 || error) {
		/*
		 * don't hint at options yet (none are really supported)
		 */
		(void) fprintf(stderr, gettext(
		    "Usage: %s [generic options] [-o suboptions] "
		    "special mount_point\n"), typename);
		(void) fprintf(stderr, gettext(
		    "\tsuboptions are:\n"
		    "\t     ro,rw\n"
		    "\t     foldcase,nofoldcase\n"));
		exit(32);
	}

	mnt.mnt_special = argv[optind++];
	mnt.mnt_mountp = argv[optind++];
	mnt.mnt_fstype = MNTTYPE_PCFS;

	(void) tzset();
	tz.secondswest = timezone;
	tz.dsttime = daylight;
	mflg |= MS_DATA;
	mnt.mnt_mntopts = buf;
	if (roflag) {
		mflg |= MS_RDONLY;
		(void) strcpy(mnt.mnt_mntopts, "ro");
	} else {
		(void) strcpy(mnt.mnt_mntopts, "rw");
	}
	if (hidden) {
		tz.flags |= PCFS_MNT_HIDDEN;
		(void) strcat(mnt.mnt_mntopts, "," HIDDEN_STR);
	} else {
		tz.flags &= ~PCFS_MNT_HIDDEN;
		(void) strcat(mnt.mnt_mntopts, "," NOHIDDEN_STR);
	}
	if (foldcase) {
		tz.flags |= PCFS_MNT_FOLDCASE;
		(void) strcat(mnt.mnt_mntopts, "," FOLDCASE_STR);
	} else {
		tz.flags &= ~PCFS_MNT_FOLDCASE;
		(void) strcat(mnt.mnt_mntopts, "," NOFOLDCASE_STR);
	}

	signal(SIGHUP,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGINT,  SIG_IGN);

	if (verbose) {
		(void) fprintf(stderr, "mount(%s, \"%s\", %d, %s",
		    mnt.mnt_special, mnt.mnt_mountp, mflg, MNTTYPE_PCFS);
	}
	if (mount(mnt.mnt_special, mnt.mnt_mountp, mflg, MNTTYPE_PCFS,
	    (char *)&tz, sizeof (struct pcfs_args))) {
		if (errno == EBUSY) {
			fprintf(stderr, gettext(
			    "mount: %s is already mounted, %s is busy,\n\tor"
			    " allowable number of mount points exceeded\n"),
			    mnt.mnt_special, mnt.mnt_mountp);
		} else if (errno == EINVAL) {
			fprintf(stderr, gettext(
			    "mount: %s is not a DOS filesystem.\n"),
			    mnt.mnt_special);
		} else {
			perror("mount");
		}
		exit(32);
	}

	if (!rflg && fsaddtomtab(&mnt))
		exit(1);
	return (0);
}
