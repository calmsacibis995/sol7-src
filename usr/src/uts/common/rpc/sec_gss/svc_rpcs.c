/*
 * Copyright (c) 1996,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)svc_rpcsec_gss.c 1.46	97/11/16 SMI"

/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id: svc_auth_gssapi.c,v 1.19 1994/10/27 12:38:51 jik Exp $
 */

/*
 * Server side handling of RPCSEC_GSS flavor.
 */

#include <sys/systm.h>
#include <sys/kstat.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/types.h>
#include <sys/time.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
#include <rpc/rpc.h>
#include <rpc/rpcsec_defs.h>

extern bool_t __rpc_gss_make_principal(rpc_gss_principal_t *, gss_buffer_t);

/*
 * Sequence window definitions.
 */
#define	SEQ_ARR_SIZE	4
#define	SEQ_WIN		(SEQ_ARR_SIZE*32)
#define	SEQ_HI_BIT	0x80000000
#define	SEQ_LO_BIT	1
#define	DIV_BY_32	5
#define	SEQ_MASK	0x1f
#define	SEQ_MAX		((unsigned int)0x80000000)

/*
 * Server side RPCSEC_GSS context information.
 */
typedef struct _svc_rpc_gss_data {
	struct _svc_rpc_gss_data	*next, *prev;
	struct _svc_rpc_gss_data	*lru_next, *lru_prev;
	bool_t				established;
	gss_ctx_id_t			context;
	gss_buffer_desc			client_name;
	gss_cred_id_t			server_creds;
	time_t				expiration;
	u_int				seq_num;
	u_int				seq_bits[SEQ_ARR_SIZE];
	u_int				key;
	OM_uint32			qop;
	bool_t				done_docallback;
	bool_t				locked;
	rpc_gss_rawcred_t		raw_cred;
	rpc_gss_ucred_t			u_cred;
	time_t				u_cred_set;
	void				*cookie;
	gss_cred_id_t			deleg;
	kmutex_t			clm;
	int				ref_cnt;
	bool_t				stale;
} svc_rpc_gss_data;

/*
 * Data structures used for LRU based context management.
 */
#define	HASHMOD			256
#define	HASHMASK		255

static svc_rpc_gss_data		*clients[HASHMOD];
static svc_rpc_gss_data		*lru_first, *lru_last;
static time_t			sweep_interval = 60*60;
static time_t			last_swept = 0;
static int			num_gss_contexts = 0;
static int			max_gss_contexts = 128;
static bool_t			do_sweep_clients = FALSE;
static time_t			svc_rpcgss_gid_timeout = 60*60*12;

/*
 * lock used with context/lru variables
 */
static kmutex_t			ctx_mutex;

/*
 * server credential management data and structures
 */
typedef struct svc_creds_list_s {
	struct svc_creds_list_s	*next;
	gss_cred_id_t		cred;
	gss_name_t		name;
	u_int			program;
	u_int			version;
	time_t			expiration;
	gss_OID_set_desc	oid_set;
	OM_uint32		req_time;
	char			*service_name;
	gss_OID			mech;
	kmutex_t		refresh_mutex;
} svc_creds_list_t;

static svc_creds_list_t		*svc_creds_list = NULL;
static int			svc_creds_count = 0;

/*
 * lock used with server credential variables list
 */
static krwlock_t		cred_lock;

/*
 * server callback list
 */
typedef struct rpc_gss_cblist_s {
	struct rpc_gss_cblist_s		*next;
	rpc_gss_callback_t	cb;
} rpc_gss_cblist_t;

static rpc_gss_cblist_t			*rpc_gss_cblist = NULL;

/*
 * lock used with callback variables
 */
static kmutex_t			cb_mutex;

/*
 * forward declarations
 */
static bool_t			svc_rpc_gss_wrap();
static bool_t			svc_rpc_gss_unwrap();
static svc_rpc_gss_data		*create_client();
static svc_rpc_gss_data		*get_client();
static svc_rpc_gss_data		*find_client();
static void			destroy_client();
static void			sweep_clients();
static void			drop_lru_client();
static void			insert_client();
static bool_t			check_verf(struct rpc_msg *, gss_ctx_id_t,
					int *, uid_t);
static bool_t			set_response_verf();
static bool_t			rpc_gss_refresh_svc_cred();

/*
 * server side wrap/unwrap routines
 */
struct svc_auth_ops svc_rpc_gss_ops = {
	svc_rpc_gss_wrap,
	svc_rpc_gss_unwrap,
};

/*
 *  Init stuff on the server side.
 */
void
svc_gss_init()
{
	mutex_init(&cb_mutex, "cb_mutex", MUTEX_DEFAULT, NULL);
	mutex_init(&ctx_mutex, "ctx_mutex", MUTEX_DEFAULT, NULL);
	rw_init(&cred_lock, "cred_lock", RW_DEFAULT, NULL);
}


/*
 * Cleanup routine for destroying context, called after service
 * procedure is executed.
 */
void
rpc_gss_cleanup(xprt)
	SVCXPRT *xprt;
{
	svc_rpc_gss_data	*cl;
	SVCAUTH			*svcauth;

	/*
	 * First check if current context needs to be cleaned up.
	 * There might be other threads stale this client data
	 * in between.
	 */
	svcauth = &xprt->xp_auth;
	if ((cl = (svc_rpc_gss_data *)svcauth->svc_ah_private) != NULL) {
		mutex_enter(&cl->clm);
		ASSERT(cl->ref_cnt > 0);
		if (--cl->ref_cnt == 0 && cl->stale) {
			mutex_exit(&cl->clm);
			mutex_enter(&ctx_mutex);
			destroy_client(cl);
			svcauth->svc_ah_private = NULL;
			mutex_exit(&ctx_mutex);
		} else
			mutex_exit(&cl->clm);
	}

	/*
	 * Check for other expired contexts.
	 */
	if (do_sweep_clients) {

		if ((hrestime.tv_sec - last_swept) > sweep_interval) {
			mutex_enter(&ctx_mutex);

			/* Check again, in case some other thread got in. */
			if ((hrestime.tv_sec - last_swept) >
					sweep_interval) {
				sweep_clients();
			}
			mutex_exit(&ctx_mutex);
		}
	}
}

/*
 * Shift the array arr of length arrlen right by nbits bits.
 */
