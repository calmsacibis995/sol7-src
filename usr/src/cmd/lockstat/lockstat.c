/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lockstat.c	1.4	97/06/29 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/lockstat.h>

static void report_stats(FILE *, lsrec_t **, int, uint64_t, uint64_t);
static void report_trace(FILE *, lsrec_t **);

extern int symtab_init(void);
extern char *addr_to_sym(uintptr_t, uintptr_t *, size_t *);
extern uintptr_t sym_to_addr(char *name);
extern size_t sym_size(char *name);
extern char *strtok_r(char *, const char *, char **);

#define	DEFAULT_NLOCKS	10000

static lsctl_t lsctl;
static int stk_depth;
static int watched_locks = 0;
static int nlocks_used;
static int top_n = INT_MAX;
static hrtime_t elapsed_time;
static int do_rates = 0;
static int pflag = 0;
static int Pflag = 0;
static int wflag = 0;
static int Wflag = 0;
static int cflag = 0;

typedef struct ls_event_info {
	char	ev_type;
	char	ev_desc[80];
	char	ev_units[10];
} ls_event_info_t;

static ls_event_info_t ls_event_info[LS_MAX_EVENTS] = {
	{ 'C',	"Adaptive mutex spin",			"spin"	},
	{ 'C',	"Adaptive mutex block",			"nsec"	},
	{ 'C',	"Spin lock spin",			"spin"	},
	{ 'C',	"Thread lock spin",			"spin"	},
	{ 'C',	"R/W writer blocked by writer",		"nsec"	},
	{ 'C',	"R/W writer blocked by readers",	"nsec"	},
	{ 'C',	"R/W reader blocked by writer",		"nsec"	},
	{ 'C',	"R/W reader blocked by write wanted",	"nsec"	},
	{ 'C',	"Unknown event (type 8)",		"units"	},
	{ 'C',	"Unknown event (type 9)",		"units"	},
	{ 'C',	"Unknown event (type 10)",		"units"	},
	{ 'C',	"Unknown event (type 11)",		"units"	},
	{ 'C',	"Unknown event (type 12)",		"units"	},
	{ 'C',	"Unknown event (type 13)",		"units"	},
	{ 'C',	"Unknown event (type 14)",		"units"	},
	{ 'C',	"Unknown event (type 15)",		"units"	},
	{ 'C',	"Unknown event (type 16)",		"units"	},
	{ 'C',	"Unknown event (type 17)",		"units"	},
	{ 'C',	"Unknown event (type 18)",		"units"	},
	{ 'C',	"Unknown event (type 19)",		"units"	},
	{ 'C',	"Unknown event (type 20)",		"units"	},
	{ 'C',	"Unknown event (type 21)",		"units"	},
	{ 'C',	"Unknown event (type 22)",		"units"	},
	{ 'C',	"Unknown event (type 23)",		"units"	},
	{ 'C',	"Unknown event (type 24)",		"units"	},
	{ 'C',	"Unknown event (type 25)",		"units"	},
	{ 'C',	"Unknown event (type 26)",		"units"	},
	{ 'C',	"Unknown event (type 27)",		"units"	},
	{ 'C',	"Unknown event (type 28)",		"units"	},
	{ 'C',	"Unknown event (type 29)",		"units"	},
	{ 'C',	"Unknown event (type 30)",		"units"	},
	{ 'C',	"Unknown event (type 31)",		"units"	},
	{ 'H',	"Adaptive mutex hold",			"nsec"	},
	{ 'H',	"Spin lock hold",			"nsec"	},
	{ 'H',	"R/W writer hold",			"nsec"	},
	{ 'H',	"R/W reader hold",			"nsec"	},
	{ 'H',	"Unknown event (type 36)",		"units"	},
	{ 'H',	"Unknown event (type 37)",		"units"	},
	{ 'H',	"Unknown event (type 38)",		"units"	},
	{ 'H',	"Unknown event (type 39)",		"units"	},
	{ 'H',	"Unknown event (type 40)",		"units"	},
	{ 'H',	"Unknown event (type 41)",		"units"	},
	{ 'H',	"Unknown event (type 42)",		"units"	},
	{ 'H',	"Unknown event (type 43)",		"units"	},
	{ 'H',	"Unknown event (type 44)",		"units"	},
	{ 'H',	"Unknown event (type 45)",		"units"	},
	{ 'H',	"Unknown event (type 46)",		"units"	},
	{ 'H',	"Unknown event (type 47)",		"units"	},
	{ 'H',	"Unknown event (type 48)",		"units"	},
	{ 'H',	"Unknown event (type 49)",		"units"	},
	{ 'H',	"Unknown event (type 50)",		"units"	},
	{ 'H',	"Unknown event (type 51)",		"units"	},
	{ 'H',	"Unknown event (type 52)",		"units"	},
	{ 'H',	"Unknown event (type 53)",		"units"	},
	{ 'H',	"Unknown event (type 54)",		"units"	},
	{ 'H',	"Unknown event (type 55)",		"units"	},
	{ 'H',	"Unknown event (type 56)",		"units"	},
	{ 'H',	"Unknown event (type 57)",		"units"	},
	{ 'H',	"Unknown event (type 58)",		"units"	},
	{ 'H',	"Unknown event (type 59)",		"units"	},
	{ 'E',	"Recursive lock entry detected",	"(N/A)"	},
	{ 'E',	"Lockstat enter failure",		"(N/A)"	},
	{ 'E',	"Lockstat exit failure",		"nsec"	},
	{ 'E',	"Lockstat record failure",		"(N/A)"	},
};

