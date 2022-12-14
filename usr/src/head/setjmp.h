/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */


#ifndef _SETJMP_H
#define	_SETJMP_H

#pragma ident	"@(#)setjmp.h	1.32	98/01/30 SMI"	/* SVr4.0 1.9.2.9 */

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _JBLEN

/*
 * The sizes of the jump-buffer (_JBLEN) and the sigjump-buffer
 * (_SIGJBLEN) are defined by the appropriate, processor specific,
 * ABI.
 */
#if defined(i386) || defined(__i386)
#define	_JBLEN		10	/* ABI value */
#define	_SIGJBLEN	128	/* ABI value */
#elif defined(__sparcv9)
#define	_JBLEN		12	/* ABI value */
#define	_SIGJBLEN	19	/* ABI value */
#elif defined(sparc) || defined(__sparc)
#define	_JBLEN		12	/* ABI value */
#define	_SIGJBLEN	19	/* ABI value */
#else
#error "ISA not supported"
#endif

#if defined(i386) || defined(sparc) || defined(__i386) || defined(__sparc)
#if defined(_LP64) || defined(_I32LPx)
typedef	long	jmp_buf[_JBLEN];
#else
typedef int	jmp_buf[_JBLEN];
#endif
#else
#error "ISA not supported"
#endif

#if defined(__STDC__)
extern int setjmp(jmp_buf);
#pragma unknown_control_flow(setjmp)
extern int _setjmp(jmp_buf);
#pragma unknown_control_flow(_setjmp)
extern void longjmp(jmp_buf, int);
extern void _longjmp(jmp_buf, int);

#if __STDC__ == 0 || defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || \
	defined(__EXTENSIONS__)
/* non-ANSI standard compilation */

#if defined(_LP64) || defined(_I32LPx)
typedef long sigjmp_buf[_SIGJBLEN];
#else
typedef int sigjmp_buf[_SIGJBLEN];
#endif

extern int sigsetjmp(sigjmp_buf, int);
#pragma unknown_control_flow(sigsetjmp)
extern void siglongjmp(sigjmp_buf, int);
#endif

#if __STDC__ != 0
#define	setjmp(env)	setjmp(env)
#endif

#else

#if defined(_LP64) || defined(_I32LPx)
typedef long sigjmp_buf[_SIGJBLEN];
#else
typedef int sigjmp_buf[_SIGJBLEN];
#endif

extern int setjmp();
#pragma unknown_control_flow(setjmp)
extern int _setjmp();
#pragma unknown_control_flow(_setjmp)
extern void longjmp();
extern void _longjmp();
extern int sigsetjmp();
#pragma unknown_control_flow(sigsetjmp)
extern void siglongjmp();

#endif  /* __STDC__ */

#endif  /* _JBLEN */

#ifdef	__cplusplus
}
#endif

#endif	/* _SETJMP_H */
