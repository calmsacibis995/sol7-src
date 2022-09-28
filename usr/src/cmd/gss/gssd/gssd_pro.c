/*
 * Copyright (c) 1995,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gssd_proc.c 1.59	97/11/13 SMI"

/*
 *  RPC server procedures for the gssapi usermode daemon gssd.
 */

#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <strings.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <mechglueP.h>
#include "gssd.h"
#include <gssapi/gssapi.h>
#include <rpc/rpc.h>
#include <stdlib.h>
#include <syslog.h>

#define	SRVTAB	""

extern int gssd_debug;		/* declared in gssd.c */
extern  OM_uint32	gssd_time_verf;		/* declared in gssd.c */

static int checkfrom(struct svc_req *, uid_t *);
extern void set_gssd_uid(uid_t);
extern int __rpc_get_local_uid(SVCXPRT *, uid_t *);

bool_t
gss_acquire_cred_1_svc(argp, res, rqstp)
	gss_acquire_cred_arg *argp;
	gss_acquire_cred_res *res;
	struct svc_req *rqstp;
{

	OM_uint32 		minor_status;
	gss_name_t		desired_name;
	gss_OID_desc		name_type_desc;
	gss_OID			name_type = &name_type_desc;
	OM_uint32		time_req;
	gss_OID_set_desc	desired_mechs_desc;
	gss_OID_set		desired_mechs;
	int			cred_usage;
	gss_cred_id_t 		output_cred_handle;
	gss_OID_set 		actual_mechs;
	gss_buffer_desc		external_name;
	uid_t			uid;
	int			i, j;

	if (gssd_debug)
		fprintf(stderr, gettext("gss_acquire_cred\n"));

	memset(res, 0, sizeof (*res));

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->output_cred_handle.GSS_CRED_ID_T_val = NULL;
		res->actual_mechs.GSS_OID_SET_val = NULL;
		return (FALSE);
	}

/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

/* convert the desired name from external to internal format */

	external_name.length = argp->desired_name.GSS_BUFFER_T_len;
	external_name.value = (void *) malloc(external_name.length);
	if (!external_name.value)
		return (GSS_S_FAILURE);
	memcpy(external_name.value, argp->desired_name.GSS_BUFFER_T_val,
		external_name.length);

	if (argp->name_type.GSS_OID_len == 0) {
		name_type = GSS_C_NULL_OID;
	} else {
		name_type->length = argp->name_type.GSS_OID_len;
		name_type->elements = (void *) malloc(name_type->length);
		if (!name_type->elements) {
			free(external_name.value);
			return (GSS_S_FAILURE);
		}
		memcpy(name_type->elements, argp->name_type.GSS_OID_val,
			name_type->length);
	}

	if (gss_import_name(&minor_status, &external_name, name_type,
			    &desired_name) != GSS_S_COMPLETE) {

		res->status = (OM_uint32) GSS_S_FAILURE;
		res->minor_status = minor_status;

		free(external_name.value);
		if (name_type != GSS_C_NULL_OID)
			free(name_type->elements);

		return (TRUE);
	}

/*
 * copy the XDR structured arguments into their corresponding local GSSAPI
 * variables.
 */

	cred_usage = argp->cred_usage;
	time_req = argp->time_req;

	if (argp->desired_mechs.GSS_OID_SET_len != 0) {
		desired_mechs = &desired_mechs_desc;
		desired_mechs->count =
			(int) argp->desired_mechs.GSS_OID_SET_len;
		desired_mechs->elements = (gss_OID)
			malloc(sizeof (gss_OID_desc) * desired_mechs->count);
		if (!desired_mechs->elements) {
			free(external_name.value);
			free(name_type->elements);
			return (GSS_S_FAILURE);
		}
		for (i = 0; i < desired_mechs->count; i++) {
			desired_mechs->elements[i].length =
				(OM_uint32) argp->desired_mechs.
				GSS_OID_SET_val[i].GSS_OID_len;
			desired_mechs->elements[i].elements =
				(void *) malloc(desired_mechs->elements[i].
						length);
			if (!desired_mechs->elements[i].elements) {
				free(external_name.value);
				free(name_type->elements);
				for (j = 0; j < (i -1); j++) {
					free
					(desired_mechs->elements[j].elements);
				}
				free(desired_mechs->elements);
				return (GSS_S_FAILURE);
			}
			memcpy(desired_mechs->elements[i].elements,
				argp->desired_mechs.GSS_OID_SET_val[i].
				GSS_OID_val,
				desired_mechs->elements[i].length);
		}
	} else
		desired_mechs = GSS_C_NULL_OID_SET;

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_acquire_cred(&minor_status,
				desired_name,
				time_req,
				desired_mechs,
				cred_usage,
				&output_cred_handle,
				&actual_mechs,
				&res->time_rec);

	/*
	 * convert the output args from the parameter given in the call to the
	 * variable in the XDR result
	 */

	res->output_cred_handle.GSS_CRED_ID_T_len = sizeof (gss_cred_id_t);
	res->output_cred_handle.GSS_CRED_ID_T_val =
		(void *) malloc(sizeof (gss_cred_id_t));
	if (!res->output_cred_handle.GSS_CRED_ID_T_val) {
		free(external_name.value);
		free(name_type->elements);
		for (i = 0; i < desired_mechs->count; i++) {
			free(desired_mechs->elements[i].elements);
			}
		free(desired_mechs->elements);
		return (GSS_S_FAILURE);
	}
	memcpy(res->output_cred_handle.GSS_CRED_ID_T_val, &output_cred_handle,
		sizeof (gss_cred_id_t));

	if (actual_mechs != GSS_C_NULL_OID_SET) {
		res->actual_mechs.GSS_OID_SET_len =
			(u_int) actual_mechs->count;
		res->actual_mechs.GSS_OID_SET_val = (GSS_OID *)
			malloc(sizeof (GSS_OID) * actual_mechs->count);
		if (!res->actual_mechs.GSS_OID_SET_val) {
			free(external_name.value);
			free(name_type->elements);
			for (i = 0; i < desired_mechs->count; i++) {
				free(desired_mechs->elements[i].elements);
			}
			free(desired_mechs->elements);
			free(res->output_cred_handle.GSS_CRED_ID_T_val);
			return (GSS_S_FAILURE);
		}
		for (i = 0; i < actual_mechs->count; i++) {
			res->actual_mechs.GSS_OID_SET_val[i].GSS_OID_len =
				(u_int) actual_mechs->elements[i].length;
			res->actual_mechs.GSS_OID_SET_val[i].GSS_OID_val =
				(char *) malloc(actual_mechs->elements[i].
						length);
			if (!res->actual_mechs.GSS_OID_SET_val[i].GSS_OID_val) {
				free(external_name.value);
				free(name_type->elements);
				free(desired_mechs->elements);
				for (j = 0; j < desired_mechs->count; j++) {
					free
					(desired_mechs->elements[i].elements);
				}
				free(res->actual_mechs.GSS_OID_SET_val);
				for (j = 0; j < (i - 1); j++) {
					free
					(res->actual_mechs.
						GSS_OID_SET_val[j].GSS_OID_val);
				}
				return (GSS_S_FAILURE);
			}
			memcpy(res->actual_mechs.GSS_OID_SET_val[i].GSS_OID_val,
				actual_mechs->elements[i].elements,
				actual_mechs->elements[i].length);
		}
	} else
		res->actual_mechs.GSS_OID_SET_len = 0;

	/*
	 * set the time verifier for credential handle.  To ensure that the
	 * timestamp is not the same as previous gssd process, verify that
	 * time is not the same as set earlier at start of process.  If it
	 * is, sleep one second and reset. (due to one second granularity)
	 */

	if (res->status == GSS_S_COMPLETE) {
		res->gssd_cred_verifier = (OM_uint32) time(NULL);
		if (res->gssd_cred_verifier == gssd_time_verf) {
			sleep(1);
			gssd_time_verf = (OM_uint32) time(NULL);
		}
		res->gssd_cred_verifier = gssd_time_verf;
	}

	/*
	 * now release the space allocated by the underlying gssapi mechanism
	 * library for actual_mechs as well as by this routine for
	 * external_name, name_type and desired_name
	 */

	free(external_name.value);
	if (name_type != GSS_C_NULL_OID)
		free(name_type->elements);
	gss_release_name(&minor_status, &desired_name);

	if (actual_mechs != GSS_C_NULL_OID_SET) {
		for (i = 0; i < actual_mechs->count; i++)
			free(actual_mechs->elements[i].elements);
		free(actual_mechs->elements);
		free(actual_mechs);
	}

	if (desired_mechs != GSS_C_NULL_OID_SET) {
		for (i = 0; i < desired_mechs->count; i++)
			free(desired_mechs->elements[i].elements);
		free(desired_mechs->elements);

	}

