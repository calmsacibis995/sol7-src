/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)pthr_mutex.c	1.16	98/01/16 SMI"

#ifdef __STDC__

#pragma weak	pthread_mutexattr_init = _pthread_mutexattr_init
#pragma weak	pthread_mutexattr_destroy =  _pthread_mutexattr_destroy
#pragma weak	pthread_mutexattr_setpshared =  _pthread_mutexattr_setpshared
#pragma weak	pthread_mutexattr_getpshared =  _pthread_mutexattr_getpshared
#pragma weak	pthread_mutexattr_setprotocol =  _pthread_mutexattr_setprotocol
#pragma weak	pthread_mutexattr_getprotocol =  _pthread_mutexattr_getprotocol
#pragma weak	pthread_mutexattr_setprioceiling = \
					_pthread_mutexattr_setprioceiling
#pragma weak	pthread_mutexattr_getprioceiling = \
					_pthread_mutexattr_getprioceiling
#pragma weak	pthread_mutex_setprioceiling =  _pthread_mutex_setprioceiling
#pragma weak	pthread_mutex_getprioceiling =  _pthread_mutex_getprioceiling
#pragma weak	pthread_mutex_init = _pthread_mutex_init
#pragma weak	pthread_mutexattr_settype =  _pthread_mutexattr_settype
#pragma weak	pthread_mutexattr_gettype =  _pthread_mutexattr_gettype


#pragma	weak	_ti_pthread_mutex_init = _pthread_mutex_init
#pragma weak	_ti_pthread_mutex_getprioceiling = \
					_pthread_mutex_getprioceiling
#pragma weak	_ti_pthread_mutexattr_destroy =  _pthread_mutexattr_destroy
#pragma weak	_ti_pthread_mutexattr_getprioceiling = \
					_pthread_mutexattr_getprioceiling
#pragma weak	_ti_pthread_mutexattr_getprotocol = \
					_pthread_mutexattr_getprotocol
#pragma weak	_ti_pthread_mutexattr_getpshared = \
					_pthread_mutexattr_getpshared
#pragma weak	_ti_pthread_mutexattr_init = _pthread_mutexattr_init
#pragma weak	_ti_pthread_mutex_setprioceiling = \
					_pthread_mutex_setprioceiling
#pragma weak	_ti_pthread_mutexattr_setprioceiling = \
					_pthread_mutexattr_setprioceiling
#pragma weak	_ti_pthread_mutexattr_setprotocol = \
					_pthread_mutexattr_setprotocol
#pragma weak	_ti_pthread_mutexattr_setpshared = \
			_pthread_mutexattr_setpshared
#pragma weak	_ti_pthread_mutexattr_settype =  _pthread_mutexattr_settype
#pragma weak	_ti_pthread_mutexattr_gettype =  _pthread_mutexattr_gettype
#endif /* __STDC__ */

#include "libpthr.h"
#include "libthread.h"
#include <pthread.h>

/*
 * POSIX.1c
 * pthread_mutexattr_init: allocates the mutex attribute object and
 * initializes it with the default values.
 */
int
_pthread_mutexattr_init(pthread_mutexattr_t *attr)
{
	mattr_t	*ap;

	if ((ap = (mattr_t *)_alloc_attr(sizeof (mattr_t))) != NULL) {
		ap->pshared = DEFAULT_TYPE;
		ap->type = PTHREAD_MUTEX_DEFAULT;
		attr->__pthread_mutexattrp = ap;
		return (0);
	} else
		return (ENOMEM);
}

/*
 * POSIX.1c
 * pthread_mutexattr_destroy: frees the mutex attribute object and
 * invalidates it with NULL value.
 */
int
_pthread_mutexattr_destroy(pthread_mutexattr_t *attr)
{
	if (attr == NULL || attr->__pthread_mutexattrp == NULL ||
			_free_attr(attr->__pthread_mutexattrp) < 0)
		return (EINVAL);
	attr->__pthread_mutexattrp = NULL;
	return (0);
}

/*
 * POSIX.1c
 * pthread_mutexattr_setpshared: sets the shared attr to PRIVATE or
 * SHARED.
 * This is equivalent to setting USYNC_PROCESS/USYNC_THREAD flag in
 * mutex_init().
 */
