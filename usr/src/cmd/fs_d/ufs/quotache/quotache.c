/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)quotacheck.c	1.32	97/01/29 SMI"	/* SVr4.0 1.8 */
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
 * Copyright (c) 1986,1987,1988,1989,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *                All rights reserved.
 *
 */

/*
 * Fix up / report on disc quotas & usage
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/mntent.h>

#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_quota.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mnttab.h>
#include <sys/vfstab.h>
#include <pwd.h>

union {
	struct	fs	sblk;
	char	dummy[MAXBSIZE];
} un;
#define	sblock	un.sblk

#define	ITABSZ	256
struct	dinode	itab[ITABSZ];
struct	dinode	*dp;

struct fileusage {
	struct fileusage *fu_next;
	u_long fu_curfiles;
	u_long fu_curblocks;
	uid_t	fu_uid;
};
#define	FUHASH 997
struct fileusage *fuhead[FUHASH];
struct fileusage *lookup(uid_t);
struct fileusage *adduid(uid_t);

int fi;
ino_t ino;
struct	dinode	*ginode();
char *mntopt(), *hasvfsopt(), *hasmntopt();

extern int	optind;
extern char	*optarg;
extern int	errno;
extern int	fsync(int);

static void acct();
static void bread();
static void usage();
static int chkquota();
static int quotactl();
static int preen();
static int waiter();
static int oneof();

int	vflag;		/* verbose */
int	aflag;		/* all file systems */
int	pflag;		/* fsck like parallel check */
int	fflag;		/* force flag */

#define	QFNAME "quotas"
#define	CHUNK	50
char **listbuf;
struct dqblk zerodqbuf;
struct fileusage zerofileusage;

void
main(int argc, char **argv)
{
	struct mnttab mntp;
	struct vfstab vfsbuf;
	char **listp;
	int listcnt;
	int listmax = 0;
	char quotafile[MAXPATHLEN + 1];
	FILE *mtab, *vfstab;
	int errs = 0;
	int	opt;

	if ((listbuf = (char **)malloc(sizeof (char *) * CHUNK)) == NULL) {
		fprintf(stderr, "Can't alloc lisbuf array.");
		exit(31+1);
	}
	listmax = CHUNK;
	while ((opt = getopt(argc, argv, "vapVf")) != EOF) {
		switch (opt) {

		case 'v':
			vflag++;
			break;

		case 'a':
			aflag++;
			break;

		case 'p':
			pflag++;
			break;

		case 'V':		/* Print command line */
			{
				char		*opt_text;
				int		opt_count;

				(void) fprintf(stdout, "quotacheck -F UFS ");
				for (opt_count = 1; opt_count < argc;
				    opt_count++) {
					opt_text = argv[opt_count];
					if (opt_text)
						(void) fprintf(stdout, " %s ",
						    opt_text);
				}
				(void) fprintf(stdout, "\n");
			}
			break;

		case 'f':
			fflag++;
			break;

		case '?':
			usage();
		}
	}
	if (argc <= optind && !aflag) {
		usage();
	}

	if (quotactl(Q_ALLSYNC, NULL, 0, NULL) < 0 && errno == EINVAL && vflag)
		printf("Warning: Quotas are not compiled into this kernel\n");
	sync();

	if (aflag) {
		/*
		 * Go through vfstab and make a list of appropriate
		 * filesystems.
		 */
		listp = listbuf;
		listcnt = 0;
		if ((vfstab = fopen(VFSTAB, "r")) == NULL) {
			fprintf(stderr, "Can't open ");
			perror(VFSTAB);
			exit(31+8);
		}
		while (getvfsent(vfstab, &vfsbuf) == NULL) {
			if (strcmp(vfsbuf.vfs_fstype, MNTTYPE_UFS) != 0 ||
			    (vfsbuf.vfs_mntopts == 0) ||
			    hasvfsopt(&vfsbuf, MNTOPT_RO) ||
			    (!hasvfsopt(&vfsbuf, MNTOPT_RQ) &&
			    !hasvfsopt(&vfsbuf, MNTOPT_QUOTA)))
				continue;
			*listp = malloc(strlen(vfsbuf.vfs_special) + 1);
			strcpy(*listp, vfsbuf.vfs_special);
			listp++;
			listcnt++;
			/* grow listbuf if needed */
			if (listcnt >= listmax) {
				listmax += CHUNK;
				listbuf = (char **) realloc(listbuf,
					sizeof (char *) * listmax);
				if (listbuf == NULL) {
					fprintf(stderr,
						"Can't grow listbuf.\n");
					exit(31+1);
				}
				listp = &listbuf[listcnt];
			}
		}
		fclose(vfstab);
		*listp = (char *)0;
		listp = listbuf;
	} else {
		listp = &argv[optind];
		listcnt = argc - optind;
	}
	if (pflag) {
		errs = preen(listcnt, listp);
	} else {
		if ((mtab = fopen(MNTTAB, "r")) == NULL) {
			fprintf(stderr, "Can't open ");
			perror(MNTTAB);
			exit(31+8);
		}
		while (getmntent(mtab, &mntp) == NULL) {
			if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) == 0 &&
			    !hasmntopt(&mntp, MNTOPT_RO) &&
			    (oneof(mntp.mnt_special, listp, listcnt) ||
			    oneof(mntp.mnt_mountp, listp, listcnt))) {
				sprintf(quotafile,
				    "%s/%s", mntp.mnt_mountp, QFNAME);
				errs +=
				    chkquota(mntp.mnt_special,
					mntp.mnt_mountp, quotafile);
			}
		}
		fclose(mtab);
	}
	while (listcnt--) {
		if (*listp) {
			fprintf(stderr, "Cannot check %s\n", *listp);
			errs++;
		}
		listp++;
	}
	if (errs > 0)
		errs += 31;
	exit(errs);
}

