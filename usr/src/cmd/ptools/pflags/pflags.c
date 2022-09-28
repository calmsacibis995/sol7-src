/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pflags.c	1.9	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <libproc.h>

static	int	look(char *, int);
static	void	lwplook(lwpstatus_t *, int, int);
static	void	uncontrol(char *, size_t);
static	int	perr(const char *);
static	char	*prflags(int);
static	char	*prwhy(int);
static	char	*prwhat(int, int);
static	void	dumpregs(prgregset_t, int);

static	char	*command;
static	char	*procname;

#define	LWPFLAGS	\
	(PR_STOPPED|PR_ISTOP|PR_DSTOP|PR_ASLEEP|PR_PCINVAL|PR_STEP \
	|PR_ASLWP|PR_AGENT)

#define	PROCFLAGS	\
	(PR_ISSYS|PR_VFORKP|PR_ORPHAN|PR_FORK|PR_RLC|PR_KLC|PR_ASYNC \
	|PR_BPTADJ|PR_MSACCT|PR_MSFORK|PR_PTRACE)

#define	ALLFLAGS	(LWPFLAGS|PROCFLAGS)

main(int argc, char **argv)
{
	int rc = 0;
	int errflg = 0;
	int rflag = 0;
	int opt;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "r")) != EOF) {
		switch (opt) {
		case 'r':		/* show registers */
			rflag = 1;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s pid [-r] ...\n", command);
		(void) fprintf(stderr, "  (report process status flags)\n");
		(void) fprintf(stderr, "  -r : report registers\n");
		return (2);
	}

	while (argc-- > 0)
		rc += look(*argv++, rflag);

	return (rc);
}

static int
look(char *arg, int rflag)
{
	pid_t pid;
	pstatus_t pstatus;
	psinfo_t psinfo;
	int flags;
	sigset_t sigmask;
	fltset_t fltmask;
	sysset_t entryset;
	sysset_t exitset;
	uint32_t sigtrace, sigtrace2, fltbits;
	uint32_t sigpend, sigpend2;
	uint32_t *bits;
	char lwpdirname[100];
	char *lp;
	DIR *dirp;
	struct dirent *dentp;
	int is64;

	procname = arg;		/* for perr() */
	if ((pid = proc_pidarg(arg, &psinfo)) < 0) {
		(void) fprintf(stderr, "%s: no such process: %s\n",
			command, arg);
		return (1);
	}
	if (psinfo.pr_nlwp == 0) {
		(void) printf("%d:\t<defunct>\n", (int)pid);
		return (0);
	}
	if (proc_get_status(pid, &pstatus) < 0)
		return (perr("pstatus"));

	is64 = (pstatus.pr_dmodel == PR_MODEL_LP64);

	sigmask = pstatus.pr_sigtrace;
	fltmask = pstatus.pr_flttrace;
	entryset = pstatus.pr_sysentry;
	exitset = pstatus.pr_sysexit;

	uncontrol(psinfo.pr_psargs, PRARGSZ);
	(void) printf("%d:\t%.70s\n", (int)pid,
		psinfo.pr_psargs);
	(void) printf("\tdata model = %s", is64? "_LP64" : "_ILP32");
	if ((flags = (pstatus.pr_flags & PROCFLAGS)) != 0)
		(void) printf("  flags = %s", prflags(flags));
	(void) printf("\n");

	sigtrace = *((uint32_t *)&sigmask);
	sigtrace2 = *((uint32_t *)&sigmask + 1);
	fltbits = *((uint32_t *)&fltmask);
	if (sigtrace || sigtrace2 || fltbits) {
		if (fltbits)
			(void) printf("  flttrace = 0x%.8x\n", fltbits);
		if (sigtrace || sigtrace2)
			(void) printf("  sigtrace = 0x%.8x 0x%.8x\n",
				sigtrace, sigtrace2);
	}

	bits = ((uint32_t *)&entryset);
	if (bits[0] | bits[1] | bits[2] | bits[3] |
	    bits[4] | bits[5] | bits[6] | bits[7])
		(void) printf("  entryset = "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n             "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			bits[0], bits[1], bits[2], bits[3],
			bits[4], bits[5], bits[6], bits[7]);

	bits = ((uint32_t *)&exitset);
	if (bits[0] | bits[1] | bits[2] | bits[3] |
	    bits[4] | bits[5] | bits[6] | bits[7])
		(void) printf("  exitset  = "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n             "
			"0x%.8x 0x%.8x 0x%.8x 0x%.8x\n",
			bits[0], bits[1], bits[2], bits[3],
			bits[4], bits[5], bits[6], bits[7]);

	sigpend  = *((uint32_t *)&pstatus.pr_sigpend);
	sigpend2 = *((uint32_t *)&pstatus.pr_sigpend + 1);
	if (sigpend || sigpend2)
		(void) printf("  sigpend = 0x%.8x,0x%.8x\n",
			sigpend, sigpend2);

	(void) sprintf(lwpdirname, "/proc/%d/lwp", (int)pid);
	if ((dirp = opendir(lwpdirname)) == NULL)
		return (0);
	lp = lwpdirname + strlen(lwpdirname);
	*lp++ = '/';

	/* for each lwp */
	while (dentp = readdir(dirp)) {
		int lwpfd;
		lwpstatus_t lwpstatus;

		if (dentp->d_name[0] == '.')
			continue;
		(void) strcpy(lp, dentp->d_name);
		(void) strcat(lp, "/lwpstatus");
		if ((lwpfd = open(lwpdirname, O_RDONLY)) < 0) {
			(void) perr("lwpdirname");
		} else if (read(lwpfd, (char *)&lwpstatus, sizeof (lwpstatus))
		    < 0) {
			(void) close(lwpfd);
			(void) perr("read lwpstatus");
		} else {
			(void) close(lwpfd);
			flags = (lwpstatus.pr_flags & LWPFLAGS);
			lwplook(&lwpstatus, flags, pstatus.pr_flags & PR_ISSYS);
			if (rflag) {
				if (pstatus.pr_flags & PR_STOPPED)
					dumpregs(lwpstatus.pr_reg, is64);
				else
					(void) printf("   Not stopped,"
						" can't show registers\n");
			}
		}
	}
	(void) closedir(dirp);

	return (0);
}