static void
fail(int do_perror, char *message, ...)
{
	va_list args;
	int save_errno = errno;

	va_start(args, message);
	(void) fprintf(stderr, "lockstat: ");
	(void) vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		(void) fprintf(stderr, ": %s", strerror(save_errno));
	(void) fprintf(stderr, "\n");
	exit(2);
}

static void
show_events(char event_type, char *desc)
{
	int i, first = -1, last;

	for (i = 0; i < LS_MAX_EVENTS; i++) {
		ls_event_info_t *evp = &ls_event_info[i];
		if (evp->ev_type != event_type ||
		    strncmp(evp->ev_desc, "Unknown event", 13) == 0)
			continue;
		if (first == -1)
			first = i;
		last = i;
	}

	(void) fprintf(stderr,
	    "\n%s events (lockstat -%c or lockstat -e %d-%d):\n\n",
	    desc, event_type, first, last);

	for (i = first; i <= last; i++)
		fprintf(stderr, "%4d = %s\n", i, ls_event_info[i].ev_desc);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: lockstat [options] command [args]\n"
	    "\nEvent selection options:\n\n"
	    "  -C              watch contention events [on by default]\n"
	    "  -E              watch error events [on by default]\n"
	    "  -H              watch hold events [off by default]\n"
	    "  -A              watch all events [equivalent to -CHE]\n"
	    "  -e <event_list> only watch the specified events (shown below);\n"
	    "                  <event_list> is a comma-separated list of\n"
	    "                  events or ranges of events, e.g. 1,4-7,35\n"
	    "\nData gathering options:\n\n"
	    "  -b              basic statistics (lock, caller, event count)\n"
	    "  -t              timing for all events [default]\n"
	    "  -h              histograms for event times\n"
	    "  -s depth        stack traces <depth> deep\n"
	    "\nData filtering options:\n\n"
	    "  -n nlocks       maximum number of locks to watch [default=%d]\n"
	    "  -l lock[,size]  only watch <lock>, which can be specified as a\n"
	    "                  symbolic name or hex address; <size> defaults\n"
	    "                  to the ELF symbol size if available, 1 if not\n"
	    "  -d <duration>   only watch events longer than <duration>\n"
	    "  -T              trace (rather than sample) events\n"
	    "\nData reporting options:\n\n"
	    "  -c              coalesce lock data for arrays like pse_mutex[]\n"
	    "  -w              wherever: don't distinguish events by caller\n"
	    "  -W              whichever: don't distinguish events by lock\n"
	    "  -R              display rates rather than counts\n"
	    "  -p              parsable output format (awk(1)-friendly)\n"
	    "  -P              sort lock data by (count * avg_time) product\n"
	    "  -D <n>          only display top <n> events of each type\n"
	    "  -o <filename>   output filename\n",
	    DEFAULT_NLOCKS);

	show_events('C', "Contention");
	show_events('H', "Hold-time");
	show_events('E', "Error");
	fprintf(stderr, "\n");

	exit(1);
}

static int
lockcmp(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (0);
}

static int
countcmp(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	return (b->ls_count - a->ls_count);
}

static int
timecmp(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_time < b->ls_time)
		return (1);
	if (a->ls_time > b->ls_time)
		return (-1);
	
	return (0);
}

