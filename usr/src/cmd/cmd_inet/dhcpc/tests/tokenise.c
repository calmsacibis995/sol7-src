/*
 * tokeniseString: "Parse and tokenise a String".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)tokenise.c	1.4	96/11/25 SMI"

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "utils.h"

#define	MAXTOK 20

int
main()
{
	char buf[200], *p[MAXTOK + 1];
	register int i, m, n;
	int fsep, max;

	for (;;) {
		fputs("Enter separator (single character or numeric):",
		    stdout);
		if (gets(buf) == NULL)
			break;
		if (sscanf(buf, "%d", &fsep) != 1)
			fsep = buf[0];
		fputs("max number of tokens:", stdout);
		if (gets(buf) == NULL)
			break;
		if (sscanf(buf, "%d", &max) != 1) {
			puts("invalid numeric");
			continue;
		}
		if (max > MAXTOK) {
			puts("too many");
			continue;
		}
		fputs("Enter string to be parsed (e.g. xxx|yyy|zzz):", stdout);
		if (gets(buf) == NULL)
			break;
		m = countTokens(buf, fsep, max);
		n = tokeniseString(buf, p, fsep, max);
		printf("countTokens = %d tokeniseString = %d\n", m, n);
		for (i = 0; i < n; i++) {
			printf("<token[%d]> = <%s>\n", i, p[i]);
			free(p[i]);
		}
		printf("<token[%d]> = <%s>\n", n, p[n] ? p[n] : "(null)");
		free(p[n]);
		printf("<original> = <%s>\n", buf);
	}
	puts("");
	return (0);
}
