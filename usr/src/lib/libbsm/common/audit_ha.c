#ifndef lint
static char	sccsid[] = "@(#)audit_halt.c 1.12 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <netinet/in.h>
#include <libgen.h>
#include <generic.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

extern int	errno;

static int	pflag;			/* preselection flag */
static au_event_t	event;	/* audit event number */

int audit_halt_generic();

int
audit_halt_setup(argc, argv)
int	argc;
char	**argv;
{
	char *cmdname;

	dprintf(("audit_halt_setup()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	cmdname = basename(*argv);

	aug_init();

	if (strcmp(cmdname, "halt") == 0)
		aug_save_event(AUE_halt_solaris);
	else if (strcmp(cmdname, "poweroff") == 0)
		aug_save_event(AUE_poweroff_solaris);
	else
		exit(1);
	aug_save_me();
	return (0);
}

int
audit_halt_fail()
{
	return (audit_halt_generic(-1));
}

int
audit_halt_success()
{
	return (audit_halt_generic(0));
}

int
audit_halt_generic(sorf)
	int sorf;
{
	int r;

	dprintf(("audit_halt_generic(%d)\n", sorf));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_sorf(sorf);
	r = aug_audit();

	return (r);
}
