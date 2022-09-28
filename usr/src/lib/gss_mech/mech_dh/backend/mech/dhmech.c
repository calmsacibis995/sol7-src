/*
 *	dhmec.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dhmech.c	1.2	98/01/20 SMI"

#include "dh_gssapi.h"
#include <stdlib.h>

static struct gss_config dh_mechanism = {
	{0, 0},				/* OID for mech type. */
	0,
	__dh_gss_acquire_cred,
	__dh_gss_release_cred,
	__dh_gss_init_sec_context,
	__dh_gss_accept_sec_context,
	__dh_gss_process_context_token,
	__dh_gss_delete_sec_context,
	__dh_gss_context_time,
	__dh_gss_display_status,
	__dh_gss_indicate_mechs,
	__dh_gss_compare_name,
	__dh_gss_display_name,
	__dh_gss_import_name,
	__dh_gss_release_name,
	__dh_gss_inquire_cred,
	__dh_gss_add_cred,
	__dh_gss_export_sec_context,
	__dh_gss_import_sec_context,
	__dh_gss_inquire_cred_by_mech,
	__dh_gss_inquire_names_for_mech,
	__dh_gss_inquire_context,
	__dh_gss_internal_release_oid,
	__dh_gss_wrap_size_limit,
	NULL,
	__dh_gss_export_name,
	__dh_gss_sign,
	__dh_gss_verify,
};

gss_mechanism
__dh_gss_initialize(gss_mechanism mech)
{
	if (mech->context != NULL)
		return (mech);    /* already initialized */

	*mech = dh_mechanism;

	mech->context = malloc(sizeof (dh_context_desc));
	if (mech->context == NULL)
		return (NULL);

	memset(mech->context, 0, sizeof (dh_context_desc));

	return (mech);
}