static struct active {
	char *rdev;
	pid_t pid;
	struct active *nxt;
};

int
preen(listcnt, listp)
	int listcnt;
	char **listp;
{
	register int i, rc, errs;
	register char **lp, *rdev, *bdev;
	extern char *getfullrawname(), *getfullblkname();
	struct mnttab mntp, mpref;
	struct active *alist, *ap;
	FILE *mtab;
	char quotafile[MAXPATHLEN + 1];
	char name[MAXPATHLEN + 1];
	int nactive, serially;

	if ((mtab = fopen(MNTTAB, "r")) == NULL) {
		fprintf(stderr, "Can't open ");
		perror(MNTTAB);
		exit(31+8);
	}
	memset(&mpref, 0, sizeof (struct mnttab));
	errs = 0;

	for (lp = listp, i = 0; i < listcnt; lp++, i++) {
		serially = 0;
		rdev = getfullrawname(*lp);
		if (rdev == NULL || *rdev == '\0') {
			fprintf(stderr, "can't get rawname for `%s'\n", *lp);
			serially = 1;
		} else if (preen_addev(rdev) != 0) {
			fprintf(stderr, "preen_addev error\n");
			serially = 1;
		}

		if (rdev != NULL)
			free(rdev);

		if (serially) {
			rewind(mtab);
			mpref.mnt_special = *lp;
			if (getmntany(mtab, &mntp, &mpref) == 0 &&
			    strcmp(mntp.mnt_fstype, MNTTYPE_UFS) == 0 &&
			    !hasmntopt(&mntp, MNTOPT_RO)) {
				errs += (31+chkquota(mntp.mnt_special,
				    mntp.mnt_mountp, quotafile));
				*lp = (char *)0;
			}
		}
	}

	nactive = 0;
	alist = NULL;
	while ((rc = preen_getdev(name)) > 0) {
		switch (rc) {
		case 1:
			bdev = getfullblkname(name);
			if (bdev == NULL || *bdev == '\0') {
				fprintf(stderr, "can't get blkname for `%s'\n",
				    name);
				if (bdev)
					free(bdev);
				continue;
			}
			rewind(mtab);
			mpref.mnt_special = bdev;
			if (getmntany(mtab, &mntp, &mpref) != 0) {
				fprintf(stderr, "`%s' not mounted?\n", name);
				preen_releasedev(name);
				free(bdev);
				continue;
			} else if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0 ||
			    hasmntopt(&mntp, MNTOPT_RO) ||
			    (!oneof(mntp.mnt_special, listp, listcnt) &&
			    !oneof(mntp.mnt_mountp, listp, listcnt))) {
				preen_releasedev(name);
				free(bdev);
				continue;
			}
			free(bdev);
			ap = (struct active *) malloc(sizeof (struct active));
			if (ap == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(31+8);
			}
			ap->rdev = (char *)strdup(name);
			if (ap->rdev == NULL) {
				fprintf(stderr, "out of memory\n");
				exit(31+8);
			}
			ap->nxt = alist;
			alist = ap;
			switch (ap->pid = fork()) {
			case -1:
				perror("fork");
				exit(31+8);
				break;
			case 0:
				sprintf(quotafile, "%s/%s",
				    mntp.mnt_mountp, QFNAME);
				exit(31+chkquota(mntp.mnt_special,
				    mntp.mnt_mountp, quotafile));
				break;
			default:
				nactive++;
				break;
			}
			break;
		case 2:
			errs += waiter(&alist);
			nactive--;
			break;
		}
	}
	fclose(mtab);

	while (nactive > 0) {
		errs += waiter(&alist);
		nactive--;
	}
	return (errs);
}

