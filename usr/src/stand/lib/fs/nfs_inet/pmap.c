#pragma ident	"@(#)pmap.c	1.25	97/04/29 SMI"

/*
 * Copyright (c) 1991, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

/*
 * This file contains the routines that maintain a linked list of known
 * program to udp port mappings. There are three static members initialized
 * by default, one for the portmapper itself (of course), one for rpcbind,
 * and one for nfs. If a program number is not in the list, then routines
 * in this file contact the portmapper on the server, and dynamically add
 * new members to this list.
 *
 * This file also contains pmap_rmtcall() - which lets one get the port
 * number AND run the rpc call in one step. Only the server that successfully
 * completes the rpc call will return a result.
 *
 * NOTE: Because we will end up caching the port entries we need
 * before the kernel begins running, we can use dynamic allocation here.
 * boot_memfree() calls pmap_mem_free() to free up any dynamically
 * allocated entries when the boot program has finished its job.
 */

#include <sys/types.h>
#include <rpc.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <sys/sainet.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_rmt.h>
#undef NFSSERVER
#include <nfs_prot.h>
#include <rpc/rpcb_prot.h>
#include <netaddr.h>
#include <sys/salib.h>
#ifdef DEBUG
#include <sys/promif.h>
#endif /* DEBUG */

#define	PMAP_STATIC	2	/* last statically allocated list entry */
#define	UA_SIZE		100	/* max space need for an universal addr */
				/* it's larger than it has to be XXXX	*/

extern enum clnt_stat rpc_call(rpcprog_t, rpcvers_t, rpcproc_t, xdrproc_t,
	caddr_t, xdrproc_t, caddr_t, int, int, struct sainet *, u_int);

extern char *kmem_alloc(unsigned int);
extern void kmem_free(char *, unsigned int);
static int uaddr2port(char *);

/* portmap structure */
struct pmaplist pre_init[3] = {
	{ {PMAPPROG, PMAPVERS, IPPROTO_UDP, PMAPPORT },
		&pre_init[1] },
	/* SVR4 rpcbind listens to old portmapper port */
	{ {RPCBPROG, RPCBVERS, IPPROTO_UDP, PMAPPORT},
		&pre_init[2] },
	{ {NFS_PROGRAM, NFS_VERSION, IPPROTO_UDP, NFS_PORT},
	    (struct pmaplist *)0 }
};
struct pmaplist *map_head = &pre_init[0];
struct pmaplist *map_tail = &pre_init[PMAP_STATIC];

/*
 * Forward
 */
void pmap_addport(rpcprog_t prog, rpcvers_t vers, rpcport_t port);
void pmap_delport(rpcprog_t prog, rpcvers_t vers);

/*
 * pmap_rmtcall: does PMAPPROC_CALLIT broadcasts w/ rpc_call requests.
 * Lets one do a PMAPGETPORT/RPC PROC call in one easy step.
 *
 * Code adapted from pmap_rmtcall() in dlboot_inet.c (kernel)
 */
