/*
 * newif.c:
 * "Construct internal structures for new interface under DHCP control".
 *
 * SYNOPSIS
 * IFINSTANCE *newIf(const char *interface)
 * int armIf(IFINSTANCE *anIf, const HFLAGS requestVector,
 *    const in_addr *server)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)newif.c	1.6	97/03/06 SMI"

#include <string.h>
#include "dhcpstate.h"
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "bootp.h"
#include "hostdefs.h"
#include "catype.h"
#include "error.h"
#include "ca_time.h"
#include <alloca.h>
#include "unixgen.h"
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

static int
hostAtIf(const char *ifname, char **ifHostName)
{
	static const char *roots = "/etc/hostname.";
	char hostname[MAXHOSTNAMELEN];
	char *path = (char *)alloca(1 + strlen(roots) + strlen(ifname));
	FILE *f;

	sprintf(path, "%s%s", roots, ifname);
	f = fopen(path, "r");
	if (f) {
		char *q = fgets(hostname, sizeof (hostname), f);
		fclose(f);
		if (q) {
			int l = strlen(hostname);
			if (hostname[l - 1] == '\n')
				hostname[--l] = '\0';
			if (l > 0) {
				*ifHostName = xstrdup(hostname);
				return (0);
			}
		}
	}
	if (client.hostname[0] != '\0') {
		*ifHostName = xstrdup(client.hostname);
		return (0);
	}
	return (-1);
}

static void
linkIf(IFINSTANCE *anIf)
{
	anIf->ifNext = ifList;
	if (!ifList)
		anIf->ifOrd = 1;
	else
		anIf->ifOrd = ifList->ifOrd + 1;
	ifList = anIf;
}

static int
leaseExpired(time_t expires)
{
	time_t current;

	current = GetCurrentSecond();
	return (expires <= current);
}

IFINSTANCE *
newIf(const char *interface)
{
	IFINSTANCE *anIf;

	cancel_inactivity_timer();
	anIf = (IFINSTANCE *)xmalloc(sizeof (IFINSTANCE));
	memset(anIf, 0, sizeof (IFINSTANCE));
	strncpy(anIf->ifName, interface, IFNAMSIZ);
	anIf->ifName[IFNAMSIZ-1] = '\0';
	anIf->fd = -1;
	anIf->lli.fd = -1;
	anIf->connfd = -1;
	linkIf(anIf);
	return (anIf);
}

int
armIf(IFINSTANCE *anIf, const HFLAGS requestVector,
    const struct in_addr *server)
{
	struct ifreq	ifr;
	int status;

	anIf->ifsendh = (Host *)xmalloc(sizeof (Host));
	memset(anIf->ifsendh, 0, sizeof (Host));

	hostAtIf(anIf->ifName, &anIf->ifsendh->stags[TAG_HOSTNAME].dhccp);

	if (client.class_id != 0 && client.class_id[0] != '\0') {
		anIf->ifsendh->stags[TAG_CLASS_ID].dhccp = xstrdup(
		    client.class_id);
		onhost(anIf->ifsendh->flags, TAG_CLASS_ID);
	}
	anIf->ifXid = mrand48();
	memcpy(anIf->ifRequestVector, requestVector, sizeof (HFLAGS));
	if (server)
		memcpy(&anIf->ifServer, server, sizeof (struct in_addr));

#if DHCP_OVER_PPP
	if (!anIf->ifInformOnly)
#endif /* DHCP_OVER_PPP */
		canonical_if(anIf, FALSE);
	status = liOpen(anIf->ifName, &anIf->lli);
	if (status) {
		fail(anIf, status);
		return (-1);
	}
	anIf->fd = -1;
	anIf->ifMacAddr.htype = anIf->lli.bpmactype;
	anIf->ifMacAddr.hlen = (anIf->lli.lmacaddr <
	    sizeof (anIf->ifMacAddr.chaddr) ? anIf->lli.lmacaddr :
	    sizeof (anIf->ifMacAddr.chaddr));
	memset(anIf->ifMacAddr.chaddr, 0, sizeof (anIf->ifMacAddr.chaddr));
	memcpy(anIf->ifMacAddr.chaddr, anIf->lli.macaddr, anIf->ifMacAddr.hlen);

#if DEBUG
	if (client.UserDefinedMacAddr != 0)
		anIf->ifMacAddr = *client.UserDefinedMacAddr;
#endif

	if (client.client_id != 0) {
		struct shared_bindata *p;
		convertClient(&p, client.client_id, anIf->ifName,
		    &anIf->ifMacAddr, anIf->lli.ord);
		anIf->ifsendh->stags[TAG_CLIENT_ID].dhcsb = p;
		onhost(anIf->ifsendh->flags, TAG_CLIENT_ID);
	}

	anIf->ifsendh->hw = anIf->ifMacAddr;
	if (anIf->ifsendh->Hwaddr != NULL)
		(void) free(anIf->ifsendh->Hwaddr);
	anIf->ifsendh->Hwaddr = xstrdup(addrstr(anIf->ifsendh->hw.chaddr,
	    anIf->ifsendh->hw.hlen, TRUE, ':'));
	onhost(anIf->ifsendh->flags, TAG_HWADDR);

	if (ioctl(client.sock, SIOCGIFMTU, (caddr_t)&ifr) == 0) {
		anIf->ifsendh->stags[TAG_DHCP_SIZE].dhcd2 = ifr.ifr_metric -
		    (sizeof (struct ip) + sizeof (struct udphdr));
		onhost(anIf->ifsendh->flags, TAG_DHCP_SIZE);
	}

	status = read_dhc(client.dhcpconfigdir, anIf->ifName,
	    &anIf->ifOldConfig);
	if (status) {
		anIf->ifState = DHCP_SELECTING;
	} else if (leaseExpired(anIf->ifOldConfig->lease_expires)) {
		anIf->ifState = DHCP_SELECTING;
		discard_dhc(anIf);
		free_host(anIf->ifOldConfig);
		anIf->ifOldConfig = 0;
	} else
		anIf->ifState = DHCP_REBOOTING;

	if (!client.useNonVolatile)
		discard_dhc(anIf);

	return (0);
}
