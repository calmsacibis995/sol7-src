/*
 *  inactivity.c: "Agent activity monitors".
 *
 * SYNOPSIS
 *	void cancel_inactivity_timer()
 *	void install_inactivity_timer()
 *
 * DESCRIPTION
 *	The agent will exit after a grace-period whenever the number of
 *	interfaces under its control drops to zero. The grace period
 *	must be sufficiently long to allow the agent to actually
 *	receive instructions from ifconfig, but other than that is
 *	arbitrary. Currently set at 5 minutes.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)activity.c 1.2 96/11/17 SMI"

#include "msgindex.h"
#include "unixgen.h"
#include "utils.h"

#define	AGENT_INACTIVITY_TIMEOUT	(300)

static int32_t inactivity_timer_id;

/* ARGSUSED */
static void
inactivity_timeout(void *v)
{
	logl(DHCPCMSG66);
	exit(0);
}

void
cancel_inactivity_timer(void)
{
	cancelWakeup(inactivity_timer_id);
}

void
install_inactivity_timer(void)
{
	inactivity_timer_id =
		schedule(AGENT_INACTIVITY_TIMEOUT, inactivity_timeout, 0, 0);
}
