/*
 * Copyright (c) Sun Microsystems Inc. 1997
 */


#ifndef	_SYS_LCHAN_IMPL_H
#define	_SYS_LCHAN_IMPL_H

#pragma ident	"@(#)lwpchan_impl.h	1.2	98/01/14 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	LWPCHAN_BITS	10
#define	LWPCHAN_BUCKETS	(1 << LWPCHAN_BITS)
#define	LWPCHAN_HASH(addr)	((((int)addr) ^ (((int)addr)>>LWPCHAN_BITS)) & \
				    (LWPCHAN_BUCKETS - 1))

/*
 * lwpchan_entry_t translates an LWP sync object's virtual address
 * into its logical address, an LWPCHAN.
 */
typedef struct lwpchan_entry {
	caddr_t lwpchan_addr;			/* virtual address */
	uint32_t lwpchan_type;			/* type */
	quad lwpchan_lwpchan;			/* unique logical address */
	struct lwpchan_entry *lwpchan_next;	/* hash chain */
} lwpchan_entry_t;

/*
 * lwpchan_bucket_t is the first element in the hash chain. it contains
 * a mutex to protect the consistency of the hash chain, and one lwpchan
 * entry. The hash chain will grow when there are collisions.
 */
typedef struct lwpchan_hashbucket {
	kmutex_t lwpchan_lock;
	lwpchan_entry_t *lwpchan_chain;
} lwpchan_hashbucket_t;

/*
 * each process maintains a cache of LWPCHAN translations. the cache
 * is a hash list consisting of LWPCHAN_BUGKETS. each bucket is a
 * unique translation for the first set of unique sync objects. the
 * hashing functioning is very simple, and assumes an even distribution
 * of sync objects within the process's address space. new entries will be
 * kmem_alloc()'ed when conflicts occur. When the process exits, the
 * memory allocated to this cache should be freed.
 */
typedef struct lwpchan_data {
	lwpchan_hashbucket_t lwpchan_cache[LWPCHAN_BUCKETS];
}lwpchan_data_t;

/*
 * exported functions
 */
void lwpchan_delete_mapping(lwpchan_data_t *, caddr_t start, caddr_t end);
void lwpchan_destroy_cache(void);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LCHAN_IMPL_H */
