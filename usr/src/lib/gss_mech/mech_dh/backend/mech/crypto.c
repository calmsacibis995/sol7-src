/*
 *	crypto.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)crypto.c	1.1	97/11/19 SMI"

#include <sys/note.h>
#include "dh_gssapi.h"
#include "crypto.h"

void
__free_signature(signature_t sig)
{
	Free(sig->signature_val);
	sig->signature_val = NULL;
	sig->signature_len = 0;
}

void
__dh_release_buffer(gss_buffer_t b)
{
	Free(b->value);
	b->length = 0;
	b->value = NULL;
}


void
__MD5UpdtLong(MD5_CTX *ctx, unsigned long l)
{
	unsigned long nl = htonl(l);
	MD5Update(ctx, (unsigned char *)&nl, sizeof (nl));
}

void
__MD5UpdtString(MD5_CTX *ctx, const char *s)
{
	MD5Update(ctx, (unsigned char *)s, strlen(s)+1);
}


bool_t
__dh_is_valid_QOP(qop_t qop)
{
	bool_t is_valid = FALSE;


	return (is_valid);
}

OM_uint32
__alloc_sig(qop_t qop, signature_t sig)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;


	return (stat);
}

OM_uint32
__mk_sig(qop_t qop, char *tok, long len, gss_buffer_t mesg,
    key_set_t keys, signature_t sig)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;


	return (stat);
}

OM_uint32
__verify_sig(token_t token, qop_t qop, key_set_t keys, signature_t sig)
{
	OM_uint32 stat = DH_VERIFIER_FAILURE;


	return (stat);
}

bool_t
__cmpsig(signature_t s1, signature_t s2)
{
	return (s1->signature_len == s2->signature_len &&
	    memcmp(s1->signature_val,
		s2->signature_val, s1->signature_len) == 0);
}

static OM_uint32
wrap_msg_body(gss_buffer_t in, gss_buffer_t out, unsigned int pad)
{
	XDR xdrs;
	unsigned int len;

	len = ((RNDUP(in->length + sizeof (OM_uint32)) + pad - 1) / pad) * pad;
	if ((out->value = (void *)New(char, len)) == NULL)
		return (DH_NOMEM_FAILURE);
	out->length = len;
	xdrmem_create(&xdrs, out->value, out->length, XDR_ENCODE);
	if (!xdr_bytes(&xdrs, (char **)&in->value,
	    (unsigned int *)&in->length, len)) {
		__dh_release_buffer(out);
		return (DH_ENCODE_FAILURE);
	}

	return (DH_SUCCESS);
}

OM_uint32
__QOPSeal(qop_t qop, gss_buffer_t input, int conf_req,
	key_set_t keys, gss_buffer_t output, int *conf_ret)
{
	OM_uint32 stat = DH_CIPHER_FAILURE;


	return (stat);
}

static OM_uint32
unwrap_msg_body(gss_buffer_t in, gss_buffer_t out)
{
	XDR xdrs;

	xdrmem_create(&xdrs, in->value, in->length, XDR_DECODE);
	if (!xdr_bytes(&xdrs, (char **)&out->value,
	    (unsigned int *)&out->length, in->length))
		return (DH_DECODE_FAILURE);

	return (DH_SUCCESS);
}

OM_uint32
__QOPUnSeal(qop_t qop, gss_buffer_t input, int conf_req,
    key_set_t keys, gss_buffer_t output)
{
	OM_uint32 stat = DH_CIPHER_FAILURE;


	return (stat);
}
