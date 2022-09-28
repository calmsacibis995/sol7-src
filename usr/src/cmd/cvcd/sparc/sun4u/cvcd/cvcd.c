/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 * This code implements the Starfire Virtual Console host daemon
 * (see cvcd(1M)).  It accepts a connection from netcon_server
 * and transfers console I/O to/from the SSP across the
 * network via TLI.  The I/O is sent to the cvcredir device
 * on the host (see cvc(7) and cvcredir(7)).  It also sends
 * disconnect and break ioctl's to the kernel CVC drivers.
 */

#pragma	ident	"@(#)cvcd.c	1.21	98/01/14 SMI"

#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdlib.h>
#include <tiuser.h>
#include <sys/timod.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stropts.h>
#include <sys/conf.h>
#include <pwd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <locale.h>
#include <termio.h>
#include <signal.h>
#include <sys/cvc.h>

#include <string.h>

#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/sockio.h>

#include <sys/tihdr.h>

#include <netdb.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#include <inet/common.h>
#include <inet/mib2.h>
#include <sys/systeminfo.h>

/* Process priority control */
#include <sys/priocntl.h>
#include <sys/tspriocntl.h>
#include <sys/rtpriocntl.h>

/*
 *  Misc. defines.
 */
#define	CONREF "connection request from illegal host"
#define	SSPHOSTNAMEFILE	"/etc/ssphostname"
#define	NODENAME	"/etc/nodename"
#define	MAXIFS		256

/*
 * Function prototypes
 */
static void cvcd_connect(int fd, struct pollfd *);
static void cvcd_reject(int fd);
static void cvcd_read(struct pollfd *);
static void cvcd_write(char *data, int size);
static void cvcd_status(int fd);
static void cvcd_winch(int, char *, int);
static void cvcd_ioctl(int fd, int cmd);
static void cvcd_err(int code, char *format, ...);
static void usage(void);
static id_t schedinfo(char *name, short *maxpri);

typedef struct mib_item_s {
	struct mib_item_s	*next_item;
	long			group;
	long			mib_id;
	long			length;
	char			*valp;
} mib_item_t;

static int	cvcd_find_ssp_nic(ulong ssp_addr);
static void	cvcd_set_ssp_nic(char *);
static int	cvcd_mibopen(void);
static int	cvcd_mibclose(int);
static mib_item_t	*cvcd_mibget(int);
static void	cvcd_mibfree(mib_item_t *);
static char	*cvcd_octetstr(char *, Octet_t *);
static int	cvcd_nic_check(mib_item_t *, char *, char *);
static char 	*cvcd_rtname(ulong, char *);
static int	cvcd_verify_ip(char *, char *);

/*
 *  Globals
 */
static int		rconsfd;	/* Console redirection driver */
static char		progname[MAXPATHLEN];
static char		ssphostname[MAXPATHLEN];
static int		debug = 0;
static int		connected = 0;
static int		peercheck = 1;
static char		nic_name[32];

