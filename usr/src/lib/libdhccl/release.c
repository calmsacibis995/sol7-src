/*
 * release.c: "Construct and send DHCPRELEASE packet".
 *
 * SYNOPSIS
 *    void releaseIf(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * BUGS
 *    When the interface is in the DHCP_REQUESTING state there is
 *    a transaction in progress with the server. Although the
 *    local interface will be marked "down" the server will
 *    not be informed that the lease is not needed. This is
 *    not very serious, and in any case the time window for
 *    this occuring is small.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)release.c 1.5 96/12/27 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "unixgen.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>
#include "catype.h"

void
releaseIf(IFINSTANCE *anIf)
{
	struct ifreq ifr;
	/* LINTED [alignment ok] */
	struct sockaddr_in *sai = (struct sockaddr_in *)&ifr.ifr_addr;
	int rc;

	if (anIf->ifOldConfig == 0)
		return;

	/* Tell the server that the lease is no longer wanted: */

	if (anIf->ifState == DHCP_BOUND ||
	    anIf->ifState == DHCP_RENEWING ||
	    anIf->ifState == DHCP_REBINDING ||
	    anIf->ifState == DHCP_REBOOTING) {
		if (anIf->fd < 0) {
			rc = clientSocket(TRUE, &anIf->ifOldConfig->Yiaddr);
			if (rc < 0) {
				fail(anIf, rc);
				return;
			}
			anIf->fd = rc;
		}
		prepareRelease(anIf);
		anIf->ifXid = mrand48();
		sendBp(anIf, FALSE);
	}

	/*
	 * If the interface has the same address as the configuration take it
	 * down and set the IP address to 0.0.0.0:
	 */

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, anIf->ifName, IFNAMSIZ - 1);

	if (ioctl(client.sock, SIOCGIFADDR, &ifr) < 0) {
		loge(DHCPCMSG17, anIf->ifName, SYSMSG);
		goto endioctl;
	}

	if (sai->sin_addr.s_addr == anIf->ifOldConfig->Yiaddr.s_addr) {
		if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG10, anIf->ifName, SYSMSG);
			goto endioctl;
		}
		ifr.ifr_flags &= ~IFF_UP;
		if (ioctl(client.sock, SIOCSIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG13, anIf->ifName, SYSMSG);
			goto endioctl;
		}
		sai->sin_addr.s_addr = 0;
		sai->sin_family = AF_INET;
		if (ioctl(client.sock, SIOCSIFADDR, &ifr) < 0)
			loge(DHCPCMSG11, anIf->ifName, SYSMSG);

		memset(ifr.ifr_addr.sa_data + 2, 0, sizeof (struct in_addr));
		if (ioctl(client.sock, SIOCSIFNETMASK, &ifr) < 0)
			loge(DHCPCMSG23, anIf->ifName, SYSMSG);

		/* LINTED [alignment ok] */
		sai = (struct sockaddr_in *)&ifr.ifr_broadaddr;
		sai->sin_addr.s_addr = 0;
		sai->sin_family = AF_INET;
		(void) ioctl(client.sock, SIOCSIFBRDADDR, &ifr);
	}
endioctl:
	discard_dhc(anIf);
	dropIf(anIf);
}
