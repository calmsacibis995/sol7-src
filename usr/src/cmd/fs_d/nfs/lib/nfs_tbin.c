/*
 *	nfs_tbind.c
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)nfs_tbind.c	1.6	97/11/12 SMI"

#include <tiuser.h>
#include <fcntl.h>
#include <netconfig.h>
#include <stropts.h>
#include <errno.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#include <netdir.h>
#include <unistd.h>
#include <string.h>
#include <netinet/tcp.h>
#include <malloc.h>
#include <stdlib.h>
#include "nfs_tbind.h"

/*
 * Determine valid semantics for most applications.
 */
#define	OK_TPI_TYPE(_nconf) \
	(_nconf->nc_semantics == NC_TPI_CLTS || \
	_nconf->nc_semantics == NC_TPI_COTS || \
	_nconf->nc_semantics == NC_TPI_COTS_ORD)

/*
 * Borrow the first unused long field in the netconfig structure
 * to hold nfsd state.  This isn't declared in netconfig.h because
 * we don't want to expose this interface.
 */
#define	nc_closing	nc_unused[0]

#define	BE32_TO_U32(a)	((((u_long)((u_char *)a)[0] & 0xFF) << (u_long)24) | \
	(((u_long)((u_char *)a)[1] & 0xFF) << (u_long)16) | \
	(((u_long)((u_char *)a)[2] & 0xFF) << (u_long)8)  | \
	((u_long)((u_char *)a)[3] & 0xFF))

/*
 * Number of elements to add to the poll array on each allocation.
 */
#define	POLL_ARRAY_INC_SIZE	64

/*
 * Number of file descriptors by which the process soft limit may be
 * increased on each call to nofile_increase(0).
 */
#define	NOFILE_INC_SIZE	64

struct conn_ind {
	struct conn_ind *conn_next;
	struct conn_ind *conn_prev;
	struct t_call   *conn_call;
};

/*
 * this file contains transport routines common to nfsd and lockd
 */
static	int	nofile_increase(int);
static	int	reuseaddr(int);
static	void	add_to_poll_list(int, struct netconfig *);
static	char *	serv_name_to_port_name(char *);
static	int	bind_to_proto(char *, char *, struct netbuf **,
				struct netconfig **);
static	int	bind_to_provider(char *, char *, struct netbuf **,
					struct netconfig **);
static	void	conn_close_oldest(void);
static	boolean_t conn_get(int, struct netconfig *, struct conn_ind **);
static	void	cots_listen_event(int, int);
static	int	discon_get(int, struct netconfig *, struct conn_ind **);
static	int	do_poll_clts_action(int, int);
static	int	do_poll_cots_action(int, int);
static	void	remove_from_poll_list(int);
static	int	set_addrmask(int, struct netconfig *, struct netbuf *);

static	struct pollfd *poll_array;
static	struct netconfig *nconf_polled;
static	int	num_conns;		/* Current number of connections */

/*
 * Called to create and prepare a transport descriptor for in-kernel
 * RPC service.
 * Returns -1 on failure and a valid descriptor on success.
 */
int
nfslib_transport_open(struct netconfig *nconf)
{
	int fd;
	struct strioctl	strioc;

	if ((nconf == (struct netconfig *) NULL) ||
	    (nconf->nc_device == (char *) NULL)) {
		syslog(LOG_ERR, "no netconfig device");
		return (-1);
	}

	/*
	 * Open the transport device.
	 */
	fd = t_open(nconf->nc_device, O_RDWR, (struct t_info *)NULL);
	if (fd == -1) {
		if (t_errno == TSYSERR && errno == EMFILE &&
		    (nofile_increase(0) == 0)) {
			/* Try again with a higher NOFILE limit. */
			fd = t_open(nconf->nc_device, O_RDWR,
					(struct t_info *)NULL);
		}
		if (fd == -1) {
			syslog(LOG_ERR, "t_open %s failed:  t_errno %d, %m",
			    nconf->nc_device, t_errno);
			return (-1);
		}
	}

	/*
	 * Pop timod because the RPC module must be as close as possible
	 * to the transport.
	 */
	if (ioctl(fd, I_POP, 0) < 0) {
		syslog(LOG_ERR, "I_POP of timod failed: %m");
		(void) t_close(fd);
		return (-1);
	}

	if (nconf->nc_semantics == NC_TPI_CLTS) {
		/*
		 * Push rpcmod to filter data traffic to KRPC.
		 */
		if (ioctl(fd, I_PUSH, "rpcmod") < 0) {
			syslog(LOG_ERR, "I_PUSH of rpcmod failed: %m");
			(void) t_close(fd);
			return (-1);
		}
	} else {
		if (ioctl(fd, I_PUSH, "rpcmod") < 0) {
			syslog(LOG_ERR, "I_PUSH of CONS rpcmod failed: %m");
			(void) t_close(fd);
			return (-1);
		}

		strioc.ic_cmd = RPC_SERVER;
		strioc.ic_dp = (char *)0;
		strioc.ic_len = 0;
		strioc.ic_timout = -1;
		/* Tell CONS rpcmod to act like a server stream. */
		if (ioctl(fd, I_STR, &strioc) < 0) {
			syslog(LOG_ERR, "CONS rpcmod set-up ioctl failed: %m");
			(void) t_close(fd);
			return (-1);
		}
	}

	/*
	 * Re-push timod so that we will still be doing TLI
	 * operations on the descriptor.
	 */
	if (ioctl(fd, I_PUSH, "timod") < 0) {
		syslog(LOG_ERR, "I_PUSH of timod failed: %m");
		(void) t_close(fd);
		return (-1);
	}

	return (fd);
}

