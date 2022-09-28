/*
 *	context.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)context.c	1.1	97/11/19 SMI"

#include <sys/note.h>
#include "dh_gssapi.h"

/*ARGSUSED*/
OM_uint32
__dh_gss_context_time(void *ctx, OM_uint32 * minor,
    gss_ctx_id_t context, OM_uint32* time)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
__dh_gss_delete_sec_context(void *ctx, OM_uint32 *minor,
    gss_ctx_id_t *context, gss_buffer_t token)
{
_NOTE(ARGUNUSED(ctx))
	dh_gss_context_t cntx = (dh_gss_context_t)*context;

	*minor = DH_SUCCESS;
	if (token) {
		token->length = 0;
		token->value = NULL;
	}
	if (cntx == NULL)
		return (GSS_S_COMPLETE);

	/* XXX validate context and unregister  */

	memset(cntx->keys, 0, cntx->no_keys * sizeof (des_block));
	free(cntx->remote);
	free(cntx->local);
	Free(cntx->keys);
	Free(cntx);

	*context = NULL;

	return (GSS_S_COMPLETE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_export_sec_context(void *ctx, OM_uint32 *minor,
    gss_ctx_id_t *context, gss_buffer_t token)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_import_sec_context(void * ctx, OM_uint32 *minor,
    gss_buffer_t token, gss_ctx_id_t *context)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_inquire_context(void *ctx, OM_uint32 *minor, gss_ctx_id_t context,
    gss_name_t *initiator, gss_name_t *acceptor, OM_uint32 *time_rec,
    gss_OID *mech, OM_uint32 *flags_rec, int *local, int *open)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_process_context_token(void *ctx, OM_uint32 *minor,
    gss_ctx_id_t context, gss_buffer_t token)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_wrap_size_limit(void *ctx, OM_uint32 *minor, gss_ctx_id_t context,
    int conf_req, gss_qop_t qop_req, OM_uint32 output_size,
    OM_uint32 *input_size)
{
	return (GSS_S_UNAVAILABLE);
}
