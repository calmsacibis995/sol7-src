#ident	"@(#)getrpcport.c	1.5	97/04/11 SMI"

/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>

u_short
getrpcport(host, prognum, versnum, proto)
	char *host;
{
	struct sockaddr_in addr;
	struct hostent *hp;

	if ((hp = gethostbyname(host)) == NULL)
		return (0);
	memcpy((char *) &addr.sin_addr, hp->h_addr, hp->h_length);
	addr.sin_family = AF_INET;
	addr.sin_port =  0;
	return (pmap_getport(&addr, prognum, versnum, proto));
}
