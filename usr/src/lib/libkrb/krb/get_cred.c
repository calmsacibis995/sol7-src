#ident	"@(#)get_cred.c	1.2	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/get_cred.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_get_cred_c =
"$Header: get_cred.c,v 4.10 89/05/31 17:46:22 jtkohl Exp $";
#endif /* lint */

#include <kerberos/mit-copyright.h>
#include <stdio.h>
#include <string.h>
#include <kerberos/krb.h>

/*
 * krb_get_cred takes a service name, instance, and realm, and a
 * structure of type CREDENTIALS to be filled in with ticket
 * information.  It then searches the ticket file for the appropriate
 * ticket and fills in the structure with the corresponding
 * information from the file.  If successful, it returns KSUCCESS.
 * On failure it returns a Kerberos error code.
 */

krb_get_cred(service,instance,realm,c)
    char *service;              /* Service name */
    char *instance;             /* Instance */
    char *realm;                /* Auth domain */
    CREDENTIALS *c;             /* Credentials struct */
{
    int tf_status;              /* return value of tf function calls */
    extern tf_init(), tf_get_pname(), tf_get_pinst(), tf_get_cred(),
		tf_close();

    /* Open ticket file and lock it for shared reading */
    if ((tf_status = tf_init(TKT_FILE, R_TKT_FIL)) != KSUCCESS)
	return(tf_status);

    /* Copy principal's name and instance into the CREDENTIALS struc c */

    if ( (tf_status = tf_get_pname(c->pname)) != KSUCCESS ||
    	 (tf_status = tf_get_pinst(c->pinst)) != KSUCCESS )
	return (tf_status);

    /* Search for requested service credentials and copy into c */
       
    while ((tf_status = tf_get_cred(c)) == KSUCCESS) {
        /* Is this the right ticket? */
	if ((strcmp(c->service,service) == 0) &&
           (strcmp(c->instance,instance) == 0) &&
           (strcmp(c->realm,realm) == 0))
		   break;
    }
    (void) tf_close();

    if (tf_status == EOF)
	return (GC_NOTKT);
    return(tf_status);
}
