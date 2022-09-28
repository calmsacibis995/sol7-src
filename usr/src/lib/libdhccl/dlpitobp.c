/*
 * dlpitobp.c: "Convert DLPI media type to BOOTP conventions".
 *
 * SYNOPSIS
 *    int DLPItoBOOTPmediaType(int dlpimedium)
 *
 * DESCRIPTION
 *    Return the BOOTP media (MAC) type corresponding to a particular
 *    DLPI media type. If no sensible correspondence can be found
 *    return zero.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)dlpitobp.c 1.2 96/11/18 SMI"

#include "ca_dlpi.h"
#include "camacros.h"
#include "utils.h"
#include "haddr.h"

extern char *sys_errlist[];

static int DLPImediaTypes[] = {
	DL_CSMACD,	DL_TPB,		DL_TPR,		DL_METRO,
	DL_ETHER,	DL_HDLC,	DL_CHAR,	DL_CTCA,
	DL_FDDI,	DL_OTHER
};

static int BootpMediaTypes[] = {
	HTYPE_IEEE802,	HTYPE_IEEE802,	HTYPE_IEEE802,	HTYPE_IEEE802,
	HTYPE_ETHERNET,	0,		0,		0,
	HTYPE_IEEE802,	0
};

int
DLPItoBOOTPmediaType(int dlpimedium)
{
	register int	i;
	register int	retval = 0;

	for (i = 0; i < ARY_SIZE(DLPImediaTypes); i++) {
		if (DLPImediaTypes[i] == dlpimedium) {
			retval = BootpMediaTypes[i];
			break;
		}
	}
	return (retval);
}
