/*
 * Copyright (c) 1994-1996, by Sun Microsystems, Inc.
 * All rights reserved
 */

/*
 * This code is MKS code ported to Solaris originally with minimum
 * modifications so that upgrades from MKS would readily integrate.
 * The MKS basis for this modification was:
 *
 * /u/rd/src/libc/sys/RCS/fexecve.c,v 1.9 1992/06/19 13:30:36 gord
 *
 * Additional modifications have been made to this code to make it
 * the 64-bit clean.
 */

#pragma ident	"@(#)fexecve.c 1.3	96/12/06 SMI"

/* LINTLIBRARY */

/*
 * MKS C library -- fexecve
 *
 * Copyright 1985, 1992 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * This Software is unpublished, valuable, confidential property of
 * Mortice Kern Systems Inc.  Use is authorized only in accordance
 * with the terms and conditions of the source licence agreement
 * protecting this Software.  Any unauthorized use or disclosure of
 * this Software is strictly prohibited and will result in the
 * termination of the licence agreement.
 *
 * If you have any questions, please consult your supervisor.
 *
 */

#include "mks.h"
#include <unistd.h>
#include <stdlib.h>

/*
 * Fork and exec (but no wait).
 */
pid_t
_fexecve(const char *path, char *const *argv, char *const *envp)
{
	pid_t pid;

	if ((pid = fork()) == -1)
		return (-1);
	if (pid != 0)
		return (pid);
	(void) execve(path, argv, envp);
	exit(-1);
	/* NOTREACHED */
}
