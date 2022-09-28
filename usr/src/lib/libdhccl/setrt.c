/*
 * setrt.c: "Add or delete a route".
 *
 * SYNOPSIS
 *    int addrt(int sock, int net_entry, int remote,
 *        const struct sockaddr *dest, const struct sockaddr *gate)
 *
 *    int delrt(int sock, int net_entry, const struct sockaddr *dest,
 *          const struct sockaddr *gate)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)setrt.c 1.3 96/11/26 SMI"

#include "utils.h"
#include "utiltxt.h"
#include "unixgen.h"
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/route.h>
#include <netinet/in.h>
#include <string.h>

int
addrt(int sock, int net_entry, int remote, const struct sockaddr *dest,
    const struct sockaddr *gate)
{
	struct rtentry r;

	memset(&r, 0, sizeof (r));
	r.rt_dst = *dest;
	r.rt_gateway = *gate;

	r.rt_flags |= RTF_UP;
	if (!net_entry)
		r.rt_flags |= RTF_HOST;
	if (remote)
		r.rt_flags |= RTF_GATEWAY;

	if (ioctl(sock, SIOCADDRT, (char *)&r) < 0 && errno != EEXIST) {
		const char *q1 = ITOA(dest, 0);
		const char *q2 = ITOA(gate, 1);
		loge(UTILMSG29, q1, q2, SYSMSG);
		return (-1);
	}

	return (0);
}

#ifdef	__CODE_UNUSED
int
delrt(int sock, int net_entry, const struct sockaddr *dest,
    const struct sockaddr *gate)
{
	struct rtentry r;

	memset((char *)&r, 0, sizeof (r));
	r.rt_dst = *dest;
	r.rt_gateway = *gate;

	r.rt_flags |= RTF_UP;
	if (!net_entry)
		r.rt_flags |= RTF_HOST;

	if (ioctl(sock, SIOCDELRT, (char *)&r) < 0 && errno != ESRCH) {
		const char *q1 = ITOA(dest, 0);
		const char *q2 = ITOA(gate, 1);
		loge(UTILMSG30, q1, q2, SYSMSG);
		return (1);
	}

	return (0);
}
#endif	/* __CODE_UNUSED */
