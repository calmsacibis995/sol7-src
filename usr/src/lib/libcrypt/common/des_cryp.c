/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)des_crypt.c	1.15	97/07/07 SMI"
/*LINTLIBRARY*/

#pragma weak des_crypt = _des_crypt
#pragma weak des_encrypt = _des_encrypt
#pragma weak des_setkey = _des_setkey

#include "synonyms.h"
#include "mtlib.h"
#include <sys/types.h>
#include <crypt.h>
#include "des_soft.h"

#if INTERNATIONAL
#include 	<errno.h>
#endif

#include <stdlib.h>
#include <thread.h>
#include <synch.h>
#include <sys/types.h> 



static void
des_setkey_nolock(const char *key)
{
}

void
des_setkey(const char *key)
{
}


static void
des_encrypt_nolock(char *block, int edflag)
{
}

void
des_encrypt(char *block, int edflag)
{
}



#define	IOBUF_SIZE	16

#ifdef _REENTRANT
static char *
_get_iobuf(thread_key_t *key, unsigned size)
{
	char *iobuf = NULL;

	if (thr_getspecific(*key, (void **)&iobuf) != 0) {
		if (_thr_keycreate(key, free) != 0) {
			return (NULL);
		}
	}

	if (!iobuf) {
		if (_thr_setspecific(*key, (void *)(iobuf = malloc(size)))
			!= 0) {
			if (iobuf)
				(void) free(iobuf);
			return (NULL);
		}
	}
	return (iobuf);
}
#endif /* _REENTRANT */

char *
des_crypt(const char *pw, const char *salt)
{
	return (0);
}
