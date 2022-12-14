/*
 * Copyright (c) 1995,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gssd_clnt_stubs.c	1.83	98/02/20 SMI"

/*
 *  GSSAPI library stub module for gssd.
 */

#include <mechglueP.h>
#include "gssd_prot.h"
#include <rpc/rpc.h>

#include <sys/systm.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/kmem.h>
#include <gssapi/kgssapi_defs.h>
#include <sys/debug.h>

#ifdef GSSDEBUG
/*
 * Kernel kgssd module debugging aid. The global variable "gss_log"
 * is a bit mask which allows various types of debugging messages
 * to be printed out.
 *
 *	 gss_log & 1  will cause actual failures to be printed.
 *	 gss_log & 2  will cause informational messages to be
 *	                 printed on the client side of kgssd.
 *	 gss_log & 4  will cause informational messages to be
 *	                 printed on the server side of kgssd.
 *	 gss_log & 8  will cause informational messages to be
 *	                 printed on both client and server side of kgssd.
 */

u_int gss_log = 1;

#endif /* GSSDEBUG */

char *server = "localhost";

static OM_uint32 kgss_sign_wrapped(void *, OM_uint32 *, gss_ctx_id_t, int,
	gss_buffer_t, gss_buffer_t,  OM_uint32);

static OM_uint32 kgss_verify_wrapped(void *, OM_uint32 *, gss_ctx_id_t,
	gss_buffer_t, gss_buffer_t, int *qop_state, OM_uint32);

static OM_uint32 kgss_seal_wrapped(void *, OM_uint32 *, gss_ctx_id_t,
	int, int, gss_buffer_t, int *, gss_buffer_t,  OM_uint32);

static OM_uint32 kgss_unseal_wrapped(void *, OM_uint32 *, gss_ctx_id_t,
	gss_buffer_t, gss_buffer_t, int *conf_state, int *qop_state,
	OM_uint32);

static OM_uint32 kgss_delete_sec_context_wrapped(void *, OM_uint32 *,
	gssd_ctx_id_t *, gss_buffer_t, OM_uint32);

static void __kgss_reset_mech(gss_mechanism *, gss_OID);

#define	DEFAULT_MINOR_STAT	((OM_uint32) ~0)