static int
lockcmp_anywhere(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (0);
}

static int
lock_and_count_cmp_anywhere(lsrec_t *a, lsrec_t *b)
{
	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	if (a->ls_lock < b->ls_lock)
		return (-1);
	if (a->ls_lock > b->ls_lock)
		return (1);

	return (b->ls_count - a->ls_count);
}

static int
sitecmp_anylock(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	return (0);
}

static int
site_and_count_cmp_anylock(lsrec_t *a, lsrec_t *b)
{
	int i;

	if (a->ls_event < b->ls_event)
		return (-1);
	if (a->ls_event > b->ls_event)
		return (1);

	for (i = stk_depth - 1; i >= 0; i--) {
		if (a->ls_stack[i] < b->ls_stack[i])
			return (-1);
		if (a->ls_stack[i] > b->ls_stack[i])
			return (1);
	}

	if (a->ls_caller < b->ls_caller)
		return (-1);
	if (a->ls_caller > b->ls_caller)
		return (1);

	return (b->ls_count - a->ls_count);
}

static void
mergesort(int (*cmp)(lsrec_t *, lsrec_t *), lsrec_t **a, lsrec_t **b, int n)
{
	int m = n / 2;
	int i, j;

	if (m > 1)
		mergesort(cmp, a, b, m);
	if (n - m > 1)
		mergesort(cmp, a + m, b + m, n - m);
	for (i = m; i > 0; i--)
		b[i - 1] = a[i - 1];
	for (j = m - 1; j < n - 1; j++)
		b[n + m - j - 2] = a[j + 1];
	while (i < j)
		*a++ = cmp(b[i], b[j]) < 0 ? b[i++] : b[j--];
	*a = b[i];
}

static void
coalesce(int (*cmp)(lsrec_t *, lsrec_t *), lsrec_t **lock, int n)
{
	int i, j;
	lsrec_t *target, *current;

	target = lock[0];

	for (i = 1; i < n; i++) {
		current = lock[i];
		if (cmp(current, target) != 0) {
			target = current;
			continue;
		}
		current->ls_event = LS_MAX_EVENTS;
		target->ls_count += current->ls_count;
		target->ls_refcnt += current->ls_refcnt;
		if (lsctl.lc_recsize < LS_TIME)
			continue;
		target->ls_time += current->ls_time;
		if (lsctl.lc_recsize < LS_HIST)
			continue;
		for (j = 0; j < 64; j++)
			target->ls_hist[j] += current->ls_hist[j];
	}
}

