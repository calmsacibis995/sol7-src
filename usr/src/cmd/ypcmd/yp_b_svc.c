/*
 * Copyright (c) 1986-1993 by Sun Microsystems Inc.
 */

#ident	"@(#)yp_b_svc.c	1.23	97/12/05 SMI"

#ident	"@(#)yp_b_svc.c	1.23	97/12/05 SMI"

#include <stdio.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <memory.h>
#include <netconfig.h>
#include <syslog.h>
#include <rpcsvc/yp_prot.h>
#include "yp_b.h"
#include <sys/resource.h>
#include <sys/stropts.h>
#include <unistd.h>
#include <rpc/nettype.h>
#include <string.h>
#include <tiuser.h>


#ifdef DEBUG
#define	RPC_SVC_FG
#endif

#define	_RPCSVC_CLOSEDOWN 120
#define	YPBIND_ERR_ERR 1		/* Internal error */
#define	YPBIND_ERR_NOSERV 2		/* No bound server for passed domain */
#define	YPBIND_ERR_RESC 3		/* System resource allocation failure */
#define	YPBIND_ERR_NODOMAIN 4		/* Domain doesn't exist */

static	int _rpcpmstart;	/* Started by a port monitor ? */
static	int _rpcsvcdirty;	/* Still serving ? */
int	setok = YPSETNONE;	/* who is allowed to ypset */
int	broadcast = 0;
int	cache_okay = 0;		/* if set, then bindings are cached in files */

extern int sigcld_event;
extern void broadcast_proc_exit();
extern int __rpc_negotiate_uid();
extern bool_t __rpcbind_is_up();
extern void ypbind_init_default();
static void set_signal_handlers();
static void clear_bindings();
static void unregister(int);
void closedown();
void ypbindprog_3();
void ypbindprog_2();
void msgout();
extern void cache_transport();
extern void clean_cache();

