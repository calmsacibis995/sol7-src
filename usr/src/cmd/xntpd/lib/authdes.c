/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)authdes.c	1.2	97/05/12 SMI"

/*
 * authdes.c - an implementation of the DES cipher algorithm for NTP
 */
#include "ntp_stdlib.h"

/*
 * Permute the key to give us our key schedule.
 */
void
DESauth_subkeys(key, encryptkeys, decryptkeys)
	const u_int32 *key;
	u_char *encryptkeys;
	u_char *decryptkeys;
{
}



/*
 * DESauth_des - perform an in place DES encryption on 64 bits
 *
 * Note that the `data' argument is always in big-end-first
 * byte order, i.e. *(char *)data is the high order byte of
 * the 8 byte data word.  We modify the initial and final
 * permutation computations for little-end-first machines to
 * swap bytes into the natural host order at the beginning and
 * back to big-end order at the end.  This is unclean but avoids
 * a byte swapping performance penalty on Vaxes (which are slow already).
 */
void
DESauth_des(data, subkeys)
	u_int32 *data;
	u_char *subkeys;
{
}
