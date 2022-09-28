/*
 * Copyright 1991 NCR Corporation - Dayton, Ohio, USA
 *
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lm_subr.c	1.74	97/11/13 SMI" /* NCR OS2.00.00 1.3 */

/*
 * This is general subroutines used by both the server and client side of
 * the Lock Manager.
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/netconfig.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/cmn_err.h>
#include <sys/pathname.h>
#include <sys/utsname.h>
#include <sys/flock.h>
#include <sys/share.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/rpcb_prot.h>
#include <rpcsvc/sm_inter.h>
#include <nfs/nfs.h>
#include <nfs/nfssys.h>
#include <nfs/nfs_clnt.h>
#include <nfs/rnode.h>
#include <nfs/lm.h>
#include <nfs/lm_server.h>
#include <sys/varargs.h>

/*
 * Definitions and declarations.
 */
struct lm_svc_args	lm_sa;
struct lm_stat		lm_stat;
kmutex_t		lm_lck;
struct			lm_client;	/* forward reference only */

/*
 * For the benefit of warlock.  The "scheme" that protects lm_sa is
 * that it is only modified at initialization in lm_svc().  While multiple
 * kernel threads may modify it simultaneously, they are all writing the
 * same data to it - all other accesses are reads.
 */
#ifndef lint
_NOTE(SCHEME_PROTECTS_DATA("LM svc init", lm_sa))
#endif

static struct lm_sleep *lm_sleeps = NULL;
static struct kmem_cache *lm_sleep_cache = NULL;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_sleeps))
#endif

/*
 * When trying to contact a portmapper that doesn't speak the version we're
 * using, we should theoretically get back RPC_PROGVERSMISMATCH.
 * Unfortunately, some (all?) 4.x hosts return an accept_stat of
 * PROG_UNAVAIL, which gets mapped to RPC_PROGUNAVAIL, so we have to check
 * for that, too.
 */
#define	PMAP_WRONG_VERSION(s)	((s) == RPC_PROGVERSMISMATCH || \
	(s) == RPC_PROGUNAVAIL)

/*
 * Flag to indicate whether the struct kmem_cache's have been created yet.
 * Protected by lm_lck for writing, and sampled only once any time it's read.
 */
bool_t lm_caches_created = FALSE;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_caches_created))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_caches_created))
#endif

/*
 * Function prototypes.
 */
#ifdef LM_DEBUG_SUPPORT
static void lm_hex_byte(int);
#endif /* LM_DEBUG_SUPPORT */
static bool_t lm_same_host(struct knetconfig *config1, struct netbuf *addr1,
		struct knetconfig *config2, struct netbuf *addr2);
static void lm_clnt_destroy(CLIENT **);
static void lm_rel_client(struct lm_client *, int error);
static enum clnt_stat lm_get_client(struct lm_sysid *, rpcprog_t prog,
		rpcvers_t vers, int timout, struct lm_client **, bool_t);
static enum clnt_stat lm_get_pmap_addr(struct knetconfig *, rpcprog_t prog,
		rpcvers_t vers, struct netbuf *);
static enum clnt_stat lm_get_rpc_handle(struct knetconfig *configp,
		struct netbuf *addrp, rpcprog_t prog, rpcvers_t vers,
		int ignore_signals, CLIENT **clientp);
/*
 * Debug routines.
 */
#ifdef LM_DEBUG_SUPPORT

static void
lm_hex_byte(int n)
{
	static char hex[] = "0123456789ABCDEF";
	int i;

	printf(" ");
	if ((i = (n & 0xF0) >> 4) != 0)
		printf("%c", hex[i]);
	printf("%c", hex[n & 0x0F]);
}

/*PRINTFLIKE3*/
void
lm_debug(int level, char *function, const char *fmt, ...)
{
	va_list adx;

	if (lm_sa.debug >= level) {
		printf("%x %s:\t", curthread->t_did, function);
		va_start(adx, fmt);
		vprintf(fmt, adx);
		va_end(adx);
		printf("\n");
	}
}

/*
 * print an alock structure
 *
 * N.B. this is one of the routines that is duplicated for NLM4.  changes
 * maded in this version should be made in the other version as well.
 */
void
lm_alock(int level, char *function, nlm_lock *alock)
{
	int i;

	if (lm_sa.debug >= level) {
		printf("%x %s:\tcaller= %s, svid= %u, offset= %u, len= %u",
			curthread->t_did, function,
			alock->caller_name, alock->svid, alock->l_offset,
			alock->l_len);
		printf("\nfh=");

		for (i = 0;  i < alock->fh.n_len;  i++)
			lm_hex_byte(alock->fh.n_bytes[i]);
		printf("\n");
	}
}

/*
 * print a shareargs structure
 *
 * N.B. this is one of the routines that is duplicated for NLM4.  changes
 * maded in this version should be made in the other version as well.
 */
void
lm_d_nsa(int level, char *function, nlm_shareargs *nsa)
{
	int i;

	if (lm_sa.debug >= level) {
		printf("%x %s:\tcaller= %s, mode= %d, access= %d, reclaim= %d",
			curthread->t_did, function,
			nsa->share.caller_name, nsa->share.mode,
			nsa->share.access, nsa->reclaim);
		printf("\nfh=");

		for (i = 0;  i < nsa->share.fh.n_len;  i++)
			lm_hex_byte(nsa->share.fh.n_bytes[i]);
		printf("\noh=");

		for (i = 0;  i < nsa->share.oh.n_len;  i++)
			lm_hex_byte(nsa->share.oh.n_bytes[i]);
		printf("\n");
	}
}

void
lm_n_buf(int level, char *function, char *name, struct netbuf *addr)
{
	int	i;

	if (lm_sa.debug >= level) {
		printf("%x %s:\t%s=", curthread->t_did, function, name);
		if (! addr)
			printf("(NULL)\n");
		else {
			for (i = 0;  i < addr->len;  i++)
				lm_hex_byte(addr->buf[i]);
			printf(" (%d)\n", addr->maxlen);
		}
	}
}

/*
 * print an alock structure
 *
 * N.B. this is one of the routines that is duplicated for NLM4.  changes
 * maded in this version should be made in the other version as well.
 */
void
lm_alock4(int level, char *function, nlm4_lock *alock)
{
	int i;

	if (lm_sa.debug >= level) {
		printf("%x %s:\tcaller= %s, svid= %u, offset= %llu, len= %llu",
			curthread->t_did, function,
			alock->caller_name, alock->svid, alock->l_offset,
			alock->l_len);
		printf("\nfh=");

		for (i = 0; i < alock->fh.n_len; i++)
			lm_hex_byte(alock->fh.n_bytes[i]);
		printf("\n");
	}
}

/*
 * print a shareargs structure
 *
 * N.B. this is one of the routines that is duplicated for NLM4.  changes
 * maded in this version should be made in the other version as well.
 */
void
lm_d_nsa4(int level, char *function, nlm4_shareargs *nsa)
{
	int i;

	if (lm_sa.debug >= level) {
		printf("%x %s:\tcaller= %s, mode= %d, access= %d, reclaim= %d",
			curthread->t_did, function,
			nsa->share.caller_name, nsa->share.mode,
			nsa->share.access, nsa->reclaim);
		printf("\nfh=");

		for (i = 0;  i < nsa->share.fh.n_len;  i++)
			lm_hex_byte(nsa->share.fh.n_bytes[i]);
		printf("\noh=");

		for (i = 0;  i < nsa->share.oh.n_len;  i++)
			lm_hex_byte(nsa->share.oh.n_bytes[i]);
		printf("\n");
	}
}
#endif /*  LM_DEBUG_SUPPORT */

/*
 * Utilities
 */

/*
 * Append the number n to the string str.
 */
void
lm_numcat(char *str, int n)
{
	char d[12];
	int  i = 0;

	while (*str++)
		;
	str--;
	while (n > 9) {
		d[i++] = n % 10;
		n /= 10;
	}
	d[i] = n;

	do {
		*str++ = "0123456789"[d[i]];
	} while (i--);
	*str = 0;
}

/*
 * A useful utility.
 */
char *
lm_dup(char *str, size_t len)
{
	char *s = kmem_zalloc(len, KM_SLEEP);
	bcopy(str, s, len);
	return (s);
}

/*
 * Fill in the given byte array with the owner handle that corresponds to
 * the given pid.  If you change this code, be sure to update LM_OH_LEN.
 */
