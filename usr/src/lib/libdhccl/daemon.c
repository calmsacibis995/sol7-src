/*
 * daemon.c: "Daemonise process".
 *
 * SYNOPSIS
 *    int isdaemon()
 *    void daemon()
 *
 * DESCRIPTION
 *    daemon:
 *    Convert the process to a daemon.
 *
 *    isdaemon:
 *    Returns TRUE if the process is a daemon (started by the Internet
 *    superserver inetd) or FALSE otherwise.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)daemon.c 1.4 97/03/10 SMI"

#include "daemon.h"
#include "catype.h"
#include "caunistd.h"
#include "ca_time.h"
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>

int
daemon(void)
{
	int		rc, fd;
	struct rlimit	rl;

	if ((rc = fork()) < 0)
		return (DAEMON_FORK_FAILED);

	if (rc > 0)
		exit(0);

	signal(SIGHUP, SIG_IGN);

	if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		for (fd = 0; (rlim_t)fd < rl.rlim_cur; fd++)
			(void) close(fd);
		errno = 0;	/* set from closing a non-open fd */
	}

	(void) open("/dev/null", O_RDONLY, 0);
	(void) dup2(0, 1);
	(void) dup2(0, 2);

	/* Detach console */
	(void) setsid();

	if (chdir("/") != 0)
		return (DAEMON_CANNOT_CDROOT);

	signal(SIGCHLD, SIG_IGN);
	return (DAEMON_OK);
}

#ifdef	__CODE_UNUSED
int
isdaemon(void)
{
	struct stat t;

	if (!fstat(0, &t) && S_ISSOCK(t.st_mode))
		return (TRUE);

	return (FALSE);
}
#endif	/* __CODE_UNUSED */
