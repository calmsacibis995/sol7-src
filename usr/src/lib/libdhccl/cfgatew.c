/*
 * cfgatew.c: "Configure default and static routes".
 *
 * SYNOPSIS
 *    void configGateway(int sockfd, const Host* hp)
 *    void configRoutes (int sockfd, const Host* hp)
 *
 * DESCRIPTION
 *    Add a default route. SunOS4.x  only allows a single default route,
 *    so the one choosen is simply the first in the list. SunOS5.x
 *    allows many, so they are all installed.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)cfgatew.c 1.4 96/11/26 SMI"

#include <string.h>
#include "catype.h"
#include "client.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "unixgen.h"
#include "utils.h"

void
configGateway(int sockfd, const Host *hp)
{
	struct sockaddr dest, via;
	int i;
	struct in_addr_list *addrs = hp->stags[TAG_GATEWAYS].dhcial;

	/* If default gateway is in host struct configure it */
	memset(&dest, 0, sizeof (dest));
	memset(&via, 0, sizeof (via));
	via.sa_family = dest.sa_family = AF_INET;

	if (inhost(hp->flags, TAG_GATEWAYS) && addrs->addrcount > 0) {
		for (i = 0; i < addrs->addrcount; i++) {
			logl(DHCPCMSG14, "default gateway",
			    ADDRSTR(&addrs->addr[i]));

			memcpy(via.sa_data + 2, addrs->addr + i,
			    sizeof (struct in_addr));

			if (addrt(sockfd, TRUE, TRUE, &dest, &via) < 0)
				loge(DHCPCMSG15, via.sa_data + 2, SYSMSG);
		}
	}
}

#ifdef	__CODE_UNUSED
void
configRoutes(int sockfd, const Host *hp)
{
	struct sockaddr dest, via;
	struct in_addr	*top, *byp;
	int i;

	if (inhost(hp->flags, TAG_STATIC_ROUTES) &&
	    hp->stags[TAG_STATIC_ROUTES].dhcial->addrcount > 0) {
		logl(DHCPCMSG16);
		memset(&dest, 0, sizeof (dest));
		memset(&via, 0, sizeof (via));
		dest.sa_family = via.sa_family = AF_INET;

		for (i = 0; (i + 1) < hp->stags[TAG_STATIC_ROUTES].dhcial->
		    addrcount; i += 2) {
			top = (struct in_addr *)
			    (hp->stags[TAG_STATIC_ROUTES].dhcial->addr + i);
			byp = (struct in_addr *)
			    (hp->stags[TAG_STATIC_ROUTES].dhcial->addr +
			    (i + 1));
			if (top->s_addr == 0) {
				logw(DHCPCMSG18, ADDRSTR(top));
			} else {
				const char *q1 = ITOA(top, 0);
				const char *q2 = ITOA(byp, 1);
				memcpy(dest.sa_data + 2, top,
				    sizeof (struct in_addr));
				memcpy(via.sa_data + 2, byp,
				    sizeof (struct in_addr));

				if (addrt(sockfd, FALSE, TRUE, &dest,
				    &via) == 0) {
					logl(DHCPCMSG20, q1, q2);
				} else
					logl(DHCPCMSG19, q1, q2, SYSMSG);
			}
		}
	}
}
#endif	/* __CODE_UNUSED */
