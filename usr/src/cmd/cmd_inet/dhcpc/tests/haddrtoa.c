/*
 * haddrtoa.c: "Convert a hardware address to an ASCII string.".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)haddrtoa.c	1.3	96/11/21 SMI"

#include "catype.h"
#include "utils.h"
#include "haddr.h"
#include "unixgen.h"
#include <string.h>
int
main()
{
	static const char *addrstrs[] = {
		"8:0:20:8:a6:84", "08:00:20:08:a6:84",
		"08:00:20:08:0xa6:84", "08:00:20:08:a6", "08002008a684",
		"0x08002008a684", "-08002008a684",
		"8:0:20:8:16:84:", "8:0:20:8:16:84:7",
		"8:a6:84", "8:a6:84:", "8:a6:84:z", 0
	};
	static int addrlens[] = {
		MAXHADDRLEN, 6
	};
	int i, j, k;
	unsigned char p[MAXHADDRLEN];
	const char *src;

	for (i = 0; addrstrs[i]; i++) {
		for (j = 0; j < sizeof (addrlens) / sizeof (addrlens[0]); j++) {
			src = addrstrs[i];
			memset(p, 0, sizeof (p));
			k = haddrtok(&src, p, addrlens[j]);
			printf(
			    "length=%d return=%d string=%s\n\t%s\n\t*src=%c\n",
			    addrlens[j], k, addrstrs[i],
			    addrstr(p, MAXHADDRLEN, 1, '.'), *src);
		}
	}
	return (0);
}