static void
shift_bits(arr, arrlen, nbits)
	u_int	*arr;
	int	arrlen;
	int	nbits;
{
	int	i, j;
	u_int	lo, hi;

	/*
	 * If the number of bits to be shifted exceeds SEQ_WIN, just
	 * zero out the array.
	 */
	if (nbits < SEQ_WIN) {
		for (i = 0; i < nbits; i++) {
			hi = 0;
			for (j = 0; j < arrlen; j++) {
				lo = arr[j] & SEQ_LO_BIT;
				arr[j] >>= 1;
				if (hi)
					arr[j] |= SEQ_HI_BIT;
				hi = lo;
			}
		}
	} else {
		for (j = 0; j < arrlen; j++)
			arr[j] = 0;
	}
}

/*
 * Check that the received sequence number seq_num is valid.
 */
static bool_t
check_seq(cl, seq_num, kill_context)
	svc_rpc_gss_data	*cl;
	u_int			seq_num;
	bool_t			*kill_context;
{
	int			i, j;
	u_int			bit;

	/*
	 * If it exceeds the maximum, kill context.
	 */
	if (seq_num >= SEQ_MAX) {
		*kill_context = TRUE;
		RPCGSS_LOG0(4, "check_seq: seq_num not valid\n");
		return (FALSE);
	}

	/*
	 * If greater than the last seen sequence number, just shift
	 * the sequence window so that it starts at the new sequence
	 * number and extends downwards by SEQ_WIN.
	 */
	if (seq_num > cl->seq_num) {
		(void) shift_bits(cl->seq_bits, SEQ_ARR_SIZE,
				(int) (seq_num - cl->seq_num));
		cl->seq_bits[0] |= SEQ_HI_BIT;
		cl->seq_num = seq_num;
		return (TRUE);
	}

	/*
	 * If it is outside the sequence window, return failure.
	 */
	i = cl->seq_num - seq_num;
	if (i >= SEQ_WIN) {
		RPCGSS_LOG0(4, "check_seq: seq_num is outside the window\n");
		return (FALSE);
	}

	/*
	 * If within sequence window, set the bit corresponding to it
	 * if not already seen;  if already seen, return failure.
	 */
	j = SEQ_MASK - (i & SEQ_MASK);
	bit = j > 0 ? (1 << j) : 1;
	i >>= DIV_BY_32;
	if (cl->seq_bits[i] & bit) {
		RPCGSS_LOG0(4, "check_seq: sequence number already seen\n");
		return (FALSE);
	}
	cl->seq_bits[i] |= bit;
	return (TRUE);
}

/*
 * Set server callback.
 */
bool_t
rpc_gss_set_callback(cb)
	rpc_gss_callback_t	*cb;
{
	rpc_gss_cblist_t		*cbl, *tmp;

	if (cb->callback == NULL) {
		RPCGSS_LOG0(1, "rpc_gss_set_callback: no callback to set\n");
		return (FALSE);
	}

	/* check if there is already an entry in the rpc_gss_cblist. */
	if (rpc_gss_cblist) {
		for (tmp = rpc_gss_cblist; tmp != NULL; tmp = tmp->next) {
			if ((tmp->cb.callback == cb->callback) &&
			    (tmp->cb.version == cb->version) &&
			    (tmp->cb.program == cb->program)) {
				return (TRUE);
			}
		}
	}

	/* Not in rpc_gss_cblist.  Create a new entry. */
	if ((cbl = (rpc_gss_cblist_t *) kmem_alloc(sizeof (*cbl), KM_SLEEP))
	    == NULL)
		return (FALSE);
	cbl->cb = *cb;
	mutex_enter(&cb_mutex);
	cbl->next = rpc_gss_cblist;
	rpc_gss_cblist = cbl;
	mutex_exit(&cb_mutex);
	return (TRUE);
}

/*
 * Locate callback (if specified) and call server.  Release any
 * delegated credentials unless passed to server and the server
 * accepts the context.  If a callback is not specified, accept
 * the incoming context.
 */
static bool_t
do_callback(req, client_data)
	struct svc_req		*req;
	svc_rpc_gss_data	*client_data;
{
	rpc_gss_cblist_t		*cbl;
	bool_t			ret = TRUE, found = FALSE;
	rpc_gss_lock_t		lock;
	OM_uint32		minor;
	mutex_enter(&cb_mutex);
	for (cbl = rpc_gss_cblist; cbl != NULL; cbl = cbl->next) {
		if (req->rq_prog != cbl->cb.program ||
					req->rq_vers != cbl->cb.version)
			continue;
		found = TRUE;
		lock.locked = FALSE;
		lock.raw_cred = &client_data->raw_cred;
		ret = (*cbl->cb.callback)(req, client_data->deleg,
			client_data->context, &lock, &client_data->cookie);
		req->rq_xprt->xp_cookie = client_data->cookie;

		if (ret) {
			client_data->locked = lock.locked;
			client_data->deleg = GSS_C_NO_CREDENTIAL;
		}
		break;
	}
	if (!found) {
		if (client_data->deleg != GSS_C_NO_CREDENTIAL) {
			(void) kgss_release_cred(&minor, &client_data->deleg,
					CRED()->cr_uid);
			client_data->deleg = GSS_C_NO_CREDENTIAL;
		}
	}
	mutex_exit(&cb_mutex);
	return (ret);
}

/*
 * Get caller credentials.
 */
bool_t
rpc_gss_getcred(req, rcred, ucred, cookie)
	struct svc_req		*req;
	rpc_gss_rawcred_t	**rcred;
	rpc_gss_ucred_t		**ucred;
	void			**cookie;
{
	SVCAUTH			*svcauth;
	svc_rpc_gss_data	*client_data;
	int			gssstat, gidlen;

	svcauth = &req->rq_xprt->xp_auth;
	client_data = (svc_rpc_gss_data *)svcauth->svc_ah_private;

	if (rcred != NULL) {
		svcauth->raw_cred = client_data->raw_cred;
		*rcred = &svcauth->raw_cred;
	}
	if (ucred != NULL) {
		*ucred = &client_data->u_cred;

		if (client_data->u_cred_set == 0 ||
		    client_data->u_cred_set < hrestime.tv_sec) {
		    mutex_enter(&client_data->clm);
		    if (client_data->u_cred_set == 0) {
			if ((gssstat = kgsscred_expname_to_unix_cred(
			    &client_data->client_name,
			    &client_data->u_cred.uid,
			    &client_data->u_cred.gid,
			    &client_data->u_cred.gidlist,
			    &gidlen, CRED()->cr_uid)) != GSS_S_COMPLETE) {
				RPCGSS_LOG(1, "rpc_gss_getcred: "
				    "kgsscred_expname_to_unix_cred failed %x\n",
				    gssstat);
				*ucred = NULL;
			} else {
				client_data->u_cred.gidlen = (short) gidlen;
				client_data->u_cred_set =
				hrestime.tv_sec + svc_rpcgss_gid_timeout;
			}
		    } else if (client_data->u_cred_set < hrestime.tv_sec) {
			if ((gssstat = kgss_get_group_info(
			    client_data->u_cred.uid,
			    &client_data->u_cred.gid,
			    &client_data->u_cred.gidlist,
			    &gidlen, CRED()->cr_uid)) != GSS_S_COMPLETE) {
				RPCGSS_LOG(1, "rpc_gss_getcred: "
				    "kgss_get_group_info failed %x\n",
				    gssstat);
				*ucred = NULL;
			} else {
				client_data->u_cred.gidlen = (short) gidlen;
				client_data->u_cred_set =
				    hrestime.tv_sec + svc_rpcgss_gid_timeout;
			}
		    }
		    mutex_exit(&client_data->clm);
		}
	}

	if (cookie != NULL)
		*cookie = client_data->cookie;
	req->rq_xprt->xp_cookie = client_data->cookie;

	return (TRUE);
}

