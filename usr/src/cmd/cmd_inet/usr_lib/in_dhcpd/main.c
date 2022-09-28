#ident	"@(#)main.c	1.58	98/01/13 SMI"

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * This file contains the argument parsing routines of the dhcpd daemon.
 * It corresponds to the START state as spec'ed.
 */

/*
 * Multithreading Notes:
 * =====================
 *
 * libdhcp is not MT-safe; thus only the main thread can successfully access
 * routines contained therein.
 *
 * There is a thread per configured interface which reads requests,
 * determines if they are for this server, and appends them to the
 * interface's PKT list.
 *
 * The main thread creates a thread to handle signals. All subsequent threads
 * (and the main thread) mask out all signals.
 *
 * The main thread will deal with the -t option. This is done using a
 * condition variable and cond_timedwait() to provide the idle timeout we
 * used to get via poll(). The condition the main thread is waiting for
 * is for npkts to become greater than zero.
 *
 * dhcp_offer: if the main thread determines that a icmp_echo check is
 * needed, it calls icmp_echo_register(), which creates a DETACHED thread,
 * puts the pkt on the interface's PKT list marked with a DHCP_ICMP_PENDING
 * flag. The ICMP validation thread (icmp_echo_async) creates the ICMP echo
 * request, and waits for a replies. If one is returned, it locates the PKT
 * list entry, and changes the flag from DHCP_ICMP_PENDING to DHCP_ICMP_IN_USE.
 * If one isn't returned in the time limit, it marks the flag from
 * DHCP_ICMP_PENDING to DHCP_ICMP_AVAILABLE. We prevent dhcp_offer from
 * registering the same address for ICMP validation due to multiple DISCOVERS
 * by using icmp_echo_status() in select_offer() to ensure we don't offer IP
 * addresses currently undergoing ICMP validation.
 *
 * bootp: If automatic allocation is in effect,
 * bootp behaves in the same fashion as dhcp_offer.
 *
 * Summary:
 *
 *	Threads:
 *		1) Main thread: Handles responding to clients; database reads
 *		   and writes. Also implements the -t option thru the use
 *		   of cond_timedwait() - The main thread goes into action
 *		   if npkts becomes non-zero or the timeout occurs. If the
 *		   timeout occurs, the main thread runs the idle routine.
 *
 *		2) Signal thread: The main thread creates this thread, and
 *		   then masks out all signals. The signal thread waits on
 *		   sigwait(), and processes all signals. It notifies the
 *		   main thread of EINTR or ETERM via a global variable, which
 *		   the main thread checks upon the exit to cond_timedwait.
 *		   This thread is on it's own LWP, and is DETACHED | DAEMON.
 *		   The thread function is sig_handle().
 *
 *		3) Interface threads: Each interface structure has a thread
 *		   associated with it (created in open_interfaces) which is
 *		   responsible for polling the interface, validating bootp
 *		   packets received, and placing them on the interface's
 *		   PKT_LIST. The thread function is monitor_interface().
 *		   When notified by the main thread via the thr_exit flag,
 *		   the thread prints interface statistics for the interface,
 *		   and then exits.
 *
 *		4) ICMP validation threads: Created as needed when dhcp_offer
 *		   or bootp_compatibility wish to ICMP validate a potential
 *		   OFFER candidate IP address. These threads are created
 *		   DETACHED and SUSPENDED by the main thread, which then
 *		   places the plp structure associated with the request
 *		   back on the interface's PKT_LIST, then continues the
 *		   thread. A ICMP validation thread exits when it has
 *		   determined the status of an IP address, changing the
 *		   d_icmpflag in the plp appropriately. It then bumps npkts
 *		   by one, and notifies the main thread that there is work
 *		   to do via cond_signal. It then exits. The thread function
 *		   is icmp_echo_async().
 *
 *	Locks:
 *		1) npkts_mtx	-	Locks the global npkts.
 *		   npkts_cv	-	Condition variable for npkts.
 *					Lock-order independent.
 *
 *		2) totpkts_mtx	-	Locks the global totpkts.
 *					Lock-order independent.
 *
 *		3) if_head_mtx	-	Locks the global interface list.
 *
 *		4) ifp_mtx	-	Locks contents of the enclosed
 *					interface (IF) structure, including
 *					such things as thr_exit flag and
 *					statistics counters.
 *
 *		5) pkt_mtx	-	Locks PKT_LIST head list within the
 *					enclosed interface (IF) structure.
 *					This lock should be held before
 *					plp_mtx is held, and unlocked AFTER
 *					plp_mtx is unlocked.
 *
 *		6) plp_mtx	-	Mutex lock protecting access to
 *					a plp's structure elements,
 *					specifically those associated with
 *					a ICMP validation - off_ip and
 *					d_icmpflag. pkt_mtx of IF structure
 *					containing the list for which this
 *					plp is a member should be locked
 *					before this lock is held. Note that
 *					if this plp is not currently on
 *					a PKT_LIST, there's no need to hold
 *					either lock.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/systeminfo.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/dhcp.h>
#include <synch.h>
#include <netdb.h>
#include <dhcdata.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include <locale.h>

extern int optind, opterr;
extern char *optarg;

int verbose;
int debug;
int noping = FALSE;		/* Always ping before offer by def */
time_t icmp_timeout;		/* milliseconds to wait for response */
int icmp_tries = DHCP_ICMP_ATTEMPTS;	/* Number of attempts @ icmp_timeout */
int ethers_compat = TRUE;	/* set if server should check ethers table */
int no_dhcptab;			/* set if no dhcptab exists */
int be_automatic;		/* set if bootp server should allocate IPs */
int reinitialize;		/* set to reinitialize when idle */
int server_mode = TRUE;		/* set if running in server mode */
time_t off_secs = 0L;		/* def ttl of an offer */
int max_hops = DHCP_DEF_HOPS; 	/* max relay hops before discard */
time_t rescan_interval = 0L;	/* dhcptab rescan interval */
time_t abs_rescan = 0L;		/* absolute dhcptab rescan time */
struct in_addr	server_ip;	/* IP address of server's default hostname */

