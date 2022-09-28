/*
 * init.c: "Setup DHCP init state".
 *
 * SYNOPSIS
 *    void init  (IFINSTANCE *anIf)
 *	void reinit(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)init.c	1.4	96/11/25 SMI"

#include <stdlib.h>
#include "client.h"
#include "catype.h"
#include "dhcpstate.h"

void
init(IFINSTANCE *anIf)
{
	anIf->ifReceived = 0;
	anIf->ifRetries = 0;
	anIf->ifOffers = 0;
	anIf->ifBadOffers = 0;
	anIf->ifSent = 0;
	anIf->ifXid = mrand48();
	anIf->ifState = DHCP_SELECTING;
	anIf->ifBusy = TRUE;
	prepareDiscover(anIf);
	retry(anIf, TRUE);
}

void
reinit(IFINSTANCE *anIf)
{
	anIf->ifRetries = 0;
	anIf->ifOffers = 0;
	anIf->ifXid = mrand48();
	anIf->ifState = DHCP_SELECTING;
	anIf->ifBusy = TRUE;
	prepareDiscover(anIf);
	retry(anIf, TRUE);
}
