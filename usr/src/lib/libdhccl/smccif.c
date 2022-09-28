/*
 * smccif.c: "Interface DHCP to programs requiring its services".
 *
 * SYNOPSIS
 * void setifdhcp(const char *caller, const char *name, int argc, char *argv[])
 *
 * DESCRIPTION
 *    This code interfaces the agent to programs requiring its
 *	services (dhcpinfo, ifconfig, netstat).
 *
 * DIAGNOSTICS
 *    May exit with the following codes:
 *        0 success
 *        2 DHCP was not successful (presumably no servers replied)
 *        3 Bad arguments
 *        4 Timed out (interface not successfully configured before
 *                     interval given with -w)
 *        5 Permission: program must be run as root
 *        6 Some system error (should never occur)
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)smccif.c 1.4 96/12/19 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <net/if.h>
#include <errno.h>
#include <signal.h>
#include "dhcpstate.h"
#include "msgindex.h"
#include "error.h"
#include "client.h"
#include "utils.h"
#include "ifinst.h"
#include "dhcpctrl.h"
#include "unixgen.h"
#include "catype.h"

const char *prog;
static union dhcpipc reqipc;
static int fd = -1;

/* ARGSUSED */
static void
catchTermination(int sig)
{
	printf(DHCPCMSG06, prog);
#if 0 /* prefer to stop configuration of inetrface in this case */
	reqipc.ctrl.flags = CP_CTRL_TIMEOUT;
#else
	reqipc.ctrl.flags = CP_DROP;
#endif
	if (fd >= 0) {
		send(fd, (char *)&reqipc, sizeof (reqipc), 0);
		close(fd);
	}
	exit(ECODE_SIGNAL);
}

/* ARGSUSED */
static void
catchAlarm(int sig)
{
	printf(DHCPCMSG77, prog, reqipc.ctrl.timeout);
	reqipc.ctrl.flags = CP_CTRL_TIMEOUT;
	if (fd >= 0)
		send(fd, (char *)&reqipc, sizeof (reqipc), 0);
	exit(ECODE_TIMEOUT);
}

static void
setupHandlers(int timeout)
{
	struct sigaction s;

	s.sa_flags = 0;
	s.sa_handler = catchTermination;
	sigemptyset(&s.sa_mask);
	sigaddset(&s.sa_mask, SIGINT);
	sigaddset(&s.sa_mask, SIGQUIT);
	sigaddset(&s.sa_mask, SIGTERM);
	sigaddset(&s.sa_mask, SIGALRM);
	sigaction(SIGINT, &s, 0);
	sigaction(SIGQUIT, &s, 0);
	sigaction(SIGTERM, &s, 0);

	if (timeout > 0) {
		s.sa_flags = 0;
		sigemptyset(&s.sa_mask);
		sigaddset(&s.sa_mask, SIGALRM);
		s.sa_handler = catchAlarm;
		sigaction(SIGALRM, &s, 0);
		alarm((unsigned)timeout);
	}
}

static int
chkif(const char *ifname)
{
	struct ifreq ifr;
	int sock;

	if (ifname == NULL) {
		fprintf(stderr, DHCPCMSG73, prog, "<NULL>");
		return (ECODE_BAD_ARGS);
	}

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		fprintf(stderr, DHCPCMSG72, prog, SYSMSG);
		exit(ECODE_SYSERR);
	}
	memset(&ifr, 0, sizeof (ifr));
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
		fprintf(stderr, DHCPCMSG73, prog, ifname);
		close(sock);
		return (ECODE_BAD_ARGS);
	}
	close(sock);
	if (ifr.ifr_flags & (IFF_LOOPBACK | IFF_POINTOPOINT)) {
		fprintf(stderr, DHCPCMSG82, prog, ifname);
		return (ECODE_BAD_ARGS);
	}
	if (!(ifr.ifr_flags & IFF_BROADCAST)) {
		fprintf(stderr, DHCPCMSG82, prog, ifname);
		return (ECODE_BAD_ARGS);
	}
	return (0);
}

