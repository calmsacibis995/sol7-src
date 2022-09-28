/*
 * iftoppa.c: "Convert an interface name to device, PPA and logical IP number"
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)iftoppa.c	1.4	96/11/25 SMI"

#include <ctype.h>
#include <string.h>
#include "utils.h"

int debug;
int
main()
{
	char *device;
	int ord, ppa;
	char buf[255];

	loglbyl();

	for (;;) {
		fputs("Enter Interface:", stdout);
		if (gets(buf) == NULL)
			break;
		device = 0;
		iftoppa(buf, &device, &ppa, &ord);
		printf("device=%s    ppa=%d    ord=%d\n",
		    device == 0 ? "<nul>" : device, ppa, ord);
	}
	return (0);
}
