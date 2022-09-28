/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 * Copyright (c) 1988, 1989, 1991, 1994, 1995, 1996, 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * @(#)$Header: traceroute.c,v 1.49 97/06/13 02:30:23 leres Exp $ (LBL)
 */

#pragma ident   "@(#)traceroute.c 1.2     98/01/27 SMI"


#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <malloc.h>
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>
#include <locale.h>

#include "ifaddrlist.h"


/*
 * Maximum number of gateways (include room for one noop)
 * 'in_addr_t' is 32 bits, size of IPv4 address
 */
#define	NGATEWAYS ((int)((MAX_IPOPTLEN - IPOPT_MINOFF - 1) / \
			sizeof (in_addr_t)))

#define	MAXPACKET 65535
#define	ICMP_UNREACH_FILTER_PROHIB 13	/* admin prohibited filter RFC1716 */

#define	Fprintf (void)fprintf
#define	Printf (void)printf


/* Host name and address list */
struct hostinfo {
	char *name;
	int n;
	in_addr_t *addrs;
};

/* Data section of the probe packet */
struct outdata {
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	struct timeval tv;	/* time packet left */
};

/*
 * LBNL bug fixed: in LBNL traceroute 'u_char packet[512];'
 * not sufficient to hold the complete packet for ECHO REPLY of a big probe
 * packet size reported incorrectly in such a case
 */
u_char	packet[MAXPACKET];	/* last inbound (icmp) packet */

struct ip *outip;		/* last output (udp) packet */
struct udphdr *outudp;		/* last output (udp) packet */
struct outdata *outdata;	/* last output (udp) packet */

struct icmp *outicmp;		/* last output (icmp) packet */

/* loose source route gateway list (including room for final destination) */
in_addr_t gwlist[NGATEWAYS + 1];

int s;				/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp/icmp) socket file descriptor */
int lsrr = 0;
struct sockaddr whereto;	/* Who to try to reach */
struct sockaddr_in wherefrom;	/* Who we are */
int packlen = 0;		/* total length of packet */
int minpacket;			/* min ip packet size */
/*
 * LBNL bug fixed: LBNL using 32K
 */
int maxpacket = MAXPACKET;	/* max ip packet size */

char *prog;
char *source;
char *hostname;
char *device;

int nprobes = 3;
int max_ttl = 30;
int first_ttl = 1;
u_short ident;
u_short port = 32768 + 666;	/* start udp dest port # for probe packets */

int options = 0;		/* socket options */
int verbose = 0;		/* verbose output */
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag = 0;			/* print addresses numerically */
int useicmp = 0;		/* use icmp echo instead of udp packets */
int docksum = 1;		/* calculate checksums */
int settos = 0;			/* set type-of-service field */
int optlen = 0;			/* length of ip options */

extern int optind;
extern int opterr;
extern char *optarg;

/* Forwards */
double	deltaT(struct timeval, struct timeval);
void	freehostinfo(struct hostinfo *);
void	getaddr(in_addr_t *, char *);
struct	hostinfo *gethostinfo(char *);
u_short	in_cksum(u_short *, int);
char	*inetname(struct in_addr);
void	main(int, char **);
int	packet_ok(u_char *, int, struct sockaddr_in *, int);
char	*pr_type(u_char);
void	print(u_char *, int, struct sockaddr_in *);
void	send_probe(int, int, struct timeval *);
void	setsin(struct sockaddr_in *, in_addr_t);
int	str2val(const char *, const char *, int, int);
void	tvsub(struct timeval *, struct timeval *);
void 	usage(void);
int	wait_for_reply(int, struct sockaddr_in *, struct timeval *);
void 	traceroute(struct in_addr);

/*
 * main
 */
