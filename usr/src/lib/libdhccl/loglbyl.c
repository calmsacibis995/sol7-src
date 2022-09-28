/*
 * loglbyl.c:
 * "Standard functions for unpacking error messages to stdout, stderr".
 *
 * SYNOPSIS
 *    void loglbyl()
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)loglbyl.c 1.2 96/11/20 SMI"

#include "unixgen.h"
#include "utils.h"
#include <stdio.h>
#include <stdarg.h>

void (*logb)(const char *, ...), (*loge)(const char *, ...),
	(*logl)(const char *, ...), (*logw)(const char *, ...);

static void
error(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

void
loglbyl()
{
	logb = loge = logl = logw = (void (*)(const char *, ...))error;
}
