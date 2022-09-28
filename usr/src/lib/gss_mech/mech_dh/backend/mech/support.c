/*
 *	support.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)support.c	1.1	97/11/19 SMI"

#include "dh_gssapi.h"

OM_uint32
/*ARGSUSED*/
__dh_gss_display_status(void *ctx, OM_uint32 *minor, OM_uint32 status_value,
    int status_type, gss_OID mech, OM_uint32* mesg_ctx,
    gss_buffer_t  status_str)
{
	return (GSS_S_UNAVAILABLE);
}


OM_uint32
/*ARGSUSED*/
__dh_gss_indicate_mechs(void *ctx, OM_uint32 *minor, gss_OID_set *mechs)
{
	return (GSS_S_UNAVAILABLE);
}
