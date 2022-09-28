/*
 * drop.c: "Drop an interface from DHCP control".
 *
 * SYNOPSIS
 *    void dropIf(IFINSTANCE *anIf)
 *
 * DESCRIPTION
 *    Remove the specified interface from DHCP control. Note that
 *    this is not the same as release, which is part of the
 *    DHCP protocol. Drop is not part of DHCP control -- it
 *    says, in effect, I don't want DHCP control of this
 *    interface any longer.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)drop.c 1.5 96/12/27 SMI"

#include <unistd.h>
#include <malloc.h>
#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "catype.h"

void
dropIf(IFINSTANCE *anIf)
{
	IFINSTANCE *pIf, *prevIf = NULL;

	for (pIf = ifList; pIf != NULL && pIf != anIf; prevIf = pIf,
	    pIf = pIf->ifNext) {
		/* Null body */;
	}
	if (pIf != NULL) {
		if (prevIf)
			prevIf->ifNext = anIf->ifNext;
		else
			ifList = anIf->ifNext;
		cancelWakeup(anIf->ifTimerID);
		free_host(anIf->ifsendh);
		free_host(anIf->ifConfig);
		free_host(anIf->ifOldConfig);
		liClose(&anIf->lli);
		if (anIf->fd >= 0)
			close(anIf->fd);
		canonical_if(anIf, TRUE);
		free(anIf);
		if (ifList == NULL)
			install_inactivity_timer();
	}
}
