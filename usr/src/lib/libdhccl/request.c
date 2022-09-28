/*
 * request.c: "Make DHCP state transition from selecting to requesting".
 *
 * SYNOPSIS
 *    void request(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    Called when the client makes a transition from "selecting"
 *    to "requesting". An offer has been received which the
 *    client likes and the client is asking the selected server
 *    to confirm the offer with an ACK.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)request.c 1.4 96/11/25 SMI"

#include <stdlib.h>
#include "dhcpstate.h"
#include "client.h"
#include "catype.h"

void
request(IFINSTANCE *anIf)
{
	anIf->ifRetries = 0;
	anIf->ifXid = mrand48();
	anIf->ifState = DHCP_REQUESTING;
	prepareRequest(anIf);
	retry(anIf, TRUE);
}
