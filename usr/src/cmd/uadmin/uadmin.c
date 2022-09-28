/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)uadmin.c	1.11	98/02/09 SMI"	/* SVr4.0 1.4	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/uadmin.h>
#include <libintl.h>
#include <bsm/libbsm.h>
#include <bsm/generic.h>

static char *Usage = "Usage: %s cmd fcn\n";

extern int audit_uadmin_setup();
extern int audit_uadmin_success();

main(argc, argv)
char *argv[];
{
	register cmd, fcn;
	sigset_t set, oset;

	if (argc != 3) {
		(void) fprintf(stderr, Usage, argv[0]);
		return (1);
	}

	(void) audit_uadmin_setup(argc, argv);

	(void) sigfillset(&set);
	(void) sigprocmask(SIG_BLOCK, &set, &oset);

	cmd = atoi(argv[1]);
	fcn = atoi(argv[2]);

	if (geteuid() == 0) {
		(void) audit_uadmin_success();
		/*
		 * wait for audit daemon to put halt message onto audit trail
		 */
		if (!cannot_audit(0)) {
			int cond = AUC_NOAUDIT;
			int canaudit;

			(void) sleep(1);

			/* find out if audit daemon is running */
			(void) auditon(A_GETCOND, (caddr_t)&cond,
				sizeof (cond));
			canaudit = (cond == AUC_AUDITING);

			/* turn off audit daemon and try to flush audit queue */
			if (canaudit && system("/usr/sbin/audit -t")) {
				(void) fprintf(stderr,
					gettext("%s: can't turn off auditd\n"),
					argv[0]);
			}
			(void) sleep(5);
		}
	}


	if (uadmin(cmd, fcn, 0) < 0) {
		perror("uadmin");
		return (1);
	}

	return (0);
}
