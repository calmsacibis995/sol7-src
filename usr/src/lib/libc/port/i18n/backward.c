/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)backward.c 1.9	97/04/09 SMI"

/*LINTLIBRARY*/

/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)20	1.3.2.1  "
	"src/bos/usr/ccs/lib/libc/backward.c, bos, bos410 10/8/92 21:31:39";
#endif
*/
/*
 *   COMPONENT_NAME: LIBCSTR
 *
 *   FUNCTIONS: backward_collate_std
 *		backward_collate_sb
 *
 *   ORIGINS: 27
 *
 *
 *   (C) COPYRIGHT International Business Machines Corp. 1991,1992
 *   All Rights Reserved
 *   Licensed Materials - Property of IBM
 *   US Government Users Restricted Rights - Use, duplication or
 *   disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include "libc.h"

int
backward_collate_std(_LC_collate_t *hdl, const char *str1, const char *str2,
		int order)
{
	wchar_t *str1_colvals;
	wchar_t *str2_colvals;
	int rc;
	size_t len;
	int str1_colvals_len;
	int str2_colvals_len;
	wchar_t wc;

	/*
	 * Get the space for the collation values.  Currently there cannot be
	 * more collation values than bytes in the string.
	 */
	len = strlen(str1) * sizeof (int) + 1;
	if ((str1_colvals = alloca(len)) == NULL) {
		perror("alloca");
		exit(-1);
	}
	len = strlen(str2) * sizeof (int) + 1;
	if ((str2_colvals = alloca(len)) == NULL) {
		perror("alloca");
		exit(-1);
	}

	/* Put all of the colvals in str1_colvals and keep count. */
	str1_colvals_len = 0;
	str2_colvals_len = 0;
	while (*str1 != '\0') {
		/*
		 * Get the collating value for each character.  If it is
		 * an invalid character, assume 1 byte and go on.
		 */
		if ((rc = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
		    __lc_collate->cmapp, &wc, str1,
		    __lc_collate->cmapp->cm_mb_cur_max)) == -1) {
			errno = EINVAL;
			wc = (wchar_t)*str1++ & 0xff;
		} else
			str1 += rc;

		str1 += _getcolval(hdl, &str1_colvals[str1_colvals_len], wc,
				str1, order);
		if (str1_colvals[str1_colvals_len] != 0)
			str1_colvals_len++;
	}
	str1_colvals_len--;

	/* Do the same for str2. */
	while (*str2 != '\0') {
		/*
		 * Get the collating value for each character.  If it is
		 * an invalid character, assume 1 byte and go on.
		 */
		if ((rc = METHOD_NATIVE(__lc_collate->cmapp, mbtowc)(
		    __lc_collate->cmapp, &wc, str2,
		    __lc_collate->cmapp->cm_mb_cur_max)) == -1) {
			errno = EINVAL;
			wc = (wchar_t) *str2++ & 0xff;
		} else
			str2 += rc;

		str2 += _getcolval(hdl, &str2_colvals[str2_colvals_len], wc,
				str2, order);
		if (str2_colvals[str2_colvals_len] != 0)
			str2_colvals_len++;
	}
	str2_colvals_len--;

	/* Start at the end of both string and compare the values. */
	while ((str1_colvals_len >= 0) && (str2_colvals_len >= 0)) {
		if (str1_colvals[str1_colvals_len] <
		    str2_colvals[str2_colvals_len])
			return (-1);
		else if (str1_colvals[str1_colvals_len] >
		    str2_colvals[str2_colvals_len])
			return (1);
		str1_colvals_len--;
		str2_colvals_len--;
	}

	/*
	 * If we are here, they are equal, if str1 is longer than str2,
	 * it is greater.
	 */
	return (str1_colvals_len - str2_colvals_len);
}


int
backward_collate_sb(_LC_collate_t *hdl, const char *str1, const char *str2,
			int order)
{
	wchar_t *str1_colvals;
	wchar_t *str2_colvals;
	size_t len;
	int str1_colvals_len;
	int str2_colvals_len;
	wchar_t wc;

	/*
	 * Get the space for the collation values.  Currently there cannot be
	 * more collation values than bytes in the string.
	 */
	len = strlen(str1) * sizeof (int) + 1;
	if ((str1_colvals = alloca(len)) == NULL) {
		perror("alloca");
		exit(-1);
	}
	len = strlen(str2) * sizeof (int) + 1;
	if ((str2_colvals = alloca(len)) == NULL) {
		perror("alloca");
		exit(-1);
	}

	/* Put all of the colvals in str1_colvals and keep count. */
	str1_colvals_len = 0;
	str2_colvals_len = 0;
	while (*str1 != '\0') {
		/*
		 * Get the collating value for each character.  If it is
		 * an invalid character, assume 1 byte and go on.
		 */
		wc = (wchar_t)*str1++ & 0xff;
		str1 += _getcolval(hdl, &str1_colvals[str1_colvals_len], wc,
				str1, order);
		if (str1_colvals[str1_colvals_len] != 0)
			str1_colvals_len++;
	}
	str1_colvals_len--;

	/* Do the same for str2. */
	while (*str2 != '\0') {
		/*
		 * Get the collating value for each character.  If it is
		 * an invalid character, assume 1 byte and go on.
		 */
		wc = (wchar_t)*str2++ & 0xff;
		str2 += _getcolval(hdl, &str2_colvals[str2_colvals_len], wc,
				str2, order);
		if (str2_colvals[str2_colvals_len] != 0)
			str2_colvals_len++;
	}
	str2_colvals_len--;

	/* Start at the end of both string and compare the values. */
	while ((str1_colvals_len >= 0) && (str2_colvals_len >= 0)) {
		if (str1_colvals[str1_colvals_len] <
		    str2_colvals[str2_colvals_len])
			return (-1);
		else if (str1_colvals[str1_colvals_len] >
		    str2_colvals[str2_colvals_len])
			return (1);
		str1_colvals_len--;
		str2_colvals_len--;
	}

	/*
	 * If we are here, they are equal, if str1 is longer than str2,
	 * it is greater.
	 */
	return (str1_colvals_len - str2_colvals_len);
}
