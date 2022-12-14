/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)isspecial.c 1.5	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isspecial
 *
 */
/*
 *
 * FUNCTION: Determines if the process code, pc, is a special character
 *
 *
 * PARAMETERS: pc  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if pc is not a special character
 *                >0 - If c is a special character
 *
 *
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>
#include <wctype.h>

#undef isspecial
#pragma weak isspecial = _isspecial

int
_isspecial(wint_t pc)
{
	if ((unsigned int)pc > 0x9f)
		return (METHOD(__lc_ctype, iswctype)(__lc_ctype, pc, _E5));
	else
		return (0);
}
