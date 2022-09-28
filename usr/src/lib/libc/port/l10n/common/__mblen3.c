/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__mblen_dense_pck.c 1.7	97/09/23  SMI"

/*
 * static char sccsid[] = "@(#)38	1.5.1.1  src/bos/usr/ccs/lib/libc/"
 * "__mblen_dense_pck.c, bos, bos410 5/25/92 13:42:52";
 */
/*
 * COMPONENT_NAME: (LIBCCCPPC) LIBC Code-Point/Process-Code Conversion Functions
 *
 * FUNCTIONS: __mblen_dense_pck
 *
 * ORIGINS: 27
 *
 * This module contains IBM CONFIDENTIAL code. -- (IBM
 * Confidential Restricted when combined with the aggregated
 * modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1991 , 1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 */

#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/localedef.h>
/*
 *  Returns the number of bytes that comprise a multi-byte string
 *  for the PCK codeset
 *
 *  |  process code   |   s[0]    |   s[1]    |
 *  +-----------------+-----------+-----------+
 *  | 0x0000 - 0x007f | 0x00-0x7f |    --     |
 *  | 0x007e - 0x00a0 |    --     |    --     |
 *  | 0x00a1 - 0x00df | 0xa1-0xdf |    --     |
 *  | 0x00e0 - 0x00ff |    --     |    --     |
 *  | 0x0100 - 0x189e | 0x81-0x9f | 0x40-0xfc (excluding 0x7f)
 *  | 0x189f - 0x303b | 0xe0-0xfc | 0xa1-0xfe (excluding 0x7f)
 *  +-----------------+-----------+-----------+
 *
 */

int
__mblen_dense_pck(_LC_charmap_t *hdl, const char *ts, size_t maxlen)
{

unsigned char *s = (unsigned char *)ts;

	/*
	 * If length == 0 return -1
	 */
	if (maxlen < 1) {
		errno = EILSEQ;
		return ((size_t)-1);
	}

	/*
	 * if s is NULL or points to a NULL return 0
	 */
	if (s == NULL || *s == '\0')
		return (0);

	/*
	 * single byte (<0x7f)
	 */
	if (s[0] <= 0x7f)
		return (1);

	/*
	 * Double byte [0x81-0x9f] [0x40-0x7e]
	 * [0x81-0x9f] [0x80-0xfc]
	 */
	else if (s[0] >= 0x81 && s[0] <= 0x9f) {
		if ((maxlen >= 2) && (s[1] >= 0x40 && s[1] <= 0xfc &&
			s[1] != 0x7f))
			return (2);
}

	/*
	 * Double byte [0xe0-0xfc] [0x40-0x7e]
	 * [0xeo-0xfc] [0x80-0xfc]
	 */
	else if (s[0] >= 0xe0 && s[0] <= 0xfc) {
		if ((maxlen >= 2) && (s[1] >= 0x40 &&
				s[1] <= 0xfc && s[1] != 0x7f))
			return (2);
	}

	/*
	 * Single Byte 0xa1 - 0xdf
	 */
	else if (s[0] >= 0xa1 && s[0] <= 0xdf) {
		return (1);
	}

	/*
	 * If we are here, then this is an invalid multi-byte character
	 */
	errno = EILSEQ;
	return (-1);
}