static int
nofile_increase(int limit)
{
	struct rlimit rl;

	if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		syslog(LOG_ERR, "getrlimit of NOFILE failed: %m");
		return (-1);
	}

	if (limit > 0)
		rl.rlim_cur = limit;
	else
		rl.rlim_cur += NOFILE_INC_SIZE;

	if (rl.rlim_cur > rl.rlim_max &&
	    rl.rlim_max != RLIM_INFINITY)
		rl.rlim_max = rl.rlim_cur;

	if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
		syslog(LOG_ERR, "setrlimit of NOFILE to %d failed: %m",
			rl.rlim_cur);
		return (-1);
	}

	return (0);
}

int
nfslib_bindit(struct netconfig *nconf, struct netbuf **addr,
	struct nd_hostserv *hs, int backlog)
{
	int fd;
	struct t_bind * ntb;
	struct t_bind tb;
	struct nd_addrlist *addrlist;
	struct t_optmgmt req, resp;
	struct opthdr *opt;
	char reqbuf[128];

	if ((fd = nfslib_transport_open(nconf)) == -1) {
		syslog(LOG_ERR, "cannot establish transport service over %s",
			nconf->nc_device);
		return (-1);
	}

	addrlist = (struct nd_addrlist *)NULL;
	if (netdir_getbyname(nconf, hs, &addrlist) != 0) {
		syslog(LOG_ERR,
		"Cannot get address for transport %s host %s service %s",
			nconf->nc_netid, hs->h_host, hs->h_serv);
		(void) t_close(fd);
		return (-1);
	}

	if (strcmp(nconf->nc_proto, "tcp") == 0) {
		/*
		 * If we're running over TCP, then set the
		 * SO_REUSEADDR option so that we can bind
		 * to our preferred address even if previously
		 * left connections exist in FIN_WAIT states.
		 * This is somewhat bogus, but otherwise you have
		 * to wait 2 minutes to restart after killing it.
		 */
		if (reuseaddr(fd) == -1) {
			syslog(LOG_WARNING,
			"couldn't set SO_REUSEADDR option on transport");
		}
	}

	if (nconf->nc_semantics == NC_TPI_CLTS)
		tb.qlen = 0;
	else
		tb.qlen = backlog;

	/* LINTED pointer alignment */
	ntb = (struct t_bind *) t_alloc(fd, T_BIND, T_ALL);
	if (ntb == (struct t_bind *) NULL) {
		syslog(LOG_ERR, "t_alloc failed:  t_errno %d, %m", t_errno);
		(void) t_close(fd);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		return (-1);
	}

	/*
	 * XXX - what about the space tb->addr.buf points to? This should
	 * be either a memcpy() to/from the buf fields, or t_alloc(fd,T_BIND,)
	 * should't be called with T_ALL.
	 */
	tb.addr = *(addrlist->n_addrs);		/* structure copy */

	if (t_bind(fd, &tb, ntb) == -1) {
		syslog(LOG_ERR, "t_bind failed:  t_errno %d, %m", t_errno);
		(void) t_free((char *) ntb, T_BIND);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		(void) t_close(fd);
		return (-1);
	}

	/* make sure we bound to the right address */
	if (tb.addr.len != ntb->addr.len ||
	    memcmp(tb.addr.buf, ntb->addr.buf, tb.addr.len) != 0) {
		syslog(LOG_ERR, "t_bind to wrong address");
		(void) t_free((char *) ntb, T_BIND);
		netdir_free((void *)addrlist, ND_ADDRLIST);
		(void) t_close(fd);
		return (-1);
	}

	*addr = &ntb->addr;
	netdir_free((void *)addrlist, ND_ADDRLIST);

	if (strcmp(nconf->nc_proto, "tcp") == 0) {
		/*
		 * Disable the Nagle algorithm on TCP connections.
		 * Connections accepted from this listener will
		 * inherit the listener options.
		 */

		/* LINTED pointer alignment */
		opt = (struct opthdr *)reqbuf;
		opt->level = IPPROTO_TCP;
		opt->name = TCP_NODELAY;
		opt->len = sizeof (int);

		/* LINTED pointer alignment */
		*(int *)((char *)opt + sizeof (*opt)) = 1;

		req.flags = T_NEGOTIATE;
		req.opt.len = sizeof (*opt) + opt->len;
		req.opt.buf = (char *)opt;
		resp.flags = 0;
		resp.opt.buf = reqbuf;
		resp.opt.maxlen = sizeof (reqbuf);

		if (t_optmgmt(fd, &req, &resp) < 0 ||
				resp.flags != T_SUCCESS) {
			syslog(LOG_ERR,
	"couldn't set NODELAY option for proto %s: t_errno = %d, %m",
				nconf->nc_proto, t_errno);
		}
	}

	return (fd);
}

static int
reuseaddr(int fd)
{
	struct t_optmgmt req, resp;
	struct opthdr *opt;
	char reqbuf[128];
	int *ip;

	/* LINTED pointer alignment */
	opt = (struct opthdr *)reqbuf;
	opt->level = SOL_SOCKET;
	opt->name = SO_REUSEADDR;
	opt->len = sizeof (int);

	/* LINTED pointer alignment */
	ip = (int *)&reqbuf[sizeof (struct opthdr)];
	*ip = 1;

	req.flags = T_NEGOTIATE;
	req.opt.len = sizeof (struct opthdr) + opt->len;
	req.opt.buf = (char *)opt;

	resp.flags = 0;
	resp.opt.buf = reqbuf;
	resp.opt.maxlen = sizeof (reqbuf);

	if (t_optmgmt(fd, &req, &resp) < 0 || resp.flags != T_SUCCESS) {
		t_error("t_optmgmt");
		return (-1);
	}
	return (0);
}