int
_pthread_mutexattr_setpshared(pthread_mutexattr_t *attr,
						int pshared)
{
	mattr_t	*ap;


	if (attr != NULL && (ap = attr->__pthread_mutexattrp) != NULL &&
		(pshared == PTHREAD_PROCESS_PRIVATE ||
			pshared == PTHREAD_PROCESS_SHARED)) {
		ap->pshared = pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * POSIX.1c
 * pthread_mutexattr_getpshared: gets the shared attr.
 */
int
_pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr,
						int *pshared)
{
	mattr_t	*ap;

	if (pshared != NULL && attr != NULL &&
			(ap = attr->__pthread_mutexattrp) != NULL) {
		*pshared = ap->pshared;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * POSIX.1c
 * pthread_mutexattr_setprioceiling: sets the prioceiling attr.
 * Currently unsupported.
 */
int
_pthread_mutexattr_setprioceiling(pthread_mutexattr_t *attr,
						int prioceiling)
{
	return (ENOSYS);
}

/*
 * POSIX.1c
 * pthread_mutexattr_getprioceiling: gets the prioceiling attr.
 * Currently unsupported.
 */
int
_pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *attr,
						int *ceiling)
{
	return (ENOSYS);
}

/*
 * POSIX.1c
 * pthread_mutexattr_setprotocol: sets the protocol attribute.
 * Currently unsupported.
 */
int
_pthread_mutexattr_setprotocol(pthread_mutexattr_t *attr,
						int protocol)
{
	return (ENOSYS);
}

/*
 * POSIX.1c
 * pthread_mutexattr_getprotocol: gets the protocol attribute.
 * Currently unsupported.
 */
int
_pthread_mutexattr_getprotocol(const pthread_mutexattr_t *attr,
						int *protocol)
{
	return (ENOSYS);
}

/*
 * POSIX.1c
 * pthread_mutex_init: Initializes the mutex object. It copies the
 * pshared attr into type argument and calls mutex_init().
 */
int
_pthread_mutex_init(pthread_mutex_t *mutex, pthread_mutexattr_t *attr)
{
	int	type, scope, rc;

	if (attr != NULL) {
		if (attr->__pthread_mutexattrp == NULL)
			return (EINVAL);
		scope = ((mattr_t *)attr->__pthread_mutexattrp)->pshared;
		type = ((mattr_t *)attr->__pthread_mutexattrp)->type;
		/*
		 * Following is not supported due to squeezed space
		 * in mutex_t and since it is not necessary for UNIX98 branding.
		 */
		if (scope == PTHREAD_PROCESS_SHARED &&
			type == PTHREAD_MUTEX_RECURSIVE)
			return (EINVAL);
	} else {
		scope = DEFAULT_TYPE;
		type = PTHREAD_MUTEX_DEFAULT;
	}

	rc = _mutex_init((mutex_t *) mutex, scope, NULL);
	if (!rc)
		_mutex_set_typeattr((mutex_t *) mutex, type);
	return (rc);

}

/*
 * POSIX.1c
 * pthread_mutex_setprioceiling: sets the prioceiling.
 * Currently unsupported.
 */
int
_pthread_mutex_setprioceiling(pthread_mutex_t *mutex,
				int prioceiling, int *oldceiling)
{
	return (ENOSYS);
}

/*
 * POSIX.1c
 * pthread_mutex_getprioceiling: gets the prioceiling.
 * Currently unsupported.
 */
int
_pthread_mutex_getprioceiling(const pthread_mutex_t *mutex,
					int *ceiling)
{
	return (ENOSYS);
}

/*
 * UNIX98
 * pthread_mutexattr_settype: sets the type attribute
 */
int
_pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type)
{
	mattr_t	*ap;

	if (attr != NULL && (ap = attr->__pthread_mutexattrp) != NULL &&
		(type == PTHREAD_MUTEX_NORMAL ||
		    type == PTHREAD_MUTEX_ERRORCHECK ||
		    type == PTHREAD_MUTEX_RECURSIVE)) {
		ap->type = type;
		return (0);
	} else {
		return (EINVAL);
	}
}

/*
 * UNIX98
 * pthread_mutexattr_gettype: gets the type attr.
 */
int
_pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type)
{
	mattr_t	*ap;

	if (type != NULL && attr != NULL &&
			(ap = attr->__pthread_mutexattrp) != NULL) {
		*type = ap->type;
		return (0);
	} else {
		return (EINVAL);
	}
}
