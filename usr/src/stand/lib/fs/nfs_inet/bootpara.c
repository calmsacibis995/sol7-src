/*
 * Copyright (c) 1991, 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)bootparams.c	1.23	97/04/29 SMI"

/*
 * This file contains routines responsible for getting the system's
 * name and boot params. Most of it comes from the SVR4 diskless boot
 * code (dlboot_inet), modified to work in a non socket environment.
 */

#include <sys/types.h>
#include <rpc.h>
#include <bootparam.h>
#include <rpc/auth.h>
#include <rpc/xdr.h>
#include <rpc/rpc_msg.h>
#include <sys/t_lock.h>
#include <rpc/clnt.h>
#include <sys/utsname.h>
#include <in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <sys/promif.h>
#include <sys/sainet.h>
#include <netaddr.h>
#include <sys/salib.h>

/* globals */
struct bp_whoami_res	bp;
struct sainet		router;		/* address block for router */
char			bp_hostname[SYS_NMLN + 1];
char			bp_domainname[SYS_NMLN + 1];
static ipaddr_t		ipbcast = INADDR_BROADCAST; /* byte order independent */

extern void inet_print(struct in_addr);

static char *noserver =
	"No bootparam (%s) server responding; still trying...\n";

/*
 * Returns TRUE if it has set the global structure 'bp' to our boot
 * parameters, FALSE if some failure occurred.
 */
