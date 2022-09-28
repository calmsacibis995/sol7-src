/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)towctrans.c	1.8	97/12/06 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <wctype.h>
#include <sys/localedef.h>
#include "libc.h"

/*
 * __towctrans_std is strongly related to __towupper_std and __towlower_std.
 * If updating this function, check whether you need to update other functions
 * at the same time or not.
 */
wint_t
__towctrans_std(_LC_ctype_t *hdl, wint_t wc, wctrans_t ind)
{
	const _LC_transtabs_t	*tabs;

	if (!hdl || (wc == (wint_t)-1) || (ind == 0)) {
		return (wc);
	} else {
		tabs = hdl->transtabs + hdl->transname[ind].index;
		while (tabs) {
			if (wc >= tabs->tmin) {
				if (wc <= tabs->tmax) {
					/* wc belongs to this sub-table */
					return (tabs->table[wc - tabs->tmin]);
				} else {
					/* wc is larger than the range of */
					/* this sub-table. */
					/* so, continues to scan other tables */
					tabs = tabs->next;
				}
			} else {
				/* wc is smaller than the minimum code */
				/* of this sub-table, which means */
				/* this wc is not contained in */
				/* this transformation table */
				return (wc);
			}
		}
		return (wc);
	}
}


wint_t
towctrans(wint_t wc, wctrans_t index)
{
	return (METHOD(__lc_ctype, towctrans)(__lc_ctype, wc, index));
}