/*ARGSUSED*/
enum clnt_stat
pmap_rmtcall(
	rpcprog_t	prog,	/* rpc program number to call. */
	rpcvers_t	vers,	/* rpc program version */
	rpcproc_t	proc,	/* rpc procedure to call */
	xdrproc_t	in_xdr,	/* routine to serialize arguments */
	caddr_t		args,	/* arg vector for remote call */
	xdrproc_t	out_xdr, /* routine to deserialize results */
	caddr_t		ret,	/* addr of buf to place results in */
	int		rexmit,	/* retransmission interval (secs) */
	int		wait,	/* how long (secs) to wait for a resp */
	struct sainet	*net,	/* network addresses */
	u_int		auth)	/* type of authentication wanted. */
{
	/* functions */
	bool_t		xdr_rmtcall_args(XDR *, struct rmtcallargs *);
	bool_t		xdr_rmtcallres(XDR *, struct rmtcallres *);
	bool_t		xdr_rpcb_rmtcallargs(XDR *, struct rpcb_rmtcallargs *);
	bool_t		xdr_rpcb_rmtcallres(XDR *, struct rpcb_rmtcallres *);
	/* variables */
	enum clnt_stat		status;	/* rpc_call status */
	rpcport_t		port;	/* returned port # */
	struct rmtcallargs	pmap_a;	/* args for pmap call */
	struct rmtcallres	pmap_r;	/* results from pmap call */
	struct rpcb_rmtcallargs	rpcb_a;	/* args for rpcb call */
	struct rpcb_rmtcallres	rpcb_r;	/* results from rpcb call */
	char			ua[UA_SIZE]; /* universal addr buffer */

	/* initialize pmap */
	pmap_a.prog = prog;
	pmap_a.vers = vers;
	pmap_a.proc = proc;
	pmap_a.args_ptr = args;
	pmap_a.xdr_args = in_xdr;
	pmap_r.port_ptr = &port;
	pmap_r.results_ptr = ret;
	pmap_r.xdr_results = out_xdr;

	status = rpc_call((rpcprog_t)PMAPPROG, (rpcvers_t)PMAPVERS,
	    (rpcproc_t)PMAPPROC_CALLIT, xdr_rmtcall_args, (caddr_t)&pmap_a,
	    xdr_rmtcallres, (caddr_t)&pmap_r, rexmit, wait, net,
	    AUTH_NONE);

	if (status != RPC_PROGUNAVAIL) {
		if (status == RPC_SUCCESS) {
			/* delete old port mapping, if it exists */
			pmap_delport(prog, vers);

			/* save the new port mapping */
			pmap_addport(prog, vers, port);
		}
		return (status);
	}
	/*
	 * PMAP is unavailable. Maybe there's a SVR4 machine, with rpcbind.
	 */

	/* initialize rpcb */
	rpcb_a.prog = prog;
	rpcb_a.vers = vers;
	rpcb_a.proc = proc;
	rpcb_a.args_ptr = args;
	rpcb_a.xdr_args = in_xdr;
	rpcb_r.addr_ptr = &ua[0];
	rpcb_r.results_ptr = ret;
	rpcb_r.xdr_results = out_xdr;

	status = rpc_call((rpcprog_t)RPCBPROG, (rpcvers_t)RPCBVERS,
	    (rpcproc_t)RPCBPROC_CALLIT, xdr_rpcb_rmtcallargs, (caddr_t)&rpcb_a,
	    xdr_rpcb_rmtcallres, (caddr_t)&rpcb_r, rexmit, wait, 0, AUTH_NONE);

	if (status == RPC_SUCCESS) {
		/* delete old port mapping, if it exists */
		pmap_delport(prog, vers);

		/* save the new port mapping */
		port = uaddr2port(&ua[0]);
		pmap_addport(prog, vers, port);
	}
	return (status);
}

/*
 * pmap_getport: Queries current list of cached pmap_list entries,
 * returns the port number of the entry found. If the port number
 * is not cached, then getport makes a rpc call first to the portmapper,
 * and then to rpcbind (SVR4) if the portmapper does not respond. The
 * returned port is then added to the cache, and the port number is
 * returned. If both portmapper and rpc bind fail to give us the necessary
 * port, we return 0 to signal we hit an error, and set rpc_stat to
 * the appropriate RPC error code. Only IPPROTO_UDP protocol is supported.
 */
rpcport_t
pmap_getport(rpcprog_t prog, rpcvers_t vers,
	enum clnt_stat *rpc_stat)	/* for RPC status */
{
	bool_t				xdr_rpcb();
	register struct pmaplist	*walk;
	struct pmap			pmap_send;	/* portmap */
	RPCB				rpcb_send;	/* rpcbind */
	char				ua_buf[UA_SIZE]; /* univ addr buf */
	char				*ua;		/* universal address */
	u_short				dport;

