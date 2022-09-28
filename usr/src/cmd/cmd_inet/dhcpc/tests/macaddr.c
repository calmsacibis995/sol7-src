/*
 * macaddr.c: "Find MAC address of interface".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)macaddr.c	1.4	96/12/26 SMI"

#include "unixgen.h"
#include "haddr.h"
#include "ca_time.h"
#include "utils.h"
#include "utiltxt.h"
#include "ca_dlpi.h"
#include <fcntl.h>
#include <string.h>
#include <stropts.h>

#ifdef	DEBUG
int debug;
#endif	/* DEBUG */

int
main(int argc, char *argv[])
{
	HADDR h;
	int rc;

	loglbyl();
	for (argv++; --argc > 0; argv++) {
		rc = myMacAddr(&h, argv[0]);
		if (rc == 0)
			printf("%-10s <HW> = (%d, %s)\n", argv[0], h.hlen,
			    addrstr(h.chaddr, h.hlen, 1, ':'));
		else
			printf("%-10s failed\n", argv[0]);
	}
	return (0);
}
