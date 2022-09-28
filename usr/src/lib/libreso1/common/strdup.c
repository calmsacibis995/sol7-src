/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#pragma ident   "@(#)strdup.c 1.1     97/12/03 SMI"

#include "port_before.h"

#include <stdlib.h>

#include "port_after.h"

#ifndef NEED_STRDUP
int __bind_strdup_unneeded;
#else
char *
strdup(const char *src) {
	char *dst = malloc(strlen(src) + 1);

	if (dst)
		strcpy(dst, src);
	return (dst);
}
#endif