main(argc, argv)
	int argc;
	char **argv;
{
	pid_t pid;
	int i;
	int pfd[2];
	char domain[256], servers[300];
	char **Argv = argv;
	struct netconfig *nconf;
	void *nc_handle;
	int loopback_found = 0, udp_found = 0;
	int pipe_closed = 0;
	struct rlimit rl;

	if (geteuid() != 0) {
		(void) fprintf(stderr, "must be root to run %s\n", argv[0]);
		exit(1);
	}

	argc--;
	argv++;

	while (argc > 0) {
		if (strcmp(*argv, "-ypset") == 0) {
			setok = YPSETALL;
		} else if (strcmp(*argv, "-ypsetme") == 0) {
			setok = YPSETLOCAL;
		} else if (strcmp(*argv, "-broadcast") == 0) {
			broadcast = TRUE;
		} else {
			fprintf(stderr,
		"usage: ypbind [-broadcast] [-ypset] [-ypsetme]\n");
			exit(1);
		}
		argc--,
		argv++;
	}

	if (setok == YPSETALL) {
		fprintf(stderr,
	"ypbind -ypset: allowing ypset! (this is REALLY insecure)\n");
	}
	if (setok == YPSETLOCAL) {
		fprintf(stderr,
	"ypbind -ypsetme: allowing local ypset! (this is insecure)\n");
	}
	if (broadcast == TRUE) {
		fprintf(stderr,
			"ypbind -broadcast: allowing broadcast! \
(insecure and transport dependent)\n");
	}

	if (getdomainname(domain, sizeof (domain)) == 0) {
		sprintf(servers, "%s/%s/ypservers", BINDING, domain);
		if (!broadcast && access(servers, R_OK) != 0) {
			(void) fprintf(stderr,
		"%s: no info on servers - run ypinit -c\n", Argv[0]);
			exit(1);
		}
	} else {
		(void) fprintf(stderr, "%s: domainname not set - exiting\n",
			Argv[0]);
		exit(1);
	}

	getrlimit(RLIMIT_NOFILE, &rl);
	rl.rlim_cur = rl.rlim_max;
	setrlimit(RLIMIT_NOFILE, &rl);

	openlog("ypbind", LOG_PID, LOG_DAEMON);

	/*
	 * If stdin looks like a TLI endpoint, we assume
	 * that we were started by a port monitor. If
	 * t_getstate fails with TBADF, this is not a
	 * TLI endpoint.
	 */
	_rpcpmstart = (t_getstate(0) != -1 || t_errno != TBADF);

	if (!__rpcbind_is_up()) {
		msgout("terminating: rpcbind is not running");
		exit(1);
	}

	if (_rpcpmstart) {
		/*
		 * We were invoked by ypbind with the request on stdin.
		 *
		 * XXX - This is not the normal way ypbind is used
		 * and has never been tested.
		 */
		char *netid;
		struct netconfig *nconf = NULL;
		SVCXPRT *transp;
		int pmclose;
		extern char *getenv();

		clear_bindings();
		if ((netid = getenv("NLSPROVIDER")) == NULL) {
#ifdef DEBUG
			msgout("cannot get transport name");
#endif
		} else if ((nconf = getnetconfigent(netid)) == NULL) {
#ifdef DEBUG
			msgout("cannot get transport info");
#endif
		}

		pmclose = (t_getstate(0) != T_DATAXFER);
		if ((transp = svc_tli_create(0, nconf, NULL, 0, 0)) == NULL) {
			msgout("cannot create server handle");
			exit(1);
		}

		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			if ((setok != YPSETNONE) &&
				__rpc_negotiate_uid(transp->xp_fd)) {
					syslog(LOG_ERR,
				"could not negotiate with loopback tranport %s",
				nconf->nc_netid);
			}
		}
		if (nconf)
			freenetconfigent(nconf);
		if (!svc_reg(transp, YPBINDPROG, YPBINDVERS, ypbindprog_3, 0)) {
			msgout("unable to register (YPBINDPROG, YPBINDVERS).");
			exit(1);
		}
		if (!svc_reg(transp, YPBINDPROG, YPBINDVERS_2,
		    ypbindprog_2, 0)) {
			msgout(
			    "unable to register (YPBINDPROG, YPBINDVERS_2).");
			exit(1);
		}
		/* version 2 and version 1 are the same as far as we care */
		if (!svc_reg(transp, YPBINDPROG, YPBINDVERS_1,
		    ypbindprog_2, 0)) {
			msgout(
			    "unable to register (YPBINDPROG, YPBINDVERS_1).");
			exit(1);
		}
		set_signal_handlers();
		if (pmclose) {
			(void) signal(SIGALRM, closedown);
			(void) alarm(_RPCSVC_CLOSEDOWN);
		}
#ifdef INIT_DEFAULT
	ypbind_init_default();
#endif
		svc_run();
		msgout("svc_run returned");
		exit(1);
		/* NOTREACHED */
	}
#ifndef RPC_SVC_FG
	/*
	 *  In normal operation, ypbind forks a child to do all the work
	 *  so that it can run in background.  But, if the parent exits
	 *  too soon during system startup, clients will start trying to
	 *  talk to the child ypbind before it is ready.  This can cause
	 *  spurious client errors.
	 *
	 *  To prevent these problems, the parent process creates a pipe,
	 *  which is inherited by the child, and waits for the child to
	 *  close its end.  This happens explicitly before the child goes
	 *  into svc_run(), or as a side-effect of exiting.
	 */
	if (pipe(pfd) == -1) {
		perror("pipe");
		exit(1);
	}
	pid = fork();
	if (pid < 0) {
		perror("cannot fork");
		exit(1);
	}
	if (pid) {
		/*
		 *  The parent waits for the child to close its end of
		 *  the pipe (to indicate that it is ready to process
		 *  requests).  The read blocks until the child does
		 *  a close (the "domain" array is just a handy buffer).
		 */
		close(pfd[1]);
		read(pfd[0], domain, sizeof (domain));
		exit(0);
	}
	getrlimit(RLIMIT_NOFILE, &rl);
	for (i = 0; i < rl.rlim_max; i++) {
		if (i != pfd[1])
			(void) close(i);
	}
	(void) open("/dev/null", O_RDONLY);
	(void) open("/dev/null", O_WRONLY);
	(void) dup(1);
	setsid();
#endif
	clean_cache();    /* make sure there are no left-over files */
	cache_okay = cache_check();
	cache_pid();

