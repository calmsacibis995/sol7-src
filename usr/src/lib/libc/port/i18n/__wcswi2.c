/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wcswidth_sb.c 1.6	97/01/29  SMI"

/*LINTLIBRARY*/

/*
 * static char sccsid[] = "@(#)54	1.2.1.2  src/bos/usr/ccs/lib/libc/
 * "__wcswidth_latin.c, bos, bos410 1/12/93 11:11:53";
*/
/*
 * COMPONENT_NAME: (LIBCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __wcswidth_sb
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991 , 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */

#include <sys/localedef.h>
#include <sys/types.h>
#include "libc.h"

/*
 * returns the number of characters for a SINGLE-BYTE codeset
 */
/*ARGSUSED*/	/* hdl not used here, but needed for interface, don't remove */
int
__wcswidth_sb(_LC_charmap_t *hdl, const wchar_t *wcs, size_t n)
{
	int dispwidth;

	/*
	 * if wcs is null or points to a null, return 0
	*/
	if (wcs == (wchar_t *)NULL || *wcs == (wchar_t) '\0')
		return (0);

	/*
	* count the number of process codes in wcs, if
	* there is a process code that is not printable, return -1
	*/
	for (dispwidth = 0; wcs[dispwidth] != (wchar_t)NULL &&
			dispwidth < n; dispwidth++)
		if (!iswprint(wcs[dispwidth]))
			return (-1);

	return (dispwidth);
}
