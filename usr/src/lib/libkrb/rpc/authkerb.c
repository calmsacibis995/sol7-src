#ident	"@(#)authkerb_prot.c	1.4	97/11/25 SMI"

/*
 * authkerbprt.c, XDR routines for Kerberos authentication
 *
 * Copyright (C) 1986-1997, Sun Microsystems, Inc.
 * All Rights reserved.
 *
 */

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_kerb.h>

#define	ATTEMPT(xdr_op) if (!(xdr_op)) return (FALSE)

bool_t
xdr_authkerb_cred(xdrs, cred)
	XDR *xdrs;
	struct authkerb_cred *cred;
{
	char *tktp;		/* for xdr_bytes */

	/*
	 * Unrolled xdr
	 */
	ATTEMPT(xdr_enum(xdrs, (enum_t *)&cred->akc_namekind));
	switch (cred->akc_namekind) {
	case AKN_FULLNAME:
		tktp = (char *)cred->akc_fullname.ticket.dat;
		ATTEMPT(xdr_bytes(xdrs, &tktp,
				(u_int *)&cred->akc_fullname.ticket.length,
				MAX_KTXT_LEN));
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->akc_fullname.window,
				sizeof (cred->akc_fullname.window)));
		return (TRUE);
	case AKN_NICKNAME:
		ATTEMPT(xdr_opaque(xdrs, (caddr_t)&cred->akc_nickname,
				sizeof (cred->akc_nickname)));
		return (TRUE);
	default:
		return (FALSE);
	}
}


bool_t
xdr_authkerb_verf(xdrs, verf)
	register XDR *xdrs;
	register struct authkerb_verf *verf;
{
	/*
	 * Unrolled xdr
	 */
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->akv_xtimestamp,
			sizeof (des_block)));
	ATTEMPT(xdr_opaque(xdrs, (caddr_t)&verf->akv_int_u,
			sizeof (verf->akv_int_u)));
	return (TRUE);
}
