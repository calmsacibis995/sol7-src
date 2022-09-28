/*
 * readbp.c: "Read and unpack BOOTP packets".
 *
 * SYNOPSIS
 *    void readBOOTPsocket (int fd)
 *    void readIFfd        (int fd)
 *
 * DESCRIPTION
 *    readBOOTPsocket:
 *        Reads a BOOTP packet (presumably) from the socket fd.
 *        This only occurs when the client is renewing. The
 *        packet has presumably been unicast back to the client
 *        from the server.
 *
 *    readIFfd:
 *        From from the fd of the link level interface. This
 *        is the normal situation when the client is listening
 *        for responses to its broadcast to the 255.255.255.255
 *
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)readbp.c 1.5 97/07/16 SMI"

#include <string.h>
#include "client.h"
#include "bootp.h"
#include "msgindex.h"
#include "unixgen.h"
#include <sys/socket.h>

void
readBOOTPsocket(int fd)
{
	struct sockaddr_in from;
	socklen_t fromlen = (socklen_t)sizeof (from);
	union bigbootp reply;
	int len;

	len = recvfrom(fd, reply.buf, sizeof (reply), 0,
	    (struct sockaddr *)&from, &fromlen);
	if (len < 0) {
		loge(DHCPCMSG37, SYSMSG);
		return;
	}
	examineReply(&reply.bp, len, &from);
}

void
readIFfd(int fd)
{
	int rc;
	struct sockaddr_in from;
	union bigbootp reply;
	int len = sizeof (reply);

	rc = liRead(fd, reply.buf, &len, ((struct sockaddr *)&from)->sa_data,
	    0);
	if (rc < 0) {
		loge(DHCPCMSG04, SYSMSG);
		return;
	}
	examineReply(&reply.bp, len, &from);
}