/*
 * Server side authentication for RPCSEC_GSS.
 */
enum auth_stat
__svcrpcsec_gss(rqst, msg, no_dispatch)
	struct svc_req		*rqst;
	struct rpc_msg		*msg;
	bool_t			*no_dispatch;
{
	XDR			xdrs;
	rpc_gss_creds		creds;
	rpc_gss_init_arg	call_arg;
	rpc_gss_init_res	call_res;
	gss_buffer_desc		output_token, process_token;
	gss_cred_id_t		server_creds;
	OM_uint32		gssstat, minor, minor_stat, time_rec;
	struct opaque_auth	*cred;
	svc_rpc_gss_data	*client_data;
	int			ret_flags, ret;
	svc_creds_list_t	*sc;
	svc_rpc_gss_parms_t	*gss_parms;

	/*
	 * Initialize response verifier to NULL verifier.  If
	 * necessary, this will be changed later.
	 */
	rqst->rq_xprt->xp_verf.oa_flavor = AUTH_NONE;
	rqst->rq_xprt->xp_verf.oa_base = NULL;
	rqst->rq_xprt->xp_verf.oa_length = 0;

	/*
	 * Need to null out results to start with.
	 */
	bzero((char *)&call_res, sizeof (call_res));

	/*
	 * Pull out and check credential and verifier.
	 */
	cred = &msg->rm_call.cb_cred;

	if (cred->oa_length == 0) {
		RPCGSS_LOG0(1, "_svcrpcsec_gss: zero length cred\n");
		ret = AUTH_BADCRED;
		goto error;
	}

	xdrmem_create(&xdrs, cred->oa_base, cred->oa_length, XDR_DECODE);
	bzero((char *)&creds, sizeof (creds));
	if (!__xdr_rpc_gss_creds(&xdrs, &creds)) {
		XDR_DESTROY(&xdrs);
		RPCGSS_LOG0(1, "_svcrpcsec_gss: can't decode creds\n");
		ret = AUTH_BADCRED;
		goto error;
	}
	XDR_DESTROY(&xdrs);

	/*
	 * Only accept valid values for service parameter
	 */
	switch (creds.service) {
	case rpc_gss_svc_none:
	case rpc_gss_svc_integrity:
	case rpc_gss_svc_privacy:
		break;
	default:
		RPCGSS_LOG(1, "_svcrpcsec_gss: unknown service type: 0x%x\n",
			creds.service);
		ret = AUTH_BADCRED;
		goto error;
	}

	/*
	 * If this is a control message and proc is GSSAPI_INIT, then
	 * create a client handle for this client.  Otherwise, look up
	 * the existing handle.
	 */
	if (creds.gss_proc == RPCSEC_GSS_INIT) {
		if (creds.ctx_handle.length != 0) {
			RPCGSS_LOG0(1, "_svcrpcsec_gss: ctx_handle not null\n");
			ret = AUTH_BADCRED;
			goto error;
		}
		if ((client_data = create_client()) == NULL) {
			RPCGSS_LOG0(1,
			"_svcrpcsec_gss: can't create a new cache entry\n");
			ret = AUTH_FAILED;
			goto error;
		}
	} else {
		if (creds.ctx_handle.length == 0) {
			RPCGSS_LOG0(1, "_svcrpcsec_gss: no ctx_handle\n");
			ret = AUTH_BADCRED;
			goto error;
		}
		if ((client_data = get_client(&creds.ctx_handle)) == NULL) {
			ret = RPCSEC_GSS_NOCRED;
			RPCGSS_LOG0(1, "_svcrpcsec_gss: no security context\n");
			goto error;
		}
	}

	/*
	 * lock the client data until it's safe; if it's already stale,
	 * no more processing is possible
	 */
	mutex_enter(&client_data->clm);
	if (client_data->stale) {
		ret = RPCSEC_GSS_NOCRED;
		RPCGSS_LOG0(1, "_svcrpcsec_gss: client data stale\n");
		goto error2;
	}

	/*
	 * Any response we send will use ctx_handle, so set it now;
	 * also set seq_window since this won't change.
	 */
	call_res.ctx_handle.length = sizeof (client_data->key);
	call_res.ctx_handle.value = (char *)&client_data->key;
	call_res.seq_window = SEQ_WIN;

	/*
	 * Set the appropriate wrap/unwrap routine for RPCSEC_GSS.
	 */
	rqst->rq_xprt->xp_auth.svc_ah_ops = svc_rpc_gss_ops;
	rqst->rq_xprt->xp_auth.svc_ah_private = (caddr_t)client_data;

	/*
	 * Keep copy of parameters we'll need for response, for the
	 * sake of reentrancy (we don't want to look in the context
	 * data because when we are sending a response, another
	 * request may have come in.
	 */
	gss_parms = &rqst->rq_xprt->xp_auth.svc_gss_parms;
	gss_parms->established = client_data->established;
	gss_parms->service = creds.service;
	gss_parms->qop_rcvd = (u_int)client_data->qop;
	gss_parms->context = (void *)client_data->context;
	gss_parms->seq_num = creds.seq_num;

	if (!client_data->established) {
		if (creds.gss_proc == RPCSEC_GSS_DATA) {
			RPCGSS_LOG0(1, "_svcrpcsec_gss: data exchange "
				"message but context not established\n");

			ret = RPCSEC_GSS_FAILED;
			client_data->stale = TRUE;
			goto error2;
		}

		/*
		 * If the context is not established, then only
		 * RPCSEC_GSS_INIT and RPCSEC_GSS_CONTINUE_INIT
		 * requests are valid.
		 */
		if (creds.gss_proc != RPCSEC_GSS_INIT && creds.gss_proc !=
						RPCSEC_GSS_CONTINUE_INIT) {
			RPCGSS_LOG(1, "_svcrpcsec_gss: not an INIT or "
				"CONTINUE_INIT message (0x%x) and context not "
				"established\n", creds.gss_proc);

			ret = RPCSEC_GSS_FAILED;
			client_data->stale = TRUE;
			goto error2;
		}

		/*
		 * call is for us, deserialize arguments
		 */
		bzero(&call_arg, sizeof (call_arg));
		if (!svc_getargs(rqst->rq_xprt, __xdr_rpc_gss_init_arg,
							(caddr_t)&call_arg)) {
			RPCGSS_LOG0(1, "_svcrpcsec_gss: svc_getargs failed\n");
			ret = RPCSEC_GSS_FAILED;
			client_data->stale = TRUE;
			goto error2;
		}

		/*
		 * If the client's server_creds is already set, use it.
		 * Otherwise, try each server credential in svc_creds_list
		 * until one of them succeeds.
		 */
		gssstat = GSS_S_FAILURE;
		minor = 0;
		minor_stat = 0;
		rw_enter(&cred_lock, RW_READER);
		for (sc = svc_creds_list; sc != NULL; sc = sc->next) {
			if (rqst->rq_prog != sc->program ||
					rqst->rq_vers != sc->version)
				continue;
			if (client_data->server_creds != NULL)
				server_creds = client_data->server_creds;
			else {
				if (sc->expiration != GSS_C_INDEFINITE &&
					sc->expiration <= hrestime.tv_sec) {
					if (!rpc_gss_refresh_svc_cred(sc))
						continue;
				}
				server_creds = sc->cred;
			}

			if (client_data->client_name.length) {
				(void) gss_release_buffer(&minor,
					&client_data->client_name);
			}

			gssstat = kgss_accept_sec_context(&minor_stat,
					&client_data->context,
					server_creds,
					&call_arg,
					GSS_C_NO_CHANNEL_BINDINGS,
					&client_data->client_name,
					NULL,
					&output_token,
					&ret_flags,
					&time_rec,
					/*
					 * Don't need a delegated cred back.
					 * No memory will be allocated if
					 * passing NULL.
					 */
					NULL,
					CRED()->cr_uid);

			RPCGSS_LOG(4, "_svc_rpcsec_gss: "
			    "kgss_accept_sec_context: mech 0x%p,",
			    (void *)sc->mech);
			RPCGSS_LOG(4, " gssstat 0x%x\n", gssstat);

			if (gssstat == GSS_S_DEFECTIVE_CREDENTIAL) {
				sc->expiration = 0;
				if (!rpc_gss_refresh_svc_cred(sc))
					continue;
				server_creds = sc->cred;

				gssstat = kgss_accept_sec_context(&minor_stat,
					&client_data->context,
					server_creds,
					&call_arg,
					GSS_C_NO_CHANNEL_BINDINGS,
					&client_data->client_name,
					NULL,
					&output_token,
					&ret_flags,
					&time_rec,
					NULL,
					CRED()->cr_uid);
			}

			if (server_creds == client_data->server_creds)
				break;

			if (gssstat == GSS_S_COMPLETE) {
				/*
				 * Server_creds was right - set it.  Also
				 * set the raw and unix credentials at this
				 * point.  This saves a lot of computation
				 * later when credentials are retrieved.
				 */
				client_data->server_creds = server_creds;
				client_data->raw_cred.version = creds.version;
				client_data->raw_cred.service = creds.service;
				client_data->raw_cred.svc_principal =
					sc->service_name;
				client_data->raw_cred.mechanism =
					(rpc_gss_OID) sc->mech;

				/*
				 *  The client_name returned from
				 *  kgss_accept_sec_context() is in an
				 *  exported flat format.
				 */
				if (! __rpc_gss_make_principal(
				    &client_data->raw_cred.client_principal,
				    &client_data->client_name)) {
					RPCGSS_LOG0(1, "_svcrpcsec_gss: "
					    "make principal failed\n");
					gssstat = GSS_S_FAILURE;
					minor_stat = 0;
				}
				break;
			}

			if (gssstat == GSS_S_CONTINUE_NEEDED)
				break;
		}
		rw_exit(&cred_lock);

		call_res.gss_major = gssstat;
		call_res.gss_minor = minor_stat;

		xdr_free(__xdr_rpc_gss_init_arg, (caddr_t)&call_arg);

		if (gssstat != GSS_S_COMPLETE &&
					gssstat != GSS_S_CONTINUE_NEEDED) {
			/*
			 * We have a failure - send response and delete
			 * the context.  Don't dispatch.  Set ctx_handle
			 * to NULL and seq_window to 0.
			 */
			call_res.ctx_handle.length = 0;
			call_res.ctx_handle.value = NULL;
			call_res.seq_window = 0;
			RPCGSS_LOG(1, "_svcrpcsec_gss: context create failure:"
				" 0x%x\n", gssstat);
			(void) svc_sendreply(rqst->rq_xprt,
				__xdr_rpc_gss_init_res, (caddr_t)&call_res);
			*no_dispatch = TRUE;
			client_data->stale = TRUE;
			ret = AUTH_OK;
			goto error2;
		}

		/*
		 * This step succeeded.  Send a response, along with
		 * a token if there's one.  Don't dispatch.
		 */
		if (output_token.length != 0) {
			GSS_COPY_BUFFER(call_res.token, output_token);
		}

		/*
		 * If GSS_S_COMPLETE: set response verifier to
		 * checksum of SEQ_WIN
		 */
		if (gssstat == GSS_S_COMPLETE) {
		    if (!set_response_verf(rqst, msg, client_data,
				(u_int) SEQ_WIN)) {
			ret = RPCSEC_GSS_FAILED;
			client_data->stale = TRUE;
			RPCGSS_LOG0(1,
			"_svc_rpcsec_gss:set response verifier failed\n");
			goto error2;
		    }
		}

		(void) svc_sendreply(rqst->rq_xprt, __xdr_rpc_gss_init_res,
							(caddr_t)&call_res);
		*no_dispatch = TRUE;
		ASSERT(client_data->ref_cnt > 0);
		client_data->ref_cnt--;
		(void) gss_release_buffer(&minor_stat, &output_token);

		/*
		 * If appropriate, set established to TRUE *after* sending
		 * response (otherwise, the client will receive the final
		 * token encrypted)
		 */
		if (gssstat == GSS_S_COMPLETE) {
			/*
			 * Context is established.  Set expiration time
			 * for the context.
			 */
			client_data->seq_num = 1;
			if ((time_rec == GSS_C_INDEFINITE) || (time_rec == 0)) {
				client_data->expiration = GSS_C_INDEFINITE;
			} else {
				client_data->expiration =
					time_rec + hrestime.tv_sec;
				do_sweep_clients = TRUE;
			}
			client_data->established = TRUE;
			/*
			 * Now call kgss_export_sec_context
			 * if an error is returned log a message
			 * go to error handling
			 * Otherwise call kgss_import_sec_context to
			 * convert the token into a context
			 */
			gssstat  = kgss_export_sec_context(&minor_stat,
						client_data->context,
						&process_token);
			/*
			 * if export_sec_context returns an error we delete the
			 * context just to be safe.
			 */
			if (gssstat == GSS_S_NAME_NOT_MN) {
				RPCGSS_LOG0(4, "svc_rpcsec_gss: "
				"export sec context Kernel mod unavailable\n");
				goto success;
			} else if (gssstat != GSS_S_COMPLETE) {
				RPCGSS_LOG(1, "svc_rpcsec_gss: "
				"export sec context failed   gssstat = 0x%x\n",
					gssstat);
				(void) gss_release_buffer(&minor_stat,
							&process_token);
				goto success;
			} else if (process_token.length == 0) {
				RPCGSS_LOG0(1, "svc_rpcsec_gss:zero length "
						"token in response for "
						"export_sec_context, but "
						"gsstat == GSS_S_COMPLETE\n");
				(void) kgss_delete_sec_context(&minor,
						&client_data->context, NULL);
				goto error2;
			} else {
				gssstat = kgss_import_sec_context(&minor_stat,
							&process_token,
							client_data->context);
				if (gssstat != GSS_S_COMPLETE) {
					RPCGSS_LOG(1, "svc_rpcsec_gss:"
						"import sec context failed "
						"gssstat = 0x%x\n",
						gssstat);
					(void) kgss_delete_sec_context(&minor,
						&client_data->context, NULL);

					(void) gss_release_buffer(&minor_stat,
						&process_token);
					goto error2;
				}

				RPCGSS_LOG0(4, "gss_import_sec_context "
					"successful\n");
				(void) gss_release_buffer(&minor_stat,
					&process_token);
			}
		}
	} else {
		/*
		 * Context is already established.  Check verifier, and
		 * note parameters we will need for response in gss_parms.
		 */
		if (!check_verf(msg, client_data->context,
			(int *)&gss_parms->qop_rcvd, client_data->u_cred.uid)) {
			ret = RPCSEC_GSS_NOCRED;
			RPCGSS_LOG0(1, "_svcrpcsec_gss: check verf failed\n");
			goto error2;
		}

		/*
		 *  Check and invoke callback if necessary.
		 */
		if (!client_data->done_docallback) {
			client_data->done_docallback = TRUE;
			client_data->qop = gss_parms->qop_rcvd;
			client_data->raw_cred.qop = gss_parms->qop_rcvd;
			client_data->raw_cred.service = creds.service;
			if (!do_callback(rqst, client_data)) {
				ret = AUTH_FAILED;
				*no_dispatch = TRUE;
				client_data->stale = TRUE;
				RPCGSS_LOG0(1,
					"_svc_rpcsec_gss:callback failed\n");
				goto error2;
			}
		}

		/*
		 * If the context was locked, make sure that the client
		 * has not changed QOP.
		 */
		if (client_data->locked &&
				gss_parms->qop_rcvd != client_data->qop) {
			ret = AUTH_BADVERF;
			RPCGSS_LOG0(1, "_svcrpcsec_gss: can not change qop\n");
			goto error2;
		}

		/*
		 * Validate sequence number.
		 */
		if (!check_seq(client_data, creds.seq_num,
						&client_data->stale)) {
			if (client_data->stale) {
				ret = RPCSEC_GSS_FAILED;
				RPCGSS_LOG0(1,
					"_svc_rpcsec_gss:check seq failed\n");
			} else {
				RPCGSS_LOG0(4, "_svc_rpcsec_gss:check seq "
					"failed on good context. Ignoring "
					"request\n");
				/*
				 * Operational error, drop packet silently.
				 * The client will recover after timing out,
				 * assuming this is a client error and not
				 * a relpay attack.  Don't dispatch.
				 */
				ret = AUTH_OK;
				*no_dispatch = TRUE;
			}
			goto error2;
		}

		/*
		 * set response verifier
		 */
		if (!set_response_verf(rqst, msg, client_data,
				creds.seq_num)) {
			ret = RPCSEC_GSS_FAILED;
			client_data->stale = TRUE;
			RPCGSS_LOG0(1,
			"_svc_rpcsec_gss:set response verifier failed\n");
			goto error2;
		}

		/*
		 * If control message, process the call; otherwise, return
		 * AUTH_OK so it will be dispatched to the application server.
		 */
		if (creds.gss_proc != RPCSEC_GSS_DATA) {
			switch (creds.gss_proc) {
			/*
			 * XXX Kernel client is not issuing this procudure
			 * right now. Need to revisit.
			 */
			case RPCSEC_GSS_DESTROY:
				(void) svc_sendreply(rqst->rq_xprt, xdr_void,
					NULL);
				*no_dispatch = TRUE;
				ASSERT(client_data->ref_cnt > 0);
				client_data->ref_cnt--;
				client_data->stale = TRUE;
				break;

			default:
				RPCGSS_LOG0(1,
					"_svc_rpcsec_gss:check seq failed\n");
				ret = AUTH_FAILED;
				goto error2;
			}
		} else {
			/*
			 * If context is locked, make sure that the client
			 * has not changed the security service.
			 */
			if (client_data->locked &&
			    client_data->raw_cred.service != creds.service) {
				RPCGSS_LOG0(1, "_svc_rpcsec_gss: "
					"security service changed.\n");
				ret = AUTH_FAILED;
				goto error2;
			}

			/*
			 * Set client credentials to raw credential
			 * structure in context.  This is okay, since
			 * this will not change during the lifetime of
			 * the context (so it's MT safe).
			 */
			rqst->rq_clntcred = (char *)&client_data->raw_cred;
		}
	}

success:

	/*
	 * Success.
	 */
	if (creds.ctx_handle.length != 0)
		xdr_free(__xdr_rpc_gss_creds, (caddr_t)&creds);
	mutex_exit(&client_data->clm);

	return (AUTH_OK);
error2:
	ASSERT(client_data->ref_cnt > 0);
	client_data->ref_cnt--;
	mutex_exit(&client_data->clm);
error:
	/*
	 * Failure.
	 */
	if (creds.ctx_handle.length != 0)
		xdr_free(__xdr_rpc_gss_creds, (caddr_t)&creds);

	return (ret);
}