void
nfslib_log_tli_error(char *tli_name, int fd, struct netconfig *nconf)
{
	int error;

	/*
	 * Save the error code across syslog(), just in case syslog()
	 * gets its own error and, therefore, overwrites errno.
	 */
	error = errno;
	if (t_errno == TSYSERR) {
		syslog(LOG_ERR, "%s(file descriptor %d/transport %s) %m",
			tli_name, fd, nconf->nc_proto);
	} else {
		syslog(LOG_ERR,
			"%s(file descriptor %d/transport %s) TLI error %d",
			tli_name, fd, nconf->nc_proto, t_errno);
	}
	errno = error;
}

/*
 * Called to set up service over a particular transport.
 */
void
do_one(int nservers, char *provider, NETSELDECL(proto), struct protob *protobp0,
	int (*svc)(int, int, struct netbuf, struct netconfig *))
{
	register int sock;
	struct protob *protobp;
	struct netbuf *retaddr;
	struct netconfig *retnconf;
	struct netbuf addrmask;
	int vers;

	if (provider)
		sock = bind_to_provider(provider, protobp0->serv, &retaddr,
					&retnconf);
	else
		sock = bind_to_proto(proto, protobp0->serv, &retaddr,
					&retnconf);

	if (sock == -1) {
		(void) syslog(LOG_ERR,
	"Cannot establish %s service over %s: transport setup problem.",
			protobp0->serv, provider ? provider : proto);
		return;
	}

	if (set_addrmask(sock, retnconf, &addrmask) < 0) {
		(void) syslog(LOG_ERR,
		    "Cannot set address mask for %s", retnconf->nc_netid);
		return;
	}

	/*
	 * Register all versions of the programs in the protocol block list.
	 */
	for (protobp = protobp0; protobp; protobp = protobp->next) {
		for (vers = protobp->versmin; vers <= protobp->versmax;
			vers++) {
			(void) rpcb_unset(protobp->program, vers, retnconf);
			(void) rpcb_set(protobp->program, vers, retnconf,
					retaddr);
		}
	}

	if (retnconf->nc_semantics == NC_TPI_CLTS) {
		/* Don't drop core if supporting module(s) aren't loaded. */
		(void) signal(SIGSYS, SIG_IGN);

		/*
		 * svc() doesn't block, it returns success or failure.
		 */
		if ((*svc)(nservers, sock, addrmask, retnconf) < 0) {
			(void) syslog(LOG_ERR,
"Cannot establish %s service over <file desc. %d, protocol %s> : %m. Exiting",
				protobp0->serv, sock, retnconf->nc_proto);
			exit(1);
		}
	}

	/*
	 * We successfully set up nservers copies of the server over this
	 * transport. Add this descriptor to the one being polled on.
	 */
	add_to_poll_list(sock, retnconf);
}

/*
 * Set up the NFS service over all the available transports.
 * Returns -1 for failure, 0 for success.
 */
int
do_all(int nservers, struct protob *protobp, int (*svc)())
{
	struct netconfig *nconf;
	NCONF_HANDLE *nc;

	if ((nc = setnetconfig()) == (NCONF_HANDLE *)NULL) {
		syslog(LOG_ERR, "setnetconfig failed: %m");
		return (-1);
	}
	while (nconf = getnetconfig(nc)) {
		if ((nconf->nc_flag & NC_VISIBLE) &&
		    strcmp(nconf->nc_protofmly, "loopback") != 0 &&
		    OK_TPI_TYPE(nconf))
			do_one(nservers, nconf->nc_device, nconf->nc_proto,
				protobp, svc);
	}
	(void) endnetconfig(nc);
	return (0);
}

/*
 * poll on the open transport descriptors for events and errors.
 */
void
poll_for_action(void)
{
	int nfds;
	int i;

	/*
	 * Keep polling until all transports have been closed. When this
	 * happens, we return.
	 */
	while ((int)num_fds > 0) {

		nfds = poll(poll_array, num_fds, INFTIM);
		switch (nfds) {
		case 0:
			continue;

		case -1:
			/*
			 * Some errors from poll could be
			 * due to temporary conditions, and we try to
			 * be robust in the face of them. Other
			 * errors (should never happen in theory)
			 * are fatal (eg. EINVAL, EFAULT).
			 */
			switch (errno) {
			case EAGAIN:
			case ENOMEM:
				(void) sleep(10);
				continue;
			default:
				(void) syslog(LOG_ERR,
						"poll failed: %m. Exiting");
				exit(1);
			}
		default:
			break;
		}

		/*
		 * Go through the poll list looking for events.
		 */
		for (i = 0; i < num_fds && nfds > 0; i++) {
			if (poll_array[i].revents) {
				nfds--;
				/*
				 * We have a message, so try to read it.
				 * Record the error return in errno,
				 * so that syslog(LOG_ERR, "...%m")
				 * dumps the corresponding error string.
				 */
				if (nconf_polled[i].nc_semantics ==
				    NC_TPI_CLTS) {
					errno = do_poll_clts_action(
							poll_array[i].fd, i);
				} else {
					errno = do_poll_cots_action(
							poll_array[i].fd, i);
				}

				if (errno == 0)
					continue;
				/*
				 * Most returned error codes mean that there is
				 * fatal condition which we can only deal with
				 * by closing the transport.
				 */
				if (errno != EAGAIN && errno != ENOMEM) {
					(void) syslog(LOG_ERR,
		"Error (%m) reading descriptor %d/transport %s. Closing it.",
						poll_array[i].fd,
						nconf_polled[i].nc_proto);
					(void) t_close(poll_array[i].fd);
					remove_from_poll_list(poll_array[i].fd);

				} else if (errno == ENOMEM)
					(void) sleep(5);
			}
		}
	}

	(void) syslog(LOG_ERR,
		"All transports have been closed with errors. Exiting.");
}

