/*
 * tags.c: "Read and parse file describing DHCP options".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)tags.c	1.4	96/11/25 SMI"

#include "hosttype.h"
#include "hostdefs.h"
#include "dcuttxt.h"
#include "dccommon.h"
#include <string.h>
#include <stdio.h>
#include "unixgen.h"
#include "utils.h"
#include "ca_vbuf.h"

static void
dbg(const TFSTRUCT *tfp)
{
	extern const char *tsValueToString(int);
	VTSTRUCT *vtp;
	HTSTRUCT *htp;
	register int i;
	register int j;

	for (i = 0; i < tfp->ht_count; i++) {
		htp = tfp->htp+i;
		printf("%-3d  %-3u  %-8.8s  %-10.10s  %s\n", i, htp->tag,
		    htp->symbol, tsValueToString(htp->type),
		    htp->longname == 0 ? "(null)" : htp->longname);
		fflush(stdout);
	}

	putchar('\n');
	for (i = 0; i < tfp->vt_count; i++) {
		vtp = tfp->vtp+i;
		printf("\n<Vendor> = <%s>  self = %d  hightag = %d\n",
		    vtp->vendorClass ?  vtp->vendorClass : "(null)",
		    vtp->selfindex, vtp->hightag);
		for (j = 0; j <= vtp->hightag; j++)
			printf("%d -> %d\n", j, vtp->htindex[j]);
	}
}

static void
usage(const char *prog)
{
	fprintf(stderr, "Usage: %s <filename>\n", prog);
}

int
main(int argc, char *argv[])
{
	extern void scanTagFile(const char *, const char *);
	extern const TFSTRUCT *getTagInfo(void);
	const TFSTRUCT	*tfsp;

	if (argc != 2) {
		usage(argv[0]);
		return (1);
	}

	loglbyl();
	scanTagFile(argv[1], 0);
	tfsp = getTagInfo();
	dbg(tfsp);
	return (0);
}
