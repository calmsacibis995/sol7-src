/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stream.c	1.120	98/02/01 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/thread.h>
#include <sys/sysmacros.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/conf.h>
#include <sys/var.h>
#include <sys/debug.h>
#include <sys/inline.h>
#include <sys/tuneable.h>
#include <sys/map.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/vtrace.h>

/*
 * This file contains all the STREAMS utility routines that may
 * be used by modules and drivers.
 */

/*
 * STREAMS message allocator: principles of operation
 *
 * The streams message allocator consists of all the routines that
 * allocate, dup and free streams messages: allocb(), [d]esballoc[a],
 * dupb(), freeb() and freemsg().  What follows is a high-level view
 * of how the allocator works.
 *
 * Every streams message consists of one or more mblks, a dblk, and data.
 * All mblks for all types of messages come from a common mblk_cache.
 * The dblk and data come in several flavors, depending on how the
 * message is allocated:
 *
 * (1) Small messages are allocated from a collection of fixed-size
 *     dblk/data caches.  These caches cover all message sizes up to
 *     DBLK_MAX_CACHE.  Objects in these caches consist of a dblk
 *     plus its associated data, allocated as a single contiguous
 *     chunk of memory.  allocb() determines the nearest-size cache
 *     by table lookup: the dblk_cache[] array provides the mapping
 *     from size to dblk cache.
 *
 * (2) Large messages (size > DBLK_MAX_CACHE) are constructed by
 *     kmem_alloc()'ing a buffer for the data and supplying that
 *     buffer to gesballoc(), described below.
 *
 * (3) The four flavors of [d]esballoc[a] are all implemented by a
 *     common routine, gesballoc() ("generic esballoc").  gesballoc()
 *     allocates a dblk from the global dblk_esb_cache and sets db_base,
 *     db_lim and db_frtnp to describe the caller-supplied buffer.
 *
 * While there are several routines to allocate messages, there is only
 * one routine to free messages: freeb().  freeb() simply invokes the
 * dblk's free method, dbp->db_free(), which is set at allocation time.
 *
 * dupb() creates a new reference to a message by allocating a new mblk,
 * incrementing the dblk reference count and setting the dblk's free
 * method to dblk_decref().  The dblk's original free method is retained
 * in db_lastfree.  dblk_decref() decrements the reference count on each
 * freeb().  If this is not the last reference it just frees the mblk;
 * if this *is* the last reference, it restores db_free to db_lastfree,
 * sets db_mblk to the current mblk (see below), and invokes db_lastfree.
 *
 * The implementation makes aggressive use of kmem object caching for
 * maximum performance.  This makes the code simple and compact, but
 * also a bit abstruse in some places.  The invariants that constitute a
 * message's constructed state, described below, are more subtle than usual.
 *
 * Every dblk has an "attached mblk" as part of its constructed state.
 * The mblk is allocated by the dblk's constructor and remains attached
 * until the message is either dup'ed or pulled up.  In the dupb() case
 * the mblk association doesn't matter until the last free, at which time
 * dblk_decref() attaches the last mblk to the dblk.  pullupmsg() affects
 * the mblk association because it swaps the leading mblks of two messages,
 * so it is responsible for swapping their db_mblk pointers accordingly.
 * From a constructed-state viewpoint it doesn't matter that a dblk's
 * attached mblk can change while the message is allocated; all that
 * matters is that the dblk has *some* attached mblk when it's freed.
 *
 * The sizes of the allocb() small-message caches are not magical.
 * They represent a good trade-off between internal and external
 * fragmentation for current workloads.  They should be reevaluated
 * periodically, especially if allocations larger than DBLK_MAX_CACHE
 * become common.  We use 32-byte alignment so that dblks don't
 * straddle cache lines unnecessarily.
 */
#define	DBLK_MAX_CACHE		12288
#define	DBLK_ALIGN		32
#define	DBLK_ALIGN_SHIFT	5

#ifdef _BIG_ENDIAN
#define	DBLK_RTFU_SHIFT(field)	\
	(8 * (&((dblk_t *)0)->db_struioflag - &((dblk_t *)0)->field))
#else
#define	DBLK_RTFU_SHIFT(field)	\
	(8 * (&((dblk_t *)0)->field - &((dblk_t *)0)->db_ref))
#endif

#define	DBLK_RTFU(ref, type, flags, uioflag)	\
	(((ref) << DBLK_RTFU_SHIFT(db_ref)) | \
	((type) << DBLK_RTFU_SHIFT(db_type)) | \
	(((flags) | (ref - 1)) << DBLK_RTFU_SHIFT(db_flags)) | \
	((uioflag) << DBLK_RTFU_SHIFT(db_struioflag)))
#define	DBLK_RTFU_REF_MASK	(DBLK_REFMAX << DBLK_RTFU_SHIFT(db_ref))
#define	DBLK_RTFU_WORD(dbp)	(*((uint32_t *)&(dbp)->db_ref))
#define	MBLK_BAND_FLAG_WORD(mp)	(*((uint32_t *)&(mp)->b_band))

static size_t dblk_sizes[] = {
#ifdef _LP64
	128, 160, 192, 224, 256,
#else
	96, 128, 192,
#endif
	384, 672, 1152, 1632, 2048, 2720, 4096, 8192, DBLK_MAX_CACHE, 0
};

static struct kmem_cache *dblk_cache[DBLK_MAX_CACHE / DBLK_ALIGN];
static struct kmem_cache *mblk_cache;
static struct kmem_cache *dblk_esb_cache;

static void dblk_lastfree(mblk_t *mp, dblk_t *dbp);
static mblk_t *allocb_oversize(size_t size, int flags);
static int allocb_tryhard_fails;
static void frnop_func(void *arg);
frtn_t frnop = { frnop_func };

static int
dblk_constructor(void *buf, void *cdrarg, int kmflags)
{
	dblk_t *dbp = buf;
	ssize_t msg_size = (ssize_t)cdrarg;
	int32_t index = (int32_t)(sizeof (dblk_t) - 1 + msg_size) >>
			DBLK_ALIGN_SHIFT;

	if ((dbp->db_mblk = kmem_cache_alloc(mblk_cache, kmflags)) == NULL)
		return (-1);
	dbp->db_mblk->b_datap = dbp;
	dbp->db_cache = dblk_cache[index];
	dbp->db_base = (unsigned char *)&dbp[1];
	dbp->db_lim = ((unsigned char *)&dbp[1]) + msg_size;
	dbp->db_free = dbp->db_lastfree = dblk_lastfree;
	dbp->db_frtnp = NULL;
	return (0);
}

/*ARGSUSED*/
static int
dblk_esb_constructor(void *buf, void *cdrarg, int kmflags)
{
	dblk_t *dbp = buf;

	if ((dbp->db_mblk = kmem_cache_alloc(mblk_cache, kmflags)) == NULL)
		return (-1);
	dbp->db_mblk->b_datap = dbp;
	dbp->db_cache = dblk_esb_cache;
	return (0);
}

/*ARGSUSED*/
static void
dblk_destructor(void *buf, void *cdrarg)
{
	dblk_t *dbp = buf;

	ASSERT(dbp->db_mblk->b_datap == dbp);
	kmem_cache_free(mblk_cache, dbp->db_mblk);
}

void
mhinit()
{
	char name[40];
	size_t size;
	size_t lastsize = DBLK_ALIGN;
	size_t *sizep;
	struct kmem_cache *cp;

	mblk_cache = kmem_cache_create("streams_mblk",
		sizeof (mblk_t), 32, NULL, NULL, NULL, NULL, NULL, 0);

	for (sizep = dblk_sizes; (size = *sizep) != 0; sizep++) {
		ASSERT((size & (DBLK_ALIGN - 1)) == 0);
		(void) sprintf(name, "streams_dblk_%ld",
		    size - sizeof (dblk_t));
		cp = kmem_cache_create(name, size, DBLK_ALIGN,
			dblk_constructor, dblk_destructor, NULL,
			(void *)(size - sizeof (dblk_t)), NULL, 0);
		while (lastsize <= size) {
			dblk_cache[(lastsize - 1) >> DBLK_ALIGN_SHIFT] = cp;
			lastsize += DBLK_ALIGN;
		}
	}

	dblk_esb_cache = kmem_cache_create("streams_dblk_esb",
		sizeof (dblk_t), DBLK_ALIGN,
		dblk_esb_constructor, dblk_destructor, NULL, NULL, NULL, 0);
}

/*ARGSUSED*/
mblk_t *
allocb(size_t size, uint pri)
{
	dblk_t *dbp;
	mblk_t *mp;
	int32_t index = (int32_t)(sizeof (dblk_t) - 1 + size) >>
			DBLK_ALIGN_SHIFT;

	if (index >= (DBLK_MAX_CACHE >> DBLK_ALIGN_SHIFT))
		return (allocb_oversize(size, KM_NOSLEEP));

	if ((dbp = kmem_cache_alloc(dblk_cache[index], KM_NOSLEEP)) == NULL)
		return (NULL);

	mp = dbp->db_mblk;
	DBLK_RTFU_WORD(dbp) = DBLK_RTFU(1, M_DATA, 0, 0);
	mp->b_next = mp->b_prev = mp->b_cont = NULL;
	mp->b_rptr = mp->b_wptr = (unsigned char *)&dbp[1];
	MBLK_BAND_FLAG_WORD(mp) = 0;

	return (mp);
}

void
freeb(mblk_t *mp)
{
	dblk_t *dbp = mp->b_datap;

	ASSERT(dbp->db_ref > 0);
	ASSERT(mp->b_next == NULL && mp->b_prev == NULL);
	dbp->db_free(mp, dbp);
}

void
freemsg(mblk_t *mp)
{
	while (mp) {
		dblk_t *dbp = mp->b_datap;
		mblk_t *mp_cont = mp->b_cont;

		ASSERT(dbp->db_ref > 0);
		ASSERT(mp->b_next == NULL && mp->b_prev == NULL);
		dbp->db_free(mp, dbp);
		mp = mp_cont;
	}
}

/*
 * Reallocate a block for another use.  Try hard to use the old block.
 * If the old data is wanted (copy), leave b_wptr at the end of the data,
 * otherwise return b_wptr = b_rptr.
 *
 * This routine is private and unstable.
 */
mblk_t	*
reallocb(mblk_t *mp, size_t size, uint copy)
{
	mblk_t		*mp1;
	unsigned char	*old_rptr;
	ptrdiff_t	cur_size;

	if (mp == NULL)
		return (allocb(size, BPRI_HI));

	cur_size = mp->b_wptr - mp->b_rptr;
	old_rptr = mp->b_rptr;

	ASSERT(mp->b_datap->db_ref != 0);

	if (mp->b_datap->db_ref == 1 &&
	    mp->b_datap->db_lim - mp->b_datap->db_base >= size) {
		/*
		 * If the data is wanted and it will fit where it is, no
		 * work is required.
		 */
		if (copy && mp->b_datap->db_lim - mp->b_rptr >= size)
			return (mp);

		mp->b_wptr = mp->b_rptr = mp->b_datap->db_base;
		mp1 = mp;
	} else if (mp1 = allocb(size, BPRI_HI)) {
		/* XXX other mp state could be copied too, db_flags ... ? */
		mp1->b_datap->db_type = mp->b_datap->db_type;
		mp1->b_cont = mp->b_cont;
	} else
		return (NULL);

	if (copy) {
		bcopy(old_rptr, mp1->b_rptr, cur_size);
		mp1->b_wptr = mp1->b_rptr + cur_size;
	}

	if (mp != mp1)
		freeb(mp);

	return (mp1);
}

