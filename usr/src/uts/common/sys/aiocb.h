/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_AIOCB_H
#define	_SYS_AIOCB_H

#pragma ident	"@(#)aiocb.h	1.5	97/08/29 SMI"

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/siginfo.h>
#include <sys/file.h>
#include <sys/aio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct aiocb {
	int 		aio_fildes;
#if	defined(__STDC__)
	volatile void	*aio_buf;		/* buffer location */
#else
	void		*aio_buf;		/* buffer location */
#endif
	size_t 		aio_nbytes;		/* length of transfer */
	off_t 		aio_offset;		/* file offset */
	int		aio_reqprio;		/* request priority offset */
	struct sigevent	aio_sigevent;		/* signal number and offset */
	int 		aio_lio_opcode;		/* listio operation */
	aio_result_t	aio_resultp;		/* results */
	int 		aio_state;		/* state flag for List I/O */
	int		aio__pad[1];		/* extension padding */
} aiocb_t;

#ifdef _LARGEFILE64_SOURCE
#if	!defined(_KERNEL)
typedef struct aiocb64 {
	int 		aio_fildes;
#if	defined(__STDC__)
	volatile void	*aio_buf;		/* buffer location */
#else
	void		*aio_buf;		/* buffer location */
#endif
	size_t 		aio_nbytes;		/* length of transfer */
	off64_t		aio_offset;		/* file offset */
	int		aio_reqprio;		/* request priority offset */
	struct sigevent	aio_sigevent;		/* signal number and offset */
	int 		aio_lio_opcode;		/* listio operation */
	aio_result_t	aio_resultp;		/* results */
	int 		aio_state;		/* state flag for List I/O */
	int		aio__pad[1];		/* extension padding */
} aiocb64_t;
#else
typedef struct aiocb64_32 {
	int 		aio_fildes;
	caddr32_t	aio_buf;		/* buffer location */
	uint32_t 	aio_nbytes;		/* length of transfer */
	off64_t 	aio_offset;		/* file offset */
	int		aio_reqprio;		/* request priority offset */
	struct sigevent32 aio_sigevent;		/* signal number and offset */
	int 		aio_lio_opcode;		/* listio operation */
	aio_result32_t	aio_resultp;		/* results */
	int 		aio_state;		/* state flag for List I/O */
	int		aio__pad[1];		/* extension padding */
} aiocb64_32_t;

#endif /* !defined(_KERNEL) */
#endif /* _LARGEFILE64_SOURCE */

#ifdef	_SYSCALL32
typedef struct aiocb32 {
	int 		aio_fildes;
	caddr32_t	aio_buf;		/* buffer location */
	uint32_t	aio_nbytes;		/* length of transfer */
	uint32_t	aio_offset;		/* file offset */
	int		aio_reqprio;		/* request priority offset */
	struct sigevent32 aio_sigevent;		/* signal number and offset */
	int 		aio_lio_opcode;		/* listio operation */
	aio_result32_t	aio_resultp;		/* results */
	int 		aio_state;		/* state flag for List I/O */
	int		aio__pad[1];		/* extension padding */
} aiocb32_t;

#endif /* _SYSCALL32 */
/*
 * return values for aiocancel()
 */
#define	AIO_CANCELED	0
#define	AIO_ALLDONE	1
#define	AIO_NOTCANCELED	2

/*
 * mode values for lio_listio()
 */
#define	LIO_NOWAIT	0
#define	LIO_WAIT	1


/*
 * listio operation codes
 */
#define	LIO_NOP		0
#define	LIO_READ	FREAD
#define	LIO_WRITE	FWRITE

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_AIOCB_H */