	/* initialize */
	ua = &ua_buf[0];

#ifdef DEBUG
	printf("pmap_getport: called with: prog: 0x%x, vers: 0x%x\n",
	    prog, vers);
#endif /* DEBUG */
	for (walk = map_head; walk != 0; walk = walk->pml_next) {
		if ((walk->pml_map.pm_prog == prog) &&
		    (walk->pml_map.pm_vers == vers) &&
		    (walk->pml_map.pm_prot == (rpcprot_t)IPPROTO_UDP)) {
#ifdef DEBUG
			printf("pmap_getport: Found in cache. returning: %d\n",
			    walk->pml_map.pm_port);
#endif /* DEBUG */
			return (walk->pml_map.pm_port);
		}
	}
	/*
	 * Not in the cache. First try the portmapper (SunOS server?) and
	 * if that fails, try rpcbind (SVR4 server).
	 */
	pmap_send.pm_prog = prog;
	pmap_send.pm_vers = vers;
	pmap_send.pm_prot = (rpcprot_t)IPPROTO_UDP;
	pmap_send.pm_port = 0;	/* what we're after */

#ifdef DEBUG
	printf("pmap_getport: trying portmapper: prog: 0x%x, vers: 0x%x\n",
	    prog, vers);
#endif /* DEBUG */
	*rpc_stat = rpc_call(PMAPPROG, PMAPVERS, PMAPPROC_GETPORT,
	    xdr_pmap, (caddr_t)&pmap_send, xdr_u_short,
	    (caddr_t)&dport, 0, 0, 0, AUTH_NONE);

	if (*rpc_stat == RPC_PROGUNAVAIL) {
		/*
		 * The portmapper isn't available. Try rpcbind.
		 * Maybe the server is a SVR4 server.
		 */
#ifdef DEBUG
		printf("pmap_getport: portmapper failed. Trying rpcbind...\n");
#endif /* DEBUG */
		rpcb_send.r_prog = prog;
		rpcb_send.r_vers = vers;
		rpcb_send.r_netid = (caddr_t)0;
		rpcb_send.r_addr = (caddr_t)0;
		rpcb_send.r_owner = (caddr_t)0;
		bzero(ua, UA_SIZE);

		/*
		 * Again, default # of retries. xdr_wrapstring()
		 * wants a char **.
		 */
		*rpc_stat = rpc_call(RPCBPROG, RPCBVERS, RPCBPROC_GETADDR,
		    xdr_rpcb, (caddr_t)&rpcb_send, xdr_wrapstring,
		    (char *)&ua, 0, 0, 0, AUTH_NONE);

		if (*rpc_stat == RPC_SUCCESS) {
#ifdef DEBUG
			printf("pmap_getport: call to rpcbind succeeded.\n");
#endif /* DEBUG */
			if (ua[0] != '\0') {
				dport = uaddr2port(ua);
				dport = ntohs(dport);
			} else {
				/* Address unknown */
				return (0);
			}
		}
	}

	if (*rpc_stat == RPC_SUCCESS)  {
#ifdef DEBUG
	printf("pmap_getport: prog: %x, vers: %x; returning port: %d.\n",
	    prog, vers, dport);
#endif /* DEBUG */
		pmap_addport(prog, vers, (rpcport_t)dport);
		return (dport);
	} else {
#ifdef DEBUG
		printf("pmap_getport: Failed getting port.\n");
#endif /* DEBUG */
		return (0);	/* we failed. */
	}
/*NOTREACHED*/
}

/*
 * pmap_addport: adds a new entry on to the end of the pmap cache.
 */
void
pmap_addport(rpcprog_t prog, rpcvers_t vers, rpcport_t port)
{
	register struct pmaplist *new;

	/* allocate new pmaplist */
	new = (struct pmaplist *)kmem_alloc(sizeof (struct pmaplist));

	if (new == (struct pmaplist *)0)
		return; /* not fatal here, we'll just throw out the entry */

	new->pml_map.pm_prog = prog;
	new->pml_map.pm_vers = vers;
	new->pml_map.pm_prot = (rpcprot_t)IPPROTO_UDP;
	new->pml_map.pm_port = port;

	map_tail->pml_next = new;
	new->pml_next = (struct pmaplist *)0;
	map_tail = new;
}

