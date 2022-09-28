/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)prun.c	1.6	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <libproc.h>

static	int	start(char *);
static	int	perr(char *);

static	char	*command;
static	char	*procname;

main(int argc, char **argv)
{
	int rc = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	if (argc <= 1) {
		(void) fprintf(stderr, "usage:\t%s pid ...\n", command);
		(void) fprintf(stderr, "  (set stopped processes running)\n");
		return (2);
	}

	while (--argc > 0)
		rc += start(*++argv);

	return (rc);
}

static int
start(char *arg)
{
	pid_t pid;
	char ctlfile[100];
	int ctlfd;
	long ctl[4];

	procname = arg;		/* for perr() */
	if ((pid = proc_pidarg(arg, NULL)) < 0) {
		(void) fprintf(stderr, "%s: no such process: %s\n",
			command, arg);
		return (1);
	}

	(void) sprintf(ctlfile, "/proc/%d/ctl", (int)pid);
	errno = 0;
	if ((ctlfd = open(ctlfile, O_WRONLY)) >= 0) {
		ctl[0] = PCKILL;
		ctl[1] = SIGCONT;
		ctl[2] = PCRUN;
		ctl[3] = 0;
		(void) write(ctlfd, (char *)ctl, 4*sizeof (long));
		(void) close(ctlfd);
	}

	return (perr(NULL));
}

static int
perr(char *s)
{
	if (errno == 0 || errno == EBUSY)
		return (0);
	if (s)
		(void) fprintf(stderr, "%s: ", procname);
	else
		s = procname;
	perror(s);
	return (1);
}
