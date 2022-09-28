/*
 * Copyright (c) 1993, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SYNCH_H
#define	_SYS_SYNCH_H

#pragma ident	"@(#)synch.h	1.32	98/02/19 SMI"

#include <sys/types.h>
#include <sys/int_types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Thread and LWP mutexes have the same type
 * definitions.
 */
typedef struct _lwp_mutex {
	struct _mutex_flags {
		union _flagw_un {
			uint8_t		pad[4];
			struct _flag_waiter {
				uint8_t		waiter;
				uint8_t		fpad;
				uint16_t	flag;
			} flag_waiter;
		} flagw_un;
		union _mbcp_type_un {
			uint16_t bcptype;
			struct _mtype_rcount {
				uint8_t		count_type1;
				uint8_t		count_type2;
			} mtype_rcount;
		} mbcp_type_un;
		uint16_t	magic;
	} flags;
	union _mutex_lock_un {
		struct _mutex_lock {
			uint8_t	pad[8];
		} lock64;
		upad64_t owner64;
	} lock;
	upad64_t data;
} lwp_mutex_t;

/*
 * Thread and LWP condition variables have the same
 * type definition.
 */
typedef struct _lwp_cond {
	struct _lwp_cond_flags {
		uint8_t		flag[4];
		uint16_t 	type;
		uint16_t 	magic;
	} flags;
	upad64_t data;
} lwp_cond_t;


/*
 * LWP semaphores
 */

typedef struct _lwp_sema {
	uint32_t	count;		/* semaphore count */
	uint32_t	type;
	uint8_t		flags[8];	/* last byte reserved for waiters */
	upad64_t	data;		/* optional data */
} lwp_sema_t;

/*
 * Definitions of synchronization types.
 */
#define	USYNC_THREAD	0		/* private to a process */
#define	USYNC_PROCESS	1		/* shared by processes */

/* Keep the following 3 fields in sync with pthread.h */
#define	LOCK_NORMAL	0x0		/* same as USYNC_THREAD */
#define	LOCK_ERRORCHECK	0x2		/* error check lock */
#define	LOCK_RECURSIVE	0x4		/* recussive lock */

#define	USYNC_PROCESS_ROBUST	0x8	/* shared by processes robustly */

/*
 * lwp_mutex_t flags
 */
#define	LOCK_OWNERDEAD		0x1
#define	LOCK_NOTRECOVERABLE	0x2
#define	LOCK_INITED		0x4
#define	LOCK_UNMAPPED		0x8

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_SYNCH_H */