void
lm_set_oh(char *array, size_t array_len, pid_t pid)
{
	char *ptr;

	ASSERT(array_len >= LM_OH_LEN);
	ptr = array;
	bcopy(&pid, ptr, sizeof (pid));
	ptr += sizeof (pid);
	bcopy(&lm_owner_handle_sys, ptr, LM_OH_SYS_LEN);
}

/*
 * Clean up and exit.
 *
 * XXX NCR porting issues:
 *	1. This routine should go away.  The only place it's called, lm_svc(),
 *		should return a failure code instead of `exiting.'
 */
void
lm_exit(void)
{
	LM_DEBUG((2, "lm_exit", "Exiting\n"));

	exit(CLD_EXITED, 0);
}

/*
 * Returns a reference to the lm_sysid for myself.  This is a loopback
 * kernel RPC endpoint used to talk with our own statd.
 */
struct lm_sysid *
lm_get_me(void)
{
	int error;
	struct knetconfig config;
	struct netbuf addr;
	char keyname[SYS_NMLN + 16];
	struct vnode *vp;
	struct lm_sysid *ls;
	static bool_t found_ticlts = TRUE;

	config.knc_semantics = NC_TPI_CLTS;
	config.knc_protofmly = NC_LOOPBACK;
	config.knc_proto = NC_NOPROTO;
	error = lookupname("/dev/ticlts", UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (error != 0) {
		/*
		 * This can fail if /dev/ticlts is not properly
		 * configured, or (more likely) if the process
		 * has invoked chroot() and there is no ticlts
		 * in the new hierarchy.  This means our statd
		 * is out of touch for now.
		 *
		 * `found_ticlts' simply prevents us from printing
		 * the warning message more often than would be
		 * friendly.  It isn't critical enough to warrant
		 * mutex protection.
		 */
		if (found_ticlts == TRUE) {
			found_ticlts = FALSE;
			nfs_cmn_err(error, CE_WARN,
			"lockd: can't get loopback transport (%m), continuing");
		}

		return ((struct lm_sysid *)NULL);
	}

	found_ticlts = TRUE;

	config.knc_rdev = vp->v_rdev;
	VN_RELE(vp);

	/*
	 * Get a unique (node,service) name from which we
	 * build up a netbuf.
	 */
	(void) strcpy(keyname, utsname.nodename);
	(void) strcat(keyname, ".");
	keyname[strlen(utsname.nodename) + 1] = NULL;	/* is strcat broken? */
	addr.buf = keyname;
	addr.len = addr.maxlen = (unsigned int)strlen(keyname);

	LM_DEBUG((8, "get_me", "addr = %s", addr.buf));

	rw_enter(&lm_sysids_lock, RW_READER);
	ls = lm_get_sysid(&config, &addr, "me", TRUE, NULL);
	rw_exit(&lm_sysids_lock);

	if (ls == NULL)
		cmn_err(CE_PANIC, "lm_get_me: cached entry not found");

	return (ls);
}

/*
 * Both config and addr must be the same.
 * Comparison of addr's must be done config dependent.
 */
static bool_t
lm_same_host(struct knetconfig *config1, struct netbuf *addr1,
		struct knetconfig *config2, struct netbuf *addr2)
{
	struct sockaddr_in *si1;
	struct sockaddr_in *si2;
	int namelen1, namelen2;
	char *dot;

	lm_n_buf(9, "same_host", "addr1", addr1);
	lm_n_buf(9, "same_host", "addr2", addr2);
	LM_DEBUG((9, "same_host", "fmly1= %s, rdev1= %lx, "
	    "fmly2= %s, rdev2= %lx",
	    config1->knc_protofmly, config1->knc_rdev,
	    config2->knc_protofmly, config2->knc_rdev));

	if (strcmp(config1->knc_protofmly, config2->knc_protofmly) != 0 ||
	    strcmp(config1->knc_proto, config2->knc_proto) != 0 ||
	    config1->knc_rdev != config2->knc_rdev) {
		return (FALSE);
	}

	if (strcmp(config1->knc_protofmly, NC_INET) == 0) {
		si1 = (struct sockaddr_in *)(addr1->buf);
		si2 = (struct sockaddr_in *)(addr2->buf);
		if (si1->sin_family != si2->sin_family ||
		    si1->sin_addr.s_addr != si2->sin_addr.s_addr) {
			return (FALSE);
		}
	} else if (strcmp(config1->knc_protofmly, NC_LOOPBACK) == 0) {
		dot = strnrchr(addr1->buf, '.', addr1->len);
		ASSERT(dot != NULL);
		namelen1 = (int)(dot - addr1->buf);
		dot = strnrchr(addr2->buf, '.', addr2->len);
		ASSERT(dot != NULL);
		namelen2 = (int)(dot - addr2->buf);
		if (namelen1 != namelen2 || namelen1 <= 0 ||
		    strncmp(addr1->buf, addr2->buf, namelen1)) {
			return (FALSE);
		}
	} else {
		LM_DEBUG((1, "same_host", "UNSUPPORTED PROTO FAMILY %s",
		    config1->knc_protofmly));
		return (FALSE);
	}

	return (TRUE);
}

/*
 * Returns non-zero if the two netobjs have the same contents, zero if they
 * do not.
 */
int
lm_netobj_eq(netobj *obj1, netobj *obj2)
{
	if (obj1->n_len != obj2->n_len)
		return (0);
	/*
	 * Lengths are equal if we get here. Thus if obj1->n_len == 0, then
	 * obj2->n_len == 0. If both lengths are 0, the objects are
	 * equal.
	 */
	if (obj1->n_len == 0)
		return (1);
	return (bcmp(obj1->n_bytes, obj2->n_bytes, obj1->n_len) == 0);
}

/*
 * The lm_sysids list is a cache of system IDs for which we have built RPC
 * client handles (either as client or server).  See the definition of
 * struct lm_sysid for more details.
 */
struct lm_sysid *lm_sysids = NULL;
krwlock_t lm_sysids_lock;
static struct kmem_cache *lm_sysid_cache = NULL;
unsigned int lm_sysid_len = 0;

#ifndef lint
_NOTE(RWLOCK_PROTECTS_DATA(lm_sysids_lock, lm_sysids))
_NOTE(RWLOCK_PROTECTS_DATA(lm_sysids_lock, lm_sysid_len))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_sysid_cache))
#endif

/*
 * lm_get_sysid
 * Returns a reference to the sysid associated with the knetconfig
 * and address.
 *
 * If name == NULL then set name to "NoName".
 * If alloc == FALSE and the entry can't be found in the cache, don't
 * allocate a new one; panic instead (caller says it must be there.)
 * Callers that pass alloc == FALSE expect that lm_sysids_lock cannot
 * be dropped without panicking.
 * If dropped_read_lock != NULL, *dropped_read_lock is set to TRUE
 * if the lock was dropped else FALSE.  If dropped_read_lock == NULL,
 * there is no way to convey this information to the caller (which
 * therefore must assume that lm_sysids may be changed after the call.)
 *
 * All callers must hold lm_sysids_lock as a reader.  If alloc is TRUE
 * and the entry can't be found, this routine upgrades the lock to a
 * writer lock, inserts the entry into the list, and downgrades the
 * lock back to a reader lock.
 *
 * The list assumed to be sorted with the highest lm_sysid at the head of the
 * list. This is crucial for finding an available lm_sysid when one needs
 * to be allocated.
 */
