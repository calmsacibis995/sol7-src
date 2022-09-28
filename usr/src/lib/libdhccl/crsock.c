/*
 * crsock.c: "Create socket".
 *
 * SYNOPSIS
 *    int clientSocket(int,const struct in_addr*)
 *
 * DESCRIPTION
 *    Open general purpose (unbound) socket for use as "window"
 *    into the kernel.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */
#pragma ident   "@(#)crsock.c 1.3 96/11/25 SMI"

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "client.h"
#include "utils.h"
#include "error.h"
#include "msgindex.h"
#include "unixgen.h"

#ifndef IPPORT_BOOTPC
#define	IPPORT_BOOTPC	((short)68)
#endif

int
clientSocket(int bindToBootpc, const struct in_addr *ipaddr)
{
	int on = 1;
	struct sockaddr_in raddr;
	int sock;

	/* Create the read-from-BOOTP-client socket: */

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1) {
		loge(DHCPCMSG31, SYSMSG);
		return (ERR_CREATE_SOCKET);
	}

	if (!bindToBootpc)
		return (sock);

	memset(&raddr, 0, sizeof (raddr));
	raddr.sin_family = AF_INET;
	raddr.sin_port = htons(IPPORT_BOOTPC);
	if (ipaddr)
		raddr.sin_addr = *ipaddr;
	else
		raddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr *)&raddr, sizeof (raddr)) < 0) {
		loge(DHCPCMSG34, IPPORT_BOOTPC, ADDRSTR(&raddr.sin_addr),
		    SYSMSG);
		return (ERR_BIND_SOCKET);
	}

#ifdef SO_BROADCAST
	if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&on,
	    sizeof (on)) < 0) {
		loge(DHCPCMSG32, SYSMSG);
		return (ERR_SOCKET_BROADCAST);
	}
#endif

	return (sock);
}
