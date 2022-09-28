/*
 * prpacket.c: "Prepare necessary data to send from DHCP client to server".
 *
 * SYNOPSIS
 * void prepareDiscover(IFINSTANCE *anIf)
 * void prepareRequest(IFINSTANCE *anIf)
 * void prepareReboot(IFINSTANCE *anIf)
 * void prepareRelease(IFINSTANCE *anIf)
 * void prepareRenew(IFINSTANCE *anIf)
 * void prepareRebind(IFINSTANCE *anIf)
 * void prepareInform(IFINSTANCE *anIf, const struct in_addr ipaddr)
 *
 * DESCRIPTION
 *    These functions modify the contents of the "send" host
 *    structure for the interface preparatory to their being
 *    encapsulated in a DHCP/BOOTP packet. The actions taken
 *    are specified in the DHCP protocol, RFC1541 and revisions.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)prpacket.c 1.3 96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "dhcp.h"
#include "hostdefs.h"
#include "bootp.h"
#include "unixgen.h"
#include "ca_time.h"
#include <string.h>
#include "dhcpctrl.h"
#include <sys/socket.h>

extern struct in_addr server_addr;

void
prepareDiscover(IFINSTANCE *anIf)
{
	if (anIf->ifsendh->stags[TAG_HOSTNAME].dhccp)
		onhost(anIf->ifsendh->flags, TAG_HOSTNAME);

	if (client.lease_desired > 0 || client.lease_desired == -1) {
		anIf->ifsendh->stags[TAG_LEASETIME].dhcd4 =
		    client.lease_desired;
		onhost(anIf->ifsendh->flags, TAG_LEASETIME);
	} else
		ofhost(anIf->ifsendh->flags, TAG_LEASETIME);

	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPDISCOVER;

	onhost(anIf->ifsendh->flags, TAG_REQUEST_LIST);
	memcpy(anIf->ifsendh->request_vector, anIf->ifRequestVector,
	    sizeof (HFLAGS));

	ofhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	ofhost(anIf->ifsendh->flags, TAG_IP_REQUEST);

	/* Setup the broadcast address: */

	memset(&anIf->ifsaib, 0, sizeof (anIf->ifsaib));
	anIf->ifsaib.sin_family = AF_INET;
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);

	if (server_addr.s_addr)
		anIf->ifsaib.sin_addr = server_addr;
	else
		anIf->ifsaib.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	/* Set ciaddr to zero (can't respond to ARP) */
	anIf->ifsendh->Ciaddr.s_addr = htonl(0);
}

void
prepareRequest(IFINSTANCE *anIf)
{
	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPREQUEST;

	/* Set dhcp_server from previous response to discover: */

	onhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	anIf->ifsendh->stags[TAG_DHCP_SERVER].dhcia =
	anIf->ifConfig->stags[TAG_DHCP_SERVER].dhcia;

	/* Set lease required from offer accepted */
	if (inhost(anIf->ifConfig->flags, TAG_LEASETIME)) {
		onhost(anIf->ifsendh->flags, TAG_LEASETIME);
		anIf->ifsendh->stags[TAG_LEASETIME].dhcd4 =
		anIf->ifConfig->stags[TAG_LEASETIME].dhcd4;
	} else
		ofhost(anIf->ifsendh->flags, TAG_LEASETIME);

	onhost(anIf->ifsendh->flags, TAG_REQUEST_LIST);
	memcpy(anIf->ifsendh->request_vector, anIf->ifRequestVector,
	    sizeof (HFLAGS));

	onhost(anIf->ifsendh->flags, TAG_IP_REQUEST);
	anIf->ifsendh->stags[TAG_IP_REQUEST].dhcia = anIf->ifConfig->Yiaddr;
}