/*ARGSUSED*/
static void
dblk_lastfree(mblk_t *mp, dblk_t *dbp)
{
	ASSERT(dbp->db_mblk == mp);
	kmem_cache_free(dbp->db_cache, dbp);
}

static void
dblk_decref(mblk_t *mp, dblk_t *dbp)
{
	if (dbp->db_ref != 1) {
		uint32_t rtfu = atomic_add_32_nv(&DBLK_RTFU_WORD(dbp),
		    -(1 << DBLK_RTFU_SHIFT(db_ref)));
		/*
		 * atomic_add_32_nv() just decremented db_ref, so we no longer
		 * have a reference to the dblk, which means another thread
		 * could free it.  Therefore we cannot examine the dblk to
		 * determine whether ours was the last reference.  Instead,
		 * we extract the new and minimum reference counts from rtfu.
		 * Note that all we're really saying is "if (ref != refmin)".
		 */
		if (((rtfu >> DBLK_RTFU_SHIFT(db_ref)) & DBLK_REFMAX) !=
		    ((rtfu >> DBLK_RTFU_SHIFT(db_flags)) & DBLK_REFMIN)) {
			kmem_cache_free(mblk_cache, mp);
			return;
		}
	}
	dbp->db_mblk = mp;
	dbp->db_free = dbp->db_lastfree;
	dbp->db_lastfree(mp, dbp);
}

mblk_t *
dupb(mblk_t *mp)
{
	dblk_t *dbp = mp->b_datap;
	mblk_t *new_mp;
	uint32_t oldrtfu, newrtfu;

	if ((new_mp = kmem_cache_alloc(mblk_cache, KM_NOSLEEP)) == NULL)
		return (NULL);

	new_mp->b_next = new_mp->b_prev = new_mp->b_cont = NULL;
	new_mp->b_rptr = mp->b_rptr;
	new_mp->b_wptr = mp->b_wptr;
	new_mp->b_datap = dbp;
	MBLK_BAND_FLAG_WORD(new_mp) = MBLK_BAND_FLAG_WORD(mp);

	/*
	 * First-dup optimization.  The enabling assumption is that there
	 * can can never be a race (in correct code) to dup the first copy
	 * of a message.  Therefore we don't need to do it atomically.
	 */
	if (dbp->db_free != dblk_decref) {
		dbp->db_free = dblk_decref;
		dbp->db_ref++;
		return (new_mp);
	}

	do {
		ASSERT(dbp->db_ref > 0);
		oldrtfu = DBLK_RTFU_WORD(dbp);
		newrtfu = oldrtfu + (1 << DBLK_RTFU_SHIFT(db_ref));
		/*
		 * If db_ref is maxed out we can't dup this message anymore.
		 */
		if ((oldrtfu & DBLK_RTFU_REF_MASK) == DBLK_RTFU_REF_MASK) {
			kmem_cache_free(mblk_cache, new_mp);
			return (NULL);
		}
	} while (cas32(&DBLK_RTFU_WORD(dbp), oldrtfu, newrtfu) != oldrtfu);

	return (new_mp);
}

/*ARGSUSED*/
static void
dblk_lastfree_desb(mblk_t *mp, dblk_t *dbp)
{
	frtn_t *frp = dbp->db_frtnp;

	ASSERT(dbp->db_mblk == mp);
	frp->free_func(frp->free_arg);
	kmem_cache_free(dbp->db_cache, dbp);
}

/*ARGSUSED*/
static void
frnop_func(void *arg)
{
}

/*
 * Generic esballoc used to implement the four flavors: [d]esballoc[a].
 */
static mblk_t *
gesballoc(unsigned char *base, size_t size, uint32_t db_rtfu, frtn_t *frp,
	void (*lastfree)(mblk_t *, dblk_t *), int kmflags)
{
	dblk_t *dbp;
	mblk_t *mp;

	ASSERT(base != NULL && frp != NULL);

	if ((dbp = kmem_cache_alloc(dblk_esb_cache, kmflags)) == NULL)
		return (NULL);

	mp = dbp->db_mblk;
	dbp->db_base = base;
	dbp->db_lim = base + size;
	dbp->db_free = dbp->db_lastfree = lastfree;
	dbp->db_frtnp = frp;
	DBLK_RTFU_WORD(dbp) = db_rtfu;
	mp->b_next = mp->b_prev = mp->b_cont = NULL;
	mp->b_rptr = mp->b_wptr = base;
	MBLK_BAND_FLAG_WORD(mp) = 0;

	return (mp);
}

/*ARGSUSED*/
mblk_t *
esballoc(unsigned char *base, size_t size, uint pri, frtn_t *frp)
{
	return (gesballoc(base, size, DBLK_RTFU(1, M_DATA, 0, 0),
		frp, freebs_enqueue, KM_NOSLEEP));
}

/*ARGSUSED*/
mblk_t *
desballoc(unsigned char *base, size_t size, uint pri, frtn_t *frp)
{
	return (gesballoc(base, size, DBLK_RTFU(1, M_DATA, 0, 0),
		frp, dblk_lastfree_desb, KM_NOSLEEP));
}

/*ARGSUSED*/
mblk_t *
esballoca(unsigned char *base, size_t size, uint pri, frtn_t *frp)
{
	return (gesballoc(base, size, DBLK_RTFU(2, M_DATA, 0, 0),
		frp, freebs_enqueue, KM_NOSLEEP));
}

/*ARGSUSED*/
mblk_t *
desballoca(unsigned char *base, size_t size, uint pri, frtn_t *frp)
{
	return (gesballoc(base, size, DBLK_RTFU(2, M_DATA, 0, 0),
		frp, dblk_lastfree_desb, KM_NOSLEEP));
}

/*ARGSUSED*/
static void
dblk_lastfree_oversize(mblk_t *mp, dblk_t *dbp)
{
	ASSERT(dbp->db_mblk == mp);
	kmem_free(dbp->db_base, dbp->db_lim - dbp->db_base);
	kmem_cache_free(dbp->db_cache, dbp);
}

static mblk_t *
allocb_oversize(size_t size, int kmflags)
{
	mblk_t *mp;
	void *buf;

	size = (size + (DBLK_ALIGN - 1)) & -DBLK_ALIGN;
	if ((buf = kmem_alloc(size, kmflags)) == NULL)
		return (NULL);
	if ((mp = gesballoc(buf, size, DBLK_RTFU(1, M_DATA, 0, 0),
	    &frnop, dblk_lastfree_oversize, kmflags)) == NULL)
		kmem_free(buf, size);
	return (mp);
}

static mblk_t *
allocb_tryhard(size_t target_size)
{
	size_t size;
	mblk_t *bp;

	for (size = target_size; size < target_size + 256; size += DBLK_ALIGN)
		if ((bp = allocb(size, BPRI_HI)) != NULL)
			return (bp);
	allocb_tryhard_fails++;
	return (NULL);
}

/*
 * This routine is consolidation private for STREAMS internal use
 * This routine may only be called from sync routines (i.e., not
 * from put or service procedures).  It is located here (rather
 * than strsubr.c) so that we don't have to expose all of the
 * allocb() implementation details in header files.
 */
mblk_t *
allocb_wait(size_t size, uint pri, uint flags, int *error)
{
	dblk_t *dbp;
	mblk_t *mp;
	int32_t index = (int32_t)(sizeof (dblk_t) - 1 + size) >>
		DBLK_ALIGN_SHIFT;

	if (flags & STR_NOSIG) {
		if (index >= (DBLK_MAX_CACHE >> DBLK_ALIGN_SHIFT))
			return (allocb_oversize(size, KM_SLEEP));
		dbp = kmem_cache_alloc(dblk_cache[index], KM_SLEEP);
		mp = dbp->db_mblk;
		DBLK_RTFU_WORD(dbp) = DBLK_RTFU(1, M_DATA, 0, 0);
		mp->b_next = mp->b_prev = mp->b_cont = NULL;
		mp->b_rptr = mp->b_wptr = (unsigned char *)&dbp[1];
		MBLK_BAND_FLAG_WORD(mp) = 0;
	} else {
		while ((mp = allocb(size, pri)) == NULL) {
			if ((*error = strwaitbuf(size, BPRI_HI)) != 0)
				return (NULL);
		}
	}
	return (mp);
}

/*
 * Call function 'func' with 'arg' when a class zero block can
 * be allocated with priority 'pri'.
 */
/* ARGSUSED */
bufcall_id_t
esbbcall(uint pri, void (*func)(void *), void *arg)
{
	return (bufcall(1, pri, func, arg));
}

/*
 * Allocates an iocblk (M_IOCTL) block. Properly sets the credentials
 * ioc_id, rval and error of the struct ioctl to set up an ioctl call.
 * This provides consistency for all internal allocators of ioctl.
 */
mblk_t *
mkiocb(uint cmd)
{
	struct iocblk	*ioc;
	mblk_t		*mp;

	/*
	 * Allocate enough space for any of the ioctl related messages.
	 */
	if ((mp = allocb(sizeof (union ioctypes), BPRI_MED)) == NULL)
		return (NULL);

	bzero(mp->b_rptr, sizeof (union ioctypes));

	/*
	 * Set the mblk_t information and ptrs correctly.
	 */
	mp->b_wptr += sizeof (struct iocblk);
	mp->b_datap->db_type = M_IOCTL;

	/*
	 * Fill in the fields.
	 */
	ioc		= (struct iocblk *)mp->b_rptr;
	ioc->ioc_cmd	= cmd;
	ioc->ioc_cr	= kcred;
	ioc->ioc_id	= getiocseqno();
	ioc->ioc_flag	= IOC_NATIVE;
	return (mp);
}

/*
 * test if block of given size can be allocated with a request of
 * the given priority.
 * 'pri' is no longer used, but is retained for compatibility.
 */
/* ARGSUSED */
int
testb(size_t size, uint pri)
{
	return ((size + sizeof (dblk_t)) <= kmem_avail());
}

/*
 * Call function 'func' with argument 'arg' when there is a reasonably
 * good chance that a block of size 'size' can be allocated.
 * 'pri' is no longer used, but is retained for compatibility.
 */
/* ARGSUSED */
bufcall_id_t
bufcall(size_t size, uint pri, void (*func)(void *), void *arg)
{
	static long bid = 1;	/* always odd to save checking for zero */
	struct strbufcall *bcp;

	if ((bcp = kmem_cache_alloc(bufcall_cache, KM_NOSLEEP)) == NULL)
		return (0);

	bcp->bc_func = func;
	bcp->bc_arg = arg;
	bcp->bc_size = size;
	bcp->bc_next = NULL;
	bcp->bc_executor = NULL;

	mutex_enter(&service_queue);
	bcp->bc_id = (bufcall_id_t)(bid += 2);	/* keep it odd (see above) */

	/*
	 * add newly allocated stream event to existing
	 * linked list of events.
	 */
	if (strbcalls.bc_head == NULL) {
		strbcalls.bc_head = strbcalls.bc_tail = bcp;
	} else {
		strbcalls.bc_tail->bc_next = bcp;
		strbcalls.bc_tail = bcp;
	}

	mutex_exit(&service_queue);
	return (bcp->bc_id);
}

