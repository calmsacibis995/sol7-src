/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * Copyright (c) 1988 by Nihon Sun Microsystems K.K.
 */

#pragma ident	"@(#)wsdup.c	1.7	96/12/20 SMI"

/*LINTLIBRARY*/

/*
 *	string duplication
 *	returns pointer to a new string which is the duplicate of string
 *	pointed to by s1
 *	NULL is returned if new string can't be created
 */

#include <stdlib.h>
#include <widec.h>

wchar_t *
wsdup(const wchar_t * s1)
{
	wchar_t * s2;

	s2 = malloc((wslen(s1) + 1) * sizeof (wchar_t));
	return (s2 == NULL ? NULL : wscpy(s2, s1));
}