struct lm_sysid *
lm_get_sysid(struct knetconfig *config, struct netbuf *addr, char *name,
	bool_t alloc, bool_t *dropped_read_lock)
{
	struct lm_sysid *ls, *insert_after;
	sysid_t	unused;
	bool_t writer = FALSE;

	LM_DEBUG((3, "get_sysid", "config= %p addr= %p name= %s alloc= %s",
	    (void *)config, (void *)addr, name,
	    ((alloc == TRUE) ? "TRUE" : "FALSE")));

	/*
	 * We can't verify that caller has lm_sysids_lock as a reader the
	 * way we'd like to, but at least we can assert that somebody does.
	 */
	ASSERT(RW_READ_HELD(&lm_sysids_lock));
	if (dropped_read_lock != NULL)
		*dropped_read_lock = FALSE;

	/*
	 * Try to find an existing lm_sysid that contains what we're
	 * looking for. While we're searching, look for an unused sysid
	 * value so that we can use it in a new lm_sysid if we need
	 * it.
	 */
start:
	unused = LM_SYSID_MAX;
	insert_after = NULL;
	for (ls = lm_sysids; ls; ls = ls->next) {
		mutex_enter(&ls->lock);
		ASSERT(ls->refcnt >= 0);

		if (lm_same_host(config, addr, &ls->config, &ls->addr)) {
			ls->refcnt++;
			mutex_exit(&ls->lock);
			if (writer == TRUE) {
				rw_downgrade(&lm_sysids_lock);
			}
			return (ls);
		} else {
			mutex_exit(&ls->lock);
		}
		/*
		 * the list is ordered from highest to lowest. If we find
		 * an unused item then store the value for below. If this
		 * variable ever hits LM_SYSID -1 then we are out of ids.
		 */
		if (ls->sysid == unused) {
			unused--;
			/*
			 * always need to have a pointer to the previous
			 * item since this is not a doubly linked list we
			 * need a pointer so we can add a new record into
			 * the list.
			 */
			insert_after = ls;
		}
	}

	if (alloc == FALSE)
		cmn_err(CE_PANIC, "lm_get_sysid: cached entry not found");

	/*
	 * It's necessary to get write access to the lm_sysids list here.
	 * Since we already own a READER lock, we acquire the WRITER lock
	 * with some care.
	 *
	 * In particular, if we fail to upgrade to writer immediately, there
	 * is already a writer or there are one or more other threads that
	 * are already blocking waiting to become writers.  In this case,
	 * we wait to acquire the writer lock, re-search the list,
	 * and then add our new entry onto the list.  The next time past
	 * here we're already a writer, so we skip this stuff altogether.
	 */
	if (writer == FALSE) {
		if (rw_tryupgrade(&lm_sysids_lock) == 0) {
			rw_exit(&lm_sysids_lock);
			if (dropped_read_lock != NULL) {
				*dropped_read_lock = TRUE;
			}
			rw_enter(&lm_sysids_lock, RW_WRITER);
			writer = TRUE;
			goto start;
		} else {
			if (dropped_read_lock != NULL) {
				*dropped_read_lock = FALSE;
			}
		}
	}

	/*
	 * search for an available sysid did not find one. Return this fact.
	 */
	if (unused == (LM_SYSID - 1)) {
		cmn_err(CE_WARN, "lm_get_sysid: all sysids are in use.");
		ls = NULL;
		goto no_ids;
	}

	/*
	 * We have acquired the WRITER lock.  Create and add a
	 * new lm_sysid to the list.
	 */
	ls = kmem_cache_alloc(lm_sysid_cache, KM_SLEEP);
	mutex_init(&ls->lock, NULL, MUTEX_DEFAULT, NULL);
	ls->config.knc_semantics = config->knc_semantics;
	ls->config.knc_protofmly = lm_dup(config->knc_protofmly,
	    strlen(config->knc_protofmly) + 1);
	ls->config.knc_proto = (config->knc_proto ?
	    lm_dup(config->knc_proto, strlen(config->knc_proto) + 1) :
	    NULL);
	ls->config.knc_rdev = config->knc_rdev;
	ls->addr.buf = lm_dup(addr->buf, addr->maxlen);
	ls->addr.len = addr->len;
	ls->addr.maxlen = addr->maxlen;
	ls->name = name ? lm_dup(name, strlen(name) + 1) : "NoName";
	ls->sysid = unused;
	ls->sm_client = FALSE;
	ls->sm_server = FALSE;
	ls->sm_state = 0;
	ls->in_recovery = FALSE;
	ls->refcnt = 1;
	if (insert_after == NULL) {
		ls->next = lm_sysids;
		lm_sysids = ls;
	} else {
		ls->next = insert_after->next;
		insert_after->next = ls;
	}
	lm_sysid_len++;

	LM_DEBUG((3, "get_sysid", "name= %s, sysid= %x, sysids= %d",
	    ls->name, ls->sysid, lm_sysid_len));
	LM_DEBUG((3, "get_sysid", "semantics= %d protofmly= %s proto= %s",
	    ls->config.knc_semantics, ls->config.knc_protofmly,
	    ls->config.knc_proto));

	/*
	 * Make sure we return still holding just the READER lock.
	 */
no_ids:
	rw_downgrade(&lm_sysids_lock);
	return (ls);
}

/*
 * Increment the reference count for an lm_sysid.
 */

void
lm_ref_sysid(struct lm_sysid *ls)
{
	mutex_enter(&ls->lock);

	/*
	 * Most callers should already have a reference to the lm_sysid.
	 * Some routines walk the lm_sysids list, though, in which case the
	 * reference count could be zero.
	 */
	ASSERT(ls->refcnt >= 0);

	ls->refcnt++;

	mutex_exit(&ls->lock);
}

/*
 * Release the reference to an lm_sysid.  If the reference count goes to
 * zero, the lm_sysid is left around in case it will be used later.
 */

void
lm_rel_sysid(struct lm_sysid *ls)
{
	mutex_enter(&ls->lock);

	ls->refcnt--;
	ASSERT(ls->refcnt >= 0);

	mutex_exit(&ls->lock);
}

/*
 * Try to free all the lm_sysid's that we have registered.  In-use entries
 * are left alone.
 */
/*ARGSUSED*/
void
lm_free_sysid_table(void *cdrarg)
{
	struct lm_sysid *ls;
	struct lm_sysid *nextls = NULL;
	struct lm_sysid *prevls = NULL;	/* previous kept element */
	int chklck;

	/*
	 * Free all the lm_client's that are unused.  This must be done
	 * first, so that they drop their references to the lm_sysid's.
	 */
	lm_flush_clients_mem(NULL);

	rw_enter(&lm_sysids_lock, RW_WRITER);

	LM_DEBUG((5, "free_sysid", "start length: %d\n", lm_sysid_len));

	chklck = FLK_QUERY_ACTIVE | FLK_QUERY_SLEEPING;

	for (ls = lm_sysids; ls != NULL; ls = nextls) {
		mutex_enter(&ls->lock);
		ASSERT(ls->refcnt >= 0);
		nextls = ls->next;

		if (ls->refcnt > 0 || flk_sysid_has_locks(ls->sysid, chklck) ||
		    flk_sysid_has_locks(ls->sysid | LM_SYSID_CLIENT, chklck) ||
#if 0 /* notyet */
		    lm_shr_sysid_has_locks(ls->sysid)) {
#else
		    0) {
#endif
			/* can't free now */
			LM_DEBUG((6, "free_sysid", "%x (%s) kept (ref %d)\n",
			    ls->sysid, ls->name, ls->refcnt));
			prevls = ls;
			mutex_exit(&ls->lock);
		} else {
			LM_DEBUG((6, "free_sysid", "%x (%s) freed\n",
			    ls->sysid, ls->name));
			if (prevls == NULL) {
				lm_sysids = nextls;
			} else {
				prevls->next = nextls;
			}
			ASSERT(lm_sysid_len != 0);
			--lm_sysid_len;
			kmem_free(ls->config.knc_protofmly,
			    strlen(ls->config.knc_protofmly) + 1);
			kmem_free(ls->config.knc_proto,
			    strlen(ls->config.knc_proto) + 1);
			kmem_free(ls->addr.buf, ls->addr.maxlen);
			kmem_free(ls->name, strlen(ls->name) + 1);
			mutex_exit(&ls->lock);
			mutex_destroy(&ls->lock);
			kmem_cache_free(lm_sysid_cache, ls);
		}
	}

	LM_DEBUG((5, "free_sysid", "end length: %d\n", lm_sysid_len));
	rw_exit(&lm_sysids_lock);
}

/*
 * lm_configs is a null-terminated (next == NULL) list.
 * lm_numconfigs is the number of elements in the list.
 */
static struct lm_config *lm_configs = NULL;
unsigned int lm_numconfigs = 0;
static struct kmem_cache *lm_config_cache = NULL;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_configs))
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_numconfigs))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_numconfigs))
#endif

/*
 * Number of outstanding NLM requests.
 */
unsigned int lm_num_outstanding = 0;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_num_outstanding))
_NOTE(DATA_READABLE_WITHOUT_LOCK(lm_num_outstanding))
#endif

/*
 * Save the knetconfig information that was passed down to us from
 * userland during the _nfssys(LM_SVC) call.  This is used by the server
 * code ONLY to map fp -> config.
 */
