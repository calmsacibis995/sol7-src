/*
 * clinit.c: "Initialise DHCP agent with defaults and external file".
 *
 * SYNOPSIS
 *    int agentInit()
 *
 * DESCRIPTION
 *    Perform client initialisation. Setup path to configuration
 *    directory, read the policy file (client.pcy), and setup mask for those
 *    signals which are to be blocked during a timeout (i.e.
 *    in alarm()).
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)clinit.c 1.4 96/12/26 SMI"

#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "msgindex.h"
#include "error.h"
#include <alloca.h>
#include <signal.h>
#include "catype.h"
#include <sys/systeminfo.h>
#include <stdio.h>
#include <string.h>
#include <unixgen.h>

IFINSTANCE *ifList;

int
agentInit(void)
{
	int rc;

	seed();

	rc = clientSocket(FALSE, 0);  /* don't bind */
	if (rc < 0)
		return (rc);
	client.sock = rc;

	sysinfo(SI_HOSTNAME, client.hostname, sizeof (client.hostname));
	init_dhcp_types(client.dhcpconfigdir, 0);
	rc = clpolicy(client.dhcpconfigdir);
	sigemptyset(&client.blockDuringTimeout);
	sigaddset(&client.blockDuringTimeout, SIGINT);
	sigaddset(&client.blockDuringTimeout, SIGQUIT);
	sigaddset(&client.blockDuringTimeout, SIGTERM);
	sigaddset(&client.blockDuringTimeout, SIGPIPE);
	sigaddset(&client.blockDuringTimeout, SIGPOLL);
	sigaddset(&client.blockDuringTimeout, SIGALRM);
	sigaddset(&client.blockDuringTimeout, SIGUSR1);
	return (rc);
}