/*
 * Cancel a bufcall request.
 */
void
unbufcall(bufcall_id_t id)
{
	strbufcall_t *bcp, *pbcp;

	mutex_enter(&service_queue);
again:
	pbcp = NULL;
	for (bcp = strbcalls.bc_head; bcp; bcp = bcp->bc_next) {
		if (id == bcp->bc_id)
			break;
		pbcp = bcp;
	}
	if (bcp) {
		if (bcp->bc_executor != NULL) {
			if (bcp->bc_executor != curthread) {
				cv_wait(&bcall_cv, &service_queue);
				goto again;
			}
		} else {
			if (pbcp)
				pbcp->bc_next = bcp->bc_next;
			else
				strbcalls.bc_head = bcp->bc_next;
			if (bcp == strbcalls.bc_tail)
				strbcalls.bc_tail = pbcp;
			kmem_cache_free(bufcall_cache, bcp);
		}
	}
	mutex_exit(&service_queue);
}

/*
 * Duplicate a message block by block (uses dupb), returning
 * a pointer to the duplicate message.
 * Returns a non-NULL value only if the entire message
 * was dup'd.
 */
mblk_t *
dupmsg(mblk_t *bp)
{
	mblk_t *head, *nbp;

	if (!bp || !(nbp = head = dupb(bp)))
		return (NULL);

	while (bp->b_cont) {
		if (!(nbp->b_cont = dupb(bp->b_cont))) {
			freemsg(head);
			return (NULL);
		}
		nbp = nbp->b_cont;
		bp = bp->b_cont;
	}
	return (head);
}

/*
 * Copy data from message block to newly allocated message block and
 * data block.  The copy is rounded out to full word boundaries so that
 * the (usually) more efficient word copy can be done.
 * Returns new message block pointer, or NULL if error.
 * The alignment of rptr (w.r.t. word alignment) will be the same in the copy
 * as in the original even when db_base is not word aligned. (bug 1052877)
 */
mblk_t *
copyb(mblk_t *bp)
{
	mblk_t *nbp;
	dblk_t *dp, *ndp;
	caddr_t base;
	size_t	unaligned;

	ASSERT(bp->b_wptr >= bp->b_rptr);

	dp = bp->b_datap;
	unaligned = (size_t)dp->db_base & (sizeof (int) - 1);
	if (!(nbp = allocb(dp->db_lim - dp->db_base + unaligned, BPRI_MED)))
		return (NULL);
	nbp->b_flag = bp->b_flag;
	nbp->b_band = bp->b_band;
	ndp = nbp->b_datap;
	ndp->db_type = dp->db_type;
	nbp->b_rptr = ndp->db_base + (bp->b_rptr - dp->db_base) + unaligned;
	nbp->b_wptr = nbp->b_rptr + (bp->b_wptr - bp->b_rptr);
	base = straln(nbp->b_rptr);
	strbcpy(straln(bp->b_rptr), base,
	    straln(nbp->b_wptr + (sizeof (int)-1)) - base);
	return (nbp);
}


/*
 * Copy data from message to newly allocated message using new
 * data blocks.  Returns a pointer to the new message, or NULL if error.
 */
mblk_t *
copymsg(mblk_t *bp)
{
	mblk_t *head, *nbp;

	if (!bp || !(nbp = head = copyb(bp)))
		return (NULL);

	while (bp->b_cont) {
		if (!(nbp->b_cont = copyb(bp->b_cont))) {
			freemsg(head);
			return (NULL);
		}
		nbp = nbp->b_cont;
		bp = bp->b_cont;
	}
	return (head);
}

/*
 * link a message block to tail of message
 */
void
linkb(mblk_t *mp, mblk_t *bp)
{
	ASSERT(mp && bp);

	for (; mp->b_cont; mp = mp->b_cont)
		;
	mp->b_cont = bp;
}

/*
 * unlink a message block from head of message
 * return pointer to new message.
 * NULL if message becomes empty.
 */
mblk_t *
unlinkb(mblk_t *bp)
{
	mblk_t *bp1;

	bp1 = bp->b_cont;
	bp->b_cont = NULL;
	return (bp1);
}

/*
 * remove a message block "bp" from message "mp"
 *
 * Return pointer to new message or NULL if no message remains.
 * Return -1 if bp is not found in message.
 */
mblk_t *
rmvb(mblk_t *mp, mblk_t *bp)
{
	mblk_t *tmp;
	mblk_t *lastp = NULL;

	ASSERT(mp && bp);
	for (tmp = mp; tmp; tmp = tmp->b_cont) {
		if (tmp == bp) {
			if (lastp)
				lastp->b_cont = tmp->b_cont;
			else
				mp = tmp->b_cont;
			tmp->b_cont = NULL;
			return (mp);
		}
		lastp = tmp;
	}
	return ((mblk_t *)-1);
}

/*
 * Concatenate and align first len bytes of common
 * message type.  Len == -1, means concat everything.
 * Returns 1 on success, 0 on failure
 * After the pullup, mp points to the pulled up data.
 */
int
pullupmsg(mblk_t *mp, ssize_t len)
{
	mblk_t *bp, *b_cont;
	dblk_t *dbp;
	ssize_t n;

	ASSERT(mp->b_datap->db_ref > 0);
	ASSERT(mp->b_next == NULL && mp->b_prev == NULL);

	if (len == -1) {
		if (mp->b_cont == NULL && str_aligned(mp->b_rptr))
			return (1);
		len = xmsgsize(mp);
	} else {
		ssize_t first_mblk_len = mp->b_wptr - mp->b_rptr;
		ASSERT(first_mblk_len >= 0);
		/*
		 * If the length is less than that of the first mblk,
		 * we want to pull up the message into an aligned mblk.
		 * Though not part of the spec, some callers assume it.
		 */
		if (len <= first_mblk_len) {
			if (str_aligned(mp->b_rptr))
				return (1);
			len = first_mblk_len;
		} else if (xmsgsize(mp) < len)
			return (0);
	}

	if ((bp = allocb(len, BPRI_MED)) == NULL)
		return (0);

	dbp = bp->b_datap;
	dbp->db_type = mp->b_datap->db_type;	/* inherit original type */
	*bp = *mp;		/* swap mblks so bp heads the old msg... */
	mp->b_datap = dbp;	/* ... and mp heads the new message */
	mp->b_datap->db_mblk = mp;
	bp->b_datap->db_mblk = bp;
	mp->b_rptr = mp->b_wptr = dbp->db_base;

	do {
		ASSERT(bp->b_datap->db_ref > 0);
		ASSERT(bp->b_wptr >= bp->b_rptr);
		n = MIN(bp->b_wptr - bp->b_rptr, len);
		bcopy(bp->b_rptr, mp->b_wptr, (size_t)n);
		mp->b_wptr += n;
		bp->b_rptr += n;
		len -= n;
		if (bp->b_rptr != bp->b_wptr)
			break;
		b_cont = bp->b_cont;
		freeb(bp);
		bp = b_cont;
	} while (len && bp);

	mp->b_cont = bp;	/* tack on whatever wasn't pulled up */

	return (1);
}

/*
 * Concatenate and align first len byte of common
 * message type.  Len == -1 means concat everything.
 * The original message is unaltered.
 * Returns a pointer to a new message on success,
 * otherwise returns NULL.
 */
mblk_t *
msgpullup(mblk_t *mp, ssize_t len)
{
	mblk_t	*new_bp;
	ssize_t	totlen;
	ssize_t	n;

	totlen = xmsgsize(mp);

	if ((len > 0) && (len > totlen))
		return (NULL);

	/*
	 * Copy all of the first msg type into one new mblk
	 * and dupmsg and link the rest onto this.
	 */

	len = totlen;

	if ((new_bp = allocb(len, BPRI_LO)) == NULL)
		return (NULL);

	new_bp->b_datap->db_type = mp->b_datap->db_type;
	new_bp->b_flag = mp->b_flag;
	new_bp->b_band = mp->b_band;

	while (len > 0) {
		n = mp->b_wptr - mp->b_rptr;
		bcopy(mp->b_rptr, new_bp->b_wptr, (size_t)n);
		new_bp->b_wptr += n;
		len -= n;
		mp = mp->b_cont;
	}

	if (mp)
		new_bp->b_cont = dupmsg(mp);

	return (new_bp);
}

/*
 * Trim bytes from message
 *  len > 0, trim from head
 *  len < 0, trim from tail
 * Returns 1 on success, 0 on failure.
 */
int
adjmsg(mblk_t *mp, ssize_t len)
{
	mblk_t *bp;
	mblk_t *save_bp = NULL;
	mblk_t *prev_bp;
	mblk_t *bcont;
	unsigned char type;
	ssize_t n;
	int fromhead;
	int first;

	ASSERT(mp != NULL);

	if (len < 0) {
		fromhead = 0;
		len = -len;
	} else {
		fromhead = 1;
	}

	if (xmsgsize(mp) < len)
		return (0);


	if (fromhead) {
		first = 1;
		while (len) {
			ASSERT(mp->b_wptr >= mp->b_rptr);
			n = MIN(mp->b_wptr - mp->b_rptr, len);
			mp->b_rptr += n;
			len -= n;

			/*
			 * If this is not the first zero length
			 * message remove it
			 */
			if (!first && (mp->b_wptr == mp->b_rptr)) {
				bcont = mp->b_cont;
				freeb(mp);
				mp = save_bp->b_cont = bcont;
			} else {
				save_bp = mp;
				mp = mp->b_cont;
			}
			first = 0;
		}
	} else {
		type = mp->b_datap->db_type;
		while (len) {
			bp = mp;
			save_bp = NULL;

			/*
			 * Find the last message of same type
			 */

			while (bp && bp->b_datap->db_type == type) {
				ASSERT(bp->b_wptr >= bp->b_rptr);
				prev_bp = save_bp;
				save_bp = bp;
				bp = bp->b_cont;
			}
			if (save_bp == NULL)
				break;
			n = MIN(save_bp->b_wptr - save_bp->b_rptr, len);
			save_bp->b_wptr -= n;
			len -= n;

			/*
			 * If this is not the first message
			 * and we have taken away everything
			 * from this message, remove it
			 */

			if ((save_bp != mp) &&
				(save_bp->b_wptr == save_bp->b_rptr)) {
				bcont = save_bp->b_cont;
				freeb(save_bp);
				prev_bp->b_cont = bcont;
			}
		}
	}
	return (1);
}

/*
 * get number of data bytes in message
 */
size_t
msgdsize(mblk_t *bp)
{
	size_t count = 0;

	for (; bp; bp = bp->b_cont)
		if (bp->b_datap->db_type == M_DATA) {
			ASSERT(bp->b_wptr >= bp->b_rptr);
			count += bp->b_wptr - bp->b_rptr;
		}
	return (count);
}

