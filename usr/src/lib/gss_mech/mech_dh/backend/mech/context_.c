/*
 *	context_establish.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)context_establish.c	1.1	97/11/19 SMI"

#include <string.h>
#include "dh_gssapi.h"

static bool_t
gss_chanbind_cmp(gss_channel_bindings_t local, gss_channel_bindings_t remote)
{
	if (local == NULL)
		return (TRUE); /* local doesn't care so we won't either */

	if (remote == NULL)
		return (FALSE);

	return (memcmp(local, remote, sizeof (gss_channel_bindings_t *)));
}

static
OM_uint32
gen_accept_token(dh_gss_context_t ctx,
    gss_channel_bindings_t channel, gss_buffer_t output)
{
	token_desc token;
	key_set keys;
	context_t accept = &token.body.token_body_desc_u.accept_context.cntx;

	token.verno = DH_PROTO_VERSION;
	token.body.type = DH_ACCEPT_CNTX;
	accept->remote = ctx->local;
	accept->local = ctx->remote;
	accept->flags = ctx->flags;
	accept->expire = ctx->expire;
	accept->channel = (channel_binding_t)channel;
	keys.key_set_len = ctx->no_keys;
	keys.key_set_val = ctx->keys;

	return (__make_token(output, NULL, &token, &keys));
}

static OM_uint32
validate_cred(dh_context_t cntx, OM_uint32 *minor, dh_cred_id_t cred,
    gss_cred_usage_t usage, dh_principal *netname)
{
	*minor = 0;

	if (!cntx->keyopts->key_secretkey_is_set()) {
		*minor = DH_NO_SECRET;
		return (GSS_S_NO_CRED);
	}

	if ((*netname = cntx->keyopts->get_principal()) == NULL) {
		*minor = DH_NO_PRINCIPAL;
		return (GSS_S_NO_CRED);
	}

	if ((gss_cred_id_t)cred != GSS_C_NO_CREDENTIAL) {
		if ((cred->usage != usage &&
		    cred->usage != GSS_C_BOTH) ||
		    strcmp(*netname, cred->principal) != 0) {
			free(*netname);
			return (GSS_S_NO_CRED);
		}

		if (cred->expire != GSS_C_INDEFINITE &&
		    time(0) > cred->expire) {
			free(*netname);
			return (GSS_S_CREDENTIALS_EXPIRED);
		}
	}
	return (0);
}

