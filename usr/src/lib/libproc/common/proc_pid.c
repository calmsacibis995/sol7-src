/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)proc_pidarg.c	1.1	97/12/23 SMI"

#include <fcntl.h>
#include <string.h>
#include <alloca.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libproc.h"

/*
 * Given a string naming a process, fetch the psinfo struct for the process.
 * On success, if the psinfo_t pointer is non-NULL copy the psinfo struct
 * into the supplied buffer; return the process-id.
 * On failure, set errno to ENOENT and return -1.
 */
pid_t
proc_pidarg(const char *arg, psinfo_t *psp)
{
	psinfo_t psinfo;
	struct stat statb;
	char *path;
	int fd;
	pid_t pid;

	/*
	 * Allocate space for "/proc/" "arg" "/psinfo"
	 */
	path = alloca(strlen(arg) + 14);

	/*
	 * If caller passed a NULL pointer, use the local buffer.
	 */
	if (psp == NULL)
		psp = &psinfo;

	if (strchr(arg, '/') != NULL) {
		(void) strcpy(path, arg);
	} else {
		(void) strcpy(path, "/proc/");
		(void) strcat(path, arg);
	}
	(void) strcat(path, "/psinfo");

	if ((fd = open(path, O_RDONLY)) >= 0 &&
	    fstat(fd, &statb) == 0 &&
	    (statb.st_mode & S_IFMT) == S_IFREG &&
	    strcmp(statb.st_fstype, "proc") == 0 &&
	    read(fd, psp, sizeof (*psp)) == sizeof (*psp)) {
		pid = psp->pr_pid;
	} else {
		pid = -1;
		errno = ENOENT;
	}

	if (fd >= 0)
		(void) close(fd);
	return (pid);
}
