/*
 * Copyright (c) 1986-1991, 1997 by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)clnt_bcast.c	1.23	97/08/29 SMI"

/*
 * clnt_bcast.c
 * Client interface to broadcast service.
 *
 * Copyright (C) 1988, Sun Microsystems, Inc.
 *
 * The following is kludged-up support for simple rpc broadcasts.
 * Someday a large, complicated system will replace these routines.
 */

#include <string.h>
#include <rpc/rpc.h>
#include <rpc/trace.h>
#include <rpc/nettype.h>
#include <sys/poll.h>
#include <netdir.h>
#ifdef PORTMAP
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_rmt.h>
#endif
#ifdef RPC_DEBUG
#include <stdio.h>
#endif
#include <errno.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>

#define	MAXBCAST 20	/* Max no of broadcasting transports */
#define	INITTIME 4000	/* Time to wait initially */
#define	WAITTIME 8000	/* Maximum time to wait */

int lowvers = 1;	/* by default, broadcast only version 2 over UDP */
#ifndef NETIDLEN
#define	NETIDLEN 32
#endif

/*
 * If nettype is NULL, it broadcasts on all the available
 * datagram_n transports. May potentially lead to broadacst storms
 * and hence should be used with caution, care and courage.
 *
 * The current parameter xdr packet size is limited by the max tsdu
 * size of the transport. If the max tsdu size of any transport is
 * smaller than the parameter xdr packet, then broadcast is not
 * sent on that transport.
 *
 * Also, the packet size should be less the packet size of
 * the data link layer (for ethernet it is 1400 bytes).  There is
 * no easy way to find out the max size of the data link layer and
 * we are assuming that the args would be smaller than that.
 *
 * The result size has to be smaller than the transport tsdu size.
 *
 * If PORTMAP has been defined, we send two packets for UDP, one for
 * rpcbind and one for portmap. For those machines which support
 * both rpcbind and portmap, it will cause them to reply twice, and
 * also here it will get two responses ... inefficient and clumsy.
 */


