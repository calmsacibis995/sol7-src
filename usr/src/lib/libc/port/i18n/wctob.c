/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)wctob.c	1.1	97/11/11 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTIONS: wctob
 */

#include <stdio.h>
#include <wchar.h>
#include <sys/localedef.h>

int
wctob(wint_t c)
{
	return (METHOD(__lc_charmap, wctob)(__lc_charmap, c));
}
