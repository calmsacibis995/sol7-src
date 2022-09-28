/* Copyright 1991 NCR Corporation - Dayton, Ohio, USA */

/* LINTLIBRARY */
/* PROTOLIB1 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#pragma ident	"@(#)lockd.c	1.31	97/09/19 SMI"	/* SVr4.0 1.9	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1994,1996-1997 by Sun Microsystems, Inc.
 * 	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *		    All rights reserved.
 *
 */

/* NLM server */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <tiuser.h>
#include <rpc/rpc.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/file.h>
#include <nfs/nfs.h>
#include <nfs/lm.h>
#include <nfs/nfssys.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netconfig.h>
#include <netdir.h>
#include <string.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/tihdr.h>
#include <poll.h>
#include <sys/tiuser.h>
#include <netinet/tcp.h>
#include <libintl.h>
#include "nfs_tbind.h"

/*
 * Define the default maximum number of servers per transport.  This value
 * has not yet been tuned.  It needs to be large enough so that standard
 * tests using blocking locks don't fail.
 */
#define	NLM_SERVERS	20

#define	DEBUGVALUE	0
#define	TIMOUT		300	/* One-way RPC connections valid for 5 min. */
#define	RETX_TIMOUT	5	/* Retransmit RPC requests every 5 seconds. */
#define	GRACE		45	/* We have a 45-second grace period. */
#define	RET_OK		0	/* return code for no error */
#define	RET_ERR		33	/* return code for error(s) */

static	int	nlmsvc(int nservers, int fd, struct netbuf addrmask,
			struct netconfig *nconf);
static	int	convert_nconf_to_knconf(struct netconfig *,
			struct knetconfig *);
static	void	usage(void);

extern	int	_nfssys(int, void *);

static	char	*MyName;
static	struct lm_svc_args lsa;
static	NETSELDECL(defaultprotos)[] =	{ NC_UDP, NC_TCP, NULL };

/*
 * The following are all globals used by routines in nfs_tbind.c.
 */
size_t	end_listen_fds;		/* used by conn_close_oldest() */
size_t	num_fds = 0;		/* used by multiple routines */
int	listen_backlog = 32;	/* used by bind_to_{provider,proto}() */
int	num_servers;		/* used by cots_listen_event() */
int	(*Mysvc)(int nservers, int fd, struct netbuf,
		struct netconfig *) = nlmsvc;
				/* used by cots_listen_event() */
int	max_conns_allowed = -1; /* used by cots_listen_event() */

