#pragma ident	"@(#)kerb_krpc.c	1.19	97/08/12 SMI"
/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*
 *  Routines for kernel kerberos implementation to talk to usermode
 *  kerb daemon.
 *  This file is not needed in userland.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <rpc/rpc.h>
#include <rpc/kerbd_prot.h>

#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/pathname.h>
#include <sys/debug.h>
#include <sys/utsname.h>
#include <sys/cmn_err.h>

#define	KERB_TIMEOUT	30	/* per-try timeout in seconds */
#define	KERB_NRETRY	6	/* number of retries */

static struct timeval trytimeout = { KERB_TIMEOUT, 0 };
static enum clnt_stat kerb_call(rpcproc_t, bool_t (*)(), char *,
    bool_t (*)(), char *);

extern kmutex_t kerbrpcb_lock;

/*
 *  Called by client rpc to set up authenticator for the requested
 *  service.  Returns ticket and kerberos authenticator information.
 */
int
kerb_mkcred(char *service, char *inst, char *realm, uint32_t cksum,
	KTEXT ticket, des_block *pkey, enum clnt_stat *rpcstat)
{
	ksetkcred_arg    ska;
	ksetkcred_res    skr;
	ksetkcred_resd  *skd = &skr.ksetkcred_res_u.res;
	enum clnt_stat	stat;

	ska.sname = service;
	ska.sinst = inst;
	ska.srealm = realm;
	ska.cksum = cksum;

	bzero((char *)&skr, sizeof (skr));
	stat = kerb_call((rpcproc_t)KSETKCRED, xdr_ksetkcred_arg,
	    (char *)&ska, xdr_ksetkcred_res, (char *)&skr);
	if (rpcstat)
		*rpcstat = stat;

	if (stat != RPC_SUCCESS)
		return (-1);

	if (skr.status == KSUCCESS) {
		bzero((char *)ticket, sizeof (*ticket));
		ticket->length = skd->ticket.TICKET_len;
		bcopy(skd->ticket.TICKET_val, ticket->dat, ticket->length);
		ticket->mbz = 0;

		*pkey = skd->key;
	}
	return (skr.status);
}

/*
 *  Called by server rpc to check the authenticator presented by the client.
 */
int
kerb_rdcred(KTEXT ticket, char *service, char *inst, uint32_t faddr,
	AUTH_DAT *kcred, enum clnt_stat *rpcstat)
{
	kgetkcred_arg    gka;
	kgetkcred_res    gkr;
	kgetkcred_resd  *gkd = &gkr.kgetkcred_res_u.res;
	enum clnt_stat	stat;

	gka.ticket.TICKET_len = ticket->length;
	gka.ticket.TICKET_val = (char *)ticket->dat;
	gka.sname = service;
	gka.sinst = inst;
	gka.faddr = faddr;

	bzero(&gkr, sizeof (gkr));
	stat = kerb_call((rpcproc_t)KGETKCRED, xdr_kgetkcred_arg,
	    (char *)&gka, xdr_kgetkcred_res, (char *)&gkr);
	if (rpcstat)
		*rpcstat = stat;

	if (stat != RPC_SUCCESS)
		return (-1);

	if (gkr.status == KSUCCESS) {
		(void) strncpy(inst, gkd->sinst, INST_SZ);
		kcred->k_flags = (u_char)gkd->k_flags;
		(void) strncpy(kcred->pname, gkd->pname, ANAME_SZ);
		(void) strncpy(kcred->pinst, gkd->pinst, INST_SZ);
		(void) strncpy(kcred->prealm, gkd->prealm, REALM_SZ);
		kcred->checksum = gkd->checksum;
		bcopy(&gkd->session, kcred->session, sizeof (des_block));
		kcred->life = gkd->life;
		kcred->time_sec = gkd->time_sec;
		kcred->address = gkd->address;

		bzero(&kcred->reply, sizeof (kcred->reply));
		kcred->reply.length = gkd->reply.TICKET_len;
		bcopy(gkd->reply.TICKET_val, kcred->reply.dat,
		    kcred->reply.length);
		kcred->reply.mbz = 0;
	}

	return (gkr.status);
}

/*
 *  Get the user's unix credentials.  Return 1 if cred ok, else 0 or -1.
 */
int
kerb_getpwnam(char *name, uid_t *uid, gid_t *gid, short *grouplen,
	int *groups, enum clnt_stat *rpcstat)
{
	kgetucred_arg	gua;
	kgetucred_res	gur;
	kerb_ucred	*ucred = &gur.kgetucred_res_u.cred;
	enum clnt_stat	stat;
	int		ret = 0;
	u_int		ngrps, *gp, *grpend;

	gua.pname = name;
	bzero(&gur, sizeof (gur));
	stat = kerb_call((rpcproc_t)KGETUCRED, xdr_kgetucred_arg,
	    (char *)&gua, xdr_kgetucred_res, (char *)&gur);
	if (rpcstat)
		*rpcstat = stat;

	if (stat != RPC_SUCCESS)
		return (-1);

	if (gur.status == UCRED_OK) {
		*uid = (uid_t)ucred->uid;
		*gid = (gid_t)ucred->gid;
		if ((ngrps = ucred->grplist.grplist_len) > 0) {
		    if (ngrps > ngroups_max)
			ngrps = ngroups_max;
		    gp =  ucred->grplist.grplist_val;
		    grpend = &ucred->grplist.grplist_val[ngrps];
		    while (gp < grpend)
			*groups++ = (int)*gp++;
		}
		*grouplen = (short)ngrps;
		ret = 1;
	}
	return (ret);
}

