/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pr_getitimer.c	1.1	97/12/23 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "libproc.h"

/*
 * getitimer() system call -- executed by subject process.
 */
int
pr_getitimer(struct ps_prochandle *Pr, int which, struct itimerval *itv)
{
	sysret_t rval;			/* return value from getitimer() */
	argdes_t argd[2];		/* arg descriptors for getitimer() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (getitimer(which, itv));

	adp = &argd[0];		/* which argument */
	adp->arg_value = which;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* itv argument */
	adp->arg_value = 0;
	adp->arg_object = itv;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_OUTPUT;
	adp->arg_size = sizeof (struct itimerval);

	rval = Psyscall(Pr, SYS_getitimer, 2, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}

/*
 * setitimer() system call -- executed by subject process.
 */
int
pr_setitimer(struct ps_prochandle *Pr,
	int which, const struct itimerval *itv, struct itimerval *oitv)
{
	sysret_t rval;			/* return value from setitimer() */
	argdes_t argd[3];		/* arg descriptors for setitimer() */
	argdes_t *adp;

	if (Pr == NULL)		/* no subject process */
		return (setitimer(which, (struct itimerval *)itv, oitv));

	adp = &argd[0];		/* which argument */
	adp->arg_value = which;
	adp->arg_object = NULL;
	adp->arg_type = AT_BYVAL;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = 0;

	adp++;			/* itv argument */
	adp->arg_value = 0;
	adp->arg_object = (void *)itv;
	adp->arg_type = AT_BYREF;
	adp->arg_inout = AI_INPUT;
	adp->arg_size = sizeof (struct itimerval);

	adp++;			/* oitv argument */
	adp->arg_value = 0;
	adp->arg_object = oitv;
	if (oitv == NULL) {
		adp->arg_type = AT_BYVAL;
		adp->arg_inout = AI_INPUT;
		adp->arg_size = 0;
	} else {
		adp->arg_type = AT_BYREF;
		adp->arg_inout = AI_OUTPUT;
		adp->arg_size = sizeof (struct itimerval);
	}

	rval = Psyscall(Pr, SYS_setitimer, 3, &argd[0]);

	if (rval.sys_errno) {
		errno = (rval.sys_errno > 0)? rval.sys_errno : ENOSYS;
		return (-1);
	}

	return (rval.sys_rval1);
}
