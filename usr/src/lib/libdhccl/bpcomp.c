/*
 * SYNOPSIS
 *	void bootpComplete(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma	ident	"@(#)bpcomp.c	1.3	96/11/25 SMI"

#include <unistd.h>
#include "dhcpstate.h"
#include "client.h"
#include "dccommon.h"
#include "hostdefs.h"
#include "dhcpctrl.h"
#include "catype.h"

void
bootpComplete(IFINSTANCE *anIf)
{
	if (!client.DontConfigure)
		discard_dhc(anIf);

	if (anIf->connfd >= 0) {
		union dhcpipc reply;
		reply.ctrl.flags = CP_SUCCESS;
		reply.ctrl.ctrl_id = anIf->ctrl_id;
		ctrl_reply(anIf->connfd, &reply);
		anIf->ctrl_flags = 0;
		anIf->ctrl_id = 0;
		close(anIf->connfd);
		anIf->connfd = -1;
	}

	free_host(anIf->ifOldConfig);
	anIf->ifOldConfig = anIf->ifConfig;
	anIf->ifConfig = 0;
	anIf->ifXid = 0;
	anIf->ifBadOffers = 0;
	anIf->ifState = BOOTP;
	anIf->ifBusy = FALSE;
	liClose(&anIf->lli);
	if (client.DontConfigure) {
		canonical_if(anIf, TRUE);
	} else {
		anIf->ifRestore = FALSE;
		onhost(anIf->ifOldConfig->flags, TAG_LEASETIME);
		anIf->ifOldConfig->stags[TAG_LEASETIME].dhcd4 =
		    -1;   /* infinite */
		setExpiration(anIf->ifOldConfig);
		configIf(anIf->ifName, anIf->ifOldConfig);
		if (anIf->ifPrimary) {
			configGateway(client.sock, anIf->ifOldConfig);
			configHostname(anIf->ifOldConfig);
		}
	}
}