/*
 * Allocate poll/transport array entries for this descriptor.
 */
static void
add_to_poll_list(int fd, struct netconfig *nconf)
{
	static int poll_array_size = 0;

	/*
	 * If the arrays are full, allocate new ones.
	 */
	if (num_fds == poll_array_size) {
		struct pollfd *tpa;
		struct netconfig *tnp;

		if (poll_array_size != 0) {
			tpa = poll_array;
			tnp = nconf_polled;
		} else
			tpa = (struct pollfd *)0;

		poll_array_size += POLL_ARRAY_INC_SIZE;
		/*
		 * Allocate new arrays.
		 */
		poll_array = (struct pollfd *)
		    malloc(poll_array_size * sizeof (struct pollfd) + 256);
		nconf_polled = (struct netconfig *)
		    malloc(poll_array_size * sizeof (struct netconfig) + 256);
		if (poll_array == (struct pollfd *)NULL ||
		    nconf_polled == (struct netconfig *)NULL) {
			syslog(LOG_ERR, "malloc failed for poll array");
			exit(1);
		}

		/*
		 * Copy the data of the old ones into new arrays, and
		 * free the old ones.
		 */
		if (tpa) {
			(void) memcpy((void *)poll_array, (void *)tpa,
				num_fds * sizeof (struct pollfd));
			(void) memcpy((void *)nconf_polled, (void *)tnp,
				num_fds * sizeof (struct netconfig));
			free((void *)tpa);
			free((void *)tnp);
		}
	}

	/*
	 * Set the descriptor and event list. All possible events are
	 * polled for.
	 */
	poll_array[num_fds].fd = fd;
	poll_array[num_fds].events = POLLIN|POLLRDNORM|POLLRDBAND|POLLPRI;

	/*
	 * Copy the transport data over too.
	 */
	nconf_polled[num_fds] = *nconf;
	nconf_polled[num_fds].nc_closing = 0;

	/*
	 * Set the descriptor to non-blocking. Avoids a race
	 * between data arriving on the stream and then having it
	 * flushed before we can read it.
	 */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		(void) syslog(LOG_ERR,
	"fcntl(file desc. %d/transport %s, F_SETFL, O_NONBLOCK): %m. Exiting",
			num_fds, nconf->nc_proto);
		exit(1);
	}

	/*
	 * Count this descriptor.
	 */
	++num_fds;
}

static void
remove_from_poll_list(int fd)
{
	int i;
	int num_to_copy;

	for (i = 0; i < num_fds; i++) {
		if (poll_array[i].fd == fd) {
			--num_fds;
			num_to_copy = num_fds - i;
			(void) memcpy((void *)&poll_array[i],
				(void *)&poll_array[i+1],
				num_to_copy * sizeof (struct pollfd));
			(void) memset((void *)&poll_array[num_fds], 0,
				sizeof (struct pollfd));
			(void) memcpy((void *)&nconf_polled[i],
				(void *)&nconf_polled[i+1],
				num_to_copy * sizeof (struct netconfig));
			(void) memset((void *)&nconf_polled[num_fds], 0,
				sizeof (struct netconfig));
			return;
		}
	}
	syslog(LOG_ERR, "attempt to remove nonexistent fd from poll list");

}

/*
 * Called to read and interpret the event on a connectionless descriptor.
 * Returns 0 if successful, or a UNIX error code if failure.
 */