main(int argc, char **argv)
{
	int			opt;
	int			tport = 0;
	char			*hostname;
	struct utsname		utsname;
	struct t_info		tinfo;
	int			cvcd_ssp;
	int			nfd;
	struct pollfd		*cvcd_pfd;
	int			i;
	struct servent		*se;
	struct sockaddr_in	*sin;
	struct t_bind		*reqb;
	struct t_bind		*retb;
	struct t_optmgmt	*topt, *tropt;
	struct opthdr		*sockopt;
	int			on = 1;
	int			tmperr = 0;
	int			event;
	char 			prefix[256];
	pcparms_t	pcparms;
	tsparms_t	*tsparmsp;
	id_t	pid, tsID;
	short	tsmaxpri;

	(void) setlocale(LC_ALL, "");
	(void) strcpy(progname, argv[0]);
	(void) memset(ssphostname, 0, MAXPATHLEN);

	if ((cvcd_ssp = open(SSPHOSTNAMEFILE, O_RDONLY)) < 0) {
		/*
		 * If there is no /etc/ssphostname disable peer check after
		 * issuing warning.
		 */
		tmperr = errno;
		peercheck = 0;
	} else {
		if ((i = read(cvcd_ssp, ssphostname, MAXPATHLEN)) < 0) {
			cvcd_err(LOG_ERR, "failed to read ssphostname");
		}
		/*
		 * The ssp-config(1M) command newline terminates the
		 * ssphostname in the /etc/ssphostname file
		 */
		ssphostname[i-1] = '\0';
		(void) close(cvcd_ssp);

		(void) memset(nic_name, 0, sizeof (nic_name));
	}

#if defined(DEBUG)
	while ((opt = getopt(argc, argv, "dp:r:")) != EOF) {
#else
	while ((opt = getopt(argc, argv, "r:")) != EOF) {
#endif  /* DEBUG */
		switch (opt) {

#if defined(DEBUG)
			case 'd' :	debug = 1;
					break;

			case 'p' :	tport = atoi(optarg);
					break;
#endif  /* DEBUG */

			case 'r' :	(void) strcpy(ssphostname, optarg);
					break;

			default  :	usage();
					exit(1);
		}
	}

	if (uname(&utsname) == -1) {
		perror("HOSTNAME not defined");
		exit(1);
	}
	hostname = utsname.nodename;

	/*
	 * hostname may still be NULL, depends on when cvcd was started
	 * in the boot sequence.  If it is NULL, try one more time
	 * to get a hostname -> look in the /etc/nodename file.
	 */
	if (!strlen(hostname)) {
		/*
		 * try to get the hostname from the /etc/nodename file
		 * we reuse the utsname.nodename buffer here!  hostname
		 * already points to it.
		 */
		if ((nfd = open(NODENAME, O_RDONLY)) > 0) {
			if ((i = read(nfd, utsname.nodename, SYS_NMLN)) <= 0) {
				cvcd_err(LOG_WARNING,
				"failed to acquire hostname");
			}
			utsname.nodename[i-1] = '\0';
			(void) close(nfd);
		}
	}

	/*
	 * Daemonize...
	 */
	if (debug == 0) {
		for (i = 0; i < NOFILE; i++) {
			(void) close(i);
		}
		(void) chdir("/");
		(void) umask(0);
		if (fork() != 0) {
			exit(0);
		}
		(void) setpgrp();
		(void) sprintf(prefix, "%s-(HOSTNAME:%s)", progname, hostname);
		openlog(prefix, LOG_CONS | LOG_NDELAY, LOG_LOCAL0);
	}
	if (peercheck == 0) {
		cvcd_err(LOG_ERR, "open(SSPHOSTNAMEFILE):%s",
		    strerror(tmperr));
	}

	cvcd_pfd = (struct pollfd *)malloc(3*sizeof (struct pollfd));
	if (cvcd_pfd == (struct pollfd *)NULL) {
		cvcd_err(LOG_ERR, "malloc:", strerror(errno));
		exit(1);
	}
	(void) memset((void *)cvcd_pfd, 0, 3*sizeof (struct pollfd));
	cvcd_pfd[0].fd = -1;
	cvcd_pfd[1].fd = -1;
	cvcd_pfd[2].fd = -1;

	/*
	 * Must be root.
	 */
	if (debug == 0 && geteuid() != 0) {
		cvcd_err(LOG_ERR, "cvcd: Must be root");
		exit(1);
	}

	/* SPR 94004 */
	(void) sigignore(SIGTERM);

	/*
	 * SPR 83644: cvc and kadb are not compatible under heavy loads.
	 *	Fix: will give cvcd highest TS priority at execution time.
	 */
	pid = getpid();
	pcparms.pc_cid = PC_CLNULL;
	tsparmsp = (tsparms_t *)pcparms.pc_clparms;

	/* Get scheduler properties for this PID */
	if (priocntl(P_PID, pid, PC_GETPARMS, (caddr_t)&pcparms) == -1L) {
		cvcd_err(LOG_ERR,
			"cvcd: GETPARMS failed. Warning: can't get ",
			"TS priorities.");
	} else {
		/* Get class IDs and maximum priorities for a TS process */
		if ((tsID = schedinfo("TS", &tsmaxpri)) == -1) {
			cvcd_err(LOG_ERR, "cvcd: Warning, can't get ",
				"TS scheduler info.");
		} else {
			if (debug) {	/* Print priority info */
				if (pcparms.pc_cid == tsID) {
					cvcd_err(LOG_DEBUG, "%s%d%s%d%s%d\n",
						"cvcd:: PID:", pid,
						", TS priority:",
						tsparmsp->ts_upri,
						", TS max_pri:", tsmaxpri);
				}
			}
			/* Change proc's priority to maxtspri */
			pcparms.pc_cid = tsID;
			tsparmsp = (struct tsparms *)pcparms.pc_clparms;
			tsparmsp->ts_upri = tsmaxpri;
			tsparmsp->ts_uprilim = tsmaxpri;

			if (priocntl(P_PID, pid, PC_SETPARMS,
				(caddr_t)&pcparms) == -1L) {
				cvcd_err(LOG_ERR, "cvcd: Warning, ",
					"can't set TS maximum priority.");
			}
			/* Done */
			if (debug) { /* Get new scheduler properties for PID */
				if (priocntl(P_PID, pid, PC_GETPARMS,
					(caddr_t)&pcparms) == -1L) {
					cvcd_err(LOG_DEBUG, "GETPARMS failed");
					exit(1);
				} else {
					cvcd_err(LOG_DEBUG, "%s%d%s%d%s%d\n",
						"cvcd:: PID:", pid,
						", New TS priority:",
						tsparmsp->ts_upri,
						", TS max_pri:", tsmaxpri);
				}
			}
		}
	}

	if (debug == 1) {
		cvcd_err(LOG_DEBUG, "tport = %d, debug = %d", tport, debug);
	}

	if (tport == 0) {
		if ((se = getservbyname(CVCD_SERVICE, "tcp")) == NULL) {
			cvcd_err(LOG_ERR, "getservbyname(%s) not found",
				CVCD_SERVICE);
			exit(1);
		}
		tport = se->s_port;
	}

	cvcd_ssp = t_open(TCP_DEV, O_RDWR, &tinfo);
	if (cvcd_ssp == -1) {
		cvcd_err(LOG_ERR, "t_open: %s", t_errlist[t_errno]);
		exit(1);
	}

	/*
	 * Set the SO_REUSEADDR option for this TLI endpoint.
	 */
	topt = (struct t_optmgmt *)t_alloc(cvcd_ssp, T_OPTMGMT, 0);
	if (topt == NULL) {
		cvcd_err(LOG_ERR, "t_alloc: %s", t_errlist[t_errno]);
		exit(1);
	}
	tropt = (struct t_optmgmt *)t_alloc(cvcd_ssp, T_OPTMGMT, T_OPT);
	if (tropt == NULL) {
		cvcd_err(LOG_ERR, "t_alloc: %s", t_errlist[t_errno]);
		exit(1);
	}
	topt->opt.buf = (char *)malloc(sizeof (struct opthdr) + sizeof (int));
	topt->opt.maxlen = 0;
	topt->opt.len = sizeof (struct opthdr) + sizeof (int);
	topt->flags = T_NEGOTIATE;
	sockopt = (struct opthdr *)topt->opt.buf;
	sockopt->level = SOL_SOCKET;
	sockopt->name = SO_REUSEADDR;
	sockopt->len = sizeof (int);
	(void) memcpy((char *)(topt->opt.buf + sizeof (struct opthdr)),
		(char *)&on, sizeof (on));
	tropt->opt.buf = (char *)malloc(sizeof (struct opthdr) + sizeof (int));
	tropt->opt.maxlen = sizeof (struct opthdr) + sizeof (int);

	if (t_optmgmt(cvcd_ssp, topt, tropt) == -1) {
		t_error("t_optmgmt");
		exit(1);
	}

	/*
	 * Bind it.
	 */
	if (((reqb = (struct t_bind *)t_alloc(cvcd_ssp, T_BIND, T_ALL))
		== (struct t_bind *)NULL)) {
			cvcd_err(LOG_ERR, "%s", t_errlist[t_errno]);
			exit(1);
	}
	if (((retb = (struct t_bind *)t_alloc(cvcd_ssp, T_BIND, T_ALL))
		== (struct t_bind *)NULL)) {
			cvcd_err(LOG_ERR, "%s", t_errlist[t_errno]);
			exit(1);
	}
	reqb->qlen = 1;
	reqb->addr.len = sizeof (struct sockaddr_in);
	sin = (struct sockaddr_in *)reqb->addr.buf;
	(void) memset((void *)sin, 0, sizeof (struct sockaddr_in));
	sin->sin_family = AF_INET;
	sin->sin_addr.s_addr = htonl(INADDR_ANY);
	sin->sin_port = htons(tport);
	if (t_bind(cvcd_ssp, reqb, retb) == -1) {
		cvcd_err(LOG_ERR, "t_bind: %s", t_errlist[t_errno]);
		exit(1);
	}
	sin = (struct sockaddr_in *)retb->addr.buf;
	if (sin->sin_port != htons(tport)) {
		cvcd_err(LOG_ERR, "t_bind: bound to wrong port");
		cvcd_err(LOG_ERR, "Wanted %d, got %d", tport,
			ntohs(sin->sin_port));
		exit(1);
	}
	t_free((char *)reqb, T_BIND);
	t_free((char *)retb, T_BIND);


	/*
	 *  Wait for connect from OBP.
	 */
	cvcd_pfd[2].fd = cvcd_ssp;
	cvcd_pfd[2].events = POLLIN|POLLPRI;
	if ((event = poll(&cvcd_pfd[2], 1, -1)) == -1) {
			cvcd_err(LOG_ERR, "poll: %s", strerror(errno));
			exit(1);
	}
	/*
	 * cvcd_connect sets global
	 * connected = 1 if successful.
	 */
	cvcd_connect(cvcd_ssp, cvcd_pfd);

	/*
	 * Now set up the Network Console redirection driver.
	 */
	rconsfd = open(CVCREDIR_DEV, O_RDWR|O_NDELAY);
	if (rconsfd < 0) {
		cvcd_err(LOG_ERR, "open: %s", strerror(errno));
		exit(1);
	}

	/*
	 * cvcd_pfd holds three file descriptors we need to poll from:
	 * 0 will be connected to in_cvcd;  1 is the CVC Redirection driver;
	 * and 2 is the listen endpoint for new connections.
	 */
	cvcd_pfd[1].fd = rconsfd;
	cvcd_pfd[1].events = POLLIN;
	/*
	 *  Loop through main service routine.  We check for inbound in.cvcd
	 * connection and data xfer between host and in.cvcd.
	 */
	for (;;) {

		char	buf[MAXPKTSZ];

		/*
		 * Check for in_cvcd connect requests.
		 */
		switch ((event = t_look(cvcd_ssp))) {
			case -1 :
				cvcd_err(LOG_ERR, "%s", t_errlist[t_errno]);
				exit(1);
				/* NOTREACHED */
				break;
			case 0  : /* Nothing to do */
				break;
			case T_LISTEN :
				if (connected == 1) {
					/*
					 * Someone already connected.
					 */
					cvcd_reject(cvcd_ssp);
				} else {
					/*
					 * cvcd_connect sets global
					 * connected = 1 if successful.
					 */
					cvcd_connect(cvcd_ssp, cvcd_pfd);
				}
				break;
			default :
				cvcd_err(LOG_ERR,
					"Illegal event %d for cvcd_ssp", event);
				exit(1);
		}
		/*
		 * Take a look for console I/O or connect request.
		 */
		if ((event = poll(cvcd_pfd, 3, -1)) == -1) {
			cvcd_err(LOG_ERR, "poll: %s", strerror(errno));
			exit(1);
		} else if (event != 0) {
			if (cvcd_pfd[0].revents == POLLIN) {
				/*
				 * Process cvcd_ssp data and commands.
				 */
				cvcd_read(cvcd_pfd);
			}
			if (cvcd_pfd[1].revents == POLLIN) {
				int s;

				if ((s = read(rconsfd, buf, MAXPKTSZ)) == -1) {
					cvcd_err(LOG_ERR, "read: %s",
						strerror(errno));
					exit(1);
				}
				if ((s > 0) && (connected == 1)) {
					if (write(cvcd_pfd[0].fd, buf, s) !=
					    s) {
						cvcd_err(LOG_ERR,
							"lost data output");
					}
				}
			}
		}
	} /* End forever loop */

#ifdef lint
	/* NOTREACHED */
	return (1);
#endif lint
}

static void
cvcd_reject(int fd)
{
	struct t_call		*tcall;

	tcall = (struct t_call *)t_alloc(fd, T_CALL, T_ALL);
	if (tcall == (struct t_call *)NULL) {
		cvcd_err(LOG_ERR, "cvcd_reject: t_alloc: %s",
			t_errlist[t_errno]);
		return;
	}
	if (t_listen(fd, tcall) == -1) {
		if (t_errno == TNODATA) {
			cvcd_err(LOG_ERR, "cvcd_reject: No client data!");
		}
		cvcd_err(LOG_ERR, "cvcd_reject: t_listen: %s",
			t_errlist[t_errno]);
		t_free((char *)tcall, T_CALL);
		return;
	}
	if (t_snddis(fd, tcall) < 0) {
		cvcd_err(LOG_ERR, "cvcd_reject: t_snddis: %s",
			t_errlist[t_errno]);
	}
}

static void
cvcd_connect(int fd, struct pollfd *pfd)
{
	struct t_call		*tcall;
	int			newfd;
	struct sockaddr_in	*peer;
	int			badpeer = 0;
	struct hostent		*he;
	struct netbuf		netbuf;
	char			addr[100];
	ulong 			tmpaddr;	/* network byte order */

	tcall = (struct t_call *)t_alloc(fd, T_CALL, T_ALL);
	if (tcall == (struct t_call *)NULL) {
		cvcd_err(LOG_ERR, "cvcd_connect: t_alloc: %s",
			t_errlist[t_errno]);
		return;
	}
	if (t_listen(fd, tcall) == -1) {
		if (t_errno == TNODATA) {
			cvcd_err(LOG_ERR, "cvcd_connect: No client data!");
		}
		cvcd_err(LOG_ERR, "cnctip_connect: t_listen: %s",
			t_errlist[t_errno]);
		t_free((char *)tcall, T_CALL);
		return;
	}
	if (pfd[0].fd != -1) {
		cvcd_err(LOG_ERR, "cvcd_connect: no free file descriptors!");
		t_free((char *)tcall, T_CALL);
		return;
	}
	newfd = t_open(TCP_DEV, O_RDWR|O_NDELAY, NULL);
	if (newfd == -1) {
		cvcd_err(LOG_ERR, "cvcd_connect: t_open: %s",
			t_errlist[t_errno]);
		t_free((char *)tcall, T_CALL);
		return;
	}
	if (t_accept(fd, newfd, tcall) < 0) {
		cvcd_err(LOG_ERR, "cvcd_connect: t_accept: %s",
			t_errlist[t_errno]);
		t_close(newfd);
		t_free((char *)tcall, T_CALL);
		return;
	}
	t_free((char *)tcall, T_CALL);

	/*
	 *  If /etc/ssphostname doesnt exists, dont bother verifying
	 * peer since we cant do gethostbyname.
	 */
	if (peercheck == 1) {
		he = gethostbyname(ssphostname);
		if (he == NULL) {
			cvcd_err(LOG_ERR, "gethostbyname: %s",
			    strerror(h_errno));
			cvcd_err(LOG_ERR, "unable to get SSP name %s!",
			    ssphostname);
			exit(1);
		}
		/*
		 *  Verify peer is from specified host by comparing the
		 *  address (in network byte order) of the TLI endpoint
		 *  and the address (in network byte order) of the ssp
		 *  (using the hostname found in /etc/ssphostname).
		 */
		(void) memset(addr, 0, 100);
		netbuf.buf = addr;
		netbuf.len = 0;
		netbuf.maxlen = 100;
		if (ioctl(newfd, TI_GETPEERNAME, &netbuf) < 0) {
			cvcd_err(LOG_ERR, "ioctl(TI_GETPEERNAME): %s",
			    strerror(errno));
			badpeer = 1;
		} else  {
			peer = (struct sockaddr_in *)addr;
			tmpaddr = htonl(*(ulong *)he->h_addr);
			if (memcmp(&peer->sin_addr.s_addr, &tmpaddr,
			    he->h_length) != 0) {
				cvcd_err(LOG_ERR, CONREF);
				cvcd_err(LOG_ERR, "remote host = %s.",
					inet_ntoa(peer->sin_addr));
				badpeer = 1;
			}
		}
		/*
		 * if not bad already, check that hostname is on nic
		 * this should catch IP spoofing
		 */
		if (badpeer == 0) {
			cvcd_set_ssp_nic(ssphostname);
			badpeer = cvcd_verify_ip(ssphostname, nic_name);
		}
	}
	if (badpeer == 1) {
		t_close(newfd);
	} else {
		pfd[0].fd = newfd;
		pfd[0].events = POLLIN;
		connected = 1;
	}
}

/*
 *  Read in data from client.
 */
static void
cvcd_read(struct pollfd *pd)
{
	register char *data;
	register int fd = pd[0].fd;
	char	buf[MAXPKTSZ];
	int	flags = 0;

	data = buf;

	if (pd[0].revents & POLLIN) {
		int	n;

		if ((n = t_rcv(fd, data, MAXPKTSZ, &flags)) == -1) {
			cvcd_err(LOG_ERR, "cvcd_read: t_rcv: %s",
				t_errlist[t_errno]);
			(void) close(pd[0].fd);
			pd[0].fd = -1;
			connected = 0;
			return;
		}
		if (flags & T_EXPEDITED) {
			if (n != 1) {
				cvcd_err(LOG_ERR,
					"cvcd_read: %d bytes EXD!!",
					n);
			}
			/*
			 * Deal with cvcd_ssp_commands.
			 */
			switch (data[n-1]) {
				case CVC_CONN_BREAK :
					cvcd_ioctl(rconsfd, CVC_BREAK);
					break;

				case CVC_CONN_DIS :
					(void) close(pd[0].fd);
					pd[0].fd = -1;
					cvcd_ioctl(rconsfd, CVC_DISCONNECT);
					connected = 0;
					break;

				case CVC_CONN_STAT :
					cvcd_status(fd);
					break;

				default :
					cvcd_err(LOG_ERR,
						"Illegal cmd 0x%x", buf[n-1]);
					break;
			}
		} else {
			if (((data[0] & 0377) == 0377) &&
			    ((data[1] & 0377) == 0377)) {
				/*
				 * Pass on window size changes (TIOCSWINSZ).
				 */
				cvcd_winch(rconsfd, data, n);
				(void) memset(data, 0, n);
			} else {
				cvcd_write(buf, n);
			}
		}
	}

}

static void
cvcd_ioctl(int fd, int flags)
{
	struct strioctl cmd;

	cmd.ic_cmd = flags;
	cmd.ic_timout = 0;
	cmd.ic_len = 0;
	cmd.ic_dp = NULL;

	if (ioctl(fd, I_STR, &cmd) == -1) {
		cvcd_err(LOG_ERR, "cvcd_ioctl: %s", strerror(errno));
		exit(1);
	}
}


/* ARGSUSED */
static void
cvcd_status(int fd)
{
}


/*
 * Write input to console - called from cvcd_read.
 */
static void
cvcd_write(char *data, int size)
{
	int n;

	if ((n = write(rconsfd, data, size)) == -1) {
		cvcd_err(LOG_ERR, "cvcd_write: write: %s", strerror(errno));
		exit(1);
	}
	if (n != size) {
		cvcd_err(LOG_ERR, "cvcd_write: wrote %d of %d bytes", n, size);
	}
}

static void
usage()
{
#if defined(DEBUG)
	(void) printf("%s [-d] [-p port]\n", progname);
#else
	(void) printf("%s -r [ssp host]\n", progname);
#endif  /* DEBUG */
}

/*
 * cvcd_err ()
 *
 * Description:
 * Log messages via syslog daemon.
 *
 * Input:
 * code - logging code
 * format - messages to log
 *
 * Output:
 * void
 *
 */
static void
cvcd_err(int code, char *format, ...)
{
	va_list	varg_ptr;
	char	buf[MAXPKTSZ];

	va_start(varg_ptr, format);
	(void) vsprintf(buf, format, varg_ptr);
	va_end(varg_ptr);

	if (debug == 0)
		syslog(code, buf);
	else
		(void) fprintf(stderr, "%s: %s\n", progname, buf);
}

/*
 * Handle a "control" request (signaled by magic being present)
 * in the data stream.  For now, we are only willing to handle
 * window size changes.
 */
void
cvcd_winch(int pty, char *cp, int n)
{
	struct	winsize	w;

	if (n < 4+sizeof (w) || cp[2] != 's' || cp[3] != 's')
		return;
	(void) memcpy(&w, cp + 4, sizeof (w));
	w.ws_row = ntohs(w.ws_row);
	w.ws_col = ntohs(w.ws_col);
	w.ws_xpixel = ntohs(w.ws_xpixel);
	w.ws_ypixel = ntohs(w.ws_ypixel);
	(void) ioctl(pty, TIOCSWINSZ, &w);
}


/*
 * Return class ID and maximum priority of it.
 * Input:
 *	name: is class name (either TS or RT).
 *	maxpri: maximum priority for the class, returned in *maxpri.
 * Output:
 *	pc_cid: class ID
 */
static id_t
schedinfo(char *name, short *maxpri)
{
	pcinfo_t info;
	tsinfo_t *tsinfop;
	rtinfo_t *rtinfop;

	(void) strcpy(info.pc_clname, name);
	if (priocntl(0L, 0L, PC_GETCID, (caddr_t)&info) == -1L) {
		return (-1);
	}
	if (strcmp(name, "TS") == 0) {	/* Time Shared */
		tsinfop = (struct tsinfo *)info.pc_clinfo;
		*maxpri = tsinfop->ts_maxupri;
	} else if (strcmp(name, "RT") == 0) {	/* Real Time */
		rtinfop = (struct rtinfo *)info.pc_clinfo;
		*maxpri = rtinfop->rt_maxpri;
	} else {
		return (-1);
	}
	return (info.pc_cid);
}


/*
 * Find the nic interface which should support ssp_addr
 */
static int
cvcd_find_ssp_nic(ulong ssp_addr)
{
	char		*buf;
	int		savedflags;
	int		n;
	int		s;
	struct ifconf	ifc;
	struct ifreq	*ifrp;
	struct ifreq	ifr;
	int		numifs;
	unsigned	bufsize;
	struct sockaddr_in	*sin;
	struct sockaddr_in	netmask = { AF_INET };

	/*
	 * Determine the number of interfaces to check
	 */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		cvcd_err(LOG_ERR, "cvcd_find_ssp_nic: socket: %s",
		    strerror(errno));
		return (-1);
	}

	if (ioctl(s, SIOCGIFNUM, (char *)&numifs) < 0) {
		numifs = MAXIFS;
	}

	bufsize = numifs * sizeof (struct ifreq);
	if ((buf = malloc(bufsize)) == NULL) {
		cvcd_err(LOG_ERR, "out of memory\n");
		(void) close(s);
		return (-1);
	}

	ifc.ifc_len = bufsize;
	ifc.ifc_buf = buf;
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
		perror("ifconfig: SIOCGIFCONF");
		goto error;
	}

	ifrp = ifc.ifc_req;

	if (debug) {
		cvcd_err(LOG_DEBUG,
		    "cvcd_find_ssp_nic: checking %d interfaces\n",
		    ifc.ifc_len / sizeof (struct ifreq));
	}
	for (n = ifc.ifc_len / sizeof (struct ifreq); n > 0; n--, ifrp++) {
		long	nic_addr;
		long	nic_mask;
		/*
		 *	We must close and recreate the socket each time
		 *	since we don't know what type of socket it is now
		 *	(each status function may change it).
		 */
		(void) close(s);

		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
			cvcd_err(LOG_ERR, "cvcd_find_ssp_nic: socket2: %s",
			    strerror(errno));
			goto error;
		}

		(void) strncpy(nic_name, ifrp->ifr_name, sizeof (ifr.ifr_name));
		/*
		 * Only check interfaces that are up
		 */
		(void) memset((char *)&ifr, 0, sizeof (ifr));
		(void) strncpy(ifr.ifr_name, ifrp->ifr_name,
		    sizeof (ifr.ifr_name));

		if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
			cvcd_err(LOG_ERR,
			    "cvcd_find_ssp_nic: SIOCGIFFLAGS: %s",
			    strerror(errno));
			goto error;
		}

		/* if interface not up, skip it */
		if ((ifr.ifr_flags & IFF_UP) != IFF_UP)
			continue;

		savedflags = ifr.ifr_flags;

		/*
		 * Get the NetworkInterfaceCard IP address
		 */
		if (ioctl(s, SIOCGIFADDR, (caddr_t)&ifr) < 0) {
			if (errno == EADDRNOTAVAIL || errno == EAFNOSUPPORT) {
				continue;
			} else {
				cvcd_err(LOG_ERR,
				    "cvcd_find_ssp_nic: SIOCGIFADDR: %s",
				    strerror(errno));
				goto error;
			}
		}
		sin = (struct sockaddr_in *)&ifr.ifr_addr;

		if (debug) {
			cvcd_err(LOG_DEBUG, "%s:\tinet %s ",
			    nic_name, inet_ntoa(sin->sin_addr));
		}
		nic_addr = (long)sin->sin_addr.s_addr;

		if (ioctl(s, SIOCGIFNETMASK, (caddr_t)&ifr) < 0) {
			if (errno != EADDRNOTAVAIL) {
				cvcd_err(LOG_ERR,
				    "cvcd_find_ssp_nic: SIOCGIFNETMASK: %s",
				    strerror(errno));
				goto error;
			}
			(void) memset((char *)&ifr.ifr_addr,
			    0, sizeof (ifr.ifr_addr));
		} else
			netmask.sin_addr =
			    ((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr;
		if (savedflags & IFF_POINTOPOINT) {
			if (ioctl(s, SIOCGIFDSTADDR, (caddr_t)&ifr) < 0) {
				if (errno == EADDRNOTAVAIL) {
				    (void) memset((char *)&ifr.ifr_addr, 0,
					sizeof (ifr.ifr_addr));
				} else {
					cvcd_err(LOG_ERR,
					    "cvcd_find_ssp_nic: "
					    "SIOCGIFDSTADDR: %s",
					    strerror(errno));
				    goto error;
				}
			}
			sin = (struct sockaddr_in *)&ifr.ifr_dstaddr;
			if (debug) {
				cvcd_err(LOG_DEBUG,
					"--> %s ", inet_ntoa(sin->sin_addr));
			}
		}
		nic_mask = ntohl(netmask.sin_addr.s_addr);

		if (debug) {
			cvcd_err(LOG_DEBUG, "netmask %x \n", nic_mask);
			cvcd_err(LOG_DEBUG, "ssp_addr %x nic_mask %x a&m %x\n",
				ssp_addr, nic_mask, ssp_addr & nic_mask);
			cvcd_err(LOG_DEBUG, "nic_addr %x nic_mask %x a&m %x\n",
				nic_addr, nic_mask, nic_addr & nic_mask);
		}

		/* if this interface works, we are done */
		if ((nic_addr & nic_mask) == (ssp_addr & nic_mask)) {
			(void) close(s);
			free(buf);
			return (0);
		}
	}

error:
	(void) close(s);
	free(buf);
	return (-1);
}