/* return to caller */

	return (TRUE);
}

bool_t
gss_release_cred_1_svc(argp, res, rqstp)
gss_release_cred_arg *argp;
gss_release_cred_res *res;
struct svc_req *rqstp;
{

	uid_t uid;
	gss_cred_id_t cred_handle;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_release_cred\n"));

	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	/*
	 * if the cred_handle verifier is not correct,
	 * set status to GSS_S_DEFECTIVE_CREDENTIAL and return
	 */

	if (argp->gssd_cred_verifier != gssd_time_verf) {
		res->status = (OM_uint32) GSS_S_DEFECTIVE_CREDENTIAL;
		return (TRUE);
	}

	/*
	 * if the cred_handle length is 0
	 * set cred_handle argument to GSS_S_NO_CREDENTIAL
	 */

	if (argp->cred_handle.GSS_CRED_ID_T_len == 0)
		cred_handle = GSS_C_NO_CREDENTIAL;
	else
		cred_handle =
		/*LINTED*/
		(gss_cred_id_t *) argp->cred_handle.GSS_CRED_ID_T_val;

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_release_cred(&res->minor_status,
					cred_handle);

	/* return to caller */

	return (TRUE);
}

bool_t
gss_init_sec_context_1_svc(argp, res, rqstp)
gss_init_sec_context_arg *argp;
gss_init_sec_context_res *res;
struct svc_req *rqstp;
{

	OM_uint32 	minor_status;
	gss_ctx_id_t	context_handle;
	gss_cred_id_t	claimant_cred_handle;
	gss_buffer_desc	external_name;
	gss_OID_desc	name_type_desc;
	gss_OID		name_type = &name_type_desc;
	gss_name_t	internal_name;

	gss_OID_desc	mech_type_desc;
	gss_OID		mech_type = &mech_type_desc;
	struct gss_channel_bindings_struct
			input_chan_bindings;
	gss_channel_bindings_t input_chan_bindings_ptr;
	gss_buffer_desc input_token;
	gss_buffer_desc output_token;
	gss_buffer_t input_token_ptr;
	gss_OID actual_mech_type;

	uid_t uid;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_init_sec_context\n"));

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->context_handle.GSS_CTX_ID_T_val =  NULL;
		res->actual_mech_type.GSS_OID_val = NULL;
		res->output_token.GSS_BUFFER_T_val = NULL;
		return (FALSE);
	}

/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

/*
 * copy the supplied context handle into the local context handle, so it
 * can be supplied to the gss_init_sec_context call
 */

	context_handle = (argp->context_handle.GSS_CTX_ID_T_len == 0 ?
			GSS_C_NO_CONTEXT :
			/*LINTED*/
			*((gss_ctx_id_t *)argp->context_handle.
			GSS_CTX_ID_T_val));

	claimant_cred_handle =
		(argp->claimant_cred_handle.GSS_CRED_ID_T_len == 0 ?
		GSS_C_NO_CREDENTIAL :
		/*LINTED*/
		*((gss_cred_id_t *) argp->claimant_cred_handle.
		GSS_CRED_ID_T_val));

	if (claimant_cred_handle != GSS_C_NO_CREDENTIAL)
	/* verify the verifier_cred_handle */
		if (argp->gssd_cred_verifier != gssd_time_verf) {
			res->context_handle.GSS_CTX_ID_T_val = NULL;
			res->output_token.GSS_BUFFER_T_val = NULL;
			res->actual_mech_type.GSS_OID_val = NULL;
			res->context_handle.GSS_CTX_ID_T_len = 0;
			res->output_token.GSS_BUFFER_T_len = 0;
			res->actual_mech_type.GSS_OID_len = 0;
			res->status = (OM_uint32) GSS_S_DEFECTIVE_CREDENTIAL;
			res->minor_status = 0;
			return (TRUE);
		}

	if (context_handle != GSS_C_NO_CONTEXT)
	/* verify the verifier_context_handle */

		if (argp->gssd_context_verifier != gssd_time_verf) {
			res->context_handle.GSS_CTX_ID_T_val = NULL;
			res->output_token.GSS_BUFFER_T_val = NULL;
			res->actual_mech_type.GSS_OID_val = NULL;
			res->context_handle.GSS_CTX_ID_T_len = 0;
			res->output_token.GSS_BUFFER_T_len = 0;
			res->actual_mech_type.GSS_OID_len = 0;
			res->status = (OM_uint32) GSS_S_NO_CONTEXT;
			res->minor_status = 0;
			return (TRUE);
		}

	/* convert the target name from external to internal format */

	external_name.length = argp->target_name.GSS_BUFFER_T_len;
	external_name.value = (void *) argp->target_name.GSS_BUFFER_T_val;

	if (argp->name_type.GSS_OID_len == 0) {
		name_type = GSS_C_NULL_OID;
	} else {
		name_type->length = argp->name_type.GSS_OID_len;
		name_type->elements = (void *) malloc(name_type->length);
		if (!name_type->elements)
			return (GSS_S_FAILURE);
		memcpy(name_type->elements, argp->name_type.GSS_OID_val,
			name_type->length);
	}

	if (argp->mech_type.GSS_OID_len == 0)
		mech_type = GSS_C_NULL_OID;
	else {
		mech_type->length = (OM_uint32) argp->mech_type.GSS_OID_len;
		mech_type->elements = (void *) argp->mech_type.GSS_OID_val;
	}

	if (gss_import_name(&minor_status, &external_name, name_type,
			    &internal_name) != GSS_S_COMPLETE) {

		if (name_type != GSS_C_NULL_OID)
			free(name_type->elements);
		res->status = (OM_uint32) GSS_S_FAILURE;
		res->minor_status = minor_status;

		return (TRUE);
	}