static int
do_poll_clts_action(int fd, int nconf_index)
{
	int error;
	int ret;
	int flags;
	struct netconfig *nconf = &nconf_polled[nconf_index];
	static struct t_unitdata *unitdata = NULL;
	static struct t_uderr *uderr = NULL;
	static int oldfd = -1;
	struct nd_hostservlist *host = NULL;
	struct strbuf ctl[1], data[1];
	/*
	 * We just need to have some space to consume the
	 * message in the event we can't use the TLI interface to do the
	 * job.
	 *
	 * We flush the message using getmsg(). For the control part
	 * we allocate enough for any TPI header plus 32 bytes for address
	 * and options. For the data part, there is nothing magic about
	 * the size of the array, but 256 bytes is probably better than
	 * 1 byte, and we don't expect any data portion anyway.
	 *
	 * If the array sizes are too small, we handle this because getmsg()
	 * (called to consume the message) will return MOREDATA|MORECTL.
	 * Thus we just call getmsg() until it's read the message.
	 */
	char ctlbuf[sizeof (union T_primitives) + 32];
	char databuf[256];

	/*
	 * If this is the same descriptor as the last time
	 * do_poll_clts_action was called, we can save some
	 * de-allocation and allocation.
	 */
	if (oldfd != fd) {
		oldfd = fd;

		if (unitdata) {
			(void) t_free((char *)unitdata, T_UNITDATA);
			unitdata = NULL;
		}
		if (uderr) {
			(void) t_free((char *)uderr, T_UDERROR);
			uderr = NULL;
		}
	}

	/*
	 * Allocate a unitdata structure for receiving the event.
	 */
	if (unitdata == NULL) {
		/* LINTED pointer alignment */
		unitdata = (struct t_unitdata *)t_alloc(fd, T_UNITDATA, T_ALL);
		if (unitdata == NULL) {
			if (t_errno == TSYSERR) {
				/*
				 * Save the error code across
				 * syslog(), just in case
				 * syslog() gets its own error
				 * and therefore overwrites errno.
				 */
				error = errno;
				(void) syslog(LOG_ERR,
	"t_alloc(file descriptor %d/transport %s, T_UNITDATA) failed: %m",
					fd, nconf->nc_proto);
				return (error);
			}
			(void) syslog(LOG_ERR,
"t_alloc(file descriptor %d/transport %s, T_UNITDATA) failed TLI error %d",
					fd, nconf->nc_proto, t_errno);
			goto flush_it;
		}
	}

try_again:
	flags = 0;

	/*
	 * The idea is we wait for T_UNITDATA_IND's. Of course,
	 * we don't get any, because rpcmod filters them out.
	 * However, we need to call t_rcvudata() to let TLI
	 * tell us we have a T_UDERROR_IND.
	 *
	 * algorithm is:
	 * 	t_rcvudata(), expecting TLOOK.
	 * 	t_look(), expecting T_UDERR.
	 * 	t_rcvuderr(), expecting success (0).
	 * 	expand destination address into ASCII,
	 *	and dump it.
	 */

	ret = t_rcvudata(fd, unitdata, &flags);
	if (ret == 0 || t_errno == TBUFOVFLW) {
		(void) syslog(LOG_WARNING,
"t_rcvudata(file descriptor %d/transport %s) got unexpected data, %d bytes",
			fd, nconf->nc_proto, unitdata->udata.len);

		/*
		 * Even though we don't expect any data, in case we do,
		 * keep reading until there is no more.
		 */
		if (flags & T_MORE)
			goto try_again;

		return (0);
	}

	switch (t_errno) {
	case TNODATA:
		return (0);
	case TSYSERR:
		/*
		 * System errors are returned to caller.
		 * Save the error code across
		 * syslog(), just in case
		 * syslog() gets its own error
		 * and therefore overwrites errno.
		 */
		error = errno;
		(void) syslog(LOG_ERR,
			"t_rcvudata(file descriptor %d/transport %s) %m",
			fd, nconf->nc_proto);
		return (error);
	case TLOOK:
		break;
	default:
		(void) syslog(LOG_ERR,
		"t_rcvudata(file descriptor %d/transport %s) TLI error %d",
			fd, nconf->nc_proto, t_errno);
		goto flush_it;
	}

	ret = t_look(fd);
	switch (ret) {
	case 0:
		return (0);
	case -1:
		/*
		 * System errors are returned to caller.
		 */
		if (t_errno == TSYSERR) {
			/*
			 * Save the error code across
			 * syslog(), just in case
			 * syslog() gets its own error
			 * and therefore overwrites errno.
			 */
			error = errno;
			(void) syslog(LOG_ERR,
				"t_look(file descriptor %d/transport %s) %m",
				fd, nconf->nc_proto);
			return (error);
		}
		(void) syslog(LOG_ERR,
			"t_look(file descriptor %d/transport %s) TLI error %d",
			fd, nconf->nc_proto, t_errno);
		goto flush_it;
	case T_UDERR:
		break;
	default:
		(void) syslog(LOG_WARNING,
	"t_look(file descriptor %d/transport %s) returned %d not T_UDERR (%d)",
			fd, nconf->nc_proto, ret, T_UDERR);
	}

	if (uderr == NULL) {
		/* LINTED pointer alignment */
		uderr = (struct t_uderr *)t_alloc(fd, T_UDERROR, T_ALL);
		if (uderr == NULL) {
			if (t_errno == TSYSERR) {
				/*
				 * Save the error code across
				 * syslog(), just in case
				 * syslog() gets its own error
				 * and therefore overwrites errno.
				 */
				error = errno;
				(void) syslog(LOG_ERR,
	"t_alloc(file descriptor %d/transport %s, T_UDERROR) failed: %m",
					fd, nconf->nc_proto);
				return (error);
			}
			(void) syslog(LOG_ERR,
"t_alloc(file descriptor %d/transport %s, T_UDERROR) failed TLI error: %d",
				fd, nconf->nc_proto, t_errno);
			goto flush_it;
		}
	}

	ret = t_rcvuderr(fd, uderr);
	if (ret == 0) {

		/*
		 * Save the datagram error in errno, so that the
		 * %m argument to syslog picks up the error string.
		 */
		errno = uderr->error;

		/*
		 * Log the datagram error, then log the host that
		 * probably triggerred. Cannot log both in the
		 * same transaction because of packet size limitations
		 * in /dev/log.
		 */
		(void) syslog((errno == ECONNREFUSED) ? LOG_DEBUG : LOG_WARNING,
"NFS response over <file descriptor %d/transport %s> generated error: %m",
			fd, nconf->nc_proto);

		/*
		 * Try to map the client's address back to a
		 * name.
		 */
		ret = netdir_getbyaddr(nconf, &host, &uderr->addr);
		if (ret != -1 && host && host->h_cnt > 0 &&
		    host->h_hostservs) {
		(void) syslog((errno == ECONNREFUSED) ? LOG_DEBUG : LOG_WARNING,
"Bad NFS response was sent to client with host name: %s; service port: %s",
				host->h_hostservs->h_host,
				host->h_hostservs->h_serv);
		} else {
			int i, j;
			char *buf;
			char *hex = "0123456789abcdef";

			/*
			 * Mapping failed, print the whole thing
			 * in ASCII hex.
			 */
			buf = (char *)malloc(uderr->addr.len * 2 + 1);
			for (i = 0, j = 0; i < uderr->addr.len; i++, j += 2) {
				buf[j] = hex[((uderr->addr.buf[i]) >> 4) & 0xf];
				buf[j+1] = hex[uderr->addr.buf[i] & 0xf];
			}
			buf[j] = '\0';
		(void) syslog((errno == ECONNREFUSED) ? LOG_DEBUG : LOG_WARNING,
	"Bad NFS response was sent to client with transport address: 0x%s",
				buf);
			free((void *)buf);
		}

		if (ret == 0 && host != NULL)
			netdir_free((void *)host, ND_HOSTSERVLIST);
		return (0);
	}

	switch (t_errno) {
	case TNOUDERR:
		goto flush_it;
	case TSYSERR:
		/*
		 * System errors are returned to caller.
		 * Save the error code across
		 * syslog(), just in case
		 * syslog() gets its own error
		 * and therefore overwrites errno.
		 */
		error = errno;
		(void) syslog(LOG_ERR,
			"t_rcvuderr(file descriptor %d/transport %s) %m",
			fd, nconf->nc_proto);
		return (error);
	default:
		(void) syslog(LOG_ERR,
		"t_rcvuderr(file descriptor %d/transport %s) TLI error %d",
			fd, nconf->nc_proto, t_errno);
		goto flush_it;
	}

flush_it:
	/*
	 * If we get here, then we could not cope with whatever message
	 * we attempted to read, so flush it. If we did read a message,
	 * and one isn't present, that is all right, because fd is in
	 * nonblocking mode.
	 */
	(void) syslog(LOG_ERR,
	"Flushing one input message from <file descriptor %d/transport %s>",
		fd, nconf->nc_proto);

	/*
	 * Read and discard the message. Do this this until there is
	 * no more control/data in the message or until we get an error.
	 */
	do {
		ctl->maxlen = sizeof (ctlbuf);
		ctl->buf = ctlbuf;
		data->maxlen = sizeof (databuf);
		data->buf = databuf;
		flags = 0;
		ret = getmsg(fd, ctl, data, &flags);
		if (ret == -1)
			return (errno);
	} while (ret != 0);

	return (0);
}

