/*
 * evntwait.c: "Top level event loop in agent".
 *
 * SYNOPSIS
 *    void waitForEvent()
 *
 * DESCRIPTION
 *    The DHCP client is event driven. Actions are initiated from three
 *    sources:
 *        1. User commands arriving through a UNIX domain socket.
 *        2. Timeouts
 *        3. Arriving BOOTP packets which generate SIGPOLLs
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)evntwait.c 1.7 97/07/16 SMI"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include "catype.h"
#include "dhcpstate.h"
#include "utils.h"
#include "client.h"
#include "dccommon.h"
#include "hostdefs.h"
#include "hosttype.h"
#include "dhcpctrl.h"
#include "msgindex.h"
#include "ca_time.h"
#include "unixgen.h"

#if DEBUG
extern int debug;
#endif

struct in_addr server_addr;

/*
 * findif
 * Find the specific interface. If no interface is specified
 * find the so-called primary interface.
 */
static IFINSTANCE *
findif(const char *ifname)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (ifname[0] != '\0' && strcmp(ifname, anIf->ifName) == 0) {
			if (anIf->ifState == DHCP_BOUND)
				return (anIf);
#if EMPLOY_BOOTP
			if (anIf->ifState == BOOTP)
				return (anIf);
#endif
			break;
		} else if (ifname[0] == '\0') {
			if (anIf->ifPrimary) {
				if (anIf->ifState == DHCP_BOUND)
					return (anIf);
#if EMPLOY_BOOTP
				else if (anIf->ifState == BOOTP)
					return (anIf);
#endif
				else
					return (0);
			}
		}
	}
	return (0);
}

static void
setPrimaryInterface(const char *ifname)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (ifname != NULL && strcmp(anIf->ifName, ifname) == 0)
			anIf->ifPrimary = 1;
		else
			anIf->ifPrimary = 0;
	}
}

void
ctrl_reply(int fd, const union dhcpipc *reply)
{
	int rc;
	rc = send(fd, (char *)reply, sizeof (*reply), 0);
	if (rc < 0)
		loge(DHCPCMSG33, SYSMSG);
}

static void
ctrl_tag(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;
	int msg_id;
	union dhcpipc reply;

	anIf = findif(request->ctrl.ifname);
	if (anIf == NULL) {
		msg_id = CP_NOT_CONFIGURED;
		memset(reply.parm.ifname, 0, sizeof (reply.parm.ifname));
	} else {
		msg_id = CP_SUCCESS;
		strcpy(reply.parm.ifname, anIf->ifName);
		if (inhost(anIf->ifOldConfig->flags, request->ctrl.tag)) {
			HTSTRUCT *pht = find_bytag(request->ctrl.tag,
			    VENDOR_INDEPENDANT);
			const void *p = anIf->ifOldConfig->stags +
			    request->ctrl.tag;
			int type, len;
			if (pht == NULL)
				type = TYPE_BINDATA;
			else
				type = pht->type;
			len = need(type, p);
			if (len > 0)
				serializeItem(type, len, p, reply.parm.buf);
			else
				len = 0;
			reply.parm.len = len;
		} else
			reply.parm.len = 0;
	}
	reply.parm.flags = msg_id;
	reply.parm.ctrl_id = request->ctrl.ctrl_id;
	ctrl_reply(fd, &reply);
	close(fd);
}

static void
ctrl_release(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf == NULL)
		request->ctrl.flags = CP_IF_NOT_FOUND;
	else
		request->ctrl.flags = CP_SUCCESS;

	ctrl_reply(fd, request);
	close(fd);
	if (request->ctrl.flags == CP_SUCCESS)
		releaseIf(anIf);
}

static void
ctrl_extend(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf == NULL)
		request->ctrl.flags = CP_IF_NOT_FOUND;
	else if (anIf->ifState == DHCP_BOUND ||
	    anIf->ifState == DHCP_RENEWING ||
	    anIf->ifState == DHCP_REBINDING) {
		request->ctrl.flags = CP_SUCCESS;
	} else
		request->ctrl.flags = CP_IF_ALREADY_STARTED;

	ctrl_reply(fd, request);
	close(fd);
	if (request->ctrl.flags == CP_SUCCESS)
		extend(anIf);
}

static void
ctrl_drop(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf == NULL)
		request->ctrl.flags = CP_IF_NOT_FOUND;
	else
		request->ctrl.flags = CP_SUCCESS;

	ctrl_reply(fd, request);
	close(fd);
	if (request->ctrl.flags == CP_SUCCESS)
		dropIf(anIf);
}

