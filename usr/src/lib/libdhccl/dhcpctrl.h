/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */


/*
 * DESCRIPTION
 * Defines and structures used in inter-process communication
 * between the agent and the controlling entities (ifconfig,
 * netstat, dhcpinfo).
 */

#ifndef _DHCPCTRL_H
#define	_DHCPCTRL_H

#pragma ident	"@(#)dhcpctrl.h	1.4	96/11/25 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	IPPORT_DHCPCONTROL	((uint16_t)4999)

typedef struct dhcpctrl Dhcpctrl;
struct dhcpctrl {
	char  ifname[IFNAMSIZ];
	uint16_t flags;
	uint32_t ctrl_id;
	uint32_t tag;
	int32_t timeout;
	int32_t ifPrimary;
	int32_t ifSent;
	int32_t ifReceived;
	int32_t ifOffers;
	int32_t ifBadOffers;
	int32_t ifState;
	int32_t ifBusy;
	int32_t ifError;
	time_t ifwakeup;
	time_t ifgranted;
	time_t ifT0;
	time_t ifT1;
	time_t ifT2;
	struct in_addr server;
};

typedef struct dhcpparm Dhcpparm;
struct dhcpparm {
	char ifname[IFNAMSIZ];
	uint16_t flags;
	uint32_t ctrl_id;
	u_char len;
	u_char buf[255];
};

union dhcpipc {
	struct dhcpctrl ctrl;
	struct dhcpparm parm;
};

/*
 * All these control protocol (as opposed to DHCP protocol) start with the
 * 3 character CP_ mnemonic
 */

/* Commands to control DHCP start/stio on interfaces */
#define	CP_REQUEST_TYPES	(0x0f)
#define	CP_NOOP			(0x00)
#define	CP_START		(0x01)
#define	CP_DROP			(0x02)
#define	CP_RELEASE		(0x03)
#define	CP_STATUS		(0x04) /* status of DHCP on the interface */
#define	CP_RENEW		(0x05)
#define	CP_PING			(0x06)
#define	CP_FIND_TAG		(0x07)
/* tells the agent that the instigator of the DHCP operation is timing out */
#define	CP_CTRL_TIMEOUT		(0x08)
#define	CP_INFORM		(0x09) /* primarily for PPP interfaces */
#define	CP_RETRY_MODES		(0x100)
#define	CP_FAIL_IF_TIMEOUT	(0x100)
#define	CP_SERVER_SPECIFIED	(0x200)
#define	CP_PRIMARY		(0x400)

/* Return codes from agent (set in flags field) */
#define	CP_SUCCESS		0
/* interface already started and bound to another instance of dhcpconf */
#define	CP_IF_ALREADY_STARTED	1
#define	CP_IF_NOT_FOUND		2 /* no such interface */
#define	CP_NO_RESPONSE		3 /* no responses to DHCP on interface */
#define	CP_INTERNAL_ERROR	4 /* unexpected system error */
#define	CP_UNIMPLEMENTED	5 /* unimplemented option */
#define	CP_PERMISSION		6 /* not run as root */
#define	CP_NOT_CONFIGURED	7 /* Interface(s) not config'd successfully */

#define	CP_REQUIRES_ROOT_PRIV(x) \
	((x) != CP_STATUS && (x) != CP_PING && (x) != CP_FIND_TAG)

#define	CP_DEFAULT_TIMEOUT	30 /* default timeout on "start" or "extend" */

/* Exit codes of dhcpconf, dhcpinfo */

#define	ECODE_NOT_FOUND		0 /* parameter(s) not found in DHCP config */
#define	ECODE_NO_DHCP		2 /* DHCP not run or failed */
#define	ECODE_BAD_ARGS		3 /* bad arguments or non-existant if name */
#define	ECODE_TIMEOUT		4 /* controlling program timed out */
#define	ECODE_PERMISSION	5 /* not run as root */
#define	ECODE_SYSERR		6 /* unexpected system error */
#define	ECODE_SIGNAL		7 /* going down on signal */

/* agent settings */
#define	DHCPIPC_DAEMON		"/sbin/dhcpagent"	/* path to agent */
#define	DHCPIPC_WAIT_FOR_AGENT	5			/* seconds */

void doipc(int, const char *, union dhcpipc *, union dhcpipc *);
int getIPCfd(const char *, int);
void ipc_endpoint(int);
int isDaemonUp(const char *);
void startDaemon(void);

extern int ipc_errno;
extern const char *prog;

#ifdef	__cplusplus
}
#endif

#endif /* _DHCPCTRL_H */