static const char *
errorString(int errorCode)
{
	register char	*retp;

	switch (errorCode) {
	case ERR_IF_ADDR_CHANGED:
		retp = "Interface IP address changed";
		break;
	case ERR_TIMED_OUT:
		retp = "DHCP timed out";
		break;
	case ERR_RETRIES:
		retp = "Too many retries";
		break;
	case ERR_MEDIA_UNSUPPORTED:
		retp = "DHCP unsupported on this physical layer";
		break;
	case ERR_NO_CONFIG:
	case ERR_BAD_CONFIG:
	case ERR_WRITE_CONFIG:
		retp = "Bad configuration file information";
		break;
	case ERR_IF_GET_FLAGS:
		retp = "Cannot get interface flags";
		break;
	case ERR_IF_SET_FLAGS:
		retp = "Cannot set interface flags";
		break;
	case ERR_IF_SET_ADDR:
		retp = "Cannot set IP address";
		break;
	case ERR_IF_NOT_UP:
		retp = "Interface not up";
		break;
	case ERR_IF_GET_ADDR:
		retp = "Cannot get IP address";
		break;
	case ERR_SET_BROADCAST:
	case ERR_SOCKET_BROADCAST:
	case ERR_IF_SET_BRDADDR:
		retp = "Cannot set IP broadcast address";
		break;
	case ERR_SET_HOSTNAME:
		retp = "Cannot set hostname";
		break;
	case ERR_SET_NETMASK:
		retp = "Cannot set netmask";
		break;
	case ERR_SET_GATEWAY:
		retp = "Cannot set default router";
		break;
	case ERR_OUT_OF_MEMORY:
		retp = "No more memory";
		break;
	case ERR_NOT_ROOT:
		retp = "User must be superuser (root)";
		break;
	case ERR_DUPLICATE_AGENT:
		retp = "Attempt to start another DHCP client agent";
		break;
	default:
		retp = "unspecified system error";
	};

	return (retp);
}

static void
print_dhcp_status(const Dhcpctrl *reply)
{
	char buf[64];
	struct tm *tm;

	printf("%-10s", reply->ifname);
	printf(" %-8s", ifStateString(reply->ifState));

	printf(" %8d", reply->ifSent);
	printf(" %8d", reply->ifReceived);
	printf(" %8d", reply->ifBadOffers);
	putchar('\n');
	if (reply->ifState == DHCP_BOUND || reply->ifState == DHCP_RENEWING ||
	    reply->ifState == DHCP_REBINDING) {
		tm = localtime(&reply->ifgranted);
		strftime(buf, sizeof (buf), "%m/%d/%Y %R", tm);
		printf("\t (Began,Expires,Renew) = (%s,", buf);
		if (reply->ifT0 < 0)
			printf(" Never, Never)\n");
		else {
			time_t expires = reply->ifgranted + reply->ifT0;
			tm = localtime(&expires);
			strftime(buf, sizeof (buf), "%m/%d/%Y %R", tm);
			printf(" %s,", buf);
			expires = reply->ifgranted + reply->ifT1;
			tm = localtime(&expires);
			strftime(buf, sizeof (buf), "%m/%d/%Y %R", tm);
			printf(" %s)\n", buf);
		}
	}
	if (reply->ifError != 0)
		printf(" %s\n", errorString(reply->ifError));
}

static void
print_reply_status(const union dhcpipc *request, const union dhcpipc *reply)
{
	int ret = 0;

	switch (reply->ctrl.flags) {
	case CP_IF_ALREADY_STARTED:
		fprintf(stderr, DHCPCMSG80, prog, request->ctrl.ifname);
		break;
	case CP_IF_NOT_FOUND:
		if ((request->ctrl.flags & CP_REQUEST_TYPES) != CP_STATUS)
			fprintf(stderr, DHCPCMSG81, prog, request->ctrl.ifname);
		break;
	case CP_NO_RESPONSE:
		if (request->ctrl.ifname[0] != '\0')
			fprintf(stderr, DHCPCMSG90, prog, request->ctrl.ifname);
		break;
	case CP_INTERNAL_ERROR:
		fprintf(stderr, DHCPCMSG83, prog);
		ret = ECODE_SYSERR;
		break;
	case CP_UNIMPLEMENTED:
		fprintf(stderr, DHCPCMSG86, prog);
		break;
	case CP_PERMISSION:
		fprintf(stderr, DHCPCMSG87, prog);
		ret = ECODE_PERMISSION;
		break;
	case CP_NOT_CONFIGURED:
		if (request->ctrl.ifname[0] != '\0' &&
		    ((request->ctrl.flags & CP_REQUEST_TYPES) != CP_STATUS))
			fprintf(stderr, DHCPCMSG90, prog, request->ctrl.ifname);
		break;
	case CP_SUCCESS:
		if ((request->ctrl.flags & CP_REQUEST_TYPES) == CP_STATUS)
			print_dhcp_status(&reply->ctrl);
		break;
	default:
		fprintf(stderr, DHCPCMSG84, prog);
		ret = ECODE_SYSERR;
		break;
	};
	if (ret != 0)
		exit(ret);
}

