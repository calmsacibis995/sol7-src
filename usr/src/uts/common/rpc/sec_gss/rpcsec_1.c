/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)rpcsec_gss_misc.c 1.19	97/11/11 SMI"

/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Header:
 * /afs/gza.com/product/secure/rel-eng/src/1.1/rpc/RCS/auth_gssapi_misc.c,v 1.10
 * 1994/10/27 12:39:23 jik Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <gssapi/gssapi.h>
#include <rpc/rpc.h>
#include <rpc/rpcsec_defs.h>

/*
 * The initial allocation size for dynamic allocation.
 */
#define	CKU_INITSIZE    2048

/*
 * The size of additional allocations, if required.  It is larger to
 * reduce the number of actual allocations.
 */
#define	CKU_ALLOCSIZE   8192


/*
 * Miscellaneous XDR routines.
 */
bool_t
__xdr_gss_buf(xdrs, buf)
	XDR		*xdrs;
	gss_buffer_t	buf;
{
	u_int cast_len, bound_len;

	/*
	 * We go through this contortion because size_t is a now a ulong,
	 * GSS-API uses ulongs.
	 */

	if (xdrs->x_op != XDR_DECODE) {
		bound_len = cast_len = (u_int) buf->length;
	} else {
		bound_len = (u_int)-1;
	}

	if (xdr_bytes(xdrs, (char **)&buf->value, &cast_len,
	    bound_len) == TRUE) {
		if (xdrs->x_op == XDR_DECODE)
			buf->length = cast_len;

		return (TRUE);
	}

	return (FALSE);
}

bool_t
__xdr_rpc_gss_creds(xdrs, creds)
	XDR			*xdrs;
	rpc_gss_creds		*creds;
{
	if (!xdr_u_int(xdrs, (u_int *)&creds->version) ||
				!xdr_u_int(xdrs, (u_int *)&creds->gss_proc) ||
				!xdr_u_int(xdrs, (u_int *)&creds->seq_num) ||
				!xdr_u_int(xdrs, (u_int *)&creds->service) ||
				!__xdr_gss_buf(xdrs, &creds->ctx_handle))
		return (FALSE);
	return (TRUE);
}

bool_t
__xdr_rpc_gss_init_arg(xdrs, init_arg)
	XDR			*xdrs;
	rpc_gss_init_arg	*init_arg;
{
	if (!__xdr_gss_buf(xdrs, init_arg))
		return (FALSE);
	return (TRUE);
}

bool_t
__xdr_rpc_gss_init_res(xdrs, init_res)
	XDR			*xdrs;
	rpc_gss_init_res	*init_res;
{
	if (!__xdr_gss_buf(xdrs, &init_res->ctx_handle) ||
			!xdr_u_int(xdrs, (u_int *)&init_res->gss_major) ||
			!xdr_u_int(xdrs, (u_int *)&init_res->gss_minor) ||
			!xdr_u_int(xdrs, (u_int *)&init_res->seq_window) ||
			!__xdr_gss_buf(xdrs, &init_res->token))
		return (FALSE);
	return (TRUE);
}

/*
 * Generic routine to wrap data used by client and server sides.
 */
bool_t
__rpc_gss_wrap_data(service, qop, context, seq_num, out_xdrs,
			xdr_func, xdr_ptr)
	OM_uint32		qop;
	rpc_gss_service_t	service;
	gss_ctx_id_t		context;
	u_int			seq_num;
	XDR			*out_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	OM_uint32		minor;
	gss_buffer_desc		in_buf, out_buf;
	XDR			temp_xdrs;
	char			*mp;
	bool_t			conf_state;
	bool_t			ret = FALSE;
	int			bigsize = 40000;

	/*
	 * Create a temporary XDR/buffer to hold the data to be wrapped.
	 */
	mp = (char *)kmem_alloc(bigsize, KM_SLEEP);
	out_buf.length = 0;

	xdrmem_create(&temp_xdrs, mp, bigsize, XDR_ENCODE);

	/*
	 * serialize the sequence number into tmp memory
	 */
	if (!xdr_u_int(&temp_xdrs, &seq_num))
		goto fail;

	/*
	 * serialize the arguments into tmp memory
	 */
	if (!(*xdr_func)(&temp_xdrs, xdr_ptr))
		goto fail;

	/*
	 * Data to be wrapped goes in in_buf.  If privacy is used,
	 * out_buf will have wrapped data (in_buf will no longer be
	 * needed).  If integrity is used, out_buf will have checksum
	 * which will follow the data in in_buf.
	 */
	in_buf.length = xdr_getpos(&temp_xdrs);
	in_buf.value = (char *) temp_xdrs.x_base;

	switch (service) {
	case rpc_gss_svc_privacy:
		if (kgss_seal(&minor, context, TRUE, qop, &in_buf,
				&conf_state, &out_buf) != GSS_S_COMPLETE)
			goto fail;
		in_buf.length = 0;	/* in_buf not needed */
		if (!conf_state)
			goto fail;
		break;
	case rpc_gss_svc_integrity:
		if (kgss_sign(&minor, context, qop, &in_buf,
				&out_buf) != GSS_S_COMPLETE)
			goto fail;
		break;
	default:
		goto fail;
	}

	/*
	 * write out in_buf and out_buf as needed
	 */
	if (in_buf.length != 0) {
		if (!__xdr_gss_buf(out_xdrs, &in_buf))
			goto fail;
	}

	if (!__xdr_gss_buf(out_xdrs, &out_buf))
		goto fail;
	ret = TRUE;
