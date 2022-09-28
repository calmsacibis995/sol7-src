/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pwdx.c	1.13	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/stack.h>
#include <elf.h>
#include <libproc.h>

#define	LIBCWD32	"/usr/proc/lib/libcwd.so.1"
#if defined(sparc)
#define	LIBCWD64	"/usr/proc/lib/sparcv9/libcwd.so.1"
#elif defined(i386)
#define	LIBCWD64	"/usr/proc/lib/ia64/libcwd.so.1"
#else
#error "Unrecognized architecture"
#endif

static	int	interrupt;
static	char	*command;
static	int	Fflag;

static void intr(int);
static	int	cwd_self(void);

main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int fd;
	char *library;
	caddr_t ptr1;
	caddr_t ptr2;
	intptr_t cwd;
	intptr_t ill = -16;
	int errflg = 0;
	Elf32_Ehdr Ehdr32;
#if defined(_LP64)
	Elf64_Ehdr Ehdr64;
#endif
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
		(void) fprintf(stderr, "usage:\t%s [-F] pid ...\n", command);
		(void) fprintf(stderr, "  (show process working directory)\n");
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
		pstatus_t *Psp;
		char *arg;
		pid_t pid;
		int gret;
		prgreg_t sp;

		Psp = NULL;
		(void) fflush(stdout);	/* line-at-a-time */

		/* get the specified pid */
		if ((pid = proc_pidarg(arg = *argv++, NULL)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			retc++;
			continue;
		}

		if ((Pr = Pgrab(pid, Fflag, &gret)) == NULL) {
			switch (gret) {
			case G_SYS:
				(void) printf("%d:\t/\t[system process]\n",
					(int)pid);
				break;
			case G_SELF:
				if (cwd_self() != 0)
					retc++;
				break;
			default:
				(void) fprintf(stderr, "%s: %s: %d\n",
					command, Pgrab_error(gret), (int)pid);
				retc++;
				break;
			}
			continue;
		}

		Psp = Pstatus(Pr);

		if (Pcreate_agent(Pr) != 0) {
			(void) fprintf(stderr,
				"%s: cannot control process %d\n",
				command, (int)pid);
			retc++;
			Prelease(Pr, 0);
			continue;
		}

#if i386
		(void) Pgetareg(Pr, R_SP, &sp);
		(void) Pputareg(Pr, R_SP, sp - 2*sizeof (int));
#endif

		if (Psp->pr_dmodel == PR_MODEL_LP64)
			library = LIBCWD64;
		else
			library = LIBCWD32;

		/* make the lwp map in the getcwd library code */
		fd = pr_open(Pr, library, O_RDONLY, 0);
		ptr1 = pr_mmap(Pr, NULL, 8192, PROT_READ|PROT_EXEC,
			MAP_PRIVATE, fd, 0);
		(void) pr_close(Pr, fd);

		ptr2 = pr_zmap(Pr, NULL, 8192, PROT_READ|PROT_WRITE,
			MAP_PRIVATE);

		/* get the ELF header for the entry point */
#if defined(_LP64)
		if (Psp->pr_dmodel == PR_MODEL_LP64) {
			(void) Pread(Pr, &Ehdr64, sizeof (Ehdr64),
				(uintptr_t)ptr1);
			cwd = (intptr_t)(ptr1 + (long)Ehdr64.e_entry);
		} else {
			(void) Pread(Pr, &Ehdr32, sizeof (Ehdr32),
				(uintptr_t)ptr1);
			cwd = (intptr_t)(ptr1 + (long)Ehdr32.e_entry);
		}
#else
		(void) Pread(Pr, &Ehdr32, sizeof (Ehdr32), (uintptr_t)ptr1);
		cwd = (intptr_t)(ptr1 + (long)Ehdr32.e_entry);
#endif

		/* arrange to call (*cwd)() */
#if i386
		(void) Pgetareg(Pr, R_SP, &sp);
		(void) Pwrite(Pr, (char *)&ptr2, 4, (uintptr_t)(sp+4));
		(void) Pwrite(Pr, (char *)&ill, 4, (uintptr_t)sp);
		(void) Pputareg(Pr, R_PC, (prgreg_t)cwd);
#elif sparc
#if defined(_LP64)
		if (Psp->pr_dmodel == PR_MODEL_LP64) {
			(void) Pputareg(Pr, R_O0, (prgreg_t)ptr2);
			(void) Pputareg(Pr, R_O7, (prgreg_t)ill);
			(void) Pputareg(Pr, R_PC, (prgreg_t)cwd);
			(void) Pputareg(Pr, R_nPC, (prgreg_t)cwd + 4);
			(void) Pgetareg(Pr, R_SP, &sp);
			(void) Pputareg(Pr, R_SP, sp - SA64(MINFRAME64));
		} else {
			(void) Pputareg(Pr, R_O0, (prgreg_t)(uint32_t)ptr2);
			(void) Pputareg(Pr, R_O7, (prgreg_t)(uint32_t)ill);
			(void) Pputareg(Pr, R_PC, (prgreg_t)(uint32_t)cwd);
			(void) Pputareg(Pr, R_nPC, (prgreg_t)(uint32_t)cwd + 4);
			(void) Pgetareg(Pr, R_SP, &sp);
			(void) Pputareg(Pr, R_SP, sp - SA32(MINFRAME32));
		}
#else
		(void) Pputareg(Pr, R_O0, (prgreg_t)ptr2);
		(void) Pputareg(Pr, R_O7, (prgreg_t)ill);
		(void) Pputareg(Pr, R_PC, (prgreg_t)cwd);
		(void) Pputareg(Pr, R_nPC, (prgreg_t)cwd + 4);
		(void) Pgetareg(Pr, R_SP, &sp);
		(void) Pputareg(Pr, R_SP, sp - SA(MINFRAME));
#endif
#else
		"unknown architecture"
#endif
		(void) Pfault(Pr, FLTBOUNDS, 1);
		(void) Pfault(Pr, FLTILL, 1);

		if (Psetrun(Pr, 0, 0) == 0 &&
		    Pwait(Pr, 0) == 0 &&
		    Pstate(Pr) == PS_STOP &&
		    Psp->pr_lwp.pr_why == PR_FAULTED) {
			prgreg_t addr;
			char dir[PATH_MAX];

			if (Psp->pr_lwp.pr_what != FLTBOUNDS)
				(void) printf("%d:\texpected FLTBOUNDS\n",
					(int)Psp->pr_pid);
			(void) Pclearfault(Pr);
			(void) Pgetareg(Pr, R_RVAL, &addr);
			if (addr == 0 ||
			    proc_read_string(Pasfd(Pr), dir,
			    sizeof (dir), (off_t)addr) < 0)
				(void) printf("%d:\t???\n",
					(int)Psp->pr_pid);
			else
				(void) printf("%d:\t%s\n",
					(int)Psp->pr_pid, dir);
		}

		(void) Pfault(Pr, FLTILL, 0);
		(void) Pfault(Pr, FLTBOUNDS, 0);
		(void) pr_munmap(Pr, ptr1, 8192);
		(void) pr_munmap(Pr, ptr2, 8192);

		Pdestroy_agent(Pr);
		Prelease(Pr, 0);
	}

	if (interrupt)
		retc++;
	return (retc);
}

