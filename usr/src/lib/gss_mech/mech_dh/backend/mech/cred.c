/*
 *	cred.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)cred.c	1.2	98/01/22 SMI"

#include <unistd.h>
#include <sys/note.h>
#include "dh_gssapi.h"

OM_uint32
__dh_gss_acquire_cred(void *ctx, OM_uint32 *minor, gss_name_t principal,
    OM_uint32  expire_req, gss_OID_set desired_mechs, gss_cred_usage_t usage,
    gss_cred_id_t *cred, gss_OID_set *mechs, OM_uint32 *expire_rec)
{
	dh_context_t cntx = (dh_context_t)ctx;
	dh_principal netname;
	dh_cred_id_t dh_cred;

	*minor = 0;
	if (mechs)
		*mechs = GSS_C_NO_OID_SET;
	*expire_rec = 0;
	*cred = GSS_C_NO_CREDENTIAL;

	if (desired_mechs != GSS_C_NO_OID_SET &&
	    !__OID_is_member(desired_mechs, cntx->mech))
		return (GSS_S_BAD_MECH);

	if (!cntx->keyopts->key_secretkey_is_set())
		return (GSS_S_NO_CRED);

	if ((netname = cntx->keyopts->get_principal()) == NULL)
		return (GSS_S_NO_CRED);

	if (principal &&
	    strncmp(netname, (char *)principal, MAXNETNAMELEN) != 0) {
		Free(netname);
		return (GSS_S_NO_CRED);
	}

	dh_cred = New(dh_cred_id_desc, 1);
	if (dh_cred == NULL) {
		Free(netname);
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}
	dh_cred->uid = geteuid();
	dh_cred->usage = usage;
	dh_cred->principal = netname;
	dh_cred->expire = expire_req ? time(0) + expire_req : GSS_C_INDEFINITE;

	if (mechs && (*minor = __OID_to_OID_set(mechs, cntx->mech))) {
		Free(dh_cred);
		Free(netname);
		return (GSS_S_FAILURE);
	}

	*expire_rec = expire_req ? expire_req : GSS_C_INDEFINITE;
	*cred  = (gss_cred_id_t)dh_cred;

	return (GSS_S_COMPLETE);
}


OM_uint32
/*ARGSUSED*/
__dh_gss_add_cred(void * ctx, OM_uint32 *minor, gss_cred_id_t cred_in,
    gss_name_t name, gss_OID mech, gss_cred_usage_t usage,
    OM_uint32 init_time_req, OM_uint32 accep_time_req,
    gss_cred_id_t *cred_out, gss_OID_set *mechs,
    OM_uint32 *init_time_rec, OM_uint32 *accep_time_rec)
{
	return (GSS_S_UNAVAILABLE);
}


OM_uint32
/*ARGSUSED*/
__dh_gss_inquire_cred(void *ctx, OM_uint32 *minor, gss_cred_id_t cred,
    gss_name_t *name, OM_uint32 *lifetime, gss_cred_usage_t *usage,
    gss_OID_set *mechs)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
/*ARGSUSED*/
__dh_gss_inquire_cred_by_mech(void *ctx, OM_uint32 *minor, gss_cred_id_t cred,
    gss_OID mech, gss_name_t *name, OM_uint32 *init_time,
    OM_uint32 *accep_time, gss_cred_usage_t *usage)
{
	return (GSS_S_UNAVAILABLE);
}

OM_uint32
__dh_gss_release_cred(void *ctx, OM_uint32 *minor, gss_cred_id_t *cred)
{
_NOTE(ARGUNUSED(ctx))
	dh_cred_id_t dh_cred = (dh_cred_id_t)*cred;
	*minor = 0;
	Free(dh_cred->principal);
	Free(dh_cred);
	*cred = GSS_C_NO_CREDENTIAL;

	return (GSS_S_COMPLETE);
}
