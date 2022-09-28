/*
 * convsyms.c: "Set client and class IDs using symbolic substitutions".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)convsyms.c	1.5	96/11/26 SMI"

#include <stdio.h>
#include "client.h"
#include "utils.h"
#include "hostdefs.h"
#include "camacros.h"
#include "haddr.h"
#include "catype.h"
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <ctype.h>

#define	MAXSIZE	255

int debug;
main()
{
	char *newcl;
	struct shared_bindata *newid;
	HADDR macAddr;
	char buf[MAXSIZE];
	char ifname[16];

	loglbyl();

	for (;;) {
		fputs("Enter ID:", stdout);
		if (gets(buf) == NULL)
			break;
		fputs("Enter interface name (CR for none):", stdout);
		if (gets(ifname) == NULL)
			break;
		if (ifname[0] != '\0') {
			myMacAddr(&macAddr, ifname);
			convertClient(&newid, buf, ifname, &macAddr, 0);
			YCDISP(stdout, newid->data, "New Client ID:\n",
			    newid->length);
		} else {
			convertClass(&newcl, buf);
			printf("New Class ID:\n%s\n",
			    newcl == 0 ? "<nul>" : newcl);
		}
	}
	return (0);
}
