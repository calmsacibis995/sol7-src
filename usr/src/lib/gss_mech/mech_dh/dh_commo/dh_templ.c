/*
 *	dh_template.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dh_template.c	1.1	97/11/19 SMI"

#include <stdlib.h>
#include <string.h>
#include <dh_gssapi.h>
#include "../dh_common/dh_common.h"

extern int key_encryptsession_pk_g();
extern int key_decryptsession_pk_g();
extern int key_gendes_g();
extern int key_secretkey_is_set_g();

static int __encrypt(const char *remotename, des_block deskeys[], int no_keys);
static int __decrypt(const char *remotename, des_block deskeys[], int no_keys);
static int __gendes(des_block deskeys[], int no_keys);
static int __secret_is_set(void);
static char *__get_principal(void);

static keyopts_desc dh_keyopts = {
	__encrypt,
	__decrypt,
	__gendes,
	__secret_is_set,
	__get_principal
};

static struct gss_config  dh_mech;

gss_mechanism
gss_mech_initialize()
{
	gss_mechanism mech;

	mech = __dh_generic_initialize(&dh_mech, OID, &dh_keyopts);

	if (mech == NULL) {
		return (NULL);
	}

	return (mech);
}


int
__rpcsec_gss_is_server(void)
{
	return (0);
}


static int
dh_getpublickey(const char *remotename, keylen_t keylen, algtype_t alg,
    char *pk, size_t plen)
{
	int key_cached = __rpcsec_gss_is_server();

	return (__getpublickey_cached_g(remotename, keylen, alg, pk,
	    plen, &key_cached));
}

static int __encrypt(const char *remotename, des_block deskeys[], int no_keys)
{
	char pk[HEX_KEY_BYTES+1];

	if (!dh_getpublickey(remotename, KEYLEN, 0, pk, sizeof (pk)))
		return (-1);

	if (key_encryptsession_pk_g(remotename, pk,
	    KEYLEN, ALGTYPE, deskeys, no_keys))
			return (-1);
	return (0);
}

static int __decrypt(const char *remotename, des_block deskeys[], int no_keys)
{
	char pk[HEX_KEY_BYTES+1];

	if (!dh_getpublickey(remotename, KEYLEN, 0, pk, sizeof (pk)))
		return (-1);

	if (key_decryptsession_pk_g(remotename, pk,
	    KEYLEN, ALGTYPE, deskeys, no_keys))
			return (-1);
	return (0);
}

static int __gendes(des_block deskeys[], int no_keys)
{

	memset(deskeys, 0, no_keys* sizeof (des_block));
	if (key_gendes_g(deskeys, no_keys))
			return (-1);

	return (0);
}

static int __secret_is_set(void)
{
	return (key_secretkey_is_set_g(KEYLEN, ALGTYPE));
}

static char * __get_principal(void)
{
	char netname[MAXNETNAMELEN+1];

	if (getnetname(netname))
		return (strdup(netname));

	return (NULL);
}
