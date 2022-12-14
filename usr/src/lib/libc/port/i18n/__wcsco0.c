/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__wcscoll_bc.c 1.3	96/12/20 SMI"

/*LINTLIBRARY*/

#include <wchar.h>
#include <sys/localedef.h>
#include <stdlib.h>

int
__wcscoll_bc(_LC_collate_t *hdl, const wchar_t *ws1, const wchar_t *ws2)
{
	wchar_t *nws1, *nws2;
	wchar_t *ws, *wd, wc;
	int	result;

	if ((nws1 = malloc((wcslen(ws1) + 1) * sizeof (wchar_t))) == NULL)
		return (0);
	if ((nws2 = malloc((wcslen(ws2) + 1) * sizeof (wchar_t))) == NULL) {
		free(nws1);
		return (0);
	}
	for (ws = (wchar_t *) ws1, wd = nws1; (wc = *ws) != 0; ws++)
		*wd++ = _eucpctowc(hdl->cmapp, wc);
	*wd = wc;
	for (ws = (wchar_t *) ws2, wd = nws2; (wc = *ws) != 0; ws++)
		*wd++ = _eucpctowc(hdl->cmapp, wc);
	*wd = wc;
	result = METHOD_NATIVE(hdl, wcscoll)(hdl, (const wchar_t *) nws1,
					(const wchar_t *) nws2);
	free(nws1);
	free(nws2);
	return (result);
}
