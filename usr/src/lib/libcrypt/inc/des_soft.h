/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 */

#ident	"@(#)des_soft.h 1.2	97/07/07 SMI"

#ifndef DES_SOFT_H
#define	DES_SOFT_H

#include <sys/des.h>

extern int	__des_crypt(char *, unsigned int, struct desparams *);
extern int	crypt_close_nolock(int p[2]);
extern void	des_encrypt1();
extern void	des_decrypt1();

#endif DES_SOFT_H