void
main(int argc, char **argv)
{
	int op, n;
	char *cp;
	u_char *outp;
	in_addr_t *ap;
	struct sockaddr_in *from = &wherefrom;
	struct sockaddr_in *to = (struct sockaddr_in *)&whereto;
	struct hostinfo *hi_src, *hi_dst;
	int on = 1;
	struct protoent *pe;
	int i;
	int tos = 0;
	u_short off = 0;
	struct ifaddrlist *al, *tmp1_al, *tmp2_al;
	char errbuf[ERRBUFSIZE];

	(void) setlocale(LC_ALL, "");

	/* internationalization magic, copied from rlogin.c */
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);


	if ((cp = strrchr(argv[0], '/')) != NULL)
		prog = cp + 1;
	else
		prog = argv[0];

	opterr = 0;
	while ((op = getopt(argc, argv, "dFInrvxf:g:i:m:p:q:s:t:w:"))
		!= EOF) {
		switch (op) {
		case 'd':
			options |= SO_DEBUG;
			break;

		case 'f':
			first_ttl = str2val(optarg, gettext("first ttl"),
						1, 255);
			break;

		case 'F':
			off = IP_DF;
			break;

		case 'g':
			if (lsrr >= NGATEWAYS) {
				Fprintf(stderr,
				    gettext("%s: No more than %d gateways\n"),
				    prog, NGATEWAYS);
				exit(EXIT_FAILURE);
			}
			getaddr(gwlist + lsrr, optarg);
			++lsrr;
			break;

		case 'i':
			device = optarg;
			break;

		case 'I':
			++useicmp;
			break;

		case 'm':
			max_ttl = str2val(optarg, gettext("max ttl"), 1, 255);
			break;

		case 'n':
			++nflag;
			break;

		case 'p':
			port = str2val(optarg, gettext("port"), 1, -1);
			break;

		case 'q':
			nprobes = str2val(optarg, gettext("nprobes"), 1, -1);
			break;

		case 'r':
			options |= SO_DONTROUTE;
			break;

		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;

		case 't':
			tos = str2val(optarg, gettext("tos"), 0, 255);
			++settos;
			break;

		case 'v':
			++verbose;
			break;

		case 'x':
			docksum = 0;
			break;

		case 'w':
			waittime = str2val(optarg, gettext("wait time"), 2, -1);
			break;

		default:
			usage();
		}
	}
	if (first_ttl > max_ttl) {
		Fprintf(stderr,
		    gettext("%s: first ttl (%d) may not be greater than max "\
				"ttl (%d)\n"),
		    prog, first_ttl, max_ttl);
		exit(EXIT_FAILURE);
	}


	if (!docksum)
		Fprintf(stderr, gettext("%s: Warning: checksums disabled\n"),
			prog);

	/*
	 * LBNL bug fixed: miscalculation of optlen
	 */
	if (lsrr > 0) {
		/*
		 * 5 (NO OPs) + 3 (code, len, ptr) + gateways
		 * IP options field can hold upto 9 gateways. But the API
		 * allows you to specify only 8, because last one is the
		 * destination host. When this packet is sent, on the wire
		 * you see one gateway replaced by 4 NO OPs. The other 1 NO
		 * OP is for alignment
		 */
		optlen = 8 + lsrr * sizeof (gwlist[0]);
	}
	minpacket = sizeof (*outip) + sizeof (*outdata) + optlen;
	if (useicmp)
		minpacket += ICMP_MINLEN;	/* minimum ICMP header size */
	else
		minpacket += sizeof (*outudp);
	if (packlen == 0)
		packlen = minpacket;		/* minimum sized packet */
	else if (minpacket > packlen || packlen > maxpacket) {
		Fprintf(stderr, gettext("%s: packet size must be "\
			"%d <= s <= %d\n"), prog, minpacket, maxpacket);
		exit(EXIT_FAILURE);
	}

	/* Process destination and optional packet size */
	i = argc - optind;
	if (i == 1 || i == 2) {
		hostname = argv[optind];
		hi_dst = gethostinfo(hostname);
		setsin(to, hi_dst->addrs[0]);
		if (hi_dst->n > 1)
			Fprintf(stderr,
			gettext("%s: Warning: %s has multiple addresses;"\
				" using %s\n"), prog, hostname,
				inet_ntoa(to->sin_addr));
		hostname = hi_dst->name;
		hi_dst->name = NULL;

		if (i == 2)
			/*
			 * LBNL bug fixed: used to accept [minpacket, infinity)
			 */
			packlen = str2val(argv[optind + 1], "packet length",
					minpacket, maxpacket);
	} else {
		usage();
	}

	if (lsrr && (options & SO_DONTROUTE)) {
		Fprintf(stderr, gettext("%s: loose source route gateways (-g)"\
			" cannot be specified when probe packets are sent"\
			" directly to a host on an attached network (-r)\n"),
			prog);
		exit(EXIT_FAILURE);
	}

	(void) setlinebuf(stdout);

	outip = (struct ip *)malloc((unsigned)packlen);
	if (outip == NULL) {
		Fprintf(stderr, "%s: malloc: %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}
	(void) memset((char *)outip, 0, packlen);

	outip->ip_v = IPVERSION;
	if (settos)
		outip->ip_tos = tos;

	/*
	 * LBNL bug fixed: missing '- optlen' before, causing optlen
	 * added twice
	 *
	 * BSD bug: BSD touches the header fields 'len' and 'ip_off' even
	 * when HDRINCL is set. It applies htons() on these fields. It should
	 * send the header untouched when HDRINCL is set.
	 */
	outip->ip_len = htons(packlen - optlen);

	/*
	 * XXX
	 * Bug in icmp.c has to be fixed to get traceroute to work for x86.
	 * Byte order in x86 is not same as network byte order. The 'ip_off'
	 * field of raw packet is ANDed with IPH_DF, it should be htons(IPH_DF)
	 */
	outip->ip_off = htons(off);
	outp = (u_char *)(outip + 1);

	outip->ip_hl = (outp - (u_char *)outip) >> 2;
	ident = (getpid() & 0xffff) | 0x8000;
	if (useicmp) {
		outip->ip_p = IPPROTO_ICMP;

		outicmp = (struct icmp *)outp;
		outicmp->icmp_type = ICMP_ECHO;
		outicmp->icmp_id = htons(ident);

		outdata = (struct outdata *)(outp + ICMP_MINLEN);
	} else {
		outip->ip_p = IPPROTO_UDP;

		outudp = (struct udphdr *)outp;
		outudp->uh_sport = htons(ident);
		outudp->uh_ulen =
		    htons((u_short)(packlen - (sizeof (*outip) + optlen)));
		outdata = (struct outdata *)(outudp + 1);
	}

	cp = "icmp";
	if ((pe = getprotobyname(cp)) == NULL) {
		Fprintf(stderr, gettext("%s: unknown protocol %s\n"), prog, cp);
		exit(EXIT_FAILURE);
	}
	if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0) {
		Fprintf(stderr, "%s: icmp socket: %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (options & SO_DEBUG)
		(void) setsockopt(s, SOL_SOCKET, SO_DEBUG, (char *)&on,
		    sizeof (on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char *)&on,
		    sizeof (on));

	sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

	if (sndsock < 0) {
		Fprintf(stderr, "%s: raw socket: %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Revert to non-privileged user after opening sockets */
	(void) setuid(getuid());

	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&packlen,
	    sizeof (packlen)) < 0) {
		Fprintf(stderr, "%s: SO_SNDBUF: %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
	    sizeof (on)) < 0) {
		Fprintf(stderr, "%s: IP_HDRINCL: %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}

	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG, (char *)&on,
		    sizeof (on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
				(char *)&on, sizeof (on));

	/* Get the interface address list */
	n = ifaddrlist(&al, errbuf);
	if (n < 0) {
		Fprintf(stderr, "%s: ifaddrlist: %s\n", prog, errbuf);
		exit(EXIT_FAILURE);
	}
	if (n == 0) {
		Fprintf(stderr,
		    gettext("%s: Can't find any network interfaces\n"), prog);
		exit(EXIT_FAILURE);
	}

	tmp1_al = al;

	/* Look for a specific device */
	if (device != NULL) {
		/*
		 * XXX
		 * IF:   IP@:
		 * le0   a.b.c.d
		 * le0:1 w.x.y.z
		 *
		 * "traceroute -i le0 -s w.x.y.z host" gives error. But in BSD:
		 *
		 * IF:   IP@:
		 * le0   a.b.c.d
		 * le0   w.x.y.z
		 *
		 * This issue left to IPv6 extension.
		 */
		for (i = n; i > 0; --i, ++tmp1_al)
			if (strcoll(device, tmp1_al->device) == 0)
				break;
		if (i <= 0) {
			Fprintf(stderr, gettext("%s: Can't find interface"\
				" %s\n"), prog, device);
			exit(EXIT_FAILURE);
		}
	}

	/* Determine our source address */
	if (source == NULL) {
		/*
		 * If a device was specified, use the interface address.
		 * Otherwise, use the first interface found.
		 * Warn if there are more than one.
		 */

		setsin(from, tmp1_al->addr);
		if (n > 1 && device == NULL) {
			Fprintf(stderr, gettext("%s: Warning: Multiple"\
				" interfaces found; using %s @ %s\n"),
				prog, inet_ntoa(from->sin_addr),
				tmp1_al->device);
		}
	} else {
		tmp2_al = al;
		hi_src = gethostinfo(source);
		source = hi_src->name;
		hi_src->name = NULL;
		if (device == NULL) {
			setsin(from, hi_src->addrs[0]);
			/*
			 * LBNL bug fixed: used to accept any src address
			 */
			for (i = n; i > 0; --i, ++tmp2_al)
				if (tmp2_al->addr == hi_src->addrs[0])
					break;
			if (i <= 0) {
				Fprintf(stderr, gettext("%s: %s is an invalid"\
					" source address.\n"), prog,
					inet_ntoa(from->sin_addr));
				exit(EXIT_FAILURE);
			}

			if (hi_src->n > 1)
				Fprintf(stderr,
					gettext("%s: Warning: %s has multiple "\
					"addresses; using %s\n"), prog, source,
					inet_ntoa(from->sin_addr));
		} else {
			/*
			 * Make sure the source specified matches the
			 * interface address.
			 */
			for (i = hi_src->n, ap = hi_src->addrs; i > 0;
				--i, ++ap)
				if (*ap == tmp1_al->addr)
					break;
			if (i <= 0) {
				Fprintf(stderr,
				    gettext("%s: %s is not on interface %s\n"),
				    prog, source, device);
				exit(EXIT_FAILURE);
			}
			setsin(from, *ap);
		}
		freehostinfo(hi_src);
	}
	outip->ip_src = from->sin_addr;


	setsin(to, hi_dst->addrs[0]);

	Fprintf(stderr, gettext("%s to %s (%s)"), prog, hostname,
		inet_ntoa(to->sin_addr));
	if (source)
		Fprintf(stderr, gettext(" from %s"), source);
	Fprintf(stderr, gettext(", %d hops max, %d byte packets\n"),
		max_ttl, packlen);
	(void) fflush(stderr);

	traceroute(to->sin_addr);


	free(al);
	freehostinfo(hi_dst);

	exit(EXIT_SUCCESS);
}

/*
 * trace the route to the host with given IP address
 */
void
traceroute(struct in_addr addr)
{
	int ttl, probe, i, code;
	int seq = 0;
	struct sockaddr_in *from = &wherefrom;
	struct protoent *pe;
	char *cp;

	outip->ip_dst = addr;

	if (lsrr > 0) {
		u_char optlist[MAX_IPOPTLEN];

		cp = "ip";
		if ((pe = getprotobyname(cp)) == NULL) {
			Fprintf(stderr, gettext("%s: unknown protocol %s\n"),
				prog, cp);
			exit(EXIT_FAILURE);
		}

		/* make sure #gateways is still reasonable (extra caution) */
		if (lsrr > NGATEWAYS) {
			Fprintf(stderr, gettext("%s: No more than %d"\
				" gateways\n"), prog, NGATEWAYS);
			exit(EXIT_FAILURE);
		}

		/* final hop */
		gwlist[lsrr] = addr.s_addr;

		/* force 4 byte alignment */
		optlist[0] = IPOPT_NOP;
		/* loose source route option */
		optlist[1] = IPOPT_LSRR;
		i = (lsrr + 1) * sizeof (gwlist[0]);
		/* 3 = 1 (code) + 1 (len) + 1 (ptr) */
		optlist[2] = i + 3;
		/* Pointer to LSRR addresses */
		optlist[3] = IPOPT_MINOFF;
		/*
		 * copy the gateways into the optlist, they should begin
		 * at offset 4 (NOP+code+len+ptr)
		 */
		(void) memcpy(optlist + 4, gwlist, i);

		if ((setsockopt(sndsock, pe->p_proto, IP_OPTIONS,
				(const char *)optlist,
				i + sizeof (gwlist[0]))) < 0) {
			Fprintf(stderr, "%s: IP_OPTIONS: %s\n", prog,
				strerror(errno));
			exit(EXIT_FAILURE);
		}
	}


	for (ttl = first_ttl; ttl <= max_ttl; ++ttl) {
		in_addr_t lastaddr = 0;
		int got_there = 0;
		int unreachable = 0;


		if ((ttl == (first_ttl + 1)) && (options & SO_DONTROUTE)) {
			Fprintf(stderr, gettext("%s: host %s is not on a"\
				" directly-attached network\n"), prog,
				hostname);
			break;
		}

		Printf("%2d ", ttl);
		for (probe = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;
			struct timezone tz;
			struct ip *ip;

			(void) gettimeofday(&t1, &tz);
			send_probe(++seq, ttl, &t1);
			while ((cc = wait_for_reply(s, from, &t1)) != 0) {
				(void) gettimeofday(&t2, &tz);
				i = packet_ok(packet, cc, from, seq);

				/* Skip short packet */
				if (i == 0)
					continue;

				if (from->sin_addr.s_addr != lastaddr) {
					print(packet, cc, from);
					lastaddr = from->sin_addr.s_addr;
				}

				Printf(gettext("  %.3f ms"), deltaT(t1, t2));

				ip = (struct ip *)packet;

				if (i == -2) {
					if (ip->ip_ttl <= 1)
						Printf(" !");

					++got_there;
					break;
				}


				/* time exceeded in transit */
				if (i == -1)
					break;
				code = i - 1;
				switch (code) {
				case ICMP_UNREACH_PORT:
					if (ip->ip_ttl <= 1)
						Printf(" !");

					++got_there;
					break;

				case ICMP_UNREACH_NET:
					++unreachable;
					Printf(" !N");
					break;

				case ICMP_UNREACH_HOST:
					++unreachable;
					Printf(" !H");
					break;

				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					Printf(" !P");
					break;

				case ICMP_UNREACH_NEEDFRAG:
					++unreachable;
					Printf(" !F");
					break;

				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					Printf(" !S");
					break;

				case ICMP_UNREACH_FILTER_PROHIB:
					++unreachable;
					Printf(" !X");
					break;

				default:
					++unreachable;
					Printf(" !<%d>", code);
					break;
				}
				break;
			}
			if (cc == 0)
				Printf(" *");

			(void) fflush(stdout);

		}

		(void) putchar('\n');
		if (got_there ||
		    (unreachable > 0 && unreachable >= nprobes -1))
			break;
	}
}

/*
 * wait until a reply arrives or timeout occurs
 * read the reply packet
 */
int
wait_for_reply(int sock, struct sockaddr_in *fromp, struct timeval *tp)
{
	fd_set fds;
	struct timeval now, wait;
	struct timezone tz;
	int cc = 0;
	int fromlen = sizeof (*fromp);
	int result;

	(void) FD_ZERO(&fds);
	FD_SET(sock, &fds);

	wait.tv_sec = tp->tv_sec + waittime;
	wait.tv_usec = tp->tv_usec;
	(void) gettimeofday(&now, &tz);
	tvsub(&wait, &now);

	result = select(sock + 1, &fds, NULL, NULL, &wait);

	if (result == -1)
		Fprintf(stderr, "%s: select: %s\n", prog, strerror(errno));
	else if (result > 0)
		cc = recvfrom(s, (char *)packet, sizeof (packet), 0,
				(struct sockaddr *)fromp, (uint *)&fromlen);

	return (cc);
}

/*
 * send a probe packet to the destination
 */
void
send_probe(int seq, int ttl, struct timeval *tp)
{
	int cc;
	struct udpiphdr *ui;
	struct ip tip;

	outip->ip_ttl = ttl;

	outip->ip_id = htons(ident + seq);

	/*
	 * In most cases, the kernel will recalculate the ip checksum.
	 * But we must do it anyway so that the udp checksum comes out
	 * right.
	 */
	if (docksum) {
		outip->ip_sum =
		    in_cksum((u_short *)outip, sizeof (*outip) + optlen);
		if (outip->ip_sum == 0)
			outip->ip_sum = 0xffff;
	}

	/* Payload */
	outdata->seq = seq;
	outdata->ttl = ttl;
	outdata->tv = *tp;

	if (useicmp)
		outicmp->icmp_seq = htons(seq);
	else
		outudp->uh_dport = htons(port + seq);

	/* (We can only do the checksum if we know our ip address) */
	if (docksum) {
		if (useicmp) {
			outicmp->icmp_cksum = 0;
			outicmp->icmp_cksum = in_cksum((u_short *)outicmp,
			    packlen - (sizeof (*outip) + optlen));
			if (outicmp->icmp_cksum == 0)
				outicmp->icmp_cksum = 0xffff;
		} else {
			/* Checksum (must save and restore ip header) */
			tip = *outip;
			ui = (struct udpiphdr *)outip;
			ui->ui_next = 0;
			ui->ui_prev = 0;
			ui->ui_x1 = 0;
			ui->ui_len = outudp->uh_ulen;
			outudp->uh_sum = 0;
			outudp->uh_sum = in_cksum((u_short *)ui, packlen);
			if (outudp->uh_sum == 0)
				outudp->uh_sum = 0xffff;
			*outip = tip;
		}
	}

	/*
	 * LBNL bug fixed: missing '- optlen', causing optlen sent twice
	 */
	cc = sendto(sndsock, (char *)outip,
	    packlen - optlen, 0, &whereto, sizeof (whereto));

	if (cc < 0 || cc != (packlen - optlen))  {
		if (cc < 0)
			Fprintf(stderr, "%s: sendto: %s\n", prog,
				strerror(errno));
		Printf(gettext("%s: wrote %s %d chars, ret=%d\n"),
		    prog, hostname, packlen, cc);
		(void) fflush(stdout);
	}
}

/*
 * return the difference (in msec) between two time values
 */
double
deltaT(struct timeval t1p, struct timeval t2p)
{
	double dt;

	dt = (double)(t2p.tv_sec - t1p.tv_sec) * 1000.0 +
		(double)(t2p.tv_usec - t1p.tv_usec) / 1000.0;
	return (dt);
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(u_char t)
{
	static char *ttab[17];
	static int first = 1;

	if (first) {
		int i;

		first = 0;

		ttab[0] = strdup(gettext("Echo Reply"));
		ttab[1] = strdup(gettext("ICMP 1"));
		ttab[2] = strdup(gettext("ICMP 2"));
		ttab[3] = strdup(gettext("Dest Unreachable"));
		ttab[4] = strdup(gettext("Source Quench"));
		ttab[5] = strdup(gettext("Redirect"));
		ttab[6] = strdup(gettext("ICMP 6"));
		ttab[7] = strdup(gettext("ICMP 7"));
		ttab[8] = strdup(gettext("Echo"));
		ttab[9] = strdup(gettext("ICMP 9"));
		ttab[10] = strdup(gettext("ICMP 10"));
		ttab[11] = strdup(gettext("Time Exceeded"));
		ttab[12] = strdup(gettext("Param Problem"));
		ttab[13] = strdup(gettext("Timestamp"));
		ttab[14] = strdup(gettext("Timestamp Reply"));
		ttab[15] = strdup(gettext("Info Request"));
		ttab[16] = strdup(gettext("Info Reply"));

		for (i = 0; i < 17; i++)
			if (ttab[i] == NULL) {
				Fprintf(stderr, "%s: strdup %s\n", prog,
					strerror(errno));
				exit(EXIT_FAILURE);
			}
	}

	if (t > 16)
		return (gettext("OUT-OF-RANGE"));

	return (ttab[t]);
}

/*
 * verify the received packet.
 * returns:
 *    0 : received packet too short or not a reply to my probe
 *   -1 : reply from intermediate gateway
 *   -2 : reply from destination (using ICMP)
 *   ICMP_code + 1 : otherwise
 */
int
packet_ok(u_char *buf, int cc, struct sockaddr_in *from, int seq)
{
	struct icmp *icp;
	u_char type, code;
	int hlen;
	int save_cc = cc;
	struct ip *ip;

	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			Printf(gettext("packet too short (%d bytes) from %s\n"),
				cc, inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);

	type = icp->icmp_type;
	code = icp->icmp_code;

	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
		struct ip *hip;
		struct udphdr *up;
		struct icmp *hicmp;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		if (useicmp) {
			if (type == ICMP_ECHOREPLY &&
			    icp->icmp_id == htons(ident) &&
			    icp->icmp_seq == htons(seq))
				return (-2);

			hicmp = (struct icmp *)((u_char *)hip + hlen);

			if (hlen + ICMP_MINLEN <= cc &&
			    hip->ip_p == IPPROTO_ICMP &&
			    hicmp->icmp_id == htons(ident) &&
			    hicmp->icmp_seq == htons(seq))
				return (type == ICMP_TIMXCEED ? -1 : code + 1);
		} else {
			up = (struct udphdr *)((u_char *)hip + hlen);
			/*
			 * 12 = ICMP_MINLEN + 4 (at least 4 bytes of UDP
			 * header is required for this check)
			 */
			if (hlen + 12 <= cc &&
			    hip->ip_p == IPPROTO_UDP &&
			    up->uh_sport == htons(ident) &&
			    up->uh_dport == htons(port + seq))
				return (type == ICMP_TIMXCEED ? -1 : code + 1);
		}
	}


	if (verbose) {
		int i, j;
		u_char *lp = (u_char *)ip;

		cc = save_cc;
		Printf(gettext("\n%d bytes from %s to "), cc,
			inet_ntoa(from->sin_addr));
		Printf(gettext("%s: icmp type %d (%s) code %d\n"),
			inet_ntoa(ip->ip_dst), type, pr_type(type),
			icp->icmp_code);
		for (i = 0; i < cc; i += 4) {
			Printf("%2d: x", i);
			for (j = 0; ((j < 4) && ((i + j) < cc)); j ++)
				Printf("%2.2x", *lp++);
			(void) putchar('\n');
		}
	}

	return (0);
}

/*
 * print the info about the reply packet
 */
void
print(u_char *buf, int cc, struct sockaddr_in *from)
{
	struct ip *ip;

	ip = (struct ip *)buf;

	if (nflag)
		Printf(" %s", inet_ntoa(from->sin_addr));
	else
		Printf(" %s (%s)", inetname(from->sin_addr),
		    inet_ntoa(from->sin_addr));

	if (verbose)
		Printf(gettext(" %d bytes to %s"), cc, inet_ntoa(ip->ip_dst));
}

/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(u_short *addr, int len)
{
	int nleft = len;
	u_short *w = addr;
	u_short answer;
	int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += *(u_char *)w;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * Subtract 2 timeval structs:  out = out - in.
 * Out is assumed to be >= in.
 */
void
tvsub(struct timeval *out, struct timeval *in)
{
	if ((out->tv_usec -= in->tv_usec) < 0)   {
		--out->tv_sec;
		out->tv_usec += 1000000;
	}
	out->tv_sec -= in->tv_sec;
}

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(struct in_addr in)
{
	char *cp;
	struct hostent *hp;
	static int first = 1;
	static char domain[MAXHOSTNAMELEN + 1], line[MAXHOSTNAMELEN + 1];

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, MAXHOSTNAMELEN) == 0 &&
		    (cp = strchr(domain, '.')) != NULL) {
			(void) strncpy(domain, cp + 1, sizeof (domain) - 1);
			domain[sizeof (domain) - 1] = '\0';
		} else
			domain[0] = '\0';
	}
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof (in), AF_INET);
		if (hp != NULL) {
			if ((cp = strchr(hp->h_name, '.')) != NULL &&
			    strcoll(cp + 1, domain) == 0)
				*cp = '\0';
			(void) strncpy(line, hp->h_name, sizeof (line) - 1);
			line[sizeof (line) - 1] = '\0';
			return (line);
		}
	}
	return (inet_ntoa(in));
}