#ifdef INIT_DEFAULT
	ypbind_init_default();
#endif

	nc_handle = __rpc_setconf("netpath");	/* open netconfig file */
	if (nc_handle == NULL) {
		syslog(LOG_ERR, "could not read /etc/netconfig, exiting..");
		exit(1);
	}

	/*
	 *  The parent waits for the child to close its end of
	 *  the pipe (to indicate that it is ready to process
	 *  requests). Now the non-diskless client will wait because the
	 *  cache file is valid.
	 */
	if (cache_okay) {
		close(pfd[1]);
		pipe_closed = 1;
	}

	clear_bindings();

	while (nconf = __rpc_getconf(nc_handle)) {
		SVCXPRT *xprt;

		if (!__rpcbind_is_up()) {
			msgout("terminating: rpcbind is not running");
			exit(1);
		}
		if ((xprt = svc_tp_create(ypbindprog_3,
			YPBINDPROG, YPBINDVERS, nconf)) == NULL)
			continue;

		cache_transport(nconf, xprt, YPBINDVERS);

		/* support ypbind V2 and V1, but only on udp/tcp transports */
		if ((strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
			((nconf->nc_semantics == NC_TPI_CLTS) ||
				(nconf->nc_semantics == NC_TPI_COTS_ORD))) {
			(void) rpcb_unset(YPBINDPROG, YPBINDVERS_2, nconf);
			if (!svc_reg(xprt, YPBINDPROG, YPBINDVERS_2,
			    ypbindprog_2, nconf)) {
				syslog(LOG_ERR,
			    "unable to register (YPBINDPROG, YPBINDVERS_2).");
				exit(1);
			}

			cache_transport(nconf, xprt, YPBINDVERS_2);

			/* V2 and V1 are the same as far as we care */
			(void) rpcb_unset(YPBINDPROG, YPBINDVERS_1, nconf);
			if (!svc_reg(xprt, YPBINDPROG, YPBINDVERS_1,
			    ypbindprog_2, nconf)) {
				syslog(LOG_ERR,
			    "unable to register (YPBINDPROG, YPBINDVERS_1).");
				exit(1);
			}

			cache_transport(nconf, xprt, YPBINDVERS_1);
		}
		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			loopback_found++;
			if ((setok != YPSETNONE) &&
				__rpc_negotiate_uid(xprt->xp_fd)) {
					syslog(LOG_ERR,
				"could not negotiate with loopback tranport %s",
				nconf->nc_netid);
			}
			/*
			 *  On a diskless client:
			 *  The parent waits for the child to close its end of
			 *  the pipe (to indicate that it is ready to process
			 *  requests). Now the diskless client will wait
			 *  only if ypbind is registered on the loopback.
			 */
			if ((!pipe_closed) &&
				((nconf->nc_semantics == NC_TPI_COTS) ||
				(nconf->nc_semantics == NC_TPI_COTS_ORD))) {
				close(pfd[1]);
				pipe_closed = 1;
			}
		}
		if ((strcmp(nconf->nc_protofmly, NC_INET) == 0) &&
			(nconf->nc_semantics == NC_TPI_CLTS))
			udp_found++;
	}
	if (!pipe_closed) {
		close(pfd[1]);
		pipe_closed = 1;
	}
	__rpc_endconf(nc_handle);
	if (!loopback_found) {
		syslog(LOG_ERR,
			"could not find loopback transports, exiting..");
		exit(1);
	}
	if (!udp_found) {
		syslog(LOG_ERR,
			"could not find inet-clts (udp) transport, exiting..");
		exit(1);
	}
	set_signal_handlers();
	svc_run();
	syslog(LOG_ERR, "svc_run returned, exiting..");
	exit(1);
	/* NOTREACHED */
}

void
ypbindprog_3(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		ypbind_domain ypbindproc_domain_3_arg;
		ypbind_setdom ypbindproc_setdom_3_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	if (sigcld_event)
		broadcast_proc_exit();

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case YPBINDPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_null_3;
		break;

	case YPBINDPROC_DOMAIN:
		xdr_argument = xdr_ypbind_domain;
		xdr_result = xdr_ypbind_resp;
		local = (char *(*)()) ypbindproc_domain_3;
		break;

	case YPBINDPROC_SETDOM:
		xdr_argument = xdr_ypbind_setdom;
		xdr_result = xdr_void;
		local = (char *(*)()) ypbindproc_setdom_3;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		return;
	}
	if (rqstp->rq_proc == YPBINDPROC_SETDOM)
		result = (*local)(&argument, rqstp, transp);
	else
		result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;
}