/*
 * copy the XDR structured arguments into their corresponding local GSSAPI
 * variables.
 */

	if (argp->input_chan_bindings.present == YES) {
		input_chan_bindings_ptr = &input_chan_bindings;
		input_chan_bindings.initiator_addrtype =
			(OM_uint32) argp->input_chan_bindings.
			initiator_addrtype;
		input_chan_bindings.initiator_address.length =
			(u_int) argp->input_chan_bindings.initiator_address.
			GSS_BUFFER_T_len;
		input_chan_bindings.initiator_address.value =
			(void *) argp->input_chan_bindings.initiator_address.
			GSS_BUFFER_T_val;
		input_chan_bindings.acceptor_addrtype =
			(OM_uint32) argp->input_chan_bindings.acceptor_addrtype;
		input_chan_bindings.acceptor_address.length =
			(u_int) argp->input_chan_bindings.acceptor_address.
			GSS_BUFFER_T_len;
		input_chan_bindings.acceptor_address.value =
			(void *) argp->input_chan_bindings.acceptor_address.
			GSS_BUFFER_T_val;
		input_chan_bindings.application_data.length =
			(u_int) argp->input_chan_bindings.application_data.
			GSS_BUFFER_T_len;
		input_chan_bindings.application_data.value =
			(void *) argp->input_chan_bindings.application_data.
			GSS_BUFFER_T_val;
	} else {
		input_chan_bindings_ptr = GSS_C_NO_CHANNEL_BINDINGS;
		input_chan_bindings.initiator_addrtype = 0;
		input_chan_bindings.initiator_address.length = 0;
		input_chan_bindings.initiator_address.value = 0;
		input_chan_bindings.acceptor_addrtype = 0;
		input_chan_bindings.acceptor_address.length = 0;
		input_chan_bindings.acceptor_address.value = 0;
		input_chan_bindings.application_data.length = 0;
		input_chan_bindings.application_data.value = 0;
	}

	if (argp->input_token.GSS_BUFFER_T_len == 0) {
		input_token_ptr = GSS_C_NO_BUFFER;
	} else {
		input_token_ptr = &input_token;
		input_token.length = (size_t)
				argp->input_token.GSS_BUFFER_T_len;
		input_token.value = (void *) argp->input_token.GSS_BUFFER_T_val;
	}

/* call the gssapi routine */

	res->status = (OM_uint32) gss_init_sec_context(&res->minor_status,
			(gss_cred_id_t) argp->claimant_cred_handle.
						GSS_CRED_ID_T_val,
					&context_handle,
					internal_name,
					mech_type,
					argp->req_flags,
					argp->time_req,
					input_chan_bindings_ptr,
					input_token_ptr,
					&actual_mech_type,
					&output_token,
					&res->ret_flags,
					&res->time_rec);

	/*
	 * convert the output args from the parameter given in the call to the
	 * variable in the XDR result
	 */

	if (res->status == (OM_uint32) GSS_S_COMPLETE ||
		res->status == (OM_uint32) GSS_S_CONTINUE_NEEDED) {

		res->gssd_context_verifier = gssd_time_verf;
		res->context_handle.GSS_CTX_ID_T_len = sizeof (gss_ctx_id_t);
		res->context_handle.GSS_CTX_ID_T_val =
			(void *) malloc(sizeof (gss_ctx_id_t));
		if (!res->context_handle.GSS_CTX_ID_T_val) {
			free(name_type->elements);
			return (GSS_S_FAILURE);
		}

		memcpy(res->context_handle.GSS_CTX_ID_T_val, &context_handle,
			sizeof (gss_ctx_id_t));

		res->output_token.GSS_BUFFER_T_len =
			(u_int) output_token.length;
		res->output_token.GSS_BUFFER_T_val =
			(char *) output_token.value;

		/*
		 * the actual mech type parameter
		 * is ready only upon GSS_S_COMPLETE
		 */
		if (res->status == GSS_S_COMPLETE) {
			res->actual_mech_type.GSS_OID_len =
				(u_int) actual_mech_type->length;
			res->actual_mech_type.GSS_OID_val =
				(void *) malloc(actual_mech_type->length);
			if (!res->actual_mech_type.GSS_OID_val) {
				free(name_type->elements);
				free(res->context_handle.GSS_CTX_ID_T_val);
				return (GSS_S_FAILURE);
			}
			memcpy(res->actual_mech_type.GSS_OID_val,
				(char *) actual_mech_type->elements,
				actual_mech_type->length);
		} else
			res->actual_mech_type.GSS_OID_len = 0;
	} else {
		if (context_handle != GSS_C_NO_CONTEXT) {
			(void) gss_delete_sec_context(&minor_status,
				&context_handle, NULL);
		}
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->actual_mech_type.GSS_OID_len = 0;
		res->output_token.GSS_BUFFER_T_len = 0;
	}

	/*
	 * now release the space allocated by the underlying gssapi mechanism
	 * library for internal_name and for the name_type.
	 */

	gss_release_name(&minor_status, &internal_name);
	if (name_type != GSS_C_NULL_OID)
		free(name_type->elements);


	/* return to caller */
	return (TRUE);
}

