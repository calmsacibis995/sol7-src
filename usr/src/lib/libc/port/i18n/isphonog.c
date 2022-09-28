/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isphonogram.c 1.5	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isphonogram
 *
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is a phonogram character
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not a phonogram character
 *                >0 - If c is a phonogram character
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>
#include <wctype.h>

#undef isphonogram
#pragma weak isphonogram = _isphonogram

int
_isphonogram(wint_t pc)
{
	if ((unsigned int)pc > 0x9f)
		return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _E1));
	else
		return (0);
}
