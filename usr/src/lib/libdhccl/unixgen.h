/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_UNIXGEN_H
#define	_CA_UNIXGEN_H

#pragma ident	"@(#)unixgen.h	1.4	97/01/06 SMI"

#include <stddef.h> /* types that are system dependant e.g. size_t */
#include <unistd.h>
#include <stdlib.h> /* general external defns. like getenv() */
#include <stdio.h>
#include <errno.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern int debug;
extern int loglevel;
extern FILE *stdbug;

#define	SYSMSG strerror(errno)

/*
 * These pointers to functions are intended to make utility routines useful
 * in environments other than the standard line i/o. For instance, a
 * daemon might define loge to point to a function that ultimately calls
 * syslog. These pointers should point to functions that take printf style
 * format strings.
 */

extern void
	(*loge)(const char *, ...), /* for errors */
	(*logw)(const char *, ...), /* for warnings */
	(*logb)(const char *, ...), /* for debug */
	(*logl)(const char *, ...); /* for logging */

#ifdef	__cplusplus
}
#endif

#endif /* _CA_UNIXGEN_H */
