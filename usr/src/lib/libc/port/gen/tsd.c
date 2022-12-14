/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident  "@(#)tsd.c 1.4     97/04/05 SMI"

/*LINTLIBRARY*/

#include "synonyms.h"
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <thread.h>

#define DELETE_SUPPORTED

typedef void (*PFrV)(void *);

/*
 * key structure
 */
typedef struct key_info {
	void	*value;		/* value associated with key */
	PFrV	destructor;	/* key destructor */
} keys_t;

static keys_t	*keys = NULL;		/* keys area */

static int keys_allocated = 0;		/* number of keys allocated */
static int keys_used = 0;		/* number of keys in use so far */


static void
keys_destruct(void)
{
	int i;

	for (i = 0; i < keys_used; i++) {
		if (keys[i].value != NULL && keys[i].destructor != (PFrV)NULL) {
			(keys[i].destructor)(keys[i].value);
			keys[i].value = NULL;
		}
	}
}

/*
 * Null destructor assigned in case the key is deleted
 * It is better to have some proper function address rather
 * than having -1 or some thing like that.
 * if for any key, destructor is found to equal to this function
 * then it is considered invalid (deleted) key.
 */
static void
nulldestructor(void *value)
{}

/* public interfaces */
int
_libc_thr_keycreate(thread_key_t *pkey, PFrV destruct)
{
	keys_t *p;
	int nkeys;

	/*
	 * if keys has not been allocated, then the destructor routine has
	 * not been registered yet.
	 */
	if (keys == NULL)
		(void) atexit(keys_destruct);

	if (keys_used >= keys_allocated) {
		if (keys_allocated == 0) {
			nkeys = 1;
		} else {
			/*
			 * Reallocate, doubling size.
			 */
			nkeys = keys_allocated * 2;
		}
		p = (keys_t *) realloc(keys, (nkeys * sizeof (keys_t)));
		if (p == NULL)
			return (ENOMEM);
		keys_allocated = nkeys;
		keys = p;
	}

	/* key index is the key value minus one, since 0 is an invalid key */
	keys[keys_used].destructor = destruct;
	keys[keys_used].value = NULL;
	*pkey = ++keys_used;
	return (0);
}


#ifdef DELETE_SUPPORTED
int
_libc_thr_key_delete(thread_key_t key)
{
	/* check for out-of-range key */
	if ((key == 0) || (key > keys_used))
		return (EINVAL);

	/* check for already deleted key */
	if (keys[key - 1].destructor == nulldestructor)
		return (EINVAL);

	keys[key - 1].destructor = nulldestructor;

	return (0);
}
#endif /* DELETE_SUPPORTED */


int
_libc_thr_getspecific(thread_key_t key, void **valuep)
{
	/* check for out-of-range key */
	if ((key == 0) || (key > keys_used))
		return (EINVAL);

#ifdef DELETE_SUPPORTED
	/* check for deleted key */
	if (keys[key - 1].destructor == nulldestructor)
		return (EINVAL);
#endif /* DELETE_SUPPORTED */

	*valuep = keys[key - 1].value;
	return (0);
}


int
_libc_thr_setspecific(thread_key_t key, void *value)
{
	/* check for out-of-range key */
	if ((key == 0) || (key > keys_used))
		return (EINVAL);

#ifdef DELETE_SUPPORTED
	/* check for deleted key */
	if (keys[key - 1].destructor == nulldestructor)
		return (EINVAL);
#endif /* DELETE_SUPPORTED */

	keys[key - 1].value = value;
	return (0);
}

void *
_libc_pthread_getspecific(thread_key_t key)
{
	void    *value;

	if (_libc_thr_getspecific((thread_key_t) key, &value) < 0)
		return (NULL);
	else
		return (value);
}

