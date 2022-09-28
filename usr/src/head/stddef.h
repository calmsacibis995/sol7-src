/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _STDDEF_H
#define	_STDDEF_H

#pragma ident	"@(#)stddef.h	1.15	97/12/22 SMI"	/* SVr4.0 1.5	*/

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_PTRDIFF_T
#define	_PTRDIFF_T
#if defined(_LP64) || defined(_I32LPx)
typedef	long	ptrdiff_t;		/* pointer difference */
#else
typedef int	ptrdiff_t;		/* (historical version) */
#endif
#endif	/* !_PTRDIFF_T */

#ifndef _SIZE_T
#define	_SIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef unsigned long	size_t;		/* size of something in bytes */
#else
typedef unsigned int	size_t;		/* (historical version) */
#endif
#endif	/* !_SIZE_T */

#ifndef	NULL
#if defined(_LP64) && !defined(__cplusplus)
#define	NULL	0L
#else
#define	NULL	0
#endif
#endif

#ifndef _WCHAR_T
#define	_WCHAR_T
#if defined(_LP64)
typedef	int	wchar_t;
#else
typedef long	wchar_t;
#endif
#endif	/* !_WCHAR_T */

#define	offsetof(s, m)	(size_t)(&(((s *)0)->m))

#ifdef	__cplusplus
}
#endif

#endif	/* _STDDEF_H */