static void
ctrl_start(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf != NULL) { /* interface already put under DHCP control */
		if (anIf->connfd >= 0) {
			request->ctrl.flags = CP_IF_ALREADY_STARTED;
			ctrl_reply(fd, request);
			close(fd);
			return;
		}
		if (anIf->ifState != DHCP_FAIL && anIf->ifState != DHCP_NULL) {
			request->ctrl.flags = CP_SUCCESS;
			ctrl_reply(fd, request);
			close(fd);
			return;
		}
		anIf->ifError = 0;
		anIf->ifState = DHCP_NULL;
	} else {
		/* new: not an interface which has failed (unless dropped) */
		anIf = newIf(request->ctrl.ifname);
		if (anIf == NULL) {
			request->ctrl.flags = CP_INTERNAL_ERROR;
			ctrl_reply(fd, request);
			close(fd);
			return;
		}

		/*
		 * random delay to avoid network swamping in cases of
		 * synchronized booting. Don't do this if trying to debug:
		 * it's too tedious for the user to wait
		 */
		if (debug <= 0)
			sleep((unsigned)(mrand48() % client.waitonboot));
	}

	logl(DHCPCMSG07, anIf->ifName);

	if (armIf(anIf, client.reqvec, &request->ctrl.server)) {
		request->ctrl.flags = CP_INTERNAL_ERROR;
		ctrl_reply(fd, request);
		close(fd);
		return;
	}

	if (request->ctrl.flags & CP_PRIMARY)
		setPrimaryInterface(request->ctrl.ifname);

	if (request->ctrl.timeout != 0) {
		anIf->connfd = fd;
		anIf->ctrl_id = request->ctrl.ctrl_id;
		anIf->ctrl_flags = request->ctrl.flags;
	}

#if DEBUG
	if (debug >= 4) {
		logb("new IFINSTANCE:\n");
		dumpIf(anIf);
	}
#endif

	if (request->ctrl.timeout == 0) {
		request->ctrl.flags = CP_SUCCESS;
		ctrl_reply(fd, request);
		close(fd);
	}
	startIf(anIf);
}

static void
ctrl_status(int fd, union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf == NULL)
		request->ctrl.flags = CP_IF_NOT_FOUND;
	else {
		request->ctrl.flags = CP_SUCCESS;
		request->ctrl.ifSent = anIf->ifSent;
		request->ctrl.ifReceived = anIf->ifReceived;
		request->ctrl.ifOffers = anIf->ifOffers;
		request->ctrl.ifBadOffers = anIf->ifBadOffers;
		request->ctrl.ifState = anIf->ifState;
		request->ctrl.ifBusy  = anIf->ifBusy;
		request->ctrl.ifError = anIf->ifError;
		request->ctrl.ifwakeup = anIf->ifTimer.wakeup;
		request->ctrl.ifgranted = anIf->ifTimer.granted;
		request->ctrl.ifPrimary = anIf->ifPrimary;
		request->ctrl.ifT0 = anIf->ifTimer.T0;
		request->ctrl.ifT1 = anIf->ifTimer.T1;
		request->ctrl.ifT2 = anIf->ifTimer.T2;
	}

	ctrl_reply(fd, request);
	close(fd);
}

static void
ctrl_timeout(const union dhcpipc *request)
{
	IFINSTANCE *anIf;

	if (request->ctrl.ifname[0] == '\0')
		return;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	if (anIf != NULL && anIf->ctrl_id == request->ctrl.ctrl_id) {
#if DEBUG
		if (debug >= 4)
			logb("controller on %s timed out : closing fd %d\n",
			    anIf->ifName, anIf->connfd);
#endif
		close(anIf->connfd);
		anIf->connfd = -1;
		anIf->ctrl_flags = 0;
		anIf->ctrl_id = 0;
	}
}

#if DHCP_OVER_PPP
static void
ctrl_inform(int fd, const union dhcpipc *request)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext) {
		if (strcmp(anIf->ifName, request->ctrl.ifname) == 0)
			break;
	}

	/*
	 * Want to allow getting info over an interface even if it wasn't
	 * started by DHCP. So create an IFINSTANCE.
	 */
	if (anIf == NULL) {
		anIf = newIf(request->ctrl.ifname);
		if (anIf)
			anIf->ifInformOnly = 1; /* don't cannonize iface */
		if (!anIf || armIf(anIf, client.reqvec,
		    &request->ctrl.server)) {
			request->ctrl.flags = CP_INTERNAL_EROR;
			ctrl_reply(fd, request);
			close(fd);
			return;
		}
		liClose(&anIf->lli);
	} else {
	/*
	 * Check state of existing Iface. Valid for bound & fail. if renew,
	 * check busy -- ok if not
	 */
	}

	informIf(anIf, ctrl);
	request->ctrl.flags = CP_SUCCESS;
	ctrl_reply(fd, request);
	close(fd);
}
#endif /* DHCP_OVER_PPP */

