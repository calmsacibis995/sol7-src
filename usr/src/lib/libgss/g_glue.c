/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)g_glue.c	1.14	98/01/22 SMI"

#include <mechglueP.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>

/*
 * This file contains the support routines for the glue layer.
 */

/*
 *  glue routine for get_mech_type
 *
 */
OM_uint32
__gss_get_mech_type(OID, token)
	gss_OID			OID;
	gss_buffer_t		token;
{
	unsigned char * buffer_ptr;
	int length;

	/*
	 * This routine reads the prefix of "token" in order to determine
	 * its mechanism type. It assumes the encoding suggested in
	 * Appendix B of RFC 1508. This format starts out as follows :
	 *
	 * tag for APPLICATION 0, Sequence[constructed, definite length]
	 * length of remainder of token
	 * tag of OBJECT IDENTIFIER
	 * length of mechanism OID
	 * encoding of mechanism OID
	 * <the rest of the token>
	 *
	 * Numerically, this looks like :
	 *
	 * 0x60
	 * <length> - could be multiple bytes
	 * 0x06
	 * <length> - assume only one byte, hence OID length < 127
	 * <mech OID bytes>
	 *
	 * The routine fills in the OID value and returns an error as necessary.
	 */

	if (token == NULL)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Skip past the APP/Sequnce byte and the token length */

	buffer_ptr = (unsigned char *) token->value;

	if (*(buffer_ptr++) != 0x60)
		return (GSS_S_DEFECTIVE_TOKEN);
	length = *buffer_ptr++;
	if (length & 0x80) {
		if ((length & 0x7f) > 4)
			return (GSS_S_DEFECTIVE_TOKEN);
		buffer_ptr += length & 0x7f;
	}

	if (*(buffer_ptr++) != 0x06)
		return (GSS_S_DEFECTIVE_TOKEN);

	OID->length = (OM_uint32) *(buffer_ptr++);
	OID->elements = (void *) buffer_ptr;
	return (GSS_S_COMPLETE);
}


/*
 *  Internal routines to get and release an internal mechanism name
 */
OM_uint32 __gss_import_internal_name(minor_status, mech_type, union_name,
					internal_name)
OM_uint32		*minor_status;
const gss_OID		mech_type;
gss_union_name_t	union_name;
gss_name_t		*internal_name;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_import_name)
			status = mech->gss_import_name(
						mech->context,
						minor_status,
						union_name->external_name,
						union_name->name_type,
						internal_name);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}


OM_uint32 __gss_export_internal_name(minor_status, mech_type,
		internal_name, name_buf)
OM_uint32		*minor_status;
const gss_OID		mech_type;
const gss_name_t	internal_name;
gss_buffer_t		name_buf;
{
	OM_uint32 status;
	gss_mechanism mech;
	gss_buffer_desc dispName;
	gss_OID nameOid;
	unsigned char *buf = NULL;
	int nameLen = 0;
	const unsigned char tokId[] = "\x04\x01";
	const int tokIdLen = 2;
	const int mechOidLenLen = 2, nameOidLenLen = 2, nameLenLen = 4;

	mech = __gss_get_mechanism(mech_type);
	if (!mech)
		return (GSS_S_BAD_MECH);

	if (mech->gss_export_name)
		return (mech->gss_export_name(mech->context,
						minor_status,
						internal_name,
						name_buf));

	/*
	 * if we are here it is because the mechanism does not provide
	 * a gss_export_name so we will use our implementation.  We
	 * do required that the mechanism define a gss_display_name.
	 */
	if (!mech->gss_display_name)
		return (GSS_S_UNAVAILABLE);

	/*
	 * our implementation of gss_export_name puts the
	 * following all in a contiguos buffer:
	 * tok Id + mech oid len + mech oid +
	 * total name len + name oid len + name oid + name.
	 * The name oid and name buffer is obtained by calling
	 * gss_display_name.
	 */
	if ((status = mech->gss_display_name(mech->context,
						minor_status,
						internal_name,
						&dispName,
						&nameOid))
						!= GSS_S_COMPLETE)
		return (status);

	/* determine the size of the buffer needed */
	nameLen = nameOidLenLen + nameOid->length + dispName.length;
	name_buf->length = tokIdLen + mechOidLenLen +
				mech_type->length +
				nameLenLen + nameLen;
	if ((name_buf->value = (void*)malloc(name_buf->length)) ==
		(void*)NULL) {
			name_buf->length = 0;
			gss_release_buffer(&status, &dispName);
			return (GSS_S_FAILURE);
	}

	/* now create the name ..... */
	buf = (unsigned char *)name_buf->value;
	memset(name_buf->value, '\0', name_buf->length);
	memcpy(buf, tokId, tokIdLen);
	buf += tokIdLen;

	/* spec allows only 2 bytes for the mech oid length */
	*buf++ = (mech_type->length & 0xFF00) >> 8;
	*buf++ = (mech_type->length & 0x00FF);
	memcpy(buf, mech_type->elements, mech_type->length);
	buf += mech_type->length;

	/* spec designates the next 4 bytes for the name length */
	*buf++ = (nameLen & 0xFF000000) >> 24;
	*buf++ = (nameLen & 0x00FF0000) >> 16;
	*buf++ = (nameLen & 0x0000FF00) >> 8;
	*buf++ = (nameLen & 0X000000FF);

	/* start our name by putting in the nameOid length  2 bytes only */
	*buf++ = (nameOid->length & 0xFF00) >> 8;
	*buf++ = (nameOid->length & 0x00FF);

	/* now add the nameOid */
	memcpy(buf, nameOid->elements, nameOid->length);
	buf += nameOid->length;

	/* for the final ingredient - add the name from gss_display_name */
	memcpy(buf, dispName.value, dispName.length);

	/* release the buffer obtained from gss_display_name */
	gss_release_buffer(minor_status, &dispName);
	return (GSS_S_COMPLETE);
} /*  __gss_export_internal_name */