/*
 * Check verifier.  The verifier is the checksum of the RPC header
 * upto and including the credentials field.
 */

/* ARGSUSED */
static bool_t
check_verf(struct rpc_msg *msg, gss_ctx_id_t context, int *qop_state, uid_t uid)
{
	int			*buf, *tmp;
	char			hdr[128];
	struct opaque_auth	*oa;
	int			len;
	gss_buffer_desc		msg_buf;
	gss_buffer_desc		tok_buf;
	OM_uint32		gssstat, minor_stat;

	/*
	 * We have to reconstruct the RPC header from the previously
	 * parsed information, since we haven't kept the header intact.
	 */
	buf = (int *)hdr;
	IXDR_PUT_U_INT32(buf, msg->rm_xid);
	IXDR_PUT_ENUM(buf, msg->rm_direction);
	IXDR_PUT_U_INT32(buf, msg->rm_call.cb_rpcvers);
	IXDR_PUT_U_INT32(buf, msg->rm_call.cb_prog);
	IXDR_PUT_U_INT32(buf, msg->rm_call.cb_vers);
	IXDR_PUT_U_INT32(buf, msg->rm_call.cb_proc);
	oa = &msg->rm_call.cb_cred;
	IXDR_PUT_ENUM(buf, oa->oa_flavor);
	IXDR_PUT_U_INT32(buf, oa->oa_length);
	if (oa->oa_length) {
		len = RNDUP(oa->oa_length);
		tmp = buf;
		buf += len / sizeof (int);
		*(buf - 1) = 0;
		(void) bcopy(oa->oa_base, (caddr_t)tmp, oa->oa_length);
	}
	len = ((char *)buf) - hdr;
	msg_buf.length = len;
	msg_buf.value = hdr;
	oa = &msg->rm_call.cb_verf;
	tok_buf.length = oa->oa_length;
	tok_buf.value = oa->oa_base;

	gssstat = kgss_verify(&minor_stat, context, &msg_buf, &tok_buf,
				qop_state);
	if (gssstat != GSS_S_COMPLETE) {
		RPCGSS_LOG(1, "check_verf: kgss_verify status 0x%x\n", gssstat);

		RPCGSS_LOG(4, "check_verf: msg_buf length %d\n", len);
		RPCGSS_LOG(4, "check_verf: msg_buf value 0x%x\n", *(int *)hdr);
		RPCGSS_LOG(4, "check_verf: tok_buf length %ld\n",
				tok_buf.length);
		RPCGSS_LOG(4, "check_verf: tok_buf value 0x%p\n",
			(void *)oa->oa_base);
		RPCGSS_LOG(4, "check_verf: context 0x%p\n", context);

		return (FALSE);
	}
	return (TRUE);
}

