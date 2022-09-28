/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wcrtomb_sb.c	1.4	97/12/07 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include <errno.h>
#include <sys/localedef.h>
#include "libc.h"
#include "mse.h"

/*ARGSUSED*/	/* hdl, ps not used here; needed for interface, don't remove */
size_t
__wcrtomb_sb(_LC_charmap_t *hdl, char *s, wchar_t pwc, mbstate_t *ps)
{
	MBSTATE_RESTART(ps);

	/*
	 * if s is NULL return 1
	 */
	if (s == (char *)NULL)
		return (1);

	if ((pwc < 0) || (pwc > 255)) {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	s[0] = (char) pwc;

	return (1);
}
