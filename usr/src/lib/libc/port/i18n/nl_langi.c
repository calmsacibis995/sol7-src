/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nl_langinfo.c 1.9	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 * static char sccsid[] = "@(#)70	1.2.1.2  src/bos/usr/ccs/lib/libc/"
 * "nl_langinfo.c, bos, bos410 1/12/93 11:18:11";
 */
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: nl_langinfo
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989 , 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */
/*
 * FUNCTION: nl_langinfo
 *
 * DESCRIPTION: stub function which invokes locale specific method
 * which implements the nl_langinfo() function.
 *
 * RETURNS:
 * char * ptr to locale string.
 */

#pragma weak nl_langinfo = _nl_langinfo

#include <sys/localedef.h>

char *
_nl_langinfo(nl_item item)
{
	return (METHOD(__lc_locale, nl_langinfo)(__lc_locale, item));

}
