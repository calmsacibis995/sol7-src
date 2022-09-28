/*	Copyright (c) 1992 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma	ident	"@(#)flockf.c	1.23	98/01/30 SMI"	/* SVr4.0 1.2   */

/* LINTLIBRARY */

#pragma weak flockfile = _flockfile
#pragma weak ftrylockfile = _ftrylockfile
#pragma weak funlockfile = _funlockfile

#define	flockfile _flockfile
#define	ftrylockfile _ftrylockfile
#define	funlockfile _funlockfile

#include "synonyms.h"
#include "mtlib.h"
#include "file64.h"
#include <stdio.h>
#include <thread.h>
#include <synch.h>
#include <stdlib.h>
#include "stdiom.h"

static void _rmutex_lock(rmutex_t *rm);
/*
 * we move the threads check out of the
 * The _rmutex_lock/_rmutex_unlock routines are only called (within libc !)
 * by _flockget, _flockfile, and _flockrel, _funlockfile, respectively.
 * _flockget and _flockrel are only called by the FLOCKFILE/FUNLOCKFILE
 * macros in mtlib.h. We place the "if (_thr_main() == -1)" check for
 * threads there, and remove it from:
 *	_rmutex_lock(rm)
 *	_rmutex_unlock(rm)
 *	_flockget(FILE *iop)
 *	_ftrylockfile(FILE *iop)
 * No change is made to _funlockfile(iop) as it makes no additional thread
 * check.
 *
 * No such change is made to
 *	_rmutex_trylock(rl)		since it is called by _findiop
 *	_flockfile(iop)			since we don't know who uses it
 */

static void
_rmutex_lock(rmutex_t *rm)
{
	thread_t self = _thr_self();
	mutex_t *lk = &rm->_mutex;

	(void) _mutex_lock(lk);
	if (rm->_owner != 0 && rm->_owner != self) {
		rm->_wait_cnt++;
		do {
			(void) _cond_wait(&rm->_cond, lk);
		} while (rm->_owner != 0 && rm->_owner != self);
		rm->_wait_cnt--;
	}
	/* lock is now available to this thread */
	rm->_owner = self;
	rm->_lock_cnt++;
	(void) _mutex_unlock(lk);
}


int
_rmutex_trylock(rmutex_t *rm)
{
	thread_t self = _thr_self();
	mutex_t *lk = &rm->_mutex;

	/*
	 * Treat like a stub if not linked with libthread as
	 * indicated by _thr_main() returning -1.
	 */
	if (_thr_main() == -1)
		return (0);

	(void) _mutex_lock(lk);
	if (rm->_owner != 0 && rm->_owner != self) {
		(void) _mutex_unlock(lk);
		return (-1);
	}
	/* lock is now available to this thread */
	rm->_owner = self;
	rm->_lock_cnt++;
	(void) _mutex_unlock(lk);
	return (0);
}


/*
 * recursive mutex unlock function
 */

static void
_rmutex_unlock(rmutex_t *rm)
{
	thread_t self = _thr_self();
	mutex_t *lk = &rm->_mutex;

	(void) _mutex_lock(lk);
	if (rm->_owner == self) {
		rm->_lock_cnt--;
		if (rm->_lock_cnt == 0) {
			rm->_owner = 0;
			if (rm->_wait_cnt)
				(void) cond_signal(&rm->_cond);
		}
	} else {
		(void) abort();
	}
	(void) _mutex_unlock(lk);
}


/*
 * compute the lock's position, acquire it and return its pointer
 */

rmutex_t *
_flockget(FILE *iop)
{
	rmutex_t *rl = NULL;

	rl = IOB_LCK(iop);
	/*
	 * IOB_LCK may return a NULL pointer which means that
	 * the iop does not require locking, and does not have
	 * a lock associated with it.
	 */
	if (rl != NULL)
		_rmutex_lock(rl);
	return (rl);
}


/*
 * POSIX.1c version of ftrylockfile().
 * It returns 0 if it gets the lock else returns -1 to indicate the error.
 */

int
_ftrylockfile(FILE *iop)
{
	return (_rmutex_trylock(IOB_LCK(iop)));
}


void
_flockrel(rmutex_t *rl)
{
	/*
	 * may be called with a NULL pointer which means that the
	 * associated iop had no lock. see comments in flockget()
	 * on how lock could be NULL.
	 */
	if (rl != NULL)
		_rmutex_unlock(rl);
}


void
flockfile(FILE *iop)
{
	_rmutex_lock(IOB_LCK(iop));
}


void
funlockfile(FILE *iop)
{
	_rmutex_unlock(IOB_LCK(iop));
}