void
ypbindprog_2(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		domainname_2 ypbindproc_domain_2_arg;
		ypbind_setdom_2 ypbindproc_setdom_2_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	if (sigcld_event)
		broadcast_proc_exit();

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case YPBINDPROC_NULL:
		xdr_argument = xdr_void;
		xdr_result = xdr_void;
		/* XXX - don't need two null procedures */
		local = (char *(*)()) ypbindproc_null_3;
		break;

	case YPBINDPROC_DOMAIN:
		xdr_argument = (bool_t (*)())xdr_ypdomain_wrap_string;
		xdr_result = xdr_ypbind_resp_2;
		local = (char *(*)()) ypbindproc_domain_2;
		break;

	case YPBINDPROC_SETDOM:	/* not supported, fall through to error */
	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, (xdrproc_t)xdr_argument, (char *)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;
}

/*
 *  We clear out any old bindings that might have been
 *  left behind.  If there is already a ypbind running,
 *  it will no longer get requests.  We are in control
 *  now.  We ignore the error from rpcb_unset() because
 *  this is just a "best effort".  If the rpcb_unset()
 *  does fail, we will get an error in svc_reg().  By
 *  using 0 for the last argument we are telling the
 *  portmapper to remove the bindings for all transports.
 */
static
void
clear_bindings()
{
	rpcb_unset(YPBINDPROG, YPBINDVERS, 0);
	rpcb_unset(YPBINDPROG, YPBINDVERS_2, 0);
	rpcb_unset(YPBINDPROG, YPBINDVERS_1, 0);
}

/*
 *  This routine is called when we are killed (by most signals).
 *  It first tries to unregister with the portmapper.  Then it
 *  resets the signal handler to the default so that if we get
 *  the same signal, we will just go away.  We clean up our
 *  children by doing a hold in SIGTERM and then killing the
 *  process group (-getpid()) with SIGTERM.  Finally, we redeliver
 *  the signal to ourselves (the handler was reset to the default)
 *  so that we will do the normal handling (e.g., coredump).
 *  If we can't kill ourselves, we get drastic and just exit
 *  after sleeping for a couple of seconds.
 *
 *  This code was taken from the SunOS version of ypbind.
 */
static
void
unregister(int code)
{
	clear_bindings();
	clean_cache();
	signal(code, SIG_DFL);    /* to prevent recursive calls to unregister */
	fprintf(stderr, "ypbind: goind down on signal %d\n", code);
	sighold(SIGCHLD);
	sighold(SIGTERM);
	kill(-getpid(), SIGTERM); /* kill process group (i.e., children) */
	sigrelse(SIGTERM);
	kill(getpid(), code);	  /* throw signal again */
	sleep(2);
	exit(-1);
}

static
void
set_signal_handlers()
{
	int i;

	for (i = 1; i <= SIGTERM; i++) {
		if (i == SIGCHLD)
			continue;
		else if (i == SIGHUP)
			signal(i, SIG_IGN);
		else
			signal(i, unregister);
	}
}

void
msgout(msg)
	char *msg;
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, msg);
	else
		(void) fprintf(stderr, "%s\n", msg);
#else
	syslog(LOG_ERR, msg);
#endif
}

void
closedown()
{
	if (_rpcsvcdirty == 0) {
		extern fd_set svc_fdset;
		static struct rlimit	rl;
		int i, openfd;
		struct t_info tinfo;

		if (t_getinfo(0, &tinfo) || (tinfo.servtype == T_CLTS))
			exit(0);
		if (rl.rlim_max == 0)
			getrlimit(RLIMIT_NOFILE, &rl);
		for (i = 0, openfd = 0; i < rl.rlim_max && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= 1)
			exit(0);
	}
	(void) alarm(_RPCSVC_CLOSEDOWN);
}
