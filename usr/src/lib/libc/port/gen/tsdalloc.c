/*	Copyright (c) 1992,1996 SMI	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma  ident	"@(#)tsdalloc.c 1.12     96/11/25 SMI"

#include <sys/types.h>
#include <thread.h>
#include <synch.h>
#include <mtlib.h>
#include <errno.h>
#include "libc.h"
#include <stdlib.h>

int *
_tsdalloc(thread_key_t *key, int size)
{
	void *loc = 0;

	if (_thr_getspecific(*key, (void **)&loc) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (NULL);
		}
	}
	if (!loc) {
		if (_thr_setspecific(*key,(loc = malloc(size)))
				!= 0) {
			if (loc)
				(void) free(loc);
			return (NULL);
		}
	}
	return ((int *)loc);
}
