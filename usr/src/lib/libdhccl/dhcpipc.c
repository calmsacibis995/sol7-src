/*
 * dhcpipc.c: "Setup inter-process communication with DHCP agent".
 *
 * SYNOPSIS
 *	int getIPCfd(const char *prog, int reserved)
 *	int isDaemonUp(const char *prog)
 *	void doipc(int, const char*, union dhcpipc*, union dhcpipc*)
 *	void startDaemon()
 *
 * DESCRIPTION
 *    getIPCfd:
 *    Returns a file descriptor for use in
 *    communication with the DHCP agent.
 *    If failure returns -1 and set the variable
 *    ipc_errno to the error code.
 *
 *    isDaemonUp:
 *    Returns:
 *        0   DHCP agent is not running
 *        1   running
 *      < 0   some system error
 *
 *    doipc:
 *	Send and recive packets from the agent.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)dhcpipc.c	1.4	96/11/25 SMI"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "dhcpctrl.h"
#include "error.h"
#include "unixgen.h"
#include "msgindex.h"
#include "utils.h"

int ipc_errno;

int
getIPCfd(const char *prog, int reserved)
{
	int sock;

	struct sockaddr_in myaddr;
	short ipport;

#if USE_TCP
	sock = socket(AF_INET, SOCK_STREAM, 0);
#else
	sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif
	if (sock < 0) {
		fprintf(stderr, DHCPCMSG72, prog, SYSMSG);
		ipc_errno = ERR_CREATE_SOCKET;
		return (-1);
	}

	memset(&myaddr, 0, sizeof (myaddr));
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (reserved) {
		for (ipport = (IPPORT_RESERVED - 1);
		    ipport >= (IPPORT_RESERVED / 2); ipport--) {
			myaddr.sin_port = htons((u_short)ipport);
			if (bind(sock, (struct sockaddr *)&myaddr,
			    sizeof (myaddr)) == 0) {
				break;
			} else if (errno != EADDRINUSE) {
				fprintf(stderr, DHCPCMSG93, prog, SYSMSG);
				if (errno == EACCES)
					ipc_errno = ERR_NOT_ROOT;
				else
					ipc_errno = ERR_BIND_SOCKET;
				close(sock);
				return (-1);
			}
		}
		if (ipport < (IPPORT_RESERVED / 2)) {
			fprintf(stderr, DHCPCMSG94, prog, SYSMSG);
			ipc_errno = ERR_NO_PRIV_PORTS;
			close(sock);
			return (-1);
		}
	}
	return (sock);
}

int
isDaemonUp(const char *prog)
{
	register int retval = 0;
	static int bound;
	int rc;
	int sock;

	struct sockaddr_in addr;

	memset(&addr, 0, sizeof (addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(IPPORT_DHCPCONTROL);

#if USE_TCP
	sock = socket(AF_INET, SOCK_STREAM, 0);
#else
	sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif
	if (sock < 0) {
		fprintf(stderr, DHCPCMSG72, prog, SYSMSG);
		ipc_errno = ERR_CREATE_SOCKET;
		return (ERR_CREATE_SOCKET);
	}

	if (bound == 0) {
		sleep(2); /* Give dhcpagent time to bind in bootup */
		bound = 1;
	}
	rc = bind(sock, (struct sockaddr *)&addr, sizeof (addr));
	close(sock);

	if (rc == 0) {
		retval = 0;
	} else if (errno != EADDRINUSE) {
		fprintf(stderr, DHCPCMSG93, SYSMSG);
		ipc_errno = ERR_BIND_SOCKET;
		retval = ERR_BIND_SOCKET;
	} else
		retval = 1;
	return (retval);
}

void
ipc_endpoint(int fd)
{
	struct sockaddr_in destination;

	memset(&destination, 0, sizeof (destination));
	destination.sin_family = AF_INET;
	destination.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	destination.sin_port = htons(IPPORT_DHCPCONTROL);
	if (connect(fd, (struct sockaddr *)&destination,
	    sizeof (destination)) < 0) {
		fprintf(stderr, DHCPCMSG24, IPPORT_DHCPCONTROL,
		    ADDRSTR(&destination.sin_addr), SYSMSG);
		exit(ECODE_SYSERR);
	}
}

void
doipc(int fd, const char *prog, union dhcpipc *reqipc, union dhcpipc *reply)
{
	int rc;
	uint32_t tid = mrand48();

	reqipc->ctrl.ctrl_id = tid;

	rc = send(fd, (char *)reqipc, sizeof (*reqipc), 0);

	if (rc < 0) {
		if (errno == ENOENT) {
			fprintf(stderr, DHCPCMSG85, prog);
			exit(ECODE_NO_DHCP);
		} else {
			fprintf(stderr, DHCPCMSG76, prog, SYSMSG);
			exit(ECODE_SYSERR);
		}
	}

	for (;;) {
		rc = recv(fd, (char *)reply, sizeof (*reply), 0);
		if (rc < 0 || reply->ctrl.ctrl_id == tid)
			break;
	}

	if (rc < 0) {
		if (errno == EINTR)
			exit(ECODE_SIGNAL);
		else {
			fprintf(stderr, DHCPCMSG79, prog, SYSMSG);
			exit(ECODE_SYSERR);
		}
	}
}

void
startDaemon(void)
{
	int rc;

	rc = fork();

	if (rc < 0) {
		fprintf(stderr, DHCPCMSG88, prog, SYSMSG);
		exit(ECODE_SYSERR);
	} else if (rc > 0) {
		sleep(DHCPIPC_WAIT_FOR_AGENT);
		return;
	} else {
		rc = execl(DHCPIPC_DAEMON, DHCPIPC_DAEMON, 0);
		fprintf(stderr, DHCPCMSG89, prog, DHCPIPC_DAEMON, SYSMSG);
		exit(ECODE_SYSERR);
	}
}
