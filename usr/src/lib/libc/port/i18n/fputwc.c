/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1998 Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fputwc.c	1.20	98/01/20 SMI"

/*LINTLIBRARY*/

/*
 * Fputwc transforms the wide character c into the EUC,
 * and writes it onto the output stream "iop".
 */

#pragma weak fputwc = _fputwc
#pragma weak putwc = _putwc

#include "file64.h"
#include "mse_int.h"
#include "mtlib.h"
#include <stdlib.h>
#include <sys/localedef.h>
#include <stdio.h>
#include <wchar.h>
#include <limits.h>
#include <errno.h>
#include "stdiom.h"
#include "mse.h"

wint_t
_fputwc(wint_t wc, FILE *iop)
{
	char mbs[MB_LEN_MAX];
	unsigned char *p;
	int n;
	rmutex_t	*lk;

	if (wc == WEOF)
		return (WEOF);
	n = wctomb(mbs, (wchar_t)wc);
	if (n <= 0)
		return (WEOF);
	p = (unsigned char *)mbs;
	FLOCKFILE(lk, iop);
	while (n--) {
		if (PUTC((*p++), iop) == EOF) {
			FUNLOCKFILE(lk);
			return (WEOF);
		}
	}
	FUNLOCKFILE(lk);
	return (wc);
}

wint_t
_putwc(wint_t wc, FILE *iop)
{
	return (fputwc(wc, iop));
}

wint_t
__fputwc_xpg5(wint_t wc, FILE *iop)
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
	n = METHOD(lc, wctomb)(lc, mbs, (wchar_t)wc);
	if (n <= 0) {
		FUNLOCKFILE(lk);
		return (WEOF);
	}
	p = (unsigned char *)mbs;
	while (n--) {
		/* Can wide I/O functions call byte I/O functions */
		/* because a steam bound to WIDE should not be used */
		/* by byte I/O functions ? */
		/* Anyway, I assume PUTC() macro has appropriate */
		/* definition here. */
		if (PUTC((*p++), iop) == EOF) {
			FUNLOCKFILE(lk);
			return (WEOF);
		}
	}
	FUNLOCKFILE(lk);
	return (wc);
}

wint_t
__putwc_xpg5(wint_t wc, FILE *iop)
{
	return (__fputwc_xpg5(wc, iop));
}
