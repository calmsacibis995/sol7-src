#ident	"@(#)get_pw_tkt.c	1.2	97/11/25 SMI"
/*
 * Copyright (c) 1986-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/get_pw_tkt.c,v $
 * $Author: steiner $
 *
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_get_pw_tkt_c =
"$Header: get_pw_tkt.c,v 4.6 89/01/13 18:19:11 steiner Exp $";
#endif /* lint */


#include <kerberos/mit-copyright.h>
#include <kerberos/krb.h>

/*
 * Get a ticket for the password-changing server ("changepw.KRB_MASTER").
 *
 * Given the name, instance, realm, and current password of the
 * principal for which the user wants a password-changing-ticket,
 * return either:
 *
 *	GT_PW_BADPW if current password was wrong,
 *	GT_PW_NULL  if principal had a NULL password,
 *	or the result of the krb_get_pw_in_tkt() call.
 *
 * First, try to get a ticket for "user.instance@realm" to use the
 * "changepw.KRB_MASTER" server (KRB_MASTER is defined in "krb.h").
 * The requested lifetime for the ticket is "1", and the current
 * password is the "cpw" argument given.
 *
 * If the password was bad, give up.
 *
 * If the principal had a NULL password in the Kerberos database
 * (indicating that the principal is known to Kerberos, but hasn't
 * got a password yet), try instead to get a ticket for the principal
 * "default.changepw@realm" to use the "changepw.KRB_MASTER" server.
 * Use the password "changepwkrb" instead of "cpw".  Return GT_PW_NULL
 * if all goes well, otherwise the error.
 *
 * If this routine succeeds, a ticket and session key for either the
 * principal "user.instance@realm" or "default.changepw@realm" to use
 * the password-changing server will be in the user's ticket file.
 */

get_pw_tkt(user,instance,realm,cpw)
    char *user;
    char *instance;
    char *realm;
    char *cpw;
{
    int kerror;
    extern krb_get_pw_in_tkt();

    kerror = krb_get_pw_in_tkt(user, instance, realm, "changepw",
			       KRB_MASTER, 1, cpw);

    if (kerror == INTK_BADPW)
	return(GT_PW_BADPW);

    if (kerror == KDC_NULL_KEY) {
	kerror = krb_get_pw_in_tkt("default","changepw",realm,"changepw",
				   KRB_MASTER,1,"changepwkrb");
	if (kerror)
	    return(kerror);
	return(GT_PW_NULL);
    }

    return(kerror);
}