/* ARGSUSED */
static void
newConnection(int fd, int *newfd, ca_boolean_t *is_root)
{
	struct sockaddr_in from;
	socklen_t lfrom = (socklen_t)sizeof (from);
	uint16_t srcport;
#ifndef	USE_TCP
	char junk[1];
#endif

#if USE_TCP
	*newfd = accept(fd, (struct sockaddr *)&from, &lfrom);
	if (*newfd < 0) {
		if (errno == EWOULDBLOCK)
			return;
		else {
			loge(DHCPCMSG95, SYSMSG);
			exit(ECODE_SYSERR);
		}
	}
#else
	if (recvfrom(fd, junk, 1, MSG_PEEK, (struct sockaddr *)&from,
	    &lfrom) < 0) {
		loge(DHCPCMSG37, SYSMSG);
		exit(ECODE_SYSERR);
	}
#endif
	srcport = ntohs(from.sin_port);
#if DEBUG
	if (debug >= 4)
		logb("new conn on fd %d from (port, ip_address) (%d, %s)\n",
		    fd, srcport, ADDRSTR(&from.sin_addr));
#endif
	if (srcport >= IPPORT_RESERVED || srcport < (IPPORT_RESERVED / 2))
		*is_root = FALSE;
	else
		*is_root = TRUE;
#if !USE_TCP
	*newfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (*newfd < 0) {
		loge(DHCPCMSG31, SYSMSG);
		return;
	}
	if (connect(*newfd, (struct sockaddr *)&from, sizeof (from)) < 0) {
		loge(DHCPCMSG24, ntohs(from.sin_port),
		    ADDRSTR(&from.sin_addr), SYSMSG);
		return;
	}
#endif
}

static void
readControl(int readfd, int writefd, ca_boolean_t is_root, IFINSTANCE *anIf)
{
	union dhcpipc request;
	int reqType;
	int rc;

	rc = recv(readfd, (char *)&request, sizeof (request), 0);
	if (rc != sizeof (request)) {
		if (rc > 0 && rc < sizeof (request)) {
			loge(DHCPCMSG92, rc);
		} else {
			if (rc < 0)
				loge(DHCPCMSG37, SYSMSG);
			if (anIf != NULL && anIf->connfd >= 0) {
				close(anIf->connfd);
				anIf->connfd = -1;
			}
		}
		return;
	}

#if DEBUG
	if (debug >= 4) {
		logb("ifname\t= %s\n", request.ctrl.ifname);
		logb("timeout\t= %d\n", request.ctrl.timeout);
		logb("flags\t= %#x\n", request.ctrl.flags);
		logb("server\t= %s\n", ADDRSTR(&request.ctrl.server));
		logb("ctrl_id\t= %lu\n", request.ctrl.ctrl_id);
	}
#endif /* DEBUG */

	reqType = request.ctrl.flags & CP_REQUEST_TYPES;
	if (anIf == NULL && reqType != CP_STATUS && reqType != CP_PING &&
	    reqType != CP_FIND_TAG && reqType != CP_CTRL_TIMEOUT) {
		if (!is_root) {
			request.ctrl.flags = CP_PERMISSION;
			ctrl_reply(writefd, &request);
			close(writefd);
			return;
		}
	}

	if (request.ctrl.flags & CP_SERVER_SPECIFIED)
		server_addr = request.ctrl.server;
	else
		server_addr.s_addr = htonl(0);

	switch (reqType) {
	case CP_START:
		ctrl_start(writefd, &request);
		break;
	case CP_DROP:
		ctrl_drop(writefd, &request);
		break;
	case CP_RENEW:
		ctrl_extend(writefd, &request);
		break;
	case CP_RELEASE:
		ctrl_release(writefd, &request);
		break;
	case CP_PING:
		request.ctrl.flags = CP_SUCCESS;
		ctrl_reply(writefd, &request);
		close(writefd);
		break;
	case CP_FIND_TAG:
		ctrl_tag(writefd, &request);
		break;
	case CP_CTRL_TIMEOUT:
		ctrl_timeout(&request);
		break;
	case CP_STATUS:
		ctrl_status(writefd, &request);
		break;
#if DHCP_OVER_PPP
	case CP_INFORM:
		ctrl_inform(writefd, &request);
		break;
#endif /* DHCP_OVER_PPP */
	default:
		request.ctrl.flags = CP_UNIMPLEMENTED;
		ctrl_reply(writefd, &request);
		close(writefd);
		break;
	}
}

