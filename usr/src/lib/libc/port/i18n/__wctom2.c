/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__wctomb_sb.c 1.6	97/01/29  SMI"

/*LINTLIBRARY*/

/*
 * static char sccsid[] = "@(#)57	1.2.1.1  src/bos/usr/ccs/lib/libc/"
 * __wctomb_sb.c, bos, bos410 5/25/92 13:44:29";
 */
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __wctomb_sb
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

/*ARGSUSED*/	/* hdl not used here, but needed for interface, don't remove */
int
__wctomb_sb(_LC_charmap_t *hdl, char *s, wchar_t pwc)
{

	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == (char *)NULL)
		return (0);

	if ((pwc < 0) || (pwc > 255))
		return (-1);

	s[0] = (char) pwc;

	return (1);
}
