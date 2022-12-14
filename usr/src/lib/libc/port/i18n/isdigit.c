/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)isdigit.c 1.6	96/11/22  SMI"

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
static char rcsid[] = "@(#)$RCSfile: isdigit.c,v $ "
"$Revision: 1.6.2.6 $ (OSF) $Date: 1992/03/17 02:11:07 $";
#endif
*/
/*
 * COMPONENT_NAME: (LIBCCHR) LIBC Character Classification Funcions
 *
 * FUNCTIONS: isdigit
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/lib/c/chr/isdigit.c, libcchr, bos320, 9132320 7/23/91 18:17:20
 */
/*
 *
 * FUNCTION: Determines if a character is a digit
 *
 *
 * PARAMETERS: c  -- character to be classified
 *
 *
 * RETURN VALUES: 0 -- if c is not a digit
 *                >0 - If c is a digit
 *
 *
 */


#include <ctype.h>
#include <sys/localedef.h>
#include <sys/types.h>

#ifdef isdigit
#undef isdigit
#endif

int
isdigit(int c)
{

	if ((c >= 0) && (c < 256))
		return (__lc_ctype->mask[c] & _ISDIGIT);
	else
		return (0);

}