struct lm_config *
lm_saveconfig(struct file *fp, struct knetconfig *config)
{
	struct lm_config *ln;

	LM_DEBUG((7, "saveconfig",
	    "fp= %p semantics= %d protofmly= %s proto= %s",
	    (void *)fp, config->knc_semantics, config->knc_protofmly,
	    config->knc_proto));

	mutex_enter(&lm_lck);
	for (ln = lm_configs; ln; ln = ln->next) {
		if (ln->fp == fp) {	/* happens with multiple svc threads */
			mutex_exit(&lm_lck);
			LM_DEBUG((7, "saveconfig", "found ln= %p", (void *)ln));
			return (ln);
		}
	}

	ln = kmem_cache_alloc(lm_config_cache, KM_SLEEP);
	ln->fp = fp;
	ln->config = *config;
	ln->next = lm_configs;
	lm_configs = ln;
	++lm_numconfigs;

	LM_DEBUG((7, "saveconfig", "ln= %p fp= %p next= %p",
	    (void *)ln, (void *)fp, (void *)ln->next));
	mutex_exit(&lm_lck);
	return (ln);
}

/*
 * Fetch lm_config corresponding to an fp.  Server code only.
 */
struct lm_config *
lm_getconfig(struct file *fp)
{
	struct lm_config *ln;

	LM_DEBUG((7, "getconfig", "fp= %p", (void *)fp));

	mutex_enter(&lm_lck);
	for (ln = lm_configs; ln != NULL; ln = ln->next) {
		if (ln->fp == fp)
			break;
	}
	mutex_exit(&lm_lck);

	LM_DEBUG((7, "getconfig", "ln= %p", (void *)ln));
	return (ln);
}

/*
 * Remove an entry from the config table and decrement the config count.
 * This routine does not return the number of remaining entries, because
 * the caller probably wants to check it while holding lm_lck.
 */
void
lm_rmconfig(struct file *fp)
{
	struct lm_config *ln;
	struct lm_config *prev_ln;

	LM_DEBUG((7, "rmconfig", "fp=%p", (void *)fp));

	mutex_enter(&lm_lck);

	for (ln = lm_configs, prev_ln = NULL; ln != NULL; ln = ln->next) {
		if (ln->fp == fp)
			break;
		prev_ln = ln;
	}
	if (ln == NULL) {
#ifdef DEBUG
		cmn_err(CE_WARN,
		    "lm_rmconfig: couldn't find config for fp %p", (void *)fp);
#endif
	} else {
		LM_DEBUG((7, "rmconfig", "ln=%p", (void *)ln));
		if (prev_ln == NULL) {
			lm_configs = ln->next;
		} else {
			prev_ln->next = ln->next;
		}
		--lm_numconfigs;
		/*
		 * no need to call lm_clear_knetconfig, because all of its
		 * strings are statically allocated.
		 */
		kmem_cache_free(lm_config_cache, ln);
	}

	mutex_exit(&lm_lck);
}

/*
 * When an entry in the lm_client cache is released, it is just marked
 * unused.  If space is tight, it can be freed later.  Because client
 * handles are potentially expensive to keep around, we try to reuse old
 * lm_client entries only if there are lm_max_clients entries or more
 * allocated.
 *
 * If time == 0, the client handle is not valid.
 */
struct lm_client {
	struct lm_client *next;
	struct lm_sysid *sysid;
	rpcprog_t prog;
	rpcvers_t vers;
	struct netbuf addr;	/* Address to this <prog,vers> on sysid */
	time_t time;		/* In seconds */
	CLIENT *client;
	bool_t in_use;
};

/*
 * For the benefit of warlock.  The "scheme" that protects lm_client::in_use
 * is that it is only sampled or set to TRUE while lm_lck is held, and can be
 * set to FALSE without holding the lock - sort of a reference flag, rather
 * than a count.
 *
 * All other members of lm_client are protected by the `in_use' reference
 * flag itself.  lm_async works the same way.
 */
#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_client::next))
_NOTE(SCHEME_PROTECTS_DATA("LM in_use", lm_client::in_use))
_NOTE(SCHEME_PROTECTS_DATA("LM ref flag",
				lm_client::{sysid prog vers addr time client}))
#endif

static struct lm_client *lm_clients = NULL;
int lm_max_clients = 10;
static struct kmem_cache *lm_client_cache = NULL;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_clients))
_NOTE(READ_ONLY_DATA(lm_max_clients))
#endif

/*
 * We need an version of CLNT_DESTROY which also frees the auth structure.
 */
static void
lm_clnt_destroy(CLIENT **clp)
{
	if (*clp) {
		if ((*clp)->cl_auth) {
			AUTH_DESTROY((*clp)->cl_auth);
			(*clp)->cl_auth = NULL;
		}
		CLNT_DESTROY(*clp);
		*clp = NULL;
	}
}

/*
 * Release this lm_client entry.
 * Do also destroy the entry if there was an error != EINTR,
 * and mark the entry as not-valid, by setting time=0.
 */
static void
lm_rel_client(struct lm_client *lc, int error)
{
	LM_DEBUG((7, "rel_clien", "addr = (%p, %d %d)\n",
	    (void *)lc->addr.buf, lc->addr.len, lc->addr.maxlen));
	if (error && error != EINTR) {
		LM_DEBUG((7, "rel_clien", "destroying addr = (%p, %d %d)\n",
		    (void *)lc->addr.buf, lc->addr.len, lc->addr.maxlen));
		lm_clnt_destroy(&lc->client);
		if (lc->addr.buf) {
			kmem_free(lc->addr.buf, lc->addr.maxlen);
			lc->addr.buf = NULL;
		}
		lc->time = 0;
	}
	lc->in_use = FALSE;
}

/*
 * Return a lm_client to the <ls,prog,vers>.
 * The lm_client found is marked as in_use.
 * It is the responsibility of the caller to release the lm_client by
 * calling lm_rel_client().
 *
 * Returns:
 * RPC_SUCCESS		Success.
 * RPC_CANTSEND		Temporarily cannot send to this sysid.
 * RPC_TLIERROR		Unspecified TLI error.
 * RPC_UNKNOWNPROTO	ls->config is from an unrecognised protocol family.
 * RPC_PROGNOTREGISTERED The NLM prog `prog' isn't registered on the server.
 * RPC_RPCBFAILURE	Couldn't contact portmapper on remote host.
 * Any unsuccessful return codes from CLNT_CALL().
 */
static enum clnt_stat
lm_get_client(struct lm_sysid *ls, rpcprog_t prog, rpcvers_t vers, int timout,
		struct lm_client **lcp, bool_t ignore_signals)
{
	struct lm_client *lc = NULL;
	struct lm_client *lc_old = NULL;
	enum clnt_stat status = RPC_SUCCESS;

	mutex_enter(&lm_lck);

	/*
	 * Prevent client-side lock manager from obtaining handles to a
	 * server during its crash recovery, e.g. so that one of our client
	 * processes can't attempt to release a lock that the kernel is
	 * busy trying to reclaim for him.
	 */
	if (ls->in_recovery && ttolwp(curthread) != NULL) {
		mutex_exit(&lm_lck);
		LM_DEBUG((7, "get_client",
		    "ls= %p not used during crash recovery of %s", (void *)ls,
		    ls->name));
		status = RPC_CANTSEND;
		goto out;
	}

	/*
	 * Search for an lm_client that is free, valid, and matches.
	 */
	for (lc = lm_clients; lc; lc = lc->next) {
		if (! lc->in_use) {
			if (lc->time && lc->sysid == ls && lc->prog  == prog &&
			    lc->vers  == vers) {
				/* Found a valid and matching lm_client. */
				break;
			} else if ((! lc_old) || (lc->time < lc_old->time)) {
				/* Possibly reuse this one. */
				lc_old = lc;
			}
		}
	}

	LM_DEBUG((7, "get_client", "Found lc= %p, lc_old= %p, timout= %d",
	    (void *)lc, (void *)lc_old, timout));

	if (! lc) {
		/*
		 * We did not find an entry to use.
		 * Decide if we should reuse lc_old or create a new entry.
		 */
		if ((! lc_old) || (lm_stat.client_len < lm_max_clients)) {
			/*
			 * No entry to reuse, or we are allowed to create
			 * extra.
			 */
			lm_stat.client_len++;
			lc = kmem_cache_alloc(lm_client_cache, KM_SLEEP);
			lc->time = 0;
			lc->client = NULL;
			lc->next = lm_clients;
			lc->sysid = NULL;
			lm_clients = lc;
		} else {
			lm_rel_client(lc_old, EINVAL);
			lc = lc_old;
		}

		/*
		 * Update the sysid reference counts.  We get the new
		 * reference before dropping the old one in case they're
		 * the same.  This is to prevent the ref count from going
		 * to zero, which could make the sysid vanish.
		 */
		lm_ref_sysid(ls);
		if (lc->sysid != NULL)
			lm_rel_sysid(lc->sysid);
		lc->sysid = ls;

		lc->prog = prog;
		lc->vers = vers;
		lc->addr.buf = lm_dup(ls->addr.buf, ls->addr.maxlen);
		lc->addr.len = ls->addr.len;
		lc->addr.maxlen = ls->addr.maxlen;
	}
	lc->in_use = TRUE;

