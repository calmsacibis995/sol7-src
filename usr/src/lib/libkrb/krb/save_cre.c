#ident	"@(#)save_credentials.c	1.2	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/save_credentials.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_save_credentials_c =
"$Header: save_credentials.c,v 4.9 89/05/31 17:45:43 jtkohl Exp $";
#endif /* lint */

#include <kerberos/mit-copyright.h>
#include <stdio.h>
#include <kerberos/krb.h>

/*
 * This routine takes a ticket and associated info and calls
 * tf_save_cred() to store them in the ticket cache.  The peer
 * routine for extracting a ticket and associated info from the
 * ticket cache is krb_get_cred().  When changes are made to
 * this routine, the corresponding changes should be made
 * in krb_get_cred() as well.
 *
 * Returns KSUCCESS if all goes well, otherwise an error returned
 * by the tf_init() or tf_save_cred() routines.
 */

save_credentials(service, instance, realm, session, lifetime, kvno,
                 ticket, issue_date)
    char *service;              /* Service name */
    char *instance;             /* Instance */
    char *realm;                /* Auth domain */
    C_Block session;            /* Session key */
    int lifetime;               /* Lifetime */
    int kvno;                   /* Key version number */
    KTEXT ticket;               /* The ticket itself */
    int issue_date;            /* The issue time */
{
    int tf_status;   /* return values of the tf_util calls */
    extern tf_init(), tf_save_cred(), tf_close();

    /* Open and lock the ticket file for writing */
    if ((tf_status = tf_init(TKT_FILE, W_TKT_FIL)) != KSUCCESS)
	return(tf_status);

    /* Save credentials by appending to the ticket file */
    tf_status = tf_save_cred(service, instance, realm, session,
			     lifetime, kvno, ticket, issue_date);
    (void) tf_close();
    return (tf_status);
}