bool_t
whoami(void)
{
	/* functions */
	extern enum clnt_stat	pmap_rmtcall();
	extern struct in_addr	*get_netip();
	extern ether_addr_t	*get_netether();
	extern void		set_netip();
	extern void		set_netether();
	extern int		get_arp();

	/* variables */
	extern char bp_hostname[], bp_domainname[];
	extern ether_addr_t	etherbroadcastaddr; /* defined in inet.c */
	struct bp_whoami_arg	arg;
	struct sainet		bootpaddr;
	struct in_addr		ipaddr;
	enum clnt_stat		stat;
	bool_t			retval = TRUE;
	int			rexmit;		/* retransmission interval */
	int			resp_wait;	/* secs to wait for resp */
	int			namelen;
	int			printed_waiting_msg;

	/*
	 * Set our destination IP address to the limited broadcast address
	 * (INADDR_BROADCAST).
	 */
	bcopy(&ipbcast, &bootpaddr.sain_hisaddr, sizeof (struct in_addr));
	bootpaddr.sain_myaddr = *get_netip(SOURCE); /* struct copy */

	bcopy(get_netether(SOURCE), &bootpaddr.sain_myether,
	    sizeof (ether_addr_t));
	/*
	 * ethernet broadcast
	 */
	bcopy(&etherbroadcastaddr, &bootpaddr.sain_hisether,
	    sizeof (ether_addr_t));

	/*
	 * Set up the arguments expected by bootparamd.
	 */
	arg.client_address.address_type = IP_ADDR_TYPE;
	bcopy(&bootpaddr.sain_myaddr, &arg.client_address.bp_address.ip_addr,
	    sizeof (struct in_addr));

	/*
	 * Retransmit/wait for up to resp_wait secs.
	 */
	rexmit = 0;	/* start at default retransmission interval. */
	resp_wait = 16;

	bp.client_name = &bp_hostname[0];
	bp.domain_name = &bp_domainname[0];

	/*
	 * Do a broadcast call to find a bootparam daemon that
	 * will tell us our hostname, domainname and any
	 * router that we have to use to talk to our NFS server.
	 */
	printed_waiting_msg = 0;
	do {
		/*
		 * First try the SunOS portmapper and if no reply is
		 * received will then try the SVR4 rpcbind.
		 * Either way, `bootpaddr' will be set to the
		 * correct address for the bootparamd that responds.
		 */
		stat = pmap_rmtcall((rpcprog_t)BOOTPARAMPROG,
		    (rpcvers_t)BOOTPARAMVERS, (rpcproc_t)BOOTPARAMPROC_WHOAMI,
		    xdr_bp_whoami_arg, (caddr_t)&arg,
		    xdr_bp_whoami_res, (caddr_t)&bp, rexmit, resp_wait,
		    &bootpaddr, AUTH_NONE);
		if (stat == RPC_TIMEDOUT && !printed_waiting_msg) {
			printf(noserver, "whoami");
			printed_waiting_msg = 1;
		}
		/*
		 * Retransmission interval for second and subsequent tries.
		 * We expect first pmap_rmtcall to retransmit and backoff to
		 * at least this value.
		 */
		rexmit = resp_wait;
		resp_wait = 0;		/* go to default wait now. */
	} while (stat == RPC_TIMEDOUT);

	if (stat != RPC_SUCCESS) {
		printf("whoami RPC call failed with rpc status: %d\n",
		    stat);
		retval = FALSE;
		goto done;
	} else {
		if (printed_waiting_msg)
			printf("Bootparam response received\n");
		/*
		 * set our destination addresses to those of our bootparam
		 * server.
		 */
#ifdef DEBUG
		printf("setting our destination addresses to:\n");
		inet_print(bootpaddr.sain_hisaddr);
		ether_print(bootpaddr.sain_hisether);
#endif /* DEBUG */
		set_netip(&(bootpaddr.sain_hisaddr), DESTIN);
		set_netether(&(bootpaddr.sain_hisether), DESTIN);
	}

	namelen = strlen(bp.client_name);
	if (namelen > SYS_NMLN) {
		printf("whoami: hostname too long");
		retval = FALSE;
		goto done;
	}
	if (namelen > 0)
		printf("hostname: %s\n", bp.client_name);
	else {
		printf("whoami: no host name\n");
		retval = FALSE;
		goto done;
	}

	namelen = strlen(bp.domain_name);
	if (namelen > SYS_NMLN) {
		printf("whoami: domainname too long");
		retval = FALSE;
		goto done;
	}
	if (namelen > 0)
		printf("domainname: %s\n", bp.domain_name);
	else
		printf("whoami: no domain name\n");

	if (bp.router_address.address_type == IP_ADDR_TYPE) {
		bcopy(&bp.router_address.bp_address.ip_addr, &ipaddr,
		    sizeof (struct in_addr));

		if ((ipaddr.S_un.S_un_b.s_b1 != (u_char)0) ||
		    (ipaddr.S_un.S_un_b.s_b2 != (u_char)0) ||
		    (ipaddr.S_un.S_un_b.s_b3 != (u_char)0) ||
		    (ipaddr.S_un.S_un_b.s_b4 != (u_char)0)) {
			/*
			 * I have a routing address. Squirrel it away
			 * in case we need it later (getfile). If, in
			 * getfile() it turns out our server address
			 * is different from the machine that responded
			 * to us, AND we can't arp its ethernet address,
			 * we'll try going through this router. If
			 * this ARP fails, then the router's ether addr
			 * will be zero.
			 */

			/* structure copies */
			router.sain_myaddr = *get_netip(SOURCE);
			router.sain_hisaddr = ipaddr;

			bzero(&router.sain_hisether, sizeof (ether_addr_t));
			bcopy(get_netether(SOURCE), &router.sain_myether,
			    sizeof (ether_addr_t));
#ifdef DEBUG
			printf("whoami: ARPing for router ea.\n");
			printf("whoami: Dest ea was: ");
			ether_print(router.sain_hisether);
			printf("whoami: Router ip is: ");
			inet_print(ipaddr);
#endif /* DEBUG */
			/* we don't care if it fails */
			(void) get_arp(&router);
#ifdef DEBUG
			printf("whoami: Now (router) Dest ea is: ");
			ether_print(router.sain_hisether);
#endif /* DEBUG */
		}
	} else
		printf("whoami: unknown gateway addr family %d\n",
		    bp.router_address.address_type);
done:
	return (retval);
}