bool_t
gss_accept_sec_context_1_svc(argp, res, rqstp)
gss_accept_sec_context_arg *argp;
gss_accept_sec_context_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	OM_uint32 minor_status;
	gss_ctx_id_t context_handle;
	gss_cred_id_t verifier_cred_handle;
	gss_buffer_desc external_name;
	gss_name_t internal_name;

	gss_buffer_desc input_token_buffer;
	gss_buffer_t input_token_buffer_ptr;
	struct gss_channel_bindings_struct
			input_chan_bindings;
	gss_channel_bindings_t input_chan_bindings_ptr;
	gss_OID mech_type;
	gss_buffer_desc output_token;
	gss_cred_id_t delegated_cred_handle;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_accept_sec_context\n"));

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->src_name.GSS_BUFFER_T_val = NULL;
		res->mech_type.GSS_OID_val = NULL;
		res->output_token.GSS_BUFFER_T_val = NULL;
		res->delegated_cred_handle.GSS_CRED_ID_T_val = NULL;
		return (FALSE);
	}

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	/*
	 * copy the supplied context handle into the local context handle, so
	 * it can be supplied to the gss_accept_sec_context call
	 */

	context_handle = (argp->context_handle.GSS_CTX_ID_T_len == 0 ?
				GSS_C_NO_CONTEXT :
				/*LINTED*/
				*((gss_ctx_id_t *) argp->context_handle.
					GSS_CTX_ID_T_val));

	if (context_handle != GSS_C_NO_CREDENTIAL)
	/* verify the context_handle */
		if (argp->gssd_context_verifier != gssd_time_verf) {
			res->context_handle.GSS_CTX_ID_T_val = NULL;
			res->src_name.GSS_BUFFER_T_val = NULL;
			res->mech_type.GSS_OID_val = NULL;
			res->output_token.GSS_BUFFER_T_val = NULL;
			res->delegated_cred_handle.GSS_CRED_ID_T_val = NULL;
			res->src_name.GSS_BUFFER_T_len = 0;
			res->context_handle.GSS_CTX_ID_T_len = 0;
			res->delegated_cred_handle.GSS_CRED_ID_T_len = 0;
			res->output_token.GSS_BUFFER_T_len = 0;
			res->mech_type.GSS_OID_len = 0;
			res->status = (OM_uint32) GSS_S_NO_CONTEXT;
			res->minor_status = 0;
			return (TRUE);
		}

	/*
	 * copy the XDR structured arguments into their corresponding local
	 * GSSAPI variable equivalents.
	 */


	verifier_cred_handle =
		(argp->verifier_cred_handle.GSS_CRED_ID_T_len == 0 ?
			GSS_C_NO_CREDENTIAL :
			/*LINTED*/
			*((gss_cred_id_t *) argp->verifier_cred_handle.
				GSS_CRED_ID_T_val));

	if (verifier_cred_handle != GSS_C_NO_CREDENTIAL)
	/* verify the verifier_cred_handle */
		if (argp->gssd_cred_verifier != gssd_time_verf) {
			res->context_handle.GSS_CTX_ID_T_val = NULL;
			res->src_name.GSS_BUFFER_T_val = NULL;
			res->mech_type.GSS_OID_val = NULL;
			res->output_token.GSS_BUFFER_T_val = NULL;
			res->delegated_cred_handle.GSS_CRED_ID_T_val = NULL;
			res->src_name.GSS_BUFFER_T_len = 0;
			res->context_handle.GSS_CTX_ID_T_len = 0;
			res->delegated_cred_handle.GSS_CRED_ID_T_len = 0;
			res->output_token.GSS_BUFFER_T_len = 0;
			res->mech_type.GSS_OID_len = 0;
			res->status = (OM_uint32) GSS_S_DEFECTIVE_CREDENTIAL;
			res->minor_status = 0;
			return (TRUE);
		}

	if (argp->input_token_buffer.GSS_BUFFER_T_len == 0) {
		input_token_buffer_ptr = GSS_C_NO_BUFFER;
	} else {
		input_token_buffer_ptr = &input_token_buffer;
		input_token_buffer.length = (size_t) argp->input_token_buffer.
						GSS_BUFFER_T_len;
		input_token_buffer.value = (void *) argp->input_token_buffer.
						GSS_BUFFER_T_val;
	}

	if (argp->input_chan_bindings.present == YES) {
		input_chan_bindings_ptr = &input_chan_bindings;
		input_chan_bindings.initiator_addrtype =
			(OM_uint32) argp->input_chan_bindings.
					initiator_addrtype;
		input_chan_bindings.initiator_address.length =
			(u_int) argp->input_chan_bindings.initiator_address.
					GSS_BUFFER_T_len;
		input_chan_bindings.initiator_address.value =
			(void *) argp->input_chan_bindings.initiator_address.
					GSS_BUFFER_T_val;
		input_chan_bindings.acceptor_addrtype =
			(OM_uint32) argp->input_chan_bindings.
					acceptor_addrtype;
		input_chan_bindings.acceptor_address.length =
			(u_int) argp->input_chan_bindings.acceptor_address.
					GSS_BUFFER_T_len;
		input_chan_bindings.acceptor_address.value =
			(void *) argp->input_chan_bindings.acceptor_address.
					GSS_BUFFER_T_val;
		input_chan_bindings.application_data.length =
			(u_int) argp->input_chan_bindings.application_data.
					GSS_BUFFER_T_len;
		input_chan_bindings.application_data.value =
			(void *) argp->input_chan_bindings.application_data.
					GSS_BUFFER_T_val;
	} else {
		input_chan_bindings_ptr = GSS_C_NO_CHANNEL_BINDINGS;
		input_chan_bindings.initiator_addrtype = 0;
		input_chan_bindings.initiator_address.length = 0;
		input_chan_bindings.initiator_address.value = 0;
		input_chan_bindings.acceptor_addrtype = 0;
		input_chan_bindings.acceptor_address.length = 0;
		input_chan_bindings.acceptor_address.value = 0;
		input_chan_bindings.application_data.length = 0;
		input_chan_bindings.application_data.value = 0;
	}


	/* call the gssapi routine */

	res->status = (OM_uint32) gss_accept_sec_context(&res->minor_status,
						&context_handle,
						verifier_cred_handle,
						input_token_buffer_ptr,
						input_chan_bindings_ptr,
						&internal_name,
						&mech_type,
						&output_token,
						&res->ret_flags,
						&res->time_rec,
						&delegated_cred_handle);

	/* convert the src name from internal to external format */

	if (res->status == (OM_uint32) GSS_S_COMPLETE ||
		res->status == (OM_uint32) GSS_S_CONTINUE_NEEDED) {

		/*
		 * upon GSS_S_CONTINUE_NEEDED only the following
		 * parameters are ready: minor, ctxt, and output token
		 */
		res->context_handle.GSS_CTX_ID_T_len = sizeof (gss_ctx_id_t);
		res->context_handle.GSS_CTX_ID_T_val =
			(void *) malloc(sizeof (gss_ctx_id_t));
		if (!res->context_handle.GSS_CTX_ID_T_val)
			return (GSS_S_FAILURE);

		memcpy(res->context_handle.GSS_CTX_ID_T_val, &context_handle,
			sizeof (gss_ctx_id_t));
		res->gssd_context_verifier = gssd_time_verf;

		res->output_token.GSS_BUFFER_T_len =
				(u_int) output_token.length;
		res->output_token.GSS_BUFFER_T_val =
				(char *) output_token.value;

		if (res->status == GSS_S_COMPLETE) {
			if (gss_export_name(&minor_status, internal_name,
					&external_name)
				!= GSS_S_COMPLETE) {

				res->status = (OM_uint32) GSS_S_FAILURE;
				res->minor_status = minor_status;
				gss_release_name(&minor_status, &internal_name);
				free(res->context_handle.GSS_CTX_ID_T_val);
				res->context_handle.GSS_CTX_ID_T_val = NULL;
				res->context_handle.GSS_CTX_ID_T_len = 0;
				gss_release_buffer(&minor_status,
						&output_token);
				res->output_token.GSS_BUFFER_T_len = 0;
				res->output_token.GSS_BUFFER_T_val = NULL;
				return (TRUE);
			}
			res->src_name.GSS_BUFFER_T_len =
				(u_int) external_name.length;
			res->src_name.GSS_BUFFER_T_val =
				(void *) external_name.value;

			res->delegated_cred_handle.GSS_CRED_ID_T_len =
				sizeof (gss_cred_id_t);
			res->delegated_cred_handle.GSS_CRED_ID_T_val =
				(void *) malloc(sizeof (gss_cred_id_t));
			if (!res->delegated_cred_handle.GSS_CRED_ID_T_val) {
				free(res->context_handle.GSS_CTX_ID_T_val);
				return (GSS_S_FAILURE);
			}
			memcpy(res->delegated_cred_handle.GSS_CRED_ID_T_val,
				&delegated_cred_handle,
				sizeof (gss_cred_id_t));

			res->mech_type.GSS_OID_len = (u_int) mech_type->length;
			res->mech_type.GSS_OID_val =
				(void *) malloc(mech_type->length);
			if (!res->mech_type.GSS_OID_val) {
			free(res->context_handle.GSS_CTX_ID_T_val);
			free(res->delegated_cred_handle.GSS_CRED_ID_T_val);
			return (GSS_S_FAILURE);
			}
			memcpy(res->mech_type.GSS_OID_val, mech_type->elements,
				mech_type->length);

			/* release the space allocated for internal_name */
			gss_release_name(&minor_status, &internal_name);
		} else {    /* GSS_S_CONTINUE_NEEDED */
			res->src_name.GSS_BUFFER_T_len = 0;
			res->delegated_cred_handle.GSS_CRED_ID_T_len = 0;
			res->mech_type.GSS_OID_len = 0;
		}
	} else {
		if (context_handle != GSS_C_NO_CONTEXT) {
			(void) gss_delete_sec_context(&minor_status,
				&context_handle, NULL);
		}
		res->src_name.GSS_BUFFER_T_len = 0;
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->delegated_cred_handle.GSS_CRED_ID_T_len = 0;
		res->output_token.GSS_BUFFER_T_len = 0;
		res->mech_type.GSS_OID_len = 0;
	}