static void
conn_close_oldest(void)
{
	int fd;
	int i1;

	/*
	 * Find the oldest connection that is not already in the
	 * process of shutting down.
	 */
	for (i1 = end_listen_fds; /* no conditional expression */; i1++) {
		if (i1 >= num_fds)
			return;
		if (nconf_polled[i1].nc_closing == 0)
			break;
	}
#ifdef DEBUG
	printf("too many connections (%d), releasing oldest (%d)\n",
		num_conns, poll_array[i1].fd);
#else
	syslog(LOG_WARNING, "too many connections (%d), releasing oldest (%d)",
		num_conns, poll_array[i1].fd);
#endif
	fd = poll_array[i1].fd;
	if (nconf_polled[i1].nc_semantics == NC_TPI_COTS) {
		/*
		 * For politeness, send a T_DISCON_REQ to the transport
		 * provider.  We close the stream anyway.
		 */
		(void) t_snddis(fd, (struct t_call *)0);
		num_conns--;
		remove_from_poll_list(fd);
		(void) t_close(fd);
	} else {
		/*
		 * For orderly release, we do not close the stream
		 * until the T_ORDREL_IND arrives to complete
		 * the handshake.
		 */
		if (t_sndrel(fd) == 0)
			nconf_polled[i1].nc_closing = 1;
	}
}

static boolean_t
conn_get(int fd, struct netconfig *nconf, struct conn_ind **connp)
{
	struct conn_ind	*conn;
	struct conn_ind	*next_conn;

	conn = (struct conn_ind *)malloc(sizeof (*conn));
	if (conn == NULL) {
		syslog(LOG_ERR, "malloc for listen indication failed");
		return (FALSE);
	}

	/* LINTED pointer alignment */
	conn->conn_call = (struct t_call *)t_alloc(fd, T_CALL, T_ALL);
	if (conn->conn_call == NULL) {
		free((char *)conn);
		nfslib_log_tli_error("t_alloc", fd, nconf);
		return (FALSE);
	}

	if (t_listen(fd, conn->conn_call) == -1) {
		nfslib_log_tli_error("t_listen", fd, nconf);
		(void) t_free((char *)conn->conn_call, T_CALL);
		free((char *)conn);
		return (FALSE);
	}

	if (conn->conn_call->udata.len > 0) {
		syslog(LOG_WARNING,
	"rejecting inbound connection(%s) with %d bytes of connect data",
			nconf->nc_proto, conn->conn_call->udata.len);

		conn->conn_call->udata.len = 0;
		(void) t_snddis(fd, conn->conn_call);
		(void) t_free((char *)conn->conn_call, T_CALL);
		free((char *)conn);
		return (FALSE);
	}

	if ((next_conn = *connp) != NULL) {
		next_conn->conn_prev->conn_next = conn;
		conn->conn_next = next_conn;
		conn->conn_prev = next_conn->conn_prev;
		next_conn->conn_prev = conn;
	} else {
		conn->conn_next = conn;
		conn->conn_prev = conn;
		*connp = conn;
	}
	return (TRUE);
}

static int
discon_get(int fd, struct netconfig *nconf, struct conn_ind **connp)
{
	struct conn_ind	*conn;
	struct t_discon	discon;

	discon.udata.buf = (char *)0;
	discon.udata.maxlen = 0;
	if (t_rcvdis(fd, &discon) == -1) {
		nfslib_log_tli_error("t_rcvdis", fd, nconf);
		return (-1);
	}

	conn = *connp;
	if (conn == NULL)
		return (0);

	do {
		if (conn->conn_call->sequence == discon.sequence) {
			if (conn == *connp && conn->conn_next == conn)
				*connp = (struct conn_ind *)0;
			else {
				conn->conn_next->conn_prev = conn->conn_prev;
				conn->conn_prev->conn_next = conn->conn_next;
			}
			break;
		}
		conn = conn->conn_next;
	} while (conn != *connp);

	return (0);
}

