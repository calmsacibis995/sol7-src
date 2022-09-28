/*
 * lli.c: "Link level interface utilities".
 *
 * SYNOPSIS
 * void liClose   (LLinfo *r)
 * int liOpen     (const char *ifname, LLinfo *)
 * int liBroadcast(const LLinfo *r, void *data, int datalen,
 *    const struct in_addr *src)
 * int liRead     (int fd, void *packet, int *datalen, void *sender,
 *    void *receiver)
 *
 * DESCRIPTION
 *    Setup link level interface for send and receipt of DHCP packets.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)lli.c 1.4 96/11/25 SMI"

#include "unixgen.h"
#include "error.h"
#include "msgindex.h"
#include "ca_dlpi.h"
#include "lli.h"
#include "client.h"
#include "utils.h"
#include "catype.h"
#include <sys/errno.h>
#include <sys/pfmod.h>
#include <stdio.h>
#include <fcntl.h>
#include <stropts.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#define	ABS(x) ((x) >= 0 ? (x) : -(x))
#ifndef IPPORT_BOOTPS
#define	IPPORT_BOOTPS	67	/* default UDP port numbers, server .. */
#define	IPPORT_BOOTPC	68	/* ..and client. */
#endif
#ifndef ETHERTYPE_IP
#define	ETHERTYPE_IP	0x0800
#endif

static u_short ip_id;
#define	OFFSET	0

static void
createBOOTPfilter(struct packetfilt *pf)
{
	register u_short *fwp = pf->Pf_Filter;
	register u_short initoffset = OFFSET;
	u_short offset;

	/* Test that protocol is UDP */

	/* ttl and protocol field */
	offset = (initoffset + 8) / sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_AND;
	*fwp++ = htons(0x00ff);	/* want the protocol part.. */
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = htons(IPPROTO_UDP);	/* .. to be UDP */

	/* Must be the first fragment */
	/* fragments and IP offset field */
	offset = (initoffset + 6) / sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_AND;
	*fwp++ = htons(0x1fff); /* want the offset part.. */
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = 0; /* ..equal to zero */

	/* Test destination port# is BOOTP client */
	offset = (initoffset + 22) / sizeof (u_short);
	*fwp++ = ENF_PUSHWORD + offset;
	*fwp++ = ENF_PUSHLIT | ENF_CAND;
	*fwp++ = htons(IPPORT_BOOTPC);

	pf->Pf_FilterLen = fwp - pf->Pf_Filter;
}

void
liClose(LLinfo *r)
{
	if (r->macaddr)
		free(r->macaddr);
	if (r->brdaddr)
		free(r->brdaddr);
	r->macaddr = r->brdaddr = 0;
	r->lmacaddr = r->lbrdaddr = 0;
	r->bpmactype = 0;
	close(r->fd);
	r->fd = -1;
}

int
liOpen(const char *ifname, LLinfo *r)
{
	int rc;
	int ppa;
	char *device = NULL;
	Jodlpiu buf;
	dl_info_ack_t *d = &buf.d.info_ack;
	struct packetfilt pf;

	/* Initialize so that state can be identified: */

	r->fd = -1;
	r->macaddr = r->brdaddr = 0;
	r->lmacaddr = r->lbrdaddr = 0;
	r->bpmactype = 0;
	r->ord = 0;

	iftoppa(ifname, &device, &ppa, &r->ord);
	if (ppa < 0) {
		loge(DHCPCMSG00, ifname);
		return (ERR_LLI_PPA);
	}
	r->fd = open(device, O_RDWR);
	if (r->fd < 0) {
		free(device);
		loge(DHCPCMSG41, device, SYSMSG);
		return (ERR_LLI_OPEN);
	}
	free(device);

	rc = dlattach(r->fd, ppa);
	if (rc < 0) {
		liClose(r);
		return (ERR_LLI_ATTACH);
	}

	rc = dlbind(r->fd, ETHERTYPE_IP, DL_CLDLS);
	if (rc < 0) {
		liClose(r);
		return (ERR_LLI_BIND);
	}

	rc = dlinfo(r->fd, &buf);
	if (rc < 0) {
		liClose(r);
		return (ERR_LLI_PROTO);
	}

	if (d->dl_mac_type != DL_ETHER) { /* ##BUG## allow other HW */
		loge(DHCPCMSG01, ifname);
		liClose(r);
		return (ERR_LLI_NOT_ETHERNET);
	}
	r->bpmactype = DLPItoBOOTPmediaType(d->dl_mac_type);

	r->lbrdaddr = d->dl_brdcst_addr_length + ABS(d->dl_sap_length);
	r->brdaddr = (char *)xmalloc(r->lbrdaddr);
	if (!r->brdaddr) {
		liClose(r);
		return (ERR_OUT_OF_MEMORY);
	}

	if (d->dl_sap_length > 0) {  /* SAP comes before: */
		r->lmacaddr = d->dl_addr_length - d->dl_sap_length;
		memcpy(r->brdaddr, buf.b + d->dl_addr_offset,
		    d->dl_sap_length);

		memcpy(r->brdaddr + d->dl_sap_length,
		    buf.b + d->dl_brdcst_addr_offset,
		    d->dl_brdcst_addr_length);
	} else { /* SAP comes after */
		r->lmacaddr = d->dl_addr_length + d->dl_sap_length;
		memcpy(r->brdaddr, buf.b + d->dl_brdcst_addr_offset,
		    d->dl_brdcst_addr_length);
		memcpy(r->brdaddr + d->dl_brdcst_addr_length,
		    buf.b + d->dl_addr_offset + d->dl_addr_length +
		    d->dl_sap_length, -d->dl_sap_length);
	}

	r->macaddr = (char *)xmalloc(r->lmacaddr);
	if (!r->macaddr) {
		liClose(r);
		return (ERR_OUT_OF_MEMORY);
	}

	rc = dlgetphysaddr(r->fd, r->macaddr, r->lmacaddr);
	if (rc < 0) {
		loge(DHCPCMSG44, ifname, SYSMSG);
		liClose(r);
		return (ERR_LLI_PROTO);
	}

	createBOOTPfilter(&pf);

	if (ioctl(r->fd, I_PUSH, "pfmod") == -1) {
		loge(DHCPCMSG02, ifname, SYSMSG);
		liClose(r);
		return (ERR_PF_PUSH);
	}

	if (strioctl(r->fd, PFIOCSETF, 0, sizeof (pf), (void *)&pf) == -1) {
		loge(DHCPCMSG03, ifname, SYSMSG);
		liClose(r);
		return (ERR_PF_SET);
	}
	ioctl(r->fd, I_FLUSH, FLUSHR); /* chuck accumulated rubbish */
	ioctl(r->fd, I_SETSIG, S_RDNORM);
	return (0);
}