/*
 *  send request to usermode kerb daemon.
 *  returns RPC_SUCCESS if ok, else error stat.
 */
static enum clnt_stat
kerb_call(rpcproc_t procn, bool_t (*xdr_args)(), char *args,
	bool_t (*xdr_rslt)(), char *rslt)
{
	static struct knetconfig	config; /* avoid lookupname next time */
	static struct netbuf		netaddr = {0, 0, NULL};
	CLIENT				*client;
	enum clnt_stat			stat;
	struct vnode			*vp;
	int				error;
	char				*kerbname;
	int				tries = 0;
	struct netbuf			tmpaddr;

	/*
	 *  filch a knetconfig structure.
	 */
	if (config.knc_rdev == 0) {
		if ((error = lookupname("/dev/ticlts", UIO_SYSSPACE,
		    FOLLOW, NULLVPP, &vp)) != 0) {
			RPCLOG(1, "kerb_call: lookupname: %d\n", error);
			return (RPC_UNKNOWNPROTO);
		}
		config.knc_rdev = vp->v_rdev;
		config.knc_protofmly = loopback_name;
		VN_RELE(vp);
	}

	config.knc_semantics = NC_TPI_CLTS;

	/*
	 * Contact rpcbind to get kerbd's address only
	 * once and re-use the address.
	 */
	mutex_enter(&kerbrpcb_lock);
	if (netaddr.len == 0) {
retry_rpcbind:
		/* Set up netaddr to be <nodename>. */
		netaddr.len = (u_int) strlen(utsname.nodename) + 1;
		if (netaddr.buf != NULL)
			kmem_free(netaddr.buf, netaddr.maxlen);
		kerbname = kmem_zalloc(netaddr.len, KM_SLEEP);

		(void) strncpy(kerbname, utsname.nodename, netaddr.len-1);

		/* Append "." to end of kerbname */
		(void) strncpy(kerbname+(netaddr.len-1), ".", 1);
		netaddr.buf = kerbname;
		netaddr.maxlen = netaddr.len;

		/* Get address of kerbd from rpcbind */
		stat = rpcbind_getaddr(&config, KERBPROG, KERBVERS, &netaddr);
		if (stat != RPC_SUCCESS) {
			stat = RPC_RPCBFAILURE;
			kmem_free(netaddr.buf, netaddr.maxlen);
			netaddr.buf = NULL;
			netaddr.len = netaddr.maxlen = 0;
			mutex_exit(&kerbrpcb_lock);
			return (stat);
		}
	}

	/*
	 * Copy the netaddr information into a tmp location to
	 * be used by clnt_tli_kcreate.  The purpose of this
	 * is for MT race condition (ie. netaddr being modified
	 * while it is being used.
	 */
	tmpaddr.buf = kmem_zalloc(netaddr.maxlen, KM_SLEEP);
	bcopy(netaddr.buf, tmpaddr.buf, netaddr.maxlen);
	tmpaddr.maxlen = netaddr.maxlen;
	tmpaddr.len = netaddr.len;

	mutex_exit(&kerbrpcb_lock);

	RPCLOG(8, "kerb_call: procn %d, ", procn);
	RPCLOG(8, "rdev %lx, ", config.knc_rdev);
	RPCLOG(8, "len %d, ", netaddr.len);
	RPCLOG(8, "maxlen %d, ", netaddr.maxlen);
	RPCLOG(8, "name %p\n", (void *)netaddr.buf);

	/*
	 *  now call the proper stuff.
	 */
	error = clnt_tli_kcreate(&config, &tmpaddr, (rpcprog_t)KERBPROG,
	    (rpcvers_t)KERBVERS, 0, KERB_NRETRY, CRED(), &client);

	kmem_free(tmpaddr.buf, tmpaddr.maxlen);

	if (error != 0) {
		RPCLOG(1, "kerb_call: clnt_tli_kcreate: error %d", error);
		switch (error) {
		case EINTR:		return (RPC_INTR);
		case ETIMEDOUT:		return (RPC_TIMEDOUT);
		default:		return (RPC_FAILED);	/* XXX */
		}
	}

	auth_destroy(client->cl_auth);
	client->cl_auth = authloopback_create();
	if (client->cl_auth == NULL) {
		clnt_destroy(client);
		RPCLOG(1, "kerb_call: authloopback_create: error %d", EINTR);
		return (RPC_INTR);
	}

	stat = clnt_call(client, procn, (xdrproc_t)xdr_args, args,
	    (xdrproc_t)xdr_rslt, rslt, trytimeout);
	auth_destroy(client->cl_auth);
	clnt_destroy(client);
	if (stat != RPC_SUCCESS) {
		/*
		 * Just in case that kerbd may have been killed,
		 * re-contact rpcbind to get new address of kerbd.
		 */
		if (tries == 0) {
			tries++;
			mutex_enter(&kerbrpcb_lock);
			goto retry_rpcbind;
		}
		cmn_err(CE_WARN,
		    "kerb_call: can't contact kerbd: RPC stat %d (%s)",
		    stat, clnt_sperrno(stat));
		return (stat);
	}
	RPCLOG(8, "kerb call: (%d) ok\n", procn);
	return (RPC_SUCCESS);
}
