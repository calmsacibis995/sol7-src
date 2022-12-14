/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *	PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *	Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		All rights reserved.
 */

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)des.c	1.13	98/01/30 SMI"
/*LINTLIBRARY*/

/*
 * DES encryption library routines
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#include <sys/types.h>


/*
 * CBC mode encryption
 */
cbc_crypt(char *key, char *buf, size_t len, unsigned int mode, char *ivec)
{
	int err = 0;

	return (err);
}


/*
 * ECB mode encryption
 */
ecb_crypt(char *key, char *buf, size_t len, unsigned int mode)
{
	int ret = 0;

	return (ret);
}


