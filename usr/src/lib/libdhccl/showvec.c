/*
 * showvec.c: "Dump a bit vector".
 *
 * SYNOPSIS
 * void showvec(FILE *fp, const char *title, const unsigned char *u, int max)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)showvec.c 1.2 96/11/21 SMI"

#include <stdio.h>
#include "utils.h"

void
showvec(FILE *fp, const char *title, const unsigned char *u, int max)
{
	register int i;
	int first = 1;

	if (title)
		fputs(title, fp);
	if (!u)
		fputs(" (nul)", fp);
	else {
		for (i = 0; i < max; i++) {
			if ((u[(i) / 8] & (1 << ((i) % 8)) ? 1 : 0)) {
				if (!first)
					fputc(', ', fp);
				fprintf(fp, "%d", i);
				first = 0;
			}
		}
	}
}
