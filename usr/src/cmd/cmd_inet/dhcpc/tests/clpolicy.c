/*
 * clpolicy.c: "Read the agent configuration file and set parameters".
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)clpolicy.c	1.4	96/11/25 SMI"

#include <stdio.h>
#include "client.h"
#include "utils.h"
#include "hostdefs.h"
#include "hosttype.h"
#include "error.h"
#include "msgindex.h"
#include <alloca.h>
#include "ca_dict.h"
#include "camacros.h"
#include "ca_vbuf.h"
#include "unixgen.h"
#include <ctype.h>
#include <string.h>

int debug;

main()
{
	register int i;
	loglbyl();
	memset(&client, 0, sizeof (client));
	clpolicy(CONFIG_DIR);
	printf("arp_timeout\t\t= %d\n", client.arp_timeout);
	printf("retries\t\t\t= %d\n", client.retries);
	printf("wait_before_broadcast\t= %d\n", client.waitonboot);
	printf("lease_desired\t\t= %d\n", client.lease_desired);
	printf("timeouts\t\t=");
	for (i = 0; client.timeouts[i] > 0; i++)
		printf("%c%d", i == 0 ? ' ' : ', ', client.timeouts[i]);
	putchar('\n');
	return (0);
}