main(int ac, char **av)
{
	char *dir = "/";
	int nservers = NLM_SERVERS;
	int lognservers = 0;
	int pid;
	int i;
	int tmp;
	char *provider = (char *)NULL;
	struct protob *protobp;
	NETSELDECL(proto) = NULL;
	NETSELPDECL(protop);

	MyName = *av;

	if (geteuid() != 0) {
		(void) fprintf(stderr, "%s must be run as root\n", av[0]);
		exit(1);
	}

	lsa.version = LM_SVC_CUR_VERS;
	lsa.debug = DEBUGVALUE;
	lsa.timout = TIMOUT;
	lsa.grace = GRACE;
	lsa.retransmittimeout = RETX_TIMOUT;

	while ((i = getopt(ac, av, "d:g:T:t:l:")) != EOF)
		switch (i) {
		case 'd':
			(void) sscanf(optarg, "%d", &lsa.debug);
			break;
		case 'g':
			(void) sscanf(optarg, "%d", &lsa.grace);
			if (lsa.grace < 0) {
				fprintf(stderr, gettext(
				"Invalid value for -g, %d replaced with %d\n"),
					lsa.grace, GRACE);
				lsa.grace = GRACE;
			}
			break;
		case 'T':
			(void) sscanf(optarg, "%d", &lsa.timout);
			if (lsa.timout < 0) {
				fprintf(stderr, gettext(
				"Invalid value for -T, %d replaced with %d\n"),
					lsa.timout, TIMOUT);
				lsa.timout = TIMOUT;
			}
			break;
		case 't':
			/* set retransmissions timeout value */
			(void) sscanf(optarg, "%d", &lsa.retransmittimeout);
			if (lsa.retransmittimeout < 0) {
				fprintf(stderr, gettext(
				"Invalid value for -t, %d replaced with %d\n"),
					lsa.retransmittimeout, RETX_TIMOUT);
				lsa.retransmittimeout = RETX_TIMOUT;
			}
			break;
		case 'l':
			if (sscanf(optarg, "%d", &tmp)) {
				if (tmp > listen_backlog) {
					listen_backlog = tmp;
				}
			}
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	/*
	 * If there is exactly one more argument, it is the number of
	 * servers.
	 */
	if (optind == ac - 1) {
		nservers = atoi(av[optind]);
		if (nservers <= 0)
			usage();
	}
	/*
	 * If there are two or more arguments, then this is a usage error.
	 */
	else if (optind < ac - 1)
		usage();
	/*
	 * There are no additional arguments, we use a default number of
	 * servers.  We will log this.
	 */
	else
		lognservers = 1;

	if (lsa.debug >= 1) {
		printf(
	"%s: debug= %d, timout= %d, retrans= %d, grace= %d, nservers= %d\n\n",
		    MyName, lsa.debug, lsa.timout, lsa.retransmittimeout,
		    lsa.grace, nservers);
	}

	/*
	 * Set current and root dir to server root
	 */
	if (chroot(dir) < 0) {
		(void) fprintf(stderr, "%s:  ", MyName);
		perror(dir);
		exit(1);
	}
	if (chdir(dir) < 0) {
		(void) fprintf(stderr, "%s:  ", MyName);
		perror(dir);
		exit(1);
	}

#ifndef DEBUG
	/*
	 * Background
	 */
	if (lsa.debug == 0) {
		struct rlimit rl;

		pid = fork();
		if (pid < 0) {
			perror("lockd: fork");
			exit(1);
		}
		if (pid != 0)
			exit(0);

		/*
		 * Close existing file descriptors, open "/dev/null" as
		 * standard input, output, and error, and detach from
		 * controlling terminal.
		 */
		getrlimit(RLIMIT_NOFILE, &rl);
		for (i = 0; i < rl.rlim_max; i++)
			(void) close(i);
		(void) open("/dev/null", O_RDONLY);
		(void) open("/dev/null", O_WRONLY);
		(void) dup(1);
		(void) setsid();
	}
#endif

	openlog(MyName, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if (lognservers) {
		(void) syslog(LOG_INFO,
			"Number of servers not specified. Using default of %d.",
			nservers);
	}

	protobp = (struct protob *) malloc(sizeof (struct protob));
	protobp->serv = "NLM";
	protobp->versmin = NLM_VERS;
	protobp->versmax = NLM4_VERS;
	protobp->program = NLM_PROG;
	protobp->next = (struct protob *)NULL;

	if (do_all(nservers, protobp, nlmsvc) == -1) {
		for (protop = defaultprotos; *protop != NULL; protop++) {
			proto = *protop;
			do_one(nservers, provider, proto, protobp, nlmsvc);
		}
	}

	free(protobp);

	if (num_fds == 0) {
		(void) syslog(LOG_ERR,
		"Could not start NLM service for any protocol. Exiting.");
		exit(1);
	}

	num_servers = nservers;
	end_listen_fds = num_fds;

	/*
	 * Poll for non-data control events on the transport descriptors.
	 */
	poll_for_action();

	/*
	 * If we get here, something failed in poll_for_action().
	 */
	return (1);
}

/*
 * Establish NLM service thread.
 */
static int
nlmsvc(nservers, fd, addrmask, nconf)
	int	nservers;
	int	fd;
	struct netbuf addrmask;
	struct netconfig *nconf;
{
	struct knetconfig knconf;

	lsa.fd = fd;
	lsa.n_fmly = strcmp(nconf->nc_protofmly, NC_INET) == 0 ?
			LM_INET : LM_LOOPBACK;
#ifdef LOOPBACK_LOCKING
	if (lsa.n_fmly == LM_LOOPBACK) {
		lsa.n_proto = LM_NOPROTO;	/* need to add this */
	} else
#endif
	lsa.n_proto = strcmp(nconf->nc_proto, NC_TCP) == 0 ?
			LM_TCP : LM_UDP;
	(void) convert_nconf_to_knconf(nconf, &knconf);
	lsa.n_rdev = knconf.knc_rdev;
	lsa.max_threads = nservers;

	return (_nfssys(LM_SVC, &lsa));
}

static int
convert_nconf_to_knconf(struct netconfig *nconf, struct knetconfig *knconf)
{
	struct stat sb;

	if (stat(nconf->nc_device, &sb) < 0) {
		(void) syslog(LOG_ERR, "can't find device for transport %s\n",
				nconf->nc_device);
		return (RET_ERR);
	}

	knconf->knc_semantics = nconf->nc_semantics;
	knconf->knc_protofmly = nconf->nc_protofmly;
	knconf->knc_proto = nconf->nc_proto;
	knconf->knc_rdev = sb.st_rdev;

	return (RET_OK);
}

static void
usage(void)
{
	(void) fprintf(stderr,
gettext("usage: %s [-t timeout] [-g graceperiod] [-l listen_backlog]\n"),
		MyName);
	exit(1);
}
