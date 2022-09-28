/*
 * Copyright (c) 1990-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)revarp.c	1.12	97/04/24 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/stropts.h>
#include <sys/resource.h>
#include <sys/sockio.h>
#include <sys/dlpi.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <arpa/inet.h>

static ether_addr_t my_etheraddr;
static ether_addr_t etherbroadcast = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#define	ETHERADDRL	sizeof (ether_addr_t)
#define	IPADDRL		sizeof (struct in_addr)
#define	BUFSIZE		10000
#define	MAXPATHL	128
#define	MAXIFS		16
#define	DEVDIR		"/dev"
#define	RARPRETRIES	5
#define	RARPTIMEOUT	5
#define	DLPI_TIMEOUT	60

struct	etherdladdr {
	ether_addr_t	dl_phys;
	u_short		dl_sap;
};


static int rarp_timeout = RARPTIMEOUT;

/* Timeout for DLPI acks */
static int dlpi_timeout = DLPI_TIMEOUT;

/* global flags */
extern int debug;

/* globals from ifconfig.c */
extern int setaddr;
extern char name[];
extern struct sockaddr_in sin;

static int	rarp_write(int, struct ether_arp *, ether_addr_t);
static int	timed_getmsg(int, struct strbuf *, int *, int, char *, char *);
static int	rarp_open(char *, u_long, ether_addr_t);

/*
 * Get ppa and device from BSD style interface name assuming it is a
 * DLPI type 2 interface.
 */
static int
ifnametotype2device(ifname, path)
	char	*ifname;
	char 	*path;
{
	int	i;
	ulong	p = 0;
	int	m = 1;

	/* device name length has already been checked */
	(void) sprintf(path, "%s/%s", DEVDIR, ifname);
	i = strlen(path) - 1;
	while (i >= 0 && '0' <= path[i] && path[i] <= '9') {
		p += (path[i] - '0')*m;
		m *= 10;
		i--;
	}
	path[i + 1] = '\0';
	return (p);
}


/*
 * Get ppa and device from BSD style interface name assuming it is a
 * DLPI type 1 interface. Always returns -1 for the ppa signalling that no
 * attach is needed.
 */
static int
ifnametotype1device(ifname, path)
	char	*ifname;
	char 	*path;
{
	int	i;
	ulong	p = 0;
	int	m = 1;

	/* device name length has already been checked */
	(void) sprintf(path, "%s/%s", DEVDIR, ifname);
	i = strlen(path) - 1;
	while (i >= 0 && '0' <= path[i] && path[i] <= '9') {
		p += (path[i] - '0')*m;
		m *= 10;
		i--;
	}
	return (p);
}


/*
 * Get ppa, device, and style of device from BSD style interface name.
 * Return the device name in path and the ppa in *ppap
 * Returns 2 for a style 2 device name.
 * Returns 1 for a style 1 device name (signalling that no attach is needed.)
 * Returns -1 on errors.
 */
int
ifname2device_ppa(path, ppap)
	char 	*path;
	int	*ppap;
{
	struct stat st;
	int ppa;

	ppa = ifnametotype2device(name, path);
	if (stat(path, &st) >= 0) {
		*ppap = ppa;
		return (2);
	}
	if (errno != ENOENT)
		return (-1);

	ppa = ifnametotype1device(name, path);
	if (stat(path, &st) >= 0) {
		*ppap = ppa;
		return (1);
	}
	return (-1);
}


