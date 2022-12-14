/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _STDIO_IMPL_H
#define	_STDIO_IMPL_H

#pragma ident	"@(#)stdio_impl.h	1.6	98/02/06 SMI"

#include <sys/isa_defs.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _SSIZE_T
#define	_SSIZE_T
#if defined(_LP64) || defined(_I32LPx)
typedef long	ssize_t;		/* size of something in bytes or -1 */
#else
typedef int	ssize_t;		/* (historical version) */
#endif
#endif	/* !_SSIZE_T */

#ifdef	_LP64

#ifndef	_FILE64_H

typedef struct {
	long	__pad[16];
} __FILE;

#endif	/* _FILE64_H */

#else

typedef struct	/* needs to be binary-compatible with old versions */
{
#ifdef _STDIO_REVERSE
	unsigned char	*_ptr;	/* next character from/to here in buffer */
	ssize_t		_cnt;	/* number of available characters in buffer */
#else
	ssize_t		_cnt;	/* number of available characters in buffer */
	unsigned char	*_ptr;	/* next character from/to here in buffer */
#endif
	unsigned char	*_base;	/* the buffer */
	unsigned char	_flag;	/* the state of the stream */
	unsigned char	_file;	/* UNIX System file descriptor */
	unsigned	__orientation:2; /* the orientation of the stream */
	unsigned	__filler:6;
} __FILE;

#endif	/*	_LP64	*/

#ifdef	__cplusplus
}
#endif

#endif	/* _STDIO_IMPL_H */
