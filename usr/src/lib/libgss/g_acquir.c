/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)g_acquire_cred.c	1.16	98/01/22 SMI"

/*
 *  glue routine for gss_acquire_cred
 */

#include <mechglueP.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>
#include <errno.h>
#include <time.h>
/* local functions */
static gss_OID_set create_actual_mechs(const gss_OID, int);

static gss_OID_set
create_actual_mechs(mechs_array, count)
	const gss_OID	mechs_array;
	int count;
{
	gss_OID_set 	actual_mechs;
	int		i;
	OM_uint32	minor;

	actual_mechs = (gss_OID_set) malloc(sizeof (gss_OID_set_desc));
	if (!actual_mechs)
		return (NULL);

	actual_mechs->elements = (gss_OID)
		malloc(sizeof (gss_OID_desc) * count);
	if (!actual_mechs->elements) {
		free(actual_mechs);
		return (NULL);
	}

	actual_mechs->count = 0;

	for (i = 0; i < count; i++) {
		actual_mechs->elements[i].elements = (void *)
			malloc(mechs_array[i].length);
		if (actual_mechs->elements[i].elements == NULL) {
			gss_release_oid_set(&minor, &actual_mechs);
			return (NULL);
		}
		g_OID_copy(&actual_mechs->elements[i], &mechs_array[i]);
		actual_mechs->count++;
	}

	return (actual_mechs);
}


OM_uint32
gss_acquire_cred(minor_status,
			desired_name,
			time_req,
			desired_mechs,
			cred_usage,
			output_cred_handle,
			actual_mechs,
			time_rec)

OM_uint32 *		minor_status;
const gss_name_t	desired_name;
OM_uint32		time_req;
const gss_OID_set	desired_mechs;
int			cred_usage;
gss_cred_id_t *		output_cred_handle;
gss_OID_set *		actual_mechs;
OM_uint32 *		time_rec;

{
	OM_uint32 major, initTimeOut, acceptTimeOut,
		outTime = GSS_C_INDEFINITE;
	gss_OID_set_desc default_OID_set;
	gss_OID_set mechs;
	gss_OID_desc default_OID;
	gss_mechanism mech;
	int i;
	gss_union_cred_t creds;

	/* start by checking parameters */
	if (!minor_status)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (!output_cred_handle)
		return (GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CRED);

	*output_cred_handle = GSS_C_NO_CREDENTIAL;

	/* Set output parameters to NULL for now */
	if (actual_mechs)
		*actual_mechs = GSS_C_NULL_OID_SET;

	if (time_rec)
		*time_rec = 0;

	/*
	 * if desired_mechs equals GSS_C_NULL_OID_SET, then pick an
	 * appropriate default.  We use the first mechanism in the
	 * mechansim list as the default. This set is created with
	 * statics thus needs not be freed
	 */
	if (desired_mechs == GSS_C_NULL_OID_SET) {
		mech = __gss_get_mechanism(NULL);
		if (mech == NULL)
			return (GSS_S_BAD_MECH);

		mechs = &default_OID_set;
		default_OID_set.count = 1;
		default_OID_set.elements = &default_OID;
		default_OID.length = mech->mech_type.length;
		default_OID.elements = mech->mech_type.elements;
	} else
		mechs = desired_mechs;

	/* allocate the output credential structure */
	creds = (gss_union_cred_t) malloc(sizeof (gss_union_cred_desc));
	if (creds == NULL)
		return (GSS_S_FAILURE);

	/* initialize to 0s */
	memset(creds, 0, sizeof (gss_union_cred_desc));

	/* for each requested mech attempt to obtain a credential */
	for (i = 0; i < mechs->count; i++) {
		major = gss_add_cred(minor_status, creds, desired_name,
				&mechs->elements[i],
				cred_usage, time_req, time_req, NULL,
				NULL, &initTimeOut, &acceptTimeOut);
		if (major == GSS_S_COMPLETE) {
			/* update the credential's time */
			if (cred_usage == GSS_C_ACCEPT) {
				if (outTime > acceptTimeOut)
					outTime = acceptTimeOut;
			} else if (cred_usage == GSS_C_INITIATE) {
				if (outTime > initTimeOut)
					outTime = initTimeOut;
			} else {
				/*
				 * time_rec is the lesser of the
				 * init/accept times
				 */
				if (initTimeOut > acceptTimeOut)
					outTime = (outTime > acceptTimeOut) ?
						acceptTimeOut : outTime;
				else
					outTime = (outTime > initTimeOut) ?
						initTimeOut : outTime;
			}
		}
	} /* for */

	/* ensure that we have at least one credential element */
	if (creds->count < 1) {
		free(creds);
		return (major);
	}

	/*
	 * fill in output parameters
	 * setup the actual mechs output parameter
	 */
	if (actual_mechs != NULL) {
		if ((*actual_mechs = create_actual_mechs(creds->mechs_array,
					creds->count)) == NULL) {
			gss_release_cred(minor_status,
				(gss_cred_id_t *)&creds);
			*minor_status = 0;
			return (GSS_S_FAILURE);
		}
	}

	if (time_rec)
		*time_rec = outTime;


	*output_cred_handle = (gss_cred_id_t) creds;
	return (GSS_S_COMPLETE);
}

