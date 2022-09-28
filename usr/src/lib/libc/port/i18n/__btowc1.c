/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__btowc_euc.c	1.4	97/12/07 SMI"

/*LINTLIBRARY*/

/*
 *
 * FUNCTION: __btowc_euc
 *
 */

#include "file64.h"
#include <sys/localedef.h>
#include <wchar.h>
#include <widec.h>
#include "mse.h"

wint_t
__btowc_euc(_LC_charmap_t * hdl, int c)
{
	if (isascii(c)) {
		return (c);
	}
	if (c == SS2) {
		if (hdl->cm_eucinfo->euc_bytelen2 != 0)
			return (WEOF);
	} else if (c == SS3) {
		if (hdl->cm_eucinfo->euc_bytelen3 != 0)
			return (WEOF);
	}

	/* checking C1 characters */
	if IS_C1(c) {
		return (c);
	}
	if (hdl->cm_eucinfo->euc_bytelen1 != 1)
		return (WEOF);
	else
		return (c);
}
