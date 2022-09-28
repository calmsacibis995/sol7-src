/*
 * showhdrs.c: "Dump various headers of IP protocol packets".
 *
 * SYNOPSIS
 *    void showARPpacket     (const struct ether_arp *u)
 *    void showEthernetHeader(const struct ether_header *u)
 *    void showUDPheader     (FILE *f, const struct udphdr *up)
 *    void showIPheader      (FILE *f, const struct ip *ip)
 *
 * DESCRIPTION
 *    Dump various packet headers.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)showhdrs.c 1.5 96/12/26 SMI"

#include "utils.h"
#include "catype.h"
#include "unixgen.h"
#include <stdio.h>
#include <arpa/inet.h>
#include "catype.h"
#include <sys/socket.h> /* <- if_arp.h */
#include <net/if.h>
#include <netinet/in.h> /* for struct in_addr defn. */
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#ifdef	DEBUG
static const char *arpopstr[] = {
	"ARPOP_REQUEST", "ARPOP_REPLY", "REVARP_REQUEST", "REVARP_REPLY",
	"unknown"
};

static int arpopary[] = {
	ARPOP_REQUEST, ARPOP_REPLY, REVARP_REQUEST, REVARP_REPLY, -1
};

static char *ethtypestr[] = {
	"ETHERTYPE_PUP", "ETHERTYPE_IP", "ETHERTYPE_ARP", "ETHERTYPE_REVARP",
	"unknown"
};

static int ethtypeary[] = {
	ETHERTYPE_PUP, ETHERTYPE_IP, ETHERTYPE_ARP, ETHERTYPE_REVARP, -1
};

static const char *protostr[] = {
	"IPPROTO_IP", "IPPROTO_ICMP", "IPPROTO_IGMP", "IPPROTO_GGP",
	"IPPROTO_TCP", "IPPROTO_EGP", "IPPROTO_PUP", "IPPROTO_UDP",
	"IPPROTO_IDP", "IPPROTO_HELLO", "IPPROTO_ND", "IPPROTO_RAW",
	"unknown"
};

static int protoary[] = {
	IPPROTO_IP, IPPROTO_ICMP, IPPROTO_IGMP, IPPROTO_GGP, IPPROTO_TCP,
	IPPROTO_EGP, IPPROTO_PUP, IPPROTO_UDP, IPPROTO_IDP, IPPROTO_HELLO,
	IPPROTO_ND, IPPROTO_RAW, -1
};

static const char *
ARPopStr(int arpop)
{
	register int n;

	for (n = 0; arpopary[n] >= 0 && arpopary[n] != arpop; n++)
		/* NULL statement */;
	return (arpopstr[n]);
}

static const char *
etherTypeStr(int ethtype)
{
	register int n;

	for (n = 0; ethtypeary[n] >= 0 && ethtypeary[n] != ethtype; n++)
		/* NULL statement */;
	return (ethtypestr[n]);
}

void
showARPpacket(const struct ether_arp *u)
{
	logb("\t ARP body:\n");
	logb("\t\t hardware type = %0#x\n", ntohs(u->arp_hrd));
	logb("\t\t protocol type = %0#x (%s)\n",
	ntohs(u->arp_pro), etherTypeStr(ntohs(u->arp_pro)));
	logb("\t\t hardware address length = %d\n", u->arp_hln);
	logb("\t\t protocol address length = %d\n", u->arp_pln);
	logb("\t\t opcode = %d (%s)\n\n", ntohs(u->arp_op),
	    ARPopStr(ntohs(u->arp_op)));
	logb("\t\t source hardware address = %s\n",
	addrstr(&u->arp_sha, u->arp_hln, TRUE, ':'));
	logb("\t\t source protocol address = %s\n",
	addrstr(u->arp_spa, u->arp_pln, FALSE, '.'));
	logb("\t\t target hardware address = %s\n",
	addrstr(&u->arp_tha, u->arp_hln, TRUE, ':'));
	logb("\t\t target protocol address = %s\n",
	addrstr(u->arp_tpa, u->arp_pln, FALSE, '.'));
}

static const char *
IPprotoStr(int proto)
{
	register int n;

	for (n = 0; protoary[n] >= 0 && protoary[n] != proto; n++)
		/* NULL statement */;
	return (protostr[n]);
}

void
showIPheader(FILE *f, const struct ip *ip)
{
	uint16_t offset;
	uint16_t fragments;

	offset = ntohs(ip->ip_off) & (uint16_t)0x1fff;
	fragments = ntohs(ip->ip_off) & ((uint16_t)IP_DF | (uint16_t)IP_MF);

	fprintf(f, "ip version\t= %u\n", ip->ip_v);
	fprintf(f, "ip hdr length\t= %u\n", ip->ip_hl * 4);
	fprintf(f, "ip tos\t\t= %0#x\n", ip->ip_tos);
	fprintf(f, "ip length\t\t= %u\n", ntohs(ip->ip_len));
	fprintf(f, "ip identification\t= %u\n", ntohs(ip->ip_id));
	fprintf(f, "ip fragment offset\t= %u\n", offset);
	fprintf(f, "ip fragment bits\t= %0#x\n", fragments);
	fprintf(f, "ip ttl\t\t= %u\n", ip->ip_ttl);
	fprintf(f, "ip protocol\t= %u (%s)\n", ip->ip_p, IPprotoStr(ip->ip_p));
	fprintf(f, "ip checksum\t= %0#x\n", ntohs(ip->ip_sum));
	fprintf(f, "ip source\t\t= %s\n", ADDRSTR((char *)&ip->ip_src));
	fprintf(f, "ip destination\t= %s\n", ADDRSTR((char *)&ip->ip_dst));
}

void
showEthernetHeader(const struct ether_header *u)
{
	logb("\t ethernet header:\n");
	logb("\t\t dhost=%s\n", addrstr(&u->ether_dhost, 6, 1, ':'));
	logb("\t\t shost=%s\n", addrstr(&u->ether_shost, 6, 1, ':'));
	logb("\t\t type=%0#x (%s)\n\n",
	    ntohs(u->ether_type), etherTypeStr(ntohs(u->ether_type)));
}

void
showUDPheader(FILE *f, const struct udphdr *up)
{
	fprintf(f, "udp source port = %d\n", ntohs(up->uh_sport));
	fprintf(f, "udp dest   port = %d\n", ntohs(up->uh_dport));
}
#endif	/* DEBUG */
