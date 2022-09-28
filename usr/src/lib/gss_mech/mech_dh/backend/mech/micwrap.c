/*
 *	MICwrap.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)MICwrap.c	1.1	97/11/19 SMI"

#include <sys/note.h>
#include "dh_gssapi.h"
#include "crypto.h"

OM_uint32
__dh_gss_sign(void *ctx, OM_uint32 *minor, gss_ctx_id_t context,
    int qop_req, gss_buffer_t message, gss_buffer_t token)
{
_NOTE(ARGUNUSED(ctx))
	dh_gss_context_t cntx = (dh_gss_context_t)context;
	token_desc tok;
	mic_t mic = &tok.body.token_body_desc_u.sign;
	key_set keys;
	OM_uint32 stat;

	if ((*minor = __dh_validate_context(cntx)) != DH_SUCCESS)
		return (GSS_S_NO_CONTEXT);

	if (cntx->established != 1)
		return (GSS_S_NO_CONTEXT);

	keys.key_set_len = cntx->no_keys;
	keys.key_set_val = cntx->keys;

	tok.verno = DH_PROTO_VERSION;
	tok.body.type = DH_MIC;

	mic->qop = qop_req;

	mic->seqnum = ++cntx->next_seqno;

	if ((*minor = __make_token(token, message, &tok, &keys))
	    != DH_SUCCESS) {
		return (GSS_S_FAILURE);
	}

	stat = GSS_S_COMPLETE;
	if (stat == GSS_S_COMPLETE && cntx->expire != GSS_C_INDEFINITE)
		if (cntx->expire < time(0))
			stat = GSS_S_CONTEXT_EXPIRED;

	return (stat);
}

OM_uint32
__dh_gss_verify(void *ctx, OM_uint32 *minor, gss_ctx_id_t context,
    gss_buffer_t message, gss_buffer_t token, int *qop)
{
_NOTE(ARGUNUSED(ctx))
	dh_gss_context_t cntx = (dh_gss_context_t)context;
	token_desc tok;
	mic_t mic = &tok.body.token_body_desc_u.sign;
	key_set keys;
	OM_uint32 stat;

	*qop = 0;

	if ((*minor = __dh_validate_context(cntx)) != DH_SUCCESS)
		return (GSS_S_NO_CONTEXT);

	if (cntx->established != 1)
		return (GSS_S_NO_CONTEXT);

	keys.key_set_len = cntx->no_keys;
	keys.key_set_val = cntx->keys;

	if ((*minor = __get_token(token, message,
	    &tok, &keys)) != DH_SUCCESS) {
		switch (*minor) {
		case DH_DECODE_FAILURE:
			return (GSS_S_DEFECTIVE_TOKEN);
		case DH_VERIFIER_MISMATCH:
			return (GSS_S_BAD_SIG);
		default:
			return (GSS_S_FAILURE);
		}
	}

	if (tok.verno != DH_PROTO_VERSION ||
	    tok.body.type != DH_MIC) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	*qop = mic->qop;

	cntx->seqno = mic->seqnum;
	/* Sequence & Replay detection here */

	xdr_free(xdr_token_desc, (char *)&tok);

	stat = GSS_S_COMPLETE;
	if (stat == GSS_S_COMPLETE && cntx->expire != GSS_C_INDEFINITE)
		if (cntx->expire < time(0))
			stat = GSS_S_CONTEXT_EXPIRED;

	return (stat);
}

OM_uint32
__dh_gss_seal(void * ctx, OM_uint32 *minor, gss_ctx_id_t context,
    int conf_req, int qop_req, gss_buffer_t input, int *conf_state,
    gss_buffer_t output)
{
_NOTE(ARGUNUSED(ctx))
	dh_gss_context_t cntx = (dh_gss_context_t)context;
	token_desc tok;
	wrap_t wrap = &tok.body.token_body_desc_u.seal;
	key_set keys;
	gss_buffer_desc body;
	OM_uint32 stat;

	if ((*minor = __dh_validate_context(cntx)) != DH_SUCCESS)
		return (GSS_S_NO_CONTEXT);

	if (cntx->established != 1)
		return (GSS_S_NO_CONTEXT);

	keys.key_set_len = cntx->no_keys;
	keys.key_set_val = cntx->keys;

	tok.verno = DH_PROTO_VERSION;
	tok.body.type = DH_WRAP;

	wrap->mic.qop = qop_req;

	wrap->mic.seqnum = ++cntx->next_seqno;

	if ((*minor = __QOPSeal(wrap->mic.qop, input, conf_req,
	    &keys, &body, conf_state)) != DH_SUCCESS) {
		__free_signature(&tok.verifier);
		return (GSS_S_FAILURE);
	}

	wrap->body.body_len = body.length;
	wrap->body.body_val = (char *)body.value;
	wrap->conf_flag = *conf_state;

	if ((*minor = __make_token(output, NULL, &tok, &keys)) != DH_SUCCESS) {
		__dh_release_buffer(&body);
		return (GSS_S_FAILURE);
	}
	__dh_release_buffer(&body);

	stat = GSS_S_COMPLETE;
	if (stat == GSS_S_COMPLETE && cntx->expire != GSS_C_INDEFINITE)
		if (cntx->expire < time(0))
			stat = GSS_S_CONTEXT_EXPIRED;

	return (stat);
}

OM_uint32
__dh_gss_unseal(void *ctx, OM_uint32 *minor, gss_ctx_id_t context,
    gss_buffer_t input, gss_buffer_t output, int *conf_state, int *qop_used)
{
_NOTE(ARGUNUSED(ctx))
	dh_gss_context_t cntx = (dh_gss_context_t)context;
	token_desc tok;
	wrap_t wrap = &tok.body.token_body_desc_u.seal;
	key_set keys;
	gss_buffer_desc message;
	OM_uint32 stat;

	*qop_used = 0;

	if ((*minor = __dh_validate_context(cntx)) != DH_SUCCESS)
		return (GSS_S_NO_CONTEXT);

	if (cntx->established != 1)
		return (GSS_S_NO_CONTEXT);

	keys.key_set_len = cntx->no_keys;
	keys.key_set_val = cntx->keys;

	if ((*minor = __get_token(input, NULL, &tok, &keys)) != DH_SUCCESS) {
		switch (*minor) {
		case DH_DECODE_FAILURE:
		case DH_UNKNOWN_QOP:
			return (GSS_S_DEFECTIVE_TOKEN);
		case DH_VERIFIER_MISMATCH:
			return (GSS_S_BAD_SIG);
		default:
			return (GSS_S_FAILURE);
		}
	}

	*qop_used = wrap->mic.qop;
	*conf_state = wrap->conf_flag;

	if (tok.verno != DH_PROTO_VERSION ||
	    tok.body.type != DH_WRAP) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	message.length = wrap->body.body_len;
	message.value = wrap->body.body_val;

	if ((*minor = __QOPUnSeal(*qop_used, &message,
				*conf_state, &keys, output))
	    != DH_SUCCESS) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (*minor == DH_UNKNOWN_QOP ?
				GSS_S_DEFECTIVE_TOKEN : GSS_S_FAILURE);
	}

	/* Sequence & Replay detection here */
	cntx->seqno = wrap->mic.seqnum;

	stat = GSS_S_COMPLETE;
	if (stat == GSS_S_COMPLETE && cntx->expire != GSS_C_INDEFINITE)
		if (cntx->expire < time(0))
			stat = GSS_S_CONTEXT_EXPIRED;

	xdr_free(xdr_token_desc, (char *)&tok);

	return (stat);
}