/* return to caller */

	return (TRUE);
}

bool_t
gss_process_context_token_1_svc(argp, res, rqstp)
gss_process_context_token_arg *argp;
gss_process_context_token_res *res;
struct svc_req *rqstp;
{

	uid_t uid;
	gss_buffer_desc token_buffer;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_process_context_token\n"));

	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* verify the context_handle */

	if (argp->gssd_context_verifier != gssd_time_verf) {
		res->status = (OM_uint32) GSS_S_NO_CONTEXT;
		res->minor_status = 0;
		return (TRUE);
	}

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	/*
	 * copy the XDR structured arguments into their corresponding local
	 * GSSAPI variable equivalents.
	 */

	token_buffer.length = (size_t) argp->token_buffer.GSS_BUFFER_T_len;
	token_buffer.value = (void *) argp->token_buffer.GSS_BUFFER_T_val;


	/* call the gssapi routine */

	res->status = (OM_uint32) gss_process_context_token(&res->minor_status,
				(gss_ctx_id_t) argp->context_handle.
						GSS_CTX_ID_T_val,
				&token_buffer);


	/* return to caller */

	return (TRUE);
}

bool_t
gss_delete_sec_context_1_svc(argp, res, rqstp)
gss_delete_sec_context_arg *argp;
gss_delete_sec_context_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	gss_ctx_id_t  context_handle;
	gss_buffer_desc output_token;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_delete_sec_context\n"));

	/* verify the context_handle */

	if (argp->gssd_context_verifier != gssd_time_verf) {
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->output_token.GSS_BUFFER_T_val = NULL;
		res->output_token.GSS_BUFFER_T_len = 0;
		res->status = (OM_uint32) GSS_S_NO_CONTEXT;
		res->minor_status = 0;
		return (TRUE);
	}

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->output_token.GSS_BUFFER_T_val = NULL;
		return (FALSE);
	}


/*
 * copy the supplied context handle into the local context handle, so it
 * can be supplied to the gss_delete_sec_context call
 */

	context_handle = (argp->context_handle.GSS_CTX_ID_T_len == 0 ?
				GSS_C_NO_CONTEXT :
				/*LINTED*/
				*((gss_ctx_id_t *) argp->context_handle.
					GSS_CTX_ID_T_val));

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_delete_sec_context(&res->minor_status,
						&context_handle,
						&output_token);

	/*
	 * convert the output args from the parameter given in the call to the
	 * variable in the XDR result. If the delete succeeded, return a zero
	 * context handle.
	 */

	if (res->status == GSS_S_COMPLETE) {
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->output_token.GSS_BUFFER_T_len =
			(u_int) output_token.length;
		res->output_token.GSS_BUFFER_T_val =
			(char *) output_token.value;
	} else {
		res->context_handle.GSS_CTX_ID_T_len = sizeof (gss_ctx_id_t);
		res->context_handle.GSS_CTX_ID_T_val =
			(void *) malloc(sizeof (gss_ctx_id_t));
		if (!res->context_handle.GSS_CTX_ID_T_val) {
			return (GSS_S_FAILURE);
		}
		memcpy(res->context_handle.GSS_CTX_ID_T_val, &context_handle,
			sizeof (gss_ctx_id_t));
		res->output_token.GSS_BUFFER_T_len = 0;
		res->output_token.GSS_BUFFER_T_val = NULL;
	}

	/* return to caller */

	return (TRUE);
}


bool_t
gss_export_sec_context_1_svc(argp, res, rqstp)
	gss_export_sec_context_arg *argp;
	gss_export_sec_context_res *res;
	struct svc_req *rqstp;
{

	uid_t		uid;
	gss_ctx_id_t	context_handle;
	gss_buffer_desc	output_token;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, "gss_export_sec_context\n");

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->output_token.GSS_BUFFER_T_val = NULL;
		return (FALSE);
	}


/*
 * copy the supplied context handle into the local context handle, so it
 * can be supplied to the gss_export_sec_context call
 */

	context_handle = (argp->context_handle.GSS_CTX_ID_T_len == 0 ?
		GSS_C_NO_CONTEXT :
		*((gss_ctx_id_t *) argp->context_handle.GSS_CTX_ID_T_val));

/* call the gssapi routine */

	res->status = (OM_uint32) gss_export_sec_context(&res->minor_status,
					&context_handle,
					&output_token);

/*
 * convert the output args from the parameter given in the call to the
 * variable in the XDR result. If the delete succeeded, return a zero context
 * handle.
 */
	if (res->status == GSS_S_COMPLETE) {
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		res->output_token.GSS_BUFFER_T_len =
						(u_int) output_token.length;
		res->output_token.GSS_BUFFER_T_val =
						(char *) output_token.value;
	} else {
		res->context_handle.GSS_CTX_ID_T_len = sizeof (gss_ctx_id_t);
		res->context_handle.GSS_CTX_ID_T_val =
					(void *) malloc(sizeof (gss_ctx_id_t));
		memcpy(res->context_handle.GSS_CTX_ID_T_val, &context_handle,
					sizeof (gss_ctx_id_t));
		res->output_token.GSS_BUFFER_T_len = 0;
		res->output_token.GSS_BUFFER_T_val = NULL;
	}


	/* return to caller */

	return (TRUE);
}

