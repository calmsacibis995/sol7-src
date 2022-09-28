/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#ifndef	_FILE64_H
#define	_FILE64_H

#pragma ident	"@(#)file64.h	1.9	98/02/18 SMI"

#include <thread.h>
#include <synch.h>

typedef struct {
	mutex_t	_mutex;		/* protects all the fields in this struct */
	cond_t	_cond;
	unsigned short	_wait_cnt;
	unsigned short	_lock_cnt;
	thread_t	_owner;
} rmutex_t;

#ifdef	_LP64

#ifndef	_MBSTATE_T
#define	_MBSTATE_T
typedef struct {
	long	__filler[4];
} mbstate_t;
#endif /* _MBSTATE_T */

typedef struct {
	unsigned char	*_ptr;	/* next character from/to here in buffer */
	unsigned char	*_base;	/* the buffer */
	unsigned char	*_end;	/* the end of the buffer */
	ssize_t		_cnt;	/* number of available characters in buffer */
	int		_file;	/* UNIX System file descriptor */
	unsigned int	_flag;	/* the state of the stream */
	rmutex_t	_lock;	/* lock for this structure */
	mbstate_t	*_state;	/* mbstate_t */
	char		__fill[32];	/* filler to bring size to 128 bytes */
} __FILE;

#endif	/*	_LP64	*/

#endif	/* _FILE64_H */
