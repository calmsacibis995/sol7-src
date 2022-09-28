/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pstack.c	1.12	98/01/30 SMI"

#include <sys/isa_defs.h>

#ifdef _LP64
#define	_SYSCALL32
#endif

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stack.h>
#include <link.h>
#include <libelf.h>
#include <thread_db.h>
#include <libproc.h>

static	char	*command;
static	int	Fflag;
static	int	is64;

/*
 * To keep the list of user-level threads for a multithreaded process.
 */
struct threadinfo {
	struct threadinfo *next;
	id_t	threadid;
	id_t	lwpid;
	prgregset_t regs;
};

static struct threadinfo *thr_head, *thr_tail;

#define	TRUE	1
#define	FALSE	0

#define	MAX_ARGS	8

static	int	thr_stack(const td_thrhandle_t *, void *);
static	void	free_threadinfo(void);
static	id_t	find_thread(id_t);
static	int	AllCallStacks(struct ps_prochandle *, pid_t, int);
static	void	tlhead(id_t, id_t);
static	int	PrintFrame(void *, uintptr_t, u_int, const long *);
static	void	PrintSyscall(lwpstatus_t *, prgregset_t);
static	void	adjust_leaf_frame(struct ps_prochandle *, prgregset_t);
static	void	CallStack(struct ps_prochandle *, lwpstatus_t *);
static	void	uncontrol(char *, size_t);

