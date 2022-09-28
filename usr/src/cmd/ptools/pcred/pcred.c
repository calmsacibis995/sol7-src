/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pcred.c	1.8	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <libproc.h>

static	int	look(char *);
static	int	perr(char *);

static	char	*command;
static	char	*procname;
static 	int	all = 0;

main(int argc, char **argv)
{
	int rc = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr, "  (report process credentials)\n");
		return (2);
	}

	if (argc > 1 && strcmp(argv[1], "-a") == 0) {
		all = 1;
		argc--;
		argv++;
	}

	while (--argc > 0)
		rc += look(*++argv);

	return (rc);
}

static int
look(char *arg)
{
	pid_t pid;
	prcred_t *prcred;
	u_int ngroups = NGROUPS_MAX;	/* initial guess at number of groups */

	procname = arg;		/* for perr() */
	if ((pid = proc_pidarg(arg, NULL)) < 0) {
		(void) fprintf(stderr, "%s: no such process: %s\n",
			command, arg);
		return (1);
	}

	for (;;) {
		prcred = malloc(sizeof (prcred_t) +
			(ngroups - 1) * sizeof (gid_t));
		if (prcred == NULL)
			return (perr("malloc"));
		if (proc_get_cred(pid, prcred, ngroups) < 0) {
			free(prcred);
			return (perr("cred"));
		}
		if (ngroups >= prcred->pr_ngroups)    /* got all the groups */
			break;
		/* reallocate and try again */
		free(prcred);
		ngroups = prcred->pr_ngroups;
	}

	(void) printf("%d:\t", (int)pid);

	if (!all &&
	    prcred->pr_euid == prcred->pr_ruid &&
	    prcred->pr_ruid == prcred->pr_suid)
		(void) printf("e/r/suid=%d  ",
			(int)prcred->pr_euid);
	else
		(void) printf("euid=%d ruid=%d suid=%d  ",
			(int)prcred->pr_euid,
			(int)prcred->pr_ruid,
			(int)prcred->pr_suid);

	if (!all &&
	    prcred->pr_egid == prcred->pr_rgid &&
	    prcred->pr_rgid == prcred->pr_sgid)
		(void) printf("e/r/sgid=%d\n",
			(int)prcred->pr_egid);
	else
		(void) printf("egid=%d rgid=%d sgid=%d\n",
			(int)prcred->pr_egid,
			(int)prcred->pr_rgid,
			(int)prcred->pr_sgid);

	if (prcred->pr_ngroups != 0 &&
	    (all || prcred->pr_ngroups != 1 ||
	    prcred->pr_groups[0] != prcred->pr_rgid)) {
		int i;

		(void) printf("\tgroups:");
		for (i = 0; i < prcred->pr_ngroups; i++)
			(void) printf(" %d", (int)prcred->pr_groups[i]);
		(void) printf("\n");
	}

	free(prcred);

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
