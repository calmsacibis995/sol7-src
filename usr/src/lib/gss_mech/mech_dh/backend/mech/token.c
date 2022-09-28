/*
 *	token.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)token.c	1.2	97/11/21 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dh_gssapi.h"
#include "crypto.h"

#define	MSO_BIT (8*(sizeof (int) - 1))	/* Most significant octet bit */

static OM_uint32
__xdr_encode_token(XDR *, gss_buffer_t, token_t, key_set_t);

static OM_uint32
__xdr_decode_token(XDR *, gss_buffer_t, token_t, key_set_t, signature_t);

static int
get_der_length(unsigned char **buf, unsigned int buf_len, unsigned int *bytes)
{
	unsigned char *p = *buf;
	int length, new_length;
	int octets;

	*bytes = 0;
	if (buf_len < 1)
		return (-1);

	*bytes = 1;
	if (*p < 128) {
		*buf = p+1;
		return (*p);
	}

	octets = *p++ & 0x7f;
	*bytes += octets;
	if (octets > buf_len - 1)
		return (-1);

	for (length = 0; octets; octets--) {
		new_length = (length << 8) + *p++;
		if (new_length < length)  /* overflow */
			return (-1);
		length = new_length;
	}

	*buf = p;

	return (length);
}

static unsigned int
der_length_size(unsigned int len)
{
	int i;

	if (len < 128)
		return (1);

	for (i = 0; len; i++) {
		len >>= 8;
	}

	return (i+1);
}

static int
put_der_length(unsigned length, unsigned char **buf, unsigned int max_len)
{
	unsigned char *s = *buf, *p;
	unsigned int buf_len = 0;
	int i;

	if (buf == 0 || max_len < 1)
		return (-1);

	if (length < 128) {
		*s++ = length;
		*buf = s;
		return (0);
	}

	p = s + 1;
	buf_len = 0;
	for (i = MSO_BIT; i >= 0 && buf_len <= max_len; i -= 8) {
		unsigned int v;

		v = (length >> i) & 0xff;
		if (v) {
			buf_len += 1;
			*p++ = v;
		}
	}
	if (i >= 0)			/* buffer overflow */
		return (-1);

	*s = buf_len | 0x80;
	*buf = p;

	return (0);
}


static qop_t
get_qop(token_t t)
{
	switch (t->body.type) {
	case DH_INIT_CNTX:
	case DH_ACCEPT_CNTX:
		return (DH_MECH_QOP);
	case DH_MIC:
		return (t->body.token_body_desc_u.sign.qop);
	case DH_WRAP:
		return (t->body.token_body_desc_u.seal.mic.qop);
	default:
		return (DH_MECH_QOP);
	}
}

OM_uint32
__make_ap_token(gss_buffer_t result, gss_OID mech,
    token_t token, key_set_t keys)
{
	unsigned int size, hsize, token_size, app_size, oid_size, start;
	XDR xdrs;
	unsigned char *buf, *xdrmem;
	OM_uint32 stat;

	/* ASN.1 application 0 header */

	if ((stat = __alloc_sig(get_qop(token), &token->verifier))
	    != DH_SUCCESS)
		return (stat);

	token_size = xdr_sizeof((xdrproc_t)xdr_token_desc, (void *)token);

	/*
	 * The token itself needs to be pasted on to the ASN.1
	 * application header on BYTES_PER_XDR_UNIT boundry. So we may
	 *  need upto BYTES_PER_XDR_UNIT - 1 extra bytes.
	 */
	token_size += BYTES_PER_XDR_UNIT -1;

	oid_size = mech->length;
	oid_size += der_length_size(mech->length);
	oid_size += 1;   /* tag x06 for Oid */
	/* bytes to store the length */
	app_size = der_length_size(oid_size + token_size);

	hsize = app_size + oid_size;
	hsize += 1;  /* tag 0x60  for application 0 */
	size = hsize + token_size;

	buf = New(unsigned char, size);
	if (buf == NULL) {
		__free_signature(&token->verifier);
		return (DH_NOMEM_FAILURE);
	}

	result->value = buf;
	result->length = size;
	/*
	 * Token has to be on BYTES_PER_XDR_UNIT boundry. (RNDUP is
	 * from xdr.h)
	 */
	start = RNDUP(hsize);
	xdrmem = &buf[start];

	*buf++ = 0x60;
	put_der_length(oid_size + token_size, &buf, app_size);
	*buf++ = 0x06;
	put_der_length(mech->length, &buf, oid_size);
	memcpy(buf, mech->elements, mech->length);

	xdrmem_create(&xdrs, (caddr_t)xdrmem, token_size, XDR_ENCODE);
	if ((stat = __xdr_encode_token(&xdrs, NULL, token, keys))
	    != DH_SUCCESS) {
		__free_signature(&token->verifier);
		__dh_release_buffer(result);
	}

	__free_signature(&token->verifier);
	return (stat);
}

