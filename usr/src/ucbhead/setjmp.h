/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*******************************************************************

		PROPRIETARY NOTICE (Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice 

Notice of copyright on this source code product does not indicate 
publication.

	(c) 1986,1987,1988,1989,1997  Sun Microsystems, Inc
	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
	          All rights reserved.
********************************************************************/ 

/*
 * 4.3BSD setjmp compatibility header
 *
 * 4.3BSD setjmp/longjmp is equivalent to SVR4 sigsetjmp/siglongjmp -
 * 4.3BSD _setjmp/_longjmp is equivalent to SVR4 setjmp/longjmp
 */

#ifndef _SETJMP_H
#define _SETJMP_H

#ident	"@(#)setjmp.h	1.10	97/11/22 SMI"	/* SVr4.0 1.3	*/

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/ucontext.h>

/*
 * The sizes of the jump-buffer (_JBLEN) and the sigjump-buffer
 * (_SIGJBLEN) are defined by the appropriate, processor specific,
 * ABI.
 */
#if defined(i386) || defined(__i386)
#define	_JBLEN		10	/* ABI value */
#define	_SIGJBLEN	128	/* ABI value */
#elif defined(sparc) || defined(__sparc)
#define	_JBLEN		19	/* _SIGJBLEN */
#define	_SIGJBLEN	19	/* ABI value */
#else
#error ISA not supported
#endif

#if defined(i386) || defined(sparc) || defined(__i386) || defined(__sparc)
typedef int jmp_buf[_JBLEN];
#else
#error ISA not supported
#endif

typedef int sigjmp_buf[_SIGJBLEN];

#if defined(__STDC__)

#if __STDC__ == 0	/* non-ANSI standard compilation */
extern int	_setjmp(jmp_buf);
extern void	_longjmp(jmp_buf, int);
extern int	setjmp(jmp_buf);
extern void	longjmp(jmp_buf, int);
extern int	sigsetjmp(sigjmp_buf, int);
extern void	siglongjmp(sigjmp_buf, int);

#else
extern int	_setjmp();
extern void	_longjmp();
extern int	setjmp();
extern void	longjmp();
extern int	sigsetjmp();
extern void	siglongjmp();
#endif  /* __STDC__ */

#endif

#endif  /* _SETJMP_H */
