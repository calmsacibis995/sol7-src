/*
 * ctrlsock.c: "Create control socket for IPC communication".
 *
 * SYNOPSIS
 *    int controlSocket()
 *
 * DESCRIPTION
 *	Open a socket and bind to a well-known port number on the
 *	loopback address so that control and status information
 *	can be sent to and from the agent.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)ctrlfd.c 1.4 96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "dhcpctrl.h"
#include "error.h"
#include "msgindex.h"
#include "unixgen.h"
#include <string.h>
#include <sys/socket.h>

int
controlSocket(void)
{
	int rc, on = 1;
	struct sockaddr_in raddr;

#if USE_TCP
	client.controlSock = socket(AF_INET, SOCK_STREAM, 0);
#else
	client.controlSock = socket(AF_INET, SOCK_DGRAM, 0);
#endif
	if (client.controlSock < 0) {
		loge(DHCPCMSG31, SYSMSG);
		return (ERR_CREATE_SOCKET);
	}
	setsockopt(client.controlSock, SOL_SOCKET, SO_REUSEADDR, (char *)&on,
	    sizeof (on));

	memset(&raddr, 0, sizeof (raddr));
	raddr.sin_family = AF_INET;
	raddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	raddr.sin_port = htons(IPPORT_DHCPCONTROL);

	rc = bind(client.controlSock, (struct sockaddr *)&raddr,
	    sizeof (raddr));
	if (rc < 0) {
		loge(DHCPCMSG34, IPPORT_DHCPCONTROL, ADDRSTR(&raddr.sin_addr),
		    SYSMSG);
		return (ERR_BIND_SOCKET);
	}
	return (0);
}