	mutex_exit(&lm_lck);

	lm_n_buf(7, "get_client", "addr", &lc->addr);

	/*
	 * If timout == 0 then one way RPC calls are used, and the CLNT_CALL
	 * will always return RPC_TIMEDOUT. Thus we will never know whether
	 * a client handle is still OK. Therefore don't use the handle if
	 * time is older than lm_sa.timout. Note, that lm_sa.timout == 0
	 * disables the client cache for one way RPC-calls.
	 */
	if (timout == 0) {
		if (lm_sa.timout <= time - lc->time) {	/* Invalidate? */
			lc->time = 0;
		}
	}

	if (lc->time == 0) {
		status = lm_get_rpc_handle(&ls->config, &lc->addr, prog, vers,
					ignore_signals, &lc->client);
		if (status != RPC_SUCCESS)
			goto out;
		lc->time = time;
	} else {
		/*
		 * Consecutive calls to CLNT_CALL() on the same client handle
		 * get the same transaction ID.  We want a new xid per call,
		 * so we first reinitialise the handle.
		 */
		(void) clnt_tli_kinit(lc->client, &ls->config, &lc->addr,
				lc->addr.maxlen, 0, CRED());
	}

out:
	LM_DEBUG((7, "get_client",
	    "End: lc= %p status= %d, time= %lx, client= %p, clients= %d",
	    (void *)lc, status,
	    (lc ? lc->time : -1),
	    (void *)(lc ? lc->client : NULL),
	    lm_stat.client_len));

	if (status == RPC_SUCCESS) {
		*lcp = lc;
	} else {
		if (lc) {
			mutex_enter(&lm_lck);
			lm_rel_client(lc, EINVAL);
			mutex_exit(&lm_lck);
		}
		*lcp = NULL;
	}

	return (status);
}

/*
 * Get the RPC client handle to talk to the service at addrp.
 * Returns:
 * RPC_SUCCESS		Success.
 * RPC_RPCBFAILURE	Couldn't talk to the remote portmapper (e.g.,
 * 			timeouts).
 * RPC_INTR		Caught a signal before we could successfully return.
 * RPC_TLIERROR		Couldn't initialize the handle after talking to the
 * 			remote portmapper (shouldn't happen).
 */

static enum clnt_stat
lm_get_rpc_handle(struct knetconfig *configp, struct netbuf *addrp,
		rpcprog_t prog, rpcvers_t vers, int ignore_signals,
		CLIENT **clientp /* OUT */)
{
	enum clnt_stat status;
	k_sigset_t oldmask;
	k_sigset_t newmask;
	int error;

	/*
	 * It's not clear whether this function should have a retry loop,
	 * as long as things like a portmapper timeout cause the
	 * higher-level code to retry.
	 */

	/*
	 * Try to get the address from either portmapper or rpcbind.
	 * We check for posted signals after trying and failing to
	 * contact the portmapper since it can take uncomfortably
	 * long for this entire procedure to time out.
	 */
	status = lm_get_pmap_addr(configp, prog, vers, addrp);
	if (IS_UNRECOVERABLE_RPC(status) &&
	    status != RPC_UNKNOWNPROTO &&
	    !PMAP_WRONG_VERSION(status)) {
		status = RPC_RPCBFAILURE;
		goto bailout;
	}

	if (!ignore_signals && lm_sigispending()) {
		LM_DEBUG((7, "get_rpc_handle", "posted signal, RPC stat= %d",
		    status));
		status = RPC_INTR;
		goto bailout;
	}

	if (status != RPC_SUCCESS) {
		status = rpcbind_getaddr(configp, prog, vers, addrp);
		if (status != RPC_SUCCESS) {
			LM_DEBUG((7, "get_rpc_handle",
			    "can't contact portmapper or rpcbind"));
			status = RPC_RPCBFAILURE;
			goto bailout;
		}
	}

	lm_clnt_destroy(clientp);

	/*
	 * Mask signals for the duration of the handle creation,
	 * allowing relatively normal operation with a signal
	 * already posted to our thread (e.g., when we are
	 * sending an NLM_CANCEL in response to catching a signal).
	 *
	 * Any further exit paths from this routine must restore
	 * the original signal mask.
	 */
	sigfillset(&newmask);
	sigreplace(&newmask, &oldmask);
	if ((error = clnt_tli_kcreate(configp, addrp, prog,
			vers, 0, 0, CRED(), clientp)) != 0) {
		status = RPC_TLIERROR;
		sigreplace(&oldmask, (k_sigset_t *)NULL);
		LM_DEBUG((7, "get_client", "kcreate(prog) returned %d", error));
		goto bailout;
	}
	(*clientp)->cl_nosignal = 1;
	sigreplace(&oldmask, (k_sigset_t *)NULL);

bailout:
	return (status);
}

/*
 * Try to get the address for the desired service by using the old
 * portmapper protocol.  Ignores signals.
 *
 * Returns RPC_UNKNOWNPROTO if the request uses the loopback transport.
 * Use lm_get_rpcb_addr instead.
 */

static enum clnt_stat
lm_get_pmap_addr(struct knetconfig *config, rpcprog_t prog, rpcvers_t vers,
		struct netbuf *addr)
{
	u_short	port = 0;
	int error;
	enum clnt_stat status;
	CLIENT *client = NULL;
	struct pmap parms;
	struct timeval tmo;
	k_sigset_t oldmask;
	k_sigset_t newmask;

	/*
	 * Call rpcbind version 2 or earlier (SunOS portmapper, remote
	 * only) to get an address we can use in an RPC client handle.
	 * We simply obtain a port no. for <prog, vers> and plug it
	 * into `addr'.
	 */
	if (strcmp(config->knc_protofmly, NC_INET) == 0) {
		put_inet_port(addr, htons(PMAPPORT));
	} else {
		LM_DEBUG((7, "get_pmap_addr", "unsupported protofmly %s",
		    config->knc_protofmly));
		status = RPC_UNKNOWNPROTO;
		goto out;
	}

	LM_DEBUG((7, "get_pmap_addr", "semantics= %d, protofmly= %s, proto= %s",
	    config->knc_semantics, config->knc_protofmly,
	    config->knc_proto));
	lm_n_buf(7, "get_pmap_addr", "addr", addr);

	/*
	 * Mask signals for the duration of the handle creation and
	 * RPC call.  This allows relatively normal operation with a
	 * signal already posted to our thread (e.g., when we are
	 * sending an NLM_CANCEL in response to catching a signal).
	 *
	 * Any further exit paths from this routine must restore
	 * the original signal mask.
	 */
	sigfillset(&newmask);
	sigreplace(&newmask, &oldmask);

	if ((error = clnt_tli_kcreate(config, addr, PMAPPROG,
			PMAPVERS, 0, 0, CRED(), &client)) != RPC_SUCCESS) {
		status = RPC_TLIERROR;
		sigreplace(&oldmask, (k_sigset_t *)NULL);
		LM_DEBUG((7, "get_pmap_addr", "kcreate() returned %d", error));
		goto out;
	}

	parms.pm_prog = prog;
	parms.pm_vers = vers;
	if (strcmp(config->knc_proto, NC_TCP) == 0) {
		parms.pm_prot = IPPROTO_TCP;
	} else {
		parms.pm_prot = IPPROTO_UDP;
	}
	parms.pm_port = 0;
	tmo.tv_sec = LM_PMAP_TIMEOUT;
	tmo.tv_usec = 0;

	client->cl_nosignal = 1;
	if ((status = CLNT_CALL(client, PMAPPROC_GETPORT,
				xdr_pmap, (char *)&parms,
				xdr_u_short, (char *)&port,
				tmo)) != RPC_SUCCESS) {
		sigreplace(&oldmask, (k_sigset_t *)NULL);
		LM_DEBUG((7, "get_pmap_addr", "CLNT_CALL(GETPORT) returned %d",
		    status));
		goto out;
	}

	sigreplace(&oldmask, (k_sigset_t *)NULL);

	LM_DEBUG((7, "get_pmap_addr", "port= %d", port));
	put_inet_port(addr, ntohs(port));

out:
	if (client)
		lm_clnt_destroy(&client);
	return (status);
}

/*
 * Free all RPC client-handles for the machine ls.  If ls is NULL, free all
 * RPC client handles.
 */