/*
 * Returns:
 *	1) The ascii form of our root servers name in `server_name'.
 *	2) Pathname of our root on the server in `server_path'.
 *
 * NOTE: it's ok for getfile() to do dynamic allocation - it's only
 * used locally, then freed. If the server address returned from the
 * getfile call is different from our current destination address,
 * reset destination IP address to the new value.
 */
bool_t
getfile(char *fileid, char *server_name, char *server_path)
{
	struct bp_getfile_arg	arg;
	struct bp_getfile_res	res;
	enum clnt_stat		stat;
	struct in_addr		ipaddr;
	struct in_addr		*curr;
	struct sainet		sn;
	int			rexmit;
	int			wait;
	int			printed_waiting_msg;
	extern enum clnt_stat rpc_call();
	extern enum clnt_stat pmap_rmtcall();
	extern ether_addr_t	etherbroadcastaddr; /* defined in inet.c */
	extern struct sainet	router;
	extern struct sainet	*get_sainet();
	extern struct in_addr	*get_netip();
	extern void		set_netip();
	extern ether_addr_t	*get_netether();
	extern void		set_netether();
	extern int		get_arp();
	extern char		*kmem_alloc(unsigned int);
	extern void		kmem_free(char *, unsigned int);

	/* e_zero is used to zero ether_addr fields and for cmp to 0. */
	static ether_addr_t	e_zero = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

	arg.client_name = bp.client_name;
	arg.file_id = fileid;

	res.server_name = (bp_machine_name_t)kmem_alloc(SYS_NMLN+1);
	res.server_path = (bp_path_t)kmem_alloc(SYS_NMLN+1);

	bzero(res.server_name, SYS_NMLN+1);
	bzero(res.server_path, SYS_NMLN+1);

	/*
	 * Our addressing information was filled in by the call to
	 * whoami(), so now send an rpc message to the
	 * bootparam daemon requesting our server information.
	 *
	 * Wait only 32 secs for rpc_call to succeed.
	 */
	stat = rpc_call((rpcprog_t)BOOTPARAMPROG, (rpcvers_t)BOOTPARAMVERS,
	    (rpcproc_t)BOOTPARAMPROC_GETFILE, xdr_bp_getfile_arg, (caddr_t)&arg,
	    xdr_bp_getfile_res, (caddr_t)&res, 0, 32, 0, AUTH_NONE);

	if (stat == RPC_TIMEDOUT) {
		/*
		 * The server that answered the whoami doesn't
		 * answer our getfile. Broadcast the call to all. Keep
		 * trying forever. Set up for limited broadcast.
		 */
		bcopy(&ipbcast, &sn.sain_hisaddr, sizeof (struct in_addr));
		sn.sain_myaddr = *get_netip(SOURCE); /* struct copy */

		bcopy(get_netether(SOURCE), &sn.sain_myether,
		    sizeof (ether_addr_t));
		bcopy(&etherbroadcastaddr, &sn.sain_hisether,
		    sizeof (ether_addr_t));

		rexmit = 0;	/* use default rexmit interval */
		wait = 32;	/* only wait 32 secs for first response */
		printed_waiting_msg = 0;
		do {
			stat = pmap_rmtcall((rpcprog_t)BOOTPARAMPROG,
			    (rpcvers_t)BOOTPARAMVERS,
			    (rpcproc_t)BOOTPARAMPROC_GETFILE,
			    xdr_bp_getfile_arg, (caddr_t)&arg,
			    xdr_bp_getfile_res, (caddr_t)&res, rexmit,
			    wait, &sn, AUTH_NONE);

			if (stat == RPC_SUCCESS) {
				/*
				 * set our destination addresses to
				 * those of the server that responded.
				 * It's probably our server, and we
				 * can thus save arping for no reason later.
				 */
				set_netip(&(sn.sain_hisaddr), DESTIN);
				set_netether(&(sn.sain_hisether), DESTIN);
				if (printed_waiting_msg)
					printf("Bootparam response "
					    "received.\n");
				break;
			}
			if (stat == RPC_TIMEDOUT && !printed_waiting_msg) {
				printf(noserver, "getfile");
				printed_waiting_msg = 1;
			}
			/*
			 * Retransmission interval for second and
			 * subsequent tries. We expect first pmap_rmtcall
			 * to retransmit and backoff to at least this
			 * value.
			 */
			rexmit = wait;
			wait = 0;	/* use default wait time now. */
		} while (stat == RPC_TIMEDOUT);
	}
	if (stat == RPC_SUCCESS) {
		/* got the goods */
		bcopy(res.server_name, server_name, strlen(res.server_name));
		bcopy(res.server_path, server_path, strlen(res.server_path));
	}

	kmem_free(res.server_name, SYS_NMLN+1);
	kmem_free(res.server_path, SYS_NMLN+1);
	if (stat != RPC_SUCCESS) {
		printf("getfile: rpc_call failed.\n");
		return (FALSE);
	}

	if (*server_name == '\0' || *server_path == '\0') {
		printf("getfile: No info from bootparam server.\n");
		return (FALSE);
	}

	switch (res.server_address.address_type) {
	case IP_ADDR_TYPE:
		/*
		 * server_address is where we will get our root
		 * from. Replace destination entries in address if
		 * necessary.
		 */

		bcopy(&res.server_address.bp_address.ip_addr, &ipaddr,
		    sizeof (struct in_addr));

		curr = get_netip(DESTIN);
#ifdef	DEBUG
		{
			struct sainet *tmp;
			tmp = get_sainet();

			printf("getfile: dumping current address struct:\n");
			printf("myipaddr: \n");
			inet_print(tmp->sain_myaddr);
			printf("hisaddr: \n");
			inet_print(tmp->sain_hisaddr);
			printf("myether: \n");
			ether_print(tmp->sain_myether);
			printf("hisether: \n");
			ether_print(tmp->sain_hisether);
			printf("SERVER ip address: \n");
			inet_print(ipaddr);
		}
#endif	/* DEBUG */

		if ((ipaddr.S_un.S_un_b.s_b1 != curr->S_un.S_un_b.s_b1) ||
		    (ipaddr.S_un.S_un_b.s_b2 != curr->S_un.S_un_b.s_b2) ||
		    (ipaddr.S_un.S_un_b.s_b3 != curr->S_un.S_un_b.s_b3) ||
		    (ipaddr.S_un.S_un_b.s_b4 != curr->S_un.S_un_b.s_b4)) {
			/*
			 * arp for server's ethernet address. Set ip and
			 * ether for destination if ARP succeeds. Otherwise,
			 * try to route through the router, if possible.
			 */

			/* structure copies */
			sn = *get_sainet();
			sn.sain_hisaddr = ipaddr;
			bcopy(&e_zero, &sn.sain_hisether,
			    sizeof (ether_addr_t));
#ifdef	DEBUG
			printf("arp'ing for: ");
			inet_print(ipaddr);
#endif	/* DEBUG */
			if (get_arp(&sn) != 0) {
				/* set destination to new server */
#ifdef DEBUG
				printf("New addr: SERVER:\n");
				inet_print(sn.sain_hisaddr);
				ether_print(sn.sain_hisether);
#endif /* DEBUG */
				set_netip(&(sn.sain_hisaddr), DESTIN);
				set_netether(&(sn.sain_hisether), DESTIN);
			} else {
				/*
				 * The ARP failed. If the router address
				 * is non-zero, let's try to reach our
				 * server through the router.
				 *
				 * We probably could use a net mask
				 * to determine if server is on current
				 * network. If so, then no use going to
				 * the router. What complicates the use
				 * of a internet mask is subnetting.
				 * Therefore, we'll punt, and simply go
				 * for the router.
				 */
				if (bcmp((caddr_t)&(router.sain_hisether),
				    (caddr_t)&(e_zero),
				    sizeof (ether_addr_t)) == 0) {
					/* no router */
					printf("Root Server: '%s' at:",
					    server_name);
					inet_print(ipaddr);
					printf("not responding.\n");
					return (FALSE);
				} else {
					/* try the router */
#ifdef	DEBUG
					printf("Cannot ARP Root server "
					    "'%s' ethernet address.\n",
					    server_name);
#endif	/* DEBUG */
					printf("Attempting to route "
					    "through gateway at: ");
					inet_print(router.sain_hisaddr);
					set_netip(&sn.sain_hisaddr, DESTIN);
					set_netether(&router.sain_hisether,
					    DESTIN);
				}
			}
		}
		break;
	default:
		printf("getfile: unknown address type %d\n",
		    res.server_address.address_type);
		return (FALSE);
	}
	return (TRUE);
}

