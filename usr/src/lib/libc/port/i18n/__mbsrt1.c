/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__mbsrtowcs_euc.c	1.6	97/12/07 SMI"

/*LINTLIBRARY*/

#include "file64.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/localedef.h>
#include "mse.h"

/*ARGSUSED*/
size_t
__mbsrtowcs_euc(_LC_charmap_t * hdl, wchar_t *dst, const char **src, size_t len,
		mbstate_t *ps)
{
	size_t	val;
	size_t	i, count;
	const char *src0 = *src;

	if (dst == NULL)
		count = strlen(*src);
	else
		count = len;

	for (i = 0; i < count; i++) {
		if ((val = METHOD(hdl, mbrtowc)(hdl, dst, src0,
				MB_CUR_MAX, ps)) == (size_t)-1) {
			MBSTATE_RESTART(ps);
			return ((size_t)-1);
		}
		if (val == 0) {
			if (dst != NULL) {
				*src = NULL;
			}
			break;
		}
		/*
		 * mbsrtowcs() considers a partial multibyte character
		 * as an invalid character.
		 */
		if (val == (size_t)-2) {
			MBSTATE_RESTART(ps);
			errno = EILSEQ;
			return ((size_t)-1);
		}
		if (dst != NULL) {
			*src += val;
			dst++;
		}
		src0 += val;
	}
	MBSTATE_RESTART(ps);
	return (i);
}