void
prepareReboot(IFINSTANCE *anIf)
{
	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPREQUEST;

	ofhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);

	onhost(anIf->ifsendh->flags, TAG_REQUEST_LIST);
	memcpy(anIf->ifsendh->request_vector, anIf->ifRequestVector,
	    sizeof (HFLAGS));

	onhost(anIf->ifsendh->flags, TAG_IP_REQUEST);
	anIf->ifsendh->stags[TAG_IP_REQUEST].dhcia = anIf->ifOldConfig->Yiaddr;

	if (inhost(anIf->ifOldConfig->flags, TAG_HOSTNAME)) {
		if (anIf->ifsendh->stags[TAG_HOSTNAME].dhccp)
			free(anIf->ifsendh->stags[TAG_HOSTNAME].dhccp);
		anIf->ifsendh->stags[TAG_HOSTNAME].dhccp = (char *)xmalloc(
		    1 + strlen(anIf->ifOldConfig->stags[TAG_HOSTNAME].dhccp));
		if (anIf->ifsendh->stags[TAG_HOSTNAME].dhccp) {
			onhost(anIf->ifsendh->flags, TAG_HOSTNAME);
			strcpy(anIf->ifsendh->stags[TAG_HOSTNAME].dhccp,
			anIf->ifOldConfig->stags[TAG_HOSTNAME].dhccp);
		}
	}

	/*  Setup the broadcast address: */

	memset(&anIf->ifsaib, 0, sizeof (anIf->ifsaib));
	anIf->ifsaib.sin_family = AF_INET;
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);

	if (server_addr.s_addr)
		anIf->ifsaib.sin_addr = server_addr;
	else
		anIf->ifsaib.sin_addr.s_addr = htonl(INADDR_BROADCAST);
}

void
prepareRenew(IFINSTANCE *anIf)
{
	time_t now = GetCurrentSecond();

	anIf->ifsendh->Ciaddr = anIf->ifOldConfig->Yiaddr;
	memset(anIf->ifsendh->request_vector, 0, sizeof (HFLAGS));

	anIf->ifBegin = now;

	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPREQUEST;

	ofhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	ofhost(anIf->ifsendh->flags, TAG_HOSTNAME);
	ofhost(anIf->ifsendh->flags, TAG_IP_REQUEST);

	anIf->ifsaib.sin_family = AF_INET;
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);
	anIf->ifsaib.sin_addr = anIf->ifOldConfig->stags[TAG_DHCP_SERVER].dhcia;
}

void
prepareRebind(IFINSTANCE *anIf)
{
	time_t now = GetCurrentSecond();

	anIf->ifsendh->Ciaddr = anIf->ifOldConfig->Yiaddr;

	anIf->ifBegin = now;

	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPREQUEST;
}

void
prepareRelease(IFINSTANCE *anIf)
{
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPRELEASE;
	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	memcpy(&anIf->ifsendh->Ciaddr, &anIf->ifOldConfig->Yiaddr,
	    sizeof (struct in_addr));
	onhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	anIf->ifsendh->stags[TAG_DHCP_SERVER].dhcia =
	anIf->ifOldConfig->stags[TAG_DHCP_SERVER].dhcia;

	anIf->ifsaib.sin_family = AF_INET;
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);
	anIf->ifsaib.sin_addr = anIf->ifOldConfig->stags[TAG_DHCP_SERVER].dhcia;
}

#if DHCP_OVER_PPP
/*
 *  Info request modeled after renew
 */
void
prepareInform(IFINSTANCE *anIf, const struct in_addr ipaddr)
{
	time_t now = GetCurrentSecond();

	if (anIf->ifOldConfig)
		anIf->ifsendh->Ciaddr = anIf->ifOldConfig->Yiaddr;

	anIf->ifBegin = now;

	onhost(anIf->ifsendh->flags, TAG_DHCP_MESSAGE_TYPE);
	anIf->ifsendh->stags[TAG_DHCP_MESSAGE_TYPE].dhcu1 = DHCPINFORM;

#ifdef NEVER
	memset(anIf->ifsendh->request_vector, 0, sizeof (HFLAGS));
	ofhost(anIf->ifsendh->flags, TAG_DHCP_SERVER);
	ofhost(anIf->ifsendh->flags, TAG_HOSTNAME);
	ofhost(anIf->ifsendh->flags, TAG_IP_REQUEST);
#endif /* NEVER */
	onhost(anIf->ifsendh->flags, TAG_REQUEST_LIST);
	memcpy(anIf->ifsendh->request_vector, anIf->ifRequestVector,
	    sizeof (HFLAGS));

	anIf->ifsaib.sin_family = AF_INET;
	anIf->ifsaib.sin_port = htons(IPPORT_BOOTPS);
	anIf->ifsaib.sin_addr = ipaddr;
}
#endif /* DHCP_OVER_PPP */