static void
usage()
{
	const char *usageDescription =
	    "Usage: %s"
	    " [<interface> | -a | -au | -auD | -ad | -adD]"
	    " auto-dhcp"
	    " [primary]"
#if 0
	    " [autostart]"
	    " [noretry]"
	    " [server <server_ip_addr>]"
#endif
	    " [wait <seconds> | forever]"
	    " start|drop|extend|ping|release|status"
#if DHCP_OVER_PPP
	    "|inform"
#endif /* DHCP_OVER_PPP */
	    "\n"
	    "\t <interface> interface name to configure e.g. le0\n"
#if 0
	    "\t \"autostart\" to automatically start the agent\n"
	    "\t \"server\" to configure from a specific server\n"
	    "\t \"noretry\" tells agent to do only one complete backoff"
	    " & retry cycle\n"
	    "\t\tDefault is to keep cycling through backoff-retry sequence\n"
#endif
	    "\t \"primary\" to designate interface as the primary\n"
	    "\t \"wait\" to wait <seconds> for dhcp response\n"
	    "\t \"start\" to start DHCP configuration on an interface\n"
	    "\t \"drop\" to terminate DHCP on an interface but keep any lease\n"
	    "\t \"extend\" to extend the lease on an interface IP address\n"
	    "\t \"ping\" to test whether the DHCP agent is running\n"
	    "\t \"release\" to release the DHCP lease and take interface down\n"
	    "\t \"status\" to get DHCP status of an interface\n"
#if DHCP_OVER_PPP
	    "\t \"inform\" to use inform to get configuration over\n"
#endif /* DHCP_OVER_PPP */
	    "\n"
	;

	fprintf(stderr, usageDescription, prog);
	exit(ECODE_BAD_ARGS);
}

static int
requestType(const char *requestString)
{
	static struct {
		char *str;
		int tag;
	} request_types[] = {
#ifdef	DHCP_OVER_PPP
		{ "inform",	CP_INFORM},
#endif	/* DHCP_OVER_PPP */
		{ "drop",	CP_DROP},
		{ "extend",	CP_RENEW},
		{ "ping",	CP_PING},
		{ "release",	CP_RELEASE},
		{ "start",	CP_START},
		{ "status",	CP_STATUS}
	};
	int i;

	for (i = (sizeof (request_types) / sizeof (request_types[0])) - 1;
	    i >= 0; i--) {
		if (strcmp(request_types[i].str, requestString) == 0)
			return (request_types[i].tag);
	}
	fprintf(stderr, DHCPCMSG74, prog, requestString);
	usage();
	return (CP_NOOP);
}

static void
statusAllIfs()
{
	char *buf;
	struct ifreq *ifrp;
	struct ifconf ifc;
	int numifs;
	int sockfd;
	int bufsiz;
	int n;
	union dhcpipc reply;

	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, DHCPCMSG72, prog, SYSMSG);
		return;
	}
	if (ioctl(sockfd, SIOCGIFNUM, (char *)&numifs) < 0)
		return;
	bufsiz = numifs * sizeof (struct ifreq);
	buf = (char *)xmalloc(bufsiz);

	ifc.ifc_len = bufsiz;
	ifc.ifc_buf = buf;
	if (ioctl(sockfd, SIOCGIFCONF, (char *)&ifc) < 0) {
		fprintf(stderr, prog, DHCPCMSG91, SYSMSG);
		close(sockfd);
		free(buf);
		return;
	}

	ifrp = ifc.ifc_req;
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifrp++) {
		if (ioctl(sockfd, SIOCGIFFLAGS, ifrp) < 0) {
			fprintf(stderr, DHCPCMSG10, ifrp->ifr_name, SYSMSG);
		} else if (ifrp->ifr_flags & IFF_DHCPRUNNING) {
			strncpy(reqipc.ctrl.ifname, ifrp->ifr_name, IFNAMSIZ);
			reqipc.ctrl.ifname[IFNAMSIZ - 1] = '\0';
			fd = getIPCfd(prog, FALSE);
			if (fd < 0) {
				if (ipc_errno == ERR_NOT_ROOT)
					exit(ECODE_PERMISSION);
				else
					exit(ECODE_SYSERR);
			}
			ipc_endpoint(fd);
			doipc(fd, prog, &reqipc, &reply);
			print_reply_status(&reqipc, &reply);
			close(fd);
			fd = -1;
		}
	}
	close(sockfd);
	free(buf);
}

