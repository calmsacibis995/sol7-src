#ifndef lint
static char	sccsid[] = "@(#)audit_inetd.c 1.15 97/10/29 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <bsm/audit.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>
#include <netinet/in.h>
#include <generic.h>

#ifdef C2_DEBUG
#define	dprintf(x) {printf x; }
#else
#define	dprintf(x)
#endif

static struct in_addr	save_in_addr;	/* ip address of requester */
static u_short		save_iport;	/* port number of connection */
static int		save_afunc(int);

int
audit_inetd_config(void)
{
	aug_save_event(AUE_inetd_connect);
	aug_save_namask();
	return (0);
}

int
audit_inetd_service(
		char		*service_name,	/* name of service */
		struct in_addr	*in_addr,	/* ip address of requester */
		u_short		iport)		/* port number of connection */
{
	int	rd;		/* audit record descriptor */
	au_tid_t tid;		/* the terminal information */

	dprintf(("audit_inetd_service()\n"));

	if (cannot_audit(0)) {
		return (0);
	}

	aug_save_tid(aug_get_port(), in_addr->_S_un._S_addr);
	aug_save_text(service_name);
	save_in_addr = *in_addr;
	save_iport = iport;
	aug_save_sorf(0);
	aug_save_afunc(save_afunc);
	aug_audit();

	return (0);
}

static int
save_afunc(int ad)
{
	au_write(ad, au_to_in_addr(&save_in_addr));
	au_write(ad, au_to_iport(save_iport));
	return (0);
}