/*
 * gethostname - returns 0 for success, -1 for failure. Used by auth_unix.c.
 *
 * Copies the hostname from the bootparam struct (whoami) into the
 * first argument. The second argument is the max acceptable length.
 */
int
gethostname(caddr_t ret, int max)
{
	/* call whoami() if necessary. */
	if (bp.client_name == NULL) {
		if (whoami() == FALSE)
			return (-1);
	}
	bzero(ret, max);
	bcopy(bp.client_name, ret, strlen(bp.client_name));
	return (0);
}

/*
 * XDR routines for bootparams.
 */

bool_t
xdr_bp_machine_name_t(XDR *xdrs, bp_machine_name_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_MACHINE_NAME));
}

bool_t
xdr_bp_path_t(XDR *xdrs, bp_path_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_PATH_LEN));
}

bool_t
xdr_bp_fileid_t(XDR *xdrs, bp_fileid_t *objp)
{
	return (xdr_string(xdrs, objp, MAX_FILEID));
}

bool_t
xdr_ip_addr_t(XDR *xdrs, ip_addr_t *objp)
{
	if (!xdr_char(xdrs, &objp->net))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->host))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->lh))
		return (FALSE);
	if (!xdr_char(xdrs, &objp->impno))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_address(XDR *xdrs, bp_address *objp)
{
	static struct xdr_discrim choices[] = {
		{ IP_ADDR_TYPE, xdr_ip_addr_t },
		{ __dontcare__, NULL }
	};

	return (xdr_union(xdrs, (enum_t *)&objp->address_type,
	    (char *)&objp->bp_address, choices, (xdrproc_t)NULL));
}

bool_t
xdr_bp_whoami_arg(XDR *xdrs, bp_whoami_arg *objp)
{
	return (xdr_bp_address(xdrs, &objp->client_address));
}

bool_t
xdr_bp_whoami_res(XDR *xdrs, bp_whoami_res *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->client_name))
		return (FALSE);
	if (!xdr_bp_machine_name_t(xdrs, &objp->domain_name))
		return (FALSE);
	if (!xdr_bp_address(xdrs, &objp->router_address))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_getfile_arg(XDR *xdrs, bp_getfile_arg *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->client_name))
		return (FALSE);
	if (!xdr_bp_fileid_t(xdrs, &objp->file_id))
		return (FALSE);
	return (TRUE);
}

bool_t
xdr_bp_getfile_res(XDR *xdrs, bp_getfile_res *objp)
{
	if (!xdr_bp_machine_name_t(xdrs, &objp->server_name))
		return (FALSE);
	if (!xdr_bp_address(xdrs, &objp->server_address))
		return (FALSE);
	if (!xdr_bp_path_t(xdrs, &objp->server_path))
		return (FALSE);
	return (TRUE);
}