main(int argc, char **argv)
{
	int retc = 0;
	int opt;
	int errflg = FALSE;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "F")) != EOF) {
		switch (opt) {
		case 'F':
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = TRUE;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s [-F] pid ...\n", command);
		(void) fprintf(stderr, "  (show process call stack)\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		exit(2);
	}

	while (--argc >= 0) {
		char *arg;
		pid_t pid;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		/* get the specified pid and its psinfo struct */
		if ((pid = proc_pidarg(arg = *argv++, &psinfo)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			retc++;
			continue;
		}
		if ((Pr = Pgrab(pid, Fflag, &gcode)) == NULL) {
			(void) fprintf(stderr, "%s: cannot grab %d: %s\n",
				command, (int)pid, Pgrab_error(gcode));
			retc++;
			continue;
		}
		uncontrol(psinfo.pr_psargs, PRARGSZ);
		(void) printf("%d:\t%.70s\n", (int)pid, psinfo.pr_psargs);

		is64 = (psinfo.pr_dmodel == PR_MODEL_LP64);

		if (Pstatus(Pr)->pr_nlwp <= 1) {
			if (AllCallStacks(Pr, pid,  FALSE) != 0)
				retc++;
		} else {
			int libthread;
			td_thragent_t *Tap;

			/*
			 * First we need to get a thread agent handle.
			 */
			(void) td_init();
			if (td_ta_new(Pr, &Tap) != TD_OK) /* not libthread? */
				libthread = FALSE;
			else {
				/*
				 * Iterate over all threads, calling:
				 *   thr_stack(td_thrhandle_t *Thp, NULL);
				 * for each one to generate the list of threads.
				 */
				(void) td_ta_thr_iter(Tap, thr_stack, NULL,
				    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY,
				    TD_SIGNO_MASK, TD_THR_ANY_USER_FLAGS);

				(void) td_ta_delete(Tap);
				libthread = TRUE;
			}
			if (AllCallStacks(Pr, pid, libthread) != 0)
				retc++;
			if (libthread)
				free_threadinfo();
		}

		Prelease(Pr, 0);
	}

	return (retc);
}

/*
 * Thread iteration call-back function.
 * Called once for each user-level thread.
 * Used to build the list of all threads.
 */
/* ARGSUSED1 */
static int
thr_stack(const td_thrhandle_t *Thp, void *cd)
{
	td_thrinfo_t thrinfo;
	struct threadinfo *tip;

	if (td_thr_get_info(Thp, &thrinfo) != TD_OK)
		return (0);

	tip = malloc(sizeof (struct threadinfo));
	tip->next = NULL;
	tip->threadid = thrinfo.ti_tid;
	tip->lwpid = thrinfo.ti_lid;

	(void) memset(tip->regs, 0, sizeof (prgregset_t));
	(void) td_thr_getgregs(Thp, tip->regs);

	if (thr_tail)
		thr_tail->next = tip;
	else
		thr_head = tip;
	thr_tail = tip;

	return (0);
}

static void
free_threadinfo()
{
	struct threadinfo *tip = thr_head;
	struct threadinfo *next;

	while (tip) {
		next = tip->next;
		free(tip);
		tip = next;
	}

	thr_head = thr_tail = NULL;
}

/*
 * Find and eliminate the thread corresponding to the given lwpid.
 */
static id_t
find_thread(id_t lwpid)
{
	struct threadinfo *tip;
	id_t threadid;

	for (tip = thr_head; tip; tip = tip->next) {
		if (lwpid == tip->lwpid) {
			threadid = tip->threadid;
			tip->threadid = 0;
			tip->lwpid = 0;
			return (threadid);
		}
	}
	return (0);
}

static int
AllCallStacks(struct ps_prochandle *Pr, pid_t pid, int dothreads)
{
	pstatus_t status;

	status = *Pstatus(Pr);

	if (status.pr_nlwp <= 1)
		CallStack(Pr, &status.pr_lwp);
	else {
		char lwpdirname[100];
		lwpstatus_t lwpstatus;
		struct threadinfo *tip;
		int lwpfd;
		id_t tid;
		char *lp;
		DIR *dirp;
		struct dirent *dentp;

		(void) sprintf(lwpdirname, "/proc/%d/lwp", (int)pid);
		if ((dirp = opendir(lwpdirname)) == NULL) {
			perror("AllCallStacks(): opendir(lwp)");
			return (-1);
		}
		lp = lwpdirname + strlen(lwpdirname);
		*lp++ = '/';

		/* for each lwp */
		while (dentp = readdir(dirp)) {
			if (dentp->d_name[0] == '.')
				continue;
			(void) strcpy(lp, dentp->d_name);
			(void) strcat(lp, "/lwpstatus");
			if ((lwpfd = open(lwpdirname, O_RDONLY)) < 0) {
				perror("AllCallStacks(): open lwpstatus");
				break;
			} else if (pread(lwpfd, &lwpstatus, sizeof (lwpstatus),
			    (off_t)0) != sizeof (lwpstatus)) {
				perror("AllCallStacks(): read lwpstatus");
				(void) close(lwpfd);
				break;
			}
			(void) close(lwpfd);
			/*
			 * Find the corresponding thread, if any.
			 */
			if (dothreads)
				tid = find_thread(lwpstatus.pr_lwpid);
			else
				tid = 0;
			tlhead(tid, lwpstatus.pr_lwpid);
			CallStack(Pr, &lwpstatus);
		}
		(void) closedir(dirp);

		/* for each remaining thread w/o an lwp */
		(void) memset(&lwpstatus, 0, sizeof (lwpstatus));
		for (tip = thr_head; tip; tip = tip->next) {
			if ((tid = tip->threadid) != 0) {
				(void) memcpy(lwpstatus.pr_reg, tip->regs,
					sizeof (prgregset_t));
				tlhead(tid, tip->lwpid);
				CallStack(Pr, &lwpstatus);
			}
			tip->threadid = 0;
			tip->lwpid = 0;
		}
	}
	return (0);
}

static void
tlhead(id_t threadid, id_t lwpid)
{
	if (threadid == 0 && lwpid == 0)
		return;

	(void) printf("-----------------");

	if (threadid && lwpid)
		(void) printf("  lwp# %d / thread# %d  ",
			(int)lwpid, (int)threadid);
	else if (threadid)
		(void) printf("---------  thread# %d  ", (int)threadid);
	else if (lwpid)
		(void) printf("  lwp# %d  ------------", (int)lwpid);

	(void) printf("--------------------\n");
}

static int
PrintFrame(void *cd, uintptr_t pc, u_int argc, const long *argv)
{
	struct ps_prochandle *Pr = (struct ps_prochandle *)cd;
	char buff[255];
	GElf_Sym sym;
	uintptr_t start;
	int length = (is64? 16 : 8);
	int i;

	(void) sprintf(buff, "%.*lx", length, (long)pc);
	(void) strcpy(buff + length, " ????????");
	if (proc_lookup_by_addr(Pr, (uintptr_t)pc,
	    buff + 1 + length, sizeof (buff) - 1 - length, &sym) == 0)
		start = sym.st_value;
	else
		start = pc;

	(void) printf(" %-17s (", buff);
	for (i = 0; i < argc && i < MAX_ARGS; i++)
		(void) printf((i+1 == argc)? "%lx" : "%lx, ",
			argv[i]);
	if (i != argc)
		(void) printf("...");
	(void) printf((start != pc)?
		") + %lx\n" : ")\n", (long)(pc - start));

	return (0);
}

static void
PrintSyscall(lwpstatus_t *psp, prgregset_t reg)
{
	char sname[32];
	int length = (is64? 16 : 8);
	u_int i;

	(void) proc_sysname(psp->pr_syscall, sname, sizeof (sname));
	(void) printf(" %.*lx %-8s (", length, (long)reg[R_PC], sname);
	for (i = 0; i < psp->pr_nsysarg; i++)
		(void) printf((i+1 == psp->pr_nsysarg)? "%lx" : "%lx, ",
			(long)psp->pr_sysarg[i]);
	(void) printf(")\n");
}

/* ARGSUSED */
static void
adjust_leaf_frame(struct ps_prochandle *Pr, prgregset_t reg)
{
#if defined(sparc) || defined(__sparc)
	if (is64)
		reg[R_PC] = reg[R_O7];
	else
		reg[R_PC] = (uint32_t)reg[R_O7];
	reg[R_nPC] = reg[R_PC] + 4;
#elif defined(i386) || defined(__i386)
	(void) Pread(Pr, &reg[R_PC], sizeof (prgreg_t), (long)reg[R_SP]);
	reg[R_SP] += 4;
#endif
}

static void
CallStack(struct ps_prochandle *Pr, lwpstatus_t *psp)
{
	prgregset_t reg;

	(void) memcpy(reg, psp->pr_reg, sizeof (reg));

	if (psp->pr_flags & (PR_ASLEEP|PR_VFORKP)) {
		PrintSyscall(psp, reg);
		adjust_leaf_frame(Pr, reg);
	}

	(void) proc_stack_iter(Pr, reg, PrintFrame, Pr);
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
