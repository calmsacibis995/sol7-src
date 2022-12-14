/*
 * Copyright (c) 1990,1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#ifndef _RPC_KERBD_PROT_H
#define	_RPC_KERBD_PROT_H

#pragma ident	"@(#)kerbd_prot.h	1.8	98/01/06 SMI"

#include <rpc/rpc.h>

/*
 *  RPC protocol information for kerbd, the usermode daemon which
 *  assists the kernel when handling kerberos ticket generation and
 *  validation.  It is kerbd which actually communicates with the
 *  kerberos KDC.
 *
 *  File generated from kerbd.x 1.2 91/05/01 SMI
 *
 *  Copyright 1990,1991 Sun Microsystems, Inc.
 */
#include <kerberos/krb.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct {
	uint_t TICKET_len;
	char *TICKET_val;
} TICKET;
#ifdef __cplusplus
extern "C" bool_t xdr_TICKET(XDR *, TICKET*);
#elif __STDC__
extern  bool_t xdr_TICKET(XDR *, TICKET*);
#else /* Old Style C */
bool_t xdr_TICKET();
#endif /* Old Style C */


typedef char *KNAME;
#ifdef __cplusplus
extern "C" bool_t xdr_KNAME(XDR *, KNAME*);
#elif __STDC__
extern  bool_t xdr_KNAME(XDR *, KNAME*);
#else /* Old Style C */
bool_t xdr_KNAME();
#endif /* Old Style C */


typedef char *KINST;
#ifdef __cplusplus
extern "C" bool_t xdr_KINST(XDR *, KINST*);
#elif __STDC__
extern  bool_t xdr_KINST(XDR *, KINST*);
#else /* Old Style C */
bool_t xdr_KINST();
#endif /* Old Style C */


typedef char *KREALM;
#ifdef __cplusplus
extern "C" bool_t xdr_KREALM(XDR *, KREALM*);
#elif __STDC__
extern  bool_t xdr_KREALM(XDR *, KREALM*);
#else /* Old Style C */
bool_t xdr_KREALM();
#endif /* Old Style C */


struct ksetkcred_arg {
	uint32_t cksum;
	KNAME sname;
	KINST sinst;
	KREALM srealm;
};
typedef struct ksetkcred_arg ksetkcred_arg;
#ifdef __cplusplus
extern "C" bool_t xdr_ksetkcred_arg(XDR *, ksetkcred_arg*);
#elif __STDC__
extern  bool_t xdr_ksetkcred_arg(XDR *, ksetkcred_arg*);
#else /* Old Style C */
bool_t xdr_ksetkcred_arg();
#endif /* Old Style C */


struct ksetkcred_resd {
	TICKET ticket;
	des_block key;
};
typedef struct ksetkcred_resd ksetkcred_resd;
#ifdef __cplusplus
extern "C" bool_t xdr_ksetkcred_resd(XDR *, ksetkcred_resd*);
#elif __STDC__
extern  bool_t xdr_ksetkcred_resd(XDR *, ksetkcred_resd*);
#else /* Old Style C */
bool_t xdr_ksetkcred_resd();
#endif /* Old Style C */


struct ksetkcred_res {
	int status;
	union {
		ksetkcred_resd res;
	} ksetkcred_res_u;
};
typedef struct ksetkcred_res ksetkcred_res;
#ifdef __cplusplus
extern "C" bool_t xdr_ksetkcred_res(XDR *, ksetkcred_res*);
#elif __STDC__
extern  bool_t xdr_ksetkcred_res(XDR *, ksetkcred_res*);
#else /* Old Style C */
bool_t xdr_ksetkcred_res();
#endif /* Old Style C */


struct kgetkcred_arg {
	TICKET ticket;
	KNAME sname;
	KINST sinst;
	uint32_t faddr;
};
typedef struct kgetkcred_arg kgetkcred_arg;
#ifdef __cplusplus
extern "C" bool_t xdr_kgetkcred_arg(XDR *, kgetkcred_arg*);
#elif __STDC__
extern  bool_t xdr_kgetkcred_arg(XDR *, kgetkcred_arg*);
#else /* Old Style C */
bool_t xdr_kgetkcred_arg();
#endif /* Old Style C */


struct kgetkcred_resd {
	KINST sinst;
	uint_t k_flags;
	KNAME pname;
	KINST pinst;
	KREALM prealm;
	uint32_t checksum;
	des_block session;
	int life;
	uint32_t time_sec;
	uint32_t address;
	TICKET reply;
};
typedef struct kgetkcred_resd kgetkcred_resd;
#ifdef __cplusplus
extern "C" bool_t xdr_kgetkcred_resd(XDR *, kgetkcred_resd*);
#elif __STDC__
extern  bool_t xdr_kgetkcred_resd(XDR *, kgetkcred_resd*);
#else /* Old Style C */
bool_t xdr_kgetkcred_resd();
#endif /* Old Style C */