/*
 * pmap_delport: deletes an existing entry from the list. Caution - don't
 * call this function to delete statically allocated entries. Why would
 * you want to, anyway? Only IPPROTO_UDP is supported, of course.
 */
void
pmap_delport(rpcprog_t prog, rpcvers_t vers)
{
	register struct pmaplist *tmp, *prev;

	prev = map_head;
	for (tmp = map_head; tmp != (struct pmaplist *)0;
	    tmp = tmp->pml_next) {
		if ((tmp->pml_map.pm_prog == prog) &&
		    (tmp->pml_map.pm_vers == vers)) {
			if (tmp == map_head)
				map_head = tmp->pml_next; /* new head */
			else if (tmp == map_tail) {
				map_tail = prev;	/* new tail */
				map_tail->pml_next = (struct pmaplist *)0;
			} else {
				/* internal delete */
				prev->pml_next = tmp->pml_next;
			}
#ifdef	DEBUG
			printf("pmap_delport: prog: %x, vers: %x\n",
			    prog, vers);
#endif	/* DEBUG */
			kmem_free((caddr_t)tmp, sizeof (struct pmaplist));
			break;
		} else
			prev = tmp;
	}
}

/*
 * pmap_memfree: frees up any dynamically allocated entries.
 */
void
pmap_memfree()
{
	/* variables */
	extern struct pmaplist pre_init[];
	extern struct pmaplist *map_tail;
	register struct pmaplist *current, *tmp;

	if (map_tail == &pre_init[PMAP_STATIC]) {
		/* no dynamic entries */
		return;
	}

	/* free from head of the list to the tail. */
	current = pre_init[PMAP_STATIC].pml_next;
	while (current != (struct pmaplist *)0) {
		tmp = current->pml_next;
		kmem_free((caddr_t)current, sizeof (struct pmaplist));
		current = tmp;
	}
}

/*
 * (from dlboot_inet.c) (kernel)
 * Convert a port number from a sockaddr_in expressed
 * in universal address format.
 */
static int
uaddr2port(char	*addr)
{
	int	p1;
	int	p2;
	char	*next;
	static int strtoi(char *str, char **ptr);

	/*
	 * A struct sockaddr_in expressed in universal address
	 * format looks like:
	 *
	 *	"IP.IP.IP.IP.PORT[top byte].PORT[bot. byte]"
	 *
	 * Where each component expresses as a charactor,
	 * the corresponding part of the IP address
	 * and port number.
	 * Thus 127.0.0.1, port 2345 looks like:
	 *
	 *	49 50 55 46 48 46 48 46 49 46 57 46 52 49
	 *	1  2  7  .  0  .  0  .  1  .  9  .  4  1
	 *
	 * 2345 = 929base16 = 9.32+9 = 9.41
	 */
	(void) strtoi(addr, &next);
	(void) strtoi(next, &next);
	(void) strtoi(next, &next);
	(void) strtoi(next, &next);
	p1 = strtoi(next, &next);
	p2 = strtoi(next, &next);

	return ((p1 << 8) + p2);
}

/*
 * Modified strtol(3).
 */
static int
strtoi(char *str, char **ptr)
{
	int	c;
	int	val;

	for (val = 0, c = *str++; c >= '0' && c <= '9'; c = *str++) {
		val *= 10;
		val += c - '0';
	}
	*ptr = str;
	return (val);
}

/*
 * Xdr routines used for calling portmapper/rpcbind.
 */

bool_t
xdr_pmap(XDR *xdrs, struct pmap *regs)
{
	if (xdr_rpcprog(xdrs, &regs->pm_prog) &&
		xdr_rpcvers(xdrs, &regs->pm_vers) &&
		xdr_rpcprot(xdrs, &regs->pm_prot))
		return (xdr_rpcprot(xdrs, &regs->pm_port));
	return (FALSE);
}