static void
lwplook(lwpstatus_t *psp, int flags, int issys)
{
	uint32_t sighold, sighold2;
	uint32_t sigpend, sigpend2;
	int cursig;
	char buf[32];

	(void) printf("  /%d:\tflags = %s", (int)psp->pr_lwpid, prflags(flags));
	if ((flags & PR_ASLEEP) || (psp->pr_syscall && !issys)) {
		if (flags & PR_ASLEEP) {
			if ((flags & ~PR_ASLEEP) != 0)
				(void) printf("|");
			(void) printf("PR_ASLEEP");
		}
		if (psp->pr_syscall && !issys) {
			u_int i;

			(void) printf(" [ %s(",
			    proc_sysname(psp->pr_syscall, buf, sizeof (buf)));
			for (i = 0; i < psp->pr_nsysarg; i++) {
				if (i != 0)
					(void) printf(",");
				(void) printf("0x%lx", psp->pr_sysarg[i]);
			}
			(void) printf(") ]");
		}
	}
	(void) printf("\n");

	if (flags & PR_STOPPED) {
		(void) printf("  why = %s", prwhy(psp->pr_why));
		if (psp->pr_why != PR_REQUESTED &&
		    psp->pr_why != PR_SUSPENDED)
			(void) printf("  what = %s",
				prwhat(psp->pr_why, psp->pr_what));
		(void) printf("\n");
	}

	sighold  = *((uint32_t *)&psp->pr_lwphold);
	sighold2 = *((uint32_t *)&psp->pr_lwphold + 1);
	sigpend  = *((uint32_t *)&psp->pr_lwppend);
	sigpend2 = *((uint32_t *)&psp->pr_lwppend + 1);
	cursig   = psp->pr_cursig;

	if (sighold || sighold2 || sigpend || sigpend2 || cursig) {
		if (sighold || sighold2)
			(void) printf("  sigmask = 0x%.8x,0x%.8x",
				sighold, sighold2);
		if (sigpend || sigpend2)
			(void) printf("  lwppend = 0x%.8x,0x%.8x",
				sigpend, sigpend2);
		if (cursig)
			(void) printf("  cursig = %s",
			    proc_signame(cursig, buf, sizeof (buf)));
		(void) printf("\n");
	}
}

static int
perr(const char *s)
{
	if (s == NULL || errno != ENOENT) {
		if (s)
			(void) fprintf(stderr, "%s: ", procname);
		else
			s = procname;
		perror(s);
	}
	return (1);
}