bool_t
gss_import_sec_context_1_svc(argp, res, rqstp)
	gss_import_sec_context_arg *argp;
	gss_import_sec_context_res *res;
	struct svc_req *rqstp;
{

	uid_t		uid;
	gss_ctx_id_t	context_handle;
	gss_buffer_desc	input_token;
	gss_buffer_t input_token_ptr;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, "gss_export_sec_context\n");

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->context_handle.GSS_CTX_ID_T_val = NULL;
		return (FALSE);
	}


	if (argp->input_token.GSS_BUFFER_T_len == 0) {
		input_token_ptr = GSS_C_NO_BUFFER;
	} else {
		input_token_ptr = &input_token;
		input_token.length = (size_t)
				argp->input_token.GSS_BUFFER_T_len;
		input_token.value = (void *) argp->input_token.GSS_BUFFER_T_val;
	}


/* call the gssapi routine */

	res->status = (OM_uint32) gss_import_sec_context(&res->minor_status,
					input_token_ptr,
					&context_handle);

/*
 * convert the output args from the parameter given in the call to the
 * variable in the XDR result. If the delete succeeded, return a zero context
 * handle.
 */
	if (res->status == GSS_S_COMPLETE) {
		res->context_handle.GSS_CTX_ID_T_len = sizeof (gss_ctx_id_t);
		res->context_handle.GSS_CTX_ID_T_val =
					(void *) malloc(sizeof (gss_ctx_id_t));
		memcpy(res->context_handle.GSS_CTX_ID_T_val, &context_handle,
			sizeof (gss_ctx_id_t));
	} else {
		res->context_handle.GSS_CTX_ID_T_len = 0;
		res->context_handle.GSS_CTX_ID_T_val = NULL;
	}


	/* return to caller */

	return (TRUE);
}

bool_t
gss_context_time_1_svc(argp, res, rqstp)
gss_context_time_arg *argp;
gss_context_time_res *res;
struct svc_req *rqstp;
{
	uid_t uid;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_context_time\n"));

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	/* Semantics go here */

	return (TRUE);
}

bool_t
gss_sign_1_svc(argp, res, rqstp)
gss_sign_arg *argp;
gss_sign_res *res;
struct svc_req *rqstp;
{

	uid_t uid;

	gss_buffer_desc message_buffer;
	gss_buffer_desc msg_token;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_sign\n"));

	/* verify the context_handle */

	if (argp->gssd_context_verifier != gssd_time_verf) {
		res->msg_token.GSS_BUFFER_T_val = NULL;
		res->msg_token.GSS_BUFFER_T_len = 0;
		res->status = (OM_uint32) GSS_S_NO_CONTEXT;
		res->minor_status = 0;
		return (TRUE);
	}


	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->msg_token.GSS_BUFFER_T_val = NULL;
		return (FALSE);
	}

	/*
	 * copy the XDR structured arguments into their corresponding local
	 * GSSAPI variable equivalents.
	 */

	message_buffer.length = (size_t) argp->message_buffer.GSS_BUFFER_T_len;
	message_buffer.value = (void *) argp->message_buffer.GSS_BUFFER_T_val;

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_sign(&res->minor_status,
					/*LINTED*/
					*((gss_ctx_id_t *) argp->context_handle.
						GSS_CTX_ID_T_val),
					argp->qop_req,
					(gss_buffer_t) &message_buffer,
					(gss_buffer_t) &msg_token);
	/*
	 * convert the output args from the parameter given in the call to
	 * the variable in the XDR result
	 */

	if (res->status == GSS_S_COMPLETE) {
		res->msg_token.GSS_BUFFER_T_len = (u_int) msg_token.length;
		res->msg_token.GSS_BUFFER_T_val = (char *) msg_token.value;
	}

	/* return to caller */

	return (TRUE);
}

bool_t
gss_verify_1_svc(argp, res, rqstp)
gss_verify_arg *argp;
gss_verify_res *res;
struct svc_req *rqstp;
{

	uid_t uid;

	gss_buffer_desc message_buffer;
	gss_buffer_desc token_buffer;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_verify\n"));

	/* verify the context_handle */

	if (argp->gssd_context_verifier != gssd_time_verf) {
		res->status = (OM_uint32) GSS_S_NO_CONTEXT;
		res->minor_status = 0;
		return (TRUE);
	}

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/*
	 * copy the XDR structured arguments into their corresponding local
	 * GSSAPI variable equivalents.
	 */

	message_buffer.length = (size_t) argp->message_buffer.GSS_BUFFER_T_len;
	message_buffer.value = (void *) argp->message_buffer.GSS_BUFFER_T_val;

	token_buffer.length = (size_t) argp->token_buffer.GSS_BUFFER_T_len;
	token_buffer.value = (void *) argp->token_buffer.GSS_BUFFER_T_val;

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_verify(&res->minor_status,
				/*LINTED*/
				*((gss_ctx_id_t *) argp->context_handle.
					GSS_CTX_ID_T_val),
				&message_buffer,
				&token_buffer,
				&res->qop_state);

	/* return to caller */

	return (TRUE);
}


bool_t
gss_display_status_1_svc(argp, res, rqstp)
gss_display_status_arg *argp;
gss_display_status_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	gss_OID mech_type;
	gss_OID_desc mech_type_desc;
	gss_buffer_desc status_string;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_display_status\n"));

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->status_string.GSS_BUFFER_T_val = NULL;
		return (FALSE);
	}

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	/*
	 * copy the XDR structured arguments into their corresponding local
	 * GSSAPI variables.
	 */

	if (argp->mech_type.GSS_OID_len == 0)
		mech_type = GSS_C_NULL_OID;
	else {
		mech_type = &mech_type_desc;
		mech_type_desc.length = (OM_uint32) argp->mech_type.GSS_OID_len;
		mech_type_desc.elements = (void *) argp->mech_type.GSS_OID_val;
	}


	/* call the gssapi routine */

	res->status = (OM_uint32) gss_display_status(&res->minor_status,
					argp->status_value,
					argp->status_type,
					mech_type,
					(OM_uint32 *)&res->message_context,
					&status_string);

	/*
	 * convert the output args from the parameter given in the call to the
	 * variable in the XDR result
	 */

	if (res->status == GSS_S_COMPLETE) {
		res->status_string.GSS_BUFFER_T_len =
			(u_int) status_string.length;
		res->status_string.GSS_BUFFER_T_val =
			(char *) status_string.value;
	}

	return (TRUE);

}

