/*
 * initcomp.c: "Handle transition in DHCP state to bound or failed".
 *
 * SYNOPSIS
 *    void initSuccess(IFINSTANCE *anIf)
 *    void initFailure(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    These functions are called when the DHCP protocol makes a
 *    transition from the "init" or "init-reboot" state
 *    to either the "bound" state (if DHCP successful)
 *    or "fail" state. The latter is an augmentation
 *    to the state diagram in RFC1541, but it doesn't.
 *    affect the protocol, only the implementation.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)initcomp.c	1.4	96/11/25 SMI"

#include <unistd.h>
#include <malloc.h>
#include <string.h>
#include "dhcpstate.h"
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "hostdefs.h"
#include "dhcpctrl.h"
#include "catype.h"

void
initFailure(IFINSTANCE *anIf)
{
	if (anIf->connfd >= 0) {
		union dhcpipc reply;
		reply.ctrl.flags = CP_NO_RESPONSE;
		reply.ctrl.ctrl_id = anIf->ctrl_id;
		ctrl_reply(anIf->connfd, &reply);
		anIf->ctrl_flags = 0;
		anIf->ctrl_id = 0;
		close(anIf->connfd);
		anIf->connfd = -1;
	}
	if (!client.DontConfigure)
		discard_dhc(anIf);
}

void
initSuccess(IFINSTANCE *anIf)
{
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
	anIf->ifState = DHCP_BOUND;
	anIf->ifBusy = FALSE;
	liClose(&anIf->lli);
	setT1T2(anIf->ifOldConfig);
	toTimer(&anIf->ifTimer, anIf->ifOldConfig);
	setNewWakeupTime(anIf);
	if (client.DontConfigure)
		canonical_if(anIf, TRUE);
	else {
		anIf->ifRestore = FALSE;
		setExpiration(anIf->ifOldConfig);
		if (anIf->ifRWID != 0) {
			cancelWakeup(anIf->ifRWID);
			anIf->ifRWID = 0;
		}
		save_dhc(anIf);
		configIf(anIf->ifName, anIf->ifOldConfig);
		if (anIf->ifPrimary) {
			configGateway(client.sock, anIf->ifOldConfig);
			configHostname(anIf->ifOldConfig);
		}
		if (anIf->ifsendh->stags[TAG_HOSTNAME].dhccp)
			free(anIf->ifsendh->stags[TAG_HOSTNAME].dhccp);
		if (inhost(anIf->ifOldConfig->flags, TAG_HOSTNAME)) {
			anIf->ifsendh->stags[TAG_HOSTNAME].dhccp = xstrdup(
			    anIf->ifOldConfig->stags[TAG_HOSTNAME].dhccp);
		} else {
			ofhost(anIf->ifsendh->flags, TAG_HOSTNAME);
			anIf->ifsendh->stags[TAG_HOSTNAME].dhccp = 0;
		}
	}
}