/*
 * Set response verifier.  This is the checksum of the given number.
 * (e.g. sequence number or sequence window)
 */
static bool_t
set_response_verf(rqst, msg, cl, num)
	struct svc_req		*rqst;
	struct rpc_msg		*msg;
	svc_rpc_gss_data	*cl;
	u_int			num;
{
	OM_uint32		minor;
	gss_buffer_desc		in_buf, out_buf;
	u_int			num_net;

	num_net = (u_int) htonl(num);
	in_buf.length = sizeof (num);
	in_buf.value = (char *)&num_net;
/* XXX uid ? */
	if ((kgss_sign(&minor, cl->context, cl->qop, &in_buf,
				&out_buf)) != GSS_S_COMPLETE)
		return (FALSE);

	rqst->rq_xprt->xp_verf.oa_flavor = RPCSEC_GSS;
	rqst->rq_xprt->xp_verf.oa_base = msg->rm_call.cb_verf.oa_base;
	rqst->rq_xprt->xp_verf.oa_length = out_buf.length;
	bcopy(out_buf.value, rqst->rq_xprt->xp_verf.oa_base, out_buf.length);
	(void) gss_release_buffer(&minor, &out_buf);
	return (TRUE);
}

/*
 * Create client context.
 */
static svc_rpc_gss_data *
create_client()
{
	svc_rpc_gss_data	*client_data;
	static u_int		key = 1;

	client_data = (svc_rpc_gss_data *) kmem_alloc(sizeof (*client_data),
					KM_SLEEP);
	if (client_data == NULL)
		return (NULL);
	bzero((char *)client_data, sizeof (*client_data));

	/*
	 * set up client data structure
	 */
	client_data->established = FALSE;
	client_data->locked = FALSE;
	client_data->u_cred_set = 0;
	client_data->context = GSS_C_NO_CONTEXT;
	client_data->expiration = GSS_C_INDEFINITE;
	client_data->deleg = GSS_C_NO_CREDENTIAL;
	client_data->ref_cnt = 1;
	client_data->qop = GSS_C_QOP_DEFAULT;
	client_data->done_docallback = FALSE;
	client_data->stale = FALSE;
	bzero(&client_data->raw_cred, sizeof (client_data->raw_cred));
	mutex_init(&client_data->clm, "clm", USYNC_THREAD, NULL);
	/*
	 * Check totals.  If we've hit the limit, we destroy a context
	 * based on LRU method.
	 */
	mutex_enter(&ctx_mutex);
	if (num_gss_contexts >= max_gss_contexts) {
		/*
		 * now try on LRU basis
		 */
		drop_lru_client();
		if (num_gss_contexts >= max_gss_contexts) {
			RPCGSS_LOG(1, "NOTICE: _svcrpcsec_gss: "
				"drop_lru_client lru_last 0x%p\n",
				(void *)lru_last);
			RPCGSS_LOG(1, "NOTICE: _svcrpcsec_gss: "
				"drop_lru_client lru_last->ref_cnt %d\n",
				lru_last->ref_cnt);
			mutex_exit(&ctx_mutex);
			kmem_free((char *)client_data, sizeof (*client_data));
			return (NULL);
		}
	}

	/*
	 * The client context handle is a 32-bit key (unsigned int).
	 * The key is incremented until there is no duplicate for it.
	 */
	for (;;) {
		client_data->key = key++;
		if (find_client(client_data->key) == NULL) {
			insert_client(client_data);
			mutex_exit(&ctx_mutex);
			return (client_data);
		}
	}
	/*NOTREACHED*/
}

