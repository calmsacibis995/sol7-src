/*
 * Copyright (c) 1990, 1991, 1992 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)hostconfig.c	1.8	97/04/22 SMI"

#include <stdio.h>
#include <locale.h>
#include <sys/utsname.h>
#include <sys/systeminfo.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/resource.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/stream.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <rpc/rpc.h>
#include <rpcsvc/bootparam_prot.h>

#ifdef SYSV
#define signal(s,f)	sigset((s), (f))
#undef bzero
#define bzero(s,n)	memset((s), 0, (n))
#undef bcopy
#define bcopy(a,b,c)	memcpy((b),(a),(c))
#
#endif SYSV

extern char	*malloc();

#if 	defined(SYSV) && !defined(SIOCGIFCONF_FIXED)
#define MAXIFS	32	/* results in a bufsize of 1024 */
#else
#define MAXIFS	256
#endif

/* command line flags
 */
int		debug = 0;		/* do debug printfs */
int		verbose = 0;		/* do verbose printfs */
int		safe = 0;		/* don't change anything */
int		multiple = 0;		/* take multiple replies */

static u_long	if_netmask;

int		get_ifdata();		/* get IP addr, subnet mask from IF */
extern char	*inet_ntoa();
extern int	getopt();

int	bp_whoami(), notsupported();


struct prototab {
	char *name;
	int (*func)();
} prototab[] = {
	{ "bootparams", bp_whoami },
	{ "bootp", notsupported },
	{ 0, 0 }
};



/*
 * usage: hostconfig [-p <protocol>] [-v] [-n] [<ifname>] [-f <hostname>]
 *
 * options:
 *	-d		Debug mode.
 * 	-v		Verbose mode.
 *	-n		Don't change anything.
 *	-m		Wait for multiple answers (best used with the "-n"
 *			and "-v" flags).
 *	-f <hostname>	Fake mode - get bootparams for <hostname> (also
 *			best used with the "-n" and "-v" flags).
 *	<ifname>	Use IP address of <interface> in whoami request.
 *
 * If no interface name is specified, bp_whoami will cycle through the
 * interfaces, using the IP address of each in turn until an answer is
 * received.  Note that rpc_broadcast() broadcasts the RPC call on all
 * interfaces, so the <ifname> argument doesn't restrict the request
 * to that interface, it just uses that interface to determine the IP
 * address to put into the request.  If "-f <hostname>" is specified,
 * we put the IP address of <hostname> in the whoami request.  Otherwise,
 * we put the IP address of the interface in the whoami request.
 *
 */


