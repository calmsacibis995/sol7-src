/*
 * SYNOPSIS
 *	char *xstrdup(const char *str)
 *
 * DESCRIPTION
 *	malloc space for a string and stores the string there. It exits
 *	if the malloc fails.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)xstrdup.c 1.2 96/11/21 SMI"

#include <string.h>
#include "unixgen.h"
#include "utiltxt.h"
#include "utils.h"

char *
xstrdup(const char *str)
{
	char *q;

	q = strdup(str);
	if (q == NULL) {
		loge(UTILMSG31, strlen(str) + 1);
		exit(1);
	}
	return (q);
}
