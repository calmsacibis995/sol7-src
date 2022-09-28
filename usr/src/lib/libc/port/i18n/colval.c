/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)colval.c 1.15	97/04/09  SMI"

/*LINTLIBRARY*/

/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)68	1.4.2.2  "
	"src/bos/usr/ccs/lib/libc/colval.c, bos, bos410 1/12/93 11:12:54";

#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: _getcolval, _mbucoll
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/types.h>
#include <limits.h>
#include <sys/localedef.h>
#include <string.h>
#include <errno.h>
#include "libc.h"

/* _getcolval - determine n'th collation weight of collating symbol. */

int
_getcolval(_LC_collate_t *hdl, wchar_t *colval, wchar_t wc, const char *str,
		int order)
{
	int i;
	int count;

	/* As a safety net, just in case mbtowc() didn't catch invalid WC. */
	if (wc > hdl->co_wc_max || wc < hdl->co_wc_min) {
		errno = EINVAL;
		wc &= 0x7f;
	}

	/* Get the collation value for the wide character, wc. */
	*colval = hdl->co_coltbl[order][wc];

	/*
	 * Check if this locale has any collation element at all and if not,
	 * just return with already found collation value.
	 */
	if (hdl->co_cetbl == (const _LC_collel_t **)NULL)
		return (0);

	/*
	 * If the wide character, wc, has collation elements, try to
	 * find the matching collation value.
	 */
	if (hdl->co_cetbl[wc] != (const _LC_collel_t *)NULL) {
		i = 0;
		while (hdl->co_cetbl[wc][i].ce_sym != (const char *)NULL) {
			/*
			 * Get the length of the collation element that
			 * starts with this character.
			 */
			count = (int) strlen(hdl->co_cetbl[wc][i].ce_sym);

			/*
			 * If there is a match, get the collation elements
			 * value and return the number of characters that
			 * make up the collation value.
			 */
			if (!(strncmp(str,
			    hdl->co_cetbl[wc][i].ce_sym, count))) {
				*colval = hdl->co_cetbl[wc][i].ce_wgt[order];
				return (count);
			}

			/*
			 * This collation element did not match, go to
			 * the next.
			 */
			i++;
		}
	}

	/*
	 * No collation elements, or none that matched,
	 * return 0 additional characters.
	 */
	return (0);
}


/* _mbucoll - determine unique collating weight of collating symbol. */

wchar_t
_mbucoll(_LC_collate_t *hdl, char *str, char **next_char)
{
	wchar_t ucoll;	/* collating symbol unique weight	*/
	int wclen;	/* # bytes in first character		*/
	wchar_t wc;	/* first character process code		*/

	wclen = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(__lc_collate->cmapp,
			&wc, str, __lc_collate->cmapp->cm_mb_cur_max);
	if (wclen < 0)
		wc = *str++ & 0xff;
	else
		str += wclen;

	*next_char = str + _getcolval(hdl, &ucoll, wc, str, (int)hdl->co_nord);

	return (ucoll);
}