static void
cots_listen_event(int fd, int nconf_index)
{
	struct t_call *call;
	struct conn_ind	*conn;
	struct conn_ind	*conn_head;
	int event;
	struct netconfig *nconf = &nconf_polled[nconf_index];
	int new_fd;
	struct netbuf addrmask;

	conn_head = (struct conn_ind *)0;
	(void) conn_get(fd, nconf, &conn_head);

	while ((conn = conn_head) != NULL) {
		conn_head = conn->conn_next;
		if (conn_head == conn)
			conn_head = (struct conn_ind *)0;
		else {
			conn_head->conn_prev = conn->conn_prev;
			conn->conn_prev->conn_next = conn_head;
		}
		call = conn->conn_call;
		free((char *)conn);

		/*
		 * If we have already accepted the maximum number of
		 * connections allowed on the command line, then drop
		 * the oldest connection (for any protocol) before
		 * accepting the new connection.  Unless explicitly
		 * set on the command line, max_conns_allowed is -1.
		 */
		if (max_conns_allowed != -1 && num_conns >= max_conns_allowed)
			conn_close_oldest();

		/*
		 * Create a new transport endpoint for the same proto as
		 * the listener.
		 */
		new_fd = nfslib_transport_open(nconf);
		if (new_fd == -1) {
			call->udata.len = 0;
			(void) t_snddis(fd, call);
			(void) t_free((char *)call, T_CALL);
			syslog(LOG_ERR, "Cannot establish transport over %s",
				nconf->nc_device);
			continue;
		}

		/* Bind to a generic address/port for the accepting stream. */
		if (t_bind(new_fd, (struct t_bind *)NULL,
		    (struct t_bind *)NULL) == -1) {
			nfslib_log_tli_error("t_bind", new_fd, nconf);
			call->udata.len = 0;
			(void) t_snddis(fd, call);
			(void) t_free((char *)call, T_CALL);
			(void) t_close(new_fd);
			continue;
		}

		while (t_accept(fd, new_fd, call) == -1) {
			if (t_errno != TLOOK) {
				nfslib_log_tli_error("t_accept", fd, nconf);
				call->udata.len = 0;
				(void) t_snddis(fd, call);
				(void) t_free((char *)call, T_CALL);
				(void) t_close(new_fd);
				goto do_next_conn;
			}
			while (event = t_look(fd)) {
				switch (event) {
				case T_LISTEN:
#ifdef DEBUG
					printf(
"cots_listen_event(%s): T_LISTEN during accept processing\n", nconf->nc_proto);
#endif
					(void) conn_get(fd, nconf, &conn_head);
					continue;
				case T_DISCONNECT:
#ifdef DEBUG
					printf(
	"cots_listen_event(%s): T_DISCONNECT during accept processing\n",
						nconf->nc_proto);
#endif
					(void) discon_get(fd, nconf,
								&conn_head);
					continue;
				default:
					syslog(LOG_ERR,
			"unexpected event 0x%x during accept processing (%s)",
						event, nconf->nc_proto);
					call->udata.len = 0;
					(void) t_snddis(fd, call);
					(void) t_free((char *)call, T_CALL);
					(void) t_close(new_fd);
					goto do_next_conn;
				}
			}
		}

		if (set_addrmask(new_fd, nconf, &addrmask) < 0) {
			(void) syslog(LOG_ERR,
			    "Cannot set address mask for %s",
				nconf->nc_netid);
			return;
		}

		/* Tell KRPC about the new stream. */
		(*Mysvc)(num_servers, new_fd, addrmask, nconf);

		free(addrmask.buf);
		(void) t_free((char *)call, T_CALL);
		(void) t_free((char *)call, T_CALL);

		/*
		 * Poll on the new descriptor so that we get disconnect
		 * and orderly release indications.
		 */
		num_conns++;
		add_to_poll_list(new_fd, nconf);

		/* Reset nconf in case it has been moved. */
		nconf = &nconf_polled[nconf_index];
do_next_conn:;
	}
}

