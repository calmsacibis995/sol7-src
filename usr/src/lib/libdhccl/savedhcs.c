/*
 * savedhcs.c: "Write or unlink all DHCP configurations to disk".
 *
 * SYNOPSIS
 *	void discard_dhc(IFINSTANCE *anIf)
 *	void save_dhc(IFINSTANCE *vanIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)savedhcs.c 1.3 96/11/25 SMI"

#include "client.h"
#include "utils.h"

static void
try_save(void *v)
{
	IFINSTANCE *anIf = (IFINSTANCE *)v;

	anIf->ifRWID = 0;
	if (!fswok(client.dhcpconfigdir))
		anIf->ifRWID = schedule(FSWRITE_RETRY_TIME, try_save, anIf,
		    &client.blockDuringTimeout);
	else
		write_dhc(client.dhcpconfigdir, anIf->ifName,
		    anIf->ifOldConfig);
}

void
save_dhc(IFINSTANCE *anIf)
{
	if (anIf->ifRWID != 0)
		cancelWakeup(anIf->ifRWID);

	try_save(anIf);
}

static void
try_discard(void *v)
{
	IFINSTANCE *anIf = (IFINSTANCE *)v;

	anIf->ifRWID = 0;
	if (!fswok(client.dhcpconfigdir))
		anIf->ifRWID = schedule(FSWRITE_RETRY_TIME, try_discard, anIf,
		    &client.blockDuringTimeout);
	else
		unlink_dhc(client.dhcpconfigdir, anIf->ifName);
}

void
discard_dhc(IFINSTANCE *anIf)
{
	if (anIf->ifRWID != 0)
		cancelWakeup(anIf->ifRWID);
	try_discard(anIf);
}