/*
 * given IP address or hostname, return a hostinfo structure
 */
struct hostinfo *
gethostinfo(char *hostname)
{
	int n;
	struct hostent *hp;
	struct hostinfo *hi;
	char **p;
	in_addr_t addr, *ap;

	hi = calloc(1, sizeof (struct hostinfo));
	if (hi == NULL) {
		Fprintf(stderr, "%s: calloc %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}
	addr = inet_addr(hostname);
	if ((int32_t)addr != -1) {
		/*
		 * LBNL bug fixed: used to call savestr(), which was buggy
		 * it gives bus error when more than one -g used
		 * savestr.h removed
		 */
		hi->name = strdup(hostname);
		if (hi->name == NULL) {
			Fprintf(stderr, "%s: strdup %s\n", prog,
				strerror(errno));
			exit(EXIT_FAILURE);
		}
		hi->n = 1;
		hi->addrs = calloc(1, sizeof (hi->addrs[0]));
		if (hi->addrs == NULL) {
			Fprintf(stderr, "%s: calloc %s\n", prog,
				strerror(errno));
			exit(EXIT_FAILURE);
		}
		hi->addrs[0] = addr;
		return (hi);
	}

	hp = gethostbyname(hostname);
	if (hp == NULL) {
		Fprintf(stderr, gettext("%s: unknown host %s\n"), prog,
			hostname);
		exit(EXIT_FAILURE);
	}
	if (hp->h_addrtype != AF_INET || hp->h_length != 4) {
		Fprintf(stderr, gettext("%s: bad host %s\n"), prog, hostname);
		exit(EXIT_FAILURE);
	}

	hi->name = strdup(hp->h_name);
	if (hi->name == NULL) {
		Fprintf(stderr, "%s: strdup %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (n = 0, p = hp->h_addr_list; *p != NULL; ++n, ++p)
		continue;
	hi->n = n;

	hi->addrs = calloc(n, sizeof (hi->addrs[0]));
	if (hi->addrs == NULL) {
		Fprintf(stderr, "%s: calloc %s\n", prog, strerror(errno));
		exit(EXIT_FAILURE);
	}

	for (ap = hi->addrs, p = hp->h_addr_list; *p != NULL; ++ap, ++p)
		(void) memcpy(ap, *p, sizeof (*ap));

	return (hi);
}

/*
 * free a hostinfo structure
 */
void
freehostinfo(struct hostinfo *hi)
{
	if (hi->name != NULL) {
		free(hi->name);
		hi->name = NULL;
	}
	free((char *)hi->addrs);
	free((char *)hi);
}

/*
 * determine an IP address for a given IP address or hostname
 */
void
getaddr(in_addr_t *ap, char *hostname)
{
	struct hostinfo *hi;

	hi = gethostinfo(hostname);
	*ap = hi->addrs[0];
	freehostinfo(hi);
}

/*
 * set the IP address in a sockaddr_in struct
 */
void
setsin(struct sockaddr_in *sin, in_addr_t addr)
{

	(void) memset(sin, 0, sizeof (*sin));

	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = addr;
}

/* String to value with optional min and max. Handles decimal and hex. */
int
str2val(const char *str, const char *what, int mi, int ma)
{
	const char *cp;
	int val;
	char *ep;

	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
		cp = str + 2;
		val = (int)strtol(cp, &ep, 16);
	} else
		val = (int)strtol(str, &ep, 10);
	if (*ep != '\0') {
		Fprintf(stderr, gettext("%s: \"%s\" bad value for %s \n"),
		    prog, str, what);
		exit(EXIT_FAILURE);
	}
	if (val < mi && mi >= 0) {
		if (mi == 0)
			Fprintf(stderr, gettext("%s: %s must be >= %d\n"),
			    prog, what, mi);
		else
			Fprintf(stderr, gettext("%s: %s must be > %d\n"),
			    prog, what, mi - 1);
		exit(EXIT_FAILURE);
	}
	if (val > ma && ma >= 0) {
		Fprintf(stderr, gettext("%s: %s must be <= %d\n"), prog, what,
			ma);
		exit(EXIT_FAILURE);
	}
	return (val);
}

/*
 * display the usage of traceroute
 */
void
usage(void)
{
	Fprintf(stderr, gettext("Usage: %s [-dFInvx] [-f first_ttl]"\
" [-g gateway | -r] [-i iface]\n\t [-m max_ttl] [-p port] [-q nqueries]"\
" [-s src_addr] [-t tos]\n\t [-w waittime] host [packetlen]\n"), prog);

	exit(EXIT_FAILURE);
}
