/*
 * Copyright (c) 1993, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYNCH_H
#define	_SYNCH_H

#pragma ident	"@(#)synch.h	1.42	98/02/20 SMI"

/*
 * synch.h:
 * definitions needed to use the thread synchronization interface
 */

#ifndef _ASM
#include <sys/machlock.h>
#include <sys/time_impl.h>
#include <sys/synch.h>
#endif /* _ASM */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

/*
 * Semaphores
 */
typedef struct _sema {
	/* this structure must be the same as sem_t in <semaphore.h> */
	uint32_t	count;		/* semaphore count */
	uint16_t	type;
	uint16_t	magic;
	upad64_t	pad1[3];	/* reserved for a mutex_t */
	upad64_t 	pad2[2];	/* reserved for a cond_t */
} sema_t;

/*
 * POSIX.1c Note:
 * POSIX.1c requires that <pthread.h> define the structures pthread_mutex_t
 * and pthread_cond_t.  These structures are identical to mutex_t (lwp_mutex_t)
 * and cond_t (lwp_cond_t) which are defined in <synch.h>.  A nested included
 * of <synch.h> (to allow a "#typedef mutex_t  pthread_mutex_t") would pull in
 * non-posix symbols/constants violating the namespace restrictions.  Hence,
 * pthread_mutex_t/pthread_cond_t have been redefined in <pthread.h>.  Any
 * modifications done to mutex_t/lwp_mutex_t or cond_t/lwp_cond_t should also
 * be done to pthread_mutex_t/pthread_cond_t.
 */
typedef lwp_mutex_t mutex_t;
typedef lwp_cond_t cond_t;

/*
 * Readers/writer locks
 */
typedef struct _rwlock {
	int32_t		readers;	/* -1 == writer else # of readers */
	uint16_t	type;
	uint16_t	magic;
	upad64_t	pad1[3];	/* internal consistency mutex */
	upad64_t	pad2[2];	/* really cond_t for readers */
	upad64_t	pad3[2];	/* really cond_t for writers */
} rwlock_t;

#ifdef	__STDC__
int	_lwp_mutex_lock(lwp_mutex_t *);
int	_lwp_mutex_unlock(lwp_mutex_t *);
int	_lwp_mutex_trylock(lwp_mutex_t *);
int	_lwp_cond_wait(lwp_cond_t *, lwp_mutex_t *);
int	_lwp_cond_timedwait(lwp_cond_t *, lwp_mutex_t *, timestruc_t *);
int	_lwp_cond_signal(lwp_cond_t *);
int	_lwp_cond_broadcast(lwp_cond_t *);
int	_lwp_sema_init(lwp_sema_t *, int);
int	_lwp_sema_wait(lwp_sema_t *);
int	_lwp_sema_trywait(lwp_sema_t *);
int	_lwp_sema_post(lwp_sema_t *);
int	cond_init(cond_t *, int, void *);
int	cond_destroy(cond_t *);
int	cond_wait(cond_t *, mutex_t *);
int	cond_timedwait(cond_t *, mutex_t *, timestruc_t *);
int	cond_signal(cond_t *);
int	cond_broadcast(cond_t *);
int	mutex_init(mutex_t *, int, void *);
int	mutex_destroy(mutex_t *);
int	mutex_lock(mutex_t *);
int	mutex_trylock(mutex_t *);
int	mutex_unlock(mutex_t *);
int	rwlock_init(rwlock_t *, int, void *);
int	rwlock_destroy(rwlock_t *);
int	rw_rdlock(rwlock_t *);
int	rw_wrlock(rwlock_t *);
int	rw_unlock(rwlock_t *);
int	rw_tryrdlock(rwlock_t *);
int	rw_trywrlock(rwlock_t *);
int	sema_init(sema_t *, unsigned int, int, void *);
int	sema_destroy(sema_t *);
int	sema_wait(sema_t *);
int	sema_post(sema_t *);
int	sema_trywait(sema_t *);

#else	/* __STDC__ */

