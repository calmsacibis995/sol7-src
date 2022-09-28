/*
 * ireboot.c: "Setup DHCP init-reboot state".
 *
 * SYNOPSIS
 *    void initReboot(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)ireboot.c	1.4	96/11/25 SMI"

#include <stdlib.h>
#include "dhcpstate.h"
#include "client.h"
#include "catype.h"

void
initReboot(IFINSTANCE *anIf)
{
	anIf->ifRetries = 0;
	anIf->ifReceived = 0;
	anIf->ifSent = 0;
	anIf->ifOffers = 0;
	anIf->ifBadOffers = 0;
	anIf->ifState = DHCP_REBOOTING;
	anIf->ifXid = mrand48();
	anIf->ifBusy = TRUE;
	prepareReboot(anIf);
	retry(anIf, TRUE);
}