static void
sigpoll(void)
{
	int rc;
	int fdmax;
	int fd;
	IFINSTANCE *anIf;
	ca_boolean_t is_root;
	static struct pollfd *readfds;
	static int readfds_capacity;
#define	DELTA_READFDS_CAPACITY 4

	for (;;) {
		fdmax = 0;
		if (fdmax >= readfds_capacity) {
			readfds_capacity += DELTA_READFDS_CAPACITY;
			readfds = xrealloc(readfds, readfds_capacity *
			    sizeof (struct pollfd));
		}
		readfds[0].fd = client.controlSock;
		readfds[0].events = POLLIN|POLLPRI|POLLRDNORM|POLLRDBAND;
		readfds[0].revents = 0;
		fdmax++;

		for (anIf = ifList; anIf; anIf = anIf->ifNext) {
			if (anIf->lli.fd > 0) {
				if (fdmax >= readfds_capacity) {
					readfds_capacity +=
					    DELTA_READFDS_CAPACITY;
					readfds = xrealloc(readfds,
					    readfds_capacity *
					    sizeof (struct pollfd));
				}
				readfds[fdmax].fd = anIf->lli.fd;
				readfds[fdmax].events = POLLIN | POLLPRI |
				    POLLRDNORM | POLLRDBAND;
				readfds[fdmax].revents = 0;
				fdmax++;
			}
			if (anIf->fd > 0) {
				if (fdmax >= readfds_capacity) {
					readfds_capacity +=
					    DELTA_READFDS_CAPACITY;
					readfds = xrealloc(readfds,
					    readfds_capacity *
					    sizeof (struct pollfd));
				}
				readfds[fdmax].fd = anIf->fd;
				readfds[fdmax].events = POLLIN | POLLPRI |
				    POLLRDNORM | POLLRDBAND;
				readfds[fdmax].revents = 0;
				fdmax++;
			}
#if USE_TCP
			if (anIf->connfd > 0) {
				if (fdmax >= readfds_capacity) {
					readfds_capacity +=
					    DELTA_READFDS_CAPACITY;
					readfds = xrealloc(readfds,
					    readfds_capacity *
					    sizeof (struct pollfd));
				}
				readfds[fdmax].fd = anIf->connfd;
				readfds[fdmax].events = POLLIN | POLLPRI |
				    POLLRDNORM | POLLRDBAND;
				readfds[fdmax].revents = 0;
				fdmax++;
			}
#endif
		}

		rc = poll(readfds, fdmax, 0);
		if (rc == 0)
			return;
		else if (rc > 0) {
			if (readfds[0].revents != 0) {
#if DEBUG
				if (debug >= 4)
					polltype(readfds);
#endif
				newConnection(client.controlSock, &fd,
				    &is_root);
				if (fd >= 0) {
#if USE_TCP
					enableSigpoll(fd);
					readControl(fd, fd, is_root, 0);
#else
					readControl(client.controlSock, fd,
					    is_root, 0);
#endif
				}
			}
			for (; --fdmax > 0; ) {
				if (readfds[fdmax].revents != 0) {
#if DEBUG
					if (debug >= 4)
					    polltype(readfds + fdmax);
#endif
					for (anIf = ifList; anIf != NULL;
					    anIf = anIf->ifNext) {
						if (readfds[fdmax].fd ==
						    anIf->lli.fd) {
							readIFfd(anIf->lli.fd);
						} else if (readfds[fdmax].fd ==
						    anIf->fd) {
							readBOOTPsocket(
							    anIf->fd);
#if USE_TCP
						} else if (readfds[fdmax].fd ==
						    anIf->connfd) {
							readControl(
							    anIf->connfd,
							    anIf->connfd,
							    TRUE, anIf);
#endif
						}
					}
				}
			}
		}
	}
}

static void
handle_events(void)
{
	struct sigaction s;

	s.sa_flags = 0;
	s.sa_flags = SA_RESTART;
	sigemptyset(&s.sa_mask);
	sigaddset(&s.sa_mask, SIGPOLL);
	sigaddset(&s.sa_mask, SIGALRM);
	sigaddset(&s.sa_mask, SIGUSR1);
	s.sa_handler = sigpoll;
	(void) sigaction(SIGPOLL, &s, 0);
}

void
waitForEvent()
{
	int rc;

	handle_events();

	rc = controlSocket();
	if (rc)
		exit(1);

	rc = enableSigpoll(client.controlSock);
	if (rc < 0)
		exit(1);
#if USE_TCP
	listen(client.controlSock, 5);
#endif

	for (;;)
		pause();
}
