/*
 * dhcpinfo.c: "Retrieve and display data from DHCP configuration".
 *
 * SYNOPSIS
 *    dhcpinfo [-i <interface>] <tag> | <symbol> | <name>
 *
 * DESCRIPTION
 *    Dhcpinfo prints the value of a DHCP parameter to stdout. The parameter
 *    may be given as a numeric tag, a two character abbreviation, or by
 *    its full name. The interface name (-i argument) is not usually given
 *    except for parameters which are set on a per-interface basis, rather
 *    than per-host, but whenever it is given dhcpinfo will only consult
 *    the configuration file for that specific interface. Parameters
 *    are normally single valued numerics or strings: in the case of
 *    lists the list items are printed one per line. This makes dhcpinfo
 *    a convenient program to use in shell scripts, but its exit status
 *    (see below) should be checked to determine the validity of its
 *    output.
 *
 * DIAGNOSTICS
 *    May exit with the following codes:
 *	0 success
 *	2 DHCP was not successful (presumably no servers replied)
 *	3 Bad arguments
 *	4 Timed out (interface not successfully configured before
 *	    interval given with -w)
 *	5 Permission: program must be run as root
 *	6 Some system error (should never occur)
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 */

#pragma ident	"@(#)dhcpinfo.c	1.6	96/12/17 SMI"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/errno.h>
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "hostdefs.h"
#include "hosttype.h"
#include "unixgen.h"
#include "dhcpctrl.h"
#include "error.h"

#define	DHCPINFO_TIMEOUT	20

const char *prog;
static int fd = -1;
static union dhcpipc query;

/* ARGSUSED */
static void
catchTermination(int sig)
{
	query.ctrl.flags = CP_CTRL_TIMEOUT;
	if (fd >= 0) {
		send(fd, (char *)&query, sizeof (query), 0);
		close(fd);
	}
	exit(ECODE_SIGNAL);
}

/* ARGSUSED */
static void
catchAlarm(int sig)
{
	query.ctrl.flags = CP_CTRL_TIMEOUT;
	if (fd >= 0)
		send(fd, (char *)&query, sizeof (query), 0);
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

static void
usage(const char *prog)
{
	const char *usageDescription =
	    "Usage: %s [-i <interface>] [-n <number>] <tag>|<id>|<name>\n"
	    "\t -i use DHCP configuration for <interface>\n"
	    "\t -n to limit # values printed from a list\n"
	    "\t use integer tag#, symbol, or description for DHCP parameter\n";

	fprintf(stderr, usageDescription, prog);
	exit(ECODE_BAD_ARGS);
}

int
main(int argc, char **argv)
{
	register int c;
	char ifname[IFNAMSIZ];
	char symbuf[16];
	int tag = 0;
	int type;
	int rc;
	int limit = -1;
	const HTSTRUCT *pht;
	const VTSTRUCT *vendor = 0;
	union dhcpipc answer;
	DHCP_TYPES data;

	prog = argv[0];
	loglbyl();
	memset(ifname, 0, sizeof (ifname));

	opterr = 0;
	while ((c = getopt(argc, argv, "i:n:")) != -1) {
		switch (c) {
		case 'i':
			(void) strncpy(ifname, optarg, IFNAMSIZ - 1);
			break;
		case 'n':
			if (isdigit(*optarg))
				limit = atoi(optarg);
			else
				limit = -1;
			break;
		default:
			usage(prog);
			return (ECODE_BAD_ARGS);
		}
	}

	if (optind >= argc) {
		usage(prog);
		return (ECODE_BAD_ARGS);
	}

	rc = isDaemonUp(prog);
	if (rc >= ERR_FIRST && rc <= ERR_LAST)
		return (ECODE_SYSERR);
	else if (rc == 0)
		return (ECODE_NO_DHCP);

	client.dhcpconfigdir = CONFIG_DIR;
	init_dhcp_types(CONFIG_DIR, 0);
	clpolicy(CONFIG_DIR);
	if (client.class_id != 0 && client.class_id[0] != '\0')
		vendor = findVT(client.class_id);

	if (sscanf(argv[optind], "%d", &tag) == 1) {
		if (vendor)
			pht = find_bytag(tag, vendor->selfindex);
		else
			pht = find_bytag(tag, VENDOR_INDEPENDANT);
		if (pht != 0) {
			type = pht->type;
		} else if (IS_PSEUDO(tag)) {
			fprintf(stderr, "Bad tag Number: %d\n", tag);
			return (ECODE_BAD_ARGS);
		} else {
			sprintf(symbuf, "T%d", tag);
			type = TYPE_BINDATA;
		}
	} else {
		pht = find_bysym(argv[optind]);
		if (pht == 0) {
			pht = find_bylongname(argv[optind]);
			if (pht == 0) {
				(void) fprintf(stderr, "Bad <symbol> <%s>\n",
				    argv[optind]);
				return (ECODE_BAD_ARGS);
			}
		}
		tag = pht->tag;
		type = pht->type;
	}

	seed();

	memset(&query, 0, sizeof (query));
	query.ctrl.flags = CP_FIND_TAG;
	query.ctrl.tag = tag;
	if (ifname != 0) {
		strncpy(query.ctrl.ifname, ifname, IFNAMSIZ);
		query.ctrl.ifname[IFNAMSIZ - 1] = '\0';
	}

	fd = getIPCfd(prog, 0);
	if (fd < 0) {
		if (ipc_errno == ERR_NOT_ROOT)
			return (ECODE_PERMISSION);
		else
			return (ECODE_SYSERR);
	}
	ipc_endpoint(fd);

	setupHandlers(DHCPINFO_TIMEOUT);
	doipc(fd, prog, &query, &answer);

	if (answer.ctrl.flags != CP_SUCCESS) {
		/* failure: don't distinguish the cause */
		return (ECODE_NO_DHCP);
	}
	if (answer.parm.len == 0) {
		/*
		 * DHCP had configured the interface, but this parameter
		 * was not found
		 */
		return (ECODE_NOT_FOUND);
	}

	deserializeItem(type, answer.parm.len, &data, answer.parm.buf);
	dump_dhcp_item(stdout, type, 0, &data, 0, '\n', limit);

	return (0);
}
