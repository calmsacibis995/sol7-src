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

#pragma ident	"@(#)fgetws.c	1.17	97/12/08 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Fgetws reads EUC characters from the "iop", converts
 * them to process codes, and places them in the wchar_t
 * array pointed to by "ptr". Fgetws reads until n-1 process
 * codes are transferred to "ptr", or EOF.
 */

#include "file64.h"
#include "mse_int.h"
#include <stdlib.h>
#include <stdio.h>
#include <widec.h>
#include <errno.h>
#include <sys/localedef.h>
#include "mtlib.h"
#include "stdiom.h"
#include "libc.h"
#include "mse.h"

#define	FGETWC _fgetwc_unlocked

wchar_t *
fgetws(wchar_t *ptr, int size, FILE *iop)
{
	wchar_t *ptr0 = ptr;
	int c;
	rmutex_t	*lk;

	FLOCKFILE(lk, iop);
	for (size--; size > 0; size--) { /* until size-1 */
		if ((c = FGETWC(iop)) == EOF) {
			if (ptr == ptr0) { /* no data */
				FUNLOCKFILE(lk);
				return (NULL);
			}
			break; /* no more data */
		}
		*ptr++ = c;
		if (c == '\n')   /* new line character */
			break;
	}
	*ptr = 0;
	FUNLOCKFILE(lk);
	return (ptr0);
}

#define	FGETWC_XPG5(X)	METHOD(lc, fgetwc)(lc, X)

wchar_t *
__fgetws_xpg5(wchar_t *ptr, int size, FILE *iop)
{
	wchar_t	*ptr0 = ptr;
	int	c;
	_LC_charmap_t	*lc;
	rmutex_t	*lk;

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (NULL);
	}

	for (size--; size > 0; size--) {
		if ((c = FGETWC_XPG5(iop)) == EOF) {
			if (ptr == ptr0) {
				FUNLOCKFILE(lk);
				return (NULL);
			}
			break;
		}
		*ptr++ = c;
		if (c == '\n')
			break;
	}
	*ptr = 0;
	FUNLOCKFILE(lk);
	return (ptr0);
}
