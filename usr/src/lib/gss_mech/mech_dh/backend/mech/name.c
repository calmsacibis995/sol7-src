/*
 *	name.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)name.c	1.1	97/11/19 SMI"

#include "dh_gssapi.h"
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/note.h>
#include <thread.h>

static gss_OID_desc __DH_GSS_C_NT_NETNAME_desc =
	{ 9,  "\053\006\004\001\052\002\032\001\001" };

const gss_OID_desc * const __DH_GSS_C_NT_NETNAME = &__DH_GSS_C_NT_NETNAME_desc;

#define	OID_MAX_NAME_ENTRIES  32

OM_uint32
__dh_gss_compare_name(void *ctx, OM_uint32 *minor, gss_name_t name1,
    gss_name_t name2, int *equal)
{
_NOTE(ARGUNUSED(ctx))
	*minor = 0;

	if (name1 == 0 || name2 == 0) {
		*minor = DH_BADARG_FAILURE;
		return (GSS_S_BAD_NAME);
	}

	*equal = strcmp(name1, name2);

	return (GSS_S_COMPLETE);
}

OM_uint32
__dh_gss_display_name(void * ctx, OM_uint32* minor, gss_name_t name,
    gss_buffer_t output, gss_OID *name_type)
{
_NOTE(ARGUNUSED(ctx))
	*minor = 0;

	output->length = 0;
	output->value = (void *)strdup(name);
	if (output->value == NULL) {
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}

	if ((*minor = __OID_copy(name_type, __DH_GSS_C_NT_NETNAME))
	    != DH_SUCCESS) {
		free(output->value);
		output->value = NULL;
		return (GSS_S_FAILURE);
	}
	output->length = strlen(name) + 1;

	return (GSS_S_COMPLETE);
}

static OM_uint32
do_netname_nametype(OM_uint32 *minor, char *input, gss_name_t *output)
{
	if (__dh_validate_principal(input) != DH_SUCCESS)
		return (GSS_S_BAD_NAME);

	*minor = 0;
	*output = strdup((char *)input);
	if (*output == NULL) {
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}

	return (GSS_S_COMPLETE);
}

static OM_uint32
do_uid_nametype(OM_uint32 *minor, uid_t uid, gss_name_t *output)
{
	char netname[MAXNETNAMELEN+1];

	*minor = 0;
	if (!user2netname(netname, uid, NULL)) {
		*minor = DH_NETNAME_FAILURE;
		return (GSS_S_FAILURE);
	}
	return (do_netname_nametype(minor, (gss_name_t)netname, output));
}

static OM_uint32
do_username_nametype(OM_uint32 *minor, char *uname, gss_name_t *output)
{
	struct passwd pwd;
	char buff[1024];

	*minor = 0;
	if (getpwnam_r(uname, &pwd, buff, sizeof (buff)) == NULL) {
		*minor = DH_NO_SUCH_USER;
		return (GSS_S_FAILURE);
	}

	return (do_uid_nametype(minor, pwd.pw_uid, output));
}

static OM_uint32
do_hostbase_nametype(OM_uint32 *minor, char * input, gss_name_t *output)
{
	char *host = strchr(input, '@');
	char netname[MAXNETNAMELEN+1];

	*minor = 0;
	if (host == NULL)
		return (GSS_S_BAD_NAME);
	host += 1;

	if (!host2netname(netname, host, NULL)) {
		*minor = DH_NETNAME_FAILURE;
		return (GSS_S_FAILURE);
	}
	return (do_netname_nametype(minor, netname, output));
}

static OM_uint32
do_exported_netname(dh_context_t ctx, OM_uint32 *minor,
    gss_buffer_t input, gss_name_t *output)
{
	const char tokid[] = "\x04\x01";
	const int tokid_len = 2;
	const int OIDlen_len = 2;
	const int namelen_len = 4;
	unsigned char *p = (unsigned char *)input->value;
	OM_uint32 len = input->length;
	OM_uint32 oidlen;
	OM_uint32 namelen;

	*minor = DH_BADARG_FAILURE;

	/* The len must be at least this big */
	if (len < tokid_len + OIDlen_len + namelen_len)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Export names must start with the token id of 0x04 0x01 */
	if (memcmp(p, tokid, tokid_len) != 0)
		return (GSS_S_DEFECTIVE_TOKEN);
	p += tokid_len;

	/* Decode the Mechanism oid */
	oidlen = (*p++ << 8) & 0xff00;
	oidlen |= *p++ & 0xff;

	/* Check that we actually have the mechanism oid elements */
	if (len < tokid_len + OIDlen_len + oidlen + namelen_len)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Compare that the input is for this mechanism */
	if (oidlen != ctx->mech->length)
		return (GSS_S_DEFECTIVE_TOKEN);

	if (memcmp(p, ctx->mech->elements, oidlen) != 0)
		return (GSS_S_DEFECTIVE_TOKEN);
	p += oidlen;

	/* Grab the length of the mechanism specific name per RFC 2078 */
	namelen = (*p++ << 24) & 0xff000000;
	namelen |= (*p++ << 16) & 0xff0000;
	namelen |= (*p++ << 8) & 0xff00;
	namelen |= *p++ & 0xff;

	/* This should alway be false */
	if (len < tokid_len + OIDlen_len + oidlen + namelen_len + namelen)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Make sure the bytes for the netname oid length are available */
	if (namelen < OIDlen_len)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Get the netname oid length */
	oidlen = (*p++ << 8) & 0xff00;
	oidlen = *p++ & 0xff;

	/* See if we have the elements of the netname oid */
	if (namelen < OIDlen_len + oidlen)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Check that the oid is really a netname */
	if (oidlen != __DH_GSS_C_NT_NETNAME->length)
		return (GSS_S_DEFECTIVE_TOKEN);
	if (memcmp(p, __DH_GSS_C_NT_NETNAME->elements,
	    __DH_GSS_C_NT_NETNAME->length) != 0)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* p now points to the netname wich is null terminated */
	p += oidlen;

	/*
	 * How the netname is encoded in an export name type for
	 * this mechanism. See _dh_gss_export_name below.
	 */

	if (namelen != OIDlen_len + oidlen + strlen((char *)p) + 1)
		return (GSS_S_DEFECTIVE_TOKEN);

	/* Grab the netname */
	*output = strdup((char *)p);
	if (*output) {
		*minor = 0;
		return (GSS_S_COMPLETE);
	}

	*minor = DH_NOMEM_FAILURE;
	return (GSS_S_FAILURE);
}

