/*
 * dhcpagent.c: "DHCP agent".
 *
 * USAGE
 *    dhcpagent [-b] [-f] [-d<level>] [-l<level>] [-n] [-e xx:xx:xx:xx:xx:xx]
 *
 * DESCRIPTION
 *    This is the main program of the DHCP client daemon.
 *    It parses the command line arguments, and calls
 *    various setup functions to create sockets, read
 *    policy files, and establish signal handlers. It
 *    daemonises itself unless the -f flag is given
 *    on the command line. If running as a daemon,
 *    output (log, warning, error and debug) messages
 *    are suitably re-directed.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident	"@(#)dhcpagent.c	1.7	97/03/10 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <client.h>
#include <utils.h>
#include <dccommon.h>
#include <ca_time.h>
#include <daemon.h>
#include <magic.h>
#include <catype.h>
#include <unixgen.h>

int debug;
int loglevel;
const char *prog;

static void
Usage(const char *prog)
{
	static const char *usageDescription =
	"Usage: %s [-d [0-9]] [-f] [-l [0-9]]"
	"\n"
	"\t -d level of debug output (one if level not given)\n"
	"\t -f run in foreground (not as daemon)\n"
	"\t -h help on command line switches (this message)\n"
	"\t -l level of log output (one if level not given)\n"
#if DEBUG
	"\n"
	"\t -b Perform complete reconfiguration\n"
	"\t -e pretend to be at Ethernet address xx:xx:xx:xx:xx:xx\n"
	"\t -n test mode - don't really configure interface\n"
#endif
	;

	(void) fprintf(stderr, usageDescription, prog);
}

int
main(int argc, char **argv)
{
	register int c;
	int rc;
	int daemonise = TRUE;

	prog = argv[0];
	client.boot = 0;
	client.DontConfigure = 0;
	debug = 0;
	loglevel = 2;
	stdbug = stderr;

	if (geteuid() != (uid_t)0) {
		(void) fprintf(stderr, "Must be 'root' to run dhcpagent.\n");
		return (1);
	}
	while ((c = getopt(argc, argv,
#ifndef	DEBUG
	    "d:fl:"
#else
	    "d:e:fbl:n:"
#endif	/* !DEBUG */
	    /* CSTYLED */)) != -1) {
		switch (c) {
		case 'd':
			if (isdigit(*optarg))
				debug = atoi(optarg);
			if (debug == 0)
				debug = 1;
			break;

		case 'f':
			daemonise = FALSE;
			break;

		case 'l':
			if (isdigit(*optarg))
				loglevel = atoi(optarg);
			else
				loglevel = 1;
			break;
#ifdef	DEBUG
		/* The following three switches are only for debug purposes */
		case 'b':
			/* Perform complete reconfiguration of interface */
			client.boot = 1;
			break;

		case 'e':
			client.UserDefinedMacAddr = (HADDR *)xmalloc(
			    sizeof (HADDR));
			client.UserDefinedMacAddr->htype = HTYPE_ETHERNET;
			client.UserDefinedMacAddr->hlen = 6;
			if (haddrtok((const char **)&optarg,
			    client.UserDefinedMacAddr->chaddr, 6) != 6) {
				free(client.UserDefinedMacAddr);
				client.UserDefinedMacAddr = NULL;
				(void) fprintf(stderr,
				    "Bad ethernet address\n");
			}
			break;

		case 'n':
			/* dont' configure and don't update files */
			client.DontConfigure = 1;
			break;
#endif	/* DEBUG */
		default:
			Usage(prog);
			return (1);
		}
	}

	if (daemonise)
		(void) daemon();

	client.dhcpconfigdir = CONFIG_DIR;

	loginit(loglevel, debug, daemonise, "/dev/console");

	if ((rc = agentInit()) != 0) {
		logclose();
		return (rc);
	}

	handle_terminate_signals();
	atexit(agent_exit);

	install_inactivity_timer();
	waitForEvent();

	logclose();
	return (0);
}
