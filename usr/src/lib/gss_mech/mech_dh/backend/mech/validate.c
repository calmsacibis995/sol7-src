/*
 *	validate.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)validate.c	1.1	97/11/19 SMI"

#include "dh_gssapi.h"

/*ARGSUSED*/
OM_uint32
__dh_validate_context(dh_gss_context_t ctx)
{
	return (DH_SUCCESS);
}

/*ARGSUSED*/
OM_uint32
__dh_validate_cred(dh_cred_id_t cred)
{
	return (DH_SUCCESS);
}

/*ARGSUSED*/
OM_uint32
__dh_validate_principal(dh_principal principal)
{
	return (DH_SUCCESS);
}