/*
 * This global variable keeps the total number of packets waiting to
 * be processed.
 */
u_long npkts;
mutex_t	npkts_mtx;
cond_t	npkts_cv;

/*
 * This global keeps a running total of all packets received by all
 * interfaces.
 */
u_long totpkts;
mutex_t totpkts_mtx;

/*
 * This global is set by the signal handler when the main thread (and thus
 * the daemon should exit. No need to lock this one, as it is set once, and
 * then read (until main thread reads it and starts the exit procedure).
 */
static int time_to_go = 0;

static void usage(void);
static void local_closelog(void);
static void *sig_handle(void *);

int
main(int argc, char *argv[])
{
	int	found;
	int	tnpkts;
	int	bootp_compat = FALSE;	/* set if bootp compat mode enabled */
	sigset_t	set;
	int	c, i, ns;
	int	err = 0;
	IF	*ifp;
	PKT_LIST *plp, *tplp;
	char	*tp, *triesp, *datastore;
	struct rlimit	rl;
	struct hostent	*hp;
	thread_t	sigthread;
	int		tbl_err;
	char		*pathp = NULL;
	char		scratch[MAXHOSTNAMELEN + 1];
	timestruc_t	to;

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN	"SYS_TEXT"
#endif	/* ! TEXT_DOMAIN */

	(void) textdomain(TEXT_DOMAIN);

	if (geteuid() != (uid_t) 0) {
		(void) fprintf(stderr, gettext("Must be 'root' to run %s.\n"),
		    DHCPD);
		return (EPERM);
	}

#ifdef	PSARC1997112
	while ((c = getopt(argc, argv, "denvh:o:r:b:i:p:t:")) != -1) {
#else
	while ((c = getopt(argc, argv, "denvh:o:r:b:i:t:")) != -1) {
#endif	/* PSARC1997112 */
		switch (c) {
		case 'd':
			debug = TRUE;
			break;
		case 'n':
			noping = TRUE;
			break;
		case 'v':
			verbose = TRUE;
			break;
		case 'e':
			/* Disable ethers mode for PPC clients. */
			ethers_compat = FALSE;
			break;
		case 'r':
			/*
			 * Relay Agent Mode.  Arguments better be IP addrs
			 * or hostnames!
			 */
			server_mode = FALSE;

			if ((err = relay_agent_init(optarg)) != 0) {
				usage();
				return (err);
			}
			break;
		case 'b':
			/* Bootp compatibility mode. */
			bootp_compat = TRUE;

			if (strcmp(optarg, "automatic") == 0) {
				be_automatic = TRUE;
			} else if (strcmp(optarg, "manual") == 0) {
				be_automatic = FALSE;
			} else {
				usage();
				return (1);
			}
			break;
		case 'h':
			/* Non default relay hops maximum. */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
				    "Maximum relay hops must be numeric.\n"));
				return (1);
			}
			max_hops = atoi(optarg);
			if (max_hops == 0) {
				max_hops = DHCP_DEF_HOPS;
				(void) fprintf(stderr, gettext("Couldn't \
determine max hops from: %s, defaulting to: %d\n"), optarg, max_hops);
			}
			break;
		case 'i':
			/* Comma-separated list of interfaces. */
			interfaces = optarg;
			break;
		case 'o':
			/* Time to Live (secs) for dhcp Offers. */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
				    "DHCP Offer TTL must be an integer.\n"));
				return (1);
			}
			off_secs = atoi(optarg);
			if (off_secs == 0) {
				(void) fprintf(stderr, gettext("Could not \
determine DHCP offer TTL from: %s, defaulting to: %d\n"),
				    optarg, DHCP_OFF_SECS);
				off_secs = DHCP_OFF_SECS;
			}
			break;
		case 'p':
			/* ICMP echo validation options - <timeout>:<tries> */
			if ((triesp = strchr(optarg, ':')) != NULL)
				*triesp++ = '\0';
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
"ICMP timeout interval must be an integer (milliseconds)\n"));
				return (1);
			}
			icmp_timeout = atoi(optarg);

			if (icmp_timeout == 0) {
				(void) fprintf(stderr, gettext(
"Could not determine ICMP timeout interval from: %s, defaulting to: %d\n"),
				    optarg, DHCP_ICMP_TIMEOUT);
				icmp_timeout = DHCP_ICMP_TIMEOUT;
			}

			if (triesp != NULL) {
				if (!isdigit(*triesp)) {
					(void) fprintf(stderr, gettext(
"ICMP attempts must be an integer.\n"));
					return (1);
				}
				icmp_tries = atoi(triesp);
				if (icmp_tries == 0) {
					(void) fprintf(stderr, gettext(
"Could not determine ICMP attempts from: %s, defaulting to: %d\n"),
					    triesp, DHCP_ICMP_ATTEMPTS);
					icmp_tries = DHCP_ICMP_ATTEMPTS;
				}
			}
			break;
		case 't':
			/* dhcptab rescan interval (secs). */
			if (!isdigit(*optarg)) {
				(void) fprintf(stderr, gettext(
"dhcptab rescan interval must be an integer (minutes)\n"));
				return (1);
			}
			rescan_interval = atoi(optarg);
			if (rescan_interval == 0) {
				(void) fprintf(stderr, gettext("Zero dhcptab \
rescan interval, defaulting to no rescan.\n"));
			} else {
				abs_rescan = (rescan_interval * 60L) +
				    time(NULL);
			}
			break;
		default:
			usage();
			return (1);
		}
	}

	if (server_mode) {
		if (noping && icmp_timeout) {
			(void) fprintf(stderr, gettext(
			    "Option 'n' and 'p' mutually exclusive.\n"));
			usage();
			return (1);
		}
		if (noping)
			(void) fprintf(stderr, gettext("\nWARNING: Disabling \
duplicate IP address detection!\n\n"));
		if (icmp_timeout == 0)
			icmp_timeout = DHCP_ICMP_TIMEOUT;
		if (off_secs == 0L)
			off_secs = DHCP_OFF_SECS;	/* use default */
		if (ethers_compat && stat_boot_server() == 0) {
			/*
			 * Respect user's -b setting. Use -b manual as the
			 * default.
			 */
			if (bootp_compat == FALSE) {
				bootp_compat = TRUE;
				be_automatic = FALSE;
			}
		} else
			ethers_compat = FALSE;
		ns = dd_ns(&tbl_err, &pathp);
		if (ns == TBL_FAILURE) {
			switch (tbl_err) {
			case TBL_BAD_SYNTAX:
				(void) fprintf(stderr, gettext(
"%s: Bad syntax: keyword is missing colon (:)\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_NS:
				(void) fprintf(stderr, gettext(
"%s: Bad resource name. Must be 'files' or 'nisplus'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_BAD_DIRECTIVE:
				(void) fprintf(stderr, gettext(
"%s: Unsupported keyword. Must be 'resource:' or 'path:'.\n"), TBL_NS_FILE);
				err = 1;
				break;
			case TBL_STAT_ERROR:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value does not exist.\n"), TBL_NS_FILE);
				break;
			case TBL_BAD_DOMAIN:
				(void) fprintf(stderr, gettext(
"%s: Specified 'path' keyword value must be a valid nisplus domain name.\n"),
				    TBL_NS_FILE);
				err = 1;
				break;
			case TBL_OPEN_ERROR:
				if (ethers_compat) {
					(void) fprintf(stderr, gettext(
					    "WARNING: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 0; /* databases not required */
				} else {
					(void) fprintf(stderr, gettext(
					    "FATAL: %s does not exist.\n"),
					    TBL_NS_FILE);
					err = 1;
				}
				break;
			}
			if (err != 0)
				return (err);
		}
	} else {
		ethers_compat = FALSE;	/* Means nothing here. */
		if (noping)
			(void) fprintf(stderr, gettext(
			    "Option 'n' invalid in relay agent mode.\n"));
		if (icmp_timeout)
			(void) fprintf(stderr, gettext(
			    "Option 'p' invalid in relay agent mode.\n"));
		if (rescan_interval)
			(void) fprintf(stderr, gettext(
			    "Option 't' invalid in relay agent mode.\n"));
		if (off_secs)
			(void) fprintf(stderr, gettext(
			    "Option 'o' invalid in relay agent mode.\n"));
		if (bootp_compat)
			(void) fprintf(stderr, gettext(
			    "Option 'b' invalid in relay agent mode.\n"));
		if (noping || rescan_interval || off_secs || bootp_compat ||
		    icmp_timeout) {
			usage();
			return (1);
		}
	}

	if (!debug) {
		/* Daemon (background, detach from controlling tty). */
		switch (fork()) {
		case -1:
			(void) fprintf(stderr,
			    gettext("Daemon cannot fork(): %s\n"),
			    strerror(errno));
			return (errno);
		case 0:
			/* child */
			break;
		default:
			/* parent */
			return (0);
		}

		if ((err = getrlimit(RLIMIT_NOFILE, &rl)) < 0) {
			dhcpmsg(LOG_ERR, "Can't get resource limits: %s\n",
			    strerror(errno));
			return (err);
		}

		for (i = 0; (rlim_t) i < rl.rlim_cur; i++)
			(void) close(i);

		errno = 0;	/* clean up benign bad file no error */

		(void) open("/dev/null", O_RDONLY, 0);
		(void) dup2(0, 1);
		(void) dup2(0, 2);

		/* Detach console */
		(void) setsid();

		(void) openlog(DHCPD, LOG_PID, LOG_DAEMON);
		if (verbose)
			dhcpmsg(LOG_INFO, "Daemon started.\n");
	}

	/*
	 * Block all signals in main thread - threads created will also
	 * ignore signals.
	 */
	(void) sigfillset(&set);
	(void) thr_sigsetmask(SIG_SETMASK, &set, NULL);

	/*
	 * Create signal handling thread.
	 */
	if ((err = thr_create(NULL, 0, sig_handle, NULL,
	    THR_NEW_LWP | THR_DAEMON | THR_DETACHED, &sigthread)) != 0) {
		(void) fprintf(stderr,
		    gettext("Cannot start signal handling thread, error: %d\n"),
			err);
		return (err);
	}

	(void) sigdelset(&set, SIGABRT);	/* except for abort */

#ifdef	DEBUG
	(void) fprintf(stderr,
	    gettext("Started signal handling thread: %d\n"), sigthread);
#endif	/* DEBUG */

	/* Save away the IP address associated with our HOSTNAME. */
	(void) sysinfo(SI_HOSTNAME, scratch, MAXHOSTNAMELEN + 1);
	if ((tp = strchr(scratch, '.')) != NULL)
		*tp = '\0';

	if ((hp = gethostbyname(scratch)) != NULL &&
	    hp->h_addrtype == AF_INET &&
	    hp->h_length == sizeof (struct in_addr)) {
		(void) memcpy((char *)&server_ip, hp->h_addr_list[0],
		    sizeof (server_ip));
	} else {
		dhcpmsg(LOG_ERR,
		    "Cannot determine server hostname/IP address.\n");
		local_closelog();
		return (1);
	}

	if (verbose) {
		dhcpmsg(LOG_INFO, "Daemon Version: %s\n", DAEMON_VERS);
		dhcpmsg(LOG_INFO, "Maximum relay hops: %d\n", max_hops);
		if (server_mode) {
			dhcpmsg(LOG_INFO, "Run mode is: DHCP Server Mode.\n");
			switch (ns) {
			case TBL_NS_UFS:
				datastore = "files";
				break;
			case TBL_NS_NISPLUS:
				datastore = "nisplus";
				pathp = (
				    (tp = getenv("NIS_PATH")) == NULL ? pathp :
				    tp);
				break;
			default:
				datastore = pathp = "none";
				break;
			}
			dhcpmsg(LOG_INFO, "Datastore: %s\n", datastore);
			dhcpmsg(LOG_INFO, "Path: %s\n", pathp);
			dhcpmsg(LOG_INFO, "DHCP offer TTL: %d\n", off_secs);
			if (ethers_compat)
				dhcpmsg(LOG_INFO,
				    "Ethers compatibility enabled.\n");
			if (bootp_compat)
				dhcpmsg(LOG_INFO,
				    "BOOTP compatibility enabled.\n");
			if (rescan_interval != 0) {
				dhcpmsg(LOG_INFO,
				    "Dhcptab rescan interval: %d minutes.\n",
				    rescan_interval);
			}
			if (icmp_timeout != 0) {
				dhcpmsg(LOG_INFO,
"ICMP validation timeout: %d milliseconds, Attempts: %d.\n",
				    icmp_timeout, icmp_tries);
			} else
				icmp_timeout = DHCP_ICMP_TIMEOUT;
		} else {
			dhcpmsg(LOG_INFO,
			    "Run mode is: Relay Agent Mode.\n");
		}
	}

	if ((err = open_interfaces()) != 0) {
		local_closelog();
		return (err);
	}

	(void) mutex_init(&npkts_mtx, USYNC_THREAD, 0);
	(void) cond_init(&npkts_cv, USYNC_THREAD, 0);
	(void) mutex_init(&totpkts_mtx, USYNC_THREAD, 0);

	if (server_mode) {

		if (inittab() != 0) {
			dhcpmsg(LOG_ERR, "Cannot allocate macro hash table.\n");
			local_closelog();
			(void) mutex_destroy(&npkts_mtx);
			(void) cond_destroy(&npkts_cv);
			(void) mutex_destroy(&totpkts_mtx);
			return (1);
		}

		if ((err = checktab()) != 0 ||
		    (err = readtab(NEW_DHCPTAB)) != 0) {
			if (err == ENOENT || ethers_compat) {
				no_dhcptab = TRUE;
			} else {
				dhcpmsg(LOG_ERR,
				    "Error reading macro table.\n");
				local_closelog();
				(void) mutex_destroy(&npkts_mtx);
				(void) cond_destroy(&npkts_cv);
				(void) mutex_destroy(&totpkts_mtx);
				return (err);
			}
		} else
			no_dhcptab = FALSE;
	}

	/*
	 * While forever, read packets off selected/available interfaces
	 * and dispatch off to handle them.
	 */
	for (;;) {
		(void) mutex_lock(&npkts_mtx);
		if (server_mode) {
			to.tv_sec = time(NULL) + DHCP_IDLE_TIME;
			to.tv_nsec = 0;
			while (npkts == 0 && time_to_go == 0) {
				if (cond_timedwait(&npkts_cv, &npkts_mtx,
				    &to) == ETIME || reinitialize) {
					err = idle();
					break;
				}
			}
		} else {
			/* No idle processing */
			while (npkts == 0 && time_to_go == 0)
				(void) cond_wait(&npkts_cv, &npkts_mtx);
		}
		tnpkts = npkts;
		(void) mutex_unlock(&npkts_mtx);

		/*
		 * Fatal error during idle() processing or sig_handle thread
		 * says it's time to go...
		 */
		if (err != 0 || time_to_go)
			break;

		/*
		 * We loop through each interface and process one packet per
		 * interface. (that's one non-DHCP_ICMP_PENDING packet). Relay
		 * agent tasks are handled by the per-interface threads, thus
		 * we should only be dealing with bootp/dhcp server bound
		 * packets here.
		 *
		 * The main thread treats the interface packet lists as
		 * "stacks", or FIFO objects. We do this so that we get
		 * the latest, equivalent request from the client before
		 * responding, thus keeping the chance of responding to
		 * moldy requests to an absolute minimum.
		 */
		(void) mutex_lock(&if_head_mtx);
		for (ifp = if_head; ifp != NULL && tnpkts != 0;
		    ifp = ifp->next) {
			(void) mutex_lock(&ifp->pkt_mtx);
#ifdef	DEBUG_PKTLIST
			dhcpmsg(LOG_DEBUG, "Main: npkts is %d\n", npkts);
			display_pktlist(ifp);
#endif	/* DEBUG_PKTLIST */
			/*
			 * Remove the last packet from the list
			 * which is not awaiting ICMP ECHO
			 * validation (DHCP_ICMP_PENDING).
			 */
			plp = ifp->pkttail;
			found = FALSE;
			while (plp != NULL) {
				(void) mutex_lock(&plp->plp_mtx);
				switch (plp->d_icmpflag) {
				case DHCP_ICMP_NOENT:
					detach_plp(ifp, plp);
					(void) mutex_unlock(&plp->plp_mtx);
					/*
					 * See if there's an earlier one
					 * with a different status, exchanging
					 * this plp for that one. If that
					 * one is DHCP_ICMP_PENDING, skip it.
					 */
					plp = refresh_pktlist(ifp, plp);
					(void) mutex_lock(&plp->plp_mtx);
					if (plp->d_icmpflag !=
					    DHCP_ICMP_PENDING) {
						found = TRUE;
					}
					break;
				case DHCP_ICMP_AVAILABLE:
					/* FALLTHRU */
				case DHCP_ICMP_IN_USE:
					detach_plp(ifp, plp);
					found = TRUE;
					break;
				case DHCP_ICMP_PENDING:
					/* Skip this one. */
					break;
				case DHCP_ICMP_FAILED:
					/* FALLTHRU */
				default:
					/* clean up any failed ICMP attempts */
#ifdef	DEBUG
					{
						char	ntoab[NTOABUF];
						dhcpmsg(LOG_DEBUG,
						    "Failed ICMP attempt: %s\n",
						    inet_ntoa_r(plp->off_ip,
						    ntoab));
					}
#endif	/* DEBUG */
					tplp = plp;
					plp = plp->prev;
					detach_plp(ifp, tplp);
					(void) mutex_unlock(&tplp->plp_mtx);
					free_plp(tplp);
					continue;
				}
				(void) mutex_unlock(&plp->plp_mtx);

				if (found)
					break;
				else
					plp = plp->prev;
			}
			(void) mutex_unlock(&ifp->pkt_mtx);

			if (plp == NULL)
				continue;  /* nothing on this interface */

			/*
			 * No need to hold plp_mtx for balance of loop.
			 * We have already avoided potential DHCP_ICMP_PENDING
			 * packets.
			 */

			(void) mutex_lock(&npkts_mtx);
			tnpkts = --npkts; /* one less packet to process. */
			(void) mutex_unlock(&npkts_mtx);

			/*
			 * Based on the packet type, process accordingly.
			 */
			if (plp->pkt->op == BOOTREQUEST) {
				if (plp->opts[CD_DHCP_TYPE]) {
					/* DHCP packet */
					if (dhcp(ifp, plp) == FALSE) {
						/* preserve plp */
						continue;
					}
				} else {
					/* BOOTP packet */
					if (!bootp_compat) {
						dhcpmsg(LOG_INFO,
"BOOTP request received on interface: %s ignored.\n",
						    ifp->nm);
					} else {
						if (bootp(ifp, plp) == FALSE) {
							/* preserve plp */
							continue;
						}
					}
				}
			} else {
				dhcpmsg(LOG_ERR,
"Unexpected packet received on BOOTP server port. Interface: %s. Ignored.\n",
				    ifp->nm);
			}
			(void) mutex_lock(&ifp->ifp_mtx);
			ifp->processed++;
			(void) mutex_unlock(&ifp->ifp_mtx);
			free_plp(plp); /* Free the packet. */
		}
		(void) mutex_unlock(&if_head_mtx);
	}

	/* Daemon terminated. */
	if (server_mode)
		resettab();

	close_interfaces();	/* reaps threads */
	local_closelog();
	(void) fflush(NULL);
	(void) mutex_destroy(&npkts_mtx);
	(void) mutex_destroy(&totpkts_mtx);
	return (err);
}

/*
 * Signal handler routine. All signals handled by calling thread.
 */
/* ARGSUSED */
static void *
sig_handle(void *arg)
{
	int		sig;
	sigset_t	set;
	char buf[SIG2STR_MAX];

	(void) sigfillset(&set);	/* catch all signals */

	/* wait for a signal */
	for (;;) {
		switch (sig = sigwait(&set)) {
		case SIGHUP:
			reinitialize = sig;
			break;
		case SIGTERM:
			/* FALLTHRU */
		case SIGINT:
			(void) sig2str(sig, buf);
			dhcpmsg(LOG_ERR, "Signal: %s received...Exiting\n",
			    buf);
			time_to_go = 1;
			break;
		default:
			if (verbose) {
				(void) sig2str(sig, buf);
				dhcpmsg(LOG_INFO,
				    "Signal: %s received...Ignored\n",
				    buf);
			}
			break;
		}
		if (time_to_go) {
			(void) mutex_lock(&npkts_mtx);
			(void) cond_broadcast(&npkts_cv);
			(void) mutex_unlock(&npkts_mtx);
			break;
		}
	}
	thr_exit(NULL);
	return ((void *)NULL);	/* NOTREACHED */
}

static void
usage(void)
{
#ifdef	PSARC1997112
	(void) fprintf(stderr, gettext("%s:\n\tCommon: [-d] [-v] \
[-i interface, ...] [-h hops]\n\n\tServer: [-e] [-t rescan_interval] \
[-o DHCP_offer_TTL]\n\t\t[ -n | [-p icmp_timeout[:number_of_attempts]]]\n\t\t\
[ -b automatic | manual]\n\n\tRelay Agent: -r IP | hostname, ...\n"), DHCPD);
#else
	(void) fprintf(stderr, gettext("%s:\n\tCommon: [-d] [-v] \
[-i interface, ...] [-h hops]\n\n\tServer: [-e] [-t rescan_interval] \
[-o DHCP_offer_TTL]\n\t\t[ -n ]\n\t\t\
[ -b automatic | manual]\n\n\tRelay Agent: -r IP | hostname, ...\n"), DHCPD);
#endif	/* PSARC1997112 */
}

static void
local_closelog(void)
{
	dhcpmsg(LOG_INFO, "Daemon terminated.\n");
	if (!debug)
		closelog();
}

/*
 * Given a received BOOTP packet, generate an appropriately sized,
 * and generically initialized BOOTP packet.
 */
PKT *
gen_bootp_pkt(int size, PKT *srcpktp)
{
	/* LINTED [smalloc returns lw aligned addresses.] */
	PKT *pkt = (PKT *)smalloc(size);

	pkt->htype = srcpktp->htype;
	pkt->hlen = srcpktp->hlen;
	pkt->xid = srcpktp->xid;
	pkt->secs = srcpktp->secs;
	pkt->flags = srcpktp->flags;
	pkt->giaddr.s_addr = srcpktp->giaddr.s_addr;
	(void) memcpy(pkt->cookie, srcpktp->cookie, 4);
	(void) memcpy(pkt->chaddr, srcpktp->chaddr, srcpktp->hlen);

	return (pkt);
}

/*
 * Points field serves to identify those packets whose allocated size
 * and address is not represented by the address in pkt.
 */
void
free_plp(PKT_LIST *plp)
{
	char *tmpp;

	(void) mutex_lock(&plp->plp_mtx);
#ifdef	DEBUG
	{
		char	ntoab[NTOABUF];
		dhcpmsg(LOG_DEBUG,
"%04d: free_plp(0x%x)pkt(0x%x)len(%d)icmp(%d)IP(%s)next(0x%x)prev(0x%x)\n",
		    thr_self(), plp, plp->pkt, plp->len, plp->d_icmpflag,
		    inet_ntoa_r(plp->off_ip, ntoab), plp->next, plp->prev);
	}
#endif	/* DEBUG */
	if (plp->pkt) {
		if (plp->offset != 0)
			tmpp = (char *)((u_int)plp->pkt - plp->offset);
		else
			tmpp = (char *)plp->pkt;
		free(tmpp);
	}
	(void) mutex_unlock(&plp->plp_mtx);
	(void) mutex_destroy(&plp->plp_mtx);
	free(plp);
	plp = PKT_LIST_NULL;
}
