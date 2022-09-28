/*
 * fail.c: "Mark a DHCP controlled interface as having failed to configure".
 *
 * SYNOPSIS
 *    void fail(IFINSTANCE *anIf, int code)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 */

#pragma ident   "@(#)fail.c 1.4 96/11/25 SMI"

#include <unistd.h>
#include "dhcpstate.h"
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "catype.h"
#include "dhcpctrl.h"

void
fail(IFINSTANCE *anIf, int code)
{
	anIf->ifState = DHCP_FAIL;
	anIf->ifBusy = FALSE;
	anIf->ifError = code;
	cancelWakeup(anIf->ifTimerID);
	anIf->ifTimerID = 0;
	cancelWakeup(anIf->ifRWID);
	anIf->ifRWID = 0;
	anIf->ifXid = 0;
	liClose(&anIf->lli);
	if (anIf->fd >= 0) {
		close(anIf->fd);
		anIf->fd = -1;
	}
	free_host(anIf->ifsendh);
	free_host(anIf->ifOldConfig);
	free_host(anIf->ifConfig);
	anIf->ifsendh = anIf->ifConfig = anIf->ifOldConfig = 0;

	if (anIf->connfd >= 0) {
		union dhcpipc reply;
		reply.ctrl.flags = CP_INTERNAL_ERROR;
		reply.ctrl.ctrl_id = anIf->ctrl_id;
		ctrl_reply(anIf->connfd, &reply);
		anIf->ctrl_flags = 0;
		anIf->ctrl_id = 0;
		close(anIf->connfd);
		anIf->connfd = -1;
	}
	canonical_if(anIf, TRUE);
}
