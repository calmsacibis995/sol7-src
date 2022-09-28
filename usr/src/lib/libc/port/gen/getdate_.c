/*
 * Copyright (c) 1987 - 1996 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)getdate_err.c	1.1	93/09/20 SMI"	/* SVr4.0 1.8	*/

#pragma weak getdate_err = _getdate_err

/*LINTLIBRARY*/
/*
#include "synonyms.h"
*/
#include <mtlib.h>
#include <time.h>
#include <thread.h>
#include <thr_int.h>
#include <libc.h>

int _getdate_err = 0;
#ifdef _REENTRANT
int *
_getdate_err_addr(void)
{
	static thread_key_t gde_key = 0;

	if (_thr_main())
		return (&_getdate_err);
	return ((int *)_tsdalloc(&gde_key, sizeof (int)));
}
#endif /* _REENTRANT */