OM_uint32
__dh_gss_import_name(void *ctx, OM_uint32 *minor, gss_buffer_t input,
    gss_OID name_type, gss_name_t *output)
{
	char *name;
	OM_uint32 stat;

	*minor = 0;
	*output = NULL;

	if (input == NULL || input->value == NULL)
		return (GSS_S_BAD_NAME);
	if (name_type == GSS_C_NO_OID)
		return (GSS_S_BAD_NAMETYPE);

	if (__OID_equal(name_type, GSS_C_NT_MACHINE_UID_NAME)) {
		uid_t uid;
		if (input->length != sizeof (uid_t))
			return (GSS_S_BAD_NAME);
		uid = *(uid_t *)input->value;
		return (do_uid_nametype(minor, uid, output));
	} else if (__OID_equal(name_type, GSS_C_NT_EXPORT_NAME)) {
		stat = do_exported_netname((dh_context_t)ctx, minor,
		    input, output);
		return (stat);
	}


	name = malloc(input->length+1);		/* Null terminate name */
	if (name == NULL) {
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}
	memcpy(name, input->value, input->length);
	name[input->length] = '\0';

	if (__OID_equal(name_type, __DH_GSS_C_NT_NETNAME)) {
		stat = do_netname_nametype(minor, name, output);
		free(name);
		return (stat);
	} else if (__OID_equal(name_type, GSS_C_NT_HOSTBASED_SERVICE)) {
		stat = do_hostbase_nametype(minor, name, output);
		free(name);
		return (stat);
	} else if (__OID_equal(name_type, GSS_C_NT_USER_NAME)) {
		stat = do_username_nametype(minor, name, output);
		free(name);
		return (stat);
	} else if (__OID_equal(name_type, GSS_C_NT_STRING_UID_NAME)) {
		char *p;
		uid_t uid = (uid_t)strtol(name, &p, 0);
		free(name);
		if (*p != '\0')
			return (GSS_S_BAD_NAME);
		return (do_uid_nametype(minor, uid, output));
	} else
		return (GSS_S_BAD_NAMETYPE);
}

OM_uint32
__dh_gss_release_name(void *ctx, OM_uint32 *minor, gss_name_t *name)
{
_NOTE(ARGUNUSED(ctx))
	*minor = 0;
	free(*name);
	*name = NULL;

	return (GSS_S_COMPLETE);
}

static mutex_t name_tab_lock = DEFAULTMUTEX;
static const gss_OID_desc * oid_name_tab[OID_MAX_NAME_ENTRIES];

OM_uint32
__dh_gss_inquire_names_for_mech(void *ctx, OM_uint32 *minor,
    gss_OID mech, gss_OID_set *names)
{
_NOTE(ARGUNUSED(ctx,mech))
	if (oid_name_tab[0] == 0) {
		mutex_lock(&name_tab_lock);
		if (oid_name_tab[0] == 0) {
			oid_name_tab[0] = __DH_GSS_C_NT_NETNAME;
			oid_name_tab[1] = GSS_C_NT_HOSTBASED_SERVICE;
			oid_name_tab[2] = GSS_C_NT_USER_NAME;
			oid_name_tab[3] = GSS_C_NT_MACHINE_UID_NAME;
			oid_name_tab[4] = GSS_C_NT_STRING_UID_NAME;
			oid_name_tab[5] = GSS_C_NT_EXPORT_NAME;
			/* oid_name_tab[6] = GSS_C_NT_ANONYMOUS_NAME; */
		}
		mutex_unlock(&name_tab_lock);
	}

	if ((*minor = __OID_copy_set_from_array(names,
	    oid_name_tab, 6)) != DH_SUCCESS)
		return (GSS_S_FAILURE);

	return (GSS_S_COMPLETE);
}