int
main(int argc, char **argv)
{
	u_int data_buf_size;
	char *data_buf;
	lsrec_t *lsp, **current, **first, **sort_buf, **merge_buf;
	FILE *out = stdout;
	char c;
	pid_t child;
	int status;
	int i, fd;
	hrtime_t duration;
	char *addrp, *sizep, *evp, *lastp;
	uintptr_t addr;
	size_t size;
	int events_specified = 0;
	int tracing = 0;
	uint32_t event;

	/*
	 * Silently open and close the lockstat driver to get its
	 * symbols loaded
	 */
	(void) close(open("/dev/lockstat", O_RDONLY));

	if (symtab_init() == -1)
		fail(1, "can't load kernel symbols");

	lsctl.lc_recsize = LS_TIME;
	lsctl.lc_nlocks = DEFAULT_NLOCKS;

	/*
	 * Don't consider a pending event to be incomplete
	 * unless it's been around at least 1 second --
	 * otherwise the output will be mostly noise.
	 */
	lsctl.lc_min_duration[LS_EXIT_FAILED] = NANOSEC;

	while ((c = getopt(argc, argv, "bths:n:d:l:e:cwWCHEATD:RpPo:")) !=
	    EOF) {
		switch (c) {

		case 'b':
			lsctl.lc_recsize = LS_BASIC;
			break;

		case 't':
			lsctl.lc_recsize = LS_TIME;
			break;

		case 'h':
			lsctl.lc_recsize = LS_HIST;
			break;

		case 's':
			if (optarg == NULL || !isdigit(optarg[0]))
				usage();
			stk_depth = atoi(optarg);
			lsctl.lc_recsize = LS_STACK(stk_depth);
			break;

		case 'n':
			if (optarg == NULL || !isdigit(optarg[0]))
				usage();
			lsctl.lc_nlocks = atoi(optarg);
			break;

		case 'd':
			if (optarg == NULL || !isdigit(optarg[0]))
				usage();
			duration = atoll(optarg);

			/*
			 * XXX -- durations really should be per event
			 * since the units are different, but it's hard
			 * to express this nicely in the interface.
			 * Not clear yet what the cleanest solution is.
			 */
			for (i = 0; i < LS_MAX_EVENTS; i++)
				if (ls_event_info[i].ev_type != 'E')
					lsctl.lc_min_duration[i] = duration;

			break;

		case 'l':
			if (optarg == NULL)
				usage();

			addrp = strtok(optarg, ",");
			sizep = strtok(NULL, ",");

			size = sizep ? strtoul(sizep, NULL, 10) : 1;

			if (addrp[0] == '0') {
				addr = strtoul(addrp, NULL, 16);
			} else {
				addr = sym_to_addr(addrp);
				if (sizep == NULL)
					size = sym_size(addrp);
				if (addr == 0)
					fail(0, "symbol '%s' not found", addrp);
				if (size == 0)
					size = 1;
			}

			lsctl.lc_watch[watched_locks].lw_base = addr;
			lsctl.lc_watch[watched_locks].lw_size = size;

			if (++watched_locks > LS_MAX_WATCHED_LOCKS)
				fail(0, "too many watched locks");
			break;

		case 'e':
			if (optarg == NULL)
				usage();

			evp = strtok_r(optarg, ",", &lastp);
			while (evp) {
				int ev1, ev2;
				char *evp2;

				(void) strtok(evp, "-");
				evp2 = strtok(NULL, "-");
				ev1 = atoi(evp);
				ev2 = evp2 ? atoi(evp2) : ev1;
				if ((u_int)ev1 >= LS_MAX_EVENTS ||
				    (u_int)ev2 >= LS_MAX_EVENTS || ev1 > ev2)
					fail(0, "-e events out of range");
				for (i = ev1; i <= ev2; i++)
					lsctl.lc_event[i] |= LSE_RECORD;
				evp = strtok_r(NULL, ",", &lastp);
			}
			events_specified = 1;
			break;

		case 'c':
			cflag = 1;
			break;

		case 'w':
			wflag = 1;
			break;

		case 'W':
			Wflag = 1;
			break;

		case 'C':
		case 'H':
		case 'E':
		case 'A':
			for (i = 0; i < LS_MAX_EVENTS; i++)
				if (c == 'A' || ls_event_info[i].ev_type == c)
					lsctl.lc_event[i] |= LSE_RECORD;
			events_specified = 1;
			break;

		case 'T':
			for (i = 0; i < LS_MAX_EVENTS; i++)
				lsctl.lc_event[i] |= LSE_TRACE;
			tracing = 1;
			break;

		case 'D':
			if (optarg == NULL || !isdigit(optarg[0]))
				usage();
			top_n = atoi(optarg);
			break;

		case 'R':
			do_rates = 1;
			break;

		case 'p':
			pflag = 1;
			break;

		case 'P':
			Pflag = 1;
			break;

		case 'o':
			if (optarg == NULL)
				usage();
			if ((out = fopen(optarg, "w")) == NULL)
				fail(1, "error opening file");
			break;

		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	/*
	 * Make sure the alignment is reasonable
	 */
	lsctl.lc_recsize = (lsctl.lc_recsize + sizeof (uint64_t) - 1) &
		~(sizeof (uint64_t) - 1);

	if (wflag && lsctl.lc_recsize > LS_STACK(0))
		fail(0, "stack trace (-s %d) incompatible with -w", stk_depth);

	for (i = 0; i < LS_MAX_EVENTS; i++) {
		/*
		 * If no events were specified, enable everything except
		 * hold times.
		 */
		if (!events_specified && ls_event_info[i].ev_type != 'H')
			lsctl.lc_event[i] |= LSE_RECORD;

		/*
		 * For each enabled hold-time event, set LSE_ENTER (unless
		 * recording mode is LS_BASIC) and LSE_EXIT (regardless).
		 * If we're doing LS_BASIC stats we don't need LSE_ENTER
		 * events since we don't have to measure time intervals.
		 */
		if ((lsctl.lc_event[i] & LSE_RECORD) &&
		    ls_event_info[i].ev_type == 'H') {
			if (lsctl.lc_recsize != LS_BASIC)
				lsctl.lc_event[i] |= LSE_ENTER;
			lsctl.lc_event[i] |= LSE_EXIT;
		}
	}

	/*
	 * Make sure there is a child command to execute
	 */
	if (argc <= 0)
		usage();

	/*
	 * start the experiment
	 */
	if ((fd = open("/dev/lockstat", O_RDWR)) == -1)
		fail(1, "cannot open /dev/lockstat");

	data_buf_size = (lsctl.lc_nlocks + 1) * lsctl.lc_recsize;

	if ((data_buf = memalign(sizeof (uint64_t), data_buf_size)) == NULL)
		fail(1, "Memory allocation failed");

	if ((sort_buf = calloc(2 * (lsctl.lc_nlocks + 1),
	    sizeof (void *))) == NULL)
		fail(1, "Sort buffer allocation failed");
	merge_buf = sort_buf + (lsctl.lc_nlocks + 1);

	if (write(fd, &lsctl, sizeof (lsctl_t)) != sizeof (lsctl_t))
		fail(1, "Cannot start experiment");

	elapsed_time = -gethrtime();

	/*
	 * Spawn the specified command and wait for it to complete.
	 */
	child = fork();
	if (child == -1)
		fail(1, "cannot fork");
	if (child == 0) {
		close(fd);
		(void) execvp(argv[0], &argv[0]);
		perror(argv[0]);
		exit(errno == ENOENT ? 127 : 126);
	}
	while (wait(&status) != child)
		continue;

	elapsed_time += gethrtime();

	if ((status & 0377) != '\0')
		(void) fprintf(stderr,
		    "lockstat: warning: %s terminated abnormally\n", argv[0]);

	/*
	 * Gather the data sample
	 */
	lseek(fd, 0, SEEK_SET);
	if ((nlocks_used = read(fd, data_buf, data_buf_size)) == -1)
		fail(1, "Cannot read from lockstat driver");
	nlocks_used /= lsctl.lc_recsize;
	close(fd);

	if (nlocks_used >= lsctl.lc_nlocks)
		fprintf(stderr, "lockstat: warning: ran out of lock records\n");

	lsctl.lc_nlocks = nlocks_used;

	/*
	 * Build the sort buffer, discarding zero-count records along the way
	 */
	lsp = (lsrec_t *)data_buf;
	for (i = 0; i < nlocks_used; i++) {
		sort_buf[i] = lsp;
		if (lsp->ls_count == 0)
			lsp->ls_event = LS_MAX_EVENTS;
		lsp = (lsrec_t *)((char *)lsp + lsctl.lc_recsize);
	}

	/*
	 * Add a sentinel after the last record
	 */
	sort_buf[i] = lsp;
	lsp->ls_event = LS_MAX_EVENTS;

	if (tracing) {
		report_trace(out, sort_buf);
		return (0);
	}

	/*
	 * Coalesce locks within the same symbol if -c option specified
	 */
	if (cflag) {
		for (i = 0; i < nlocks_used; i++) {
			uintptr_t symoff;
			size_t symsize;
			lsp = sort_buf[i];
			(void) addr_to_sym(lsp->ls_lock, &symoff, &symsize);
			if (symoff < symsize) {
				/*
				 * symbol seems plausible
				 */
				lsp->ls_lock -= symoff;
			}
		}
		mergesort(lockcmp, sort_buf, merge_buf, nlocks_used);
		coalesce(lockcmp, sort_buf, nlocks_used);
	}

	/*
	 * Coalesce callers if -w option specified
	 */
	if (wflag) {
		mergesort(lock_and_count_cmp_anywhere,
		    sort_buf, merge_buf, nlocks_used);
		coalesce(lockcmp_anywhere, sort_buf, nlocks_used);
	}

	/*
	 * Coalesce locks if -W option specified
	 */
	if (Wflag) {
		mergesort(site_and_count_cmp_anylock,
		    sort_buf, merge_buf, nlocks_used);
		coalesce(sitecmp_anylock, sort_buf, nlocks_used);
	}

	/*
	 * Sort data by contention count (ls_count) or total time (ls_time),
	 * depending on Pflag.  Override Pflag if time wasn't measured.
	 */
	if (lsctl.lc_recsize < LS_TIME)
		Pflag = 0;

	if (Pflag)
		mergesort(timecmp, sort_buf, merge_buf, nlocks_used);
	else
		mergesort(countcmp, sort_buf, merge_buf, nlocks_used);

	/*
	 * Display data by event type
	 */
	first = &sort_buf[0];
	while ((event = (*first)->ls_event) < LS_MAX_EVENTS) {
		uint64_t total_count = 0;
		uint64_t total_time = 0;
		current = first;
		while ((lsp = *current)->ls_event == event) {
			total_count += lsp->ls_count;
			total_time += lsp->ls_time;
			current++;
		}
		report_stats(out, first, current - first, total_count,
		    total_time);
		first = current;
	}

	return (0);
}

static char *
format_symbol(char *buf, uintptr_t addr, int show_size)
{
	uintptr_t symoff;
	char *symname;
	size_t symsize;

	symname = addr_to_sym(addr, &symoff, &symsize);

	if (show_size && symoff == 0)
		(void) sprintf(buf, "%s[%d]", symname, symsize);
	else if (symoff == 0)
		(void) sprintf(buf, "%s", symname);
	else if (symoff <= 0x100 || symsize >= symoff)
		(void) sprintf(buf, "%s+0x%x", symname, symoff);
	else
		(void) sprintf(buf, "0x%x", addr);
	return (buf);
}

static void
report_stats(FILE *out, lsrec_t **sort_buf, int nlocks, uint64_t total_count,
	uint64_t total_time)
{
	uint32_t event = sort_buf[0]->ls_event;
	lsrec_t *lsp;
	double ptotal = 0.0;
	double percent;
	int i, j, fr;
	int displayed;
	int first_bin, last_bin, max_bin_count, total_bin_count;
	int rectype;
	char buf[256];

	rectype = lsctl.lc_recsize;
	if (event == LS_RECORD_FAILED)
		rectype = LS_BASIC;

	if (top_n == 0) {
		(void) fprintf(out, "%20llu %s\n",
		    do_rates == 0 ? total_count :
		    (u_int)(((uint64_t)total_count * NANOSEC) / elapsed_time),
		    ls_event_info[event].ev_desc);
		return;
	}

	if (!pflag)
		(void) fprintf(out, "\n\n%s: %llu events\n\n",
		    ls_event_info[event].ev_desc, total_count);

	if (!pflag && rectype < LS_HIST) {
		sprintf(buf, "%s", ls_event_info[event].ev_units);
		(void) fprintf(out, "%15s %4s %8s %-22s %-24s\n",
		    do_rates ? "ops/s indv cuml" : "Count indv cuml",
		    "rcnt", rectype >= LS_TIME ? buf : "",
		    Wflag ? "Hottest lock" : "Lock",
		    wflag ? "Hottest caller" : "Caller");
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");
	}

	displayed = 0;
	for (i = 0; i < nlocks; i++) {
		lsp = sort_buf[i];

		if (displayed++ >= top_n)
			break;

		if (pflag) {
			int j;

			(void) fprintf(out, "%u %u",
			    lsp->ls_event, lsp->ls_count);
			(void) fprintf(out, " %s",
			    format_symbol(buf, lsp->ls_lock, cflag));
			(void) fprintf(out, " %s",
			    format_symbol(buf, lsp->ls_caller, 0));
			(void) fprintf(out, " %f",
			    (double)lsp->ls_refcnt / lsp->ls_count);
			if (rectype >= LS_TIME)
				(void) fprintf(out, " %llu", lsp->ls_time);
			if (rectype >= LS_HIST) {
				for (j = 0; j < 64; j++)
					(void) fprintf(out, " %u",
					    lsp->ls_hist[j]);
			}
			for (j = 0; j < LS_MAX_STACK_DEPTH; j++) {
				if (rectype <= LS_STACK(j) ||
				    lsp->ls_stack[j] == 0)
					break;
				(void) fprintf(out, " %s",
				    format_symbol(buf, lsp->ls_stack[j], 0));
			}
			(void) fprintf(out, "\n");
			continue;
		}

		if (rectype >= LS_HIST) {
			(void) fprintf(out, "---------------------------------"
			    "----------------------------------------------\n");
			sprintf(buf, "%s", ls_event_info[event].ev_units);
			(void) fprintf(out, "%15s %4s %8s %-22s %-24s\n",
			    do_rates ? "ops/s indv cuml" : "Count indv cuml",
			    "rcnt", buf,
			    Wflag ? "Hottest lock" : "Lock",
			    wflag ? "Hottest caller" : "Caller");
		}

		if (Pflag && total_time != 0)
			percent = (lsp->ls_time * 100.00) / total_time;
		else
			percent = (lsp->ls_count * 100.00) / total_count;

		ptotal += percent;

		if (rectype >= LS_TIME)
			sprintf(buf, "%llu", lsp->ls_time / lsp->ls_count);
		else
			buf[0] = '\0';
		(void) fprintf(out, "%5u %3.0f%% %3.0f%% %4.2f %8s ",
		    do_rates == 0 ? lsp->ls_count :
		    (u_int)(((uint64_t)lsp->ls_count * NANOSEC) / elapsed_time),
		    percent, ptotal,
		    (double)lsp->ls_refcnt / lsp->ls_count, buf);

		(void) fprintf(out, "%-22s ",
		    format_symbol(buf, lsp->ls_lock, cflag));

		(void) fprintf(out, "%-24s\n",
		    format_symbol(buf, lsp->ls_caller, 0));

		if (rectype < LS_HIST)
			continue;

		(void) fprintf(out, "\n");
		(void) fprintf(out, "%10s %31s %-9s %-24s\n",
			ls_event_info[event].ev_units,
			"------ Time Distribution ------",
			do_rates ? "ops/s" : "count",
			rectype > LS_STACK(0) ? "Stack" : "");

		first_bin = 0;
		while (lsp->ls_hist[first_bin] == 0)
			first_bin++;

		last_bin = 63;
		while (lsp->ls_hist[last_bin] == 0)
			last_bin--;

		max_bin_count = 0;
		total_bin_count = 0;
		for (j = first_bin; j <= last_bin; j++) {
			total_bin_count += lsp->ls_hist[j];
			if (lsp->ls_hist[j] > max_bin_count)
				max_bin_count = lsp->ls_hist[j];
		}

		/*
		 * If we went a few frames below the caller, ignore them
		 */
		for (fr = 3; fr > 0; fr--)
			if (lsp->ls_stack[fr] == lsp->ls_caller)
				break;

		for (j = first_bin; j <= last_bin; j++) {
			u_int depth = (lsp->ls_hist[j] * 30) / total_bin_count;
			(void) fprintf(out, "%10llu |%s%s %-9u ",
			    1ULL << j,
			    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@" + 30 - depth,
			    "                              " + depth,
			    do_rates == 0 ? lsp->ls_hist[j] :
			    (u_int)(((uint64_t)lsp->ls_hist[j] * NANOSEC) /
			    elapsed_time));
			if (rectype <= LS_STACK(fr) || lsp->ls_stack[fr] == 0) {
				(void) fprintf(out, "\n");
				continue;
			}
			(void) fprintf(out, "%-24s\n",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
		while (rectype > LS_STACK(fr) && lsp->ls_stack[fr] != 0) {
			(void) fprintf(out, "%15s %-36s %-24s\n", "", "",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
	}

	if (!pflag)
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");

	(void) fflush(out);
}

static void
report_trace(FILE *out, lsrec_t **sort_buf)
{
	lsrec_t *lsp;
	int i, fr;
	int rectype;
	char buf[256], buf2[256];

	rectype = lsctl.lc_recsize;

	if (!pflag) {
		(void) fprintf(out, "%5s  %10s  %8s  %-24s  %-24s\n",
		    "Event", "Time", "Owner", "Lock", "Caller");
		(void) fprintf(out, "---------------------------------"
		    "----------------------------------------------\n");
	}

	for (i = 0; i < lsctl.lc_nlocks; i++) {

		lsp = sort_buf[i];

		if (lsp->ls_event >= LS_MAX_EVENTS || lsp->ls_count == 0)
			continue;

		fprintf(out, "%5d  %10llu  %8x  %-24s  %-24s\n",
		    lsp->ls_event, lsp->ls_time, lsp->ls_next,
		    format_symbol(buf, lsp->ls_lock, 0),
		    format_symbol(buf2, lsp->ls_caller, 0));

		if (rectype <= LS_STACK(0) || lsp->ls_event == LS_RECORD_FAILED)
			continue;

		/*
		 * If we went a few frames below the caller, ignore them
		 */
		for (fr = 3; fr > 0; fr--)
			if (lsp->ls_stack[fr] == lsp->ls_caller)
				break;

		while (rectype > LS_STACK(fr) && lsp->ls_stack[fr] != 0) {
			(void) fprintf(out, "%53s  %-24s\n", "",
			    format_symbol(buf, lsp->ls_stack[fr], 0));
			fr++;
		}
		fprintf(out, "\n");
	}

	(void) fflush(out);
}
