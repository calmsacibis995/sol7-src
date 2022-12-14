/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)strcoll.c 1.11	97/01/29  SMI"

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
 *
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: strcoll.c,v $ $Revision: 1.7.2.5 "
	"$ (OSF) $Date: 1992/02/20 23:03:57 $";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: strcoll, wcscoll
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1989
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.21  com/lib/c/str/strcoll.c, libcstr, bos320,9130320 7/17/91 15:06:05
 */

#pragma weak strcoll = _strcoll

#include <sys/types.h>
#include <sys/localedef.h>
#include <string.h>
#include "libc.h"

#ifdef	PIC
int
_strcoll(const char *s1, const char *s2)
{
	return (METHOD(__lc_collate, strcoll)(__lc_collate, s1, s2));
}


int
/*ARGSUSED*/	/* *coll required by interface, don't remove */
__strcoll_C(_LC_collate_t *coll, const char *s1, const char *s2)
#else
/*ARGSUSED*/	/* all 3 args required by interface, don't remove */
int
__strcoll_C(_LC_collate_t *coll, const char *s1, const char *s2)
	{ return (0); }


int
_strcoll(const char *s1, const char *s2)
#endif	/* PIC */
{
	return (strcmp(s1, s2));
}
