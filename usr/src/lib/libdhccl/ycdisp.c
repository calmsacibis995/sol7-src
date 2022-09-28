/*
 * ycdisp.c : "Display a buffer in hex and character format".
 *
 * SYNOPSIS
 *    void ycdisp(FILE *f, const void *b, const char *title,
 *                int nbytes, int bytperl, int hex, int off)
 *
 * DESCRIPTION
 *    Nbytes of the data pointer to by b are displayed on the stream f. The
 *    display is preceeded by title. On each line of output bytperl bytes
 *    of the array b are written in hex. On the same line the character
 *    translation of the byte is also displayed if printable, or a blank
 *    if not printable. Each line of output is preceeded by a decimal
 *    integer (or hex integer if hex=='x') giving the offset of the first
 *    byte displayed in the line from the beginning of the array plus a
 *    constant offset, off.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)ycdisp.c 1.2 96/11/21 SMI"

#include "catype.h"
#include "utils.h"
#include <stdio.h>
#include <ctype.h>

void
ycdisp(FILE *f, const void *v, const char *title, int nbytes, int bytperl,
    int hex, int off)
{
	register int i, j;
	int ic;
	int fzero = -1;
	char *b = (char *)v;
	char *fmt;

	if (hex == 'x')
		fmt = "    %#04x:";
	else
		fmt = "    %04d:";

	if (title)
		fputs(title, f);
	if (b == 0) {
		fputs(" (null)\n", f);
		return;
	}
	for (i = 0; i < nbytes; i += bytperl) {
		j = i;
		if ((i + bytperl) < nbytes)
			for (; j < (i + bytperl) && b[j] == '\0'; j++)
				/* NULL statement */;
		if (j == (i + bytperl) && j < nbytes) {
			if (fzero < 0)
				fzero = i + off;
			continue;
		} else if (fzero >= 0) {
			fprintf(f, fmt, fzero);
			fprintf(f, " 00...\n");
			fzero = -1;
		}
		if (j != (i + bytperl) || j >= nbytes) {
			fprintf(f, fmt, i + off);
			for (j = i; j < (i + bytperl) && j < nbytes; j++)
				fprintf(f, " %02x", b[j] & 0377);
			for (; j < (i + bytperl); j++)
				fputs("   ", f);
			fputs("     ", f);
			for (j = i; j < (i + bytperl) && j < nbytes; j++) {
				ic = (int)(b[j]) & 0377;
				if (isprint(ic))
					putc(b[j], f);
				else
					putc(' ', f);
			}
			putc('\n', f);
		}
	}
}
