/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*LINTLIBRARY*/
#pragma	ident	"@(#)puttext.c	1.5	97/07/22 SMI"	/* SVr4.0 1.2 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include "libadm.h"

#define	MWIDTH	256
#define	WIDTH	60

int
puttext(FILE *fp, char *str, int lmarg, int rmarg)
{
	char	*copy, *lastword, *lastend, temp[MWIDTH+1];
	int	i, n, force, width, wordcnt;

	width = rmarg ? (rmarg-lmarg) : (WIDTH - lmarg);
	if (width > MWIDTH)
		width = MWIDTH;

	if (!str || !*str)
		return (width);

	if (*str == '!') {
		str++;
		force = 1;
		for (i = 0; i < lmarg; i++)
			(void) fputc(' ', fp);
	} else {
		while (isspace(*str))
			++str; /* eat leading white space */
		force = 0;
	}

	wordcnt = n = 0;
	copy = temp;
	lastword = str;
	lastend = NULL;
	do {
		if (force) {
			if (*str == '\n') {
				(void) fputc('\n', fp);
				for (i = 0; i < lmarg; i++)
					(void) fputc(' ', fp);
				str++;
				n = 0;
			} else {
				(void) fputc(*str++, fp);
				n++;
			}
			continue;
		}

		if (isspace(*str)) {
			/* eat multiple tabs/nl after whitespace */
			while ((*++str == '\t') || (*str == '\n'));
			wordcnt++;
			lastword = str;
			lastend = copy; /* end of recent word */
			*copy++ = ' ';
		} else if (*str == 0134) {
			if (str[1] == 'n') {
				wordcnt++;
				n = width;
				str += 2;
				lastword = str;
				lastend = copy; /* end of recent word */
			} else if (str[1] == 't') {
				wordcnt++;
				do {
					*copy++ = ' ';
				} while (++n % 8);
				str += 2;
				lastword = str;
				lastend = copy; /* end of recent word */
			} else if (str[1] == ' ') {
				*copy++ = ' ';
				str += 2;
			} else
				*copy++ = *str++;
		} else
			*copy++ = *str++;

		if (++n >= width) {
			if (lastend)
				*lastend = '\0';
			else
				*copy = '\0';
			for (i = 0; i < lmarg; i++)
				(void) fputc(' ', fp);
			(void) fprintf(fp, "%s\n", temp);

			lastend = NULL;
			copy = temp;
			if (wordcnt)
				/* only if word isn't bigger than the width */
				str = lastword;
			wordcnt = n = 0;
			if (!force) {
				while (isspace(*str))
					str++;
			}
		}
	} while (*str);
	if (!force) {
		*copy = '\0';
		for (i = 0; i < lmarg; i++)
			(void) fputc(' ', fp);
		(void) fprintf(fp, "%s", temp);
	}
	return (width - n - !force);
}
