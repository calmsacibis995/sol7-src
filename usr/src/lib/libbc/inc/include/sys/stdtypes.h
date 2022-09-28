/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Suppose you have an ANSI C or POSIX thingy that needs a typedef
 * for thingy_t.  Put it here and include this file wherever you
 * define the thingy.  This is used so that we don't have size_t in
 * N (N > 1) different places and so that we don't have to have
 * types.h included all the time and so that we can include this in
 * the lint libs instead of termios.h which conflicts with ioctl.h.
 */

#ifndef	__SYS_STDTYPES_H
#define	__SYS_STDTYPES_H

#pragma ident	"@(#)stdtypes.h	1.8	98/01/06 SMI"

#ifdef __cplusplus
extern "C" {
#endif

typedef	int		sigset_t;	/* signal mask - may change */

typedef	unsigned int	speed_t;	/* tty speeds */
typedef	unsigned long	tcflag_t;	/* tty line disc modes */
typedef	unsigned char	cc_t;		/* tty control char */
typedef	int		pid_t;		/* process id */

typedef	unsigned short	mode_t;		/* file mode bits */
typedef	short		nlink_t;	/* links to a file */

typedef	long		clock_t;	/* units=ticks (typically 60/sec) */
typedef	long		time_t;		/* value = secs since epoch */

typedef	int		size_t;		/* ??? */
typedef int		ptrdiff_t;	/* result of subtracting two pointers */

typedef	unsigned short	wchar_t;	/* big enough for biggest char set */

/*
 * POSIX Extensions
 */
typedef	unsigned char	uchar_t;
typedef	unsigned short	ushort_t;
typedef	unsigned int	uint_t;
typedef	unsigned long	ulong_t;

#ifdef __cplusplus
}
#endif

#endif	/* __SYS_STDTYPES_H */