/*
 * Get a message off head of queue
 *
 * If queue has no buffers then mark queue
 * with QWANTR. (queue wants to be read by
 * someone when data becomes available)
 *
 * If there is something to take off then do so.
 * If queue falls below hi water mark turn off QFULL
 * flag.  Decrement weighted count of queue.
 * Also turn off QWANTR because queue is being read.
 *
 * The queue count is maintained on a per-band basis.
 * Priority band 0 (normal messages) uses q_count,
 * q_lowat, etc.  Non-zero priority bands use the
 * fields in their respective qband structures
 * (qb_count, qb_lowat, etc.)  All messages appear
 * on the same list, linked via their b_next pointers.
 * q_first is the head of the list.  q_count does
 * not reflect the size of all the messages on the
 * queue.  It only reflects those messages in the
 * normal band of flow.  The one exception to this
 * deals with high priority messages.  They are in
 * their own conceptual "band", but are accounted
 * against q_count.
 *
 * If queue count is below the lo water mark and QWANTW
 * is set, enable the closest backq which has a service
 * procedure and turn off the QWANTW flag.
 *
 * getq could be built on top of rmvq, but isn't because
 * of performance considerations.
 */
mblk_t *
getq(queue_t *q)
{
	mblk_t *bp;
	int band = 0;

	bp = getq_noenab(q);
	if (bp != NULL)
		band = bp->b_band;

	/*
	 * Inlined from qbackenable().
	 * Quick check without holding the lock.
	 */
	if (band == 0 && (q->q_flag & (QWANTW|QWANTWSYNC)) == 0)
		return (bp);

	qbackenable(q, band);
	return (bp);
}

/*
 * Like getq() but does not backenable.  This is used by the stream
 * head when a putback() is likely.  The caller must call qbackenable()
 * after it is done with accessing the queue.
 */
mblk_t *
getq_noenab(queue_t *q)
{
	mblk_t *bp;
	mblk_t *tmp;
	qband_t *qbp;
	kthread_id_t freezer;

	/* freezestr should allow its caller to call getq/putq */
	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));

	if ((bp = q->q_first) == 0) {
		q->q_flag |= QWANTR;
	} else {
		if (bp->b_flag & MSGNOGET) {	/* hack hack hack */
			while (bp && (bp->b_flag & MSGNOGET))
				bp = bp->b_next;
			if (bp)
				rmvq_noenab(q, bp);
			if (freezer != curthread)
				mutex_exit(QLOCK(q));
			return (bp);
		}
		if ((q->q_first = bp->b_next) == 0)
			q->q_last = NULL;
		else
			q->q_first->b_prev = NULL;

		if (bp->b_band == 0) {
			for (tmp = bp; tmp; tmp = tmp->b_cont)
				q->q_count -= (tmp->b_wptr - tmp->b_rptr);
			if (q->q_count < q->q_hiwat) {
				q->q_flag &= ~QFULL;
			}
		} else {
			int i;

			ASSERT(bp->b_band <= q->q_nband);
			ASSERT(q->q_bandp != NULL);
			ASSERT(MUTEX_HELD(QLOCK(q)));
			qbp = q->q_bandp;
			i = bp->b_band;
			while (--i > 0)
				qbp = qbp->qb_next;
			if (qbp->qb_first == qbp->qb_last) {
				qbp->qb_first = NULL;
				qbp->qb_last = NULL;
			} else {
				qbp->qb_first = bp->b_next;
			}
			for (tmp = bp; tmp; tmp = tmp->b_cont)
				qbp->qb_count -= (tmp->b_wptr - tmp->b_rptr);
			if (qbp->qb_count < qbp->qb_hiwat)
				qbp->qb_flag &= ~QB_FULL;
		}
		q->q_flag &= ~QWANTR;
		bp->b_next = NULL;
		bp->b_prev = NULL;
	}
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (bp);
}

/*
 * Determine if a backenable is needed after removing a message in the
 * specified band.
 * NOTE: This routine assumes that something like getq_noenab() has been
 * already called.
 *
 * For the read side it is ok to hold sd_lock across calling this (and the
 * stream head often does).
 * But for the write side strwakeq might be invoked and it acuquires sd_lock.
 */
void
qbackenable(queue_t *q, int band)
{
	int backenab = 0;
	qband_t *qbp;
	kthread_id_t freezer;

	ASSERT(q);
	ASSERT((q->q_flag & QREADR) || MUTEX_NOT_HELD(&STREAM(q)->sd_lock));

	/*
	 * Quick check without holding the lock.
	 * OK since after getq() has lowered the q_count these flags
	 * would not change unless either the qbackenable() is done by
	 * another thread (which is ok) or the queue has gotten QFULL
	 * in which case another backenable will take place when the queue
	 * drops below q_lowat.
	 */
	if (band == 0 && (q->q_flag & (QWANTW|QWANTWSYNC)) == 0)
		return;

	/* freezestr should allow its caller to call getq/putq */
	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));

	if (band == 0) {
		if (q->q_count < q->q_lowat || q->q_lowat == 0)
			backenab = q->q_flag & (QWANTW|QWANTWSYNC);
	} else {
		int i;

		ASSERT((unsigned)band <= q->q_nband);
		ASSERT(q->q_bandp != NULL);

		qbp = q->q_bandp;
		i = band;
		while (--i > 0)
			qbp = qbp->qb_next;

		if (qbp->qb_count < qbp->qb_lowat || qbp->qb_lowat == 0)
			backenab = qbp->qb_flag & QB_WANTW;
	}

	if (backenab == 0) {
		if (freezer != curthread)
			mutex_exit(QLOCK(q));
		return;
	}

	/* Have to drop the lock across strwakeq and backenable */
	if (backenab & QWANTWSYNC)
		q->q_flag &= ~QWANTWSYNC;
	if (backenab & (QWANTW|QB_WANTW)) {
		if (band != 0)
			qbp->qb_flag &= ~QB_WANTW;
		else {
			q->q_flag &= ~QWANTW;
		}
	}

	if (freezer != curthread)
		mutex_exit(QLOCK(q));

	if (backenab & QWANTWSYNC)
		strwakeq(q, QWANTWSYNC);
	if (backenab & (QWANTW|QB_WANTW))
		backenable(q, band);
}


/*
 * Remove a message from a queue.  The queue count and other
 * flow control parameters are adjusted and the back queue
 * enabled if necessary.
 *
 * rmvq can be called with the stream frozen, but other utility functions
 * holding QLOCK, and by streams modules without any locks/frozen.
 */
void
rmvq(queue_t *q, mblk_t *mp)
{
	int band;

	ASSERT(mp);
	band = mp->b_band;

	rmvq_noenab(q, mp);
	if (curthread != STREAM(q)->sd_freezer &&
	    MUTEX_HELD(QLOCK(q))) {
		/*
		 * qbackenable can handle a frozen stream but not a "random"
		 * qlock being held. Drop lock across qbackenable.
		 */
		mutex_exit(QLOCK(q));
		qbackenable(q, band);
		mutex_enter(QLOCK(q));
	} else
		qbackenable(q, band);
}

/*
 * Like rmvq() but without any backenabling.
 * This exists to handle the MSGNOGET case in getq() and SR_CONSOL_DATA
 * in strrput().
 */
void
rmvq_noenab(queue_t *q, mblk_t *mp)
{
	mblk_t *tmp;
	int i;
	qband_t *qbp = NULL;
	kthread_id_t freezer;

	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else if (MUTEX_HELD(QLOCK(q))) {
		/* Don't drop lock on exit */
		freezer = curthread;
	} else
		mutex_enter(QLOCK(q));

	ASSERT(mp->b_band <= q->q_nband);
	if (mp->b_band != 0) {		/* Adjust band pointers */
		ASSERT(q->q_bandp != NULL);
		qbp = q->q_bandp;
		i = mp->b_band;
		while (--i > 0)
			qbp = qbp->qb_next;
		if (mp == qbp->qb_first) {
			if (mp->b_next && mp->b_band == mp->b_next->b_band)
				qbp->qb_first = mp->b_next;
			else
				qbp->qb_first = NULL;
		}
		if (mp == qbp->qb_last) {
			if (mp->b_prev && mp->b_band == mp->b_prev->b_band)
				qbp->qb_last = mp->b_prev;
			else
				qbp->qb_last = NULL;
		}
	}

	/*
	 * Remove the message from the list.
	 */
	if (mp->b_prev)
		mp->b_prev->b_next = mp->b_next;
	else
		q->q_first = mp->b_next;
	if (mp->b_next)
		mp->b_next->b_prev = mp->b_prev;
	else
		q->q_last = mp->b_prev;
	mp->b_next = NULL;
	mp->b_prev = NULL;

	if (mp->b_band == 0) {		/* Perform q_count accounting */
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			q->q_count -= (tmp->b_wptr - tmp->b_rptr);
		if (q->q_count < q->q_hiwat) {
			q->q_flag &= ~QFULL;
		}
	} else {			/* Perform qb_count accounting */
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			qbp->qb_count -= (tmp->b_wptr - tmp->b_rptr);
		if (qbp->qb_count < qbp->qb_hiwat)
			qbp->qb_flag &= ~QB_FULL;
	}
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
}

/*
 * Empty a queue.
 * If flag is set, remove all messages.  Otherwise, remove
 * only non-control messages.  If queue falls below its low
 * water mark, and QWANTW is set, enable the nearest upstream
 * service procedure.
 *
 * Historical note: when merging the M_FLUSH code in strrput with this
 * code one difference was discovered. flushq did not have a check
 * for q_lowat == 0 in the backenabling test.
 *
 * pcproto_flag specifies whether or not a M_PCPROTO message should be flushed
 * if one exists on the queue.
 */
void
flushq_common(queue_t *q, int flag, int pcproto_flag)
{
	mblk_t *mp, *nmp;
	qband_t *qbp;
	int backenab = 0;
	unsigned char bpri;
	unsigned char	qbf[NBAND];	/* band flushing backenable flags */

	if (q->q_first == NULL)
		return;

	mutex_enter(QLOCK(q));
	mp = q->q_first;
	q->q_first = NULL;
	q->q_last = NULL;
	q->q_count = 0;
	for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
		qbp->qb_first = NULL;
		qbp->qb_last = NULL;
		qbp->qb_count = 0;
		qbp->qb_flag &= ~QB_FULL;
	}
	q->q_flag &= ~QFULL;
	mutex_exit(QLOCK(q));
	while (mp) {
		nmp = mp->b_next;
		mp->b_next = mp->b_prev = NULL;
		if (pcproto_flag && (mp->b_datap->db_type == M_PCPROTO))
			(void) putq(q, mp);
		else if (flag || datamsg(mp->b_datap->db_type))
			freemsg_flush(mp);
		else
			(void) putq(q, mp);
		mp = nmp;
	}
	bpri = 1;
	mutex_enter(QLOCK(q));
	for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
		if ((qbp->qb_flag & QB_WANTW) &&
		    (qbp->qb_count < qbp->qb_lowat || qbp->qb_lowat == 0)) {
			qbp->qb_flag &= ~QB_WANTW;
			backenab = 1;
			qbf[bpri] = 1;
		} else
			qbf[bpri] = 0;
		bpri++;
	}
	ASSERT(bpri == (unsigned char)(q->q_nband + 1));
	if ((q->q_flag & QWANTW) &&
	    (q->q_lowat == 0 || q->q_count < q->q_lowat)) {
		q->q_flag &= ~QWANTW;
		backenab = 1;
		qbf[0] = 1;
	} else
		qbf[0] = 0;

	/*
	 * If any band can now be written to, and there is a writer
	 * for that band, then backenable the closest service procedure.
	 */
	if (backenab) {
		mutex_exit(QLOCK(q));
		for (bpri = q->q_nband; bpri != 0; bpri--)
			if (qbf[bpri])
				backenable(q, (int)bpri);
		if (qbf[0])
			backenable(q, 0);
	} else
		mutex_exit(QLOCK(q));
}

