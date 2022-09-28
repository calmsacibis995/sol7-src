#ident	"@(#)krbglue.c	1.5	97/11/25 SMI"
/*
 * Copyright (c) 1988-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	$Source: /mit/kerberos/src/lib/krb/RCS/krbglue.c,v $
 *	$Author: wesommer $
 *	$Header: krbglue.c,v 4.1 89/01/23 15:51:50 wesommer Exp $
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#ifndef NCOMPAT
#ifndef lint
static char *rcsid_krbglue_c = "$Header: krbglue.c,v 4.1 89/01/23 15:51:50 wesommer Exp $";
#endif lint

/*
 * glue together new libraries and old clients
 */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <kerberos/des.h>
#include <kerberos/krb.h>

#if defined(__HIGHC__)
#undef __STDC__
#endif
#ifdef __STDC__
extern int krb_mk_priv (u_char *, u_char *, u_int, Key_schedule,
			 C_Block, struct sockaddr_in *,
			 struct sockaddr_in *);
extern int krb_rd_priv (u_char *, u_int, Key_schedule,
			 C_Block, struct sockaddr_in *,
			 struct sockaddr_in *, MSG_DAT *);
extern int krb_get_pw_in_tkt (char *, char *, char *, char *, char *, int,
			      char *);
extern int krb_get_svc_in_tkt (char *, char *, char *, char *, char *, int,
			       char *);
extern int krb_get_pw_tkt (char *, char *, char *, char *);
#ifdef DEBUG
extern KTEXT krb_create_death_packet (char *);
#endif /* DEBUG */
#else
extern int krb_mk_priv ();
extern int krb_rd_priv ();
extern int krb_get_pw_in_tkt ();
extern int krb_get_svc_in_tkt ();
extern int krb_get_pw_tkt ();
#ifdef DEBUG
extern KTEXT krb_create_death_packet ();
#endif /* DEBUG */
#endif /* STDC */
int mk_ap_req(authent, service, instance, realm, checksum)
    KTEXT authent;
    char *service, *instance, *realm;
    u_int checksum;
{
    return krb_mk_req(authent,service,instance,realm,checksum);
}

int rd_ap_req(authent, service, instance, from_addr, ad, fn)
    KTEXT authent;
    char *service, *instance;
    u_int from_addr;
    AUTH_DAT *ad;
    char *fn;
{
    return krb_rd_req(authent,service,instance,from_addr,ad,fn);
}

int an_to_ln(ad, lname)
    AUTH_DAT *ad;
    char *lname;
{
    return krb_kntoln (ad,lname);
}

int set_serv_key (key, cvt)
    char *key;
    int cvt;
{
    return krb_set_key(key,cvt);
}

int get_credentials (svc,inst,rlm,cred)
    char *svc, *inst, *rlm;
    CREDENTIALS *cred;
{
    return krb_get_cred (svc, inst, rlm, cred);
}

int mk_private_msg (in,out,in_length,schedule,key,sender,receiver)
    u_char *in, *out;
    u_int in_length;
    Key_schedule schedule;
    C_Block key;
    struct sockaddr_in *sender, *receiver;
{
    return krb_mk_priv (in,out,in_length,schedule,key,sender,receiver);
}

int rd_private_msg (in,in_length,schedule,key,sender,receiver,msg_data)
    u_char *in;
    u_int in_length;
    Key_schedule schedule;
    C_Block key;
    struct sockaddr_in *sender, *receiver;
    MSG_DAT *msg_data;
{
    return krb_rd_priv (in,in_length,schedule,key,sender,receiver,msg_data);
}

int mk_safe_msg (in,out,in_length,key,sender,receiver)
    u_char *in, *out;
    u_int in_length;
    C_Block *key;
    struct sockaddr_in *sender, *receiver;
{
    return krb_mk_safe (in,out,in_length,key,sender,receiver);
}

int rd_safe_msg (in,length,key,sender,receiver,msg_data)
    u_char *in;
    u_int length;
    C_Block *key;
    struct sockaddr_in *sender, *receiver;
    MSG_DAT *msg_data;
{
    return krb_rd_safe (in,length,key,sender,receiver,msg_data);
}

int mk_appl_err_msg (out,code,string)
    u_char *out;
    int code;
    char *string;
{
    return krb_mk_err (out,code,string);
}

int rd_appl_err_msg (in,length,code,msg_data)
    u_char *in;
    u_int length;
    int *code;
    MSG_DAT *msg_data;
{
    return krb_rd_err (in,length,code,msg_data);
}

int get_in_tkt(user,instance,realm,service,sinstance,life,password)
    char *user, *instance, *realm, *service, *sinstance;
    int life;
    char *password;
{
    return krb_get_pw_in_tkt(user,instance,realm,service,sinstance,
			     life,password);
}

int get_svc_in_tkt(user, instance, realm, service, sinstance, life, srvtab)
    char *user, *instance, *realm, *service, *sinstance;
    int life;
    char *srvtab;
{
    return krb_get_svc_in_tkt(user, instance, realm, service, sinstance,
			      life, srvtab);
}

int get_pw_tkt(user,instance,realm,cpw)
    char *user;
    char *instance;
    char *realm;
    char *cpw;
{
    return krb_get_pw_tkt(user,instance,realm,cpw);
}

int
get_krbrlm (r, n)
char *r;
int n;
{
    return krb_get_lream(r,n);
}

int
krb_getrealm (host)
{
    return krb_realmofhost(host);
}

char *
get_phost (host)
char *host
{
    return krb_get_phost(host);
}

int
get_krbhst (h, r, n)
char *h;
char *r;
int n;
{
    return krb_get_krbhst(h,r,n);
}
#ifdef DEBUG
struct ktext *create_death_packet(a_name)
    char *a_name;
{
    return krb_create_death_packet(a_name);
}
#endif /* DEBUG */

#if 0
extern int krb_ck_repl ();

int check_replay ()
{
    return krb_ck_repl ();
}
#endif
#endif /* NCOMPAT */
