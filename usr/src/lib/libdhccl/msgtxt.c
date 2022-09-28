/*
 * msgtxt.c: "Text of agent messages".
 *
 * USAGE
 *	const char *msgtxt(int index)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)msgtxt.c 1.2 96/11/21 SMI"

#include <libintl.h>

static const char *msg[] = {
	/* 00 */ "interface %s: unknown PPA\n",
	/* 01 */ "interface %s: not Ethernet\n",
	/* 02 */ "interface %s: can't push packet filter module : %s\n",
	/* 03 */ "interface %s: can't activate filter : %s\n",
	/* 04 */ "raw read failed: %s\n",
	/* 05 */ "%s: DHCP message from server: %s\n",
	/* 06 */ "%s: interrupted by signal\n",
	/* 07 */ "DHCP started on %s\n",
	/* 08 */ "",
	/* 09 */ "",
	/* 10 */ "can't get flags on %s : %s\n",
	/* 11 */ "can't set unicast address on %s : %s\n",
	/* 12 */ "",
	/* 13 */ "can't set flags on %s : %s\n",
	/* 14 */ "\t %-16s:  %s\n",
	/* 15 */ "can't set default gateway %s: %s\n",
	/* 16 */ "static routes:\n",
	/* 17 */ "can't get interface address on %s : %s\n",
	/* 18 */ "static route via %s to illegal destination 0.0.0.0 ignored\n",
	/* 19 */ "can't set route to %s via %s : %s\n",
	/* 20 */ "route to %s via %s\n",
	/* 21 */ "ERROR! Address %s is already in use\n",
	/* 22 */ "DHCP configuring %s\n",
	/* 23 */ "can't set subnet mask on %s : %s\n",
	/* 24 */ "can't connect to (%d, %s) : %s\n",
	/* 25 */ "",
	/* 26 */ "",
	/* 27 */ "",
	/* 28 */ "",
	/* 29 */ "",
	/* 30 */ "can't set hostname : %s\n",
	/* 31 */ "can't create socket : %s\n",
	/* 32 */ "can't set socket option SO_BROADCAST : %s\n",
	/* 33 */ "can't send to loopback address : %s\n",
	/* 34 */ "can't bind socket to (port, address) (%d, %s) : %s\n",
	/* 35 */ "%s failed : can't send : %s\n",
	/* 36 */ "",
	/* 37 */ "recvfrom failed : %s\n",
	/* 38 */ "",
	/* 39 */ "%s: %s : timed out after %d secs\n",
	/* 40 */ "",
	/* 41 */ "can't open %s : %s\n",
	/* 42 */ "discarding fragmented BOOTP packet from <IP> <%s>\n",
	/* 43 */ "",
	/* 44 */ "can't get MAC address of interface '%s' : %s\n",
	/* 45 */ "",
	/* 46 */ "address of interface %s has changed: old = %s, new = %s\n",
	/* 47 */ "",
	/* 48 */ "",
	/* 49 */ "",
	/* 50 */ "%s is not a configuration file : wrong magic cookie\n",
	/* 51 */ "",
	/* 52 */ "can't set ownership on control socket : %s\n",
	/* 53 */ "can't ioctl() control socket to deliver SIGPOLL : %s\n",
	/* 54 */ "%s unrecognized option : ignoring\n",
	/* 55 */ "accepting abbreviation %s for %s\n",
	/* 56 */ "%s requires a value\n",
	/* 57 */ "%s needs a numeric value (value=%s)\n",
	/* 58 */ "unrecognized configuration parameter %s\n",
	/* 59 */ "%s needs a comma-separated list of numbers with last <= 0\n",
	/* 60 */ "attempt to extend DHCP lease NAK'd: please reboot\n",
	/* 61 */ "DHCP lease on %s expires in less than %d seconds\n",
	/* 62 */ "DHCP lease on %s has expired - interface now down!\n",
	/* 63 */ "DHCP renewal on %s failed\n",
	/* 64 */ "DHCP rebind on %s failed\n",
	/* 65 */ "can't open packet filter on interface %s : %s\n",
	/* 66 */ "dhcpagent: no interfaces to configure - exiting\n",
	/* 67 */ "",
	/* 68 */ "",
	/* 69 */ "",
	/* 70 */ "",
	/* 71 */ "can't enable SIGPOLL generation on stream head\n",
	/* 72 */ "%s: can't open socket: %s\n",
	/* 73 */ "%s: no such interface as %s\n",
	/* 74 */ "%s: unrecognized request '%s'\n",
	/* 75 */ "%s: %s is not a valid IP address\n",
	/* 76 */ "%s: can't send to DHCP agent: %s\n",
	/* 77 */ "%s: timed out after %d seconds\n",
	/* 78 */ "%s: exiting on signal %d\n",
	/* 79 */ "%s: can't receive on socket: %s\n",
	/* 80 */ "%s: DHCP on interface %s already underway\n",
	/* 81 */ "%s: interface %s not under DHCP control\n",
	/* 82 */ "%s: interface %s is not one configurable by DHCP\n",
	/* 82 */ "%s: internal error in DHCP agent\n",
	/* 84 */ "%s: unknown error code (%d) returned from DHCP agent\n",
	/* 85 */ "%s: DHCP agent is not running\n",
	/* 86 */ "%s: unimplemented or unknown command\n",
	/* 87 */ "%s: must be root to execute\n",
	/* 88 */ "%s: can't fork : %s\n",
	/* 89 */ "%s: can't exec %s: %s\n",
	/* 90 */ "%s: DHCP failed to configure interface %s\n",
	/* 91 */ "%s: can't get interface list : %s\n",
	/* 92 */ "short packet (length=%d octets) received\n",
	/* 93 */ "%s: can't bind to socket: %s\n",
	/* 94 */ "%s: no unused reserved port numbers\n",
	/* 95 */ "can't create end point for connection (accept failed): %s\n",
	0
};

#define	MAXMSGINDEX	(sizeof (msg) / sizeof (msg[0]) - 2)

const char *
msgtxt(int index)
{
	static const char *retval = "message text not found\n";

	if (index >= 0 && index < MAXMSGINDEX)
		retval = msg[index];
	return (gettext(retval));
}
