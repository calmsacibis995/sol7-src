/*
 * Copyright (c) 1986-1991 by Sun Microsystems Inc.
 */

#ident	"@(#)rpc_callmsg.c	1.12	97/12/17 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)rpc_callmsg.c 1.12 89/01/31 Copyr 1984 Sun Micro";
#endif

/*
 * rpc_callmsg.c
 *
 */

#include <sys/param.h>
#include <rpc/trace.h>

#ifdef KERNEL
#include <rpc/types.h>		/* spell 'em out for make depend */
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#else
#include <rpc/rpc.h>
#endif
#include <sys/byteorder.h>

extern bool_t xdr_opaque_auth();
extern char *malloc();

/*
 * XDR a call message
 */
bool_t
xdr_callmsg(xdrs, cmsg)
	register XDR *xdrs;
	register struct rpc_msg *cmsg;
{
	register rpc_inline_t *buf;
	register struct opaque_auth *oa;
	bool_t dummy;

	trace1(TR_xdr_callmsg, 0);
	if (xdrs->x_op == XDR_ENCODE) {
		if (cmsg->rm_call.cb_cred.oa_length > MAX_AUTH_BYTES) {
			trace1(TR_xdr_callmsg, 1);
			return (FALSE);
		}
		if (cmsg->rm_call.cb_verf.oa_length > MAX_AUTH_BYTES) {
			trace1(TR_xdr_callmsg, 1);
			return (FALSE);
		}
		buf = XDR_INLINE(xdrs, 8 * BYTES_PER_XDR_UNIT
			+ RNDUP(cmsg->rm_call.cb_cred.oa_length)
			+ 2 * BYTES_PER_XDR_UNIT
			+ RNDUP(cmsg->rm_call.cb_verf.oa_length));
		if (buf != NULL) {
			IXDR_PUT_INT32(buf, cmsg->rm_xid);
			IXDR_PUT_ENUM(buf, cmsg->rm_direction);
			if (cmsg->rm_direction != CALL) {
				trace1(TR_xdr_callmsg, 1);
				return (FALSE);
			}
			IXDR_PUT_INT32(buf, cmsg->rm_call.cb_rpcvers);
			if (cmsg->rm_call.cb_rpcvers != RPC_MSG_VERSION) {
				trace1(TR_xdr_callmsg, 1);
				return (FALSE);
			}
			IXDR_PUT_INT32(buf, cmsg->rm_call.cb_prog);
			IXDR_PUT_INT32(buf, cmsg->rm_call.cb_vers);
			IXDR_PUT_INT32(buf, cmsg->rm_call.cb_proc);
			oa = &cmsg->rm_call.cb_cred;
			IXDR_PUT_ENUM(buf, oa->oa_flavor);
			IXDR_PUT_INT32(buf, oa->oa_length);
			if (oa->oa_length) {
				(void) memcpy((caddr_t)buf, oa->oa_base,
							oa->oa_length);
				buf += RNDUP(oa->oa_length) / sizeof (int32_t);
			}
			oa = &cmsg->rm_call.cb_verf;
			IXDR_PUT_ENUM(buf, oa->oa_flavor);
			IXDR_PUT_INT32(buf, oa->oa_length);
			if (oa->oa_length) {
				(void) memcpy((caddr_t)buf, oa->oa_base,
							oa->oa_length);
				/*
				 * no real need....
				buf += RNDUP(oa->oa_length) / sizeof (int32_t);
				*/
			}
			trace1(TR_xdr_callmsg, 1);
			return (TRUE);
		}
	}
	if (xdrs->x_op == XDR_DECODE) {
		buf = XDR_INLINE(xdrs, 8 * BYTES_PER_XDR_UNIT);
		if (buf != NULL) {
			cmsg->rm_xid = IXDR_GET_INT32(buf);
			cmsg->rm_direction = IXDR_GET_ENUM(buf, enum msg_type);
			if (cmsg->rm_direction != CALL) {
				trace1(TR_xdr_callmsg, 1);
				return (FALSE);
			}
			cmsg->rm_call.cb_rpcvers = IXDR_GET_INT32(buf);
			if (cmsg->rm_call.cb_rpcvers != RPC_MSG_VERSION) {
				trace1(TR_xdr_callmsg, 1);
				return (FALSE);
			}
			cmsg->rm_call.cb_prog = IXDR_GET_INT32(buf);
			cmsg->rm_call.cb_vers = IXDR_GET_INT32(buf);
			cmsg->rm_call.cb_proc = IXDR_GET_INT32(buf);
			oa = &cmsg->rm_call.cb_cred;
			oa->oa_flavor = IXDR_GET_ENUM(buf, enum_t);
			oa->oa_length = IXDR_GET_INT32(buf);
			if (oa->oa_length) {
				if (oa->oa_length > MAX_AUTH_BYTES) {
					trace1(TR_xdr_callmsg, 1);
					return (FALSE);
				}
				if (oa->oa_base == NULL) {
					oa->oa_base = (caddr_t)
						mem_alloc(oa->oa_length);
				}
				buf = XDR_INLINE(xdrs, RNDUP(oa->oa_length));
				if (buf == NULL) {
					if (xdr_opaque(xdrs, oa->oa_base,
					    oa->oa_length) == FALSE) {
						trace1(TR_xdr_callmsg, 1);
						return (FALSE);
					}
				} else {
					(void) memcpy(oa->oa_base,
					    (caddr_t)buf, (int) oa->oa_length);
					/*
					 * no real need....
					buf += RNDUP(oa->oa_length) /
						(int) sizeof (int32_t);
					*/
				}
			}
			oa = &cmsg->rm_call.cb_verf;
			buf = XDR_INLINE(xdrs, 2 * BYTES_PER_XDR_UNIT);
			if (buf == NULL) {
				if (xdr_enum(xdrs, &oa->oa_flavor) == FALSE ||
				    xdr_u_int(xdrs, &oa->oa_length) == FALSE) {
					trace1(TR_xdr_callmsg, 1);
					return (FALSE);
				}
			} else {
				oa->oa_flavor = IXDR_GET_ENUM(buf, enum_t);
				oa->oa_length = IXDR_GET_INT32(buf);
			}
			if (oa->oa_length) {
				if (oa->oa_length > MAX_AUTH_BYTES) {
					trace1(TR_xdr_callmsg, 1);
					return (FALSE);
				}
				if (oa->oa_base == NULL) {
					oa->oa_base = (caddr_t)
						mem_alloc(oa->oa_length);
				}
				buf = XDR_INLINE(xdrs, RNDUP(oa->oa_length));
				if (buf == NULL) {
					if (xdr_opaque(xdrs, oa->oa_base,
					    oa->oa_length) == FALSE) {
						trace1(TR_xdr_callmsg, 1);
						return (FALSE);
					}
				} else {
					(void) memcpy(oa->oa_base,
					    (caddr_t)buf, (int)oa->oa_length);
					/*
					 * no real need...
					buf += RNDUP(oa->oa_length) /
						(int) sizeof (int32_t);
					*/
				}
			}
			trace1(TR_xdr_callmsg, 1);
			return (TRUE);
		}
	}
	if (xdr_u_int(xdrs, &(cmsg->rm_xid)) &&
	    xdr_enum(xdrs, (enum_t *)&(cmsg->rm_direction)) &&
	    (cmsg->rm_direction == CALL) &&
	    xdr_u_int(xdrs, (u_int *)&(cmsg->rm_call.cb_rpcvers)) &&
	    (cmsg->rm_call.cb_rpcvers == RPC_MSG_VERSION) &&
	    xdr_u_int(xdrs, (u_int *)&(cmsg->rm_call.cb_prog)) &&
	    xdr_u_int(xdrs, (u_int *)&(cmsg->rm_call.cb_vers)) &&
	    xdr_u_int(xdrs, (u_int *)&(cmsg->rm_call.cb_proc)) &&
	    xdr_opaque_auth(xdrs, &(cmsg->rm_call.cb_cred))) {
	    dummy = xdr_opaque_auth(xdrs, &(cmsg->rm_call.cb_verf));
	    trace1(TR_xdr_callmsg, 1);
	    return (dummy);
	}
	trace1(TR_xdr_callmsg, 1);
	return (FALSE);
}