int
liBroadcast(const LLinfo *r, void *data, int datalen, const struct in_addr *src)
{
	struct ip ih;
	struct udphdr uh;
	char *databuf;
	int rc;

	ih.ip_v = 4;
	ih.ip_hl = 5;
	ih.ip_tos = 0;
	ih.ip_id = htons(++ip_id);
	ih.ip_off = 0;
	ih.ip_ttl = 255;
	ih.ip_p = IPPROTO_UDP;
	if (src)
		ih.ip_src = *src;
	else
		ih.ip_src.s_addr = htonl(0);
	ih.ip_dst.s_addr = htonl(INADDR_BROADCAST);
	ih.ip_len = sizeof (struct ip) + sizeof (struct udphdr) + datalen;
	ih.ip_len = htons(ih.ip_len);
	ih.ip_sum = 0; /* so that checksum calculated correctly */
	ih.ip_sum = in_cksum((u_short *)&ih, sizeof (struct ip));

	uh.uh_ulen = sizeof (struct udphdr) + datalen;
	uh.uh_ulen = htons(uh.uh_ulen);
	uh.uh_sport = htons((short)IPPORT_BOOTPC);
	uh.uh_dport = htons((short)IPPORT_BOOTPS);
	uh.uh_sum = 0;

	databuf = (char *)xmalloc(sizeof (struct ip) +
	    sizeof (struct udphdr) + datalen);
	if (!databuf)
		return (ERR_OUT_OF_MEMORY);
	memcpy(databuf, &ih, sizeof (struct ip));
	memcpy(databuf + sizeof (struct ip), &uh, sizeof (struct udphdr));
	memcpy(databuf + sizeof (struct ip) + sizeof (struct udphdr), data,
	    datalen);

	rc = dlunitdatareq(r->fd, r->brdaddr, r->lbrdaddr, databuf,
	    sizeof (struct ip) + sizeof (struct udphdr) + datalen);
#if DEBUG
	if (debug >= 3)
		logb("sending to <DLSAP> <%s>\n", addrstr(r->brdaddr,
		    r->lbrdaddr, TRUE, ':'));
	if (debug >= 6)
		YCDISP(stdbug, databuf, "Outgoing IP packet is:\n",
		    datalen + sizeof (struct ip) + sizeof (struct udphdr));
#endif
	free(databuf);
	return (rc < 0 ? rc : datalen);
}

int
liRead(int fd, void *packet, int *datalen, void *sender, void *receiver)
{
	struct strbuf ctrl, data;
	char ctrlbuf[1024], databuf[2048];
	int rc;
	int flags = 0;
	struct ip ih;

	ctrl.buf = ctrlbuf;
	ctrl.maxlen = sizeof (ctrlbuf);
	data.buf = databuf;
	data.maxlen = sizeof (databuf);
	ctrl.len = data.len = 0;

	rc = getmsg(fd, &ctrl, &data, &flags);
	if (rc < 0)
		return (-1);

#if DEBUG
	if (debug >= 6) {
		if (ctrl.len > 0) /* should never occur */
			YCDISP(stdbug, ctrl.buf,
			    "control message at stream head\n", ctrl.len);
		YCDISP(stdbug, data.buf,
		    "data message at stream head\n", data.len);
	}
#endif
	data.len -= (sizeof (struct ip) + sizeof (struct udphdr));
	*datalen = *datalen > data.len ? data.len : *datalen;
	memcpy(packet, databuf + sizeof (struct ip) + sizeof (struct udphdr),
	    *datalen);

	/*  We know this is IP */
	memcpy(&ih, databuf, sizeof (struct ip));
#if DEBUG
	if (debug >= 6)
		showIPheader(stdbug, &ih);
#endif
	if ((ih.ip_off & htons(0x1fff)) != 0)
		return (-1);
	if (((ih.ip_off & htons(0x1fff)) == 0) && (ih.ip_off & IP_MF)) {
		logw(DHCPCMSG42, ADDRSTR(&ih.ip_src));
		return (-1);
	}

	if (sender) {
		memcpy(sender, databuf + sizeof (struct ip), 2); /* port# */
		memcpy((char *)sender + 2, &ih.ip_src, 4); /* IP source */
	}
	if (receiver) {
		memcpy(receiver, databuf + sizeof (struct ip) + 2,
		    2); /* port# */
		memcpy((char *)receiver + 2, &ih.ip_dst, 4);
	}
	return (rc);
}