OM_uint32
kgss_acquire_cred_wrapped(minor_status,
			desired_name,
			time_req,
			desired_mechs,
			cred_usage,
			output_cred_handle,
			actual_mechs,
			time_rec,
			uid,
			gssd_cred_verifier)
	OM_uint32 *minor_status;
	const gss_name_t desired_name;
	OM_uint32 time_req;
	const gss_OID_set desired_mechs;
	int cred_usage;
	gssd_cred_id_t *output_cred_handle;
	gss_OID_set *actual_mechs;
	OM_uint32 *time_rec;
	uid_t uid;
	OM_uint32 *gssd_cred_verifier;
{
	CLIENT *clnt;

	OM_uint32 	minor_status_temp;
	gss_buffer_desc	external_name;
	gss_OID		name_type;
	int		i;

	gss_acquire_cred_arg arg;
	gss_acquire_cred_res res;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_acquire_cred: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}


	/* convert the desired name from internal to external format */

	if (gss_display_name(&minor_status_temp, desired_name, &external_name,
				&name_type) != GSS_S_COMPLETE) {

		*minor_status = (OM_uint32) minor_status_temp;
		(void) gss_release_buffer(&minor_status_temp, &external_name);
		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_acquire_cred: display name failed\n");
		return ((OM_uint32) GSS_S_FAILURE);
	}


	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.desired_name.GSS_BUFFER_T_len = (u_int) external_name.length;
	arg.desired_name.GSS_BUFFER_T_val = (char *) external_name.value;

	arg.name_type.GSS_OID_len =
		name_type == GSS_C_NULL_OID ?
			0 : (u_int) name_type->length;

	arg.name_type.GSS_OID_val =
		name_type == GSS_C_NULL_OID ?
			(char *) NULL : (char *) name_type->elements;

	arg.time_req = time_req;

	if (desired_mechs != GSS_C_NULL_OID_SET) {
		arg.desired_mechs.GSS_OID_SET_len =
			(u_int) desired_mechs->count;
		arg.desired_mechs.GSS_OID_SET_val = (GSS_OID *)
			MALLOC(sizeof (GSS_OID) * desired_mechs->count);

		for (i = 0; i < desired_mechs->count; i++) {
		    arg.desired_mechs.GSS_OID_SET_val[i].GSS_OID_len =
			(u_int) desired_mechs->elements[i].length;
		    arg.desired_mechs.GSS_OID_SET_val[i].GSS_OID_val =
			(char *) MALLOC(desired_mechs->elements[i].length);
		    memcpy(arg.desired_mechs.GSS_OID_SET_val[i].GSS_OID_val,
			desired_mechs->elements[i].elements,
			desired_mechs->elements[i].length);
		}
	} else
		arg.desired_mechs.GSS_OID_SET_len = 0;

	arg.cred_usage = cred_usage;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_acquire_cred_1(&arg, &res, clnt) != RPC_SUCCESS) {

		/*
		 * if the RPC call times out, null out all return arguments,
		 * set minor_status to its maximum value, and return
		 * GSS_S_FAILURE
		 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (output_cred_handle != NULL)
			*output_cred_handle = NULL;
		if (actual_mechs != NULL)
			*actual_mechs = NULL;
		if (time_rec != NULL)
			*time_rec = 0;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_acquire_cred: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* free the allocated memory for the flattened name and desire_mechs */

	(void) gss_release_buffer(&minor_status_temp, &external_name);
	for (i = 0; i < desired_mechs->count; i++)
		FREE(arg.desired_mechs.GSS_OID_SET_val[i].GSS_OID_val,
			arg.desired_mechs.GSS_OID_SET_val[i].GSS_OID_len);
	FREE(arg.desired_mechs.GSS_OID_SET_val,
		arg.desired_mechs.GSS_OID_SET_len * sizeof (GSS_OID));

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (output_cred_handle != NULL &&
		(res.status == GSS_S_COMPLETE)) {
	    *output_cred_handle =
		*((gssd_cred_id_t *) res.output_cred_handle.GSS_CRED_ID_T_val);
	    *gssd_cred_verifier = res.gssd_cred_verifier;
	}

	if (res.status == GSS_S_COMPLETE &&
		res.actual_mechs.GSS_OID_SET_len != 0 &&
		actual_mechs != NULL) {
		*actual_mechs = (gss_OID_set) MALLOC(sizeof (gss_OID_set_desc));
		(*actual_mechs)->count =
					(int) res.actual_mechs.GSS_OID_SET_len;
		(*actual_mechs)->elements = (gss_OID)
			MALLOC(sizeof (gss_OID_desc) * (*actual_mechs)->count);

		for (i = 0; i < (*actual_mechs)->count; i++) {
		    (*actual_mechs)->elements[i].length = (OM_uint32)
			res.actual_mechs.GSS_OID_SET_val[i].GSS_OID_len;
		    (*actual_mechs)->elements[i].elements =
			(void *) MALLOC((*actual_mechs)->elements[i].length);
		    memcpy((*actual_mechs)->elements[i].elements,
			res.actual_mechs.GSS_OID_SET_val[i].GSS_OID_val,
			(*actual_mechs)->elements[i].length);
		}
	} else {
		if (res.status == GSS_S_COMPLETE &&
			actual_mechs != NULL)
			(*actual_mechs)->count = 0;
	}

	if (time_rec != NULL)
		*time_rec = res.time_rec;

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_acquire_cred_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_acquire_cred(minor_status,
		desired_name,
		time_req,
		desired_mechs,
		cred_usage,
		output_cred_handle,
		actual_mechs,
		time_rec,
		uid)
	OM_uint32 *minor_status;
	const gss_name_t desired_name;
	OM_uint32 time_req;
	const gss_OID_set desired_mechs;
	int cred_usage;
	gss_cred_id_t *output_cred_handle;
	gss_OID_set *actual_mechs;
	OM_uint32 *time_rec;
	uid_t uid;
{

	OM_uint32	err;
	struct kgss_cred *kcred;

	kcred = KGSS_CRED_ALLOC();
	*output_cred_handle = kcred;
	err = kgss_acquire_cred_wrapped(minor_status, desired_name, time_req,
		desired_mechs, cred_usage, &kcred->gssd_cred, actual_mechs,
		time_rec, uid, &kcred->gssd_cred_verifier);
	if (GSS_ERROR(err)) {
		KGSS_CRED_FREE(kcred);
		*output_cred_handle = GSS_C_NO_CREDENTIAL;
	}
	return (err);
}

OM_uint32
kgss_release_cred_wrapped(minor_status,
			cred_handle,
			uid,
			gssd_cred_verifier)
    OM_uint32 *minor_status;
    gssd_cred_id_t *cred_handle;
    uid_t uid;
    OM_uint32  gssd_cred_verifier;
{
	CLIENT *clnt;

	gss_release_cred_arg arg;
	gss_release_cred_res res;


	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_release_cred: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;
	arg.gssd_cred_verifier = gssd_cred_verifier;

	if (cred_handle != NULL) {
		arg.cred_handle.GSS_CRED_ID_T_len =
					(u_int) sizeof (gssd_cred_id_t);
		arg.cred_handle.GSS_CRED_ID_T_val = (char *) cred_handle;
	} else
		arg.cred_handle.GSS_CRED_ID_T_len = 0;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_release_cred_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (cred_handle != NULL)
			*cred_handle = NULL;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_release_cred: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* if the release succeeded, null out the cred_handle */

	if (res.status == GSS_S_COMPLETE && cred_handle != NULL)
		*cred_handle = NULL;

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	/* return with status returned in rpc call */

	killgssd_handle(clnt);

	return (res.status);

}

OM_uint32
kgss_release_cred(minor_status,
			cred_handle,
			uid)
    OM_uint32 *minor_status;
    gss_cred_id_t *cred_handle;
    uid_t uid;

{

	OM_uint32	err;
	struct kgss_cred *kcred;

	if (*cred_handle == GSS_C_NO_CREDENTIAL)
		return (GSS_S_COMPLETE);
	else
		kcred = KCRED_TO_KGSS_CRED(*cred_handle);

	err = kgss_release_cred_wrapped(minor_status, &kcred->gssd_cred,
		uid, kcred->gssd_cred_verifier);
	KGSS_CRED_FREE(kcred);
	*cred_handle = GSS_C_NO_CREDENTIAL;
	return (err);
}

static OM_uint32
kgss_init_sec_context_wrapped(
	OM_uint32 *minor_status,
	const gssd_cred_id_t claimant_cred_handle,
	OM_uint32 gssd_cred_verifier,
	gssd_ctx_id_t *context_handle,
	OM_uint32 *gssd_context_verifier,
	const gss_name_t target_name,
	const gss_OID mech_type,
	int req_flags,
	OM_uint32 time_req,
	const gss_channel_bindings_t input_chan_bindings,
	const gss_buffer_t input_token,
	gss_OID *actual_mech_type,
	gss_buffer_t output_token,
	int *ret_flags,
	OM_uint32 *time_rec,
	uid_t uid)
{
	CLIENT *clnt;

	OM_uint32 	minor_status_temp;
	gss_buffer_desc	external_name;
	gss_OID		name_type;

	gss_init_sec_context_arg arg;
	gss_init_sec_context_res res;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1,
		"kgss_init_sec_context: can't connect to server on %s\n",
		server);
		return (GSS_S_FAILURE);
	}

	/* convert the target name from internal to external format */

	if (gss_display_name(&minor_status_temp, target_name,
		&external_name, &name_type) != GSS_S_COMPLETE) {

			*minor_status = (OM_uint32) minor_status_temp;
			(void) gss_release_buffer(&minor_status_temp,
				&external_name);

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_init_sec_context: can't display name\n");
		return ((OM_uint32) GSS_S_FAILURE);
	}


	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.context_handle.GSS_CTX_ID_T_len =
		*context_handle == (gssd_ctx_id_t) GSS_C_NO_CONTEXT ?
			0 : (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val =  (char *) context_handle;

	arg.gssd_context_verifier =  *gssd_context_verifier;

	arg.claimant_cred_handle.GSS_CRED_ID_T_len =
		claimant_cred_handle == (gssd_cred_id_t) GSS_C_NO_CREDENTIAL ?
			0 : (u_int) sizeof (gssd_cred_id_t);
	arg.claimant_cred_handle.GSS_CRED_ID_T_val =
						(char *) &claimant_cred_handle;
	arg.gssd_cred_verifier = gssd_cred_verifier;

	arg.target_name.GSS_BUFFER_T_len = (u_int) external_name.length;
	arg.target_name.GSS_BUFFER_T_val = (char *) external_name.value;

	arg.name_type.GSS_OID_len =
		name_type == GSS_C_NULL_OID ?
			0 : (u_int) name_type->length;

	arg.name_type.GSS_OID_val =
		name_type == GSS_C_NULL_OID ?
			(char *) NULL : (char *) name_type->elements;

	arg.mech_type.GSS_OID_len = (u_int) (mech_type != GSS_C_NULL_OID ?
						mech_type->length : 0);
	arg.mech_type.GSS_OID_val = (char *) (mech_type != GSS_C_NULL_OID ?
						mech_type->elements : 0);

	arg.req_flags = req_flags;

	arg.time_req = time_req;

	if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS) {
		arg.input_chan_bindings.present = YES;
		arg.input_chan_bindings.initiator_addrtype =
			input_chan_bindings->initiator_addrtype;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->initiator_address.length;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->initiator_address.value;
		arg.input_chan_bindings.acceptor_addrtype =
			input_chan_bindings->acceptor_addrtype;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->acceptor_address.length;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->acceptor_address.value;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->application_data.length;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->application_data.value;
	} else {
		arg.input_chan_bindings.present = NO;
		arg.input_chan_bindings.initiator_addrtype = 0;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_val = 0;
		arg.input_chan_bindings.acceptor_addrtype = 0;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_val = 0;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_val = 0;
	}

	arg.input_token.GSS_BUFFER_T_len =
		(u_int) (input_token != GSS_C_NO_BUFFER ?
		input_token->length : 0);
	arg.input_token.GSS_BUFFER_T_val =
		(char *) (input_token != GSS_C_NO_BUFFER ?
		input_token->value : 0);

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_init_sec_context_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (actual_mech_type != NULL)
			*actual_mech_type = NULL;
		if (output_token != NULL)
			output_token->length = 0;
		if (ret_flags != NULL)
			*ret_flags = 0;
		if (time_rec != NULL)
			*time_rec = 0;

		killgssd_handle(clnt);
		(void) gss_release_buffer(&minor_status_temp, &external_name);
		GSSLOG0(1, "kgss_init_sec_context: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* free the allocated memory for the flattened name */

	(void) gss_release_buffer(&minor_status_temp, &external_name);

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	/* if the call was successful, copy out the results */
	if (res.status == (OM_uint32) GSS_S_COMPLETE ||
		res.status == (OM_uint32) GSS_S_CONTINUE_NEEDED) {
		/*
		 * if the return code is GSS_S_CONTINUE_NEEDED
		 * ignore all return parameters except for
		 * status codes, output token and context handle.
		 */
		*context_handle =
			*((gssd_ctx_id_t *)
			res.context_handle.GSS_CTX_ID_T_val);
		*gssd_context_verifier = res.gssd_context_verifier;

		if (output_token != NULL) {
			output_token->length =
				(size_t) res.output_token.GSS_BUFFER_T_len;
			output_token->value =
				(void *) MALLOC(output_token->length);
			memcpy(output_token->value,
				res.output_token.GSS_BUFFER_T_val,
				output_token->length);
		}

		if (res.status == GSS_S_COMPLETE) {
			if (actual_mech_type != NULL) {
				*actual_mech_type =
					(gss_OID) MALLOC(sizeof (gss_OID_desc));
				(*actual_mech_type)->length =
					(OM_UINT32)
					res.actual_mech_type.GSS_OID_len;
				(*actual_mech_type)->elements =
					(void *)
					MALLOC((*actual_mech_type)->length);
				memcpy((*actual_mech_type)->elements,
					(void *)
					res.actual_mech_type.GSS_OID_val,
					(*actual_mech_type)->length);
			}


			if (ret_flags != NULL)
				*ret_flags = res.ret_flags;

			if (time_rec != NULL)
				*time_rec = res.time_rec;
		}
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_init_sec_context_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

static struct gss_config default_gc = {
	{ 0, NULL},
	NULL,
	NULL,
	0,
	NULL,		/* kgss_delete_sec_context_wrapped */
	NULL,		/* kgss_import_sec_context */
	kgss_sign_wrapped,
	kgss_verify_wrapped
};

void
kgss_free_oid(gss_OID oid)
{
	FREE(oid->elements, oid->length);
	FREE(oid, sizeof (gss_OID_desc));
}

OM_uint32
kgss_init_sec_context(
	OM_uint32 *minor_status,
	const gss_cred_id_t claimant_cred_handle,
	gss_ctx_id_t *context_handle,
	const gss_name_t target_name,
	const gss_OID mech_type,
	int req_flags,
	OM_uint32 time_req,
	const gss_channel_bindings_t input_chan_bindings,
	const gss_buffer_t input_token,
	gss_OID *actual_mech_type,
	gss_buffer_t output_token,
	int *ret_flags,
	OM_uint32 *time_rec,
	uid_t uid)
{
	OM_uint32	err;
	struct kgss_ctx	*kctx;
	gss_OID	amt;
	gssd_cred_id_t gssd_cl_cred_handle;
	OM_uint32 gssd_cred_verifier;

	/*
	 * If this is an initial call, we'll need to create the
	 * wrapper struct that contains kernel state information, and
	 * a reference to the handle from gssd.
	 */
	if (*context_handle == GSS_C_NO_CONTEXT) {
		kctx = KGSS_ALLOC();
		/*
		 * The default gss-mechanism struct as pointers to
		 * the sign/seal/verify/unseal routines that make
		 * upcalls to gssd.
		 */
		kctx->mech = &default_gc;
		kctx->gssd_ctx = (gssd_ctx_id_t) GSS_C_NO_CONTEXT;
		*context_handle = kctx;
	} else
		kctx = *context_handle;

	if (claimant_cred_handle != GSS_C_NO_CREDENTIAL) {
		gssd_cred_verifier = KCRED_TO_CREDV(claimant_cred_handle);
		gssd_cl_cred_handle = KCRED_TO_CRED(claimant_cred_handle);
	} else
		gssd_cl_cred_handle = (gssd_cred_id_t) GSS_C_NO_CREDENTIAL;

	/*
	 * We need to know the resulting mechanism oid, so allocate
	 * it if the caller won't.
	 */
	if (actual_mech_type == NULL)
		actual_mech_type = &amt;

	err = kgss_init_sec_context_wrapped(minor_status, gssd_cl_cred_handle,
		gssd_cred_verifier, &kctx->gssd_ctx, &kctx->gssd_ctx_verifier,
		target_name, mech_type, req_flags, time_req,
		input_chan_bindings, input_token, actual_mech_type,
		output_token, ret_flags, time_rec, uid);

	if (GSS_ERROR(err)) {
		KGSS_FREE(kctx);
		*context_handle = GSS_C_NO_CONTEXT;
	} else if (err == GSS_S_COMPLETE) {
		/*
		 * Now check if there is a kernel module for this
		 * mechanism OID. If so, set the gss_mechanism structure
		 * in the wrapper context to point to the kernel mech.
		 */
		__kgss_reset_mech(&kctx->mech, *actual_mech_type);

		/*
		 * If the mech oid was allocated for us, free it.
		 */
		if (&amt == actual_mech_type) {
			kgss_free_oid(amt);
		}
	}
	return (err);
}

static OM_uint32
kgss_accept_sec_context_wrapped(
	OM_uint32 *minor_status,
	gssd_ctx_id_t *context_handle,
	OM_uint32 *gssd_context_verifier,
	const gssd_cred_id_t verifier_cred_handle,
	OM_uint32 gssd_cred_verifier,
	const gss_buffer_t input_token,
	const gss_channel_bindings_t input_chan_bindings,
	gss_buffer_t src_name,
	gss_OID *mech_type,
	gss_buffer_t output_token,
	int *ret_flags,
	OM_uint32 *time_rec,
	gss_cred_id_t *delegated_cred_handle,
	uid_t uid)
{
	CLIENT *clnt;

	gss_accept_sec_context_arg arg;
	gss_accept_sec_context_res res;
	struct kgss_cred *kcred;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1,
		"kgss_accept_sec_context: can't connect to server on %s\n",
		server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.context_handle.GSS_CTX_ID_T_len =
		*context_handle == (gssd_ctx_id_t) GSS_C_NO_CONTEXT ?
			0 : (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val =  (char *) context_handle;
	arg.gssd_context_verifier = *gssd_context_verifier;

	arg.verifier_cred_handle.GSS_CRED_ID_T_len =
			verifier_cred_handle ==
			(gssd_cred_id_t) GSS_C_NO_CREDENTIAL ?
			0 : (u_int) sizeof (gssd_cred_id_t);
	arg.verifier_cred_handle.GSS_CRED_ID_T_val =
						(char *) &verifier_cred_handle;
	arg.gssd_cred_verifier = gssd_cred_verifier;

	arg.input_token_buffer.GSS_BUFFER_T_len =
			(u_int) (input_token != GSS_C_NO_BUFFER ?
						input_token->length : 0);
	arg.input_token_buffer.GSS_BUFFER_T_val =
			(char *) (input_token != GSS_C_NO_BUFFER ?
						input_token->value : 0);

	if (input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS) {
		arg.input_chan_bindings.present = YES;
		arg.input_chan_bindings.initiator_addrtype =
			input_chan_bindings->initiator_addrtype;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->initiator_address.length;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->initiator_address.value;
		arg.input_chan_bindings.acceptor_addrtype =
			input_chan_bindings->acceptor_addrtype;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->acceptor_address.length;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->acceptor_address.value;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_len =
			(u_int) input_chan_bindings->application_data.length;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_val =
			(void *) input_chan_bindings->application_data.value;
	} else {

		arg.input_chan_bindings.present = NO;
		arg.input_chan_bindings.initiator_addrtype = 0;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.initiator_address.GSS_BUFFER_T_val = 0;
		arg.input_chan_bindings.acceptor_addrtype = 0;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.acceptor_address.GSS_BUFFER_T_val = 0;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_len = 0;
		arg.input_chan_bindings.application_data.GSS_BUFFER_T_val = 0;
	}

	/* set the return parameters in case of errors.... */
	if (minor_status != NULL)
		*minor_status = DEFAULT_MINOR_STAT;
	if (src_name != NULL) {
		src_name->length = 0;
		src_name->value = NULL;
	}
	if (mech_type != NULL)
		*mech_type = NULL;
	if (output_token != NULL)
		output_token->length = 0;
	if (ret_flags != NULL)
		*ret_flags = 0;
	if (time_rec != NULL)
		*time_rec = 0;
	if (delegated_cred_handle != NULL)
		*delegated_cred_handle = NULL;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_accept_sec_context_1(&arg, &res, clnt) != RPC_SUCCESS) {
		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_accept_sec_context: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* if the call was successful, copy out the results */

	if (res.status == (OM_uint32) GSS_S_COMPLETE ||
		res.status == (OM_uint32) GSS_S_CONTINUE_NEEDED) {

		/*
		 * the only parameters that are ready when we
		 * get GSS_S_CONTINUE_NEEDED are: minor, ctxt_handle,
		 * and the output token to send to the peer.
		 */

		*context_handle = *((gssd_ctx_id_t *)
			res.context_handle.GSS_CTX_ID_T_val);
			*gssd_context_verifier = res.gssd_context_verifier;

		if (output_token != NULL) {
			output_token->length =
					res.output_token.GSS_BUFFER_T_len;
			output_token->value =
				(void *)  MALLOC(output_token->length);
			memcpy(output_token->value,
				res.output_token.GSS_BUFFER_T_val,
				output_token->length);
		}

		if (minor_status != NULL)
			*minor_status = res.minor_status;

		/* these other parameters are only ready upon GSS_S_COMPLETE */
		if (res.status == (OM_uint32) GSS_S_COMPLETE) {

			if (src_name != NULL) {
			    src_name->length = res.src_name.GSS_BUFFER_T_len;
			    src_name->value = res.src_name.GSS_BUFFER_T_val;
			    res.src_name.GSS_BUFFER_T_val = NULL;
			    res.src_name.GSS_BUFFER_T_len = 0;
			}

			/*
			 * move mech type returned to mech_type
			 * for gss_import_name_for_mech()
			 */
			if (mech_type != NULL) {
				*mech_type = (gss_OID)
					MALLOC(sizeof (gss_OID_desc));
				(*mech_type)->length =
					(OM_UINT32) res.mech_type.GSS_OID_len;
				(*mech_type)->elements =
					(void *) MALLOC((*mech_type)->length);
				memcpy((*mech_type)->elements,
					res.mech_type.GSS_OID_val,
					(*mech_type)->length);
			}

			if (ret_flags != NULL)
				*ret_flags = res.ret_flags;

			if (time_rec != NULL)
				*time_rec = res.time_rec;

			if ((delegated_cred_handle != NULL) &&
				(res.delegated_cred_handle.GSS_CRED_ID_T_len
				!= 0)) {
				kcred = KGSS_CRED_ALLOC();
				kcred->gssd_cred = *((gssd_cred_id_t *)
				res.delegated_cred_handle.GSS_CRED_ID_T_val);
				kcred->gssd_cred_verifier =
				res.gssd_context_verifier;
				*delegated_cred_handle = kcred;
			}

		}
	}


	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_accept_sec_context_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_accept_sec_context(
	OM_uint32 *minor_status,
	gss_ctx_id_t *context_handle,
	const gss_cred_id_t verifier_cred_handle,
	const gss_buffer_t input_token,
	const gss_channel_bindings_t input_chan_bindings,
	gss_buffer_t src_name,
	gss_OID *mech_type,
	gss_buffer_t output_token,
	int *ret_flags,
	OM_uint32 *time_rec,
	gss_cred_id_t *delegated_cred_handle,
	uid_t uid)
{
	OM_uint32 err;
	struct kgss_ctx	*kctx;
	gss_OID mt;
	OM_uint32 gssd_cred_verifier;
	gssd_cred_id_t gssd_ver_cred_handle;


	/*
	 * See kgss_init_sec_context() to get an idea of what is going
	 * on here.
	 */
	if (mech_type == NULL)
		mech_type = &mt;

	if (*context_handle == GSS_C_NO_CONTEXT) {
		kctx = KGSS_ALLOC();
		kctx->mech = &default_gc;
		kctx->gssd_ctx = (gssd_ctx_id_t) GSS_C_NO_CONTEXT;
		*context_handle = kctx;
	} else
		kctx = *context_handle;

	if (verifier_cred_handle != GSS_C_NO_CREDENTIAL) {
		gssd_cred_verifier = KCRED_TO_CREDV(verifier_cred_handle);
		gssd_ver_cred_handle = KCRED_TO_CRED(verifier_cred_handle);
	} else
		gssd_ver_cred_handle = (gssd_cred_id_t) GSS_C_NO_CREDENTIAL;

	err = kgss_accept_sec_context_wrapped(minor_status,
		&kctx->gssd_ctx, &kctx->gssd_ctx_verifier,
		gssd_ver_cred_handle, gssd_cred_verifier,
		input_token, input_chan_bindings, src_name,
		mech_type, output_token, ret_flags,
		time_rec, delegated_cred_handle, uid);

	if (GSS_ERROR(err)) {
		KGSS_FREE(kctx);
		*context_handle = GSS_C_NO_CONTEXT;

	} else if (err == GSS_S_COMPLETE) {
		__kgss_reset_mech(&kctx->mech, *mech_type);

		/*
		 * If the mech oid was allocated for us, free it.
		 */
		if (&mt == mech_type) {
			kgss_free_oid(mt);
		}
	}

	return (err);
}

OM_uint32
kgss_process_context_token(minor_status,
				context_handle,
				token_buffer,
				uid)
	OM_uint32 *minor_status;
	const gss_ctx_id_t context_handle;
	gss_buffer_t token_buffer;
	uid_t uid;
{
	CLIENT *clnt;
	OM_uint32 gssd_context_verifier;
	gssd_ctx_id_t gssd_ctx_handle;
	gss_process_context_token_arg arg;
	gss_process_context_token_res res;

	gssd_context_verifier = KGSS_CTX_TO_GSSD_CTXV(context_handle);
	gssd_ctx_handle = (gssd_ctx_id_t) KGSS_CTX_TO_GSSD_CTX(context_handle);
	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1,
		"kgss_process_context_token: can't connect to server on %s\n",
		server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) &gssd_ctx_handle;
	arg.gssd_context_verifier = gssd_context_verifier;
	arg.token_buffer.GSS_BUFFER_T_len = (u_int) token_buffer;
	arg.token_buffer.GSS_BUFFER_T_val = (char *) token_buffer->value;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_process_context_token_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		GSSLOG0(1, "kgss_process_context_token: RPC call times out\n");
		killgssd_handle(clnt);
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	/* return with status returned in rpc call */

	killgssd_handle(clnt);
	return (res.status);

}

/*ARGSUSED*/
static OM_uint32
kgss_delete_sec_context_wrapped(void *private,
			OM_uint32 *minor_status,
			gssd_ctx_id_t *context_handle,
			gss_buffer_t output_token,
			OM_uint32 gssd_context_verifier)


{
	CLIENT *clnt;

	gss_delete_sec_context_arg arg;
	gss_delete_sec_context_res res;


	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1,
		"kgss_delete_sec_context: can't connect to server on %s\n",
		server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.context_handle.GSS_CTX_ID_T_len =
		*context_handle == (gssd_ctx_id_t) GSS_C_NO_CONTEXT ?
			0 : (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val =  (char *) context_handle;

	arg.gssd_context_verifier = gssd_context_verifier;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_delete_sec_context_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (context_handle != NULL)
			*context_handle = NULL;
		if (output_token != NULL)
			output_token->length = 0;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgssd_delete_sec_context: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (res.context_handle.GSS_CTX_ID_T_len == 0)
		*context_handle = NULL;
	else
		*context_handle =
		    *((gssd_ctx_id_t *) res.context_handle.GSS_CTX_ID_T_val);

	if (output_token != NULL) {
		output_token->length = res.output_token.GSS_BUFFER_T_len;
		output_token->value = res.output_token.GSS_BUFFER_T_val;
		res.output_token.GSS_BUFFER_T_len = 0;
		res.output_token.GSS_BUFFER_T_val = NULL;
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_delete_sec_context_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_delete_sec_context(
		OM_uint32 *minor_status,
		gss_ctx_id_t *context_handle,
		gss_buffer_t output_token)
{
	OM_uint32 err;
	gss_union_ctx_id_t  ctx;
	struct kgss_ctx	*kctx;

	if (*context_handle == GSS_C_NO_CONTEXT) {
		GSSLOG0(8, "kgss_delete_sec_context: Null context handle \n");
		return (GSS_S_COMPLETE);
	} else
		kctx = (struct kgss_ctx *) *context_handle;

	if (kctx->ctx_imported == FALSE) {
		if (kctx->gssd_ctx == (gssd_ctx_id_t) GSS_C_NO_CONTEXT) {
			KGSS_FREE(kctx);
			*context_handle = GSS_C_NO_CONTEXT;
			return (GSS_S_COMPLETE);
		}
		err = kgss_delete_sec_context_wrapped(
				KCTX_TO_PRIVATE(*context_handle),
				minor_status,
				&kctx->gssd_ctx,
				output_token,
				kctx->gssd_ctx_verifier);
	} else {
		if (kctx->gssd_i_ctx == (gss_ctx_id_t) GSS_C_NO_CONTEXT) {
			KGSS_FREE(kctx);
			*context_handle = GSS_C_NO_CONTEXT;
			return (GSS_S_COMPLETE);
		}
		ctx = (gss_union_ctx_id_t) kctx->gssd_i_ctx;
		err = KGSS_DELETE_SEC_CONTEXT(minor_status, kctx,
				&ctx->internal_ctx_id,  output_token);
		FREE(ctx->mech_type->elements, ctx->mech_type->length);
		FREE(ctx->mech_type, sizeof (gss_OID_desc));
		FREE(ctx, sizeof (gss_union_ctx_id_desc));
	}
	KGSS_FREE(kctx);
	*context_handle = GSS_C_NO_CONTEXT;
	return (err);

}


OM_uint32
kgss_export_sec_context_wrapped(minor_status,
				context_handle,
				output_token,
				gssd_context_verifier)
	OM_uint32 *minor_status;
	gssd_ctx_id_t *context_handle;
	gss_buffer_t output_token;
	OM_uint32 gssd_context_verifier;
{
	CLIENT *clnt;
	gss_export_sec_context_arg arg;
	gss_export_sec_context_res res;


	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_export_sec_context_wrapped :"
			" can't connect to server on %s\n", server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) context_handle;
	arg.gssd_context_verifier = gssd_context_verifier;

	/* call the remote procedure */

	memset(&res, 0, sizeof (res));
	if (gss_export_sec_context_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments,
	 * set minor_status to its maximum value, and return
	 * GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (context_handle != NULL)
			*context_handle = NULL;
		if (output_token != NULL)
			output_token->length = 0;
		killgssd_handle(clnt);
		GSSLOG0(1,
		"kgss_export_sec_context_wrapped: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (res.context_handle.GSS_CTX_ID_T_len == 0)
		*context_handle = NULL;
	else
		*context_handle =
		    *((gssd_ctx_id_t *) res.context_handle.GSS_CTX_ID_T_val);

	if (output_token != NULL) {
		output_token->length = res.output_token.GSS_BUFFER_T_len;
		output_token->value =
			(void *)  MALLOC(output_token->length);
		memcpy(output_token->value,
			res.output_token.GSS_BUFFER_T_val,
			output_token->length);
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_export_sec_context_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_export_sec_context(minor_status,
			context_handle,
			output_token)
	OM_uint32 *minor_status;
	gss_ctx_id_t context_handle;
	gss_buffer_t output_token;
{
	struct kgss_ctx	*kctx;

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_FAILURE);
	else
		kctx = (struct kgss_ctx *) context_handle;



	/*
	 *  If there is a kernel module then import_sec context must be
	 *  supported and we make an upcall to export_sec_context.
	 *  If there is no kernel module then we return an error
	 */

	*minor_status = 0;

	if (kctx->mech->gss_import_sec_context) {
		GSSLOG0(8, "kgss_export_sec_context: Kernel mod available \n");
		return (kgss_export_sec_context_wrapped(minor_status,
						&kctx->gssd_ctx,
						output_token,
						kctx->gssd_ctx_verifier));

	} else {

		/*
		* This is not the right error value; instead of
		* inventing  new error we return GSS_S_NAME_NOT_MN
		* This error is not returned by the export routine
		*/

		GSSLOG0(8, "kgss_export_sec_context: Kernel mod "
			"unavailable \n");
		return (GSS_S_NAME_NOT_MN);
	}

} 

OM_uint32
kgss_import_sec_context(minor_status,
			interprocess_token,
			context_handle)

OM_uint32 *		minor_status;
const gss_buffer_t	interprocess_token;
gss_ctx_id_t 		context_handle;

{
OM_uint32 status;
struct kgss_ctx	*kctx;

size_t		length;
char		*p;
gss_buffer_desc token;
gss_union_ctx_id_t	ctx;

	kctx = (struct kgss_ctx *) context_handle;

	if (kctx->gssd_ctx != (gssd_ctx_id_t)GSS_C_NO_CONTEXT) {
		return (GSS_S_FAILURE);
	}

	if (!(KCTX_TO_MECH(context_handle)->gss_import_sec_context)) {

	/*
	 *  This should never happen
	 *  If Kernel import sec context does not exist the export
	 *  sec context should have caught this and returned an error
	 *  and the caller should not have called this routine
	 */
		GSSLOG0(1, "import_sec_context  called improperly\n");
		return (GSS_S_FAILURE);
	}
	*minor_status = 0;

	if (interprocess_token->length == 0 || interprocess_token->value == 0)
		return (GSS_S_DEFECTIVE_TOKEN);

	status = GSS_S_FAILURE;

	ctx = (gss_union_ctx_id_t) MALLOC(sizeof (gss_union_ctx_id_desc));
	if (!ctx) {
		*minor_status = ENOMEM;
		goto error_out;
	}
	ctx->mech_type = (gss_OID) MALLOC(sizeof (gss_OID_desc));
	if (!ctx->mech_type) {
		*minor_status = ENOMEM;
		goto error_out;
	}
	p = interprocess_token->value;
	length = *p++;
	length = (length << 8) + *p++;
	length = (length << 8) + *p++;
	length = (length << 8) + *p++;

	ctx->mech_type->length = length;
	ctx->mech_type->elements = MALLOC(length);
	if (!ctx->mech_type->elements) {
		*minor_status = ENOMEM;
		goto error_out;
	}
	memcpy(ctx->mech_type->elements, p, length);
	p += length;

	token.length = interprocess_token->length - 4 - length;
	token.value = p;

	/*
	 * select the approprate underlying mechanism routine and
	 * call it.
	 */

	status = KGSS_IMPORT_SEC_CONTEXT(minor_status, &token, kctx,
				&ctx->internal_ctx_id);

	if (status == GSS_S_COMPLETE) {
		KCTX_TO_I_CTX(kctx) = ctx;
		kctx->ctx_imported = TRUE;
		return (GSS_S_COMPLETE);
	}

error_out:
	if (ctx) {
		if (ctx->mech_type) {
		    if (ctx->mech_type->elements)
			FREE(ctx->mech_type->elements, ctx->mech_type->length);
		    FREE(ctx->mech_type, sizeof (gss_OID_desc));
		}
	FREE(ctx, sizeof (gss_union_ctx_id_desc));
	}
    return (status);
}

/*ARGSUSED*/
OM_uint32
kgss_context_time(minor_status,
		context_handle,
		time_rec,
		uid)
	OM_uint32 *minor_status;
	const gss_ctx_id_t context_handle;
	OM_uint32 *time_rec;
	uid_t uid;
{
	return (GSS_S_FAILURE);
}

/*ARGSUSED*/
static OM_uint32
kgss_sign_wrapped(void *private,
	OM_uint32 *minor_status,
	const gss_ctx_id_t ctx_handle,
	int qop_req,
	const gss_buffer_t message_buffer,
	gss_buffer_t msg_token,
	OM_uint32 gssd_context_verifier)
{
	CLIENT *clnt;
	gssd_ctx_id_t context_handle;

	gss_sign_arg arg;
	gss_sign_res res;
	context_handle = (gssd_ctx_id_t) KCTX_TO_GSSD_CTX(ctx_handle);
	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_sign: can't connect to server on %s\n", server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) &context_handle;

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) &context_handle;
	arg.gssd_context_verifier = gssd_context_verifier;

	arg.qop_req = qop_req;

	arg.message_buffer.GSS_BUFFER_T_len = (u_int) message_buffer->length;
	arg.message_buffer.GSS_BUFFER_T_val = (char *) message_buffer->value;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_sign_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (msg_token != NULL)
			msg_token->length = 0;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_sign: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (msg_token != NULL) {
		msg_token->length = res.msg_token.GSS_BUFFER_T_len;
		msg_token->value = (void *) MALLOC(msg_token->length);
		memcpy(msg_token->value, res.msg_token.GSS_BUFFER_T_val,
			msg_token->length);
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_sign_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_sign(
	OM_uint32 *minor_status,
	const gss_ctx_id_t context_handle,
	int qop_req,
	const gss_buffer_t message_buffer,
	gss_buffer_t msg_token)
{
	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_FAILURE);
	return (KGSS_SIGN(minor_status, context_handle, qop_req,
			message_buffer, msg_token));
}

/*ARGSUSED*/
static OM_uint32
kgss_verify_wrapped(void *private,
	OM_uint32 *minor_status,
	const gss_ctx_id_t ctx_handle,
	const gss_buffer_t message_buffer,
	const gss_buffer_t token_buffer,
	int *qop_state,
	OM_uint32 gssd_context_verifier)
{
	CLIENT *clnt;

	gssd_ctx_id_t context_handle;
	gss_verify_arg arg;
	gss_verify_res res;

	context_handle = (gssd_ctx_id_t) KCTX_TO_GSSD_CTX(ctx_handle);

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_verify: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gss_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) &context_handle;

	arg.context_handle.GSS_CTX_ID_T_len = (u_int) sizeof (gssd_ctx_id_t);
	arg.context_handle.GSS_CTX_ID_T_val = (char *) &context_handle;
	arg.gssd_context_verifier = gssd_context_verifier;

	arg.message_buffer.GSS_BUFFER_T_len = (u_int) message_buffer->length;
	arg.message_buffer.GSS_BUFFER_T_val = (char *) message_buffer->value;

	arg.token_buffer.GSS_BUFFER_T_len = (u_int) token_buffer->length;
	arg.token_buffer.GSS_BUFFER_T_val = (char *) token_buffer->value;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_verify_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (qop_state != NULL)
			*qop_state = 0;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_verify: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (qop_state != NULL)
		*qop_state = res.qop_state;

	/* return with status returned in rpc call */

	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_verify(OM_uint32 *minor_status,
		const gss_ctx_id_t context_handle,
		const gss_buffer_t message_buffer,
		const gss_buffer_t token_buffer,
		int *qop_state)
{
	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_FAILURE);
	return (KGSS_VERIFY(minor_status, context_handle,
			    message_buffer,
			    token_buffer,
			    qop_state));
}


OM_uint32
kgss_display_status(minor_status,
		status_value,
		status_type,
		mech_type,
		message_context,
		status_string,
		uid)
	OM_uint32 *minor_status;
	OM_uint32 status_value;
	int status_type;
	const gss_OID mech_type;
	int *message_context;
	gss_buffer_t status_string;
	uid_t uid;
{
	CLIENT *clnt;

	gss_display_status_arg arg;
	gss_display_status_res res;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
	GSSLOG(1, "kgss_display_status: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.status_value = status_value;
	arg.status_type = status_type;

	arg.mech_type.GSS_OID_len = (u_int) (mech_type != GSS_C_NULL_OID ?
						mech_type->length : 0);
	arg.mech_type.GSS_OID_val = (char *) (mech_type != GSS_C_NULL_OID ?
						mech_type->elements : 0);

	arg.message_context = *message_context;

	/* call the remote procedure */

	if (message_context != NULL)
		*message_context = 0;
	if (status_string != NULL) {
		status_string->length = 0;
		status_string->value = NULL;
	}

	bzero((caddr_t) &res, sizeof (res));
	if (gss_display_status_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_display_status: RPC call time out\n");
		return (GSS_S_FAILURE);
	}


	/* now process the results and pass them back to the caller */

	if (res.status == GSS_S_COMPLETE) {
		if (minor_status != NULL)
			*minor_status = res.minor_status;
		if (message_context != NULL)
			*message_context = res.message_context;
		if (status_string != NULL) {
			status_string->length =
				(size_t) res.status_string.GSS_BUFFER_T_len;
			status_string->value =
				(void *) MALLOC(status_string->length);
			memcpy(status_string->value,
				res.status_string.GSS_BUFFER_T_val,
				status_string->length);
		}
	}

	clnt_freeres(clnt, xdr_gss_display_status_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);
}

/*ARGSUSED*/
OM_uint32
kgss_indicate_mechs(minor_status,
			mech_set,
			uid)
	OM_uint32 *minor_status;
	gss_OID_set *mech_set;
	uid_t uid;
{
	CLIENT *clnt;
	void *arg;
	gss_indicate_mechs_res res;
	int i;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
	GSSLOG(1, "kgss_indicate_mechs: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}

	bzero((caddr_t) &res, sizeof (res));
	if (gss_indicate_mechs_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (mech_set != NULL)
			*mech_set = NULL;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_indicate_mechs: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	if (mech_set != NULL) {
		*mech_set = (gss_OID_set) MALLOC(sizeof (gss_OID_set_desc));
		(*mech_set)->count = res.mech_set.GSS_OID_SET_len;
		(*mech_set)->elements = (void *)
			MALLOC ((*mech_set)->count * sizeof (gss_OID_desc));
		for (i = 0; i < (*mech_set)->count; i++) {
			(*mech_set)->elements[i].length =
				res.mech_set.GSS_OID_SET_val[i].GSS_OID_len;
			(*mech_set)->elements[i].elements = (void *)
				MALLOC ((*mech_set)->elements[i].length);
			memcpy ((*mech_set)->elements[i].elements,
				res.mech_set.GSS_OID_SET_val[i].GSS_OID_val,
				(*mech_set)->elements[i].length);
		}
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_indicate_mechs_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);
}


OM_uint32
kgss_inquire_cred_wrapped(minor_status,
		cred_handle,
		gssd_cred_verifier,
		name,
		lifetime,
		cred_usage,
		mechanisms,
		uid)
	OM_uint32 *minor_status;
	const gssd_cred_id_t cred_handle;
	OM_uint32 gssd_cred_verifier;
	gss_name_t * name;
	OM_uint32 *lifetime;
	int * cred_usage;
	gss_OID_set * mechanisms;
	uid_t uid;
{
	CLIENT *clnt;

	OM_uint32 	minor_status_temp;
	gss_buffer_desc	external_name;
	gss_OID_desc	name_type;
	int		i;

	gss_inquire_cred_arg arg;
	gss_inquire_cred_res res;

	/* get the client handle to GSSD */

	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_inquire_cred: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}


	/* copy the procedure arguments into the rpc arg parameter */

	arg.uid = (OM_uint32) uid;

	arg.cred_handle.GSS_CRED_ID_T_len =
			cred_handle == (gssd_cred_id_t) GSS_C_NO_CREDENTIAL ?
			0 : (u_int) sizeof (gssd_cred_id_t);
	arg.cred_handle.GSS_CRED_ID_T_val = (char *) &cred_handle;
	arg.gssd_cred_verifier = gssd_cred_verifier;

	/* call the remote procedure */

	bzero((caddr_t) &res, sizeof (res));
	if (gss_inquire_cred_1(&arg, &res, clnt) != RPC_SUCCESS) {

	/*
	 * if the RPC call times out, null out all return arguments, set
	 * minor_status to its maximum value, and return GSS_S_FAILURE
	 */

		if (minor_status != NULL)
			*minor_status = DEFAULT_MINOR_STAT;
		if (name != NULL)
			*name = NULL;
		if (lifetime != NULL)
			*lifetime = 0;
		if (cred_usage != NULL)
			*cred_usage = 0;
		if (mechanisms != NULL)
			*mechanisms = NULL;

		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_inquire_cred: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the rpc results into the return arguments */

	if (minor_status != NULL)
		*minor_status = res.minor_status;

	/* convert name from external to internal format */

	if (name != NULL) {
		external_name.length = res.name.GSS_BUFFER_T_len;
		external_name.value = res.name.GSS_BUFFER_T_val;

	/*
	 * we can pass a pointer to res structure
	 * since gss_import_name treats the name_type
	 * parameter as read only and performs a copy
	 */

		name_type.length = res.name_type.GSS_OID_len;
		name_type.elements = (void *)res.name_type.GSS_OID_val;

		if (gss_import_name(&minor_status_temp, &external_name,
			&name_type, name) != GSS_S_COMPLETE) {

			*minor_status = (OM_uint32) minor_status_temp;
			(void) gss_release_buffer(&minor_status_temp,
				&external_name);

			clnt_freeres(clnt, xdr_gss_inquire_cred_res,
							(caddr_t) &res);
			killgssd_handle(clnt);
			GSSLOG0(1, "kgss_inquire_cred: import name fails\n");
			return ((OM_uint32) GSS_S_FAILURE);
		}
	}

	if (lifetime != NULL)
		*lifetime = res.lifetime;

	if (cred_usage != NULL)
		*cred_usage = res.cred_usage;

	if (mechanisms != NULL) {
		if (res.mechanisms.GSS_OID_SET_len != 0) {
			(*mechanisms)->count =
					(int) res.mechanisms.GSS_OID_SET_len;
			(*mechanisms)->elements = (gss_OID)
			MALLOC(sizeof (gss_OID) * (*mechanisms)->count);

			for (i = 0; i < (*mechanisms)->count; i++) {
			    (*mechanisms)->elements[i].length = (OM_uint32)
				res.mechanisms.GSS_OID_SET_val[i].GSS_OID_len;
			    (*mechanisms)->elements[i].elements = (void *)
				MALLOC((*mechanisms)->elements[i].length);
			    memcpy((*mechanisms)->elements[i].elements,
				res.mechanisms.GSS_OID_SET_val[i].GSS_OID_val,
				(*mechanisms)->elements[i].length);
			}
		} else
			(*mechanisms)->count = 0;
	}

	/*
	 * free the memory allocated for the results and return with the status
	 * received in the rpc call
	 */

	clnt_freeres(clnt, xdr_gss_inquire_cred_res, (caddr_t) &res);
	killgssd_handle(clnt);
	return (res.status);

}

OM_uint32
kgss_inquire_cred(minor_status,
			cred_handle,
			name,
			lifetime,
			cred_usage,
			mechanisms,
			uid)
	OM_uint32 *minor_status;
	const gss_cred_id_t cred_handle;
	gss_name_t * name;
	OM_uint32 *lifetime;
	int * cred_usage;
	gss_OID_set * mechanisms;
	uid_t uid;
{

	OM_uint32 gssd_cred_verifier;
	OM_uint32 gssd_cred_handle;

	gssd_cred_verifier = KCRED_TO_CREDV(cred_handle);
	gssd_cred_handle = KCRED_TO_CRED(cred_handle);

	return (kgss_inquire_cred_wrapped(minor_status,
			gssd_cred_handle, gssd_cred_verifier,
			name, lifetime, cred_usage, mechanisms, uid));
}


OM_uint32
kgsscred_expname_to_unix_cred(expName, uidOut, gidOut, gids, gidsLen, uid)
	const gss_buffer_t expName;
	uid_t *uidOut;
	gid_t *gidOut;
	gid_t *gids[];
	int *gidsLen;
	uid_t uid;
{
	CLIENT *clnt;
	gsscred_expname_to_unix_cred_arg args;
	gsscred_expname_to_unix_cred_res res;

	/* check input/output parameters */
	if (expName == NULL || expName->value == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	if (uidOut == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/* NULL out output parameters */
	*uidOut = UID_NOBODY;
	if (gidsLen)
		*gidsLen = 0;

	if (gids)
		*gids = NULL;

	/* get the client handle to gssd */
	if ((clnt = getgssd_handle()) == NULL)
	{
		GSSLOG(1, "kgsscred_expname_to_unix_cred:"
			" can't connect to server on %s\n", server);
		return (GSS_S_FAILURE);
	}

	/* copy the procedure arguments */
	args.uid = uid;
	args.expname.GSS_BUFFER_T_val = expName->value;
	args.expname.GSS_BUFFER_T_len = expName->length;

	/* null out the return buffer and call the remote proc */
	bzero(&res, sizeof (res));

	if (gsscred_expname_to_unix_cred_1(&args, &res, clnt) != RPC_SUCCESS)
	{
		killgssd_handle(clnt);
		GSSLOG0(1,
			"kgsscred_expname_to_unix_cred: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the results into the result parameters */
	if (res.major == GSS_S_COMPLETE)
	{
		*uidOut = res.uid;
		if (gidOut)
			*gidOut = res.gid;
		if (gids && gidsLen)
		{
			*gids = res.gids.GSSCRED_GIDS_val;
			*gidsLen = res.gids.GSSCRED_GIDS_len;
			res.gids.GSSCRED_GIDS_val = NULL;
			res.gids.GSSCRED_GIDS_len = 0;
		}
	}

	/* free RPC results */
	clnt_freeres(clnt, xdr_gsscred_expname_to_unix_cred_res, (caddr_t)&res);
	killgssd_handle(clnt);

	return (res.major);
} /* kgsscred_expname_to_unix_cred */

OM_uint32
kgsscred_name_to_unix_cred(intName, mechType, uidOut, gidOut, gids,
				gidsLen, uid)
	const gss_name_t intName;
	const gss_OID mechType;
	uid_t *uidOut;
	gid_t *gidOut;
	gid_t *gids[];
	int *gidsLen;
	uid_t uid;
{
	CLIENT *clnt;
	gsscred_name_to_unix_cred_arg args;
	gsscred_name_to_unix_cred_res res;
	OM_uint32 major, minor;
	gss_OID nameOid;
	gss_buffer_desc flatName = GSS_C_EMPTY_BUFFER;

	/* check the input/output parameters */
	if (intName == NULL || mechType == NULL)
		return (GSS_S_CALL_INACCESSIBLE_READ);

	if (uidOut == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/* NULL out the output parameters */
	*uidOut = UID_NOBODY;
	if (gids)
		*gids = NULL;

	if (gidsLen)
		*gidsLen = 0;

	/* get the client handle to gssd */
	if ((clnt = getgssd_handle()) == NULL)
	{
		GSSLOG(1,
		"kgsscred_name_to_unix_cred: can't connect to server %s\n",
				server);
		return (GSS_S_FAILURE);
	}

	/* convert the name to flat representation */
	if ((major = gss_display_name(&minor, intName, &flatName, &nameOid))
			!= GSS_S_COMPLETE)
	{
		killgssd_handle(clnt);
		GSSLOG0(1, "kgsscred_name_to_unix_cred: display name failed\n");
		return (major);
	}

	/* set the rpc parameters */
	args.uid = uid;
	args.pname.GSS_BUFFER_T_len = flatName.length;
	args.pname.GSS_BUFFER_T_val = flatName.value;
	args.name_type.GSS_OID_len = nameOid->length;
	args.name_type.GSS_OID_val = nameOid->elements;
	args.mech_type.GSS_OID_len = mechType->length;
	args.mech_type.GSS_OID_val = mechType->elements;

	/* call the remote procedure */
	bzero(&res, sizeof (res));
	if (gsscred_name_to_unix_cred_1(&args, &res, clnt) != RPC_SUCCESS) {
		killgssd_handle(clnt);
		(void) gss_release_buffer(&minor, &flatName);
		GSSLOG0(1, "kgsscred_name_to_unix_cred: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* delete the flat name buffer */
	(void) gss_release_buffer(&minor, &flatName);

	/* copy the output parameters on output */
	if (res.major == GSS_S_COMPLETE) {
		*uidOut = res.uid;

		if (gidOut)
			*gidOut = res.gid;
		if (gids && gidsLen) {
			*gids = res.gids.GSSCRED_GIDS_val;
			*gidsLen = res.gids.GSSCRED_GIDS_len;
			res.gids.GSSCRED_GIDS_val = NULL;
			res.gids.GSSCRED_GIDS_len = 0;
		}
	}

	/* delete RPC allocated memory */
	clnt_freeres(clnt, xdr_gsscred_name_to_unix_cred_res, (caddr_t)&res);
	killgssd_handle(clnt);

	return (res.major);
} /* kgsscred_name_to_unix_cred */

OM_uint32
kgss_get_group_info(puid, gidOut, gids, gidsLen, uid)
	const uid_t puid;
	gid_t *gidOut;
	gid_t *gids[];
	int *gidsLen;
	uid_t uid;
{
	CLIENT *clnt;
	gss_get_group_info_arg args;
	gss_get_group_info_res res;


	/* check the output parameters */
	if (gidOut == NULL || gids == NULL || gidsLen == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	/* get the client GSSD handle */
	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1,
			"kgss_get_group_info: can't connect to server on %s\n",
			server);
		return (GSS_S_FAILURE);
	}

	/* set the input parameters */
	args.uid = uid;
	args.puid = puid;

	/* call the remote procedure */
	bzero(&res, sizeof (res));
	if (gss_get_group_info_1(&args, &res, clnt) != RPC_SUCCESS) {
		killgssd_handle(clnt);
		GSSLOG0(1, "kgss_get_group_info: RPC call times out\n");
		return (GSS_S_FAILURE);
	}

	/* copy the results */
	if (res.major == GSS_S_COMPLETE) {
		*gidOut = res.gid;
		*gids = res.gids.GSSCRED_GIDS_val;
		*gidsLen = res.gids.GSSCRED_GIDS_len;
		res.gids.GSSCRED_GIDS_val = NULL;
		res.gids.GSSCRED_GIDS_len = 0;
	}

	/* no results to free */
	killgssd_handle(clnt);

	return (res.major);
} /* kgss_get_group_info */

static char *
kgss_get_kmod(gss_OID mech_oid)
{
	CLIENT *clnt;
	gss_get_kmod_arg args;
	gss_get_kmod_res res;


	/* get the client GSSD handle */
	if ((clnt = getgssd_handle()) == NULL) {
		GSSLOG(1, "kgss_get_kmod: can't connect to server on %s\n",
			server);
		return (NULL);
	}

	/* set the input parameters */
	args.mech_oid.GSS_OID_len = mech_oid->length;
	args.mech_oid.GSS_OID_val = mech_oid->elements;

	/* call the remote procedure */
	bzero(&res, sizeof (res));
	if (gss_get_kmod_1(&args, &res, clnt) != RPC_SUCCESS) {
		killgssd_handle(clnt);
		GSSLOG0(1, "gss_get_kmod_1: RPC call times out\n");
		return (NULL);
	}
	/* no results to free */
	killgssd_handle(clnt);

	if (res.module_follow == TRUE) {
		return (res.gss_get_kmod_res_u.modname);
	} else
		return (NULL);
} /* kgss_get_kmod */

static gss_mechanism	kgss_mech_head;
static gss_mechanism	kgss_mech_tail;
kmutex_t	__kgss_mech_lock;

/*
 * See if there is kernel mechanism module, and if so, attempt to
 * load it and reset the pointer (gss_mechanism) to the sign/seal/etc.
 * entry points to that of the kernel module.
 */
static void
__kgss_reset_mech(gss_mechanism *mechp, gss_OID mech_oid)
{
	gss_mechanism mech;
	char *kmod;

	/*
	 * We can search the list without a mutex, becuase the list never
	 * shrinks and we always add to the end.
	 */
	mech = __kgss_get_mechanism(mech_oid);
	if (mech) {
		*mechp = mech;
		return;
	}

	/*
	 * Get the module name from the kernel.
	 */
	kmod = kgss_get_kmod(mech_oid);

	if (kmod) {
		extern int modload(char *, char *);
		(void) modload("misc/kgss", kmod);

		/*
		 * Allocated in the XDR routine called by gss_get_kmod_1().
		 */
		FREE(kmod, strlen(kmod)+1);

		mech = __kgss_get_mechanism(mech_oid);
		if (mech) {
			*mechp = mech;
		}

		/*
		 * If for some reason the module load didn't take,
		 * we return anyway and hope that the next context
		 * creation succeeds.
		 */
		return;
	}


	/*
	 * No kernel module, so enter this mech oid into the list
	 * using the default sign/seal/etc. operations that upcall to
	 * gssd.
	 */
	mutex_enter(&__kgss_mech_lock);
	mech = __kgss_get_mechanism(mech_oid);
	if (mech) {
		mutex_exit(&__kgss_mech_lock);
		*mechp = mech;
		return;
	}

	/*
	 * Allocate space for the mechanism entry.
	 */
	mech = kmem_zalloc(sizeof (struct gss_config), KM_SLEEP);

	/*
	 * Copy basic information from default mechanism struct.
	 */
	*mech = default_gc;

	/*
	 * Record the real mech OID.
	 */
	mech->mech_type.length = mech_oid->length;
	mech->mech_type.elements = MALLOC(mech_oid->length);
	bcopy(mech_oid->elements,  mech->mech_type.elements, mech_oid->length);

	/*
	 * Add it to the table.
	 */
	__kgss_add_mechanism(mech);
	mutex_exit(&__kgss_mech_lock);
	*mechp = mech;
}

/*
 * Called with __kgss_mech_lock held.
 */
void
__kgss_add_mechanism(gss_mechanism mech)
{
	gss_mechanism tmp;

	tmp = kgss_mech_tail;
	kgss_mech_tail = mech;

	if (tmp != NULL)
		tmp->next = mech;

	if (kgss_mech_head == NULL)
		kgss_mech_head = mech;
}

/*
 *  given the mechs_array and a mechanism OID, return the
 *  pointer to the mechanism, or NULL if that mechanism is
 *  not supported.
 */
gss_mechanism
__kgss_get_mechanism(gss_OID type)
{
	gss_mechanism mech;

	mech = kgss_mech_head;

	/*
	 * Note that a reader can scan this list without the mutex held.
	 * This is safe because we always append, and never shrink the list.
	 * Moreover, the entry is fully initialized before it is ever
	 * added to the list.
	 */
	while (mech != NULL) {
		if ((mech->mech_type.length == type->length) &&
		    (bcmp(mech->mech_type.elements, type->elements,
		    type->length) == 0))
			return (mech);

		mech = mech->next;
	}
	return (NULL);
}