OM_uint32
__dh_pname_to_uid(void *ctx, OM_uint32 *minor,
    const gss_name_t pname, uid_t *uid)
{
_NOTE(ARGUNUSED(ctx))
	gid_t gid;
	gid_t glist[NGRPS];
	int glen;
	char *netname = (char *)pname;
	char host_netname[MAXNETNAMELEN+1];

	*minor = 0;
	*uid = UID_NOBODY;

	if (netname2user(netname, uid, &gid, &glen, glist))
		return (GSS_S_COMPLETE);
	else if (host2netname(host_netname, NULL, NULL)) {
		if (strncmp(netname, host_netname, MAXNETNAMELEN) == 0)
			*uid = 0;
		return (GSS_S_COMPLETE);
	}

	*minor = DH_NETNAME_FAILURE;
	return (GSS_S_FAILURE);
}

OM_uint32
__dh_gss_export_name(void *ctx, OM_uint32 *minor,
    const gss_name_t input_name, gss_buffer_t exported_name)
{
	dh_principal pname = (dh_principal)input_name;
	dh_context_t dc = (dh_context_t)ctx;
	const char tokid[] = "\x04\x01";
	const int tokid_len = 2;
	const int OIDlen_len = 2; /* Why did they do this? */
	const int namelen_len = 4;
	char *p;
	OM_uint32 len;
	OM_uint32 namelen;

	*minor = 0;
	exported_name->length = 0;
	exported_name->value = NULL;

	namelen = OIDlen_len + __DH_GSS_C_NT_NETNAME->length
	    + strlen(pname)+1;
	len = tokid_len + OIDlen_len + dc->mech->length
	    + namelen_len + namelen;

	p = New(char, len);
	if (p == NULL) {
		*minor = DH_NOMEM_FAILURE;
		return (GSS_S_FAILURE);
	}
	exported_name->length = len;
	exported_name->value = p;

	memcpy(p, tokid, tokid_len);
	p += tokid_len;

	/*
	 * The spec only allows two bytes for the oid length.
	 * We are assuming here that the correct encodeing is MSB first as
	 * was done in libgss.
	 */

	*p++ = (dc->mech->length & 0xff00) >> 8;
	*p++ = (dc->mech->length & 0x00ff);

	/* Now the mechanism OID elements */
	memcpy(p, dc->mech->elements, dc->mech->length);
	p += dc->mech->length;

	/* The name length most MSB first */
	*p++ = (namelen & 0xff000000) >> 24;
	*p++ = (namelen & 0x00ff0000) >> 16;
	*p++ = (namelen & 0x0000ff00) >> 8;
	*p++ = (namelen & 0x000000ff);

	/*
	 * We'll now encode the netname oid. Again we'll just use 2 bytes.
	 * This is the same encoding that the libgss implementor uses, so
	 * we'll just follow along.
	 */

	*p++ = (__DH_GSS_C_NT_NETNAME->length & 0xff00) >> 8;
	*p++ = (__DH_GSS_C_NT_NETNAME->length &0x00ff);

	/* The netname oid values */
	memcpy(p, __DH_GSS_C_NT_NETNAME->elements,
	    __DH_GSS_C_NT_NETNAME->length);

	p += __DH_GSS_C_NT_NETNAME->length;

	/* Now we copy the netname including the null byte to be safe */
	memcpy(p, pname, strlen(pname) + 1);

	return (GSS_S_COMPLETE);
}

static int
release_oid(const gss_OID_desc * const ref, gss_OID *oid)
{
	gss_OID id = *oid;

	if (id == ref)
		return (TRUE);
	if (id->elements == ref->elements) {
		Free(id);
		*oid = GSS_C_NO_OID;
		return (TRUE);
	}

	return (FALSE);
}

OM_uint32
__dh_gss_internal_release_oid(void *ctx, OM_uint32 *minor, gss_OID *oid)
{
	dh_context_t dhcxt = (dh_context_t)ctx;
	*minor = 0;

	if (oid == NULL || *oid == NULL)
		return (GSS_S_COMPLETE);

	if (release_oid(dhcxt->mech, oid))
		return (GSS_S_COMPLETE);

	if (release_oid(__DH_GSS_C_NT_NETNAME, oid))
		return (GSS_S_COMPLETE);

	return (GSS_S_FAILURE);
}