/*
 * The real flushing takes place in flushq_common. This is done so that
 * a flag which specifies whether or not M_PCPROTO messages should be flushed
 * or not. Currently the only place that uses this flag is the stream head.
 */
void
flushq(queue_t *q, int flag)
{
	flushq_common(q, flag, 0);
}

/*
 * Flush the queue of messages of the given priority band.
 * There is some duplication of code between flushq and flushband.
 * This is because we want to optimize the code as much as possible.
 * The assumption is that there will be more messages in the normal
 * (priority 0) band than in any other.
 *
 * Historical note: when merging the M_FLUSH code in strrput with this
 * code one difference was discovered. flushband had an extra check for
 * did not have a check for (mp->b_datap->db_type < QPCTL) in the band 0
 * case. That check does not match the man page for flushband and was not
 * in the strrput flush code hence it was removed.
 */
void
flushband(queue_t *q, unsigned char pri, int flag)
{
	mblk_t *mp;
	mblk_t *nmp;
	mblk_t *last;
	qband_t *qbp;
	int band;

	if (pri > q->q_nband) {
		return;
	}
	mutex_enter(QLOCK(q));
	if (pri == 0) {
		mp = q->q_first;
		q->q_first = NULL;
		q->q_last = NULL;
		q->q_count = 0;
		for (qbp = q->q_bandp; qbp; qbp = qbp->qb_next) {
			qbp->qb_first = NULL;
			qbp->qb_last = NULL;
			qbp->qb_count = 0;
			qbp->qb_flag &= ~QB_FULL;
		}
		q->q_flag &= ~QFULL;
		mutex_exit(QLOCK(q));
		while (mp) {
			nmp = mp->b_next;
			mp->b_next = mp->b_prev = NULL;
			if ((mp->b_band == 0) &&
			    (flag || datamsg(mp->b_datap->db_type)))
				freemsg_flush(mp);
			else
				(void) putq(q, mp);
			mp = nmp;
		}
		mutex_enter(QLOCK(q));
		if ((q->q_flag & QWANTW) &&
		    (q->q_lowat == 0 || q->q_count < q->q_lowat)) {
			q->q_flag &= ~QWANTW;
			mutex_exit(QLOCK(q));

			backenable(q, (int)pri);
		} else
			mutex_exit(QLOCK(q));
	} else {	/* pri != 0 */
		band = pri;
		ASSERT(MUTEX_HELD(QLOCK(q)));
		qbp = q->q_bandp;
		while (--band > 0)
			qbp = qbp->qb_next;
		mp = qbp->qb_first;
		if (mp == NULL) {
			mutex_exit(QLOCK(q));
			return;
		}
		last = qbp->qb_last;
		if (mp == last)		/* only message in band */
			last = mp->b_next;
		while (mp != last) {
			nmp = mp->b_next;
			if (mp->b_band == pri) {
				if (flag || datamsg(mp->b_datap->db_type)) {
					rmvq(q, mp);
					freemsg_flush(mp);
				}
			}
			mp = nmp;
		}
		if (mp && mp->b_band == pri) {
			if (flag || datamsg(mp->b_datap->db_type)) {
				rmvq(q, mp);
				freemsg_flush(mp);
			}
		}
		mutex_exit(QLOCK(q));
	}
}

/*
 * Return 1 if the queue is not full.  If the queue is full, return
 * 0 (may not put message) and set QWANTW flag (caller wants to write
 * to the queue).
 */
int
canput(queue_t *q)
{
	TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_IN,
	    "canput?:%s(%X)\n", QNAME(q), q);

	/* this is for loopback transports, they should not do a canput */
	ASSERT(STRMATED(q->q_stream) || STREAM(q) == STREAM(q->q_nfsrv));

	/* Find next forward module that has a service procedure */
	q = q->q_nfsrv;

	if (!(q->q_flag & QFULL)) {
		TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%X %d", q, 1);
		return (1);
	}
	mutex_enter(QLOCK(q));
	if (q->q_flag & QFULL) {
		q->q_flag |= QWANTW;
		mutex_exit(QLOCK(q));
		TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%X %d", q, 0);
		return (0);
	}
	mutex_exit(QLOCK(q));
	TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%X %d", q, 1);
	return (1);
}

/*
 * This is the new canput for use with priority bands.  Return 1 if the
 * band is not full.  If the band is full, return 0 (may not put message)
 * and set QWANTW(QB_WANTW) flag for zero(non-zero) band (caller wants to
 * write to the queue).
 */
int
bcanput(queue_t *q, unsigned char pri)
{
	qband_t *qbp;

	TRACE_2(TR_FAC_STREAMS_FR, TR_BCANPUT_IN, "bcanput?:%X %X", q, pri);
	if (!q)
		return (0);

	/* Find next forward module that has a service procedure */
	q = q->q_nfsrv;

	mutex_enter(QLOCK(q));
	if (pri == 0) {
		if (q->q_flag & QFULL) {
			q->q_flag |= QWANTW;
			mutex_exit(QLOCK(q));
			TRACE_3(TR_FAC_STREAMS_FR, TR_BCANPUT_OUT,
				"bcanput:%X %X %d", q, pri, 0);
			return (0);
		}
	} else {	/* pri != 0 */
		if (pri > q->q_nband) {
			/*
			 * No band exists yet, so return success.
			 */
			mutex_exit(QLOCK(q));
			TRACE_3(TR_FAC_STREAMS_FR, TR_BCANPUT_OUT,
				"bcanput:%X %X %d", q, pri, 1);
			return (1);
		}
		qbp = q->q_bandp;
		while (--pri)
			qbp = qbp->qb_next;
		if (qbp->qb_flag & QB_FULL) {
			qbp->qb_flag |= QB_WANTW;
			mutex_exit(QLOCK(q));
			TRACE_3(TR_FAC_STREAMS_FR, TR_BCANPUT_OUT,
				"bcanput:%X %X %d", q, pri, 0);
			return (0);
		}
	}
	mutex_exit(QLOCK(q));
	TRACE_3(TR_FAC_STREAMS_FR, TR_BCANPUT_OUT,
		"bcanput:%X %X %d", q, pri, 1);
	return (1);
}

/*
 * Put a message on a queue.
 *
 * Messages are enqueued on a priority basis.  The priority classes
 * are HIGH PRIORITY (type >= QPCTL), PRIORITY (type < QPCTL && band > 0),
 * and B_NORMAL (type < QPCTL && band == 0).
 *
 * Add appropriate weighted data block sizes to queue count.
 * If queue hits high water mark then set QFULL flag.
 *
 * If QNOENAB is not set (putq is allowed to enable the queue),
 * enable the queue only if the message is PRIORITY,
 * or the QWANTR flag is set (indicating that the service procedure
 * is ready to read the queue.  This implies that a service
 * procedure must NEVER put a high priority message back on its own
 * queue, as this would result in an infinite loop (!).
 */
