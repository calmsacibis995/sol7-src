/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)regfree.c	1.9	97/01/28 SMI"
/* LINTLIBRARY */


/*
static char sccsid[] = "@(#)69	1.3.1.2  src/bos/usr/ccs/lib/libc/regfree.c,"
" bos, bos410 1/12/93 11:18:54";
*/
/*
 * COMPONENT_NAME: (LIBCPAT) Standard C Library Pattern Functions
 *
 * FUNCTIONS: regfree
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991, 1992
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */
#pragma weak regfree = _regfree
#include "synonyms.h"

/*
 * FUNCTION:  regfree()
 *
 * DESCRIPTION: release preg memory malloc'd by regcomp()
 *              invoke appropriate method for this locale
 */

#include <sys/localedef.h>
#include <regex.h>

void
regfree(regex_t *preg)
{
	METHOD(__lc_collate, regfree)(__lc_collate, preg);
}
