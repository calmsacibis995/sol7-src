/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)__wcsxfrm_bc.c	1.3	96/12/20 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <sys/localedef.h>
#include <stdlib.h>

size_t
__wcsxfrm_bc(_LC_collate_t *hdl, wchar_t *ws1, const wchar_t *ws2, size_t n)
{
	wchar_t *nws2;
	wchar_t *ws, *wd, wc;
	size_t  result;

	if ((nws2 = malloc((wcslen(ws2) + 1) * sizeof (wchar_t))) == NULL)
		return (0);
	for (ws = (wchar_t *) ws2, wd = nws2; (wc = *ws) != 0; ws++)
		*wd++ = _eucpctowc(hdl->cmapp, wc);
	*ws = wc;
	result = METHOD_NATIVE(hdl, wcsxfrm)(hdl, ws1,
					(const wchar_t *) nws2, n);
	free(nws2);
	return (result);
}