int
putq(queue_t *q, mblk_t *bp)
{
	mblk_t *tmp;
	qband_t *qbp = NULL;
	int mcls = (int)queclass(bp);
	kthread_id_t freezer;

	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));

	/*
	 * Make sanity checks and if qband structure is not yet
	 * allocated, do so.
	 */
	if (mcls == QPCTL) {
		if (bp->b_band != 0)
			bp->b_band = 0;		/* force to be correct */
	} else if (bp->b_band != 0) {
		int i;
		qband_t **qbpp;

		if (bp->b_band > q->q_nband) {

			/*
			 * The qband structure for this priority band is
			 * not on the queue yet, so we have to allocate
			 * one on the fly.  It would be wasteful to
			 * associate the qband structures with every
			 * queue when the queues are allocated.  This is
			 * because most queues will only need the normal
			 * band of flow which can be described entirely
			 * by the queue itself.
			 */
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (bp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					if (freezer != curthread)
						mutex_exit(QLOCK(q));
					return (0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		ASSERT(MUTEX_HELD(QLOCK(q)));
		qbp = q->q_bandp;
		i = bp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	/*
	 * If queue is empty, add the message and initialize the pointers.
	 * Otherwise, adjust message pointers and queue pointers based on
	 * the type of the message and where it belongs on the queue.  Some
	 * code is duplicated to minimize the number of conditionals and
	 * hopefully minimize the amount of time this routine takes.
	 */
	if (!q->q_first) {
		bp->b_next = NULL;
		bp->b_prev = NULL;
		q->q_first = bp;
		q->q_last = bp;
		if (qbp) {
			qbp->qb_first = bp;
			qbp->qb_last = bp;
		}
	} else if (!qbp) {	/* bp->b_band == 0 */

		/*
		 * If queue class of message is less than or equal to
		 * that of the last one on the queue, tack on to the end.
		 */
		tmp = q->q_last;
		if (mcls <= (int)queclass(tmp)) {
			bp->b_next = NULL;
			bp->b_prev = tmp;
			tmp->b_next = bp;
			q->q_last = bp;
		} else {
			tmp = q->q_first;
			while ((int)queclass(tmp) >= mcls)
				tmp = tmp->b_next;

			/*
			 * Insert bp before tmp.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		}
	} else {		/* bp->b_band != 0 */
		if (qbp->qb_first) {
			tmp = qbp->qb_last;

			/*
			 * Insert bp after the last message in this band.
			 */
			bp->b_next = tmp->b_next;
			if (tmp->b_next)
				tmp->b_next->b_prev = bp;
			else
				q->q_last = bp;
			bp->b_prev = tmp;
			tmp->b_next = bp;
		} else {
			tmp = q->q_last;
			if ((mcls < (int)queclass(tmp)) ||
			    (bp->b_band <= tmp->b_band)) {

				/*
				 * Tack bp on end of queue.
				 */
				bp->b_next = NULL;
				bp->b_prev = tmp;
				tmp->b_next = bp;
				q->q_last = bp;
			} else {
				tmp = q->q_first;
				while (tmp->b_datap->db_type >= QPCTL)
					tmp = tmp->b_next;
				while (tmp->b_band >= bp->b_band)
					tmp = tmp->b_next;

				/*
				 * Insert bp before tmp.
				 */
				bp->b_next = tmp;
				bp->b_prev = tmp->b_prev;
				if (tmp->b_prev)
					tmp->b_prev->b_next = bp;
				else
					q->q_first = bp;
				tmp->b_prev = bp;
			}
			qbp->qb_first = bp;
		}
		qbp->qb_last = bp;
	}

	if (qbp) {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if (qbp->qb_count >= qbp->qb_hiwat)
			qbp->qb_flag |= QB_FULL;
	} else {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if ((mcls > QNORM) ||
	    (canenable(q) && (q->q_flag & QWANTR || bp->b_band)))
		qenable_locked(q);
	ASSERT(MUTEX_HELD(QLOCK(q)));
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (1);
}

/*
 * Put stuff back at beginning of Q according to priority order.
 * See comment on putq above for details.
 */
int
putbq(queue_t *q, mblk_t *bp)
{
	mblk_t *tmp;
	qband_t *qbp = NULL;
	int mcls = (int)queclass(bp);
	kthread_id_t freezer;

	ASSERT(q && bp);
	ASSERT(bp->b_next == NULL);
	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));

	/*
	 * Make sanity checks and if qband structure is not yet
	 * allocated, do so.
	 */
	if (mcls == QPCTL) {
		if (bp->b_band != 0)
			bp->b_band = 0;		/* force to be correct */
	} else if (bp->b_band != 0) {
		int i;
		qband_t **qbpp;

		if (bp->b_band > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (bp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					if (freezer != curthread)
						mutex_exit(QLOCK(q));
					return (0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = bp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	/*
	 * If queue is empty or if message is high priority,
	 * place on the front of the queue.
	 */
	tmp = q->q_first;
	if ((!tmp) || (mcls == QPCTL)) {
		bp->b_next = tmp;
		if (tmp)
			tmp->b_prev = bp;
		else
			q->q_last = bp;
		q->q_first = bp;
		bp->b_prev = NULL;
		if (qbp) {
			qbp->qb_first = bp;
			qbp->qb_last = bp;
		}
	} else if (qbp) {	/* bp->b_band != 0 */
		tmp = qbp->qb_first;
		if (tmp) {

			/*
			 * Insert bp before the first message in this band.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		} else {
			tmp = q->q_last;
			if ((mcls < (int)queclass(tmp)) ||
			    (bp->b_band < tmp->b_band)) {

				/*
				 * Tack bp on end of queue.
				 */
				bp->b_next = NULL;
				bp->b_prev = tmp;
				tmp->b_next = bp;
				q->q_last = bp;
			} else {
				tmp = q->q_first;
				while (tmp->b_datap->db_type >= QPCTL)
					tmp = tmp->b_next;
				while (tmp->b_band > bp->b_band)
					tmp = tmp->b_next;

				/*
				 * Insert bp before tmp.
				 */
				bp->b_next = tmp;
				bp->b_prev = tmp->b_prev;
				if (tmp->b_prev)
					tmp->b_prev->b_next = bp;
				else
					q->q_first = bp;
				tmp->b_prev = bp;
			}
			qbp->qb_last = bp;
		}
		qbp->qb_first = bp;
	} else {		/* bp->b_band == 0 && !QPCTL */

		/*
		 * If the queue class or band is less than that of the last
		 * message on the queue, tack bp on the end of the queue.
		 */
		tmp = q->q_last;
		if ((mcls < (int)queclass(tmp)) || (bp->b_band < tmp->b_band)) {
			bp->b_next = NULL;
			bp->b_prev = tmp;
			tmp->b_next = bp;
			q->q_last = bp;
		} else {
			tmp = q->q_first;
			while (tmp->b_datap->db_type >= QPCTL)
				tmp = tmp->b_next;
			while (tmp->b_band > bp->b_band)
				tmp = tmp->b_next;

			/*
			 * Insert bp before tmp.
			 */
			bp->b_next = tmp;
			bp->b_prev = tmp->b_prev;
			if (tmp->b_prev)
				tmp->b_prev->b_next = bp;
			else
				q->q_first = bp;
			tmp->b_prev = bp;
		}
	}

	if (qbp) {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if (qbp->qb_count >= qbp->qb_hiwat)
			qbp->qb_flag |= QB_FULL;
	} else {
		for (tmp = bp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if ((mcls > QNORM) || (canenable(q) && (q->q_flag & QWANTR)))
		qenable_locked(q);
	ASSERT(MUTEX_HELD(QLOCK(q)));
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (1);
}

/*
 * Insert a message before an existing message on the queue.  If the
 * existing message is NULL, the new messages is placed on the end of
 * the queue.  The queue class of the new message is ignored.  However,
 * the priority band of the new message must adhere to the following
 * ordering:
 *
 *	emp->b_prev->b_band >= mp->b_band >= emp->b_band.
 *
 * All flow control parameters are updated.
 *
 * insq can be called with the stream frozen, but other utility functions
 * holding QLOCK, and by streams modules without any locks/frozen.
 */
int
insq(queue_t *q, mblk_t *emp, mblk_t *mp)
{
	mblk_t *tmp;
	qband_t *qbp = NULL;
	int mcls = (int)queclass(mp);
	kthread_id_t freezer;

	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else if (MUTEX_HELD(QLOCK(q))) {
		/* Don't drop lock on exit */
		freezer = curthread;
	} else
		mutex_enter(QLOCK(q));

	if (mcls == QPCTL) {
		if (mp->b_band != 0)
			mp->b_band = 0;		/* force to be correct */
		if (emp && emp->b_prev &&
		    (emp->b_prev->b_datap->db_type < QPCTL))
			goto badord;
	}
	if (emp) {
		if (((mcls == QNORM) && (mp->b_band < emp->b_band)) ||
		    (emp->b_prev && (emp->b_prev->b_datap->db_type < QPCTL) &&
		    (emp->b_prev->b_band < mp->b_band))) {
			goto badord;
		}
	} else {
		tmp = q->q_last;
		if (tmp && (mcls == QNORM) && (mp->b_band > tmp->b_band)) {
badord:
			cmn_err(CE_WARN,
			    "insq: attempt to insert message out of order "
			    "on q %p", (void *)q);
			if (freezer != curthread)
				mutex_exit(QLOCK(q));
			return (0);
		}
	}

	if (mp->b_band != 0) {
		int i;
		qband_t **qbpp;

		if (mp->b_band > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (mp->b_band > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					if (freezer != curthread)
						mutex_exit(QLOCK(q));
					return (0);
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = mp->b_band;
		while (--i)
			qbp = qbp->qb_next;
	}

	if ((mp->b_next = emp) != NULL) {
		if ((mp->b_prev = emp->b_prev) != NULL)
			emp->b_prev->b_next = mp;
		else
			q->q_first = mp;
		emp->b_prev = mp;
	} else {
		if ((mp->b_prev = q->q_last) != NULL)
			q->q_last->b_next = mp;
		else
			q->q_first = mp;
		q->q_last = mp;
	}

	if (qbp) {	/* adjust qband pointers and count */
		if (!qbp->qb_first) {
			qbp->qb_first = mp;
			qbp->qb_last = mp;
		} else {
			if (mp->b_prev == NULL || (mp->b_prev != NULL &&
			    (mp->b_prev->b_band != mp->b_band)))
				qbp->qb_first = mp;
			else if (mp->b_next == NULL || (mp->b_next != NULL &&
				    (mp->b_next->b_band != mp->b_band)))
				qbp->qb_last = mp;
		}
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			qbp->qb_count += (tmp->b_wptr - tmp->b_rptr);
		if (qbp->qb_count >= qbp->qb_hiwat)
			qbp->qb_flag |= QB_FULL;
	} else {
		for (tmp = mp; tmp; tmp = tmp->b_cont)
			q->q_count += (tmp->b_wptr - tmp->b_rptr);
		if (q->q_count >= q->q_hiwat)
			q->q_flag |= QFULL;
	}
	if (canenable(q) && (q->q_flag & QWANTR))
		qenable_locked(q);

	ASSERT(MUTEX_HELD(QLOCK(q)));
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (1);
}

/*
 * Create and put a control message on queue.
 */
int
putctl(queue_t *q, int type)
{
	mblk_t *bp;

	if ((datamsg(type) && (type != M_DELAY)) ||
	    (bp = allocb_tryhard(0)) == NULL)
		return (0);
	bp->b_datap->db_type = (unsigned char) type;

	put(q, bp);

	return (1);
}

/*
 * Control message with a single-byte parameter
 */
int
putctl1(queue_t *q, int type, int param)
{
	mblk_t *bp;

	if ((datamsg(type) && (type != M_DELAY)) ||
	    (bp = allocb_tryhard(1)) == NULL)
		return (0);
	bp->b_datap->db_type = (unsigned char)type;
	*bp->b_wptr++ = (unsigned char)param;

	put(q, bp);

	return (1);
}

/*
 * Return the queue upstream from this one
 */
queue_t *
backq(queue_t *q)
{
	q = OTHERQ(q);
	if (q->q_next) {
		q = q->q_next;
		return (OTHERQ(q));
	}
	return (NULL);
}



/*
 * Send a block back up the queue in reverse from this
 * one (e.g. to respond to ioctls)
 */
void
qreply(queue_t *q, mblk_t *bp)
{
	ASSERT(q && bp);

	putnext(OTHERQ(q), bp);
}

/*
 * Streams Queue Scheduling
 *
 * Queues are enabled through qenable() when they have messages to
 * process.  They are serviced by queuerun(), which runs each enabled
 * queue's service procedure.  The call to queuerun() is processor
 * dependent - the general principle is that it be run whenever a queue
 * is enabled but before returning to user level.  For system calls,
 * the function runqueues() is called if their action causes a queue
 * to be enabled.  For device interrupts, queuerun() should be
 * called before returning from the last level of interrupt.  Beyond
 * this, no timing assumptions should be made about queue scheduling.
 */

/*
 * Enable a queue: put it on list of those whose service procedures are
 * ready to run and set up the scheduling mechanism.
 * The broadcast is done outside the mutex -> to avoid the woken thread
 * from contending with the mutex. This is OK 'cos the queue has been
 * enqueued on the runlist and flagged safely at this point.
 */
void
qenable(queue_t *q)
{
	mutex_enter(QLOCK(q));
	qenable_locked(q);
	mutex_exit(QLOCK(q));
}
/* Used within framework when the queue is already locked */
void
qenable_locked(queue_t *q)
{
	ASSERT(MUTEX_HELD(QLOCK(q)));

	if (!q->q_qinfo->qi_srvp)
		return;

	/*
	 * Do not place on run queue if already enabled.
	 */
	if (q->q_flag & (QWCLOSE|QENAB))
		return;

	mutex_enter(&service_queue);

	TRACE_3(TR_FAC_STREAMS_FR, TR_QENABLE,
		"qenable:enable %s(%X) from %K", QNAME(q), q, caller());

	ASSERT(!(q->q_flag&QHLIST));

	/*
	 * mark queue enabled and place on run list
	 * if it is not already being serviced.
	 */
	q->q_flag |= QENAB;
	if (q->q_flag & QINSERVICE) {
		mutex_exit(&service_queue);
		return;
	}
	ASSERT(q->q_link == NULL);
	if (!qhead) {
		ASSERT(qtail == NULL);
		qhead = q;
	} else {
		ASSERT(qtail);
		qtail->q_link = q;
	}
	qtail = q;
#ifdef TRACE
	enqueued++;
#endif /* TRACE */
	q->q_link = NULL;

	/*
	 * set up scheduling mechanism
	 */
	setqsched();

	mutex_exit(&service_queue);
	if (run_queues == 0)
		cv_signal(&services_to_run);
}

/*
 * Return number of messages on queue
 */
int
qsize(queue_t *qp)
{
	int count = 0;
	mblk_t *mp;

	mutex_enter(QLOCK(qp));
	for (mp = qp->q_first; mp; mp = mp->b_next)
		count++;
	mutex_exit(QLOCK(qp));
	return (count);
}

/*
 * noenable - set queue so that putq() will not enable it.
 * enableok - set queue so that putq() can enable it.
 */
void
noenable(queue_t *q)
{
	mutex_enter(QLOCK(q));
	q->q_flag |= QNOENB;
	mutex_exit(QLOCK(q));
}

void
enableok(queue_t *q)
{
	mutex_enter(QLOCK(q));
	q->q_flag &= ~QNOENB;
	mutex_exit(QLOCK(q));
}

/*
 * Set queue fields.
 */
int
strqset(queue_t *q, qfields_t what, unsigned char pri, intptr_t val)
{
	qband_t *qbp = NULL;
	queue_t	*wrq;
	int error = 0;
	kthread_id_t freezer;

	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));

	if (what >= QBAD) {
		error = EINVAL;
		goto done;
	}
	if (pri != 0) {
		int i;
		qband_t **qbpp;

		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					error = EAGAIN;
					goto done;
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;
	}
	switch (what) {

	case QHIWAT:
		if (qbp)
			qbp->qb_hiwat = (size_t)val;
		else
			q->q_hiwat = (size_t)val;
		break;

	case QLOWAT:
		if (qbp)
			qbp->qb_lowat = (size_t)val;
		else
			q->q_lowat = (size_t)val;
		break;

	case QMAXPSZ:
		if (qbp)
			error = EINVAL;
		else
			q->q_maxpsz = (ssize_t)val;

		/*
		 * Performance concern, strwrite looks at the module below
		 * the stream head for the maxpsz each time it does a write
		 * we now cache it at the stream head.  Check to see if this
		 * queue is sitting directly below the stream head.
		 */
		wrq = STREAM(q)->sd_wrq;
		if (q != wrq->q_next)
			break;

		/*
		 * If the stream is not frozen drop the current QLOCK and
		 * acquire the sd_wrq QLOCK which protects sd_qn_*
		 */
		if (freezer != curthread) {
			mutex_exit(QLOCK(q));
			mutex_enter(QLOCK(wrq));
		}
		ASSERT(MUTEX_HELD(QLOCK(wrq)));

		if (strmsgsz != 0) {
			if (val == INFPSZ)
				val = strmsgsz;
			else  {
				if (STREAM(q)->sd_vnode->v_type == VFIFO)
					val = MIN(PIPE_BUF, val);
				else
					val = MIN(strmsgsz, val);
			}
		}
		STREAM(q)->sd_qn_maxpsz = val;
		if (freezer != curthread) {
			mutex_exit(QLOCK(wrq));
			mutex_enter(QLOCK(q));
		}
		break;

	case QMINPSZ:
		if (qbp)
			error = EINVAL;
		else
			q->q_minpsz = (ssize_t)val;

		/*
		 * Performance concern, strwrite looks at the module below
		 * the stream head for the maxpsz each time it does a write
		 * we now cache it at the stream head.  Check to see if this
		 * queue is sitting directly below the stream head.
		 */
		wrq = STREAM(q)->sd_wrq;
		if (q != wrq->q_next)
			break;

		/*
		 * If the stream is not frozen drop the current QLOCK and
		 * acquire the sd_wrq QLOCK which protects sd_qn_*
		 */
		if (freezer != curthread) {
			mutex_exit(QLOCK(q));
			mutex_enter(QLOCK(wrq));
		}
		STREAM(q)->sd_qn_minpsz = (ssize_t)val;

		if (freezer != curthread) {
			mutex_exit(QLOCK(wrq));
			mutex_enter(QLOCK(q));
		}
		break;

	case QSTRUIOT:
		if (qbp)
			error = EINVAL;
		else
			q->q_struiot = (ushort)val;
		break;

	case QCOUNT:
	case QFIRST:
	case QLAST:
	case QFLAG:
		error = EPERM;
		break;

	default:
		error = EINVAL;
		break;
	}
done:
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (error);
}

/*
 * Get queue fields.
 */
int
strqget(queue_t *q, qfields_t what, unsigned char pri, void *valp)
{
	qband_t 	*qbp = NULL;
	int 		error = 0;
	kthread_id_t 	freezer;

	freezer = STREAM(q)->sd_freezer;
	if (freezer == curthread) {
		ASSERT(frozenstr(q));
		ASSERT(MUTEX_HELD(QLOCK(q)));
	} else
		mutex_enter(QLOCK(q));
	if (what >= QBAD) {
		error = EINVAL;
		goto done;
	}
	if (pri != 0) {
		int i;
		qband_t **qbpp;

		if (pri > q->q_nband) {
			qbpp = &q->q_bandp;
			while (*qbpp)
				qbpp = &(*qbpp)->qb_next;
			while (pri > q->q_nband) {
				if ((*qbpp = allocband()) == NULL) {
					error = EAGAIN;
					goto done;
				}
				(*qbpp)->qb_hiwat = q->q_hiwat;
				(*qbpp)->qb_lowat = q->q_lowat;
				q->q_nband++;
				qbpp = &(*qbpp)->qb_next;
			}
		}
		qbp = q->q_bandp;
		i = pri;
		while (--i)
			qbp = qbp->qb_next;
	}
	switch (what) {
	case QHIWAT:
		if (qbp)
			*(size_t *)valp = qbp->qb_hiwat;
		else
			*(size_t *)valp = q->q_hiwat;
		break;

	case QLOWAT:
		if (qbp)
			*(size_t *)valp = qbp->qb_lowat;
		else
			*(size_t *)valp = q->q_lowat;
		break;

	case QMAXPSZ:
		if (qbp)
			error = EINVAL;
		else
			*(ssize_t *)valp = q->q_maxpsz;
		break;

	case QMINPSZ:
		if (qbp)
			error = EINVAL;
		else
			*(ssize_t *)valp = q->q_minpsz;
		break;

	case QCOUNT:
		if (qbp)
			*(size_t *)valp = qbp->qb_count;
		else
			*(size_t *)valp = q->q_count;
		break;

	case QFIRST:
		if (qbp)
			*(mblk_t **)valp = qbp->qb_first;
		else
			*(mblk_t **)valp = q->q_first;
		break;

	case QLAST:
		if (qbp)
			*(mblk_t **)valp = qbp->qb_last;
		else
			*(mblk_t **)valp = q->q_last;
		break;

	case QFLAG:
		if (qbp)
			*(uint *)valp = qbp->qb_flag;
		else
			*(uint *)valp = q->q_flag;
		break;

	case QSTRUIOT:
		if (qbp)
			error = EINVAL;
		else
			*(short *)valp = q->q_struiot;
		break;

	default:
		error = EINVAL;
		break;
	}
done:
	if (freezer != curthread)
		mutex_exit(QLOCK(q));
	return (error);
}

/*
 * Function awakes all in cvwait/sigwait/pollwait, on one of:
 *	QWANTWSYNC or QWANTR or QWANTW,
 *
 * Note: for QWANTWSYNC/QWANTW and QWANTR, if no WSLEEPer or RSLEEPer then a
 *	 deferred wakeup will be done. Also if strpoll() in progress then a
 *	 deferred pollwakeup will be done.
 */
void
strwakeq(queue_t *q, int flag)
{
	stdata_t 	*stp = STREAM(q);
	pollhead_t 	*pl;

	mutex_enter(&stp->sd_lock);
	pl = &stp->sd_pollist;
	if (flag & QWANTWSYNC) {
		ASSERT(!(q->q_flag & QREADR));
		if (stp->sd_flag & WSLEEP) {
			stp->sd_flag &= ~WSLEEP;
			cv_broadcast(&stp->sd_wrq->q_wait);
		} else
			stp->sd_wakeq |= WSLEEP;
		if (stp->sd_sigflags & S_WRNORM)
			strsendsig(stp->sd_siglist, S_WRNORM, 0, 0);
		mutex_exit(&stp->sd_lock);
		pollwakeup(pl, POLLWRNORM);
	} else if (flag & QWANTR) {
		if (stp->sd_flag & RSLEEP) {
			stp->sd_flag &= ~RSLEEP;
			cv_broadcast(&RD(stp->sd_wrq)->q_wait);
		} else
			stp->sd_wakeq |= RSLEEP;
		if (stp->sd_sigflags) {
			if (stp->sd_sigflags & S_INPUT)
				strsendsig(stp->sd_siglist, S_INPUT, 0, 0);
			if (stp->sd_sigflags & S_RDNORM)
				strsendsig(stp->sd_siglist, S_RDNORM, 0, 0);
		}
		mutex_exit(&stp->sd_lock);
		pollwakeup(pl, POLLIN | POLLRDNORM);
	} else {
		if (stp->sd_flag & WSLEEP) {
			stp->sd_flag &= ~WSLEEP;
			cv_broadcast(&stp->sd_wrq->q_wait);
		}
		if (stp->sd_sigflags & S_WRNORM)
			strsendsig(stp->sd_siglist, S_WRNORM, 0, 0);
		mutex_exit(&stp->sd_lock);
		pollwakeup(pl, POLLWRNORM);
	}
}

int
struioput(queue_t *q, mblk_t *mp, struiod_t *dp, int noblock)
{
	stdata_t *stp = STREAM(q);
	int typ  = stp->sd_struiordq->q_struiot;
	uio_t 	*uiop = &dp->d_uio;
	dblk_t	*dbp;
	ssize_t	uiocnt;
	ssize_t	cnt;
	unsigned char *ptr;
	ssize_t	resid;
	int	error = 0;
	int	odd = 0;

	for (; (resid = uiop->uio_resid) > 0 && mp; mp = mp->b_cont) {
		dbp = mp->b_datap;
		ptr = dbp->db_struioptr;
		uiocnt = dbp->db_struiolim - ptr;
		cnt = MIN(uiocnt, uiop->uio_resid);
		if (!(dbp->db_struioflag & STRUIO_SPEC) ||
		    (dbp->db_struioflag & STRUIO_DONE) || cnt == 0)
			/*
			 * Either this mblk has already been processed
			 * or there is no more room in this mblk (?).
			 */
			continue;
		switch (typ) {
		case STRUIOT_STANDARD:
			if (error = uiomove(ptr, cnt, UIO_READ, uiop))
				goto out;
			break;

		case STRUIOT_IP:
			if (dbp->db_struioflag & STRUIO_IP) {
				/*
				 * Some data has already been checksumed
				 * so we must calculate the odd 16 bit
				 * word flag.
				 */
				odd = (ptr - dbp->db_struiobase) & 1;
			}
			/*
			 * Call the IP uiocopyout routine with pointers to
			 * the dblk checksum data (&db_struioun.data[0]),
			 * checksum odd state, and noblock flag.
			 */
			if (error = uioipcopyout(ptr, cnt, uiop,
			    (ushort *)dbp->db_struioun.data, odd, noblock))
				goto out;
			break;

		default:
			error = EIO;
			goto out;
		}
		dbp->db_struioptr += cnt;
		dbp->db_struioflag |= STRUIO_DONE;
	}
out:;
	if (error == EWOULDBLOCK && (resid -= uiop->uio_resid) > 0) {
		/*
		 * A fault has occured and some bytes were moved from the
		 * current mblk, the uio_t has already been updated by
		 * the appropriate uio routine, so also update the mblk
		 * to reflect this in case this same mblk chain is used
		 * again (after the fault has been handled).
		 */
		uiocnt = dbp->db_struiolim - dbp->db_struioptr;
		if (uiocnt >= resid)
			dbp->db_struioptr += resid;
	}
	return (error);
}

int
struioget(queue_t *q, mblk_t *mp, struiod_t *dp, int noblock)
{
	stdata_t *stp = STREAM(q);
	int typ  = stp->sd_struiowrq->q_struiot;
	uio_t	 *uiop = &dp->d_uio;
	dblk_t	 *dbp;
	ssize_t	 uiocnt;
	ssize_t	 cnt;
	unsigned char *ptr;
	ssize_t	 resid;
	int	 error = 0;
	int	 odd;
#ifdef ZC_TEST
	hrtime_t	start;
#endif

	for (; (resid = uiop->uio_resid) > 0 && mp; mp = mp->b_cont) {
		dbp = mp->b_datap;
		ptr = dbp->db_struioptr;
		uiocnt = dbp->db_struiolim - ptr;
		cnt = MIN(uiocnt, uiop->uio_resid);
		if (!(dbp->db_struioflag & STRUIO_SPEC) ||
		    (dbp->db_struioflag & STRUIO_DONE) || cnt == 0) {
			/*
			 * Either this mblk has already been processed
			 * or there is no more room in this mblk (?).
			 */
			continue;
		}
#ifdef ZC_TEST
		if ((cnt & PAGEOFFSET) == 0 && (zcperf & 1))
			start = gethrtime();
		else start = 0ll;
#endif
		switch (typ) {
		case STRUIOT_STANDARD:
			if (error = uiomove(ptr, cnt, UIO_WRITE, uiop))
				goto out;
			break;

		case STRUIOT_IP:
			if (dbp->db_struioflag & STRUIO_IP) {
				/*
				 * Some data has already been checksumed
				 * so we must calculate the odd 16 bit
				 * word flag.
				 */
				odd = (ptr - dbp->db_struiobase) & 1;
			} else {
				/*
				 * Mark mblk as being checksumed and init
				 * the odd 16 bit word flag.
				 */
				odd = 0;
				dbp->db_struioflag |= STRUIO_IP;
			}
			/*
			 * Call the IP uiocopyin routine with pointers to
			 * the dblk checksum data (&db_struioun.data[0]),
			 * checksum odd state, and noblock flag.
			 */
			if (error = uioipcopyin(ptr, cnt, uiop,
			    (ushort *)dbp->db_struioun.data, odd, noblock))
				goto out;
			break;

		default:
			error = EIO;
			goto out;
		}
		dbp->db_struioflag |= STRUIO_DONE;
		dbp->db_struioptr += cnt;
#ifdef ZC_TEST
		if (start) {
			zckstat->zc_count.value.ul++;
			zckstat->zc_hrtime.value.ull += gethrtime() - start;
		}
#endif
	}
out:
	if (error == EWOULDBLOCK && (resid -= uiop->uio_resid) > 0) {
		/*
		 * A fault has occured and some bytes were moved to the
		 * current mblk, the uio_t has already been updated by
		 * the appropriate uio routine, so also update the mblk
		 * to reflect this in case this same mblk chain is used
		 * again (after the fault has been handled).
		 */
		uiocnt = dbp->db_struiolim - dbp->db_struioptr;
		if (uiocnt >= resid)
			dbp->db_struioptr += resid;
	}
	return (error);
}

/*
 * The purpose of rwnext() is to call the rw procedure of the
 * next (downstream) modules queue.
 */
int
rwnext(queue_t *qp, struiod_t *dp)
{
	queue_t		*nqp;
	syncq_t		*sq;
	uint32_t	count;
	uint32_t	flags;
	struct qinit	*qi;
	int		(*proc)();
	struct stdata	*stp;
	int		isread;
	int		rval;

	stp = STREAM(qp);
	flags = qp->q_flag;
	/*
	 * Prevent q_next from changing by holding sd_lock until acquiring
	 * SQLOCK. Note that a read-side rwnext from the streamhead will
	 * already have sd_lock acquired. In either case sd_lock is always
	 * released after acquiring SQLOCK.
	 *
	 * The streamhead read-side holding sd_lock when calling rwnext is
	 * required to prevent a race condition were M_DATA mblks flowing
	 * up the read-side of the stream could be bypassed by a rwnext()
	 * down-call. In this case sd_lock acts as the streamhead perimeter.
	 */
	if ((nqp = WR(qp)) == qp) {
		isread = 0;
		mutex_enter(&stp->sd_lock);
		qp = nqp->q_next;
	} else {
		isread = 1;
		if (nqp != stp->sd_wrq)
			/* Not streamhead */
			mutex_enter(&stp->sd_lock);
		qp = RD(nqp->q_next);
	}
	qi = qp->q_qinfo;
	if (qp->q_struiot == STRUIOT_NONE || ! (proc = qi->qi_rwp)) {
		/*
		 * Not a synchronous module or no r/w procedure for this
		 * queue, so just return EINVAL and let the caller handle it.
		 */
		mutex_exit(&stp->sd_lock);
		return (EINVAL);
	}
	sq = qp->q_syncq;
	mutex_enter(SQLOCK(sq));
	mutex_exit(&stp->sd_lock);
	count = sq->sq_count;
	flags = sq->sq_flags;
	while ((flags & SQ_GOAWAY) || (!(flags & SQ_CIPUT) && count != 0)) {
		/*
		 * Wait until we can enter the inner perimeter.
		 */
		sq->sq_flags = flags | SQ_WANTWAKEUP;
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		count = sq->sq_count;
		flags = sq->sq_flags;
	}
	if (! (flags & SQ_CIPUT))
		sq->sq_flags = flags | SQ_EXCL;
	sq->sq_count = count + 1;
	ASSERT(sq->sq_count != 0);		/* Wraparound */
	/*
	 * Note: The only message ordering guarantee that rwnext() makes is
	 *	 for the write queue flow-control case. All others (r/w queue
	 *	 with q_count > 0 (or q_first != 0)) are the resposibilty of
	 *	 the queue's rw procedure. This could be genralized here buy
	 *	 running the queue's service procedure, but that wouldn't be
	 *	 the most efficent for all cases.
	 */
	mutex_exit(SQLOCK(sq));
	if (! isread && (qp->q_flag & QFULL)) {
		/*
		 * Write queue may be flow controlled. If so,
		 * mark the queue for wakeup when it's not.
		 */
		mutex_enter(QLOCK(qp));
		if (qp->q_flag & QFULL) {
			qp->q_flag |= QWANTWSYNC;
			mutex_exit(QLOCK(qp));
			rval = EWOULDBLOCK;
			goto out;
		}
		mutex_exit(QLOCK(qp));
	}

	rval = (*proc)(qp, dp);
out:
	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	count = sq->sq_count;
	if (flags & (SQ_QUEUED|SQ_WANTWAKEUP|SQ_WANTEXWAKEUP)) {
		putnext_tail(sq, 0, flags, count);
		return (rval);
	}
	ASSERT(count != 0);
	sq->sq_count = count - 1;
	ASSERT(flags & (SQ_EXCL|SQ_CIPUT));
	/*
	 * Safe to always drop SQ_EXCL:
	 *	Not SQ_CIPUT means we set SQ_EXCL above
	 *	For SQ_CIPUT SQ_EXCL will only be set if the put procedure
	 *	did a qwriter(INNER) in which case nobody else
	 *	is in the inner perimeter and we are exiting.
	 */
#ifdef DEBUG
	if ((flags & (SQ_EXCL|SQ_CIPUT)) == (SQ_EXCL|SQ_CIPUT)) {
		ASSERT(sq->sq_count == 0);
	}
#endif DEBUG
	sq->sq_flags = flags & ~SQ_EXCL;
out2:
	mutex_exit(SQLOCK(sq));
	return (rval);
}

/*
 * The purpose of infonext() is to call the info procedure of the next
 * (downstream) modules queue.
 */

int
infonext(queue_t *qp, infod_t *idp)
{
	queue_t		*nqp;
	syncq_t		*sq;
	uint32_t	count;
	uint32_t 	flags;
	struct qinit	*qi;
	int		(*proc)();
	struct stdata	*stp;
	int		rval;

	stp = STREAM(qp);
	flags = qp->q_flag;
	/*
	 * Prevent q_next from changing by holding sd_lock until
	 * acquiring SQLOCK.
	 */
	mutex_enter(&stp->sd_lock);
	if ((nqp = WR(qp)) == qp) {
		qp = nqp->q_next;
	} else {
		qp = RD(nqp->q_next);
	}
	qi = qp->q_qinfo;
	if (qp->q_struiot == STRUIOT_NONE || ! (proc = qi->qi_infop)) {
		mutex_exit(&stp->sd_lock);
		return (EINVAL);
	}
	sq = qp->q_syncq;
	mutex_enter(SQLOCK(sq));
	mutex_exit(&stp->sd_lock);
	count = sq->sq_count;
	flags = sq->sq_flags;
	while ((flags & SQ_GOAWAY) || (!(flags & SQ_CIPUT) && count != 0)) {
		/*
		 * Wait until we can enter the inner perimeter.
		 */
		sq->sq_flags = flags | SQ_WANTWAKEUP;
		cv_wait(&sq->sq_wait, SQLOCK(sq));
		count = sq->sq_count;
		flags = sq->sq_flags;
	}
	if (! (flags & SQ_CIPUT))
		sq->sq_flags = flags | SQ_EXCL;
	sq->sq_count = count + 1;
	ASSERT(sq->sq_count != 0);		/* Wraparound */
	mutex_exit(SQLOCK(sq));

	rval = (*proc)(qp, idp);

	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	count = sq->sq_count;
	if (flags & (SQ_QUEUED|SQ_WANTWAKEUP|SQ_WANTEXWAKEUP)) {
		putnext_tail(sq, 0, flags, count);
		return (rval);
	}
	ASSERT(count != 0);
	sq->sq_count = count - 1;
	ASSERT(flags & (SQ_EXCL|SQ_CIPUT));
	/*
	 * Safe to always drop SQ_EXCL:
	 *	Not SQ_CIPUT means we set SQ_EXCL above
	 *	For SQ_CIPUT SQ_EXCL will only be set if the put procedure
	 *	did a qwriter(INNER) in which case nobody else
	 *	is in the inner perimeter and we are exiting.
	 */
#ifdef DEBUG
	if ((flags & (SQ_EXCL|SQ_CIPUT)) == (SQ_EXCL|SQ_CIPUT)) {
		ASSERT(sq->sq_count == 0);
	}
#endif DEBUG
	sq->sq_flags = flags & ~SQ_EXCL;
	mutex_exit(SQLOCK(sq));
	return (rval);
}

/*
 * Return nonzero if the queue is responsible for struio(), else return 0.
 */
int
isuioq(queue_t *q)
{
	if (q->q_flag & QREADR)
		return (STREAM(q)->sd_struiordq == q);
	else
		return (STREAM(q)->sd_struiowrq == q);
}
