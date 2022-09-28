/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pmap.c	1.12	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <link.h>
#include <libelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <libproc.h>

/* obsolete flags */
#ifndef	MA_BREAK
#define	MA_BREAK	0
#endif
#ifndef	MA_STACK
#define	MA_STACK	0
#endif

static	int	look_map(struct ps_prochandle *);
static	int	look_xmap(struct ps_prochandle *);
static	void	uncontrol(char *, size_t);
static	int	perr(char *);
static	char	*mflags(u_int);

static	int	reserved = 0;
static	int	do_xmap = 0;
static	int	lflag = 0;

static	char	*command;
static	char	*procname;

main(int argc, char **argv)
{
	int errflg = 0;
	int rc = 0;
	int opt;
	int Fflag = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "rxlF")) != EOF) {
		switch (opt) {
		case 'r':		/* show reserved mappings */
			reserved = 1;
			break;
		case 'x':		/* show extended mappings */
			do_xmap = 1;
			break;
		case 'l':		/* show unresolved link map names */
			lflag = 1;
			break;
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

	if (do_xmap && reserved) {
		(void) fprintf(stderr, "%s: -r and -x are mutually exclusive\n",
			command);
		return (2);
	}

	if (errflg || argc <= 0) {
		(void) fprintf(stderr, "usage:\t%s [-rxlF] pid ...\n",
			command);
		(void) fprintf(stderr,
			"  (report process address maps)\n");
		(void) fprintf(stderr,
			"  -r: show reserved address maps\n");
		(void) fprintf(stderr,
			"  -x: show resident/shared/private mapping details\n");
		(void) fprintf(stderr,
			"  -l: show unresolved dynamic linker map names\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		return (2);
	}

	while (argc-- > 0) {
		char *arg;
		pid_t pid;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		/* get the specified pid and its psinfo struct */
		if ((pid = proc_pidarg(arg = *argv++, &psinfo)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			rc++;
			continue;
		}
		if ((Pr = Pgrab(pid, PGRAB_RETAIN|Fflag, &gcode))
		    == NULL) {
			(void) fprintf(stderr, "%s: cannot grab %d: %s\n",
				command, (int)pid, Pgrab_error(gcode));
			rc++;
			continue;
		}
		uncontrol(psinfo.pr_psargs, PRARGSZ);
		(void) printf("%d:\t%.70s\n", (int)pid, psinfo.pr_psargs);

		procname = arg;		/* for perr() */

		rc += do_xmap? look_xmap(Pr) : look_map(Pr);

		Prelease(Pr, 0);
	}

	return (rc);
}

static char *
make_name(struct ps_prochandle *Pr, uintptr_t addr, char *mapname,
	char *buf, size_t bufsz)
{
	pstatus_t *Psp = Pstatus(Pr);
	char fname[100];
	struct stat statb;
	int len;

	if (!lflag && strcmp(mapname, "a.out") == 0 &&
	    proc_execname(Pr, buf, bufsz) != NULL)
		return (buf);

	if (proc_objname(Pr, addr, buf, bufsz) != NULL) {
		if (lflag)
			return (buf);
		if ((len = resolvepath(buf, buf, bufsz)) > 0) {
			buf[len] = '\0';
			return (buf);
		}
	}

	if (*mapname != '\0') {
		(void) sprintf(fname, "/proc/%d/object/%s",
			(int)Psp->pr_pid, mapname);
		if (stat(fname, &statb) == 0) {
			dev_t dev = statb.st_dev;
			ino_t ino = statb.st_ino;
			(void) snprintf(buf, bufsz, "dev:%lu,%lu ino:%lu",
				(u_long)major(dev), (u_long)minor(dev), ino);
			return (buf);
		}
	}

	return (NULL);
}

static int
look_map(struct ps_prochandle *Pr)
{
	char mapname[PATH_MAX];
	pstatus_t *Psp = Pstatus(Pr);
	int mapfd;
	struct stat statb;
	prmap_t *prmapp, *pmp;
	int nmap;
	ssize_t n;
	int i;
	u_long total;
	int addr_width;

	if (Psp->pr_flags & PR_ISSYS)
		return (0);

	(void) sprintf(mapname, "/proc/%d/%s", (int)Psp->pr_pid,
		reserved? "rmap" : "map");
	if ((mapfd = open(mapname, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0) {
		if (mapfd >= 0)
			(void) close(mapfd);
		return (perr(mapname));
	}
	nmap = statb.st_size / sizeof (prmap_t);
	prmapp = malloc((nmap + 1) * sizeof (prmap_t));

	if ((n = pread(mapfd, prmapp, (nmap+1) * sizeof (prmap_t), 0L)) < 0) {
		(void) close(mapfd);
		free(prmapp);
		return (perr("read map"));
	}
	(void) close(mapfd);
	nmap = n / sizeof (prmap_t);

	addr_width = (Psp->pr_dmodel == PR_MODEL_LP64)? 16 : 8;

	total = 0;
	for (i = 0, pmp = prmapp; i < nmap; i++, pmp++) {
		char *lname = NULL;
		size_t size = (pmp->pr_size + 1023) / 1024;

		lname = make_name(Pr, pmp->pr_vaddr, pmp->pr_mapname,
			mapname, sizeof (mapname));

		if (lname == NULL && (pmp->pr_mflags & MA_ANON)) {
			if ((pmp->pr_mflags & MA_SHARED) && pmp->pr_shmid != -1)
				(void) sprintf(lname = mapname,
				    " [ shmid=0x%x ]", pmp->pr_shmid);
			else if (pmp->pr_vaddr + pmp->pr_size >
			    Psp->pr_stkbase &&
			    pmp->pr_vaddr < Psp->pr_stkbase + Psp->pr_stksize)
				lname = "  [ stack ]";
			else if (pmp->pr_vaddr + pmp->pr_size >
			    Psp->pr_brkbase &&
			    pmp->pr_vaddr < Psp->pr_brkbase + Psp->pr_brksize)
				lname = "  [ heap ]";
			else
				lname = "  [ anon ]";
		}

		(void) printf(
			lname?
			    "%.*lX %6ldK %-17s %s\n" :
			    "%.*lX %6ldK %s\n",
			addr_width,
			(uintptr_t)pmp->pr_vaddr, size,
			mflags(pmp->pr_mflags & ~(MA_ANON|MA_BREAK|MA_STACK)),
			lname);

		total += size;
	}
	(void) printf(" %stotal %8luK\n",
	    (addr_width == 16) ? "        " : "", total);

	free(prmapp);

	return (0);
}

static void
printK(long value)
{
	if (value == 0)
		(void) printf("       -");
	else
		(void) printf(" %7ld", value);
}

static int
look_xmap(struct ps_prochandle *Pr)
{
	char mapname[PATH_MAX];
	pstatus_t *Psp = Pstatus(Pr);
	int mapfd;
	struct stat statb;
	prxmap_t *prmapp, *pmp;
	int nmap;
	ssize_t n;
	int i;
	u_long total_size = 0;
	u_long total_res = 0;
	u_long total_shared = 0;
	u_long total_priv = 0;
	int addr_width;

	(void) printf(
"Address   Kbytes Resident Shared Private Permissions       Mapped File\n");

	if (Psp->pr_flags & PR_ISSYS)
		return (0);

	(void) sprintf(mapname, "/proc/%d/xmap", (int)Psp->pr_pid);
	if ((mapfd = open(mapname, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0) {
		if (mapfd >= 0)
			(void) close(mapfd);
		return (perr(mapname));
	}
	nmap = statb.st_size / sizeof (prxmap_t);
	prmapp = malloc((nmap + 1) * sizeof (prxmap_t));

	if ((n = pread(mapfd, prmapp, (nmap+1) * sizeof (prxmap_t), 0L)) < 0) {
		(void) close(mapfd);
		return (perr("read xmap"));
	}
	(void) close(mapfd);
	nmap = n / sizeof (prxmap_t);

	addr_width = (Psp->pr_dmodel == PR_MODEL_LP64)? 16 : 8;

	for (i = 0, pmp = prmapp; i < nmap; i++, pmp++) {
		char *lname = NULL;
		char *ln;

		lname = make_name(Pr, pmp->pr_vaddr, pmp->pr_mapname,
			mapname, sizeof (mapname));

		if (lname != NULL) {
			if ((ln = strrchr(lname, '/')) != NULL)
				lname = ln + 1;
		} else if (pmp->pr_mflags & MA_ANON) {
			if ((pmp->pr_mflags & MA_SHARED) && pmp->pr_shmid != -1)
				(void) sprintf(lname = mapname,
				    " [shmid=0x%x]", pmp->pr_shmid);
			else if (pmp->pr_vaddr + pmp->pr_size >
			    Psp->pr_stkbase &&
			    pmp->pr_vaddr < Psp->pr_stkbase + Psp->pr_stksize)
				lname = " [ stack ]";
			else if (pmp->pr_vaddr + pmp->pr_size >
			    Psp->pr_brkbase &&
			    pmp->pr_vaddr < Psp->pr_brkbase + Psp->pr_brksize)
				lname = " [ heap ]";
			else
				lname = " [ anon ]";
		}

		(void) printf("%.*lX", addr_width, (u_long)pmp->pr_vaddr);

		printK((pmp->pr_size + 1023) / 1024);
		printK((pmp->pr_anon + pmp->pr_vnode) *
				(pmp->pr_pagesize / 1024));
		printK((pmp->pr_ashared + pmp->pr_vshared) *
				(pmp->pr_pagesize / 1024));
		printK((pmp->pr_anon + pmp->pr_vnode-pmp->pr_ashared -
				pmp->pr_vshared) * (pmp->pr_pagesize / 1024));
		(void) printf(lname?  " %-17s %s\n" : " %s\n",
			mflags(pmp->pr_mflags & ~(MA_ANON|MA_BREAK|MA_STACK)),
			lname);

		total_size += (pmp->pr_size + 1023) / 1024;
		total_res += (pmp->pr_anon + pmp->pr_vnode) *
			(pmp->pr_pagesize / 1024);
		total_shared += (pmp->pr_ashared + pmp->pr_vshared) *
			(pmp->pr_pagesize / 1024);
		total_priv += (pmp->pr_anon + pmp->pr_vnode - pmp->pr_ashared -
		    pmp->pr_vshared) * (pmp->pr_pagesize / 1024);
	}
	(void) printf("%s--------  ------  ------  ------  ------\n",
		(addr_width == 16)? "--------" : "");

	(void) printf("%stotal Kb",
		(addr_width == 16)? "        " : "");
	printK(total_size);
	printK(total_res);
	printK(total_shared);
	printK(total_priv);
	(void) printf("\n");

	free(prmapp);

	return (0);
}

static int
perr(char *s)
{
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}

static char *
mflags(u_int arg)
{
	static char code_buf[80];
	char *str = code_buf;

	if (arg == 0)
		return ("-");

	if (arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED))
		(void) sprintf(str, "0x%x",
			arg & ~(MA_READ|MA_WRITE|MA_EXEC|MA_SHARED));
	else
		*str = '\0';

	if (arg & MA_READ)
		(void) strcat(str, "/read");
	if (arg & MA_WRITE)
		(void) strcat(str, "/write");
	if (arg & MA_EXEC)
		(void) strcat(str, "/exec");
	if (arg & MA_SHARED)
		(void) strcat(str, "/shared");

	if (*str == '/')
		str++;

	return (str);
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