struct kgetkcred_res {
	int status;
	union {
		kgetkcred_resd res;
	} kgetkcred_res_u;
};
typedef struct kgetkcred_res kgetkcred_res;
#ifdef __cplusplus
extern "C" bool_t xdr_kgetkcred_res(XDR *, kgetkcred_res*);
#elif __STDC__
extern  bool_t xdr_kgetkcred_res(XDR *, kgetkcred_res*);
#else /* Old Style C */
bool_t xdr_kgetkcred_res();
#endif /* Old Style C */


struct kgetucred_arg {
	KNAME pname;
};
typedef struct kgetucred_arg kgetucred_arg;
#ifdef __cplusplus
extern "C" bool_t xdr_kgetucred_arg(XDR *, kgetucred_arg*);
#elif __STDC__
extern  bool_t xdr_kgetucred_arg(XDR *, kgetucred_arg*);
#else /* Old Style C */
bool_t xdr_kgetucred_arg();
#endif /* Old Style C */

#define	KUCRED_MAXGRPS 32

struct kerb_ucred {
	uint_t uid;
	uint_t gid;
	struct {
		uint_t grplist_len;
		uint_t *grplist_val;
	} grplist;
};
typedef struct kerb_ucred kerb_ucred;
#ifdef __cplusplus
extern "C" bool_t xdr_kerb_ucred(XDR *, kerb_ucred*);
#elif __STDC__
extern  bool_t xdr_kerb_ucred(XDR *, kerb_ucred*);
#else /* Old Style C */
bool_t xdr_kerb_ucred();
#endif /* Old Style C */


enum ucred_stat {
	UCRED_OK = 0,
	UCRED_UNKNOWN = 1
};
typedef enum ucred_stat ucred_stat;
#ifdef __cplusplus
extern "C" bool_t xdr_ucred_stat(XDR *, ucred_stat*);
#elif __STDC__
extern  bool_t xdr_ucred_stat(XDR *, ucred_stat*);
#else /* Old Style C */
bool_t xdr_ucred_stat();
#endif /* Old Style C */


struct kgetucred_res {
	ucred_stat status;
	union {
		kerb_ucred cred;
	} kgetucred_res_u;
};
typedef struct kgetucred_res kgetucred_res;
#ifdef __cplusplus
extern "C" bool_t xdr_kgetucred_res(XDR *, kgetucred_res*);
#elif __STDC__
extern  bool_t xdr_kgetucred_res(XDR *, kgetucred_res*);
#else /* Old Style C */
bool_t xdr_kgetucred_res();
#endif /* Old Style C */


#define	KERBPROG ((rpcprog_t)100078)
#define	KERBVERS ((rpcvers_t)4)

#ifdef __cplusplus
#define	KGETKCRED ((rpcproc_t)1)
extern "C" kgetkcred_res * kgetkcred_4(kgetkcred_arg *, CLIENT *);
extern "C" kgetkcred_res * kgetkcred_4_svc(kgetkcred_arg *, struct svc_req *);
#define	KSETKCRED ((rpcproc_t)2)
extern "C" ksetkcred_res * ksetkcred_4(ksetkcred_arg *, CLIENT *);
extern "C" ksetkcred_res * ksetkcred_4_svc(ksetkcred_arg *, struct svc_req *);
#define	KGETUCRED ((rpcproc_t)3)
extern "C" kgetucred_res * kgetucred_4(kgetucred_arg *, CLIENT *);
extern "C" kgetucred_res * kgetucred_4_svc(kgetucred_arg *, struct svc_req *);

#elif __STDC__
#define	KGETKCRED ((rpcproc_t)1)
extern  kgetkcred_res * kgetkcred_4(kgetkcred_arg *, CLIENT *);
extern  kgetkcred_res * kgetkcred_4_svc(kgetkcred_arg *, struct svc_req *);
#define	KSETKCRED ((rpcproc_t)2)
extern  ksetkcred_res * ksetkcred_4(ksetkcred_arg *, CLIENT *);
extern  ksetkcred_res * ksetkcred_4_svc(ksetkcred_arg *, struct svc_req *);
#define	KGETUCRED ((rpcproc_t)3)
extern  kgetucred_res * kgetucred_4(kgetucred_arg *, CLIENT *);
extern  kgetucred_res * kgetucred_4_svc(kgetucred_arg *, struct svc_req *);

#else /* Old Style C */
#define	KGETKCRED ((rpcproc_t)1)
extern  kgetkcred_res * kgetkcred_4();
extern  kgetkcred_res * kgetkcred_4_svc();
#define	KSETKCRED ((rpcproc_t)2)
extern  ksetkcred_res * ksetkcred_4();
extern  ksetkcred_res * ksetkcred_4_svc();
#define	KGETUCRED ((rpcproc_t)3)
extern  kgetucred_res * kgetucred_4();
extern  kgetucred_res * kgetucred_4_svc();
#endif /* Old Style C */

#ifdef	__cplusplus
}
#endif

#endif	/* !_RPC_KERBD_PROT_H */
