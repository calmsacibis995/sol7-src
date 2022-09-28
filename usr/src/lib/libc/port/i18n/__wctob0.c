/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wctob_dense.c	1.2	97/12/07 SMI"

/*LINTLIBRARY*/

#include <sys/localedef.h>

int
__wctob_dense(_LC_charmap_t * hdl, wint_t c)
{
	unsigned int wc = (unsigned int) c;

	/*
	 * If ASCII or C1 control, just store it without any
	 * conversion.
	 */
	if (wc <= 0x9f) {
		return ((unsigned int) wc);
	}

	if (wc < 256) {
		if (hdl->cm_mb_cur_max == 1) {
			return ((unsigned int) wc);
		} else {
			return (EOF);
		}
	}
	return (EOF);
}
