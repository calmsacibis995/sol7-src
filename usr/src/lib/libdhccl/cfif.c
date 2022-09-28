/*
 * cfif.c: "Configure interface address and mask from DHCP data".
 *
 * SYNOPSIS
 *    void configIf(const char *ifname, const Host *hp)
 *
 * DESCRIPTION
 *    Configures the interface ifname from the host struct *hp.
 *    The IP address, netmask, broadcast address are set and the
 *    interface is marked as "up".
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 */

#pragma ident   "@(#)cfif.c 1.3 96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "hostdefs.h"
#include "msgindex.h"
#include "unixgen.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <errno.h>
#include <string.h>

void
configIf(const char *ifname, const Host *hp)
{
	struct in_addr ip_addr;
	struct ifreq ifr;

	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	ifr.ifr_addr.sa_family = AF_INET;

	memset(&ip_addr, 0, sizeof (ip_addr));
	logl(DHCPCMSG22, ifname);

	/* Configure the subnet mask */
	if (inhost(hp->flags, TAG_SUBNET_MASK)) {
		const struct in_addr *maskp =
		    &hp->stags[TAG_SUBNET_MASK].dhcia;
		logl(DHCPCMSG14, "subnet mask", ADDRSTR(maskp));

		memcpy(ifr.ifr_addr.sa_data + 2, maskp,
		    sizeof (struct in_addr));
		if (ioctl(client.sock, SIOCSIFNETMASK, &ifr) < 0)
			loge(DHCPCMSG23, ifname);
	}

	/* Configure the IP address */
	logl(DHCPCMSG14, "IP address", ADDRSTR(&hp->Yiaddr));
	memcpy(ifr.ifr_addr.sa_data + 2, &hp->Yiaddr, sizeof (struct in_addr));
	if (ioctl(client.sock, SIOCSIFADDR, &ifr) < 0)
		loge(DHCPCMSG11, ifname, SYSMSG);

	/*
	 * Don't need to configure the broadcast address. SunOS does this
	 * for you automatically when you configure the netmask and IP
	 * address, so you don't need to bother.
	 */

	/* Mark the interface as up */
	if (ioctl(client.sock, SIOCGIFFLAGS, &ifr) >= 0) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(client.sock, SIOCSIFFLAGS, &ifr) < 0)
		loge(DHCPCMSG13, ifname, SYSMSG);
	} else
		loge(DHCPCMSG10, ifname, SYSMSG);
}
