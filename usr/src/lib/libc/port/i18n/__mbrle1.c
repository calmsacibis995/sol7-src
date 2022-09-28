/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbrlen_sb.c	1.4	97/12/07 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include <errno.h>
#include <sys/localedef.h>
#include "libc.h"
#include "mse.h"

/*
 * returns the number of characters for a SINGLE-BYTE codeset
 */
/*ARGSUSED*/	/* *handle required for interface, don't remove */
size_t
__mbrlen_sb(_LC_charmap_t *handle, const char *s, size_t len, mbstate_t *ps)
{
	MBSTATE_RESTART(ps);

	/*
	 * If length == 0 return -1
	 */
	if (len < 1) {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == (char *)NULL || *s == '\0')
		return (0);

	return (1);
}