OM_uint32
__make_token(gss_buffer_t result, gss_buffer_t msg,
    token_t token, key_set_t keys)
{
	unsigned int token_size;
	XDR xdrs;
	unsigned char *buf;
	OM_uint32 stat;

	if ((stat = __alloc_sig(get_qop(token), &token->verifier))
	    != DH_SUCCESS)
		return (stat);

	token_size = xdr_sizeof((xdrproc_t)xdr_token_desc, (void *)token);

	buf = New(unsigned char, token_size);
	if (buf == NULL) {
		__free_signature(&token->verifier);
		return (DH_NOMEM_FAILURE);
	}

	result->length = token_size;
	result->value = (void *)buf;

	xdrmem_create(&xdrs, (char *)buf, token_size, XDR_ENCODE);
	if ((stat = __xdr_encode_token(&xdrs, msg, token, keys))
	    != DH_SUCCESS) {
		__free_signature(&token->verifier);
		__dh_release_buffer(result);
	}

	__free_signature(&token->verifier);
	return (stat);
}

OM_uint32
__get_ap_token(gss_buffer_t input,
    gss_OID mech, token_t token, signature_t sig)
{
	unsigned char *buf, *p;
	unsigned int oid_len, token_len, bytes, hsize;
	int len;
	OM_uint32 stat;
	XDR xdrs;

	p = buf = (unsigned char *)input->value;
	if (*p++ != 0x60)
		return (DH_DECODE_FAILURE);
	if ((len = get_der_length(&p, input->length - 1, &bytes)) < 0)
		return (DH_DECODE_FAILURE);
	if (input->length - 1 - bytes != len)
		return (DH_DECODE_FAILURE);
	hsize = 1 + bytes;
	if (*p++ != 0x06)
		return (DH_DECODE_FAILURE);
	oid_len = get_der_length(&p, len - 1, &bytes);
	hsize += 1 + bytes + oid_len;
	token_len = len - 1 - bytes - oid_len;
	if (token_len > len)
		return (DH_DECODE_FAILURE);

	if (mech->length != oid_len)
		return (DH_DECODE_FAILURE);
	if (memcmp(mech->elements, p, oid_len) != 0)
		return (DH_DECODE_FAILURE);

	hsize = RNDUP(hsize);
	p = &buf[hsize];

	xdrmem_create(&xdrs, (caddr_t)p, token_len, XDR_DECODE);
	memset(token, 0, sizeof (token_desc));
	memset(sig, 0, sizeof (*sig));
	if ((stat = __xdr_decode_token(&xdrs, NULL, token, NULL, sig))
	    != DH_SUCCESS) {
		xdr_free(xdr_token_desc, (char *)token);
		return (stat);
	}

	return (stat);
}

OM_uint32
__get_token(gss_buffer_t input, gss_buffer_t msg,
    token_t token, key_set_t keys)
{
	XDR xdrs;
	signature sig;
	OM_uint32 stat;

	xdrmem_create(&xdrs, (caddr_t)input->value, input->length, XDR_DECODE);

	memset(token, 0, sizeof (token_desc));
	memset(&sig, 0, sizeof (sig));
	if ((stat = __xdr_decode_token(&xdrs, msg, token, keys, &sig))
	    != DH_SUCCESS)
		xdr_free(xdr_token_desc, (char *)token);

	Free(sig.signature_val);

	return (stat);
}

/*
 * Warning this routine assumes that xdrs was created with xdrmem_create!
 */

static OM_uint32
__xdr_encode_token(register XDR *xdrs, gss_buffer_t msg,
    token_desc *objp, key_set_t keys)
{
	OM_uint32 stat;

	if (xdrs->x_op != XDR_ENCODE)
		return (DH_BADARG_FAILURE);

	if (!xdr_u_int(xdrs, &objp->verno))
		return (DH_ENCODE_FAILURE);
	if (!xdr_token_body_desc(xdrs, &objp->body))
		return (DH_ENCODE_FAILURE);

	stat = __mk_sig(get_qop(objp), xdrs->x_base,
	    xdr_getpos(xdrs), msg, keys, &objp->verifier);

	if (stat != DH_SUCCESS)
		return (stat);

	if (!xdr_signature(xdrs, &objp->verifier))
		return (DH_ENCODE_FAILURE);

	return (DH_SUCCESS);
}

static OM_uint32
__xdr_decode_token(register XDR *xdrs, gss_buffer_t msg, token_desc *objp,
    key_set_t keys, signature_t sig)
{
	OM_uint32 stat;

	if (xdrs->x_op != XDR_DECODE)
		return (DH_BADARG_FAILURE);

	if (!xdr_u_int(xdrs, &objp->verno))
		return (DH_DECODE_FAILURE);
	if (!xdr_token_body_desc(xdrs, &objp->body))
		return (DH_DECODE_FAILURE);

	if ((stat = __alloc_sig(get_qop(objp), sig)) != DH_SUCCESS)
		return (stat);

	stat = __mk_sig(get_qop(objp), xdrs->x_base,
	    xdr_getpos(xdrs), msg, keys, sig);
	if (stat != DH_SUCCESS)
		return (stat);

	if (!xdr_signature(xdrs, &objp->verifier))
		return (stat);

	if (keys && !__cmpsig(sig, &objp->verifier))
		return (DH_VERIFIER_MISMATCH);

	return (DH_SUCCESS);
}
