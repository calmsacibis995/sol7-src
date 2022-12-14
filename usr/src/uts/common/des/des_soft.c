/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)des_soft.c	1.8	97/09/22 SMI"	/* SVr4.0 1.3	*/

/*
 *  		PROPRIETARY NOTICE (Combined)
 *
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *
 *
 *
 *  		Copyright Notice
 *
 *  Notice of copyright on this source code product does not indicate
 *  publication.
 *
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 */

/*
 * Warning!  Things are arranged very carefully in this file to
 * allow read-only data to be moved to the text segment.  The
 * various DES tables must appear before any function definitions
 * (this is arranged by including them immediately below) and partab
 * must also appear before and function definitions
 * This arrangement allows all data up through the first text to
 * be moved to text.
 */

/*
 * Fast (?) software implementation of DES
 * Has been seen going at 2000 bytes/sec on a Sun-2
 * Works on a VAX too.
 * Won't work without 8 bit chars and 32 bit longs
 */

#include <sys/types.h>
#include <des/des.h>
#include <des/softdes.h>
#include <des/desdata.h>
#include <sys/debug.h>

static void des_setkey(u_char userkey[8], struct deskeydata *kd,
    unsigned int dir);
static void des_encrypt(u_char *data, struct deskeydata *kd);


/*
 * Software encrypt or decrypt a block of data (multiple of 8 bytes)
 * Do the CBC ourselves if needed.
 */
int
_des_crypt(char *buf, size_t len, struct desparams *desp)
{
}


/*
 * Set the key and direction for an encryption operation
 * We build the 16 key entries here
 */
static void
des_setkey(u_char userkey[8], struct deskeydata *kd, unsigned int dir)
{
}



/*
 * Do an encryption operation
 * Much pain is taken (with preprocessor) to avoid loops so the compiler
 * can do address arithmetic instead of doing it at runtime.
 * Note that the byte-to-chunk conversion is necessary to guarantee
 * processor byte-order independence.
 */
static void
des_encrypt(u_char *data, struct deskeydata *kd)
{
}