/*ARGSUSED*/
bool_t
gss_indicate_mechs_1_svc(argp, res, rqstp)
	void *argp;
	gss_indicate_mechs_res *res;
	struct svc_req *rqstp;
{
	gss_OID_set oid_set;
	uid_t uid;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_indicate_mechs\n"));

	res->mech_set.GSS_OID_SET_val = NULL;

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		return (FALSE);
	}

	res->status = gss_indicate_mechs(&res->minor_status, &oid_set);

	if (res->status == GSS_S_COMPLETE) {
		int i, j;

		res->mech_set.GSS_OID_SET_len = oid_set->count;
		res->mech_set.GSS_OID_SET_val = (void *)
				malloc(oid_set->count * sizeof (GSS_OID));
		if (!res->mech_set.GSS_OID_SET_val) {
			return (GSS_S_FAILURE);
		}
		for (i = 0; i < oid_set->count; i++) {
			res->mech_set.GSS_OID_SET_val[i].GSS_OID_len =
				oid_set->elements[i].length;
			res->mech_set.GSS_OID_SET_val[i].GSS_OID_val =
				(char *) malloc(oid_set->elements[i].length);
			if (!res->mech_set.GSS_OID_SET_val[i].GSS_OID_val) {
				for (j = 0; j < (i -1); j++) {
				free
				(res->mech_set.GSS_OID_SET_val[i].GSS_OID_val);
				}
				free(res->mech_set.GSS_OID_SET_val);
				return (GSS_S_FAILURE);
			}
			memcpy(res->mech_set.GSS_OID_SET_val[i].GSS_OID_val,
				oid_set->elements[i].elements,
				oid_set->elements[i].length);
		}
	}

	return (TRUE);
}

bool_t
gss_inquire_cred_1_svc(argp, res, rqstp)
gss_inquire_cred_arg *argp;
gss_inquire_cred_res *res;
struct svc_req *rqstp;
{

	uid_t uid;

	OM_uint32 minor_status;
	gss_cred_id_t cred_handle;
	gss_buffer_desc external_name;
	gss_OID name_type;
	gss_name_t internal_name;
	gss_OID_set mechanisms;
	int i, j;

	memset(res, 0, sizeof (*res));

	if (gssd_debug)
		fprintf(stderr, gettext("gss_inquire_cred\n"));

	/* verify the verifier_cred_handle */

	if (argp->gssd_cred_verifier != gssd_time_verf) {
		res->name.GSS_BUFFER_T_val = NULL;
		res->name_type.GSS_OID_val = NULL;
		res->mechanisms.GSS_OID_SET_val = NULL;
		res->status = (OM_uint32) GSS_S_DEFECTIVE_CREDENTIAL;
		res->minor_status = 0;
		return (TRUE);
	}

	/*
	 * if the request isn't from root, null out the result pointer
	 * entries, so the next time through xdr_free won't try to
	 * free unmalloc'd memory and then return NULL
	 */

	if (checkfrom(rqstp, &uid) == 0) {
		res->name.GSS_BUFFER_T_val = NULL;
		res->name_type.GSS_OID_val = NULL;
		res->mechanisms.GSS_OID_SET_val = NULL;
		return (FALSE);
	}

	/* set the uid sent as the RPC argument */

	uid = argp->uid;
	set_gssd_uid(uid);

	cred_handle = (argp->cred_handle.GSS_CRED_ID_T_len == 0 ?
			GSS_C_NO_CREDENTIAL :
			/*LINTED*/
			*((gss_cred_id_t *) argp->cred_handle.
				GSS_CRED_ID_T_val));

	/* call the gssapi routine */

	res->status = (OM_uint32) gss_inquire_cred(&res->minor_status,
					cred_handle,
					&internal_name,
					&res->lifetime,
					&res->cred_usage,
					&mechanisms);

	if (res->status != GSS_S_COMPLETE)
		return (TRUE);

	/* convert the returned name from internal to external format */

	if (gss_display_name(&minor_status, &internal_name,
				&external_name, &name_type)
			!= GSS_S_COMPLETE) {

		res->status = (OM_uint32) GSS_S_FAILURE;
		res->minor_status = minor_status;

		gss_release_name(&minor_status, &internal_name);

		if (mechanisms != GSS_C_NULL_OID_SET) {
			for (i = 0; i < mechanisms->count; i++)
				free(mechanisms->elements[i].elements);
			free(mechanisms->elements);
			free(mechanisms);
		}

		return (TRUE);
	}

	/*
	 * convert the output args from the parameter given in the call to the
	 * variable in the XDR result
	 */

	res->name.GSS_BUFFER_T_len = (u_int) external_name.length;
	res->name.GSS_BUFFER_T_val = (void *) external_name.value;

	/*
	 * we have to allocate storage for name_type here, since the value
	 * returned from gss_display_name points to the underlying mechanism
	 * static storage. If we didn't allocate storage, the next time
	 * through this routine, the xdr_free() call at the beginning would
	 * try to free up that static storage.
	 */

	res->name_type.GSS_OID_len = (u_int) name_type->length;
	res->name_type.GSS_OID_val = (void *) malloc(name_type->length);
	if (!res->name_type.GSS_OID_val) {
		return (GSS_S_FAILURE);
	}
	memcpy(res->name_type.GSS_OID_val, name_type->elements,
		name_type->length);

	if (mechanisms != GSS_C_NULL_OID_SET) {
		res->mechanisms.GSS_OID_SET_len =
			(u_int) mechanisms->count;
		res->mechanisms.GSS_OID_SET_val = (GSS_OID *)
				malloc(sizeof (GSS_OID) * mechanisms->count);
		if (!res->mechanisms.GSS_OID_SET_val) {
			free(res->name_type.GSS_OID_val);
			return (GSS_S_FAILURE);
		}
		for (i = 0; i < mechanisms->count; i++) {
			res->mechanisms.GSS_OID_SET_val[i].GSS_OID_len =
				(u_int) mechanisms->elements[i].length;
			res->mechanisms.GSS_OID_SET_val[i].GSS_OID_val =
				(char *) malloc(mechanisms->elements[i].
						length);
			if (!res->mechanisms.GSS_OID_SET_val[i].GSS_OID_val) {
				free(res->name_type.GSS_OID_val);
				for (j = 0; j < i; j++) {
				free(res->mechanisms.
					GSS_OID_SET_val[i].GSS_OID_val);
				}
				free(res->mechanisms.GSS_OID_SET_val);
				return (GSS_S_FAILURE);
			}
			memcpy(res->mechanisms.GSS_OID_SET_val[i].GSS_OID_val,
				mechanisms->elements[i].elements,
				mechanisms->elements[i].length);
		}
	} else
		res->mechanisms.GSS_OID_SET_len = 0;

	/* release the space allocated for internal_name and mechanisms */
	gss_release_name(&minor_status, &internal_name);

	if (mechanisms != GSS_C_NULL_OID_SET) {
		for (i = 0; i < mechanisms->count; i++)
			free(mechanisms->elements[i].elements);
		free(mechanisms->elements);
		free(mechanisms);
	}

