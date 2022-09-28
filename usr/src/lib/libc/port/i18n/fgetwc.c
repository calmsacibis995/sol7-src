/*
 * Copyright (c) 1995-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)fgetwc.c 1.14	98/01/20 SMI"

/*LINTLIBRARY*/

#pragma weak fgetwc = _fgetwc
#pragma weak getwc = _getwc

#include "file64.h"
#include "mse_int.h"
#include "mtlib.h"
#include <stdio.h>
#include <sys/localedef.h>
#include <widec.h>
#include <euc.h>
#include <errno.h>
#include "stdiom.h"
#include "mse.h"

#define	IS_C1(c) (((c) >= 0x80) && ((c) <= 0x9f))

#if defined(PIC)
wint_t
__fgetwc_euc(_LC_charmap_t * hdl, FILE *iop)
#else	/* !PIC */
wint_t
_fgetwc_unlocked(FILE *iop)
#endif	/* PIC */
{
	int	c, length;
	wint_t	intcode, mask;

	if ((c = GETC(iop)) == EOF)
		return (WEOF);

	if (isascii(c))		/* ASCII code */
		return ((wint_t)c);


	intcode = 0;
	if (c == SS2) {
#if defined(PIC)
		if ((length = hdl->cm_eucinfo->euc_bytelen2) == 0)
#else
		if ((length = eucw2) == 0)
#endif
			goto lab1;
		mask = WCHAR_CS2;
		goto lab2;
	} else if (c == SS3) {
#if defined(PIC)
		if ((length = hdl->cm_eucinfo->euc_bytelen3) == 0)
#else
		if ((length = eucw3) == 0)
#endif
			goto lab1;
		mask = WCHAR_CS3;
		goto lab2;
	}

lab1:
	if (IS_C1(c))
		return ((wint_t)c);
#if defined(PIC)
	length = hdl->cm_eucinfo->euc_bytelen1 - 1;
#else
	length = eucw1 - 1;
#endif
	mask = WCHAR_CS1;
	intcode = c & WCHAR_S_MASK;
lab2:
	if (length < 0)		/* codeset 1 is not defined? */
		return ((wint_t)c);
	while (length--) {
		c = GETC(iop);
		if (c == EOF || isascii(c) || (IS_C1(c))) {
			(void) UNGETC(c, iop);
			errno = EILSEQ;
			return (WEOF); /* Illegal EUC sequence. */
		}
		intcode = (intcode << WCHAR_SHIFT) | (c & WCHAR_S_MASK);
	}
	return ((wint_t)(intcode|mask));
}

#if defined(PIC)
wint_t
_fgetwc_unlocked(FILE *iop)
{
	return (METHOD(__lc_charmap, fgetwc)(__lc_charmap, iop));
}
#endif	/* PIC */

wint_t
_fgetwc(FILE *iop)
{
	rmutex_t	*lk;
	wint_t result;

	FLOCKFILE(lk, iop);
	result = _fgetwc_unlocked(iop);
	FUNLOCKFILE(lk);
	return (result);
}

wint_t
_getwc(FILE *iop)
{
	return (fgetwc(iop));
}

wint_t
__fgetwc_xpg5(FILE *iop)
{
	wint_t	result;
	_LC_charmap_t	*lc;
	rmutex_t	*lk;

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (WEOF);
	}

	result = METHOD(lc, fgetwc)(lc, iop);
	FUNLOCKFILE(lk);
	return (result);
}

wint_t
__getwc_xpg5(FILE *iop)
{
	return (__fgetwc_xpg5(iop));
}