void
lm_flush_clients(struct lm_sysid  *ls)
{
	struct lm_client *lc;

	mutex_enter(&lm_lck);

	for (lc = lm_clients; lc; lc = lc->next) {
		LM_DEBUG((1, "flush_clients", "flushing lc %p, in_use %d",
		    (void *)lc, lc->in_use));
		if (! lc->in_use)
			if ((! ls) || (lc->sysid == ls))
				lm_rel_client(lc, EINVAL);
	}
	mutex_exit(&lm_lck);
}

/*
 * Try to free all lm_client objects, their RPC handles, and their
 * associated memory.  In-use entries are left alone.
 */
/*ARGSUSED*/
void
lm_flush_clients_mem(void *cdrarg)
{
	struct lm_client *lc;
	struct lm_client *nextlc = NULL;
	struct lm_client *prevlc = NULL; /* previous kept element */

	mutex_enter(&lm_lck);

	for (lc = lm_clients; lc; lc = nextlc) {
		nextlc = lc->next;
		if (lc->in_use) {
			prevlc = lc;
		} else {
			if (prevlc == NULL) {
				lm_clients = nextlc;
			} else {
				prevlc->next = nextlc;
			}
			--lm_stat.client_len;
			lm_clnt_destroy(&lc->client);
			if (lc->addr.buf) {
				kmem_free(lc->addr.buf, lc->addr.maxlen);
			}
			if (lc->sysid) {
				lm_rel_sysid(lc->sysid);
			}
			kmem_cache_free(lm_client_cache, lc);
		}
	}
	mutex_exit(&lm_lck);
}

/*
 * Make an RPC call to addr via config.
 *
 * Returns:
 * 0		Success.
 * EIO		Couldn't get client handle, timed out, or got unexpected
 *		RPC status within LM_RETRY attempts.
 * EINVAL	Unrecoverable error in RPC call.  Causes client handle
 *		to be destroyed.
 * EINTR	RPC call was interrupted within LM_RETRY attempts.
 */
int
lm_callrpc(struct lm_sysid *ls, rpcprog_t prog, rpcvers_t vers, rpcproc_t proc,
	xdrproc_t inproc, caddr_t in, xdrproc_t outproc, caddr_t out,
	int timout, int tries)
{
	struct timeval tmo;
	struct lm_client *lc = NULL;
	enum clnt_stat stat;
	int error;
	int signalled;
	int iscancel;
	k_sigset_t oldmask;
	k_sigset_t newmask;

	ASSERT(proc != LM_IGNORED);

	LM_DEBUG((6, "callrpc", "Calling [%u, %u, %u] on '%s' (%x) via '%s'",
	    prog, vers, proc, ls->name, ls->sysid,
	    ls->config.knc_proto));

	tmo.tv_sec = timout;
	tmo.tv_usec = 0;
	signalled = 0;
	sigfillset(&newmask);
	iscancel = (prog == NLM_PROG &&
		    (proc == NLM_CANCEL || proc == NLMPROC4_CANCEL ||
			proc == NLM_UNLOCK || proc == NLMPROC4_UNLOCK));

	while (tries--) {
		/*
		 * If any signal has been posted to our (user) thread,
		 * bail out as quickly as possible.  The exception is
		 * if we are doing any type of CANCEL/UNLOCK:  in that case,
		 * we may already have a posted signal and we need to
		 * live with it.
		 */
		if (lm_sigispending()) {
			LM_DEBUG((6, "callrpc", "posted signal"));
			if (iscancel == 0) {
				error = EINTR;
				break;
			}
		}

		error = 0;
		stat = lm_get_client(ls, prog, vers, timout, &lc, iscancel);
		if (IS_UNRECOVERABLE_RPC(stat)) {
			error = EINVAL;
			goto rel_client;
		} else if (stat != RPC_SUCCESS) {
			error = EIO;
			continue;
		}

		if (lm_sigispending()) {
			LM_DEBUG((6, "callrpc",
			    "posted signal after lm_get_client"));
			if (iscancel == 0) {
				error = EINTR;
				goto rel_client;
			}
		}
		ASSERT(lc != NULL);
		ASSERT(lc->client != NULL);

		lm_n_buf(6, "callrpc", "addr", &lc->addr);

		sigreplace(&newmask, &oldmask);
		stat = CLNT_CALL(lc->client, proc, inproc, in,
				outproc, out, tmo);
		sigreplace(&oldmask, (k_sigset_t *)NULL);

		if (lm_sigispending()) {
			LM_DEBUG((6, "callrpc",
			    "posted signal after CLNT_CALL"));
			signalled = 1;
		}

		switch (stat) {
		case RPC_SUCCESS:
			/*
			 * Update the timestamp on the client cache entry.
			 */
			lc->time = time;
			error = 0;
			break;

		case RPC_TIMEDOUT:
			LM_DEBUG((6, "callrpc", "RPC_TIMEDOUT"));
			if (signalled && (iscancel == 0)) {
				error = EINTR;
				break;
			}
			if (timout == 0) {
				/*
				 * We will always time out when timout == 0.
				 * Don't update the lc->time stamp. We do not
				 * know if the client handle is still OK.
				 */
				error = 0;
				break;
			}
			/* FALLTHROUGH */
		case RPC_CANTSEND:
		case RPC_XPRTFAILED:
		default:
			if (IS_UNRECOVERABLE_RPC(stat)) {
				error = EINVAL;
			} else if (signalled && (iscancel == 0)) {
				error = EINTR;
			} else {
				error = EIO;
			}
		}

rel_client:
		LM_DEBUG((6, "callrpc", "RPC stat= %d error= %d", stat, error));
		if (lc != NULL)
			lm_rel_client(lc, error);

		/*
		 * If EIO, loop else we're done.
		 */
		if (error != EIO) {
			break;
		}
	}

	mutex_enter(&lm_lck);
	lm_stat.tot_out++;
	lm_stat.bad_out += (error != 0);
	lm_stat.proc_out[proc]++;

	LM_DEBUG((6, "callrpc", "End: error= %d, tries= %d, tot= %u, bad= %u",
	    error, tries, lm_stat.tot_out, lm_stat.bad_out));

	mutex_exit(&lm_lck);
	return (error);
}

/*
 * lm_async
 *
 * List of outstanding asynchronous RPC calls.
 * An entry in the list is free iff in_use == FALSE.
 * Only nlm_stats are expected as replies (can easily be extended).
 */
struct lm_async {
	struct lm_async *next;
	int cookie;
	kcondvar_t cv;
	enum nlm_stats stat;
	bool_t reply;
	bool_t in_use;
};

/*
 * For the benefit of warlock.  A description of the "LM in_use" scheme
 * is located above near the declaration of lm_client.
 */
#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_async::{next stat reply}))
_NOTE(SCHEME_PROTECTS_DATA("LM in_use", lm_async::in_use))
#endif

static struct lm_async *lm_asyncs = NULL;
static struct kmem_cache *lm_async_cache = NULL;

#ifndef lint
_NOTE(MUTEX_PROTECTS_DATA(lm_lck, lm_asyncs))
#endif

/*
 * lm_asynrpc
 *
 * Make an asynchronous RPC call.
 * Since the call is asynchronous, we put ourselves onto a list and wait for
 * the reply to arrive.  If a reply has not arrived within timout seconds,
 * we retransmit.  So far, this routine is only used by lm_block_lock()
 * to send NLM_GRANTED_MSG to asynchronous (NLM_LOCK_MSG-using) clients.
 * Note: the stat given by caller is updated in lm_asynrply().
 */
