/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)replacement.c 1.10	97/08/08  SMI"

/*LINTLIBRARY*/

/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)23	1.3.2.3  "
	"src/bos/usr/ccs/lib/libc/replacement.c, bos, bos410 1/12/93 11:19:02";
#endif
 */
/*
 *   COMPONENT_NAME: LIBCSTR
 *
 *   FUNCTIONS: do_replacement
 *
 *   ORIGINS: 27
 *
 *   (C) COPYRIGHT International Business Machines Corp. 1991,1992
 *   All Rights Reserved
 *   Licensed Materials - Property of IBM
 *   US Government Users Restricted Rights - Use, duplication or
 *   disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/types.h>
#include <sys/localedef.h>
#include <string.h>
#include <alloca.h>
#include <malloc.h>
#include "libc.h"

char *
do_replacement(_LC_collate_t *hdl, const char *str, int order, char *outstr)
{
	char *outbuf_ptr;
	char *outbuf[2];
	char *str_ptr;
	char *ptr;
	size_t space_available;
	int subs;
	unsigned char buffer;
	short order_value;

	/* Set up the pointers to the original string and the return string. */
	str_ptr = (char *)str;
	buffer = 0;
	outbuf[0] = (char *)NULL;
	outbuf[1] = (char *)NULL;

	space_available = strlen(str) * 4 * __lc_collate->cmapp->cm_mb_cur_max;

	/*
	 * For each sub string, compile the pattern and try to match it in str.
	 */
	for (subs = 0; subs < (int)hdl->co_nsubs; subs++) {
		/* Check if this sub string is used in this order. */
		order_value = hdl->co_subs[subs].ss_act[order];

		/* Determine which buffer to put the output in. */
		buffer = subs % 2;
		if (outbuf[buffer] == (char *)NULL) {
			outbuf[buffer] = (char *)alloca(space_available + 1);
			if (outbuf[buffer] == NULL) {
				perror("alloca");
				(void) strcpy(outstr, str);
				return (outstr);
			}
		}
		if (! (order_value & _SUBS_ACTIVE)) {
			str_ptr = outbuf[buffer];
			continue;
		}
		outbuf_ptr = outbuf[buffer];

		while ((ptr = strstr(str_ptr, hdl->co_subs[subs].ss_src)) !=
		    (char *)NULL) {
			(void) strncpy(outbuf_ptr, str_ptr, (ptr - str_ptr));
			outbuf_ptr += ptr - str_ptr;
			(void) strcpy(outbuf_ptr, hdl->co_subs[subs].ss_tgt);
			outbuf_ptr += strlen(hdl->co_subs[subs].ss_tgt);
			str_ptr = ptr + strlen(hdl->co_subs[subs].ss_src);
		}

		/* Put everything after the matches back in the string. */
		(void) strcpy(outbuf_ptr, str_ptr);

		/*
		 * For the next time around set str_ptr equal to the last
		 * out buffer.
		 */
		str_ptr = outbuf[buffer];
	}

	/*
	 * If the buffer is null, then none of the replacement strings
	 * were active for this order, get some space and copy the original
	 * string into it.
	 */
	if (outbuf[buffer] == (char *)NULL) {
		(void) strcpy(outstr, str);
		return (outstr);
	}

	/* Return the new string. */
	(void) strcpy(outstr, outbuf[buffer]);
	return (outstr);
}
