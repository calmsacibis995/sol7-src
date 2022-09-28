/*
 * arpcheck.c: "Construct and send an ARP packet and wait for reply".
 *
 * SYNOPSIS
 *    int arpCheck(const char *ifname,int32_t timeout,
 *		const struct in_addr *arpedIPaddr,
 *		void *myHardwareAddress)
 *
 * DESCRIPTION
 *	Construct and send an ARP packet and wait a finite interval for reply
 *	(which should not come if DHCP is working correctly).
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)arpcheck.c 1.3 96/11/25 SMI"

#include "catype.h"
#include "ca_time.h"
#include "camacros.h"
#include "utils.h"
#include "utiltxt.h"
#include "unixgen.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/sockio.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <sys/socket.h> /* <- if_arp.h <- if_ether.h */
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" int putmsg(int, const struct strbuf *, const struct strbuf *, int);
#endif

struct ether_packet {
	struct ether_header	eh;
	struct ether_arp	ah;
	char			zero[18]; /* ethernet packet min of 60 chars */
};

#include "ca_dlpi.h"
#include <sys/pfmod.h>

static int
pushFilter(int fd, const struct packetfilt *pf)
{
	if (ioctl(fd, I_PUSH, "pfmod") == -1) {
		loge(UTILMSG09, "", SYSMSG);
		return (-1);
	}
	if (strioctl(fd, PFIOCSETF, 0, sizeof (*pf), (void*)pf) == -1) {
		loge(UTILMSG03, SYSMSG);
		return (-1);
	}
	return (0);
}

static int
rddev(int fd, int32_t t)
{
	int n;
	fd_set readfds;
	struct timeval timeout;

	timeout.tv_sec = t;
	timeout.tv_usec = 0;
	FD_ZERO(&readfds);
	for (;;) {
		FD_SET(fd, &readfds);
		n = select(255, &readfds, (fd_set *)0, (fd_set *)0, &timeout);
		if (n < 0) {
			if (errno != EINTR)
				return (-1);
			else
				continue;	/* interrupted by signal */
		} else if (n == 0) {
			return (0);	/* timed out */
		} else if (FD_ISSET(fd, &readfds)) {
			return (1);	/* response received */
		} else {
			break;		/* should never get here */
		}
	}
	return (0);
}

static void
setupFilter(struct packetfilt *pf, const struct in_addr *sya)
{
	register u_short *fwp = pf->Pf_Filter;
	u_short offset;
	union {
		uint32_t ul;
		u_short us[2];
	} u;

	u.ul = sya->s_addr;

	/*
	 * Set up filter.  Test the protocol address of the sender to ensure
	 * it's what we were listening for. Then test the type field for
	 * ETHERTYPE_ARP and lastly the protocol type == ARPOP_REPLY
	 */

	offset = MEMBER_OFFSET(struct ether_packet, ah.arp_spa) /
	    sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	memcpy(fwp++, u.us, sizeof (short));

	offset++;
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	memcpy(fwp++, u.us + 1, sizeof (short));

	offset = MEMBER_OFFSET(struct ether_header, ether_type) /
	    sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = htons((short)ETHERTYPE_ARP);

	offset = MEMBER_OFFSET(struct ether_packet, ah.arp_op) /
	    sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_EQ;
	*fwp++ = htons((short)ARPOP_REPLY);

	pf->Pf_FilterLen = fwp - pf->Pf_Filter;
}

static void
constructARPpacket(struct ether_packet *packet, int ARPop,
    const struct in_addr *myInternetAddress,
    const struct in_addr *reqInternetAddress,
    void *myHardwareAddress, void *replyHardwareAddress)
{
	memset(packet, 0, sizeof (struct ether_packet));

	packet->ah.ea_hdr.ar_hrd = htons((short)ARPHRD_ETHER);
	packet->ah.ea_hdr.ar_pro = htons((short)ETHERTYPE_IP);
	packet->ah.ea_hdr.ar_hln = 6;
	packet->ah.ea_hdr.ar_pln = 4;
	packet->ah.ea_hdr.ar_op  = htons((short)ARPop);
	packet->eh.ether_type = htons((short)ETHERTYPE_ARP);


	memset(&packet->eh.ether_dhost, 0xff, 6);
	memcpy(&packet->eh.ether_shost, myHardwareAddress, 6);

	if (ARPop == ARPOP_REPLY) {
		memcpy(&packet->ah.arp_sha, replyHardwareAddress, 6);
		memset(&packet->ah.arp_tha, 0xff, 6);
		memcpy(packet->ah.arp_spa, reqInternetAddress, 4);
		memset(packet->ah.arp_tpa, 0xff, 4);
	} else {  /* ARPOP_REQUEST */
		memcpy(&packet->ah.arp_sha, myHardwareAddress, 6);
		if (myInternetAddress)
			memcpy(packet->ah.arp_spa, myInternetAddress, 4);
		memcpy(packet->ah.arp_tpa, reqInternetAddress, 4);
	}

#if DEBUG
	if (debug >= 6) {
		YCDISP(stdbug, packet, "ARP packet is:\n",
		    sizeof (struct ether_packet));
		putchar('\n');
		showEthernetHeader(&packet->eh);
		showARPpacket(&packet->ah);
	}
#endif
}

int
arpCheck(const char *ifname, int32_t timeout,
    const struct in_addr *arpedIPaddr, void *myHardwareAddress)
{
	char *device = NULL;
	int ord, ppa, fd, rc = -1;
	struct packetfilt pf;
	struct ether_packet packet;

	iftoppa(ifname, &device, &ppa, &ord);

	fd = open(device, O_RDWR);
	if (fd < 0) {
		loge(UTILMSG00, device, SYSMSG);
		if (device)
			free(device);
		return (-1);
	}
	if (device)
		free(device);

	rc = dlattach(fd, ppa);
	if (rc < 0)
		goto cleanup;

	rc = dlbind(fd, ETHERTYPE_ARP, DL_CLDLS);
	if (rc < 0)
		goto cleanup;

	constructARPpacket(&packet, ARPOP_REQUEST, 0, arpedIPaddr,
	    myHardwareAddress, 0);

	setupFilter(&pf, arpedIPaddr);
	if (pushFilter(fd, &pf)) {
		rc = -1;
		goto cleanup;
	}

	if (strioctl(fd, DLIOCRAW, -1, 0, 0) < 0) {
		loge(UTILMSG06, SYSMSG);
		rc = -1;
		goto cleanup;
	}

	if (write(fd, &packet, sizeof (packet)) < 0) {
		loge(UTILMSG03, SYSMSG);
		rc = -1;
		goto cleanup;
	}
	rc = rddev(fd, timeout);
cleanup:
	close(fd);
	return (rc);
}
