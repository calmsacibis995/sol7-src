/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)note.c	1.7	97/06/05 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"

/*
 * Print out a single `note' entry.
 */
void
Gelf_note_entry(Word * np)
{
	Word	namesz, descsz;
	Word	cnt;

	namesz = *np++;
	descsz = *np++;

	dbg_print(MSG_ORIG(MSG_NOT_TYPE), *np++);
	if (namesz) {
		char *	name = (char *)np;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_ORIG(MSG_NOT_NAME), name);
		name += (namesz + (sizeof (Word) - 1)) &
			~(sizeof (Word) - 1);
		/* LINTED */
		np = (Word *)name;
	}
	if (descsz) {
		for (cnt = 1; descsz; np++, cnt++, descsz -= sizeof (Word))
			dbg_print(MSG_ORIG(MSG_NOT_DESC), cnt, *np, *np);
	}
}