int
waiter(alp)
	struct active **alp;
{
	pid_t curpid;
	int status;
	register struct active *ap, *lap;

	curpid = wait(&status);
	if (curpid == -1) {
		if (errno == ECHILD)
			return (0);
		perror("wait");
		exit(31+8);
	}

	for (lap = NULL, ap = *alp; ap != NULL; lap = ap, ap = ap->nxt) {
		if (ap->pid == curpid)
			break;
	}

	if (ap == NULL) {
		fprintf(stderr, "wait returns unknown pid\n");
		exit(31+8);
	} else if (lap) {
		lap->nxt = ap->nxt;
	} else {
		*alp = ap->nxt;
	}
	preen_releasedev(ap->rdev);
	free(ap->rdev);
	free(ap);
	return (WHIBYTE(status));
}

int
chkquota(fsdev, fsfile, qffile)
	char *fsdev;
	char *fsfile;
	char *qffile;
{
	register struct fileusage *fup;
	dev_t quotadev;
	FILE *qf;
	uid_t uid;
	struct passwd *pw;
	int cg, i;
	char *rawdisk;
	struct stat64 statb;
	struct dqblk dqbuf;
	extern int errno;
	extern char *getfullrawname();

	if ((rawdisk = getfullrawname(fsdev)) == NULL) {
		fprintf(stderr, "malloc failed\n");
		return (1);
	}

	if (*rawdisk == '\0') {
		fprintf(stderr, "Could not find character device for %s\n",
		    fsdev);
		return (1);
	}

	if (vflag)
		printf("*** Checking quotas for %s (%s)\n", rawdisk, fsfile);
	fi = open64(rawdisk, 0);
	if (fi < 0) {
		perror(rawdisk);
		return (1);
	}
	qf = fopen64(qffile, "r+");
	if (qf == NULL) {
		perror(qffile);
		close(fi);
		return (1);
	}
	if (fstat64(fileno(qf), &statb) < 0) {
		perror(qffile);
		fclose(qf);
		close(fi);
		return (1);
	}
	quotadev = statb.st_dev;
	if (stat64(fsdev, &statb) < 0) {
		perror(fsdev);
		fclose(qf);
		close(fi);
		return (1);
	}
	if (quotadev != statb.st_rdev) {
		fprintf(stderr, "%s dev (0x%x) mismatch %s dev (0x%x)\n",
			qffile, quotadev, fsdev, statb.st_rdev);
		fclose(qf);
		close(fi);
		return (1);
	}
	bread(SBLOCK, (char *)&sblock, SBSIZE);
	/*
	 * no need to quotacheck a rw, mounted, and logging file system
	 */
	if ((fflag == 0) && pflag &&
	    (FSOKAY == (sblock.fs_state + sblock.fs_time)) &&
	    (sblock.fs_clean == FSLOG)) {
		fclose(qf);
		close(fi);
		return (0);
	}
	ino = 0;
	for (cg = 0; cg < sblock.fs_ncg; cg++) {
		dp = NULL;
		for (i = 0; i < sblock.fs_ipg; i++)
			acct(ginode());
	}
	for (uid = 0; uid < MAXUID; uid++) {
		(void) fread(&dqbuf, sizeof (struct dqblk), 1, qf);
		if (feof(qf))
			break;
		fup = lookup(uid);
		if (fup == 0)
			fup = &zerofileusage;
		if (dqbuf.dqb_bhardlimit == 0 && dqbuf.dqb_bsoftlimit == 0 &&
		    dqbuf.dqb_fhardlimit == 0 && dqbuf.dqb_fsoftlimit == 0) {
			fup->fu_curfiles = 0;
			fup->fu_curblocks = 0;
		}
		if (dqbuf.dqb_curfiles == fup->fu_curfiles &&
		    dqbuf.dqb_curblocks == fup->fu_curblocks) {
			fup->fu_curfiles = 0;
			fup->fu_curblocks = 0;
			continue;
		}
		if (vflag) {
			if (pflag || aflag)
				printf("%s: ", rawdisk);
			if ((pw = getpwuid(uid)) && pw->pw_name[0])
				printf("%-10s fixed:", pw->pw_name);
			else
				printf("#%-9d fixed:", uid);
			if (dqbuf.dqb_curfiles != fup->fu_curfiles)
				printf("  files %d -> %d",
				    dqbuf.dqb_curfiles, fup->fu_curfiles);
			if (dqbuf.dqb_curblocks != fup->fu_curblocks)
				printf("  blocks %d -> %d",
				    dqbuf.dqb_curblocks, fup->fu_curblocks);
			printf("\n");
		}
		dqbuf.dqb_curfiles = fup->fu_curfiles;
		dqbuf.dqb_curblocks = fup->fu_curblocks;
		/*
		* If quotas are not enabled for the current filesystem
		* then just update the quotas file directly.
		*/
		if ((quotactl(Q_SETQUOTA, fsfile, uid, &dqbuf) < 0) &&
		    (errno == ESRCH)) {
			/* back up, overwrite the entry we just read */
			(void) fseeko64(qf, (offset_t)dqoff(uid), 0);
			(void) fwrite(&dqbuf, sizeof (struct dqblk), 1, qf);
			(void) fflush(qf);
		}
		fup->fu_curfiles = 0;
		fup->fu_curblocks = 0;
	}
	(void) fflush(qf);
	(void) fsync(fileno(qf));
	fclose(qf);
	close(fi);
	return (0);
}