static char *
prflags(int arg)
{
	static char code_buf[100];
	char *str = code_buf;

	if (arg == 0)
		return ("0");

	if (arg & ~ALLFLAGS)
		(void) sprintf(str, "0x%x", arg & ~ALLFLAGS);
	else
		*str = '\0';

	if (arg & PR_STOPPED)
		(void) strcat(str, "|PR_STOPPED");
	if (arg & PR_ISTOP)
		(void) strcat(str, "|PR_ISTOP");
	if (arg & PR_DSTOP)
		(void) strcat(str, "|PR_DSTOP");
#if 0		/* displayed elsewhere */
	if (arg & PR_ASLEEP)
		(void) strcat(str, "|PR_ASLEEP");
#endif
	if (arg & PR_PCINVAL)
		(void) strcat(str, "|PR_PCINVAL");
	if (arg & PR_STEP)
		(void) strcat(str, "|PR_STEP");
	if (arg & PR_ASLWP)
		(void) strcat(str, "|PR_ASLWP");
	if (arg & PR_AGENT)
		(void) strcat(str, "|PR_AGENT");
	if (arg & PR_ISSYS)
		(void) strcat(str, "|PR_ISSYS");
	if (arg & PR_VFORKP)
		(void) strcat(str, "|PR_VFORKP");
	if (arg & PR_ORPHAN)
		(void) strcat(str, "|PR_ORPHAN");
	if (arg & PR_FORK)
		(void) strcat(str, "|PR_FORK");
	if (arg & PR_RLC)
		(void) strcat(str, "|PR_RLC");
	if (arg & PR_KLC)
		(void) strcat(str, "|PR_KLC");
	if (arg & PR_ASYNC)
		(void) strcat(str, "|PR_ASYNC");
	if (arg & PR_BPTADJ)
		(void) strcat(str, "|PR_BPTADJ");
	if (arg & PR_MSACCT)
		(void) strcat(str, "|PR_MSACCT");
	if (arg & PR_MSFORK)
		(void) strcat(str, "|PR_MSFORK");
	if (arg & PR_PTRACE)
		(void) strcat(str, "|PR_PTRACE");

	if (*str == '|')
		str++;

	return (str);
}

static char *
prwhy(int why)
{
	static char buf[20];
	char *str;

	switch (why) {
	case PR_REQUESTED:
		str = "PR_REQUESTED";
		break;
	case PR_SIGNALLED:
		str = "PR_SIGNALLED";
		break;
	case PR_SYSENTRY:
		str = "PR_SYSENTRY";
		break;
	case PR_SYSEXIT:
		str = "PR_SYSEXIT";
		break;
	case PR_JOBCONTROL:
		str = "PR_JOBCONTROL";
		break;
	case PR_FAULTED:
		str = "PR_FAULTED";
		break;
	case PR_SUSPENDED:
		str = "PR_SUSPENDED";
		break;
	default:
		str = buf;
		(void) sprintf(str, "%d", why);
		break;
	}

	return (str);
}

static char *
prwhat(int why, int what)
{
	static char buf[32];
	char *str;

	switch (why) {
	case PR_SIGNALLED:
	case PR_JOBCONTROL:
		str = proc_signame(what, buf, sizeof (buf));
		break;
	case PR_SYSENTRY:
	case PR_SYSEXIT:
		str = proc_sysname(what, buf, sizeof (buf));
		break;
	case PR_FAULTED:
		str = proc_fltname(what, buf, sizeof (buf));
		break;
	default:
		(void) sprintf(str = buf, "%d", what);
		break;
	}

	return (str);
}

#if defined(sparc)
static const char * const regname[NPRGREG] = {
	" %g0", " %g1", " %g2", " %g3", " %g4", " %g5", " %g6", " %g7",
	" %o0", " %o1", " %o2", " %o3", " %o4", " %o5", " %sp", " %o7",
	" %l0", " %l1", " %l2", " %l3", " %l4", " %l5", " %l6", " %l7",
	" %i0", " %i1", " %i2", " %i3", " %i4", " %i5", " %fp", " %i7",
#ifdef __sparcv9
	"%ccr", " %pc", "%npc", "  %y", "%asi", "%fprs"
#else
	"%psr", " %pc", "%npc", "  %y", "%wim", "%tbr"
#endif
};
#endif	/* sparc */

#if defined(i386)
static const char * const regname[NPRGREG] = {
	" %gs", " %fs", " %es", " %ds", "%edi", "%esi", "%ebp", "%esp",
	"%ebx", "%edx", "%ecx", "%eax", "%trapno", "%err", "%eip", " %cs",
	"%efl", "%uesp", " %ss"
};
#endif /* i386 */

static void
dumpregs(prgregset_t reg, int is64)
{
	int width = is64? 16 : 8;
	int cols = is64? 2 : 4;
	int i;

	for (i = 0; i < NPRGREG; i++) {
		(void) printf("  %s = 0x%.*lX",
			regname[i], width, (long)reg[i]);
		if ((i+1) % cols == 0)
			(void) putchar('\n');
	}
	if (i % cols != 0)
		(void) putchar('\n');
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
