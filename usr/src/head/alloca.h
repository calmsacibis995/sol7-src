/*
 * Copyright (c) 1991, 1993, 1995-1997, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef	_ALLOCA_H
#define	_ALLOCA_H

#pragma ident	"@(#)alloca.h	1.14	97/11/22 SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Many compilation systems depend upon the use of special functions
 * built into the the compilation system to handle variable argument
 * lists and stack allocations.  The method to obtain this in SunOS
 * is to define the feature test macro "__BUILTIN_VA_ARG_INCR" which
 * enables the following special built-in functions:
 *	__builtin_alloca
 *	__builtin_va_alist
 *	__builtin_va_arg_incr
 * It is intended that the compilation system define this feature test
 * macro, not the user of the system.
 *
 * The tests on the processor type are to provide a transitional period
 * for existing compilation systems, and may be removed in a future
 * release.
 */

#if defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) || defined(__sparc) || \
    defined(i386) || defined(__i386)
#define	alloca(x)	__builtin_alloca(x)

#ifdef	__STDC__
extern void *__builtin_alloca(size_t);
#else
extern void *__builtin_alloca();
#endif

#else

#ifdef	__STDC__
extern void *alloca(size_t);
#else
extern void *alloca();
#endif

#endif	/* defined(__BUILTIN_VA_ARG_INCR) || defined(sparc) ... */

#ifdef __cplusplus
}
#endif

#endif	/* _ALLOCA_H */