/*
 * Insert client context into hash list and LRU list.
 */
static void
insert_client(client_data)
	svc_rpc_gss_data	*client_data;
{
	svc_rpc_gss_data	*cl;
	int			index = (client_data->key & HASHMASK);

	client_data->prev = NULL;
	cl = clients[index];
	if ((client_data->next = cl) != NULL)
		cl->prev = client_data;
	clients[index] = client_data;

	client_data->lru_prev = NULL;
	if ((client_data->lru_next = lru_first) != NULL)
		lru_first->lru_prev = client_data;
	else
		lru_last = client_data;
	lru_first = client_data;

	num_gss_contexts++;
}

/*
 * Fetch a client, given the client context handle.  Move it to the
 * top of the LRU list since this is the most recently used context.
 */
static svc_rpc_gss_data *
get_client(ctx_handle)
	gss_buffer_t		ctx_handle;
{
	u_int			key = *(u_int *)ctx_handle->value;
	svc_rpc_gss_data	*cl;

	mutex_enter(&ctx_mutex);
	if ((cl = find_client(key)) != NULL) {
		mutex_enter(&cl->clm);
		if (cl->stale) {
			mutex_exit(&cl->clm);
			mutex_exit(&ctx_mutex);
			return (NULL);
		}
		cl->ref_cnt++;
		mutex_exit(&cl->clm);
		if (cl != lru_first) {
			cl->lru_prev->lru_next = cl->lru_next;
			if (cl->lru_next != NULL)
				cl->lru_next->lru_prev = cl->lru_prev;
			else
				lru_last = cl->lru_prev;
			cl->lru_prev = NULL;
			cl->lru_next = lru_first;
			lru_first->lru_prev = cl;
			lru_first = cl;
		}
	}
	mutex_exit(&ctx_mutex);
	return (cl);
}

