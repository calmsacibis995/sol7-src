/*
 * ifsetup.c: "Change state of interface when DHCP turns on or off".
 *
 * SYNOPSIS
 *    int canonical_if (IFINSTANCE*, int)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)ifsetup.c	1.3	96/11/20 SMI"

#include "client.h"
#include "error.h"
#include "msgindex.h"
#include "unixgen.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>
#include "catype.h"

int
canonical_if(IFINSTANCE *anIf, int set)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, anIf->ifName, IFNAMSIZ - 1);

	if (set && anIf->ifRestore) {
		ifr.ifr_flags = anIf->ifSavedFlags;
		anIf->ifSavedFlags = 0;
		anIf->ifRestore = FALSE;
	} else if (set && !anIf->ifRestore) {
		/* turn off DHCP flag */
		if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG10, anIf->ifName, SYSMSG);
			return (-1);
		}
		ifr.ifr_flags &= ~IFF_DHCPRUNNING; /* turn off DHCP flag */
	} else {
		if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG10, anIf->ifName, SYSMSG);
			return (-1);
		}
		anIf->ifRestore = TRUE;
		anIf->ifSavedFlags = ifr.ifr_flags;
		ifr.ifr_flags &= ~IFF_UP;
		ifr.ifr_flags |= IFF_DHCPRUNNING;
	}
	if (ioctl(client.sock, SIOCSIFFLAGS, &ifr) < 0) {
		loge(DHCPCMSG13, anIf->ifName, SYSMSG);
		return (-1);
	}
	return (0);
}
