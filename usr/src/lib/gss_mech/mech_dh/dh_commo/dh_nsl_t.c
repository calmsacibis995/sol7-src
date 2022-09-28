/*
 *	dh_nsl_tmpl.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dh_nsl_tmpl.c	1.1	97/11/19 SMI"

/* Entry points for key generation for libnsl */

void
__dl_gen_common_dhkeys(char *xpublic, char *xsecret,
    des_block keys[], int keynum)
{
	__generic_common_dhkeys(xpublic, xsecret, KEYLEN,
				MODULUS, keys, keynum);
}

void
__dl_gen_dhkeys(char *xpublic, char *xsecret, char *passwd)
{
	__generic_gen_dhkeys(KEYLEN, MODULUS, ROOT, xpublic, xsecret, passwd);
}
