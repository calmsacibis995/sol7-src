/*
 * Copyright (c) 1993, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KMEM_H
#define	_SYS_KMEM_H

#pragma ident	"@(#)kmem.h	1.26	98/01/06 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Kernel memory allocator: DDI interfaces.
 * See kmem_alloc(9F) for details.
 */

#define	KM_SLEEP	0	/* can block for memory; success guaranteed */
#define	KM_NOSLEEP	1	/* cannot block for memory; may fail */

#ifdef _KERNEL

extern void *kmem_alloc(size_t size, int flags);
extern void *kmem_zalloc(size_t size, int flag);
extern void kmem_free(void *buf, size_t size);

#endif	/* _KERNEL */

/*
 * Kernel memory allocator: private interfaces.
 * These interfaces are still evolving.
 * Do not use them in unbundled drivers.
 */

/*
 * Flags for kmem_cache_create()
 */
#define	KMC_NOTOUCH	0x00010000
#define	KMC_NODEBUG	0x00020000
#define	KMC_NOMAGAZINE	0x00040000
#define	KMC_NOHASH	0x00080000

/*
 * Memory classes for kmem_backend_create()
 */
#define	KMEM_CLASS_WIRED	0	/* wired-down physical memory */
#define	KMEM_CLASS_PAGEABLE	1	/* pageable physical memory */
#define	KMEM_CLASS_OTHER	2	/* anything else, e.g. device memory */

struct kmem_cache;		/* cache structure is opaque to kmem clients */
struct kmem_backend;		/* backend structure is opaque */

#ifdef _KERNEL

extern int kmem_ready;
extern pgcnt_t kmem_reapahead;

extern void kmem_init(void);
extern void kmem_reap(void);
extern void kmem_async_thread(void);
extern pgcnt_t kmem_avail(void);
extern size_t kmem_maxavail(void);

extern struct kmem_backend *kmem_backend_create(char *,
    void *(*)(size_t, uint_t), void (*)(void *, size_t), int, int);
extern void kmem_backend_destroy(struct kmem_backend *);
extern void *kmem_backend_alloc(struct kmem_backend *, size_t, int);
extern void kmem_backend_free(struct kmem_backend *, void *, size_t);

extern struct kmem_cache *kmem_cache_create(char *, size_t, int,
	int (*)(void *, void *, int), void (*)(void *, void *),
	void (*)(void *), void *, struct kmem_backend *, int);
extern void kmem_cache_destroy(struct kmem_cache *);
extern void *kmem_cache_alloc(struct kmem_cache *, int);
extern void kmem_cache_free(struct kmem_cache *, void *);
extern ulong_t kmem_cache_stat(struct kmem_cache *, char *);

extern void *kmem_perm_alloc(size_t size, int align, int flags);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KMEM_H */
