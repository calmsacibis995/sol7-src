/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)wcstombs.c 1.9	96/11/22  SMI"

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
static char rcsid[] = "@(#)$RCSfile: wcstombs.c,v $ "
"$Revision: 1.6.2.7 $ (OSF) $Date: 1992/03/30 02:40:59 $";
#endif
*/

/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: wcstombs
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/cppc/wcstombs.c, libccppc, 9130320 7/17/91 15:14:55
 */

#pragma weak wcstombs = _wcstombs

#include <stdlib.h>
#include <sys/localedef.h>
#include <sys/types.h>

size_t
_wcstombs(char *s, const wchar_t *pwcs, size_t n)
{
	return (METHOD(__lc_charmap, wcstombs)(__lc_charmap, s, pwcs, n));
}
