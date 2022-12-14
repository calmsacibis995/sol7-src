/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pfiles.c	1.13	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <door.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mkdev.h>
#include <libproc.h>

static	char	*command;
static	int	interrupt;
static	int	Fflag;

static	void	intr(int);
static	void	uncontrol(char *, size_t);
static	void	dofcntl(struct ps_prochandle *, int, int, int);
static	void	show_files(struct ps_prochandle *);
static	void	show_fileflags(int);
static	void	show_door(struct ps_prochandle *, int);

main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = 0;
	struct ps_prochandle *Pr;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "F")) != EOF) {
		switch (opt) {
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s [-F] pid ...\n",
			command);
		(void) fprintf(stderr,
			"  (report open files of each process)\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		exit(2);
	}

	/* catch signals from terminal */
	if (sigset(SIGHUP, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGHUP, intr);
	if (sigset(SIGINT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGINT, intr);
	if (sigset(SIGQUIT, SIG_IGN) == SIG_DFL)
		(void) sigset(SIGQUIT, intr);
	(void) sigset(SIGTERM, intr);

	while (--argc >= 0 && !interrupt) {
		char *arg;
		psinfo_t psinfo;
		pid_t pid;
		int gret;

		(void) fflush(stdout);	/* line-at-a-time */

		/* get the specified pid and the psinfo struct */
		if ((pid = proc_pidarg(arg = *argv++, &psinfo)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			retc++;
		} else if ((Pr = Pgrab(pid, Fflag, &gret)) != NULL) {
			if (Pcreate_agent(Pr) == 0) {
				uncontrol(psinfo.pr_psargs, PRARGSZ);
				(void) printf("%d:\t%.70s\n",
					(int)pid, psinfo.pr_psargs);
				show_files(Pr);
				Pdestroy_agent(Pr);
			} else {
				(void) fprintf(stderr,
					"%s: cannot control process %d\n",
					command, (int)pid);
				retc++;
			}
			Prelease(Pr, 0);
			Pr = NULL;
		} else {
			switch (gret) {
			case G_SYS:
			case G_SELF:
				uncontrol(psinfo.pr_psargs, PRARGSZ);
				(void) printf("%d:\t%.70s\n", (int)pid,
					psinfo.pr_psargs);
				if (gret == G_SYS)
					(void) printf("  [system process]\n");
				else
					show_files(NULL);
				break;
			default:
				(void) fprintf(stderr, "%s: %s: %d\n",
					command, Pgrab_error(gret), (int)pid);
				retc++;
				break;
			}
		}
	}

	if (interrupt && retc == 0)
		retc++;
	return (retc);
}

/* ARGSUSED */
static void
intr(int sig)
{
	interrupt = 1;
}

/* ------ begin specific code ------ */

static void
show_files(struct ps_prochandle *Pr)
{
	DIR *dirp;
	struct dirent *dentp;
	char pname[100];
	struct stat statb;
	struct rlimit rlim;
	pid_t pid;
	int fd;
	char *s;

	if (pr_getrlimit(Pr, RLIMIT_NOFILE, &rlim) == 0) {
		int nfd = rlim.rlim_cur;
		if (nfd == RLIM_INFINITY)
			(void) printf(
			    "  Current rlimit: unlimited file descriptors\n");
		else
			(void) printf(
			    "  Current rlimit: %d file descriptors\n", nfd);
	}

	/* in case we are doing this to ourself */
	pid = (Pr == NULL)? getpid() : Pstatus(Pr)->pr_pid;

	(void) sprintf(pname, "/proc/%d/fd", (int)pid);
	if ((dirp = opendir(pname)) == NULL) {
		(void) fprintf(stderr, "%s: cannot open directory %s\n",
		    command, pname);
		return;
	}

	/* for each open file --- */
	while (dentp = readdir(dirp)) {
		char unknown[12];
		dev_t rdev;

		/* skip '.' and '..' */
		if (!isdigit(dentp->d_name[0]))
			continue;

		fd = atoi(dentp->d_name);
		if (pr_fstat(Pr, fd, &statb) == -1)
			continue;

		rdev = NODEV;
		switch (statb.st_mode & S_IFMT) {
		case S_IFCHR: s = "S_IFCHR"; rdev = statb.st_rdev; break;
		case S_IFBLK: s = "S_IFBLK"; rdev = statb.st_rdev; break;
		case S_IFIFO: s = "S_IFIFO"; break;
		case S_IFDIR: s = "S_IFDIR"; break;
		case S_IFNAM: s = "S_IFNAM"; break;
		case S_IFREG: s = "S_IFREG"; break;
		case S_IFLNK: s = "S_IFLNK"; break;
		case S_IFSOCK: s = "S_IFSOCK"; break;
		case S_IFDOOR: s = "S_IFDOOR"; break;
		default:
			s = unknown;
			(void) sprintf(s, "0x%.4x ",
				(int)statb.st_mode & S_IFMT);
			break;
		}

		(void) printf("%4d: %s mode:0%.3o",
			fd,
			s,
			(int)statb.st_mode & ~S_IFMT);

		if (major(statb.st_dev) != NODEV &&
		    minor(statb.st_dev) != NODEV)
			(void) printf(" dev:%lu,%lu",
				(u_long)major(statb.st_dev),
				(u_long)minor(statb.st_dev));
		else
			(void) printf(" dev:0x%.8lX", (long)statb.st_dev);

		(void) printf(" ino:%lu uid:%d gid:%d",
			statb.st_ino,
			(int)statb.st_uid,
			(int)statb.st_gid);

		if (rdev == NODEV)
			(void) printf(" size:%ld\n", statb.st_size);
		else if (major(rdev) != NODEV && minor(rdev) != NODEV)
			(void) printf(" rdev:%lu,%lu\n",
				(u_long)major(rdev),
				(u_long)minor(rdev));
		else
			(void) printf(" rdev:0x%.8lX\n", (long)rdev);

		dofcntl(Pr, fd,
			(statb.st_mode & (S_IFMT|S_ENFMT|S_IXGRP))
			== (S_IFREG|S_ENFMT),
			(statb.st_mode & S_IFMT) == S_IFDOOR);
	}

	(void) closedir(dirp);
}

/* examine open file with fcntl() */
static void
dofcntl(struct ps_prochandle *Pr, int fd, int manditory, int isdoor)
{
	struct flock flock;
	int fileflags;
	int fdflags;

	fileflags = pr_fcntl(Pr, fd, F_GETFL, 0);
	fdflags = pr_fcntl(Pr, fd, F_GETFD, 0);

	if (fileflags != -1 || fdflags != -1) {
		(void) printf("      ");
		if (fileflags != -1)
			show_fileflags(fileflags);
		if (fdflags != -1 && (fdflags & FD_CLOEXEC))
			(void) printf(" FD_CLOEXEC");
		if (isdoor)
			show_door(Pr, fd);
		(void) fputc('\n', stdout);
	} else if (isdoor) {
		(void) printf("    ");
		show_door(Pr, fd);
		(void) fputc('\n', stdout);
	}

	flock.l_type = F_WRLCK;
	flock.l_whence = 0;
	flock.l_start = 0;
	flock.l_len = 0;
	flock.l_sysid = 0;
	flock.l_pid = 0;
	if (pr_fcntl(Pr, fd, F_GETLK, &flock) != -1) {
		if (flock.l_type != F_UNLCK && (flock.l_sysid || flock.l_pid)) {
			unsigned long sysid = flock.l_sysid;

			(void) printf("      %s %s lock set by",
				manditory? "manditory" : "advisory",
				flock.l_type == F_RDLCK? "read" : "write");
			if (sysid & 0xff000000)
				(void) printf(" system %lu.%lu.%lu.%lu",
					(sysid>>24)&0xff, (sysid>>16)&0xff,
					(sysid>>8)&0xff, (sysid)&0xff);
			else if (sysid)
				(void) printf(" system 0x%lX", sysid);
			if (flock.l_pid)
				(void) printf(" process %d", (int)flock.l_pid);
			(void) fputc('\n', stdout);
		}
	}
}

#ifdef O_PRIV
#define	ALL_O_FLAGS	O_ACCMODE | O_NDELAY | O_NONBLOCK | O_APPEND | \
			O_PRIV | O_SYNC | O_DSYNC | O_RSYNC | \
			O_CREAT | O_TRUNC | O_EXCL | O_NOCTTY | O_LARGEFILE
#else
#define	ALL_O_FLAGS	O_ACCMODE | O_NDELAY | O_NONBLOCK | O_APPEND | \
			O_SYNC | O_DSYNC | O_RSYNC | \
			O_CREAT | O_TRUNC | O_EXCL | O_NOCTTY | O_LARGEFILE
#endif
static void
show_fileflags(int flags)
{
	char buffer[128];
	char *str = buffer;

	switch (flags & O_ACCMODE) {
	case O_RDONLY:
		(void) strcpy(str, "O_RDONLY");
		break;
	case O_WRONLY:
		(void) strcpy(str, "O_WRONLY");
		break;
	case O_RDWR:
		(void) strcpy(str, "O_RDWR");
		break;
	default:
		(void) sprintf(str, "0x%x", flags & O_ACCMODE);
		break;
	}

	if (flags & O_NDELAY)
		(void) strcat(str, "|O_NDELAY");
	if (flags & O_NONBLOCK)
		(void) strcat(str, "|O_NONBLOCK");
	if (flags & O_APPEND)
		(void) strcat(str, "|O_APPEND");
#ifdef O_PRIV
	if (flags & O_PRIV)
		(void) strcat(str, "|O_PRIV");
#endif
	if (flags & O_SYNC)
		(void) strcat(str, "|O_SYNC");
	if (flags & O_DSYNC)
		(void) strcat(str, "|O_DSYNC");
	if (flags & O_RSYNC)
		(void) strcat(str, "|O_RSYNC");
	if (flags & O_CREAT)
		(void) strcat(str, "|O_CREAT");
	if (flags & O_TRUNC)
		(void) strcat(str, "|O_TRUNC");
	if (flags & O_EXCL)
		(void) strcat(str, "|O_EXCL");
	if (flags & O_NOCTTY)
		(void) strcat(str, "|O_NOCTTY");
	if (flags & O_LARGEFILE)
		(void) strcat(str, "|O_LARGEFILE");
	if (flags & ~(ALL_O_FLAGS))
		(void) sprintf(str + strlen(str), "|0x%x",
			flags & ~(ALL_O_FLAGS));

	(void) printf("%s", str);
}

/* show door info */
static void
show_door(struct ps_prochandle *Pr, int fd)
{
	door_info_t door_info;
	psinfo_t psinfo;

	if (pr_door_info(Pr, fd, &door_info) != 0)
		return;

	if (proc_get_psinfo(door_info.di_target, &psinfo) != 0)
		psinfo.pr_fname[0] = '\0';

	(void) printf("  door to ");
	if (psinfo.pr_fname[0] != '\0')
		(void) printf("%s[%d]", psinfo.pr_fname,
			(int)door_info.di_target);
	else
		(void) printf("pid %d", (int)door_info.di_target);
}

/*
 * Convert string into itself, replacing unprintable
 * characters with space along the way.  Stop on a null character.
 */
static void
uncontrol(char *buf, size_t n)
{
	int c;

	while (n-- != 0 && (c = (*buf & 0xff)) != '\0') {
		if (!isprint(c))
			c = ' ';
		*buf++ = (char)c;
	}

	*buf = '\0';
}
