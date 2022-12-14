#ident	"@(#)mk_err.c	1.3	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/mk_err.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_mk_err_c =
"$Header: mk_err.c,v 4.4 88/11/15 16:33:36 jtkohl Exp $";
#endif /* lint */

#ifdef SYSV
#include <string.h>
#else
#include <strings.h>
#endif /* SYSV */

#include <kerberos/mit-copyright.h>
#include <sys/types.h>
#include <krb_private.h>
#include <kerberos/krb.h>
#include <kerberos/prot.h>

/*
 * This routine creates a general purpose error reply message.  It
 * doesn't use KTEXT because application protocol may have long
 * messages, and may want this part of buffer contiguous to other
 * stuff.
 *
 * The error reply is built in "p", using the error code "e" and
 * error text "e_string" given.  The length of the error reply is
 * returned.
 *
 * The error reply is in the following format:
 *
 * unsigned char	KRB_PROT_VERSION	protocol version no.
 * unsigned char	AUTH_MSG_APPL_ERR	message type
 * (least significant
 * bit of above)	HOST_BYTE_ORDER		local byte order
 * 4 bytes		e			given error code
 * string		e_string		given error text
 */

int krb_mk_err(p,e,e_string)
    u_char *p;			/* Where to build error packet */
    int e;			/* Error code */
    char *e_string;		/* Text of error */
{
    u_char      *start;

    start = p;

    /* Create fixed part of packet */
    *p++ = (unsigned char) KRB_PROT_VERSION;
    *p = (unsigned char) AUTH_MSG_APPL_ERR;
    *p++ |= HOST_BYTE_ORDER;

    /* Add the basic info */
    bcopy((char *)&e,(char *)p,4); /* err code */
    p += sizeof(e);
    (void) strcpy((char *)p,e_string); /* err text */
    p += strlen(e_string);

    /* And return the length */
    return ((int) (p-start));
}