/* V2 INTERFACE */
OM_uint32
gss_add_cred(minor_status, input_cred_handle,
			desired_name, desired_mech, cred_usage,
			initiator_time_req, acceptor_time_req,
			output_cred_handle, actual_mechs,
			initiator_time_rec, acceptor_time_rec)
	OM_uint32		*minor_status;
	const gss_cred_id_t	input_cred_handle;
	const gss_name_t	desired_name;
	const gss_OID		desired_mech;
	gss_cred_usage_t	cred_usage;
	OM_uint32		initiator_time_req;
	OM_uint32		acceptor_time_req;
	gss_cred_id_t		*output_cred_handle;
	gss_OID_set		*actual_mechs;
	OM_uint32		*initiator_time_rec;
	OM_uint32		*acceptor_time_rec;
{
	OM_uint32		status, time_req, time_rec, temp_minor_status;
	gss_mechanism 		mech;
	gss_union_name_t	union_name = NULL;
	gss_union_cred_t	union_cred, new_union_cred;
	gss_name_t		internal_name = GSS_C_NO_NAME;
	gss_cred_id_t		cred = NULL;
	gss_OID			new_mechs_array = NULL;
	gss_cred_id_t		*new_cred_array = NULL;

	/* check input parameters */
	if (minor_status == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);
	*minor_status = 0;

	if (input_cred_handle == GSS_C_NO_CREDENTIAL &&
		output_cred_handle == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE | GSS_S_NO_CRED);

	if (output_cred_handle)
		*output_cred_handle = GSS_C_NO_CREDENTIAL;

	if (actual_mechs)
		*actual_mechs = NULL;

	if (acceptor_time_rec)
		*acceptor_time_rec = 0;

	if (initiator_time_rec)
		*initiator_time_rec = 0;

	mech = __gss_get_mechanism(desired_mech);
	if (!mech)
		return (GSS_S_BAD_MECH);
	else if (!mech->gss_acquire_cred)
		return (GSS_S_UNAVAILABLE);

	if (input_cred_handle == GSS_C_NO_CREDENTIAL) {
		union_cred = malloc(sizeof (gss_union_cred_desc));
		if (union_cred == NULL)
			return (GSS_S_FAILURE);

		memset(union_cred, 0, sizeof (gss_union_cred_desc));

		/* for default credentials we will use GSS_C_NO_NAME */
		internal_name = GSS_C_NO_NAME;
	} else {
		union_cred = (gss_union_cred_t) input_cred_handle;
		if (__gss_get_mechanism_cred(union_cred, desired_mech) !=
			GSS_C_NO_CREDENTIAL)
			return (GSS_S_DUPLICATE_ELEMENT);

		/* may need to create a mechanism specific name */
		if (desired_name) {
			union_name = (gss_union_name_t)desired_name;
			if (union_name->mech_type &&
				g_OID_equal(union_name->mech_type,
						&mech->mech_type))
				internal_name = union_name->mech_name;
			else {
				if (__gss_import_internal_name(minor_status,
					&mech->mech_type, union_name,
					&internal_name) != GSS_S_COMPLETE)
					return (GSS_S_BAD_NAME);
			}
		}
	}


	if (cred_usage == GSS_C_ACCEPT)
		time_req = acceptor_time_req;
	else if (cred_usage == GSS_C_INITIATE)
		time_req = initiator_time_req;
	else if (cred_usage == GSS_C_BOTH)
		time_req = (acceptor_time_req > initiator_time_req) ?
			acceptor_time_req : initiator_time_req;

	status = mech->gss_acquire_cred(mech->context, minor_status,
				internal_name, time_req,
				GSS_C_NULL_OID_SET, cred_usage,
				&cred, NULL, &time_rec);

	if (status != GSS_S_COMPLETE)
		goto errout;

	/* may need to set credential auxinfo strucutre */
	if (union_cred->auxinfo.creation_time == 0) {
		union_cred->auxinfo.creation_time = time(NULL);
		union_cred->auxinfo.time_rec = time_rec;
		union_cred->auxinfo.cred_usage = cred_usage;

		/*
		 * we must set the name; if name is not supplied
		 * we must do inquire cred to get it
		 */
		if (internal_name == NULL) {
			if (mech->gss_inquire_cred == NULL ||
				((status = mech->gss_inquire_cred(
						mech->context,
						&temp_minor_status, cred,
						&internal_name, NULL, NULL,
						NULL)) != GSS_S_COMPLETE))
				goto errout;
		}

		if ((status = mech->gss_display_name(mech->context,
				&temp_minor_status, internal_name,
				&union_cred->auxinfo.name,
				&union_cred->auxinfo.name_type)) !=
			GSS_S_COMPLETE)
			goto errout;
	}

	/* now add the new credential elements */
	new_mechs_array = (gss_OID)
		malloc(sizeof (gss_OID_desc) * (union_cred->count+1));

	new_cred_array = (gss_cred_id_t *)
		malloc(sizeof (gss_cred_id_t) * (union_cred->count+1));

	if (!new_mechs_array || !new_cred_array) {
		status = GSS_S_FAILURE;
		goto errout;
	}

	if (acceptor_time_rec)
		if (cred_usage == GSS_C_ACCEPT || cred_usage == GSS_C_BOTH)
			*acceptor_time_rec = time_rec;
	if (initiator_time_rec)
		if (cred_usage == GSS_C_INITIATE || cred_usage == GSS_C_BOTH)
			*initiator_time_rec = time_rec;

	/*
	 * OK, expand the mechanism array and the credential array
	 */
	memcpy(new_mechs_array, union_cred->mechs_array,
		sizeof (gss_OID_desc) * union_cred->count);
	memcpy(new_cred_array, union_cred->cred_array,
		sizeof (gss_cred_id_t) * union_cred->count);

	new_cred_array[union_cred->count] = cred;
	if ((new_mechs_array[union_cred->count].elements =
			malloc(mech->mech_type.length)) == NULL)
		goto errout;

	g_OID_copy(&new_mechs_array[union_cred->count],
			&mech->mech_type);

	if (actual_mechs) {
		*actual_mechs = create_actual_mechs(new_mechs_array,
					union_cred->count + 1);
		if (*actual_mechs == NULL) {
			free(new_mechs_array[union_cred->count].elements);
			goto errout;
		}
	}

	if (output_cred_handle == NULL) {
		free(union_cred->mechs_array);
		free(union_cred->cred_array);
		new_union_cred = union_cred;
	} else {
		new_union_cred = malloc(sizeof (gss_union_cred_desc));
		if (new_union_cred == NULL) {
			free(new_mechs_array[union_cred->count].elements);
			goto errout;
		}
		*new_union_cred = *union_cred;
		*output_cred_handle = new_union_cred;
	}

	new_union_cred->mechs_array = new_mechs_array;
	new_union_cred->cred_array = new_cred_array;
	new_union_cred->count++;

	/* may need to delete the name */
	if (internal_name && (union_name->mech_type == NULL ||
				internal_name != union_name->mech_name))
		__gss_release_internal_name(&temp_minor_status,
					&mech->mech_type,
					&internal_name);

	return (GSS_S_COMPLETE);

errout:
	if (new_mechs_array)
		free(new_mechs_array);
	if (new_cred_array)
		free(new_cred_array);

	if (cred != NULL && mech->gss_release_cred)
		mech->gss_release_cred(mech->context,
				&temp_minor_status, &cred);

	if (internal_name && (union_name->mech_type == NULL ||
				internal_name != union_name->mech_name))
		__gss_release_internal_name(&temp_minor_status,
					&mech->mech_type,
					&internal_name);

	if (input_cred_handle == GSS_C_NO_CREDENTIAL && union_cred) {
		if (union_cred->auxinfo.name_type) {
			if (union_cred->auxinfo.name_type->elements)
				free(union_cred->auxinfo.name_type->elements);
			free(union_cred->auxinfo.name_type);
		}
		if (union_cred->auxinfo.name.value)
			free(union_cred->auxinfo.name.value);
		free(union_cred);
	}

	return (status);
}