void
acct(ip)
	register struct dinode *ip;
{
	register struct fileusage *fup;

	if (ip == NULL)
		return;
	ip->di_mode = ip->di_smode;
	if (ip->di_suid != UID_LONG) {
		ip->di_uid = ip->di_suid;
	}
	if (ip->di_mode == 0)
		return;
	fup = adduid(ip->di_uid);
	fup->fu_curfiles++;
	if ((ip->di_mode & IFMT) == IFCHR || (ip->di_mode & IFMT) == IFBLK)
		return;
	fup->fu_curblocks += ip->di_blocks;
}

int
oneof(target, olistp, on)
	char *target;
	register char **olistp;
	register int on;
{
	char **listp = olistp;
	int n = on;

	while (n--) {
		if (*listp && strcmp(target, *listp) == 0) {
			*listp = (char *)0;
			return (1);
		}
		listp++;
	}
	return (0);
}

struct dinode *
ginode()
{
	register unsigned long iblk;

	if (dp == NULL || ++dp >= &itab[ITABSZ]) {
		iblk = itod(&sblock, ino);
		bread((u_long)fsbtodb(&sblock, iblk),
		    (char *)itab, sizeof (itab));
		dp = &itab[(int)ino % (int)INOPB(&sblock)];
	}
	if (ino++ < UFSROOTINO)
		return (NULL);
	return (dp);
}