	/* return to caller */
	return (TRUE);
}


bool_t
gsscred_name_to_unix_cred_1_svc(argsp, res, rqstp)
gsscred_name_to_unix_cred_arg *argsp;
gsscred_name_to_unix_cred_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	gss_OID_desc oid;
	gss_name_t gssName;
	gss_buffer_desc gssBuf = GSS_C_EMPTY_BUFFER;
	OM_uint32 minor;
	int gidsLen;
	gid_t *gids, gidOut;

	if (gssd_debug)
		fprintf(stderr, gettext("gsscred_name_to_unix_cred\n"));

	memset(res, 0, sizeof (*res));

	/*
	 * check the request originator
	 */
	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* set the uid from the rpc request */
	uid = argsp->uid;
	set_gssd_uid(uid);

	/*
	 * convert the principal name to gss internal format
	 * need not malloc the input parameters
	 */
	gssBuf.length = argsp->pname.GSS_BUFFER_T_len;
	gssBuf.value = (void*)argsp->pname.GSS_BUFFER_T_val;
	oid.length = argsp->name_type.GSS_OID_len;
	oid.elements = (void*)argsp->name_type.GSS_OID_val;

	res->major = gss_import_name(&minor, &gssBuf, &oid, &gssName);
	if (res->major != GSS_S_COMPLETE)
		return (TRUE);

	/* retrieve the mechanism type from the arguments */
	oid.length = argsp->mech_type.GSS_OID_len;
	oid.elements = (void*) argsp->mech_type.GSS_OID_val;

	/* call the gss extensions to map the principal name to unix creds */
	res->major = gsscred_name_to_unix_cred(gssName, &oid, &uid, &gidOut,
					&gids, &gidsLen);
	gss_release_name(&minor, &gssName);

	if (res->major == GSS_S_COMPLETE) {
		res->uid = uid;
		res->gid = gidOut;
		res->gids.GSSCRED_GIDS_val = gids;
		res->gids.GSSCRED_GIDS_len = gidsLen;
	}

	return (TRUE);
} /* gsscred_name_to_unix_cred_svc_1 */

bool_t
gsscred_expname_to_unix_cred_1_svc(argsp, res, rqstp)
gsscred_expname_to_unix_cred_arg *argsp;
gsscred_expname_to_unix_cred_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	gss_buffer_desc expName = GSS_C_EMPTY_BUFFER;
	int gidsLen;
	gid_t *gids, gidOut;

	if (gssd_debug)
		fprintf(stderr, gettext("gsscred_expname_to_unix_cred\n"));

	memset(res, 0, sizeof (*res));

	/*
	 * check the request originator
	 */
	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* set the uid from the rpc request */
	uid = argsp->uid;
	set_gssd_uid(uid);

	/*
	 * extract the export name from arguments
	 * need not malloc the input parameters
	 */
	expName.length = argsp->expname.GSS_BUFFER_T_len;
	expName.value = (void*)argsp->expname.GSS_BUFFER_T_val;

	res->major = gsscred_expname_to_unix_cred(&expName, &uid,
					&gidOut, &gids, &gidsLen);

	if (res->major == GSS_S_COMPLETE) {
		res->uid = uid;
		res->gid = gidOut;
		res->gids.GSSCRED_GIDS_val = gids;
		res->gids.GSSCRED_GIDS_len = gidsLen;
	}

	return (TRUE);
} /* gsscred_expname_to_unix_cred_1_svc */

bool_t
gss_get_group_info_1_svc(argsp, res, rqstp)
gss_get_group_info_arg *argsp;
gss_get_group_info_res *res;
struct svc_req *rqstp;
{
	uid_t uid;
	int gidsLen;
	gid_t *gids, gidOut;

	if (gssd_debug)
		fprintf(stderr, gettext("gss_get_group_info\n"));

	memset(res, 0, sizeof (*res));

	/*
	 * check the request originator
	 */
	if (checkfrom(rqstp, &uid) == 0)
		return (FALSE);

	/* set the uid from the rpc request */
	uid = argsp->uid;
	set_gssd_uid(uid);

	/*
	 * extract the uid from the arguments
	 */
	uid = argsp->puid;
	res->major = gss_get_group_info(uid, &gidOut, &gids, &gidsLen);
	if (res->major == GSS_S_COMPLETE) {
		res->gid = gidOut;
		res->gids.GSSCRED_GIDS_val = gids;
		res->gids.GSSCRED_GIDS_len = gidsLen;
	}

	return (TRUE);
} /* gss_get_group_info_1_svc */

/*ARGSUSED*/
bool_t
gss_get_kmod_1_svc(argsp, res, rqstp)
	gss_get_kmod_arg *argsp;
	gss_get_kmod_res *res;
	struct svc_req *rqstp;
{
	gss_OID_desc oid;
	char *kmodName;

	if (gssd_debug)
		fprintf(stderr, gettext("gss_get_kmod\n"));

	res->module_follow = FALSE;
	oid.length = argsp->mech_oid.GSS_OID_len;
	oid.elements = (void *)argsp->mech_oid.GSS_OID_val;
	kmodName = __gss_get_kmodName(&oid);

	if (kmodName != NULL) {
		res->module_follow = TRUE;
		res->gss_get_kmod_res_u.modname = kmodName;
	}

	return (TRUE);
}

/*
 *  Returns 1 if caller is ok, else 0.
 *  If caller ok, the uid is returned in uidp.
 */
static int
checkfrom(rqstp, uidp)
struct svc_req *rqstp;
uid_t *uidp;
{
	SVCXPRT *xprt = rqstp->rq_xprt;
	struct authunix_parms *aup;
	uid_t uid;

	/* check client agent uid to ensure it is privileged */
	if (__rpc_get_local_uid(xprt, &uid) < 0) {
		syslog(LOG_ERR, gettext("__rpc_get_local_uid failed %s %s"),
			xprt->xp_netid, xprt->xp_tp);
		goto weakauth;
	}
	if (gssd_debug)
		fprintf(stderr, gettext("checkfrom: local_uid  %d\n"), uid);
	if (uid != 0) {
		syslog(LOG_ERR,
			gettext("checkfrom: caller (uid %d) not privileged"),
			uid);
		goto weakauth;
	}

	/*
	 *  Request came from local privileged process.
	 *  Proceed to get uid of client if needed by caller.
	 */
	if (uidp) {
		if (rqstp->rq_cred.oa_flavor != AUTH_SYS) {
		syslog(LOG_ERR, gettext("checkfrom: not UNIX credentials"));
			goto weakauth;
		}
		/*LINTED*/
		aup = (struct authunix_parms *)rqstp->rq_clntcred;
		*uidp = aup->aup_uid;
		if (gssd_debug)
		fprintf(stderr, gettext("checkfrom: caller's uid %d\n"), *uidp);
	}
	return (1);

	weakauth:
	svcerr_weakauth(xprt);
	return (0);
}
