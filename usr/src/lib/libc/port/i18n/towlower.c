/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)towlower.c	1.12	97/01/29 SMI"

/*LINTLIBRARY*/

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
/*
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: towlower.c,v $ "
"$Revision: 1.3.2.5 $ (OSF) $Date: 1992/02/20 23:06:32 $";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: towlower
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/towlower.c, libcchr, bos320, 9130320 7/17/91 15:16:55
 */

#include <ctype.h>
#include <wchar.h>
#include <sys/localedef.h>
#include "libc.h"

#undef towlower

/*
 * __towlower_std is strongly related to __towupper_std and __towctrans_std.
 * If updating this function, check whether you need to update other functions
 * at the same time or not.
 * Also, __towlower_std depends on the fact that the localedef command
 * generates the transformation table of toupper for the 2nd entry of
 * _LC_transnm_t and _LC_transtabs_t.
 */

#define	_TOLOWER_INDEX	2

wint_t
__towlower_std(_LC_ctype_t *hdl, wint_t wc)
{
	_LC_transtabs_t	*tabs;

	tabs = (_LC_transtabs_t *)hdl->transtabs + _TOLOWER_INDEX;
	if (wc < 0) {
		return (wc);
	} else if (wc <= tabs->tmax) {
		/* minimum value of the top sub-table of tolower is */
		/* always 0 */
		return (tabs->table[wc]);
	} else {
		tabs = tabs->next;
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

#pragma weak towlower = _towlower

wint_t
_towlower(wint_t wc)
{
	return (METHOD(__lc_ctype, towlower)(__lc_ctype, wc));
}
