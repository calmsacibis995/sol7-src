/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__iswctype_std.c 1.6	97/01/29 SMI"

/*LINTLIBRARY*/

/*
static char sccsid[] = "@(#)09	1.4.1.2  "
"src/bos/usr/ccs/lib/libc/__is_wctype_std.c, bos, bos410 1/12/93 11:09:28";
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: __is_wctype_std
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */
/*
 *
 * FUNCTION:  Determines if the process code, wc, has the property in mask
 *
 *
 * PARAMETERS: hdl -- The ctype info for the current locale
 *             wc  -- Process code of character to classify
 *            mask -- Mask of property to check for
 *
 *
 * RETURN VALUES: 0 -- wc does not contain the property of mask
 *              >0 -- wc has the property in mask
 *
 */

#include <sys/localedef.h>
#include "libc.h"

int
__iswctype_std(_LC_ctype_t *hdl, wint_t wc, wctype_t mask)
{

	/*
	 * if the process code is outside the bounds of the locale or
	 * if the mask is -1, then return 0
	 * since wint_t is unsigned, only check the upper bound
	 */

	if ((wc > hdl->max_wc || wc < hdl->min_wc) || (mask == -1))
		return (0);

	/*
	 * if the process code is less than 256, find the mask in the direct
	 * table, otherwise get the index
	 */

	if (wc < 256)
		return (hdl->mask[wc] & mask);
	else
		return ((hdl->qmask[hdl->qidx[wc-256]]) & mask);
}
