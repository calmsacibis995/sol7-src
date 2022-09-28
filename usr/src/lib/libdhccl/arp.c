/*
 * arp.c: "ARP for an offered address".
 *
 * SYNOPSIS
 *    int arp(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    The client ARPs for the address it has been offered. If it gets a reply
 *    arp() sends a decline packet to the server.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)arp.c 1.3 96/11/25 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "unixgen.h"
#include "utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
arp(IFINSTANCE *anIf)
{
	int rc;

	rc = arpCheck(anIf->ifName, client.arp_timeout,
	    &anIf->ifConfig->Yiaddr, anIf->ifMacAddr.chaddr);
	if (rc > 0) {
		loge(DHCPCMSG21, inet_ntoa(anIf->ifConfig->Yiaddr));
#if EMPLOY_BOOTP
		if (anIf->ifState != BOOTP)
			decline(anIf);
#else
		decline(anIf);
#endif
		return (-1);
	}
	return (0);
}
