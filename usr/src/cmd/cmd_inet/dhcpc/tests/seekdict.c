/*
 * seekdict.c: "Dictionary search for keywords".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)seekdict.c	1.4	96/11/25 SMI"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ca_dict.h"

static ca_dict table[] = {
	"ab",		1,
	"beta",		2,
	"beta",		3,
	"betadelta",	4,
	"betagamma",	5,
	"delta",	6,
	"deltaalpha",	7
};

int
main()
{
	static char *a[] = {
		"a", "bet", "beta", "betadb", "betag", "gamma", "delta",
		"deltaalpha", "phi", 0
	};
	int i, m, rc;

	puts("table is:");
	for (i = 0; i < sizeof (table) / sizeof (table[0]); i++) {
		putchar('\t');
		puts(table[i].desc);
	}
	for (i = 0; a[i]; i++) {
		rc = seekdict(a[i], table, sizeof (table) / sizeof (table[0]),
		    &m);
		printf("seekdict %s (length = %d)\n", a[i], strlen(a[i]));
		printf("\treturn_code = %d match_length = %d\n", rc, m);
	}
	return (0);
}
