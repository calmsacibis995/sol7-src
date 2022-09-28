/*
 *	crypto.h
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#ifndef _CRYPTO_H_
#define	_CRYPTO_H_

#pragma ident	"@(#)crypto.h	1.1	97/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <rpc/des_crypt.h>
#include <dh_gssapi.h>
#include <md5.h>

typedef enum { ENCIPHER, DECIPHER } cipher_mode_t;

typedef OM_uint32 (*cipher_proc)(gss_buffer_t buf,
    key_set_t keys, cipher_mode_t mode);
typedef OM_uint32 (*verifier_proc)(gss_buffer_t tok, gss_buffer_t msg,
    cipher_proc signer, key_set_t keys, gss_buffer_t signature);

/* Proto types */

void
__MD5UpdtLong(MD5_CTX *ctx, unsigned long l);

void
__MD5UpdtString(MD5_CTX *ctx, const char *s);

void
__dh_release_buffer(gss_buffer_t b);

bool_t
__dh_is_valid_QOP(qop_t qop);

OM_uint32
__QOPSeal(qop_t qop, gss_buffer_t input, int conf_req,
    key_set_t keys, gss_buffer_t output, int *conf_ret);

OM_uint32
__QOPUnSeal(qop_t qop, gss_buffer_t input, int conf_req,
    key_set_t keys, gss_buffer_t output);

bool_t
__cmpsig(signature_t, signature_t);

OM_uint32
__verify_sig(token_t, qop_t, key_set_t, signature_t);

OM_uint32
__mk_sig(qop_t, char *, long, gss_buffer_t, key_set_t, signature_t);

OM_uint32
__alloc_sig(qop_t, signature_t);

bool_t
__dh_is_valid_QOP(qop_t);

void
__free_signature(signature_t sig);

#ifdef __cplusplus
}
#endif

#endif /* _CRYPTO_H_ */