/*
 * set up the global nic_name for later peer checking
 */
static void
cvcd_set_ssp_nic(char *hn)
{
	struct hostent	*he;
	ulong		ha;	/* network byte order */

	he = gethostbyname(hn);
	if (he == NULL) {
		cvcd_err(LOG_ERR, "gethostbyname: %s",
		    strerror(h_errno));
		cvcd_err(LOG_ERR, "unable to get SSP name %s!",
		    ssphostname);
		exit(1);
	}

	ha = htonl(*(ulong *)he->h_addr);
	if (cvcd_find_ssp_nic(ha) < 0) {
		cvcd_err(LOG_ERR, "cvcd_set_ssp_nic: unable to get SSP nic!");
		exit(1);
	}
}


/*
 * return 0 if ssp_name is associated with nic
 * return 1 otherwise
 */
static int
cvcd_verify_ip(char *ssp_name, char *nic_name)
{
	mib_item_t	*item;
	int		sd;
	int		rc;

	if ((sd = cvcd_mibopen()) == -1) {
		cvcd_err(LOG_ERR, "cvcd_verify_ip: can't open mib stream");
		return (-1);
	}
	if ((item = cvcd_mibget(sd)) == (mib_item_t *)0) {
		cvcd_err(LOG_ERR, "cvcd_verify_ip: cvcd_mibget() failed\n");
		(void) close(sd);
		return (-1);
	}

	rc = cvcd_nic_check(item, ssp_name, nic_name);

	cvcd_mibfree(item);

	(void) cvcd_mibclose(sd);

	return (rc);
}