/* ARGSUSED */
int
setifrevarp(arg, param)
	char *arg;
	int param;
{
	static int		if_fd;
	struct pollfd		pfd;
	int			s, flags, ret;
	char			*ctlbuf, *databuf, *cause;
	struct strbuf		ctl, data;
	struct ether_arp	req;
	struct ether_arp	ans;
	struct in_addr		from;
	struct in_addr		answer;
	union DL_primitives	*dlp;
	struct ifreq		ifr;
	int rarp_retries;	/* Number of times rarp is sent. */

	if (name[0] == '\0') {
		fprintf(stderr, "setifrevarp: name not set\n");
		exit(1);
	}

	if (debug)
		fprintf(stdout, "setifrevarp interface %s\n", name);

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		Perror("socket");
	}
	strncpy(ifr.ifr_name, name, sizeof (ifr.ifr_name));
	if (ioctl(s, SIOCGIFFLAGS, (char *) &ifr) < 0)
		Perror("SIOCGIFFLAGS");

	/* don't try to revarp if we know it won't work */
	if ((ifr.ifr_flags & IFF_LOOPBACK) || (ifr.ifr_flags & IFF_NOARP) ||
					(ifr.ifr_flags & IFF_POINTOPOINT))
		return (1);

	/* open rarp interface */
	if_fd = rarp_open(name, ETHERTYPE_REVARP, my_etheraddr);
	if (if_fd < 0)
		return (1);

	/* create rarp request */
	bzero((char *) &req, sizeof (req));
	req.arp_hrd = htons(ARPHRD_ETHER);
	req.arp_pro = htons(ETHERTYPE_IP);
	req.arp_hln = ETHERADDRL;
	req.arp_pln = IPADDRL;
	req.arp_op = htons(REVARP_REQUEST);
	bcopy((char *) my_etheraddr, (char *)&req.arp_sha, ETHERADDRL);
	bcopy((char *) my_etheraddr, (char *)&req.arp_tha, ETHERADDRL);

	rarp_retries = RARPRETRIES;