OM_uint32
__dh_gss_accept_sec_context(void *ctx, OM_uint32 *minor,
    gss_ctx_id_t *gss_ctx, gss_cred_id_t cred, gss_buffer_t input,
    gss_channel_bindings_t  channel, gss_name_t *principal, gss_OID* mech,
    gss_buffer_t output, OM_uint32 *flags, OM_uint32 *expire,
    gss_cred_id_t *del_cred)
{
	token_desc token;
	dh_context_t dhctx = (dh_context_t)ctx;
	dh_gss_context_t g_cntx = NULL;
	dh_principal netname = NULL;
	init_context_t clnt;
	OM_uint32 stat, localminor;
	int i;
	signature sig;

	if (minor == NULL)
		minor = &localminor;

	*minor = 0;
	if (principal)
		*principal = NULL;
	if (mech)
		*mech = GSS_C_NO_OID;
	if (flags)
		*flags  = 0;
	if (expire)
		*expire = 0;
	if (del_cred)
		*del_cred = GSS_C_NO_CREDENTIAL;
	memset(output, 0, sizeof (*output));

	stat = validate_cred(dhctx, minor,
	    (dh_cred_id_t) cred, GSS_C_ACCEPT, &netname);
	if (stat != GSS_S_COMPLETE)
		return (stat);

	memset(&sig, 0, sizeof (sig));
	if (*minor = __get_ap_token(input, dhctx->mech, &token, &sig)) {
		free(netname);
		__free_signature(&sig);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	clnt = &token.body.token_body_desc_u.init_context;

	if (strcmp(clnt->cntx.local, netname) != 0) {
		*minor = DH_NOT_LOCAL;
		__free_signature(&sig);
		free(netname);
		return (GSS_S_DEFECTIVE_TOKEN);
	}
	free(netname);

	if (dhctx->keyopts->key_decryptsessions(clnt->cntx.remote,
						clnt->keys.key_set_val,
						clnt->keys.key_set_len)) {
		*minor = DH_SESSION_CIPHER_FAILURE;
		stat = GSS_S_BAD_SIG;
		goto cleanup;
	}

#ifdef DH_DEBUG
	fprintf(stderr, "Received session keys:\n");
	for (i = 0; i < clnt->keys.key_set_len; i++)
		fprintf(stderr, "%08.8x%08.8x ",
			clnt->keys.key_set_val[i].key.high,
			clnt->keys.key_set_val[i].key.low);
	fprintf(stderr, "\n");
#endif

	if ((*minor = __verify_sig(&token, DH_MECH_QOP, &clnt->keys, &sig))
	    != DH_SUCCESS) {
		stat = GSS_S_BAD_SIG;
		goto cleanup;
	}

	if (!gss_chanbind_cmp(channel,
	    (gss_channel_bindings_t) clnt->cntx.channel)) {
		stat = GSS_S_BAD_BINDINGS;
		goto cleanup;
	}

	if ((g_cntx = New(dh_gss_context_desc, 1)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		stat = GSS_S_FAILURE;
		goto cleanup;
	}

	g_cntx->established = 1;
	g_cntx->initiate = 0;
	if ((g_cntx->remote = strdup(clnt->cntx.remote)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		stat = GSS_S_FAILURE;
		goto cleanup;
	}

	if ((g_cntx->local = strdup(clnt->cntx.local)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		stat = GSS_S_FAILURE;
		goto cleanup;
	}

	g_cntx->no_keys = clnt->keys.key_set_len;
	if ((g_cntx->keys = New(des_block, g_cntx->no_keys)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		stat = GSS_S_FAILURE;
		goto cleanup;
	}

	for (i = 0; i < g_cntx->no_keys; i++)
		g_cntx->keys[i] = clnt->keys.key_set_val[i];

	g_cntx->flags = clnt->cntx.flags;
	g_cntx->expire = clnt->cntx.expire;

	/* Create output token if needed */
	if (g_cntx->flags & GSS_C_MUTUAL_FLAG) {
		if (*minor = gen_accept_token(g_cntx, channel, output)) {
			stat = GSS_S_FAILURE;
			goto cleanup;
		}
	}

	*gss_ctx = (gss_ctx_id_t)g_cntx;

	if (principal)
		*principal = (gss_name_t)strdup(g_cntx->remote);
	if (flags)
		*flags = g_cntx->flags;
	if (expire)
		*expire = g_cntx->expire;
	if (mech)
		*mech = dhctx->mech; /* XXX should this be duped? */

	__free_signature(&sig);
	xdr_free(xdr_token_desc, (char *)&token);

	return (GSS_S_COMPLETE);

cleanup:
	if (mech) {
		if (*mech)
			Free((*mech)->elements);
		Free(*mech);
		*mech = NULL;
	}
	if (g_cntx) {
		free(g_cntx->remote);
		free(g_cntx->local);
		Free(g_cntx->keys);
		Free(g_cntx);
	}
	__free_signature(&sig);
	xdr_free(xdr_token_desc, (char *)&token);

	return (stat);
}

static
OM_uint32
gen_init_token(dh_gss_context_t cntx, dh_context_t dhctx,
    gss_channel_bindings_t channel, gss_buffer_t result)
{
	token_desc token;
	init_context_t remote;
	key_set keys, ukeys;
	int i, stat;

	if ((keys.key_set_val = New(des_block, cntx->no_keys)) == NULL)
		return (DH_NOMEM_FAILURE);

	keys.key_set_len = cntx->no_keys;
	for (i = 0; i < cntx->no_keys; i++)
		keys.key_set_val[i] = cntx->keys[i];

	memset(&token, 0, sizeof (token));
	token.verno = DH_PROTO_VERSION;
	token.body.type = DH_INIT_CNTX;
	remote = &token.body.token_body_desc_u.init_context;
	remote->cntx.remote = cntx->local;
	remote->cntx.local = cntx->remote;
	remote->cntx.flags = cntx->flags;
	remote->cntx.expire = cntx->expire;
	remote->cntx.channel = (channel_binding_t)channel;
	remote->keys = keys;


	/* Encrypt the keys for the other side */

	if (dhctx->keyopts->key_encryptsessions(cntx->remote,
						keys.key_set_val,
						cntx->no_keys)) {
		Free(keys.key_set_val);
		return (DH_SESSION_CIPHER_FAILURE);
	}

	ukeys.key_set_len = cntx->no_keys;
	ukeys.key_set_val = cntx->keys;
	stat =  __make_ap_token(result, dhctx->mech, &token, &ukeys);
	Free(keys.key_set_val);

	return (stat);
}

static
OM_uint32
create_context(OM_uint32 *minor, dh_context_t cntx,
    dh_gss_context_t *gss_ctx, dh_principal netname, dh_principal target,
    gss_channel_bindings_t channel, OM_uint32 flags_req, OM_uint32 time_req,
    OM_uint32 *flags_rec, OM_uint32 *time_rec, gss_buffer_t results)
{
	dh_gss_context_t dh_gss_ctx;
	time_t now = time(0);
	OM_uint32 expire;

	/* create the Diffie-Hellman context */
	if ((*gss_ctx = dh_gss_ctx = New(dh_gss_context_desc, 1)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}

	dh_gss_ctx->established = 0;
	dh_gss_ctx->initiate = 1;
	if ((dh_gss_ctx->remote = strdup(target)) == NULL) {
		*minor = DH_NOMEM_FAILURE;
		goto cleanup;
	}
	dh_gss_ctx->local = netname;

	dh_gss_ctx->no_keys = 3;
	dh_gss_ctx->keys = New(des_block, 3);
	if (dh_gss_ctx->keys == NULL) {
		*minor = DH_NOMEM_FAILURE;
		goto cleanup;
	}
	if (cntx->keyopts->key_gendeskeys(dh_gss_ctx->keys, 3)) {
		*minor = DH_NOMEM_FAILURE;
		goto cleanup;
	}

#ifdef DH_DEBUG
	{
		int i;

		fprintf(stderr, "Generated session keys:\n");
		for (i = 0; i < dh_gss_ctx->no_keys; i++)
			fprintf(stderr, "%08.8x%08.8x ",
				dh_gss_ctx->keys[i].key.high,
				dh_gss_ctx->keys[i].key.low);
		fprintf(stderr, "\n");
	}
#endif

	/*
	 *  Need to add support for GSS_C_REPLAY_FLAG and
	 *  possibly GSS_C_SEQUENCE_FLAG. We don't support
	 *  GSS_C_ANON_FLAG and GSS_C_DELEG_FLAG.
	 */

	dh_gss_ctx->flags = (flags_req &
	    (GSS_C_MUTUAL_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG));

	/* This mechanism does integrity and privacy */
	dh_gss_ctx->flags |= GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG;

	if (flags_rec)
		*flags_rec = dh_gss_ctx->flags;

	expire = time_req ? time_req : GSS_C_INDEFINITE;
	dh_gss_ctx->expire = expire == GSS_C_INDEFINITE ?
		expire : expire + now;
	if (time_rec)
		*time_rec = expire;

	*minor = gen_init_token(dh_gss_ctx, cntx,
				channel, results);
	if (*minor != DH_SUCCESS)
		goto cleanup;

	return (dh_gss_ctx->flags & GSS_C_MUTUAL_FLAG ?
		GSS_S_CONTINUE_NEEDED : GSS_S_COMPLETE);
cleanup:
	/* This assumes that freeing a NULL is ok */

	free(dh_gss_ctx->remote);
	free(dh_gss_ctx->local);
	Free(dh_gss_ctx->keys);
	Free(dh_gss_ctx);
	*gss_ctx = NULL;

	return (GSS_S_FAILURE);
}


static
OM_uint32
continue_context(OM_uint32 *minor, gss_buffer_t token,
    dh_gss_context_t dh_gss_ctx, gss_channel_bindings_t channel)
{
	key_set keys;
	token_desc tok;
	context_t remote_ctx;
	gss_channel_bindings_t remote_chan;

	*minor = 0;
	if (token == NULL || token->length == 0)
		return (GSS_S_DEFECTIVE_TOKEN);

	keys.key_set_len = dh_gss_ctx->no_keys;
	keys.key_set_val = dh_gss_ctx->keys;

	if (*minor = __get_token(token, NULL, &tok, &keys))
		return (*minor == DH_VERIFIER_MISMATCH ?
			GSS_S_BAD_SIG : GSS_S_DEFECTIVE_TOKEN);


	if (tok.verno != DH_PROTO_VERSION || tok.body.type != DH_ACCEPT_CNTX) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	remote_ctx = &tok.body.token_body_desc_u.accept_context.cntx;
	if (strcmp(remote_ctx->remote, dh_gss_ctx->remote) ||
	    strcmp(remote_ctx->local, dh_gss_ctx->local)) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	remote_chan = (gss_channel_bindings_t)remote_ctx->channel;

	if (channel && !gss_chanbind_cmp(channel, remote_chan)) {
		xdr_free(xdr_token_desc, (char *)&tok);
		return (GSS_S_BAD_BINDINGS);
	}

	dh_gss_ctx->flags = remote_ctx->flags;
	dh_gss_ctx->established = TRUE;
	xdr_free(xdr_token_desc, (char *)&tok);

	return (GSS_S_COMPLETE);
}


OM_uint32
__dh_gss_init_sec_context(void *ctx, OM_uint32 *minor, gss_cred_id_t cred,
    gss_ctx_id_t *context, gss_name_t target, gss_OID mech,
    OM_uint32 req_flags, OM_uint32 time_req, gss_channel_bindings_t channel,
    gss_buffer_t input_token, gss_OID *mech_rec, gss_buffer_t output_token,
    OM_uint32 *flags_rec, OM_uint32 *time_rec)
{
	dh_context_t cntx = (dh_context_t)ctx;
	dh_gss_context_t dh_gss_ctx = (dh_gss_context_t)*context;
	dh_principal netname;
	dh_cred_id_t dh_cred = (dh_cred_id_t)cred;
	OM_uint32 stat;

	/* Set to sane state */
	*minor = 0;
	output_token->length = 0;
	output_token->value = NULL;
	if (mech_rec)
		*mech_rec = cntx->mech;   /* XXX should this be duped? */

	if ((mech != GSS_C_NULL_OID) &&
	    (!__OID_equal(mech, cntx->mech))) {
		return (GSS_S_BAD_MECH);
	}


	stat = validate_cred(cntx, minor, dh_cred, GSS_C_INITIATE, &netname);
	if (stat != GSS_S_COMPLETE)
		return (stat);


	/* validate target name */
	/*
	 * we could check that the target is in the proper form and
	 * possibly do a lookup up on the host part.
	 */

	/* checks for new context */
	if (dh_gss_ctx == GSS_C_NO_CONTEXT) {

		if (input_token != GSS_C_NO_BUFFER) {
			*minor = 0;
			return (GSS_S_DEFECTIVE_TOKEN);
		}

		stat =  create_context(minor, cntx, &dh_gss_ctx, netname,
		    target, channel, req_flags, time_req,
		    flags_rec, time_rec, output_token);

		*context = (gss_ctx_id_t)dh_gss_ctx;

	} else {
		/* Should validate dh_gss_ctx here */
		stat = continue_context(minor,
		    input_token, dh_gss_ctx, channel);
		free(netname);
	}

	return (stat);
}
