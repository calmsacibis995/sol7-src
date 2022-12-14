/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights reserved.
 *
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI
 * The copyright notice above does not evidence any
 * actual or intended publication of such source code.
 *
 *
 * pos4.c:  Implementation of internal POSIX.1b routines
 *
 */

#pragma ident   "@(#)pos4.c 1.2     97/07/30     SMI"

/*LINTLIBRARY*/

#include <sys/types.h>
#include <semaphore.h>
#include <synch.h>
#include <string.h>
#include <sys/lwp.h>
#include <sys/mutex.h>
#include <thread.h>
#include "pos4.h"

static void __init_libpos4(void);
static int _pos4_lwp_sema_init(lwp_sema_t *, int, int, void *);
static int _pos4_lwp_sema_destroy(lwp_sema_t *);

#pragma init(__init_libpos4)

static const pos4_jmptab_t libthread_jmptab = {
	(int(*)(void *, int, int, void *)) 	sema_init,
	(int(*)(void *)) 			sema_wait,
	(int(*)(void *)) 			sema_trywait,
	(int(*)(void *)) 			sema_post,
	(int(*)(void *)) 			sema_destroy,
};

static const pos4_jmptab_t nolibthread_jmptab = {
	(int(*)(void *, int, int, void *)) 	_pos4_lwp_sema_init,
	(int(*)(void *)) 			_lwp_sema_wait,
	(int(*)(void *)) 			_lwp_sema_trywait,
	(int(*)(void *)) 			_lwp_sema_post,
	(int(*)(void *)) 			_pos4_lwp_sema_destroy,
};

const pos4_jmptab_t *pos4_jmptab;

void __init_libpos4(void) {
	if (thr_main() == -1)
		pos4_jmptab = &nolibthread_jmptab;
	else
		pos4_jmptab = &libthread_jmptab;
}

/* LINTED */
int _pos4_lwp_sema_init(lwp_sema_t *sp, int count, int type, void *arg) {
	(void) _lwp_sema_init(sp, count);
	sp->type = type;
	return (0);
}

/* LINTED */
int _pos4_lwp_sema_destroy(lwp_sema_t *sp) {
	return (0);
}
