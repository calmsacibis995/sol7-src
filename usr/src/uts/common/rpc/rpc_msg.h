/*
 * Copyright (c) 1986-1991,1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _RPC_RPCMSG_H
#define	_RPC_RPCMSG_H

#pragma ident	"@(#)rpc_msg.h	1.15	98/01/06 SMI"

/*	rpc_msg.h 1.11 88/10/25 SMI	*/

#include <rpc/clnt.h>
/*
 * rpc_msg.h
 * rpc message definition
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	RPC_MSG_VERSION		((uint32_t)2)
#define	RPC_SERVICE_PORT	((ushort_t)2048)

/*
 * Bottom up definition of an rpc message.
 * NOTE: call and reply use the same overall stuct but
 * different parts of unions within it.
 */

enum msg_type {
	CALL = 0,
	REPLY = 1
};

enum reply_stat {
	MSG_ACCEPTED = 0,
	MSG_DENIED = 1
};

enum accept_stat {
	SUCCESS = 0,
	PROG_UNAVAIL = 1,
	PROG_MISMATCH = 2,
	PROC_UNAVAIL = 3,
	GARBAGE_ARGS = 4,
	SYSTEM_ERR = 5
};

enum reject_stat {
	RPC_MISMATCH = 0,
	AUTH_ERROR = 1
};

/*
 * Reply part of an rpc exchange
 */

/*
 * Reply to an rpc request that was accepted by the server.
 * Note: there could be an error even though the request was
 * accepted.
 */
struct accepted_reply {
	struct opaque_auth	ar_verf;
	enum accept_stat	ar_stat;
	union {
		struct {
			rpcvers_t low;
			rpcvers_t high;
		} AR_versions;
		struct {
			caddr_t	where;
			xdrproc_t proc;
		} AR_results;
		/* and many other null cases */
	} ru;
#define	ar_results	ru.AR_results
#define	ar_vers		ru.AR_versions
};

/*
 * Reply to an rpc request that was rejected by the server.
 */
struct rejected_reply {
	enum reject_stat rj_stat;
	union {
		struct {
			rpcvers_t low;
			rpcvers_t high;
		} RJ_versions;
		enum auth_stat RJ_why;  /* why authentication did not work */
	} ru;
#define	rj_vers	ru.RJ_versions
#define	rj_why	ru.RJ_why
};

/*
 * Body of a reply to an rpc request.
 */
struct reply_body {
	enum reply_stat rp_stat;
	union {
		struct accepted_reply RP_ar;
		struct rejected_reply RP_dr;
	} ru;
#define	rp_acpt	ru.RP_ar
#define	rp_rjct	ru.RP_dr
};

/*
 * Body of an rpc request call.
 */
struct call_body {
	rpcvers_t cb_rpcvers;	/* must be equal to two */
	rpcprog_t cb_prog;
	rpcvers_t cb_vers;
	rpcproc_t cb_proc;
	struct opaque_auth cb_cred;
	struct opaque_auth cb_verf; /* protocol specific - provided by client */
};

/*
 * The rpc message
 */
struct rpc_msg {
	uint32_t		rm_xid;
	enum msg_type		rm_direction;
	union {
		struct call_body RM_cmb;
		struct reply_body RM_rmb;
	} ru;
#define	rm_call		ru.RM_cmb
#define	rm_reply	ru.RM_rmb
};
#define	acpted_rply	ru.RM_rmb.ru.RP_ar
#define	rjcted_rply	ru.RM_rmb.ru.RP_dr


/*
 * XDR routine to handle a rpc message.
 * xdr_callmsg(xdrs, cmsg)
 * 	XDR *xdrs;
 * 	struct rpc_msg *cmsg;
 */
#ifdef __STDC__
extern bool_t	xdr_callmsg(XDR *, struct rpc_msg *);
#else
extern bool_t	xdr_callmsg();
#endif


/*
 * XDR routine to pre-serialize the static part of a rpc message.
 * xdr_callhdr(xdrs, cmsg)
 * 	XDR *xdrs;
 * 	struct rpc_msg *cmsg;
 */
#ifdef __STDC__
extern bool_t	xdr_callhdr(XDR *, struct rpc_msg *);
#else
extern bool_t	xdr_callhdr();
#endif


/*
 * XDR routine to handle a rpc reply.
 * xdr_replymsg(xdrs, rmsg)
 * 	XDR *xdrs;
 * 	struct rpc_msg *rmsg;
 */
#ifdef __STDC__
extern bool_t	xdr_replymsg(XDR *, struct rpc_msg *);
#else
extern bool_t	xdr_replymsg();
#endif


#ifdef _KERNEL
/*
 * Fills in the error part of a reply message.
 * _seterr_reply(msg, error)
 * 	struct rpc_msg *msg;
 * 	struct rpc_err *error;
 */
#ifdef __STDC__
extern void	_seterr_reply(struct rpc_msg *, struct rpc_err *);
#else
extern void	_seterr_reply();
#endif
#else
/*
 * Fills in the error part of a reply message.
 * __seterr_reply(msg, error)
 * 	struct rpc_msg *msg;
 * 	struct rpc_err *error;
 */
#ifdef __STDC__
extern void	__seterr_reply(struct rpc_msg *, struct rpc_err *);
#else
extern void	__seterr_reply();
#endif
#endif

#ifdef _KERNEL
/*
 * Frees any verifier that xdr_replymsg() (DECODE) allocated.
 */
bool_t xdr_rpc_free_verifier(register XDR *xdrs, register struct rpc_msg *msg);

#endif

#ifdef __cplusplus
}
#endif

#endif	/* _RPC_RPCMSG_H */
