/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef _SYS_MSGBUF_H
#define	_SYS_MSGBUF_H

#pragma ident	"@(#)msgbuf.h	2.23	97/05/24 SMI"

#include <sys/types.h>
#include <sys/inttypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MSG_MAGIC	0x8724786

struct	msgbuf {
	struct	msgbuf_hd {
		/*
		 * These are fixed-sized types so that the 32-bit and
		 * 64-bit kernel implementations can share the same
		 * message buffer across boots.
		 */
		int32_t	msgh_magic;
		int32_t	msgh_size;
		int32_t	msgh_bufx;
		int32_t	msgh_bufr;
		/*
		 * hook to support crash dumps, libkvm...
		 * Since the msgbuf has a guaranteed fixed address
		 * in physical memory, we use it as the starting
		 * point for things that must translate virtual
		 * addresses to physical addresses.  This is needed
		 * becuase the kernel is no longer guaranteed to be
		 * loaded at physical address 0x4000.  The following
		 * is a pointer in physical memory to a structure
		 * that allows mapping of virtual to physical addresses.
		 */
		uint64_t	msgh_map;
	} msg_hd;
#define	msg_magic	msg_hd.msgh_magic
#define	msg_size	msg_hd.msgh_size
#define	msg_bufx	msg_hd.msgh_bufx
#define	msg_bufr	msg_hd.msgh_bufr
#define	msg_map		msg_hd.msgh_map

/*
 * XXX Machine-dependent.
 */
#if defined(_KERNEL) && defined(_MACHDEP)
	char	msg_bufc[MSG_BSIZE];	/* see <machine/param.h> */
#else
	char	msg_bufc[1];		/* actually longer (msg_size) */
#endif /* defined(_KERNEL) && defined(_MACHDEP) */
};
#ifdef _KERNEL
extern struct	msgbuf msgbuf;

extern void msgbuf_puts(caddr_t str);
extern size_t msgbuf_size(void);
extern caddr_t msgbuf_get(caddr_t buf, size_t bufsize);
extern void msgbuf_clear(void);
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MSGBUF_H */
