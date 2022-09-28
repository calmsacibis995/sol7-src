/*
 * seekdict.c: "Dictionary search for keywords".
 *
 * SYNOPSIS
 *    int seekdict(const char *, const ca_dict *, int, int *)
 *
 * DESCRIPTION
 *
 * RETURNS
 *    -1 : the keyword did not have an exact match up to the terminating
 *         null, and the span of characters that could be matched matched
 *         more than one prefix.
 *
 *    >=0  The offset into the dictionary. There are 3 possibilities for
 *         the match depending upon whether the number of matched characters
 *         is less than, equal to, or one greater than the strlen() of
 *         the search string:
 *            <     The search string did not match any keyword in its
 *                  entirety, but the span of characters that did match
 *                  only matched one unique key
 *            =     The  entire string matched a prefix of a unique key,
 *                  but the terminationating null was premature. One
 *                  could consider the search string as a unique
 *                  abbreviation of the actual key.
 *            >     The search string matched a keyword exactly, including
 *                  the final null.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)seekdict.c 1.2 96/11/21 SMI"

#include <string.h>
#include <ctype.h>
#include "ca_dict.h"

int
seekdict(const char *s, const ca_dict *t, int tablelen, int *m)
{
	register int i;
	register int j;
	int begin, end, newend;

	j = 0;
	begin = -1;
	end = tablelen - 1;

	do {
		i = newend = end;
		while (i > begin) {
			if (tolower(s[j]) > tolower(t[i].desc[j]))
				break;
			else if (tolower(s[j]) < tolower(t[i].desc[j]))
				newend--;
			i--;
		} /* loop invariant: newend >= i */

		if (newend == i) {
			if (m)
				*m = j;
			if (end > (begin + 1))
				return (-1); /* no match & ambiguous */
			return (i + 1); /* no exact match : closest match */
		} else
			end = newend;
		begin = i;
	} while (s[j++]);

	if (m)
		*m = j;
	return (i + 1);
}