main(argc, argv)
	int argc;
	char **argv;
{
	struct ifreq *reqbuf;
	struct ifreq *ifr;
	struct ifconf ifc;
	struct in_addr targetaddr;
	struct hostent *hp;
	char *targethost = NULL;
	char *cmdname;
	int c;
	int n;
	struct prototab *ptp;
	int (*protofunc)() = NULL;
	int numifs;
	unsigned bufsize;

	extern char *optarg;
	extern int optind;

	cmdname = argv[0];

	while ((c = getopt(argc, argv, "dvnmf:p:")) != -1) {

		switch ((char)c) {
		case 'd':
			debug++;
			break;

		case 'v':
			verbose++;
			break;

		case 'm':
			multiple++;
			break;

		case 'n':
			safe++;
			break;

		case 'f':
			targethost = optarg;
			break;

		case 'p':
			protofunc = NULL;
			for (ptp = &prototab[0]; ptp->func; ptp++)
				if (strcmp(optarg, ptp->name) == 0) {
					protofunc = ptp->func;
					break;
				}
			if (protofunc == NULL)
				usage(cmdname);
			break;

		case '?':
			usage(cmdname);
		}
	}

	if (protofunc == NULL)
		usage(cmdname);

	if (targethost) {
		/* we are faking it
		 */
		if (debug)
			fprintf(stdout, "targethost = %s\n", targethost);

		if ((hp = gethostbyname(targethost)) == NULL) {
			if ((targetaddr.s_addr = inet_addr(targethost)) ==
								(u_long)(-1)) {
				(void) fprintf(stderr,
					"%s: cannot get IP address for %s\n",
					cmdname, targethost);
				exit(1);
			}
		} else {
			if (hp->h_length != sizeof(targetaddr)) {
				(void) fprintf(stderr,
					"%s: cannot find host entry for %s\n",
					cmdname, targethost);
				exit(1);
			} else
				bcopy((char *)hp->h_addr,
					(char *)&targetaddr.s_addr,
					sizeof(targetaddr));
		}
	} else
		targetaddr.s_addr = 0;

	if (optind < argc) {
		/* interface names were specified
		 */
		for (; optind < argc; optind++) {
			if (debug)
				fprintf(stdout, "Trying arg %s\n",
					argv[optind]);
			(*protofunc)(argv[optind], targetaddr);
		}
	} else {
		/* no interface names specified - try them all
		 */
		int ifcount = 0;	/* count of useable interfaces */
		int s;

		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("socket");
			exit(1);
		}
#ifdef SIOCGIFNUM
		if (ioctl(s, SIOCGIFNUM, (char *)&numifs) < 0) {
			numifs = MAXIFS;
		}
#else
		numifs = MAXIFS;
#endif
		bufsize = numifs * sizeof(struct ifreq);
		reqbuf = (struct ifreq *)malloc(bufsize);
		if (reqbuf == NULL) {
			fprintf(stderr, "out of memory\n");
			exit(1);
		}
		ifc.ifc_buf = (caddr_t)&reqbuf[0];
		ifc.ifc_len = bufsize;
		if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
			perror("ioctl(SIOCGIFCONF)");
			exit(1);
		}
		ifr = ifc.ifc_req;
		n = ifc.ifc_len/sizeof(struct ifreq);
		for (; n > 0; n--, ifr++) {
			if (ioctl(s, SIOCGIFFLAGS, (char *)ifr) < 0) {
				perror ("ioctl(SIOCGIFFLAGS)");
				exit(1);
			}
			if ((ifr->ifr_flags & IFF_LOOPBACK) ||
					!(ifr->ifr_flags & IFF_BROADCAST) ||
					!(ifr->ifr_flags & IFF_UP) ||
					(ifr->ifr_flags & IFF_NOARP) ||
					(ifr->ifr_flags & IFF_POINTOPOINT)) {
				if (debug)
					fprintf(stdout, "If %s not suitable\n",
						ifr->ifr_name);
				continue;
			} else {
				if (debug)
					fprintf(stdout, "Trying device %s\n",
						ifr->ifr_name);
				(*protofunc)(ifr->ifr_name,  targetaddr);
				ifcount++;
			}
			
		}
		if (verbose && ifcount == 0) {
			fprintf(stderr, "No useable interfaces found.\n");
			exit(1);
		}
		(void) close(s);
		(void) free((char *)reqbuf);
	}
}


int
add_default_route(router_addr)
	struct in_addr router_addr;
{
	struct rtentry route;
	struct sockaddr_in *sin;
	int s;

	bzero((char *)&route, sizeof(route));

	/* route destination is "default" - zero
	 */
	sin = (struct sockaddr_in *)&route.rt_dst;
	sin->sin_family = AF_INET;

	sin = (struct sockaddr_in *)&route.rt_gateway;
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = router_addr.s_addr;

	route.rt_flags = RTF_GATEWAY | RTF_UP;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return;
	}
	if (ioctl(s, SIOCADDRT, (char *)&route) == -1) {
		perror("add default route");
		return;
	}
	(void) close(s);
}


int
bpanswer(res, nb)
	struct bp_whoami_res *res;
	struct netbuf *nb;
{
	struct in_addr router_addr;
	static int set;
	int len;
	char errbuf[128];

	bcopy((char *)&res->router_address.bp_address_u.ip_addr,
	       (char *)&router_addr, sizeof(router_addr));

	if (verbose) {
		struct sockaddr_in *addr;

		if (nb) {
			addr = (struct sockaddr_in *) nb->buf;
			fprintf(stdout, "From [%s]: ",
			    inet_ntoa(addr->sin_addr));
		} else {
			fprintf(stdout, "Reply:\\t\\t");
		}
		fprintf(stdout, "hostname = %s\n", res->client_name);
		fprintf(stdout, "\t\typdomain = %s\n", res->domain_name);
		fprintf(stdout, "\t\trouter = %s\n", inet_ntoa(router_addr));
	}