void
bread(bno, buf, cnt)
	long unsigned bno;
	char *buf;
{
	extern offset_t llseek();
	offset_t pos;

	pos = (offset_t)bno * DEV_BSIZE;
	if (llseek(fi, pos, 0) != pos) {
		perror("lseek");
		exit(31+1);
	}
	if (read(fi, buf, cnt) != cnt) {
		perror("read");
		exit(31+1);
	}
}

struct fileusage *
lookup(uid_t uid)
{
	register struct fileusage *fup;

	for (fup = fuhead[uid % FUHASH]; fup != 0; fup = fup->fu_next)
		if (fup->fu_uid == uid)
			return (fup);
	return ((struct fileusage *)0);
}

struct fileusage *
adduid(uid_t uid)
{
	struct fileusage *fup, **fhp;

	fup = lookup(uid);
	if (fup != 0)
		return (fup);
	fup = (struct fileusage *)calloc(1, sizeof (struct fileusage));
	if (fup == 0) {
		fprintf(stderr, "out of memory for fileusage structures\n");
		exit(31+1);
	}
	fhp = &fuhead[uid % FUHASH];
	fup->fu_next = *fhp;
	*fhp = fup;
	fup->fu_uid = uid;
	return (fup);
}

void
usage()
{
	fprintf(stderr, "ufs usage:\n");
	fprintf(stderr, "\tquotacheck [-v] [-p] -a\n");
	fprintf(stderr, "\tquotacheck [-v] [-p] filesys ...\n");
	exit(31+1);
}

#include <sys/errno.h>

int
quotactl(cmd, mountp, uid, addr)
	int		cmd;
	char		*mountp;
	int		uid;
	caddr_t		addr;
{
	int 		fd;
	int 		status;
	struct quotctl	quota;
	char		mountpoint[256];
	FILE		*fstab;
	struct mnttab	mntp;


	if ((mountp == NULL) && (cmd == Q_ALLSYNC)) {
		/*
		 * Find the mount point of any ufs file system.   This is
		 * because the ioctl that implements the quotactl call has
		 * to go to a real file, and not to the block device.
		 */
		if ((fstab = fopen(MNTTAB, "r")) == NULL) {
			fprintf(stderr, "%s: ", MNTTAB);
			perror("open");
			exit(31+1);
		}
		fd = -1;
		while ((status = getmntent(fstab, &mntp)) == NULL) {
			if (strcmp(mntp.mnt_fstype, MNTTYPE_UFS) != 0 ||
				hasmntopt(&mntp, MNTOPT_RO))
				continue;
			(void) strcpy(mountpoint, mntp.mnt_mountp);
			strcat(mountpoint, "/quotas");
			if ((fd = open64(mountpoint, O_RDWR)) == -1)
				break;
		}
		fclose(fstab);
		if (fd == -1) {
			errno = ENOENT;
			return (-1);
		}
	} else {
		if (mountp == NULL || mountp[0] == '\0') {
			errno =  ENOENT;
			return (-1);
		}
		strcpy(mountpoint, mountp);
		strcat(mountpoint, "/quotas");
		if ((fd = open64(mountpoint, O_RDWR)) < 0) {
			fprintf(stderr, "quotactl: ");
			perror("open");
			exit(31+1);
		}
	}	/* else */

	quota.op = cmd;
	quota.uid = uid;
	quota.addr = addr;
	status = ioctl(fd, Q_QUOTACTL, &quota);
	if (fd != 0)
		close(fd);
	return (status);
}

char *
hasvfsopt(vfs, opt)
	register struct vfstab *vfs;
	register char *opt;
{
	char *f, *opts;
	static char *tmpopts;

	if (tmpopts == 0) {
		tmpopts = (char *)calloc(256, sizeof (char));
		if (tmpopts == 0)
			return (0);
	}
	strcpy(tmpopts, vfs->vfs_mntopts);
	opts = tmpopts;
	f = mntopt(&opts);
	for (; *f; f = mntopt(&opts)) {
		if (strncmp(opt, f, strlen(opt)) == 0)
			return (f - tmpopts + vfs->vfs_mntopts);
	}
	return (NULL);
}
