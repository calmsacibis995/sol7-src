/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef _SYS_ASYNCH_H
#define	_SYS_ASYNCH_H

#pragma ident	"@(#)asynch.h	1.12	97/08/29 SMI"

#include <sys/feature_tests.h>
#include <sys/types.h>
#include <sys/aio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	AIO_INPROGRESS	-2	/* values not set by the system */

/* large file compilation environment setup */
#if !defined(_LP64) && _FILE_OFFSET_BITS == 64
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma redefine_extname	aioread		aioread64
#pragma redefine_extname	aiowrite	aiowrite64
#else
#define	aioread			aioread64
#define	aiowrite		aiowrite64
#endif
#endif /* _FILE_OFFSET_BITS */

#if defined(_LP64) && defined(_LARGEFILE64_SOURCE)
#ifdef __PRAGMA_REDEFINE_EXTNAME
#pragma	redefine_extname	aioread64	aioread
#pragma	redefine_extname	aiowrite64	aiowrite
#else
#define	aioread64	aioread
#define	aiowrite64	aiowrite
#endif
#endif	/* _LP64 && _LARGEFILE64_SOURCE */
int	aioread(int, caddr_t, int, off_t, int, aio_result_t *);
int	aiowrite(int, caddr_t, int, off_t, int, aio_result_t *);
int	aiocancel(aio_result_t *);
aio_result_t *aiowait(struct timeval *);

/* transitional large file interfaces */
#if	defined(_LARGEFILE64_SOURCE) && !((_FILE_OFFSET_BITS == 64) && \
	    !defined(__PRAGMA_REDEFINE_EXTNAME))
int	aioread64(int, caddr_t, int, off64_t, int, aio_result_t *);
int	aiowrite64(int, caddr_t, int, off64_t, int, aio_result_t *);
#endif	/* _LARGEFILE64_SOURCE... */

#define	MAXASYNCHIO 200		/* maxi.number of outstanding i/o's */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_ASYNCH_H */
