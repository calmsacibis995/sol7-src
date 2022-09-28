/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)bytetok.c	1.3	96/11/21 SMI"

#include <ctype.h>
#include <stdio.h>
#include "utils.h"

main()
	{
	static char *bytestrs[] = {
		"0", "00", "0:", "1", "1:", "01", "a", "a:", "0a", "0xa",
		"0x0a", "z", "8:", "08:", "0z08:", "8:1", "08:1", "11",
		"81", "a1", "f1", "ff", "-8", "-81", 0
	};

	register int i;
	const char *src;
	unsigned char val;
	int rc;

	for (i = 0; bytestrs[i] != 0; i++) {
		src = bytestrs[i];
		val = 0;
		rc = bytetok(&src, &val);
		printf(
"string = %-8s bytetok(&src, &val) = %-2d val = %#4x src = %c\n",
		    bytestrs[i], rc, val & 0xff, *src);
	}
	return (0);
}
