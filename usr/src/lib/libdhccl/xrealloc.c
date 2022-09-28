/*
 * xrealloc: "Get RAM and Exit with a Message if Insufficient Available."
 *
 * SYNOPSIS
 *	void *xrealloc(void *ptr, int nbytes)
 *
 * DESCRIPTION
 *	xrealloc() works in the same manner as the UNIX function realloc()
 *	(See UNIX manuals) but
 *	(a) It may be called with a null pointer not previously allocated
 *	(b) writes an error message and exits if it fails to get enough RAM.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)xrealloc.c 1.2 96/11/21 SMI"

#include "unixgen.h"
#include "utiltxt.h"
#include "utils.h"
#include <malloc.h>

void *
xrealloc(void *ptr, int nbytes)
{
	if (!ptr)
		ptr = malloc((size_t)nbytes);
	else
		ptr = realloc(ptr, (size_t)nbytes);
	if (!ptr) {
		loge(UTILMSG31, nbytes);
		exit(1);
	}
	return (ptr);
}
