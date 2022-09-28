/*
 * startif.c: "Start an interface in DHCP in either init ot init-reboot state".
 *
 * SYNOPSIS
 *    void startIf(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    Starting point for DHCP configuration. The IFINSTANCE
 *    structure has been primed with information about
 *    the current state of the interface, and any pre
 *    existing DHCP configuration stored on disk
 *    (/etc/dhcp/xx0.dhc). Depending on whether the
 *    latter was valid (lease not yet expired) the
 *    client starts in either the "selecting" state
 *    or the "rebooting" state.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)startif.c 1.3 96/11/25 SMI"

#include "dhcpstate.h"
#include "client.h"
#include "ca_time.h"

void
startIf(IFINSTANCE *anIf)
{
	anIf->ifBegin = GetCurrentSecond();

	if (anIf->ifState == DHCP_REBOOTING)
		initReboot(anIf);
	else
		init(anIf);
}