static int
do_poll_cots_action(int fd, int nconf_index)
{
	char buf[256];
	int event;
	int i1;
	int flags;
	struct netconfig *nconf = &nconf_polled[nconf_index];

	while (event = t_look(fd)) {
		switch (event) {
		case T_LISTEN:
#ifdef DEBUG
printf("do_poll_cots_action(%s,%d): T_LISTEN event\n", nconf->nc_proto, fd);
#endif
			cots_listen_event(fd, nconf_index);
			break;

		case T_DATA:
#ifdef DEBUG
printf("do_poll_cots_action(%d,%s): T_DATA event\n", fd, nconf->nc_proto);
#endif
			/*
			 * Receive a private notification from CONS rpcmod.
			 */
			i1 = t_rcv(fd, buf, sizeof (buf), &flags);
			if (i1 == -1) {
				syslog(LOG_ERR, "t_rcv failed");
				break;
			}
			if (i1 < sizeof (int))
				break;
			i1 = BE32_TO_U32(buf);
			if (i1 == 1 || i1 == 2) {
				/*
				 * This connection has been idle for too long,
				 * so release it as politely as we can.  If we
				 * have already initiated an orderly release
				 * and we get notified that the stream is
				 * still idle, pull the plug.  This prevents
				 * hung connections from continuing to consume
				 * resources.
				 */
#ifdef DEBUG
printf("do_poll_cots_action(%s,%d): ", nconf->nc_proto, fd);
printf("initiating orderly release of idle connection\n");
#endif
				if (nconf->nc_semantics == NC_TPI_COTS ||
					nconf->nc_closing != 0) {
					(void) t_snddis(fd, (struct t_call *)0);
					goto fdclose;
				}
				/*
				 * For NC_TPI_COTS_ORD, the stream is closed
				 * and removed from the poll list when the
				 * T_ORDREL is received from the provider.  We
				 * don't wait for it here because it may take
				 * a while for the transport to shut down.
				 */
				if (t_sndrel(fd) == -1) {
					syslog(LOG_ERR,
					"unable to send orderly release %m");
				}
				nconf->nc_closing = 1;
			} else
				syslog(LOG_ERR,
				"unexpected event from CONS rpcmod %d", i1);
			break;

		case T_ORDREL:
#ifdef DEBUG
printf("do_poll_cots_action(%s,%d): T_ORDREL event\n", nconf->nc_proto, fd);
#endif
			/* Perform an orderly release. */
			if (t_rcvrel(fd) == 0)
				(void) t_sndrel(fd);
			else
				nfslib_log_tli_error("t_rcvrel", fd, nconf);
			goto fdclose;

		case T_DISCONNECT:
#ifdef DEBUG
printf("do_poll_cots_action(%s,%d): T_DISCONNECT event\n", nconf->nc_proto, fd);
#endif
			if (t_rcvdis(fd, (struct t_discon *)NULL) == -1)
				nfslib_log_tli_error("t_rcvdis", fd, nconf);
			goto fdclose;

		case T_ERROR:
		default:
			syslog(LOG_ERR,
	"unexpected TLI event (0x%x) on connection-oriented transport(%s,%d)",
				event, nconf->nc_proto, fd);
fdclose:
			num_conns--;
			remove_from_poll_list(fd);
			(void) t_close(fd);
			return (0);
		}
	}

	return (0);
}

static char *
serv_name_to_port_name(char *name)
{
	/*
	 * Map service names (used primarily in logging) to
	 * RPC port names (used by netdir_*() routines).
	 */
	if (strcmp(name, "NFS") == 0) {
		return ("nfs");
	} else if (strcmp(name, "NLM") == 0) {
		return ("lockd");
	}

	return ("unrecognized");
}

static int
bind_to_provider(char *provider, char *serv, struct netbuf **addr,
		struct netconfig **retnconf)
{
	struct netconfig *nconf;
	NCONF_HANDLE *nc;
	struct nd_hostserv hs;

	hs.h_host = HOST_SELF;
	hs.h_serv = serv_name_to_port_name(serv);

	if ((nc = setnetconfig()) == (NCONF_HANDLE *)NULL) {
		syslog(LOG_ERR, "setnetconfig failed: %m");
		return (-1);
	}
	while (nconf = getnetconfig(nc)) {
		if (OK_TPI_TYPE(nconf) &&
		    strcmp(nconf->nc_device, provider) == 0) {
			*retnconf = nconf;
			return (nfslib_bindit(nconf, addr, &hs,
					listen_backlog));
		}
	}
	(void) endnetconfig(nc);

	syslog(LOG_ERR, "couldn't find netconfig entry for provider %s",
	    provider);
	return (-1);
}

static int
bind_to_proto(NETSELDECL(proto), char *serv, struct netbuf **addr,
		struct netconfig **retnconf)
{
	struct netconfig *nconf;
	NCONF_HANDLE *nc = NULL;
	struct nd_hostserv hs;

	hs.h_host = HOST_SELF;
	hs.h_serv = serv_name_to_port_name(serv);

	if ((nc = setnetconfig()) == (NCONF_HANDLE *)NULL) {
		syslog(LOG_ERR, "setnetconfig failed: %m");
		return (-1);
	}
	while (nconf = getnetconfig(nc)) {
		if (OK_TPI_TYPE(nconf) && NETSELEQ(nconf->nc_proto, proto)) {
			*retnconf = nconf;
			return (nfslib_bindit(nconf, addr, &hs,
					listen_backlog));
		}
	}
	(void) endnetconfig(nc);

	syslog(LOG_ERR, "couldn't find netconfig entry for protocol %s",
	    proto);
	return (-1);
}

#include <netinet/in.h>

/*
 * Create an address mask appropriate for the transport.
 * The mask is used to obtain the host-specific part of
 * a network address when comparing addresses.
 * For an internet address the host-specific part is just
 * the 32 bit IP address and this part of the mask is set
 * to all-ones. The port number part of the mask is zeroes.
 */
static int
set_addrmask(fd, nconf, mask)
	struct netconfig *nconf;
	struct netbuf *mask;
{
	struct t_info info;

	/*
	 * Find the size of the address we need to mask.
	 */
	if (t_getinfo(fd, &info) < 0) {
		t_error("t_getinfo");
		return (-1);
	}
	mask->len = mask->maxlen = info.addr;
	if (mask->len <= 0) {
		syslog(LOG_ERR, "set_addrmask: address size: %d",
			mask->len);
		return (-1);
	}

	mask->buf = (char *)malloc(mask->len);
	if (mask->buf == NULL) {
		syslog(LOG_ERR, "set_addrmask: no memory");
		return (-1);
	}
	(void) memset(mask->buf, 0, mask->len);	/* reset all mask bits */

	if (strcmp(nconf->nc_protofmly, NC_INET) == 0) {
		/*
		 * Set the mask so that the port is ignored.
		 */
		/* LINTED pointer alignment */
		((struct sockaddr_in *)mask->buf)->sin_addr.s_addr =
								(u_long)~0;
		/* LINTED pointer alignment */
		((struct sockaddr_in *)mask->buf)->sin_family =
								(u_short)~0;
	} else {
		/*
		 * Set all mask bits.
		 */
		(void) memset(mask->buf, 0xFF, mask->len);
	}
	return (0);
}
