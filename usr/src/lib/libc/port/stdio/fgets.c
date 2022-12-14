/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)fgets.c	1.14	97/12/07 SMI"	/* SVr4.0 3.15 */

/*LINTLIBRARY*/

#include "synonyms.h"
#include "shlib.h"
#include "file64.h"
#include "mtlib.h"
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h>
#include "stdiom.h"
#include "mse.h"

/* read size-max line from stream, including '\n' */
char *
fgets(char *buf, int size, FILE *iop)
{
	char *ptr = buf;
	int n;
	Uchar *bufend;
	char *p;
	rmutex_t *lk;

	FLOCKFILE(lk, iop);

	_set_orientation_byte(iop);

	if (!(iop->_flag & (_IOREAD | _IORW))) {
		errno = EBADF;
		FUNLOCKFILE(lk);
		return (NULL);
	}

	if (iop->_base == NULL) {
		if ((bufend = _findbuf(iop)) == NULL) {
			FUNLOCKFILE(lk);
			return (NULL);
		}
	}
	else
		bufend = _bufend(iop);

	size--;		/* room for '\0' */
	while (size > 0) {
		/* empty buffer */
		if (iop->_cnt <= 0) {
			if (_filbuf(iop) != EOF) {
				iop->_ptr--;	/* put back the character */
				iop->_cnt++;
			} else if (ptr == buf) {  /* never read anything */
				FUNLOCKFILE(lk);
				return (NULL);
			} else
				break;		/* nothing left to read */
		}
		n = (int)(size < iop->_cnt ? size : iop->_cnt);
		if ((p = memccpy(ptr, (char *)iop->_ptr, '\n',
		    (size_t)n)) != NULL)
			n = (int)(p - ptr);
		ptr += n;
		iop->_cnt -= n;
		iop->_ptr += n;
		if (_needsync(iop, bufend))
			_bufsync(iop, bufend);
		if (p != NULL)
			break; /* newline found */
		size -= n;
	}
	FUNLOCKFILE(lk);
	*ptr = '\0';
	return (buf);
}