int
lm_asynrpc(struct lm_sysid *ls, rpcprog_t prog, rpcvers_t vers, rpcproc_t proc,
	xdrproc_t inproc, caddr_t in, int cookie, enum nlm_stats *stat,
	int timout, int tries)
{
	int error;
	struct lm_async *la;

	/*
	 * Start by inserting the call in lm_asyncs.
	 * Find an empty entry, or create one.
	 */
	mutex_enter(&lm_lck);
	for (la = lm_asyncs; la; la = la->next)
		if (! la->in_use)
			break;
	if (!la) {
		la = kmem_cache_alloc(lm_async_cache, KM_SLEEP);
		cv_init(&la->cv, NULL, CV_DEFAULT, NULL);
		la->next = lm_asyncs;
		lm_asyncs = la;
		lm_stat.async_len++;
	}
	la->cookie = cookie;
	*stat = la->stat = -1;
	la->reply = FALSE;
	la->in_use = TRUE;

	LM_DEBUG((5, "asynrpc",
	    "la= %p cookie= %d stat= %d reply= %d in_use= %d asyncs= %d",
	    (void *)la, la->cookie, la->stat, la->reply, la->in_use,
	    lm_stat.async_len));
	mutex_exit(&lm_lck);

	/*
	 * Call the host asynchronously, i.e. with no timeout.
	 * Sleep timout seconds or until a reply has arrived.
	 * Note that the sleep may NOT be interrupted (we're
	 * a kernel thread).
	 */
	while (tries--) {
		if (error = lm_callrpc(ls, prog, vers, proc, inproc, in,
		    xdr_void, NULL, LM_NO_TIMOUT, 1))
			break;

		mutex_enter(&lm_lck);
		(void) cv_timedwait(&la->cv, &lm_lck,
		    lbolt + (clock_t)timout * hz);

		/*
		 * Our thread may have been cv_signal'ed.
		 */
		if (la->reply == TRUE) {
			error = 0;
		} else {
			LM_DEBUG((5, "asynrpc", "timed out"));
			error = EIO;
		}

		if (error == 0) {
			*stat = la->stat;
			LM_DEBUG((5, "asynrpc", "End: tries= %d, stat= %d",
				tries, la->stat));
			mutex_exit(&lm_lck);
			break;
		}

		mutex_exit(&lm_lck);
		LM_DEBUG((5, "asynrpc", "Timed out. tries= %d", tries));
	}

	/*
	 * Release entry in lm_asyncs.
	 */
	la->in_use = FALSE;
	return (error);
}

/*
 * lm_asynrply():
 * Find the lm_async and update reply and stat.
 * Don't bother if lm_async does not exist.
 *
 * Note that the caller can identify the async call with just the cookie.
 * This is because we generated the async call and we know that each new
 * call gets a new cookie.
 */
void
lm_asynrply(int cookie, enum nlm_stats stat)
{
	struct lm_async *la;

	LM_DEBUG((5, "asynrply", "cookie= %d stat= %d", cookie, stat));

	mutex_enter(&lm_lck);
	for (la = lm_asyncs; la; la = la->next) {
		if (la->in_use)
			if (cookie == la->cookie)
				break;
		LM_DEBUG((5, "asynrply", "passing la= %p in_use= %d cookie= %d",
		    (void *)la, la->in_use, la->cookie));
	}

	if (la) {
		la->stat = stat;
		la->reply = TRUE;
		LM_DEBUG((5, "asynrply", "signalling la= %p", (void *)la));
		cv_signal(&la->cv);
	} else {
		LM_DEBUG((5, "asynrply", "Couldn't find matching la"));
	}
	mutex_exit(&lm_lck);
}

/*
 * lm_waitfor_granted
 *
 * Wait for an NLM_GRANTED corresponding to the given lm_sleep.
 *
 * Return value:
 * 0     an NLM_GRANTED has arrived.
 * EINTR sleep was interrupted.
 * -1    the sleep timed out.
 */
int
lm_waitfor_granted(struct lm_sleep *lslp)
{
	int	error = 0;
	clock_t	time;
	clock_t time_left;

	mutex_enter(&lm_lck);
	time = lbolt + (clock_t)LM_BLOCK_SLP * hz;
	error = 0;
	while (lslp->waiting && (error == 0)) {
		time_left = cv_timedwait_sig(&lslp->cv, &lm_lck, time);
		switch (time_left) {
		case -1:		/* timed out */
			error = -1;
			break;
		case 0:			/* caught a signal */
			error = EINTR;
			break;
		default:		/* cv_signal woke us */
			error = 0;
			break;
		}
	};
	mutex_exit(&lm_lck);

	LM_DEBUG((5, "waitfor_granted", "End: error= %d", error));

	return (error);
}

/*
 * lm_signal_granted():
 * Find the lm_sleep corresponding to the given arguments.
 * If lm_sleep is found, wakeup process and return 0.
 * Otherwise return -1.
 */
int
lm_signal_granted(pid_t pid, struct netobj *fh, struct netobj *oh,
	u_offset_t offset, u_offset_t length)
{
	struct lm_sleep *lslp;

	mutex_enter(&lm_lck);
	for (lslp = lm_sleeps; lslp != NULL; lslp = lslp->next) {
		/*
		 * Theoretically, only the oh comparison is necessary to
		 * determine a match.  The other comparisons are for
		 * additional safety.  (Remember that a false match would
		 * cause a process to think it has a lock when it doesn't,
		 * which can cause file corruption.)  We can't compare
		 * sysids because the callback might come in using a
		 * different netconfig than the one the lock request went
		 * out on.
		 */
		if (lslp->in_use && pid == lslp->pid &&
		    lslp->offset == offset && lslp->length == length &&
		    lslp->oh.n_len == oh->n_len &&
		    bcmp(lslp->oh.n_bytes, oh->n_bytes, oh->n_len) == 0 &&
		    lslp->fh.n_len == fh->n_len &&
		    bcmp(lslp->fh.n_bytes, fh->n_bytes, fh->n_len) == 0)
			break;
	}

	if (lslp) {
		lslp->waiting = FALSE;
		cv_signal(&lslp->cv);
	}

	LM_DEBUG((5, "signal_granted", "pid= %d, in_use= %d, sleeps= %d",
	    (lslp ? lslp->pid : -1),
	    (lslp ? lslp->in_use : -1), lm_stat.sleep_len));
	mutex_exit(&lm_lck);

	return (lslp ? 0 : -1);
}

/*
 * Allocate and fill in an lm_sleep, and put it in the global list.
 */
struct lm_sleep *
lm_get_sleep(struct lm_sysid *ls, struct netobj *fh, struct netobj *oh,
	u_offset_t offset, len_t length)
{
	struct lm_sleep *lslp;

	mutex_enter(&lm_lck);
	for (lslp = lm_sleeps; lslp; lslp = lslp->next)
		if (! lslp->in_use)
			break;
	if (lslp == NULL) {
		lslp = kmem_cache_alloc(lm_sleep_cache, KM_SLEEP);
		cv_init(&lslp->cv, NULL, CV_DEFAULT, NULL);
		lslp->next = lm_sleeps;
		lm_sleeps = lslp;
		lm_stat.sleep_len++;
	}

	lslp->pid = curproc->p_pid;
	lslp->in_use = TRUE;
	lslp->waiting = TRUE;
	lslp->sysid = ls;
	lm_ref_sysid(ls);
	lslp->fh.n_len = fh->n_len;
	lslp->fh.n_bytes = lm_dup(fh->n_bytes, fh->n_len);
	lslp->oh.n_len = oh->n_len;
	lslp->oh.n_bytes = lm_dup(oh->n_bytes, oh->n_len);
	lslp->offset = offset;
	lslp->length = length;

	LM_DEBUG((5, "get_sleep", "pid= %d, in_use= %d, sleeps= %d",
	    lslp->pid, lslp->in_use, lm_stat.sleep_len));

	mutex_exit(&lm_lck);

	return (lslp);
}

/*
 * Release the given lm_sleep.  Resets its contents and frees any memory
 * that it owns.
 */
void
lm_rel_sleep(struct lm_sleep *lslp)
{
	mutex_enter(&lm_lck);
	lslp->in_use = FALSE;

	lm_rel_sysid(lslp->sysid);
	lslp->sysid = NULL;

	kmem_free(lslp->fh.n_bytes, lslp->fh.n_len);
	lslp->fh.n_bytes = NULL;
	lslp->fh.n_len = 0;

	kmem_free(lslp->oh.n_bytes, lslp->oh.n_len);
	lslp->oh.n_bytes = NULL;
	lslp->oh.n_len = 0;

	mutex_exit(&lm_lck);
}

/*
 * Free any unused lm_sleep structs.
 */
/*ARGSUSED*/
void
lm_free_sleep(void *cdrarg)
{
	struct lm_sleep *prevslp = NULL; /* previously kept sleep */
	struct lm_sleep *nextslp = NULL;
	struct lm_sleep *slp;

	mutex_enter(&lm_lck);
	LM_DEBUG((5, "free_sleep", "start length: %d\n", lm_stat.sleep_len));

	for (slp = prevslp; slp != NULL; slp = nextslp) {
		nextslp = slp->next;
		if (slp->in_use) {
			prevslp = slp;
		} else {
			if (prevslp == NULL) {
				lm_sleeps = nextslp;
			} else {
				prevslp->next = nextslp;
			}
			--lm_stat.sleep_len;
			ASSERT(slp->sysid == NULL);
			ASSERT(slp->fh.n_bytes == NULL);
			ASSERT(slp->oh.n_bytes == NULL);
			cv_destroy(&slp->cv);
			kmem_cache_free(lm_sleep_cache, slp);
		}
	}

	LM_DEBUG((5, "free_sleep", "end length: %d\n", lm_stat.sleep_len));
	mutex_exit(&lm_lck);
}

