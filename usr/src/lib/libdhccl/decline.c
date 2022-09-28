/*
 * decline.c: "Construct and send a DHCPDECLINE message".
 *
 * SYNOPSIS
 *    void decline(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *	Send a DHCPDECLINE packet to the server.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)decline.c 1.2 96/11/18 SMI"

#include "client.h"
#include "dhcp.h"
#include "hostdefs.h"
#include "catype.h"
#include <string.h>

void
decline(IFINSTANCE *anIf)
{
	struct Host save;

	memcpy(&save, anIf->ifsendh, sizeof (Host));
	memset(anIf->ifsendh, 0, sizeof (Host));
	onhost(anIf->ifsendh->flags, TAG_HWADDR);
	anIf->ifsendh->hw = anIf->ifMacAddr;
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPDECLINE;
	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	onhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	anIf->ifsendh->stags[TAG_DHCP_SERVER].dhcia =
	    anIf->ifConfig->stags[TAG_DHCP_SERVER].dhcia;
	onhost(anIf->ifsendh->flags, TAG_IP_REQUEST);
	anIf->ifsendh->stags[TAG_IP_REQUEST].dhcia = anIf->ifConfig->Yiaddr;
	sendBp(anIf, TRUE);
	memcpy(anIf->ifsendh, &save, sizeof (Host));
}