	if (!safe && !set) {
		/*
		 * Stuff the values from the RPC reply into the kernel.
		 * Only allow one pass through this code; There's no reason
		 * why all replies should tweak the kernel.
		 */
		set++;

		len = strlen(res->client_name);
		if (len != 0 && len <= MAXHOSTNAMELEN) {
			if (sysinfo(SI_SET_HOSTNAME, res->client_name,
							len) < 0) {
				sprintf(errbuf, "sysinfo(SI_SET_HOSTNAME)(%s)",
					res->client_name);
				perror(errbuf);
			}
		}

		len = strlen(res->domain_name);
		if (len != 0 && len <= MAXHOSTNAMELEN) {
			if (setdomainname(res->domain_name, len) == -1) {
				sprintf(errbuf, "setdomainname(%s)",
					res->domain_name);
				perror(errbuf);
			}
		}

		/* we really should validate this router value
		 */
		if (router_addr.s_addr != 0)
			add_default_route(router_addr);
	}

	if (multiple)
		return(NULL);

	/* our job is done
	 */
	exit(0);
}


bp_whoami(device, addr)
	char *device;
	struct in_addr addr;
{
	struct bp_whoami_arg req;
	struct bp_whoami_res res;
	struct in_addr lookupaddr;
	struct hostent *hp;
	enum clnt_stat stat;
	int val = 1;

	if (debug)
		fprintf(stdout, "bp_whoami on interface %s addr %s\n", device,
		       inet_ntoa(addr));

	if (addr.s_addr ==  0) {
		if (get_ifdata(device, &lookupaddr, &if_netmask) == -1)
			return(-1);
	} else
		bcopy((char *)&addr, (char *)&lookupaddr, sizeof(addr));

	lookupaddr.s_addr = ntohl(lookupaddr.s_addr);

	if (debug)
		fprintf(stdout, "lookup address is %s\n",
			inet_ntoa(lookupaddr));

	bzero((char *)&req, sizeof(req));
	bzero((char *)&res, sizeof(res));

	req.client_address.address_type = IP_ADDR_TYPE;
	bcopy((char *)&lookupaddr,
	       (char *)&req.client_address.bp_address_u.ip_addr,
	       sizeof(lookupaddr));

	/*
	 * Broadcast using portmap version number 2  ONLY to
	 * prevent broadcast storm
	 */

	(void) __rpc_control(CLCR_SET_LOWVERS, (void *)&val);

	stat = rpc_broadcast(BOOTPARAMPROG, BOOTPARAMVERS,
			      BOOTPARAMPROC_WHOAMI, xdr_bp_whoami_arg,
			     (caddr_t) &req, xdr_bp_whoami_res, (caddr_t) &res,
			     (resultproc_t) bpanswer, "udp");

	/* Now try version 3 as well */

	val = 0;
	(void) __rpc_control(CLCR_SET_LOWVERS, (void *)&val);

	stat = rpc_broadcast(BOOTPARAMPROG, BOOTPARAMVERS,
			      BOOTPARAMPROC_WHOAMI, xdr_bp_whoami_arg,
			     (caddr_t) &req, xdr_bp_whoami_res, (caddr_t) &res,
			     (resultproc_t) bpanswer, "udp");
	
	if (stat != RPC_SUCCESS) {
		clnt_perrno(stat);
		exit(1);
	}
}


/* Get IP address of an interface.  As long as we are looking, get the
 * netmask as well.
 */
int
get_ifdata(dev, ipp, maskp)
	char *dev;
	u_long *ipp, *maskp;
{
	struct ifreq ifr;
	struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
	int s;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return(-1);
	}

	(void) strncpy(ifr.ifr_name, dev, sizeof(ifr.ifr_name));

	if (ipp) {
		if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
			perror("ioctl(SIOCGIFADDR)");
			return(-1);
		}
		*ipp = ntohl(sin->sin_addr.s_addr);

		if (debug)
			(void) fprintf(stderr, "Interface '%s' address %s\n",
					dev, inet_ntoa(sin->sin_addr));
	}

	if (maskp) {
		if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
			perror("SIOCGIFNETMASK");
			return(-1);
		}
		*maskp = ntohl(sin->sin_addr.s_addr);

		if (debug)
			(void) fprintf(stderr,
				"Interface '%s' subnet mask %s\n", dev,
				inet_ntoa(sin->sin_addr));
	}

	(void) close(s);
	return(0);
}


notsupported()
{
	fprintf(stderr, "requested protocol is not supported\n");
	exit(1);
}


usage(cmdname)
	char *cmdname;
{
	(void) fprintf(stderr, "usage: %s [-v] [-n] [-m] [<ifname>] [-f <hostname>] -p bootparams|bootp\n", cmdname);
	(void) fflush(stderr);
	exit(1);
}