/*
 * Given the client context handle, find the context corresponding to it.
 * Don't change its LRU state since it may not be used.
 */
static svc_rpc_gss_data *
find_client(key)
	u_int			key;
{
	int			index = (key & HASHMASK);
	svc_rpc_gss_data	*cl = NULL;

	for (cl = clients[index]; cl != NULL; cl = cl->next) {
		if (cl->key == key)
			break;
	}
	return (cl);
}

/*
 * Destroy a client context.
 */
static void
destroy_client(client_data)
	svc_rpc_gss_data	*client_data;
{
	OM_uint32		minor;
	int			index = (client_data->key & HASHMASK);

	/*
	 * remove from hash list
	 */
	if (client_data->prev == NULL)
		clients[index] = client_data->next;
	else
		client_data->prev->next = client_data->next;
	if (client_data->next != NULL)
		client_data->next->prev = client_data->prev;

	/*
	 * remove from LRU list
	 */
	if (client_data->lru_prev == NULL)
		lru_first = client_data->lru_next;
	else
		client_data->lru_prev->lru_next = client_data->lru_next;
	if (client_data->lru_next != NULL)
		client_data->lru_next->lru_prev = client_data->lru_prev;
	else
		lru_last = client_data->lru_prev;

	/*
	 * If there is a GSS context, clean up GSS state.
	 */
	if (client_data->context != GSS_C_NO_CONTEXT) {
		(void) kgss_delete_sec_context(&minor, &client_data->context,
					NULL);
		if (client_data->client_name.length > 0) {
		    (void) gss_release_buffer(&minor,
				&client_data->client_name);
		}
		if (client_data->raw_cred.client_principal) {
		    kmem_free((caddr_t) client_data->raw_cred.client_principal,
			client_data->raw_cred.client_principal->len +
			sizeof (int));
		}
		if (client_data->u_cred.gidlist != NULL) {
		    kmem_free((char *)client_data->u_cred.gidlist,
				client_data->u_cred.gidlen * sizeof (gid_t));
		}
		if (client_data->deleg != GSS_C_NO_CREDENTIAL) {
		    (void) kgss_release_cred(&minor, &client_data->deleg,
				CRED()->cr_uid);
		}
	}
	kmem_free(client_data, sizeof (*client_data));
	num_gss_contexts--;
}

/*
 * Check for expired client contexts.
 */
static void
sweep_clients()
{
	svc_rpc_gss_data	*cl, *next;
	int			index;

	for (index = 0; index < HASHMOD; index++) {
		cl = clients[index];
		while (cl) {
			next = cl->next;
			mutex_enter(&cl->clm);
			if ((cl->expiration != GSS_C_INDEFINITE &&
			    cl->expiration <= hrestime.tv_sec) || cl->stale) {
				cl->stale = TRUE;
				mutex_exit(&cl->clm);
				if (cl->ref_cnt == 0)
					destroy_client(cl);
			} else
				mutex_exit(&cl->clm);
			cl = next;
		}
	}
	last_swept = hrestime.tv_sec;
}

/*
 * Drop the least recently used client context, if possible.
 */
static void
drop_lru_client()
{
	mutex_enter(&lru_last->clm);
	lru_last->stale = TRUE;
	mutex_exit(&lru_last->clm);
	if (lru_last->ref_cnt == 0) {
		destroy_client(lru_last);
	} else {
		sweep_clients();
	}
}

/*
 *  Check if there is already a svc_cred entry in the svc_creds_list.
 *
 *  Return TRUE if find it.
 *  Return FALSE if not found.
 */
svc_creds_list_t *
find_svc_cred(service_name, mech, program, version)
	char		*service_name;
	rpc_gss_OID	mech;
	u_int		program;
	u_int		version;
{
	svc_creds_list_t	*sc;

	if (!svc_creds_list)
		return (NULL);

	for (sc = svc_creds_list; sc != NULL; sc = sc->next) {
	    if (program != sc->program || version != sc->version)
		continue;

	    if (bcmp(service_name, sc->service_name, strlen(service_name)) != 0)
		continue;

	    if (mech->length && (mech->length == sc->mech->length)) {
		if (bcmp(mech->elements, sc->mech->elements, mech->length) == 0)
			return (sc);
	    }
	}
	return (NULL);
}

/*
 * Set the service principal name.
 * Memory allocated in this routine is not freed.
 * If the user uses it with care, the memory leak can be tolerated.
 */
