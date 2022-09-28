/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isideogram.c 1.5	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isideogram
 *
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is an ideogram character
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not an ideogram character
 *                >0 - If c is an ideogram character
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>
#include <wctype.h>

#undef isideogram
#pragma weak isideogram = _isideogram

int
_isideogram(wint_t pc)
{
	if ((unsigned int)pc > 0x9f)
		return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _E2));
	else
		return (0);
}
