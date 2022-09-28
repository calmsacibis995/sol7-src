/*
 * inetaton.c: "Convert dotted IP notation to IP address".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)inetaton.c	1.5	96/11/25 SMI"

#include <stdio.h>
#include "catype.h"
#include <ctype.h>
#include "utils.h"
#include <netinet/in.h> /* for struct in_addr defn. */

int
main()
{
	register int i;
	union {
		struct in_addr in;
		unsigned char u[4];
	} addr;
	char buf[64];

	for (;;) {
		fputs("Enter IP address: ", stdout);
		if (gets(buf) == NULL)
			break;
		if (!inet_aton(buf, &addr.in))
			puts("\tinvalid");
		else {
			for (i = 0; i < 4; i++) {
				printf("%s%02.2x%s",
				    i == 0 ?
				    "\tResult(low addr->high addr) = " : "",
				    addr.u[i] & 0xff, i == 3 ? "\n" : "");
			}
		}
	}
	putchar('\n');
	return (0);
}
