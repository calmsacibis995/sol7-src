/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__mblen_sb.c 1.6	97/01/29  SMI"

/*LINTLIBRARY*/

/*
 * static char sccsid[] = "@(#)40 1.3.1.1  src/bos/usr/ccs/lib/libc/"
 * __mblen_sb.c, bos, bos410 5/25/92 13:42:59";
*/
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __mblen_sb
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
#include <errno.h>
#include <sys/localedef.h>
#include <sys/types.h>
#include "libc.h"

/*
 * returns the number of characters for a SINGLE-BYTE codeset
 */
/*ARGSUSED*/	/* *handle required for interface, don't remove */
int
__mblen_sb(_LC_charmap_t *handle, const char *s, size_t len)
{
	/*
	 * If length == 0 return -1
	 */
	if (len < 1) {
		errno = EILSEQ;
		return (-1);
	}

	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == (char *)NULL || *s == '\0')
		return (0);

	return (1);
}
