/*
 * expire.c: "Handle expiration of DHCP lease on interface".
 *
 * SYNOPSIS
 *    void expireIf(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)expire.c 1.5 97/09/30 SMI"

#include "client.h"
#include "error.h"
#include "msgindex.h"
#include "unixgen.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <string.h>

void
expireIf(IFINSTANCE *anIf)
{
	struct ifreq ifr;
	struct sockaddr_in *sai;
	int rc;

	loge(DHCPCMSG62, anIf->ifName);
	if (checkAddr(anIf))
		return;

	if (client.DontConfigure) {
		rc = liOpen(anIf->ifName, &anIf->lli);
		if (rc)
			fail(anIf, rc);
		else
			init(anIf);
	} else {
		/* LINTED [alignment ok] */
		sai = (struct sockaddr_in *)&ifr.ifr_addr;
		memset(&ifr, 0, sizeof (ifr));
		strncpy(ifr.ifr_name, anIf->ifName, IFNAMSIZ - 1);
		if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG10, anIf->ifName, SYSMSG);
			fail(anIf, ERR_IF_GET_FLAGS);
			return;
		}

		ifr.ifr_flags &= ~IFF_UP;
		if (ioctl(client.sock, SIOCSIFFLAGS, &ifr) < 0) {
			loge(DHCPCMSG13, anIf->ifName, SYSMSG);
			fail(anIf, ERR_IF_SET_FLAGS);
			return;
		}

		memset(sai, 0, sizeof (*sai));
		sai->sin_family = AF_INET;
		rc = liOpen(anIf->ifName, &anIf->lli);
		if (rc) {
			fail(anIf, rc);
			return;
		}
		if (ioctl(client.sock, SIOCSIFADDR, &ifr) < 0) {
			loge(DHCPCMSG11, ifr.ifr_name, SYSMSG);
			fail(anIf, ERR_IF_SET_ADDR);
			return;
		}
		memset(ifr.ifr_addr.sa_data + 2, 0, sizeof (struct in_addr));
		if (ioctl(client.sock, SIOCSIFNETMASK, &ifr) < 0)
			loge(DHCPCMSG23, anIf->ifName, SYSMSG);

		/* LINTED [alignment ok] */
		sai = (struct sockaddr_in *)&ifr.ifr_broadaddr;
		sai->sin_addr.s_addr = 0;
		sai->sin_family = AF_INET;
		(void) ioctl(client.sock, SIOCSIFBRDADDR, &ifr);
		init(anIf);
	}
}