void
setifdhcp(const char *caller, const char *ifname, int argc, char *argv[])
{
	uid_t uid;
	int autoStart = FALSE;
	int type = CP_NOOP;
	int rc;
	union dhcpipc reply;
	ca_boolean_t timeout_specified = FALSE;

	prog = caller;
	memset(&reqipc, 0, sizeof (reqipc));

	for (argv++; --argc > 0; argv++) {
		if (strcmp(argv[0], "noretry") == 0) {
			/* unpublished option */
			reqipc.ctrl.flags &= ~CP_RETRY_MODES;
			reqipc.ctrl.flags |= CP_FAIL_IF_TIMEOUT;
		} else if (strcmp(argv[0], "autostart") == 0) {
			/* unpublished option */
			autoStart = TRUE;
		} else if (strcmp(argv[0], "primary") == 0) {
			reqipc.ctrl.flags |= CP_PRIMARY;
		} else if (strcmp(argv[0], "server") == 0) {
			/* unpublished option */
			if (--argc <= 0)
				usage();
			argv++;
			if (!inet_aton(argv[0], &reqipc.ctrl.server)) {
				fprintf(stderr, DHCPCMSG75, prog, argv[0]);
				exit(ECODE_BAD_ARGS);
			}
			reqipc.ctrl.flags |= CP_SERVER_SPECIFIED;
		} else if (strcmp(argv[0], "wait") == 0) {
			if (--argc <= 0)
				usage();
			timeout_specified = TRUE;
			argv++;
			if (strcmp(argv[0], "forever") == 0) {
				reqipc.ctrl.timeout = -1;
			} else if (sscanf(argv[0], "%d",
			    &reqipc.ctrl.timeout) != 1) {
				usage();
			}
		} else if ((type = requestType(argv[0])) == CP_NOOP) {
			usage();
		}
	}

#if 0  /* after discussion with carl 10/01/96 */
	if ((reqipc.ctrl.flags & CP_PRIMARY) == 0)
		reqipc.ctrl.flags |= CP_FAIL_IF_TIMEOUT;
#endif

	if (type != CP_PING && type != CP_STATUS) {
		uid = getuid();
		if (uid != 0) {
			fprintf(stderr, DHCPCMSG87, prog);
			exit(ECODE_PERMISSION);
		}
	}

	if (type == CP_NOOP)
		type = CP_START;

	if (type == CP_START)
		autoStart = TRUE;

	if ((type == CP_START || type == CP_RENEW) && !timeout_specified)
		reqipc.ctrl.timeout = CP_DEFAULT_TIMEOUT;

	if (type == CP_START || type == CP_RENEW || type == CP_DROP ||
	    type == CP_RELEASE) {
		if (chkif(ifname))
			return;
	} else
		autoStart = FALSE;

	rc = isDaemonUp(prog);
	if (rc >= ERR_FIRST && rc <= ERR_LAST) {
		fprintf(stderr, "Error: (%d): %s\n", rc, errorString(rc));
		exit(ECODE_SYSERR);
	}
	if (rc == 0) {
		if (autoStart)
			startDaemon();
		else {
			if (type != CP_STATUS)
				fprintf(stderr, DHCPCMSG85, prog);
			exit(ECODE_NO_DHCP);
		}
	}

	seed();
	setupHandlers(reqipc.ctrl.timeout);

	if (ifname != NULL) {
		strncpy(reqipc.ctrl.ifname, ifname, IFNAMSIZ);
		reqipc.ctrl.ifname[IFNAMSIZ - 1] = '\0';
	}
	reqipc.ctrl.flags &= ~CP_REQUEST_TYPES;
	reqipc.ctrl.flags |= type;
	if (type == CP_STATUS) {
		printf("%-10s %-8s %8s %8s %8s\n", "Interface", "Status",
		    "Sent", "Received", "Rejects");
	}
	if (type == CP_STATUS && ifname == NULL)
		statusAllIfs();
	else {
		fd = getIPCfd(prog, CP_REQUIRES_ROOT_PRIV(type));
		if (fd < 0) {
			if (ipc_errno == ERR_NOT_ROOT)
				exit(ECODE_PERMISSION);
			else
				exit(ECODE_SYSERR);
		}
		ipc_endpoint(fd);
		doipc(fd, prog, &reqipc, &reply);
		close(fd);
		fd = -1;
		print_reply_status(&reqipc, &reply);
	}
}
