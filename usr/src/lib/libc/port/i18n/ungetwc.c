/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

#pragma ident	"@(#)ungetwc.c	1.20	98/01/20 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * Ungetwc saves the process code c into the one character buffer
 * associated with an input stream "iop". That character, c,
 * will be returned by the next getwc call on that stream.
 */

#pragma weak ungetwc = _ungetwc

#include "file64.h"
#include "mse_int.h"
#include <stdio.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <widec.h>
#include <limits.h>
#include <errno.h>
#include "libc.h"
#include "stdiom.h"
#include "mse.h"

wint_t
_ungetwc(wint_t wc, FILE *iop)
{
	char mbs[MB_LEN_MAX];
	unsigned char *p;
	int n;
	rmutex_t	*lk;

	if (wc == WEOF)
		return (WEOF);

	FLOCKFILE(lk, iop);
	if ((iop->_flag & _IOREAD) == 0 || iop->_ptr <= iop->_base) {
		if (iop->_base != NULL && iop->_ptr == iop->_base &&
		    iop->_cnt == 0)
			++iop->_ptr;
		else {
			FUNLOCKFILE(lk);
			return (WEOF);
		}
	}

	n = _wctomb(mbs, (wchar_t)wc);
	if (n <= 0) {
		FUNLOCKFILE(lk);
		return (WEOF);
	}
	p = (unsigned char *)(mbs+n-1); /* p points the last byte */
	while (n--) {
		*--(iop)->_ptr = (*p--);
		++(iop)->_cnt;
	}
	iop->_flag &= ~_IOEOF;
	FUNLOCKFILE(lk);
	return (wc);
}

wint_t
__ungetwc_xpg5(wint_t wc, FILE *iop)
{
	char	mbs[MB_LEN_MAX];
	unsigned char	*p;
	int	n;
	_LC_charmap_t	*lc;
	rmutex_t	*lk;

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (WEOF);
	}

	if (wc == WEOF) {
		FUNLOCKFILE(lk);
		return (WEOF);
	}
	if ((iop->_flag & _IOREAD) == 0 || iop->_ptr <= iop->_base) {
		if (iop->_base != NULL && iop->_ptr == iop->_base &&
			iop->_cnt == 0)
			++iop->_ptr;
		else {
			FUNLOCKFILE(lk);
			return (WEOF);
		}
	}
	n = METHOD(lc, wctomb)(lc, mbs, (wchar_t)wc);
	if (n <= 0) {
		FUNLOCKFILE(lk);
		return (WEOF);
	}
	p = (unsigned char *)(mbs + n - 1);
	while (n--) {
		*--(iop)->_ptr = (*p--);
		++(iop)->_cnt;
	}
	iop->_flag &= ~_IOEOF;
	FUNLOCKFILE(lk);
	return (wc);
}