OM_uint32 __gss_display_internal_name(minor_status, mech_type, internal_name,
						external_name, name_type)
OM_uint32		*minor_status;
const gss_OID		mech_type;
const gss_name_t	internal_name;
gss_buffer_t		external_name;
gss_OID			*name_type;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_display_name)
			status = mech->gss_display_name(
							mech->context,
							minor_status,
							internal_name,
							external_name,
							name_type);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}

OM_uint32
__gss_release_internal_name(minor_status, mech_type, internal_name)
OM_uint32		*minor_status;
const gss_OID		mech_type;
gss_name_t		*internal_name;
{
	OM_uint32			status;
	gss_mechanism		mech;

	mech = __gss_get_mechanism(mech_type);
	if (mech) {
		if (mech->gss_release_name)
			status = mech->gss_release_name(
							mech->context,
							minor_status,
							internal_name);
		else
			status = GSS_S_UNAVAILABLE;

		return (status);
	}

	return (GSS_S_BAD_MECH);
}


/*
 * This function converts an internal gssapi name to a union gssapi
 * name.  Note that internal_name should be considered "consumed" by
 * this call, whether or not we return an error.
 */
OM_uint32 __gss_convert_name_to_union_name(minor_status, mech,
						internal_name, external_name)
	OM_uint32 *minor_status;
	gss_mechanism		mech;
	gss_name_t		internal_name;
	gss_name_t		*external_name;
{
	OM_uint32 major_status, tmp;
	gss_union_name_t union_name;

	union_name = (gss_union_name_t) malloc(sizeof (gss_union_name_desc));
	if (!union_name) {
			goto allocation_failure;
	}
	union_name->mech_type = 0;
	union_name->mech_name = internal_name;
	union_name->name_type = 0;
	union_name->external_name = 0;

	major_status = generic_gss_copy_oid(minor_status, &mech->mech_type,
						&union_name->mech_type);
	if (major_status != GSS_S_COMPLETE)
		goto allocation_failure;

	union_name->external_name =
		(gss_buffer_t) malloc(sizeof (gss_buffer_desc));
	if (!union_name->external_name) {
			goto allocation_failure;
	}

	major_status = mech->gss_display_name(mech->context, minor_status,
						internal_name,
						union_name->external_name,
						&union_name->name_type);
	if (major_status != GSS_S_COMPLETE)
		goto allocation_failure;

	*external_name =  union_name;
	return (GSS_S_COMPLETE);

allocation_failure:
	if (union_name) {
		if (union_name->external_name) {
			if (union_name->external_name->value)
				free(union_name->external_name->value);
			free(union_name->external_name);
		}
		if (union_name->name_type)
			gss_release_oid(&tmp, &union_name->name_type);
		if (union_name->mech_type)
			gss_release_oid(&tmp, &union_name->mech_type);
		free(union_name);
	}
	/*
	 * do as the top comment says - since we are now owners of
	 * internal_name, we must clean it up
	 */
	if (internal_name)
		__gss_release_internal_name(&tmp, &mech->mech_type,
						&internal_name);

	return (major_status);
}

/*
 * Glue routine for returning the mechanism-specific credential from a
 * external union credential.
 */
gss_cred_id_t
__gss_get_mechanism_cred(union_cred, mech_type)
	const gss_union_cred_t	union_cred;
	const gss_OID		mech_type;
{
	int			i;

	if (union_cred == GSS_C_NO_CREDENTIAL)
		return (GSS_C_NO_CREDENTIAL);

	for (i = 0; i < union_cred->count; i++) {
		if (g_OID_equal(mech_type, &union_cred->mechs_array[i]))
			return (union_cred->cred_array[i]);
	}
	return (GSS_C_NO_CREDENTIAL);
}


/*
 * Routine to create and copy the gss_buffer_desc structure.
 * Both space for the structure and the data is allocated.
 */
OM_uint32
__gss_create_copy_buffer(srcBuf, destBuf, addNullChar)
	const gss_buffer_t	srcBuf;
	gss_buffer_t *		destBuf;
	int			addNullChar;
{
	gss_buffer_t aBuf;
	int len;

	if (destBuf == NULL)
		return (GSS_S_CALL_INACCESSIBLE_WRITE);

	*destBuf = 0;

	aBuf = (gss_buffer_t)malloc(sizeof (gss_buffer_desc));
	if (!aBuf)
		return (GSS_S_FAILURE);

	if (addNullChar)
		len = srcBuf->length + 1;
	else
		len = srcBuf->length;

	if (!(aBuf->value = (void*)malloc(len))) {
		free(aBuf);
		return (GSS_S_FAILURE);
	}


	memcpy(aBuf->value, srcBuf->value, srcBuf->length);
	aBuf->length = srcBuf->length;
	*destBuf = aBuf;

	/* optionally add a NULL character */
	if (addNullChar)
		((char *)aBuf->value)[aBuf->length] = '\0';

	return (GSS_S_COMPLETE);
} /* ****** __gss_create_copy_buffer  ****** */
