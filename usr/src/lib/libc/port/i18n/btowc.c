/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)btowc.c	1.1	97/11/11 SMI"

/*LINTLIBRARY*/

/*
 * FUNCTIONS: btowc
 */

#include <stdio.h>
#include <wchar.h>
#include <sys/localedef.h>

wint_t
btowc(int c)
{
	return (METHOD(__lc_charmap, btowc)(__lc_charmap, c));
}
