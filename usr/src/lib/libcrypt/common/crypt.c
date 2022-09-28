/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)crypt.c	1.8	97/07/07 SMI"
/*LINTLIBRARY*/

#pragma weak setkey = _setkey
#pragma weak encrypt = _encrypt
#pragma weak crypt = _crypt

#include "synonyms.h"
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <crypt.h> 

void
setkey(const char *key)
{
	(void) des_setkey(key);
}

void
encrypt(char *block, int edflag)
{
	(void) des_encrypt(block, edflag);
}

char *
crypt(const char *pw, const char *salt)
{
	return (des_crypt(pw, salt));
}
