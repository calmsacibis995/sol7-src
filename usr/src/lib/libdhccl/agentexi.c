/*
 * agentexit.c: "Cleanup agent after termination signal received".
 *
 * SYNOPSIS
 *    void agent_exit()
 *
 * DESCRIPTION
 *	This is the signal handling routine called when a termination
 *	signal (SIGINT,SIGQUIT,SIGTERM) is received by the agent.
 *	Its only job is to turn off the IFF_DHCPRUNNING flag on the
 *	interfaces so that they do not give the false impression of
 *	being under DHCP control.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)agentexit.c 1.5 96/12/27 SMI"

#include <stdlib.h>
#include <sys/types.h>
#include <syslog.h>
#include "client.h"
#include "utils.h"

void
agent_exit(void)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf != 0; anIf = anIf->ifNext)
		canonical_if(anIf, TRUE);
	logclose();
	exit(0);
}