int	_lwp_mutex_lock();
int	_lwp_mutex_unlock();
int	_lwp_mutex_trylock();
int	_lwp_cond_wait();
int	_lwp_cond_timedwait();
int	_lwp_cond_signal();
int	_lwp_cond_broadcast();
int	_lwp_sema_init();
int	_lwp_sema_wait();
int	_lwp_sema_trywait();
int	_lwp_sema_post();
int	cond_init();
int	cond_destroy();
int	cond_wait();
int	cond_timedwait();
int	cond_signal();
int	cond_broadcast();
int	mutex_init();
int	mutex_destroy();
int	mutex_lock();
int	mutex_trylock();
int	mutex_unlock();
int	rwlock_init();
int	rwlock_destroy();
int	rw_rdlock();
int	rw_wrlock();
int	rw_unlock();
int	rw_tryrdlock();
int	rw_trywrlock();
int	sema_init();
int	sema_destroy();
int	sema_wait();
int	sema_post();
int	sema_trywait();

#endif	/* __STDC__ */

#endif /* _ASM */

/* "Magic numbers" tagging synchronization object types */
#define	MUTEX_MAGIC	0x4d58
#define	SEMA_MAGIC	0x534d
#define	COND_MAGIC	0x4356
#define	RWL_MAGIC	0x5257

/*
 * POSIX.1c Note:
 * DEFAULTMUTEX is defined same as PTHREAD_MUTEX_INITIALIZER in <pthread.h>.
 * DEFAULTCV is defined same as PTHREAD_COND_INITIALIZER in <pthread.h>.
 * Any changes to these macros should be reflected in <pthread.h>
 */
#define	DEFAULTMUTEX	{{{0, 0, 0, 0}, {USYNC_THREAD}, MUTEX_MAGIC}, \
				{0, 0, 0, 0, 0, 0, 0, 0}, 0}
#define	SHAREDMUTEX	{{{0, 0, 0, 0}, {USYNC_PROCESS}, MUTEX_MAGIC}, \
				{0, 0, 0, 0, 0, 0, 0, 0}, 0}
#define	DEFAULTCV	{0, 0, 0, 0, USYNC_THREAD, COND_MAGIC}
#define	SHAREDCV	{0, 0, 0, 0, USYNC_PROCESS, COND_MAGIC}
#define	DEFAULTSEMA	{0, USYNC_THREAD, SEMA_MAGIC, 0, 0, 0, 0}
#define	SHAREDSEMA	{0, USYNC_PROCESS, SEMA_MAGIC, 0, 0, 0, 0}
#define	DEFAULTRWLOCK	{0, USYNC_THREAD, RWL_MAGIC, 0, 0, 0, 0, 0, 0}
#define	SHAREDRWLOCK	{0, USYNC_PROCESS, RWL_MAGIC, \
			USYNC_PROCESS, MUTEX_MAGIC, 0, 0, \
			USYNC_PROCESS, COND_MAGIC, 0, \
			USYNC_PROCESS, COND_MAGIC, 0}

/*
 * Tests on lock states.
 */
#define	SEMA_HELD(x)		_sema_held(x)
#define	RW_READ_HELD(x)		_rw_read_held(x)
#define	RW_WRITE_HELD(x)	_rw_write_held(x)
#define	RW_LOCK_HELD(x)		(RW_READ_HELD(x) || RW_WRITE_HELD(x))
#define	MUTEX_HELD(x)		_mutex_held(x)

/*
 * The following definitions are for assertions which can be checked
 * statically by tools like lock_lint.  You can also define your own
 * run-time test for each.  If you don't, we define them to 1 so that
 * such assertions simply pass.
 */
#ifndef NO_LOCKS_HELD
#define	NO_LOCKS_HELD	1
#endif
#ifndef NO_COMPETING_THREADS
#define	NO_COMPETING_THREADS	1
#endif

#ifndef _ASM

#ifdef	__STDC__

int _sema_held(sema_t *);
int _rw_read_held(rwlock_t *);
int _rw_write_held(rwlock_t *);
int _mutex_held(mutex_t *);

#else	/* __STDC__ */

int _sema_held();
int _rw_read_held();
int _rw_write_held();
int _mutex_held();

#endif	/* __STDC__ */

#endif /* _ASM */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYNCH_H */