bool_t
xdr_rpcb(XDR *xdrs, RPCB *objp)
{
	if (!xdr_rpcprog(xdrs, &objp->r_prog)) {
		return (FALSE);
	}
	if (!xdr_rpcvers(xdrs, &objp->r_vers)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_netid, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_addr, ~0)) {
		return (FALSE);
	}
	if (!xdr_string(xdrs, &objp->r_owner, ~0)) {
		return (FALSE);
	}
	return (TRUE);
}

/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rmtcall_args(XDR *xdrs, struct rmtcallargs *cap)
{
	u_int	lenposition;
	u_int	argposition;
	u_int	position;

	if (xdr_rpcprog(xdrs, &(cap->prog)) &&
	    xdr_rpcvers(xdrs, &(cap->vers)) &&
	    xdr_rpcproc(xdrs, &(cap->proc))) {
		lenposition = XDR_GETPOS(xdrs);
		if (!xdr_u_int(xdrs, &(cap->arglen)))
			return (FALSE);
		argposition = XDR_GETPOS(xdrs);
		if (!(*(cap->xdr_args))(xdrs, cap->args_ptr))
			return (FALSE);
		position = XDR_GETPOS(xdrs);
		cap->arglen = position - argposition;
		XDR_SETPOS(xdrs, lenposition);
		if (!xdr_u_int(xdrs, &(cap->arglen)))
			return (FALSE);
		XDR_SETPOS(xdrs, position);
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rmtcallres(XDR *xdrs, struct rmtcallres *crp)
{
	caddr_t		port_ptr;

	port_ptr = (caddr_t)crp->port_ptr;
	if (xdr_reference(xdrs, &port_ptr, sizeof (u_int), xdr_u_int) &&
	    xdr_u_int(xdrs, &crp->resultslen)) {
		crp->port_ptr = (rpcport_t *)port_ptr;
		return ((*(crp->xdr_results))(xdrs, crp->results_ptr));
	}
	return (FALSE);
}

/*
 * XDR remote call arguments
 * written for XDR_ENCODE direction only
 */
bool_t
xdr_rpcb_rmtcallargs(XDR *xdrs, struct rpcb_rmtcallargs *objp)
{
	u_int lenposition, argposition, position;

	if (!xdr_rpcprog(xdrs, &objp->prog)) {
		return (FALSE);
	}
	if (!xdr_rpcvers(xdrs, &objp->vers)) {
		return (FALSE);
	}
	if (!xdr_rpcproc(xdrs, &objp->proc)) {
		return (FALSE);
	}
	/*
	 * All the jugglery for just getting the size of the arguments
	 */
	lenposition = XDR_GETPOS(xdrs);
	if (!xdr_u_int(xdrs, &(objp->arglen)))
		return (FALSE);
	argposition = XDR_GETPOS(xdrs);
	if (!(*(objp->xdr_args))(xdrs, objp->args_ptr))
		return (FALSE);
	position = XDR_GETPOS(xdrs);
	objp->arglen = position - argposition;
	XDR_SETPOS(xdrs, lenposition);
	if (!xdr_u_int(xdrs, &(objp->arglen)))
		return (FALSE);
	XDR_SETPOS(xdrs, position);
	return (TRUE);
}

/*
 * XDR remote call results
 * written for XDR_DECODE direction only
 */
bool_t
xdr_rpcb_rmtcallres(XDR *xdrs, struct rpcb_rmtcallres *objp)
{
	if (!xdr_string(xdrs, &objp->addr_ptr, ~0)) {
		return (FALSE);
	}
	if (!xdr_u_int(xdrs, &objp->resultslen)) {
		return (FALSE);
	}
	return ((*(objp->xdr_results))(xdrs, objp->results_ptr));
}
