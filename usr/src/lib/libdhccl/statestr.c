/*
 * statestr.c: "Make character string corresponding to the DHCP state".
 *
 * SYNOPSIS
 *    const char *ifStateString(int ifState)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)statestr.c 1.2 96/11/21 SMI"

#include "dhcpstate.h"

static int states[] = {
	DHCP_SELECTING, DHCP_REQUESTING,  DHCP_BOUND, DHCP_REBOOTING,
	DHCP_RENEWING,  DHCP_REBINDING, DHCP_FAIL, BOOTP, DHCP_NULL
};

static const char *stateAsString[] = {
	"SELECTING", "REQUESTING",  "BOUND", "REBOOTING", "RENEWING",
	"REBINDING", "FAIL", "BOOTP", "NULL"
};

static const char *unknownType = "unknown DHCP state";

const char *
ifStateString(int ifState)
{
	register int i;

	for (i = 0; i < (sizeof (states) / sizeof (states[0])); i++)
		if (ifState == states[i])
			return (stateAsString[i]);

	return (unknownType);
}

const char *
ifBusyString(int ifBusy)
{
	const char *busyStr = "BUSY";
	const char *idleStr = "IDLE";

	if (ifBusy)
		return (busyStr);
	else
		return (idleStr);
}
