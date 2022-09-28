/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)__wcscoll_std.c 1.9	97/01/31 SMI"

/*LINTLIBRARY*/

/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)71	1.6.2.1  "
	"src/bos/usr/ccs/lib/libc/__wcscoll_std.c, bos, "
	"bos410 10/8/92 21:31:20";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: __wcscoll_std.c
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/types.h>
#include <sys/localedef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <alloca.h>
#include <unistd.h>

/*ARGSUSED*/
int
__wcscoll_std(_LC_collate_t *hdl, const wchar_t *wcs1, const wchar_t *wcs2)
{
	char *str1;
	char *str2;
	size_t len1;
	size_t len2;
	int rc;

	len1 = wcslen(wcs1) * __lc_collate->cmapp->cm_mb_cur_max + 1;
	if ((str1 = alloca(len1)) == NULL) {
		perror("__wcscoll_std:alloca");
		_exit(-1);
	}
	len2 = wcslen(wcs2) * __lc_collate->cmapp->cm_mb_cur_max + 1;
	if ((str2 = alloca(len2)) == NULL) {
		perror("__wcscoll_std:alloca");
		_exit(-1);
	}

	if (wcstombs(str1, wcs1, len1) == (size_t) -1)
		errno = EINVAL;
	if (wcstombs(str2, wcs2, len2) == (size_t) -1)
		errno = EINVAL;

	rc = strcoll(str1, str2);

	return (rc);
}
