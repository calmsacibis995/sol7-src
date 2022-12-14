/*
 * Copyright (c) 1994-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *  From  common/syscall/systeminfo.c
 */

#ident	"@(#)sec_gen.c	1.9	97/04/29 SMI"

#include <sys/types.h>
#include <sys/systeminfo.h>	/* for SI_KERB stuff */
#include <kerberos/krb.h>

#include <sys/errno.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/auth_kerb.h>
#include <rpc/kerb_private.h>
#include <rpc/svc_auth.h>


/*
 * authany_wrap() is a NO-OP routines for ah_wrap().
 */
/* ARGSUSED */
int
authany_wrap(AUTH *auth, caddr_t buf, u_int buflen,
    XDR *xdrs, xdrproc_t xfunc, caddr_t xwhere)
{
	return (*xfunc)(xdrs, xwhere);
}

/*
 * authany_unwrap() is a NO-OP routines for ah_unwrap().
 */
/* ARGSUSED */
int
authany_unwrap(AUTH *auth, XDR *xdrs, xdrproc_t xfunc, caddr_t xwhere)
{
	return (*xfunc)(xdrs, xwhere);
}
