/*
 * rebind.c: "Make transtion to DHCP rebinding state".
 *
 * SYNOPSIS
 *    void rebind(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)rebind.c 1.4 96/11/25 SMI"

#include <stdlib.h>
#include "dhcpstate.h"
#include "client.h"
#include "bootp.h"
#include "error.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>
#include "catype.h"
#include "hostdefs.h"

void
rebind(IFINSTANCE *anIf)
{
	int rc;

	if (checkAddr(anIf))
		return;

	anIf->ifState = DHCP_REBINDING;
	anIf->ifRetries = 0;
	anIf->ifReceived = 0;
	anIf->ifSent = 0;
	anIf->ifOffers = 0;
	anIf->ifBadOffers = 0;
	anIf->ifXid = mrand48();
	prepareRebind(anIf);

	memset(&anIf->ifsaib, 0, sizeof (anIf->ifsaib));
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);
	anIf->ifsaib.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	anIf->ifsaib.sin_family = AF_INET;
	rc = liOpen(anIf->ifName, &anIf->lli);
	if (rc) {
		fail(anIf, rc);
		return;
	}
	retry(anIf, TRUE);
}
