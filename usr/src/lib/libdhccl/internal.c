/*
 * internal.c: "Dump internals for debugging".
 *
 * SYNOPSIS
 *    void dumpIf(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)M%	1.3	96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "unixgen.h"

void
dumpIf(IFINSTANCE *anIf)
{
	logb("%s\n", anIf->ifName);
	logb("\t Internal address\t= %#-8lx\n", anIf);
	logb("\t State\t\t= %s\n", ifStateString(anIf->ifState));
	logb("\t Busy/Idle\t\t= %s\n", ifBusyString(anIf->ifBusy));
	logb("\t Primary\t\t= %s\n", anIf->ifPrimary ? "true" : "false");
	logb("\t Retries\t\t= %d\n", anIf->ifRetries);
	logb("\t Received\t\t= %d\n", anIf->ifReceived);
	logb("\t Sent\t\t= %d\n", anIf->ifSent);
	logb("\t Offers\t\t= %d\n", anIf->ifOffers);
	logb("\t Bad offers\t\t= %d\n", anIf->ifBadOffers);
	logb("\t Timer ID\t\t= %d\n", anIf->ifTimerID);
	logb("\t Xid\t\t= %#lx\n", (u_long)anIf->ifXid);
	logb("\t Ordinal#\t\t= %d\n", anIf->ifOrd);
	logb("\t Error\t\t= %d\n", anIf->ifError);
	logb("\t Control flags\t\t= %#x\n", anIf->ctrl_flags);
	logb("\t Control ID\t\t= %lu\n", anIf->ctrl_id);
	logb("\t LLI fd\t\t= %d\n", anIf->lli.fd); /* %%% */
	logb("\t socket fd\t\t= %d\n", anIf->fd); /* %%% */
	logb("\t ipc fd\t\t= %d\n", anIf->connfd);
}

void
dumpInternals(void)
{
	IFINSTANCE *anIf;

	for (anIf = ifList; anIf; anIf = anIf->ifNext)
		dumpIf(anIf);

	showWakeups();
}