rarp_retry:
	/* send the request */
	if (rarp_write(if_fd, &req, etherbroadcast) < 0)
		return (1);

	if (debug)
		fprintf(stdout, "rarp sent\n");


	/* read the answers */
	if ((databuf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (1);
	}
	if ((ctlbuf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (1);
	}
	for (;;) {
		ctl.len = 0;
		ctl.maxlen = BUFSIZE;
		ctl.buf = ctlbuf;
		data.len = 0;
		data.maxlen = BUFSIZ;
		data.buf = databuf;
		flags = 0;

		/* start RARP reply timeout */
		pfd.fd = if_fd;
		pfd.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
		if ((ret = poll(&pfd, 1, rarp_timeout * 1000)) == 0) {
			if (--rarp_retries > 0) {
				if (debug)
					printf("rarp retry\n");
				goto rarp_retry;
			} else {
				if (debug)
					printf("rarp timeout\n");
				return (1);
			}
		} else if (ret == -1) {
			perror("ifconfig:  RARP reply poll");
			return (1);
		}

		/* poll returned > 0 for this fd so getmsg should not block */
		if ((ret = getmsg(if_fd, &ctl, &data, &flags)) < 0) {
			perror("ifconfig: RARP reply getmsg");
			return (1);
		}

		if (debug)
		    fprintf(stdout,
			"rarp: ret[%d] ctl.len[%d] data.len[%d] flags[%d]\n",
				ret, ctl.len, data.len, flags);

		/* Validate DL_UNITDATA_IND.  */
		dlp = (union DL_primitives *) ctlbuf;
		if (debug) {
			fprintf(stdout, "rarp: dl_primitive[%d]\n",
				dlp->dl_primitive);
			if (dlp->dl_primitive == DL_ERROR_ACK)
			    fprintf(stdout,
				"rarp: err ak: dl_errno %d errno %d\n",
				dlp->error_ack.dl_errno,
				dlp->error_ack.dl_unix_errno);

			if (dlp->dl_primitive == DL_UDERROR_IND)
			    fprintf(stdout,
				"rarp: ud err: err[%d] len[%d] off[%d]\n",
				dlp->uderror_ind.dl_errno,
				dlp->uderror_ind.dl_dest_addr_length,
				dlp->uderror_ind.dl_dest_addr_offset);
		}
		bcopy(databuf, (char *) &ans, sizeof (struct ether_arp));
		cause = NULL;
		if (ret & MORECTL)
			cause = "MORECTL flag";
		else if (ret & MOREDATA)
			cause = "MOREDATA flag";
		else if (ctl.len == 0)
			cause = "missing control part of message";
		else if (ctl.len < 0)
			cause = "short control part of message";
		else if (dlp->dl_primitive != DL_UNITDATA_IND)
			cause = "not unitdata_ind";
		else if (ctl.len < DL_UNITDATA_IND_SIZE)
			cause = "short unitdata_ind";

		else if (data.len < sizeof (struct ether_arp))
			cause = "short ether_arp";
		else if (ans.arp_hrd != htons(ARPHRD_ETHER))
			cause = "hrd";
		else if (ans.arp_pro != htons(ETHERTYPE_IP))
			cause = "pro";
		else if (ans.arp_hln != ETHERADDRL)
			cause = "hln";
		else if (ans.arp_pln != IPADDRL)
			cause = "pln";
		if (cause) {
			(void) fprintf(stderr,
				"sanity check failed; cause: %s\n", cause);
			continue;
		}

		switch (ntohs(ans.arp_op)) {
		case ARPOP_REQUEST:
			if (debug)
				fprintf(stdout, "Got an arp request\n");
			break;

		case ARPOP_REPLY:
			if (debug)
				fprintf(stdout, "Got an arp reply.\n");
			break;

		case REVARP_REQUEST:
			if (debug)
				fprintf(stdout, "Got an rarp request.\n");
			break;

		case REVARP_REPLY:
			bcopy(ans.arp_tpa, &answer, sizeof (answer));
			bcopy(ans.arp_spa, &from, sizeof (from));
			if (debug) {
				fprintf(stdout, "answer: %s",
					inet_ntoa(answer));
				fprintf(stdout, " [from %s]\n",
					inet_ntoa(from));
			}
			sin.sin_addr.s_addr = answer.s_addr;
			setaddr++;
			return (0);

		default:
			(void) fprintf(stderr, "unknown opcode 0x%xd\n",
				ans.arp_op);
			break;
		}
	}
}

int
dlpi_open_attach()
{
	int			fd;
	char			path[MAXPATHL];
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			style;
	int			ppa, flags;

	style = ifname2device_ppa(path, &ppa);
	if (style < 0) {
		/* Not found */
		errno = ENXIO;
		return (-1);
	}

	if (debug)
		fprintf(stdout, "device %s, ppa %d\n", path, ppa);

	/* Open the datalink provider.  */
	if ((fd = open(path, O_RDWR)) < 0) {
		return (-1);
	}

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		(void) close(fd);
		return (-1);
	}

	if (style == 2) {
		/* Issue DL_ATTACH_REQ */
		dlp = (union DL_primitives *) buf;
		dlp->attach_req.dl_primitive = DL_ATTACH_REQ;
		dlp->attach_req.dl_ppa = ppa;
		ctl.buf = (char *) dlp;
		ctl.len = DL_ATTACH_REQ_SIZE;
		if (putmsg(fd, &ctl, NULL, 0) < 0) {
			perror("ifconfig: putmsg");
			(void) close(fd);
			free(buf);
			return (-1);
		}

		/* read reply */
		ctl.buf = (char *) dlp;
		ctl.len = 0;
		ctl.maxlen = BUFSIZE;
		flags = 0;

		/* start timeout for DL_OK_ACK reply */
		if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
		    "DL_OK_ACK", "DL_ATTACH_REQ") == 0) {
			(void) close(fd);
			free(buf);
			return (-1);
		}

		if (debug) {
			fprintf(stdout,
				"ok_ack: ctl.len[%d] flags[%d]\n",
				ctl.len, flags);
		}

		/* Validate DL_OK_ACK reply.  */
		if (ctl.len < sizeof (ulong)) {
			fprintf(stderr,
			    "attach failed:  short reply to attach request\n");
			free(buf);
			return (-1);
		}

		if (dlp->dl_primitive == DL_ERROR_ACK) {
			if (debug)
				fprintf(stderr,
				    "attach failed:  dl_errno %d errno %d\n",
				    dlp->error_ack.dl_errno,
				    dlp->error_ack.dl_unix_errno);
			(void) close(fd);
			free(buf);
			errno = ENXIO;
			return (-1);
		}
		if (dlp->dl_primitive != DL_OK_ACK) {
			fprintf(stderr,
"attach failed:  unrecognizable dl_primitive %d received",
			    dlp->dl_primitive);
			(void) close(fd);
			free(buf);
			return (-1);
		}
		if (ctl.len < DL_OK_ACK_SIZE) {
			fprintf(stderr,
"attach failed:  short attach acknowledgement received\n");
			(void) close(fd);
			free(buf);
			return (-1);
		}
		if (dlp->ok_ack.dl_correct_primitive != DL_ATTACH_REQ) {
			fprintf(stderr,
"attach failed:  returned prim %d != requested prim %d\n",
			    dlp->ok_ack.dl_correct_primitive,
			    DL_ATTACH_REQ);
			(void) close(fd);
			free(buf);
			return (-1);
		}

		if (debug)
			fprintf(stdout, "attach done\n");
	}
	free(buf);
	return (fd);
}