static int
cwd_self()
{
	int fd;
	char *ptr1;
	char *ptr2;
	char *(*cwd)();
	int rv = 0;
	char *library;

#if defined(_LP64)
	library = LIBCWD64;
#else
	library = LIBCWD32;
#endif
	fd = open(library, O_RDONLY, 0);
	ptr1 = mmap((caddr_t)0, 8192, PROT_READ|PROT_EXEC,
		MAP_PRIVATE, fd, 0);
	(void) close(fd);
	if (ptr1 == NULL) {
		perror(library);
		rv = -1;
	}

	fd = open("/dev/zero", O_RDONLY, 0);
	ptr2 = mmap((caddr_t)0, 8192, PROT_READ|PROT_WRITE,
		MAP_PRIVATE, fd, 0);
	(void) close(fd);
	if (ptr2 == NULL) {
		perror("/dev/zero");
		rv = -1;
	}

#if defined(_LP64)
	/* LINTED improper alignment */
	cwd = (char *(*)())(ptr1 + (long)((Elf64_Ehdr *)ptr1)->e_entry);
#else
	/* LINTED improper alignment */
	cwd = (char *(*)())(ptr1 + (long)((Elf32_Ehdr *)ptr1)->e_entry);
#endif

	/* arrange to call (*cwd)() */
	if (ptr1 != NULL && ptr2 != NULL) {
		char *dir = (*cwd)(ptr2);
		if (dir == NULL)
			(void) printf("%d:\t???\n", (int)getpid());
		else
			(void) printf("%d:\t%s\n", (int)getpid(), dir);
	}

	(void) munmap(ptr1, 8192);
	(void) munmap(ptr2, 8192);

	return (rv);
}

/* ARGSUSED */
static void
intr(int sig)
{
	interrupt = 1;
}