fail:
	kmem_free(mp, bigsize);
	if (out_buf.length != 0)
		(void) gss_release_buffer(&minor, &out_buf);
	return (ret);
}

/*
 * Generic routine to unwrap data used by client and server sides.
 */
bool_t
__rpc_gss_unwrap_data(service, context, seq_num, qop_check, in_xdrs,
			xdr_func, xdr_ptr)
	rpc_gss_service_t	service;
	gss_ctx_id_t		context;
	u_int			seq_num;
	OM_uint32		qop_check;
	XDR			*in_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	gss_buffer_desc		in_buf, out_buf;
	XDR			temp_xdrs;
	u_int			seq_num2;
	bool_t			conf;
	OM_uint32		major = GSS_S_COMPLETE, minor = 0;
	int			qop;

	in_buf.value = NULL;
	out_buf.value = NULL;

	/*
	 * Pull out wrapped data.  For privacy service, this is the
	 * encrypted data.  For integrity service, this is the data
	 * followed by a checksum.
	 */
	if (!__xdr_gss_buf(in_xdrs, &in_buf)) {
		return (FALSE);
	}

	if (service == rpc_gss_svc_privacy) {
		major = kgss_unseal(&minor, context, &in_buf, &out_buf, &conf,
					&qop);
		kmem_free(in_buf.value, in_buf.length);
		if (major != GSS_S_COMPLETE) {
			return (FALSE);
		}
		/*
		 * Keep the returned token (unencrypted data) in in_buf.
		 */
		in_buf.length = out_buf.length;
		in_buf.value = out_buf.value;

		/*
		 * If privacy was not used, or if QOP is not what we are
		 * expecting, fail.
		 */
		if (!conf || qop != qop_check)
			goto fail;

	} else if (service == rpc_gss_svc_integrity) {
		if (!__xdr_gss_buf(in_xdrs, &out_buf)) {
			return (FALSE);
		}
		major = kgss_verify(&minor, context, &in_buf, &out_buf,
				&qop);
		kmem_free(out_buf.value, out_buf.length);
		if (major != GSS_S_COMPLETE) {
			kmem_free(in_buf.value, in_buf.length);
			return (FALSE);
		}

		/*
		 * If QOP is not what we are expecting, fail.
		 */
		if (qop != qop_check)
			goto fail;
	}

	xdrmem_create(&temp_xdrs, in_buf.value, in_buf.length, XDR_DECODE);

	/*
	 * The data consists of the sequence number followed by the
	 * arguments.  Make sure sequence number is what we are
	 * expecting (i.e., the value in the header).
	 */
	if (!xdr_u_int(&temp_xdrs, &seq_num2))
		goto fail;
	if (seq_num2 != seq_num)
		goto fail;

	/*
	 * Deserialize the arguments into xdr_ptr, and release in_buf.
	 */
	if (!(*xdr_func)(&temp_xdrs, xdr_ptr)) {
		goto fail;
	}

	if (service == rpc_gss_svc_privacy)
		(void) gss_release_buffer(&minor, &in_buf);
	else
		kmem_free(in_buf.value, in_buf.length);
	XDR_DESTROY(&temp_xdrs);
	return (TRUE);
fail:
	XDR_DESTROY(&temp_xdrs);
	if (service == rpc_gss_svc_privacy)
		(void) gss_release_buffer(&minor, &in_buf);
	else
		kmem_free(in_buf.value, in_buf.length);
	return (FALSE);
}