static mib_item_t *
cvcd_mibget(int sd)
{
	char			buf[512];
	int			flags;
	int			i, j, getcode;
	struct strbuf		ctlbuf, databuf;
	struct T_optmgmt_req	*tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	*toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	*tea = (struct T_error_ack *)buf;
	struct opthdr		*req;
	mib_item_t		*first_item = (mib_item_t *)0;
	mib_item_t		*last_item  = (mib_item_t *)0;
	mib_item_t		*temp;

	tor->PRIM_type = T_OPTMGMT_REQ;
	tor->OPT_offset = sizeof (struct T_optmgmt_req);
	tor->OPT_length = sizeof (struct opthdr);
	tor->MGMT_flags = T_CURRENT;
	req = (struct opthdr *)&tor[1];
	req->level = MIB2_IP;		/* any MIB2_xxx value ok here */
	req->name  = 0;
	req->len   = 0;

	ctlbuf.buf = buf;
	ctlbuf.len = tor->OPT_length + tor->OPT_offset;
	flags = 0;
	if (putmsg(sd, &ctlbuf, (struct strbuf *)0, flags) == -1) {
		perror("cvcd_mibget: putmsg(ctl) failed");
		goto error_exit;
	}
	/*
	 * each reply consists of a ctl part for one fixed structure
	 * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
	 * containing an opthdr structure.  level/name identify the entry,
	 * len is the size of the data part of the message.
	 */
	req = (struct opthdr *)&toa[1];
	ctlbuf.maxlen = sizeof (buf);
	j = 1;
	/*CONSTCOND*/
	while (1) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, (struct strbuf *)0, &flags);
		if (getcode == -1) {
			perror("cvcd_mibget: getmsg(ctl) failed");
			if (debug) {
				cvcd_err(LOG_DEBUG,
					"#   level   name    len\n");
				i = 0;
				for (last_item = first_item; last_item;
					last_item = last_item->next_item)
					cvcd_err(LOG_DEBUG,
					    "%d  %4ld   %5ld   %ld\n", ++i,
					    last_item->group,
					    last_item->mib_id,
					    last_item->length);
			}
			goto error_exit;
		}
		if (getcode == 0 &&
				ctlbuf.len >= sizeof (struct T_optmgmt_ack) &&
				toa->PRIM_type == T_OPTMGMT_ACK &&
				toa->MGMT_flags == T_SUCCESS &&
				req->len == 0) {
			if (debug)
				cvcd_err(LOG_DEBUG,
		"cvcd_mibget: getmsg() %d returned EOD (level %ld, name %ld)\n",
					j, req->level, req->name);
			return (first_item);		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof (struct T_error_ack) &&
				tea->PRIM_type == T_ERROR_ACK) {
			cvcd_err(LOG_ERR,
				"cvcd_mibget: %d gives T_ERROR_ACK: "
				"TLI_error = 0x%lx, UNIX_error = 0x%lx\n",
				j, tea->TLI_error, tea->UNIX_error);
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			goto error_exit;
		}

		if (getcode != MOREDATA ||
				ctlbuf.len < sizeof (struct T_optmgmt_ack) ||
				toa->PRIM_type != T_OPTMGMT_ACK ||
				toa->MGMT_flags != T_SUCCESS) {
			cvcd_err(LOG_ERR,
				"cvcd_mibget:getmsg(ctl) %d returned %d, "
				"ctlbuf.len = %d, PRIM_type = %ld\n",
				j, getcode, ctlbuf.len, toa->PRIM_type);
			if (toa->PRIM_type == T_OPTMGMT_ACK)
				cvcd_err(LOG_ERR,
	"cvcd_mibget:T_OPTMGMT_ACK: MGMT_flags = 0x%lx, req->len = %ld\n",
					toa->MGMT_flags, req->len);
			errno = ENOMSG;
			goto error_exit;
		}

		temp = (mib_item_t *)malloc(sizeof (mib_item_t));
		if (!temp) {
			cvcd_err(LOG_ERR, "cvcd_mibget:malloc failed");
			goto error_exit;
		}
		if (last_item)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = (mib_item_t *)0;
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc((int)req->len);
		if (debug)
			cvcd_err(LOG_DEBUG,
	"cvcd_mibget: msg %d: group = %4ld  mib_id = %5ld length = %ld\n",
				j, last_item->group, last_item->mib_id,
				last_item->length);

		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, (struct strbuf *)0, &databuf, &flags);
		if (getcode == -1) {
			cvcd_err(LOG_ERR, "cvcd_mibget: getmsg(data) failed");
			goto error_exit;
		} else if (getcode != 0) {
			cvcd_err(LOG_ERR,
				"cvcd_mibget: getmsg(data) returned %d, "
				"databuf.maxlen = %d, databuf.len = %d\n",
				getcode, databuf.maxlen, databuf.len);
			goto error_exit;
		}
		j++;
	}

