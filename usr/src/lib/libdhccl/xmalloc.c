/*
 * xmalloc.c : "Get RAM and Exit with a Message if Insufficient Available."
 *
 * SYNOPSIS
 *	void *xmalloc(int nbytes)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)xmalloc.c 1.2 96/11/21 SMI"

#include "unixgen.h"
#include "utils.h"
#include "utiltxt.h"
#include <malloc.h>

void *
xmalloc(int nbytes)
{
	void *q;

	q = malloc((size_t)nbytes);
	if (!q) {
		loge(UTILMSG31, nbytes);
		exit(1);
	}
	return (q);
}
