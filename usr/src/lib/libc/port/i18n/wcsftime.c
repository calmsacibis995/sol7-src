/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wcsftime.c	1.14	97/12/06 SMI"

/*LINTLIBRARY*/

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
/*
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: wcsftime.c,v $ $Revision: 1.3.4.2 $ "
"(OSF) $Date: 1992/11/18 02:11:18 $";
#endif
 */

/*
 * COMPONENT_NAME: LIBCFMT
 *
 * FUNCTIONS:  wcsftime
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.3  com/lib/c/fmt/wcsftime.c, libcfmt, bos320, 9130320 7/17/91 15:24:18
 */

/*
 * FUNCTION: wcsftime() is a method driven function where the time formatting
 *	     processes are done in the method poninted by
 *	     __lc_time->core.wcsftime.
 *           This function behaves the same as strftime() except the
 *           ouput buffer is wchar_t. Indeed, wcsftime_std() calls strftime()
 *           which performs the conversion in single byte first. Then the
 *           output from strftime() is converted to wide character string by
 *           mbstowcs().
 *
 * PARAMETERS:
 *           const char *ws  - the output data buffer in wide character
 *                             format.
 *           size_t maxsize  - the maximum number of wide character including
 *                             the terminating null to be output to ws buffer.
 *           const char *fmt - the format string which specifies the expected
 *                             format to be output to the ws buffer.
 *           struct tm *tm   - the time structure to provide specific time
 *                             information when needed.
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - if successful, it returns the number of bytes placed into the
 *             ws buffer not including the terminating null byte.
 *           - if fail for any reason, it returns 0.
 */

#include "mse_int.h"
#include <sys/localedef.h>
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include <alloca.h>
#include "libc.h"

size_t
wcsftime(wchar_t *ws, size_t maxsize,
		const char *format, const struct tm *timeptr)
{
	return (METHOD(__lc_time, wcsftime)(__lc_time, ws, maxsize,
				format, timeptr));
}

size_t
__wcsftime_xpg5(wchar_t *ws, size_t maxsize,
		const wchar_t *format, const struct tm *timeptr)
{
	size_t	wlen, clen;
	size_t	ret;
	char	*tmp_buf;

	clen = wcstombs((char *)NULL, format, 0);
	if (clen == (size_t)-1)
		return (0);

	tmp_buf = (char *)alloca(sizeof (char) * (clen + 1));
	if (tmp_buf == NULL) {
		return (0);
	}
	wlen = wcstombs(tmp_buf, format, clen + 1);
	if (wlen == (size_t)-1) {
		return (0);
	}

	ret = wcsftime(ws, maxsize, (const char *)tmp_buf, timeptr);

	return (ret);
}



/*
 * FUNCTION: This is the standard method for function wcsftime.
 *	     This function behaves the same as strftime() except the
 *	     ouput buffer is wchar_t. Indeed, __wcsftime_std() calls strftime()
 *	     which performs the conversion in single byte first. Then the
 *	     output from strftime() is converted to wide character string by
 *	     mbstowcs().
 *
 * PARAMETERS:
 *           _LC_time_t *hdl - pointer to the handle of the LC_TIME
 *                             catagory which contains all the time related
 *                             information of the specific locale.
 *           const char *ws  - the output data buffer in wide character
 *			       format.
 *	     size_t maxsize  - the maximum number of wide character including
 *			       the terminating null to be output to ws buffer.
 *           const char *fmt - the format string which specifies the expected
 *                             format to be output to the ws buffer.
 *           struct tm *tm   - the time structure to provide specific time
 *			       information when needed.
 *
 * RETURN VALUE DESCRIPTIONS:
 *           - if successful, it returns the number of bytes placed into the
 *	       ws buffer not including the terminating null byte.
 *           - if fail for any reason, it returns 0.
 */

/*ARGSUSED*/
size_t
__wcsftime_std(_LC_time_t *hdl, wchar_t *ws, size_t maxsize,
		const char *format, const struct tm *timeptr)
{
	char	*temp;
	size_t  size;
	size_t  rc;
	size_t	wc_num;

	size = MB_CUR_MAX * maxsize;
	if ((temp = alloca(size + 1)) == NULL)
		return (0);
	rc = strftime(temp, size, format, timeptr);
	temp[rc] = '\0';
	if ((wc_num = mbstowcs(ws, temp, maxsize - 1)) == (size_t) -1) {
		return (0);
	}
	ws[wc_num] = L'\0';
	if (rc)
		return ((wc_num < maxsize) ? wc_num : 0);

	return (0);
}