error_exit:;
	cvcd_mibfree(first_item);

	return	((mib_item_t *)0);
}


static void
cvcd_mibfree(mib_item_t *first_item)
{
	mib_item_t	*temp_item;

	while (first_item) {
		temp_item = first_item;
		first_item = first_item->next_item;
		if (temp_item->valp)
			free(temp_item->valp);
		free(temp_item);
	}
}


static int
cvcd_mibopen(void)
{
	int	sd;

	if ((sd = open("/dev/ip", 2)) == -1) {
		perror("ip open");
		(void) close(sd);
		return (-1);
	}
	/* must be above ip, below tcp */
	if (ioctl(sd, I_PUSH, "arp") == -1) {
		perror("arp I_PUSH");
		(void) close(sd);
		return (-1);
	}
	if (ioctl(sd, I_PUSH, "tcp") == -1) {
		perror("tcp I_PUSH");
		(void) close(sd);
		return (-1);
	}
	if (ioctl(sd, I_PUSH, "udp") == -1) {
		perror("udp I_PUSH");
		(void) close(sd);
		return (-1);
	}
	return (sd);
}

static int
cvcd_mibclose(int fd)
{
	return (close(fd));
}

static char *
cvcd_octetstr(char *buf, Octet_t *op)
{
	return (strncpy(buf, op->o_bytes, op->o_length));
}