bool_t
rpc_gss_set_svc_name(principal, mechanism, req_time, program, version)
	char			*principal;
	rpc_gss_OID		mechanism;
	u_int			req_time;
	u_int			program;
	u_int			version;
{
	gss_name_t		name;
	gss_cred_id_t		cred;
	svc_creds_list_t	*svc_cred;
	gss_OID_set_desc	oid_set;
	OM_uint32		ret_time;
	OM_uint32		major, minor;
	gss_buffer_desc		name_buf;

	oid_set.count = 1;
	oid_set.elements = (gss_OID) mechanism;

	name_buf.value = principal;
	name_buf.length = strlen(principal) + 1;
	major = gss_import_name(&minor, &name_buf,
			(gss_OID)GSS_C_NT_HOSTBASED_SERVICE, &name);

	if (major != GSS_S_COMPLETE) {
		RPCGSS_LOG(1, "rpc_gss_set_svc_name: import name failed 0x%x\n",
			major);
		return (FALSE);
	}

	/* XXX if found in the cache, do kgss_inquire_cred would be better */
	major = kgss_acquire_cred(&minor, name, req_time, &oid_set,
			GSS_C_ACCEPT, &cred, NULL, &ret_time, CRED()->cr_uid);

	if (major != GSS_S_COMPLETE) {
		RPCGSS_LOG(1, "rpc_gss_set_svc_name: "
			"acquire cred failed 0x%x\n", major);
		(void) gss_release_name(&minor, &name);
		return (FALSE);
	}

	/* Check if there is already an entry in the svc_creds_list. */
	rw_enter(&cred_lock, RW_WRITER);
	if (svc_cred = find_svc_cred(principal, mechanism, program, version)) {
		/* free existing cred before assign a new cred value */
		(void) kgss_release_cred(&minor, &svc_cred->cred,
					CRED()->cr_uid);
		svc_cred->cred = cred;
		svc_cred->expiration =
			((ret_time == GSS_C_INDEFINITE) || (ret_time == 0)) ?
			GSS_C_INDEFINITE : ret_time + hrestime.tv_sec;
		svc_cred->req_time = req_time;
		rw_exit(&cred_lock);
		(void) gss_release_name(&minor, &name);
		return (TRUE);
	}
	rw_exit(&cred_lock);

	/* create a new svc_cred entry */
	svc_cred = (svc_creds_list_t *) kmem_alloc(sizeof (*svc_cred),
						KM_SLEEP);
	if (svc_cred == NULL) {
		(void) kgss_release_cred(&minor, &cred, CRED()->cr_uid);
		(void) gss_release_name(&minor, &name);
		return (FALSE);
	}

	svc_cred->cred = cred;
	svc_cred->name = name;
	svc_cred->program = program;
	svc_cred->version = version;
	svc_cred->expiration =
		((ret_time == GSS_C_INDEFINITE) || (ret_time == 0)) ?
		GSS_C_INDEFINITE : ret_time + hrestime.tv_sec;
	svc_cred->req_time = req_time;
	__rpc_gss_dup_oid((gss_OID) mechanism, &oid_set.elements);
	svc_cred->oid_set = oid_set;
	svc_cred->service_name = kmem_alloc(strlen(principal) + 1, KM_SLEEP);
	if (svc_cred->service_name == NULL) {
		(void) kgss_release_cred(&minor, &cred, CRED()->cr_uid);
		(void) gss_release_name(&minor, &name);
		kmem_free((char *)svc_cred, sizeof (*svc_cred));
		return (FALSE);
	}
	bcopy(principal, (char *) svc_cred->service_name,
			strlen(principal) + 1);
	svc_cred->mech = oid_set.elements;
	mutex_init(&svc_cred->refresh_mutex, "refresh_mutex", USYNC_THREAD,
			NULL);

	rw_enter(&cred_lock, RW_WRITER);
	svc_cred->next = svc_creds_list;
	svc_creds_list = svc_cred;
	svc_creds_count++;
	rw_exit(&cred_lock);

	return (TRUE);
}

/*
 * Refresh server credentials.
 */
static bool_t
rpc_gss_refresh_svc_cred(svc_cred)
	svc_creds_list_t	*svc_cred;
{
	OM_uint32		major, minor;
	OM_uint32		ret_time;

	RPCGSS_LOG(4, "rpc_gss_refresh_svc_cred: svc_cred 0x%p\n",
		(void *)svc_cred);
	mutex_enter(&svc_cred->refresh_mutex);
	if (svc_cred->expiration > hrestime.tv_sec) {
		mutex_exit(&svc_cred->refresh_mutex);
		return (TRUE);
	}
	(void) kgss_release_cred(&minor, &svc_cred->cred, CRED()->cr_uid);
	svc_cred->cred = GSS_C_NO_CREDENTIAL;
	major = kgss_acquire_cred(&minor, svc_cred->name, svc_cred->req_time,
		&svc_cred->oid_set, GSS_C_ACCEPT, &svc_cred->cred, NULL,
		&ret_time, CRED()->cr_uid);
	if (major != GSS_S_COMPLETE) {
		mutex_exit(&svc_cred->refresh_mutex);
		return (FALSE);
	}

	svc_cred->expiration =
		((ret_time == GSS_C_INDEFINITE) || (ret_time == 0)) ?
		GSS_C_INDEFINITE : ret_time + hrestime.tv_sec;
	mutex_exit(&svc_cred->refresh_mutex);
	return (TRUE);
}

/*
 * Encrypt the serialized arguments from xdr_func applied to xdr_ptr
 * and write the result to xdrs.
 */
static bool_t
svc_rpc_gss_wrap(auth, out_xdrs, xdr_func, xdr_ptr)
	SVCAUTH			*auth;
	XDR			*out_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	svc_rpc_gss_parms_t	*gss_parms = SVCAUTH_GSSPARMS(auth);

	/*
	 * If context is not established, or if neither integrity nor
	 * privacy service is used, don't wrap - just XDR encode.
	 * Otherwise, wrap data using service and QOP parameters.
	 */
	if (!gss_parms->established ||
				gss_parms->service == rpc_gss_svc_none)
		return ((*xdr_func)(out_xdrs, xdr_ptr));

	return (__rpc_gss_wrap_data(gss_parms->service,
				(OM_uint32)gss_parms->qop_rcvd,
				(gss_ctx_id_t)gss_parms->context,
				gss_parms->seq_num,
				out_xdrs, xdr_func, xdr_ptr));
}

/*
 * Decrypt the serialized arguments and XDR decode them.
 */
static bool_t
svc_rpc_gss_unwrap(auth, in_xdrs, xdr_func, xdr_ptr)
	SVCAUTH			*auth;
	XDR			*in_xdrs;
	bool_t			(*xdr_func)();
	caddr_t			xdr_ptr;
{
	svc_rpc_gss_parms_t	*gss_parms = SVCAUTH_GSSPARMS(auth);

	/*
	 * If context is not established, or if neither integrity nor
	 * privacy service is used, don't unwrap - just XDR decode.
	 * Otherwise, unwrap data.
	 */
	if (!gss_parms->established ||
				gss_parms->service == rpc_gss_svc_none)
		return ((*xdr_func)(in_xdrs, xdr_ptr));

	return (__rpc_gss_unwrap_data(gss_parms->service,
				(gss_ctx_id_t)gss_parms->context,
				gss_parms->seq_num,
				gss_parms->qop_rcvd,
				in_xdrs, xdr_func, xdr_ptr));
}


/* ARGSUSED */
int
rpc_gss_svc_max_data_length(struct svc_req *req, int max_tp_unit_len)
{
	return (0);
}
