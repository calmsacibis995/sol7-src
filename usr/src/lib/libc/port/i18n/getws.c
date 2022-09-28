/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

#pragma ident	"@(#)getws.c	1.13	97/12/07 SMI" 	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Getws reads EUC characters from stdin, converts them to process
 * codes, and places them in the array pointed to by "s". Getws
 * reads until a new-line character is read or an EOF.
 */

#include "file64.h"
#include <stdio.h>
#include <stdlib.h>
#include <widec.h>
#include <sys/localedef.h>
#include "mtlib.h"
#include "stdiom.h"
#include "libc.h"

#define	FGETWC _fgetwc_unlocked

wchar_t *
getws(wchar_t *ptr)
{
	wchar_t *ptr0 = ptr;
	int c;
	rmutex_t	*lk;

	FLOCKFILE(lk, stdin);
	for (;;) {
		if ((c = FGETWC(stdin)) == EOF) {
			if (ptr == ptr0) { /* no data */
				FUNLOCKFILE(lk);
				return (NULL);
			}
			break; /* no more data */
		}
		if (c == '\n') /* new line character */
			break;
		*ptr++ = c;
	}
	*ptr = 0;
	FUNLOCKFILE(lk);
	return (ptr0);
}