/*
 * Create the kmem caches for allocating various lock manager tables.
 */
void
lm_caches_init(void)
{
	mutex_enter(&lm_lck);
	if (!lm_caches_created) {
		lm_caches_created = TRUE;
		lm_server_caches_init();
		lm_sysid_cache = kmem_cache_create("lm_sysid",
			sizeof (struct lm_sysid), 0, NULL, NULL,
			lm_free_sysid_table, NULL, NULL, 0);
		lm_client_cache = kmem_cache_create("lm_client",
			sizeof (struct lm_client), 0, NULL, NULL,
			lm_flush_clients_mem, NULL, NULL, 0);
		lm_async_cache = kmem_cache_create("lm_async",
			sizeof (struct lm_async), 0, NULL, NULL,
			NULL, NULL, NULL, 0);
		lm_sleep_cache = kmem_cache_create("lm_sleep",
			sizeof (struct lm_sleep), 0, NULL, NULL,
			lm_free_sleep, NULL, NULL, 0);
		lm_config_cache = kmem_cache_create("lm_config",
			sizeof (struct lm_config), 0, NULL, NULL,
			NULL, NULL, NULL, 0);
	}
	mutex_exit(&lm_lck);
}

/*
 * Determine whether or not a signal is pending on the calling thread.
 * If so, return 1 else return 0.
 *
 * XXX: Fixes to this code should probably be propagated to (or from)
 * the common signal-handling code in sig.c.  See bugid 1201594.
 */
int
lm_sigispending(void)
{
	klwp_t *lwp;

	/*
	 * Some callers may be non-signallable kernel threads, in
	 * which case we always return 0.  Allowing such a thread
	 * to (pointlessly) call ISSIG() would result in a panic.
	 */
	lwp = ttolwp(curthread);
	if (lwp == NULL) {
		return (0);
	}

	/*
	 * lwp_asleep and lwp_sysabort are modified only for the sake of
	 * /proc, and should always be set to 0 after the ISSIG call.
	 * Note that the lwp may sleep for a long time inside
	 * ISSIG(FORREAL) - a human being may be single-stepping in a
	 * debugger, for example - so we must not hold any mutexes or
	 * other critical resources here.
	 */
	lwp->lwp_asleep = 1;
	lwp->lwp_sysabort = 0;
	/* ASSERT(no mutexes or rwlocks are held) */
	if (ISSIG(curthread, FORREAL) || lwp->lwp_sysabort || ISHOLD(curproc)) {
		lwp->lwp_asleep = 0;
		lwp->lwp_sysabort = 0;
		return (1);
	}

	lwp->lwp_asleep = 0;
	return (0);
}
/*
 * When a Checkpoint (CPR suspend) occurs and a remote lock is held,
 * keep track of the state of system (server).
 */
void
lm_cprsuspend(void)
{
	struct lm_sysid *ls;
	sm_name		arg;
	int		error;
	sm_stat_res	res;

	rw_enter(&lm_sysids_lock, RW_READER);
	/*
	 * Check if there are at least one active lock relating
	 * to this sysid
	 */
	for (ls = lm_sysids; ls; ls = ls->next) {
		lm_ref_sysid(ls);
		/*
		 * If there is a remote lock held on this system
		 * get and store current state of statd.  Otherwise,
		 * set state to 0.
		 */
		if (flk_sysid_has_locks(ls->sysid | LM_SYSID_CLIENT,
		    FLK_QUERY_ACTIVE)) {
			arg.mon_name = ls->name;
			error = lm_callrpc(ls, SM_PROG, SM_VERS, SM_STAT,
					xdr_sm_name, (caddr_t)&arg,
					xdr_sm_stat_res, (caddr_t)&res,
					LM_CR_TIMOUT, LM_RETRY);
			/*
			 * If an error occurred while getting the
			 * state of server statd, set state to -1.
			 */
			mutex_enter(&ls->lock);
			if (error != 0) {
				nfs_cmn_err(error, CE_WARN,
			    "lockd: cannot contact statd (%m), continuing");
				ls->sm_state = -1;
			} else
				ls->sm_state = res.state;
			mutex_exit(&ls->lock);
		} else {
			mutex_enter(&ls->lock);
			ls->sm_state = 0;
			mutex_exit(&ls->lock);
		}
		lm_rel_sysid(ls);
	}
	rw_exit(&lm_sysids_lock);
}

/*
 * When CPR Resume occurs, determine if state has changed and if
 * so, either reclaim lock or send SIGLOST to process depending
 * on state.
 */
void
lm_cprresume(void)
{
	struct lm_sysid *ls;
	sm_name		arg;
	int		error = 0;
	sm_stat_res	res;
	locklist_t *llp, *next_llp;

	rw_enter(&lm_sysids_lock, RW_READER);
	for (ls = lm_sysids; ls; ls = ls->next) {
		lm_ref_sysid(ls);
		/* No remote active locks were held when we suspended. */
		mutex_enter(&ls->lock);
		if (ls->sm_state == 0) {
			mutex_exit(&ls->lock);
			lm_rel_sysid(ls);
			continue;
		}
		/*
		 * Statd on server has is an older version
		 * such that it returns -1 as the state.
		 */
		if (ls->sm_state == -1) {
			mutex_exit(&ls->lock);
			/* Get active locks */
			llp = flk_get_active_locks(ls->sysid | LM_SYSID_CLIENT,
			    NOPID);
			while (llp) {
				lm_send_siglost(&llp->ll_flock, ls);
				next_llp = llp->ll_next;
				VN_RELE(llp->ll_vp);
				kmem_free(llp, sizeof (*llp));
				llp = next_llp;
			}
			lm_rel_sysid(ls);
			continue;
		}

		mutex_exit(&ls->lock);

		/* Check current state of server statd */
		arg.mon_name = ls->name;
		error = lm_callrpc(ls, SM_PROG, SM_VERS, SM_STAT,
				xdr_sm_name, (caddr_t)&arg, xdr_sm_stat_res,
				(caddr_t)&res, LM_CR_TIMOUT, LM_RETRY);

		if (error == 0) {
			/*
			 * Server went down while we were in
			 * suspend mode and thus try to reclaim locks.
			 */
			mutex_enter(&ls->lock);
			if (ls->sm_state != res.state) {
				mutex_exit(&ls->lock);
				lm_flush_clients(ls);

				/* Get active locks */
				llp = flk_get_active_locks(
				    ls->sysid | LM_SYSID_CLIENT, NOPID);
				while (llp) {
					LM_DEBUG((1, "rlck_serv",
					    "calling lm_reclaim(%p)",
					    (void *)llp));
					lm_reclaim_lock(llp->ll_vp,
					    &llp->ll_flock);
					next_llp = llp->ll_next;
					VN_RELE(llp->ll_vp);
					kmem_free(llp, sizeof (*llp));
					llp = next_llp;
				}
			} else
				mutex_exit(&ls->lock);
		} else
			nfs_cmn_err(error, CE_WARN,
			"lockd: cannot contact statd (%m), continuing");
		lm_rel_sysid(ls);
	}
	rw_exit(&lm_sysids_lock);
}

/*
 *  Sends a SIGLOST to process associated with file.  This occurs
 *  if locks can not be reclaimed or after Checkpoint and Resume
 *  where an older version of statd is encountered.
 */
void
lm_send_siglost(struct flock64 *flkp, struct lm_sysid *ls)
{
	proc_t  *p;

	/*
	 * No need to discard the local locking layer's cached copy
	 * of the lock, since the normal closeall when process
	 * exits due to SIGLOST will clean up accordingly
	 * and/or application if it catches SIGLOST is assumed to deal
	 * with unlocking prior to handling it.  If it is
	 * discarded here only, then an unlock request will never reach
	 * server and thus causing an orphan lock.  NOTE: If
	 * application does not do unlock prior to continuing
	 * it will still hold a lock until the application exits.
	 *
	 * Find the proc and signal it that the
	 * lock could not be reclaimed.
	 */
	mutex_enter(&pidlock);
	p = prfind(flkp->l_pid);
	if (p)
		psignal(p, SIGLOST);
	mutex_exit(&pidlock);
	cmn_err(CE_NOTE, "lockd: pid %d lost lock on server %s",
		flkp->l_pid, ls->name);
}
