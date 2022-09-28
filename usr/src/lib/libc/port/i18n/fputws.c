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

#pragma ident	"@(#)fputws.c	1.18	97/12/08 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * fputws transforms the process code string pointed to by "ptr"
 * into a byte string in EUC, and writes the string to the named
 * output "iop".
 *
 * Use an intermediate buffer to transform a string from wchar_t to
 * multibyte char.  In order to not overflow the intermediate buffer,
 * impose a limit on the length of string to output to PC_MAX process
 * codes.  If the input string exceeds PC_MAX process codes, process
 * the string in a series of smaller buffers.
 */

#include "file64.h"
#include "mse_int.h"
#include <stdio.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <limits.h>
#include <widec.h>
#include <macros.h>
#include <errno.h>
#include "libc.h"
#include "stdiom.h"
#include "mse.h"

#define	PC_MAX 		256
#define	MBBUFLEN	(PC_MAX * MB_LEN_MAX)

int
fputws(const wchar_t *ptr, FILE *iop)
{
	int pcsize, ret;
	ssize_t pclen, pccnt;
	int nbytes, i;
	char mbbuf[MBBUFLEN], *mp;

	/* number of process codes in ptr */
	pclen = pccnt = wslen(ptr);

	while (pclen > 0) {
		pcsize = (int)min(pclen, PC_MAX - 1);
		nbytes = 0;
		for (i = 0, mp = mbbuf; i < pcsize; i++, mp += ret) {
			if ((ret = wctomb(mp, *ptr++)) == -1)
				return (EOF);
			nbytes += ret;
		}
		*mp = '\0';
		if (fputs(mbbuf, iop) != nbytes)
			return (EOF);
		pclen -= pcsize;
	}
	if (pccnt <= INT_MAX)
		return ((int)pccnt);
	else
		return (EOF);
}

int
__fputws_xpg5(const wchar_t *ptr, FILE *iop)
{
	int	pcsize, ret;
	ssize_t	pclen, pccnt;
	int	nbytes, i;
	char	mbbuf[MBBUFLEN], *mp;
	_LC_charmap_t	*lc;
	rmutex_t	*lk;

	FLOCKFILE(lk, iop);

	if (_set_orientation_wide(iop, &lc) == -1) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (EOF);
	}

	pclen = pccnt = wslen(ptr);
	while (pclen > 0) {
		pcsize = (int)min(pclen, PC_MAX - 1);
		nbytes = 0;
		for (i = 0, mp = mbbuf; i < pcsize; i++, mp += ret) {
			if ((ret = METHOD(lc, wctomb)(lc, mp, *ptr++))
				== -1) {
				FUNLOCKFILE(lk);
				return (EOF);
			}
			nbytes += ret;
		}
		*mp = '\0';
		/*
		 * In terms of locking, since libc is using rmutex_t
		 * for locking iop, we can call fputs() with iop that
		 * has been already locked.
		 * But again,
		 * can wide I/O functions call byte I/O functions
		 * because a steam bound to WIDE should not be used
		 * by byte I/O functions ?
		 */
		if (fputs(mbbuf, iop) != nbytes) {
			FUNLOCKFILE(lk);
			return (EOF);
		}
		pclen -= pcsize;
	}
	FUNLOCKFILE(lk);
	if (pccnt <= INT_MAX)
		return ((int)pccnt);
	else
		return (EOF);
}
