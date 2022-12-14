#ident	"@(#)decomp_ticket.c	1.4	97/11/25 SMI"
/*
 * Copyright (c) 1985-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * $Source: /mit/kerberos/src/lib/krb/RCS/decomp_ticket.c,v $
 * $Author: jtkohl $
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef lint
static char *rcsid_decomp_ticket_c =
"$Header: decomp_ticket.c,v 4.12 89/05/16 18:44:46 jtkohl Exp $";
#endif /* lint */

#ifdef SYSV
#include <string.h>
#else
#include <strings.h>
#endif /* SYSV */

#include <kerberos/mit-copyright.h>
#include <stdio.h>
#include <stdlib.h>
#include <krb_private.h>
#include <kerberos/des.h>
#include <kerberos/krb.h>
#include <kerberos/prot.h>

/*
 * This routine takes a ticket and pointers to the variables that
 * should be filled in based on the information in the ticket.  It
#ifndef NOENCRYPTION
 * decrypts the ticket using the given key, and 
#endif
 * fills in values for its arguments.
 *
 * Note: if the client realm field in the ticket is the null string,
 * then the "prealm" variable is filled in with the local realm (as
 * defined by get_krbrlm).
 *
 * If the ticket byte order is different than the host's byte order
 * (as indicated by the byte order bit of the "flags" field), then
 * the KDC timestamp "time_sec" is byte-swapped.  The other fields
 * potentially affected by byte order, "paddress" and "session" are
 * not byte-swapped.
 *
 * The routine returns KFAILURE if any of the "pname", "pinstance",
 * or "prealm" fields is too big, otherwise it returns KSUCCESS.
 *
 * The corresponding routine to generate tickets is create_ticket.
 * When changes are made to this routine, the corresponding changes
 * should also be made to that file.
 *
 * See create_ticket.c for the format of the ticket packet.
 */

decomp_ticket(tkt, flags, pname, pinstance, prealm, paddress, session,
              life, time_sec, sname, sinstance, key, key_s)
    KTEXT tkt;			/* The ticket to be decoded */
    unsigned char *flags;       /* Kerberos ticket flags */
    char *pname;		/* Authentication name */
    char *pinstance;		/* Principal's instance */
    char *prealm;		/* Principal's authentication domain */
    unsigned int *paddress;    /* Net address of entity
                                 * requesting ticket */
    C_Block session;		/* Session key inserted in ticket */
    int *life; 		        /* Lifetime of the ticket */
    unsigned int *time_sec;    /* Issue time and date */
    char *sname;		/* Service name */
    char *sinstance;		/* Service instance */
    C_Block key;		/* Service's secret key
                                 * (to decrypt the ticket) */
    Key_schedule key_s;		/* The precomputed key schedule */
{
    static int tkt_swap_bytes;
    unsigned char *uptr;
    char *ptr = (char *)tkt->dat;
    extern int pcbc_encrypt();

#ifndef NOENCRYPTION
    /* Do the decryption */
    pcbc_encrypt((C_Block *)tkt->dat,(C_Block *)tkt->dat,
                 (int) tkt->length,key_s,key,0);
#endif /* ! NOENCRYPTION */

    *flags = *ptr;              /* get flags byte */
    ptr += sizeof(*flags);
    tkt_swap_bytes = 0;
    if (HOST_BYTE_ORDER != ((*flags >> K_FLAG_ORDER)& 1))
        tkt_swap_bytes++;

    if ((int)strlen(ptr) > ANAME_SZ)
        return(KFAILURE);
    (void) strcpy(pname,ptr);   /* pname */
    ptr += strlen(pname) + 1;

    if ((int)strlen(ptr) > INST_SZ)
        return(KFAILURE);
    (void) strcpy(pinstance,ptr); /* instance */
    ptr += strlen(pinstance) + 1;

    if ((int)strlen(ptr) > REALM_SZ)
        return(KFAILURE);
    (void) strcpy(prealm,ptr);  /* realm */
    ptr += strlen(prealm) + 1;
    /* temporary hack until realms are dealt with properly */
    if (*prealm == 0) {
	if (krb_get_lrealm(prealm, 1) != KSUCCESS)
	    return(KFAILURE);
    }

    bcopy(ptr,(char *)paddress,4); /* net address */
    ptr += 4;
    if (tkt_swap_bytes)
        swap_u_long(*paddress);

    bcopy(ptr,(char *)session,8); /* session key */
    ptr+= 8;
#ifdef notdef /* DONT SWAP SESSION KEY spm 10/22/86 */
    if (tkt_swap_bytes)
        swap_C_Block(session);
#endif

    /* get lifetime, being certain we don't get negative lifetimes */
    uptr = (unsigned char *) ptr++;
    *life = (int) *uptr;

    bcopy(ptr,(char *) time_sec,4); /* issue time */
    ptr += 4;
    if (tkt_swap_bytes)
        swap_u_long(*time_sec);

    (void) strcpy(sname,ptr);   /* service name */
    ptr += 1 + strlen(sname);

    (void) strcpy(sinstance,ptr); /* instance */
    ptr += 1 + strlen(sinstance);
    return(KSUCCESS);
}