/*
 * return 0 if ssp connection found on nic
 * return 1 if error
 */
static int
cvcd_nic_check(mib_item_t *item, char *ssp_name, char *nic_name)
{
	int		jtemp = 0;
	char		buf1[IFNAMSIZ + 1];
	char		buf2[MAXHOSTNAMELEN + 1];
	mib2_ipNetToMediaEntry_t	*np;
	int		notfound = 1;

	for (; item; item = item->next_item) {
		jtemp++;
		if (debug) {
			cvcd_err(LOG_DEBUG, "\n--- Entry %d ---\n", jtemp);
			cvcd_err(LOG_DEBUG,
		"Group = %ld, mib_id = %ld, length = %ld, valp = 0x%lx\n",
			item->group, item->mib_id, item->length,
			(long)item->valp);
		}
		if (item->group != MIB2_IP)
			continue;

		switch (item->mib_id) {

		case MIB2_IP_22:
			np = (mib2_ipNetToMediaEntry_t *)item->valp;
			while ((char *)np < item->valp + item->length) {
				/* get src name */
				(void) cvcd_rtname(np->ipNetToMediaNetAddress,
				    buf2);

				if (debug)
					cvcd_err(LOG_DEBUG, "%s & %s  :",
					ssp_name, buf2);
				if (strcmp(ssp_name, buf2) == 0) {
					/* get interface name */
					(void) cvcd_octetstr(buf1,
					    &np->ipNetToMediaIfIndex);
					if (debug)
						cvcd_err(LOG_DEBUG,
						    "%s & %s :",
						    nic_name, buf1);
					if (strcmp(nic_name, buf1) == 0) {
						notfound = 0;
						break;
					}
				}
				if (debug)
					cvcd_err(LOG_DEBUG, "\n");
				np++;
			}
			break;

		case 0:
		case MIB2_IP_20:
		case MIB2_IP_21:
		case EXPER_IP_GROUP_MEMBERSHIP:
			break;

		default:
			if (debug)
				cvcd_err(LOG_DEBUG,
				    "cvcd_nic_check: unknown group = %ld\n",
				    item->group);
			break;
		}
	}
	(void) fflush(stdout);
	return (notfound);
}


static char *
cvcd_rtname(u_long addr, char *line)
{
	register char  *cp;
	struct hostent *hp;
	static char	domain[MAXHOSTNAMELEN + 1];
	static int	first = 1;

	if (first) {
		first = 0;
		if (sysinfo(SI_HOSTNAME, domain, MAXHOSTNAMELEN) != -1 &&
			(cp = strchr(domain, '.'))) {
			(void) strcpy(domain, cp + 1);
		} else
			domain[0] = 0;
	}
	cp = 0;
	hp = gethostbyaddr((char *)&addr, sizeof (u_long), AF_INET);
	if (hp) {
		if ((cp = strchr(hp->h_name, '.')) != 0 &&
				strcmp(cp + 1, domain) == 0)
			*cp = 0;
			cp = hp->h_name;
	}
	if (cp)
		(void) strcpy(line, cp);
	else {
		struct in_addr in;

		in.s_addr = addr;
		(void) strcpy(line, inet_ntoa(in));
	}
	return (line);
}
