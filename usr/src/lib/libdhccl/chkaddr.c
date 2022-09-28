/*
 * chkaddr.c: "Verify status of an interface under DHCP control".
 *
 * SYNOPSIS
 *    int checkAddr(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    DHCP has to take account of the fact that users may configure
 *    (or misconfigure!) their own machines. When the protocol
 *    renews an interface it checks that the IP address of the
 *    interface hasn't changed, and that the interface is still
 *    "up". If either of these conditions is violated, the
 *    agent considers that the interface is no longer
 *    under its control, and fails it. Thereafter the client
 *    will not renew/rebind or expire the interface.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)chkaddr.c 1.3 96/11/25 SMI"

#include <string.h>
#include <sys/sockio.h>
#include "client.h"
#include "error.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "unixgen.h"
#include "utils.h"

int
checkAddr(IFINSTANCE *anIf)
{
	struct sockaddr_in *sai, tifr_buff;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, anIf->ifName, IFNAMSIZ - 1);
	if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) < 0) {
		loge(DHCPCMSG10, anIf->ifName, SYSMSG);
		fail(anIf, ERR_IF_GET_FLAGS);
		return (-1);
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		fail(anIf, ERR_IF_NOT_UP);
		return (-1);
	}

	if (ioctl(client.sock, SIOCGIFADDR, &ifr) < 0) {
		loge(DHCPCMSG17, anIf->ifName, SYSMSG);
		fail(anIf, ERR_IF_GET_ADDR);
		return (-1);
	}

	memcpy(&tifr_buff, &ifr.ifr_addr, sizeof (struct sockaddr_in));
	sai = &tifr_buff;
	if (sai->sin_addr.s_addr != anIf->ifOldConfig->Yiaddr.s_addr) {
		const char *q1 = ITOA(&anIf->ifOldConfig->Yiaddr, 0);
		const char *q2 = ITOA(&sai->sin_addr, 1);
		loge(DHCPCMSG46, anIf->ifName, q1, q2);
		fail(anIf, ERR_IF_ADDR_CHANGED);
		return (-1);
	}
	return (0);
}