static int
dlpi_bind(fd, sap, eaddr)
	int	fd;
	u_long	sap;
	u_char	*eaddr;
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (-1);
	}
	/* Issue DL_BIND_REQ */
	dlp = (union DL_primitives *) buf;
	dlp->bind_req.dl_primitive = DL_BIND_REQ;
	dlp->bind_req.dl_sap = sap;
	dlp->bind_req.dl_max_conind = 0;
	dlp->bind_req.dl_service_mode = DL_CLDLS;
	dlp->bind_req.dl_conn_mgmt = 0;
	dlp->bind_req.dl_xidtest_flg = 0;
	ctl.buf = (char *) dlp;
	ctl.len = DL_BIND_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *) dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;
	/* start timeout for DL_BIND_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout, "DL_BIND_ACK",
	    "DL_BIND_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		fprintf(stdout,
		    "bind_ack: ctl.len[%d] flags[%d]\n", ctl.len, flags);
	}

	/* Validate DL_BIND_ACK reply.  */
	if (ctl.len < sizeof (ulong)) {
		fprintf(stderr, "bind failed:  short reply to bind request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		fprintf(stderr, "bind failed:  dl_errno %d errno %d\n",
		    dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_BIND_ACK) {
		fprintf(stderr,
		    "bind failed:  unrecognizable dl_primitive %d received\n",
			dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_BIND_ACK_SIZE) {
		fprintf(stderr,
		    "bind failed:  short bind acknowledgement received\n");
		free(buf);
		return (-1);
	}
	if (dlp->bind_ack.dl_sap != sap) {
		fprintf(stderr,
		    "bind failed:  returned dl_sap %d != requested sap %d\n",
		    dlp->bind_ack.dl_sap, sap);
		free(buf);
		return (-1);
	}
	/* copy Ethernet address */
	bcopy((char *) (buf + dlp->bind_ack.dl_addr_offset), (char *) eaddr,
		ETHERADDRL);

	free(buf);
	return (0);
}

static int
dlpi_get_phys(fd, eaddr)
	int	fd;
	u_char	*eaddr;
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (-1);
	}
	/* Issue DL_PHYS_ADDR_REQ */
	dlp = (union DL_primitives *) buf;
	dlp->physaddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	dlp->physaddr_req.dl_addr_type = DL_CURR_PHYS_ADDR;
	ctl.buf = (char *) dlp;
	ctl.len = DL_PHYS_ADDR_REQ_SIZE;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *) dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;

	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_PHYS_ADDR_ACK", "DL_PHYS_ADDR_REQ (DL_CURR_PHYS_ADDR)") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		fprintf(stdout,
		    "phys_addr_ack: ctl.len[%d] flags[%d]\n", ctl.len, flags);
	}

	/* Validate DL_PHYS_ADDR_ACK reply.  */
	if (ctl.len < sizeof (ulong)) {
		fprintf(stderr,
		    "phys_addr failed:  short reply to phys_addr request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		/*
		 * Do not print errors for DL_UNSUPPORTED and DL_NOTSUPPORTED
		 */
		if (dlp->error_ack.dl_errno != DL_UNSUPPORTED &&
		    dlp->error_ack.dl_errno != DL_NOTSUPPORTED) {
			fprintf(stderr,
"phys_addr failed:  dl_errno %d errno %d\n",
			    dlp->error_ack.dl_errno,
			    dlp->error_ack.dl_unix_errno);
		}
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_PHYS_ADDR_ACK) {
		fprintf(stderr,
"phys_addr failed:  unrecognizable dl_primitive %d received\n",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_PHYS_ADDR_ACK_SIZE) {
		fprintf(stderr,
"phys_addr failed:  short phys_addr acknowledgement received\n");
		free(buf);
		return (-1);
	}
	/* Check length of address. */
	if (dlp->physaddr_ack.dl_addr_length != ETHERADDRL)
		return (-1);

	/* copy Ethernet address */
	bcopy((char *) (buf + dlp->physaddr_ack.dl_addr_offset), (char *) eaddr,
		ETHERADDRL);

	free(buf);
	return (0);
}

static int
dlpi_set_phys(fd, eaddr)
	int	fd;
	u_char	*eaddr;
{
	union DL_primitives	*dlp;
	char			*buf;
	struct strbuf		ctl;
	int			flags;

	/* Allocate required buffers */
	if ((buf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (-1);
	}
	/* Issue DL_SET_PHYS_ADDR_REQ */
	dlp = (union DL_primitives *) buf;
	dlp->set_physaddr_req.dl_primitive = DL_SET_PHYS_ADDR_REQ;
	dlp->set_physaddr_req.dl_addr_length = ETHERADDRL;
	dlp->set_physaddr_req.dl_addr_offset = DL_SET_PHYS_ADDR_REQ_SIZE;

	/* copy Ethernet address */
	bcopy((caddr_t) eaddr,
		(caddr_t) (buf + dlp->physaddr_ack.dl_addr_offset),
		ETHERADDRL);
	ctl.buf = (char *) dlp;
	ctl.len = DL_SET_PHYS_ADDR_REQ_SIZE + ETHERADDRL;
	if (putmsg(fd, &ctl, NULL, 0) < 0) {
		perror("ifconfig: putmsg");
		free(buf);
		return (-1);
	}

	/* read reply */
	ctl.buf = (char *) dlp;
	ctl.len = 0;
	ctl.maxlen = BUFSIZE;
	flags = 0;

	/* start timeout for DL_SET_PHYS_ADDR_ACK reply */
	if (timed_getmsg(fd, &ctl, &flags, dlpi_timeout,
	    "DL_SET_PHYS_ADDR_ACK", "DL_SET_PHYS_ADDR_REQ") == 0) {
		free(buf);
		return (-1);
	}

	if (debug) {
		fprintf(stdout,
		    "set_phys_addr_ack: ctl.len[%d] flags[%d]\n",
			ctl.len, flags);
	}

	/* Validate DL_OK_ACK reply.  */
	if (ctl.len < sizeof (ulong)) {
		fprintf(stderr,
"set_phys_addr failed:  short reply to set_phys_addr request\n");
		free(buf);
		return (-1);
	}

	if (dlp->dl_primitive == DL_ERROR_ACK) {
		fprintf(stderr, "set_phys_addr failed:  dl_errno %d errno %d\n",
		    dlp->error_ack.dl_errno, dlp->error_ack.dl_unix_errno);
		free(buf);
		return (-1);
	}
	if (dlp->dl_primitive != DL_OK_ACK) {
		fprintf(stderr,
"set_phys_addr failed:  unrecognizable dl_primitive %d received\n",
		    dlp->dl_primitive);
		free(buf);
		return (-1);
	}
	if (ctl.len < DL_OK_ACK_SIZE) {
		fprintf(stderr,
"set_phys_addr failed:  short ok acknowledgement received\n");
		free(buf);
		return (-1);
	}

	free(buf);
	return (0);
}


/*
 * Open the datalink provider device and bind to the REVARP type.
 * Return the resulting descriptor.
 */
static int
rarp_open(ifname, type, e)
	char			*ifname;
	u_long			type;
	ether_addr_t		e;
{
	int			fd;
	char			*ether_ntoa();

	fd = dlpi_open_attach();
	if (fd < 0) {
		fprintf(stderr, "ifconfig: could not open device for %s\n",
			ifname);
		return (-1);
	}

	if (dlpi_bind(fd, type, (u_char *) e) < 0) {
		(void) close(fd);
		return (-1);
	}

	if (debug)
		fprintf(stdout, "device %s ethernetaddress %s\n", ifname,
			ether_ntoa(e));

	return (fd);
}

static int
rarp_write(fd, r, dhost)
	int			fd;
	struct ether_arp	*r;
	ether_addr_t		dhost;
{
	struct strbuf		ctl, data;
	union DL_primitives	*dlp;
	struct etherdladdr	*dlap;
	char			*ctlbuf;
	char			*databuf;
	int			ret;

	/* Construct DL_UNITDATA_REQ.  */
	if ((ctlbuf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (-1);
	}
	dlp = (union DL_primitives *) ctlbuf;
	dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp->unitdata_req.dl_dest_addr_length = ETHERADDRL + sizeof (u_short);
	dlp->unitdata_req.dl_dest_addr_offset = DL_UNITDATA_REQ_SIZE;
	dlp->unitdata_req.dl_priority.dl_min = 0;
	dlp->unitdata_req.dl_priority.dl_max = 0;

	/*
	 * XXX FIXME
	 * Assume a specific dlpi address format.
	 */
	dlap = (struct etherdladdr *) (ctlbuf + DL_UNITDATA_REQ_SIZE);
	bcopy((char *) dhost, (char *) dlap->dl_phys, ETHERADDRL);
	dlap->dl_sap = (u_short)(ETHERTYPE_REVARP);

	/* Send DL_UNITDATA_REQ.  */
	if ((databuf = malloc(BUFSIZE)) == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return (-1);
	}
	ctl.len = DL_UNITDATA_REQ_SIZE + ETHERADDRL + sizeof (u_short);
	ctl.buf = (char *) dlp;
	ctl.maxlen = BUFSIZE;
	bcopy((char *) r, databuf, sizeof (struct ether_arp));
	data.len = sizeof (struct ether_arp);
	data.buf = databuf;
	data.maxlen = BUFSIZE;
	ret = putmsg(fd, &ctl, &data, 0);
	free(ctlbuf);
	free(databuf);
	return (ret);
}

#ifdef SYSV
dlpi_set_address(ifname, ea)
	char *ifname;
	ether_addr_t	*ea;
{
	int 	fd;

	fd = dlpi_open_attach();
	if (fd < 0) {
		fprintf(stderr, "ifconfig: could not open device for %s\n",
			ifname);
		return (-1);
	}
	if (dlpi_set_phys(fd, (u_char *) ea) < 0) {
		(void) close(fd);
		return (-1);
	}
	(void) close(fd);

	return (0);
}

int
dlpi_get_address(ifname, ea)
	char *ifname;
	ether_addr_t	*ea;
{
	int 	fd;

	fd = dlpi_open_attach();
	if (fd < 0) {
		/* Do not report an error */
		return (-1);
	}

	if (dlpi_get_phys(fd, (u_char *) ea) < 0) {
		(void) close(fd);
		return (-1);
	}
	(void) close(fd);
	return (0);
}

static int
timed_getmsg(int fd, struct strbuf *ctlp, int *flagsp, int timeout, char *kind,
    char *request)
{
	char		perrorbuf[BUFSIZ];
	struct pollfd	pfd;
	int		ret;

	pfd.fd = fd;

	pfd.events = POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI;
	if ((ret = poll(&pfd, 1, timeout * 1000)) == 0) {
		fprintf(stderr, "ifconfig: %s timed out\n", kind);
		return (0);
	} else if (ret == -1) {
		sprintf(perrorbuf, "ifconfig: poll for %s from %s", kind,
		    request);
		perror(perrorbuf);
		return (0);
	}

	/* poll returned > 0 for this fd so getmsg should not block */
	if ((ret = getmsg(fd, ctlp, NULL, flagsp)) < 0) {
		sprintf(perrorbuf, "ifconfig: getmsg expecting %s for %s",
		    kind, request);
		perror(perrorbuf);
		return (0);
	}

	return (1);
}

#endif /* SYSV */