enum clnt_stat
rpc_broadcast_exp(prog, vers, proc, xargs, argsp, xresults, resultsp,
	eachresult, inittime, waittime, netclass)
	rpcprog_t	prog;		/* program number */
	rpcvers_t	vers;		/* version number */
	rpcproc_t	proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	resultproc_t	eachresult;	/* call with each result obtained */
	int 		inittime;	/* how long to wait initially */
	int 		waittime;	/* maximum time to wait */
	const char	*netclass;	/* transport type */
{
	enum clnt_stat	stat = RPC_SUCCESS; /* Return status */
	XDR 		xdr_stream; /* XDR stream */
	register XDR 	*xdrs = &xdr_stream;
	struct rpc_msg	msg;	/* RPC message */
	struct timeval	t;
	char 		*outbuf = NULL;	/* Broadcast msg buffer */
	char		*inbuf = NULL; /* Reply buf */
	u_int		maxbufsize = 0;
	AUTH 		*sys_auth = authsys_create_default();
	register int	i, j;
	void		*handle;
	char		uaddress[1024];	/* A self imposed limit */
	char		*uaddrp = uaddress;
	int 		pmap_reply_flag; /* reply recvd from PORTMAP */
	/* An array of all the suitable broadcast transports */
	struct {
		int fd;		/* File descriptor */
		bool_t udp_flag;	/* this is udp */
		struct netconfig *nconf; /* Netconfig structure */
		u_int asize;	/* Size of the addr buf */
		u_int dsize;	/* Size of the data buf */
		struct netbuf raddr; /* Remote address */
		struct nd_addrlist *nal; /* Broadcast addrs */
	} fdlist[MAXBCAST];
	struct pollfd pfd[MAXBCAST];
	register int 	fdlistno = 0;
	struct r_rpcb_rmtcallargs barg;	/* Remote arguments */
	struct r_rpcb_rmtcallres bres; /* Remote results */
	struct t_unitdata t_udata, t_rdata;
	struct netconfig *nconf;
	struct nd_hostserv hs;
	int msec;
	int pollretval;
	int fds_found;
	char nettype_array[NETIDLEN];
	char *nettype = &nettype_array[0];

#ifdef PORTMAP
	rpcport_t *port;	/* Remote port number */
	int pmap_flag = 0;	/* UDP exists ? */
	char *outbuf_pmap = NULL;
	struct p_rmtcallargs barg_pmap;	/* Remote arguments */
	struct p_rmtcallres bres_pmap; /* Remote results */
	struct t_unitdata t_udata_pmap;
	int udpbufsz = 0;
#endif				/* PORTMAP */

	trace4(TR_rpc_broadcast, 0, prog, vers, proc);
	if (sys_auth == (AUTH *)NULL) {
		trace4(TR_rpc_broadcast, 1, prog, vers, proc);
		return (RPC_SYSTEMERROR);
	}
	/*
	 * initialization: create a fd, a broadcast address, and send the
	 * request on the broadcast transport.
	 * Listen on all of them and on replies, call the user supplied
	 * function.
	 */

	if (netclass == NULL)
		nettype = NULL;
	else
		strcpy(nettype, netclass);

	if (nettype == NULL)
		nettype = "datagram_n";
	if ((handle = __rpc_setconf((char *)nettype)) == NULL) {
		trace4(TR_rpc_broadcast, 1, prog, vers, proc);
		return (RPC_UNKNOWNPROTO);
	}
	while (nconf = __rpc_getconf(handle)) {
		struct t_info tinfo;
		int fd;
		u_int addrlen;

		if (nconf->nc_semantics != NC_TPI_CLTS)
			continue;
		if (fdlistno >= MAXBCAST)
			break;	/* No more slots available */
		if ((fd = t_open(nconf->nc_device, O_RDWR, &tinfo)) == -1) {
			stat = RPC_CANTSEND;
			continue;
		}
		if (t_bind(fd, (struct t_bind *)NULL,
			(struct t_bind *)NULL) == -1) {
			(void) t_close(fd);
			stat = RPC_CANTSEND;
			continue;
		}
		/* Do protocol specific negotiating for broadcast */
		if (netdir_options(nconf, ND_SET_BROADCAST, fd, NULL)) {
			(void) t_close(fd);
			stat = RPC_NOBROADCAST;
			continue;
		}
		fdlist[fdlistno].fd = fd;
		fdlist[fdlistno].nconf = nconf;
		fdlist[fdlistno].udp_flag = FALSE;
		if (((addrlen = __rpc_get_a_size(tinfo.addr)) == 0) ||
		    ((fdlist[fdlistno].raddr.buf = malloc(addrlen)) == NULL)) {
			t_close(fd);
			stat = RPC_SYSTEMERROR;
			goto done_broad;
		}
		fdlist[fdlistno].raddr.maxlen = addrlen;
		fdlist[fdlistno].raddr.len = addrlen;
		pfd[fdlistno].events = POLLIN | POLLPRI |
			POLLRDNORM | POLLRDBAND;
		pfd[fdlistno].fd = fdlist[fdlistno].fd = fd;
		fdlist[fdlistno].asize = addrlen;

		if ((fdlist[fdlistno].dsize = __rpc_get_t_size(0,
				tinfo.tsdu)) == 0) {
			t_close(fd);
			free(fdlist[fdlistno].raddr.buf);
			stat = RPC_SYSTEMERROR; /* XXX */
			goto done_broad;
		}

		if (maxbufsize <= fdlist[fdlistno].dsize)
			maxbufsize = fdlist[fdlistno].dsize;
#ifdef PORTMAP
		if (strcmp(nconf->nc_protofmly, NC_INET) == 0 &&
		    strcmp(nconf->nc_proto, NC_UDP) == 0) {
			udpbufsz = fdlist[fdlistno].dsize;
			if ((outbuf_pmap = malloc(udpbufsz)) == NULL) {
				t_close(fd);
				free(fdlist[fdlistno].raddr.buf);
				stat = RPC_SYSTEMERROR;
				goto done_broad;
			}
			pmap_flag = 1;
			fdlist[fdlistno].udp_flag = TRUE;
		}
#endif
		fdlistno++;
	}

	if (fdlistno == 0) {
		if (stat == RPC_SUCCESS)
			stat = RPC_UNKNOWNPROTO;
		goto done_broad;
	}
	if (maxbufsize == 0) {
		if (stat == RPC_SUCCESS)
			stat = RPC_CANTSEND;
		goto done_broad;
	}
	inbuf = malloc((size_t) maxbufsize);
	outbuf = malloc((size_t) maxbufsize);
	if ((inbuf == NULL) || (outbuf == NULL)) {
		stat = RPC_SYSTEMERROR;
		goto done_broad;
	}

	/* Serialize all the arguments which have to be sent */
	(void) gettimeofday(&t, (struct timezone *)0);
	msg.rm_xid = getpid() ^ t.tv_sec ^ t.tv_usec;
	msg.rm_direction = CALL;
	msg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	msg.rm_call.cb_prog = RPCBPROG;
	msg.rm_call.cb_vers = RPCBVERS;
	msg.rm_call.cb_proc = RPCBPROC_CALLIT;
	barg.prog = prog;
	barg.vers = vers;
	barg.proc = proc;
	barg.args.args_val = argsp;
	barg.xdr_args = xargs;
	bres.addr = uaddrp;
	bres.results.results_val = resultsp;
	bres.xdr_res = xresults;
	msg.rm_call.cb_cred = sys_auth->ah_cred;
	msg.rm_call.cb_verf = sys_auth->ah_verf;
	xdrmem_create(xdrs, outbuf, maxbufsize, XDR_ENCODE);
	if ((! xdr_callmsg(xdrs, &msg)) ||
	    (! xdr_rpcb_rmtcallargs(xdrs, &barg))) {
		stat = RPC_CANTENCODEARGS;
		goto done_broad;
	}
	t_udata.opt.len = 0;
	t_udata.udata.buf = outbuf;
	t_udata.udata.len = xdr_getpos(xdrs);
	t_udata.udata.maxlen = t_udata.udata.len;
	/* XXX Should have set opt to its legal maxlen. */
	t_rdata.opt.len = t_rdata.opt.maxlen = 0;
	xdr_destroy(xdrs);

#ifdef PORTMAP
	/* Prepare the packet for version 2 PORTMAP */
	if (pmap_flag) {
		msg.rm_xid++;	/* One way to distinguish */
		msg.rm_call.cb_prog = PMAPPROG;
		msg.rm_call.cb_vers = PMAPVERS;
		msg.rm_call.cb_proc = PMAPPROC_CALLIT;
		barg_pmap.prog = prog;
		barg_pmap.vers = vers;
		barg_pmap.proc = proc;
		barg_pmap.args.args_val = argsp;
		barg_pmap.xdr_args = xargs;
		port = &bres_pmap.port;	/* for use later on */
		bres_pmap.xdr_res = xresults;
		bres_pmap.res.res_val = resultsp;
		xdrmem_create(xdrs, outbuf_pmap, udpbufsz, XDR_ENCODE);
		if ((! xdr_callmsg(xdrs, &msg)) ||
		    (! xdr_rmtcallargs(xdrs, &barg_pmap))) {
			stat = RPC_CANTENCODEARGS;
			goto done_broad;
		}
		t_udata_pmap.opt.len = 0;
		t_udata_pmap.udata.buf = outbuf_pmap;
		t_udata_pmap.udata.len = xdr_getpos(xdrs);
		xdr_destroy(xdrs);
	}
#endif				/* PORTMAP */

	/*
	 * Basic loop: broadcast the packets to transports which
	 * support data packets of size such that one can encode
	 * all the arguments.
	 * Wait a while for response(s).
	 * The response timeout grows larger per iteration.
	 */
	hs.h_host = HOST_BROADCAST;
	hs.h_serv = "rpcbind";

	for (msec = inittime; msec <= waittime; msec += msec) {
		/* Broadcast all the packets now */
		for (i = 0; i < fdlistno; i++) {
			struct nd_addrlist *addrlist;

			if (fdlist[i].dsize < t_udata.udata.len) {
				stat = RPC_CANTSEND;
				continue;
			}
			if (netdir_getbyname(fdlist[i].nconf, &hs, &addrlist) ||
			    (addrlist->n_cnt == 0)) {
				stat = RPC_N2AXLATEFAILURE;
				continue;
			}

			for (j = 0; j < addrlist->n_cnt; j++) {
				struct netconfig *nconf = fdlist[i].nconf;

				t_udata.addr = addrlist->n_addrs[j];

				/*
				 * Only use version 3 if lowvers is not set
				 * or transport is not UDP.
				 */

				if (!lowvers || !fdlist[i].udp_flag)
					if (t_sndudata(fdlist[i].fd,
						&t_udata)) {
						(void) syslog(LOG_ERR,
						"Cannot send broadcast\
packet: %m");
#ifdef RPC_DEBUG
				t_error("rpc_broadcast: t_sndudata");
#endif
						stat = RPC_CANTSEND;
						continue;
					};
#ifdef RPC_DEBUG
				if (!lowvers || !fdlist[i].udp_flag)
					fprintf(stderr, "Broadcast packet sent \
						for %s\n", nconf->nc_netid);
#endif
#ifdef PORTMAP
				/*
				 * Send the version 2 packet also
				 * for UDP/IP
				 */
				if (fdlist[i].udp_flag) {
					t_udata_pmap.addr = t_udata.addr;
					if (t_sndudata(fdlist[i].fd,
						&t_udata_pmap)) {
						(void) syslog(LOG_ERR,
				"Cannot send broadcast packet: %m");
#ifdef RPC_DEBUG
						t_error("rpc_broadcast:\
t_sndudata");
#endif
						stat = RPC_CANTSEND;
						continue;
					}
				}
#ifdef RPC_DEBUG
				fprintf(stderr, "PMAP Broadcast packet \
					sent for %s\n", nconf->nc_netid);
#endif
#endif				/* PORTMAP */
			}
			/* End for sending all packets on this transport */
			(void) netdir_free((char *)addrlist, ND_ADDRLIST);
		}		/* End for sending on all transports */

		if (eachresult == NULL) {
			stat = RPC_SUCCESS;
			goto done_broad;
		}

		/*
		 * Get all the replies from these broadcast requests
		 */
	recv_again:

		switch (pollretval = poll(pfd, fdlistno, msec)) {
		case 0:		/* timed out */
			stat = RPC_TIMEDOUT;
			continue;
		case -1:	/* some kind of error - we ignore it */
			goto recv_again;
		}		/* end of poll results switch */

		t_rdata.udata.buf = inbuf;

		for (i = fds_found = 0;
			i < fdlistno && fds_found < pollretval; i++) {

			int flag;
			bool_t	done = FALSE;

			if (pfd[i].revents == 0)
				continue;
			else if (pfd[i].revents & POLLNVAL) {
				/*
				 * Something bad has happened to this descri-
				 * ptor. We can cause poll() to ignore
				 * it simply by using a negative fd.  We do that
				 * rather than compacting the pfd[] and fdlist[]
				 * arrays.
				 */
				pfd[i].fd = -1;
				fds_found++;
				continue;
			} else
				fds_found++;
#ifdef RPC_DEBUG
			fprintf(stderr, "response for %s\n",
				fdlist[i].nconf->nc_netid);
#endif
		try_again:
			t_rdata.udata.maxlen = fdlist[i].dsize;
			t_rdata.udata.len = 0;
			t_rdata.addr = fdlist[i].raddr;
			if (t_rcvudata(fdlist[i].fd, &t_rdata, &flag) == -1) {
				if (t_errno == TSYSERR && errno == EINTR)
					goto try_again;

				/*
				 * Ignore any T_UDERR look errors.
				 * We should never see any ICMP port
				 * unreachables when broadcasting but it has
				 * been observed with broken IP
				 * implementations.
				 */
				if (t_errno == TLOOK &&
				    t_look(fdlist[i].fd) == T_UDERR &&
				    t_rcvuderr(fdlist[i].fd, NULL) == 0)
					goto recv_again;

				(void) syslog(LOG_ERR,
					"Cannot receive reply to \
					broadcast: %m");
				stat = RPC_CANTRECV;
				continue;
			}
			/*
			 * Not taking care of flag for T_MORE.
			 * We are assuming that
			 * such calls should not take more than one
			 * transport packet.
			 */
			if (flag & T_MORE)
				continue; /* Drop that and go ahead */
			if (t_rdata.udata.len < (u_int)sizeof (uint32_t))
				continue; /* Drop that and go ahead */
			/*
			 * see if reply transaction id matches sent id.
			 * If so, decode the results. If return id is xid + 1
			 * it was a PORTMAP reply
			 */
			if (*((uint32_t *)(inbuf)) == *((uint32_t *)(outbuf))) {
				pmap_reply_flag = 0;
				msg.acpted_rply.ar_verf = _null_auth;
				msg.acpted_rply.ar_results.where =
					(caddr_t)&bres;
				msg.acpted_rply.ar_results.proc =
					(xdrproc_t)xdr_rpcb_rmtcallres;
#ifdef PORTMAP
			} else if (pmap_flag &&
				*((uint32_t *)(inbuf)) ==
				*((uint32_t *)(outbuf_pmap))) {
				pmap_reply_flag = 1;
				msg.acpted_rply.ar_verf = _null_auth;
				msg.acpted_rply.ar_results.where =
					(caddr_t)&bres_pmap;
				msg.acpted_rply.ar_results.proc =
					(xdrproc_t)xdr_rmtcallres;
#endif				/* PORTMAP */
			} else
				continue;
			xdrmem_create(xdrs, inbuf,
				(u_int)t_rdata.udata.len, XDR_DECODE);
			if (xdr_replymsg(xdrs, &msg)) {
				if ((msg.rm_reply.rp_stat == MSG_ACCEPTED) &&
				    (msg.acpted_rply.ar_stat == SUCCESS)) {
					struct netbuf *taddr;
#ifdef PORTMAP
					if (pmap_flag && pmap_reply_flag) {
						/* convert port to taddr */
						((struct sockaddr_in *)
						t_rdata.addr.buf)->sin_port =
						htons((u_short)*port);
						taddr = &t_rdata.addr;
					} else /* Convert the uaddr to taddr */
#endif
						taddr = uaddr2taddr(
						    fdlist[i].nconf,
						    uaddrp);
					done = (*eachresult)(resultsp, taddr,
						fdlist[i].nconf);
#ifdef RPC_DEBUG
				{
					int k;

					printf("rmt addr = ");
					for (k = 0; k < taddr->len; k++)
						printf("%d ", taddr->buf[k]);
					printf("\n");
				}
#endif
					if (taddr && !pmap_reply_flag)
						netdir_free((char *)taddr,
							    ND_ADDR);
				}
				/* otherwise, we just ignore the errors ... */
			}
			/* else some kind of deserialization problem ... */

			xdrs->x_op = XDR_FREE;
			msg.acpted_rply.ar_results.proc = (xdrproc_t) xdr_void;
			(void) xdr_replymsg(xdrs, &msg);
			(void) (*xresults)(xdrs, resultsp);
			XDR_DESTROY(xdrs);
			if (done) {
				stat = RPC_SUCCESS;
				goto done_broad;
			} else {
				goto recv_again;
			}
		}		/* The recv for loop */
	}			/* The giant for loop */

done_broad:
	if (inbuf)
		(void) free(inbuf);
	if (outbuf)
		(void) free(outbuf);
#ifdef PORTMAP
	if (outbuf_pmap)
		(void) free(outbuf_pmap);
#endif
	for (i = 0; i < fdlistno; i++) {
		(void) t_close(fdlist[i].fd);
		(void) free(fdlist[i].raddr.buf);
	}
	AUTH_DESTROY(sys_auth);
	(void) __rpc_endconf(handle);

	trace4(TR_rpc_broadcast, 1, prog, vers, proc);
	return (stat);
}


enum clnt_stat
rpc_broadcast(prog, vers, proc, xargs, argsp, xresults, resultsp,
			eachresult, nettype)
	rpcprog_t	prog;		/* program number */
	rpcvers_t	vers;		/* version number */
	rpcproc_t	proc;		/* procedure number */
	xdrproc_t	xargs;		/* xdr routine for args */
	caddr_t		argsp;		/* pointer to args */
	xdrproc_t	xresults;	/* xdr routine for results */
	caddr_t		resultsp;	/* pointer to results */
	resultproc_t	eachresult;	/* call with each result obtained */
	const char	*nettype;	/* transport type */
{
	enum clnt_stat	dummy;

	trace4(TR_rpc_broadcast, 0, prog, vers, proc);
	dummy = rpc_broadcast_exp(prog, vers, proc, xargs, argsp,
		xresults, resultsp, eachresult,
		INITTIME, WAITTIME, nettype);
	trace1(TR_rpc_broadcast, 1);
	return (dummy);
}
