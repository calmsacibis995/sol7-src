/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 * rewritten from UCB 4.13 83/09/25
 * rewritten from SunOS 4.1 SID 1.18 89/10/06
 */

#pragma ident	"@(#)iostat.c	1.20	97/12/08 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <kstat.h>
#include <stropts.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>

kstat_ctl_t	*kc;		/* libkstat cookie */
static	int	ncpus;
static	kstat_t	**cpu_stat_list = NULL;

#define	DISK_OLD		0x0001
#define	DISK_NEW		0x0002
#define	DISK_EXTENDED		0x0004
#define	DISK_ERRORS		0x0008
#define	DISK_EXTENDED_ERRORS	0x0010
#define	DISK_NORMAL	(DISK_OLD | DISK_NEW)

#define	DISK_IO_MASK	(DISK_OLD | DISK_NEW | DISK_EXTENDED)
#define	DISK_ERROR_MASK	(DISK_ERRORS | DISK_EXTENDED_ERRORS)
#define	PRINT_VERTICAL	(DISK_ERROR_MASK | DISK_EXTENDED)

#define	REPRINT 19

/*
 * Name and print priority of each supported ks_class.
 */
#define	IO_CLASS_DISK		0
#define	IO_CLASS_PARTITION	0
#define	IO_CLASS_TAPE		1
#define	IO_CLASS_NFS		2

struct io_class {
	char	*class_name;
	int	class_priority;
};

static struct io_class io_class[] = {
	{ "disk",	IO_CLASS_DISK},
	{ "partition",	IO_CLASS_PARTITION},
	{ "tape",	IO_CLASS_TAPE},
	{ "nfs",	IO_CLASS_NFS},
	{ NULL,		0}
};

struct diskinfo {
	struct diskinfo *next;
	kstat_t *ks;
	kstat_io_t new_kios, old_kios;
	int	selected;
	int	class;
	char	*device_name;
	kstat_t	*disk_errs;	/* pointer to the disk's error kstats */
};

#define	DISK_GIGABYTE	1000000000.0

static void *dl = 0;	/* for device name lookup */
extern void *build_disk_list(void *);
extern char *lookup_ks_name(char *, void *);

extern char *lookup_nfs_name(char *);

#define	NULLDISK (struct diskinfo *)0
static	struct diskinfo zerodisk;
static	struct diskinfo *firstdisk = NULLDISK;
static	struct diskinfo *lastdisk = NULLDISK;
static	struct diskinfo *snip = NULLDISK;

static	cpu_stat_t	old_cpu_stat, new_cpu_stat;

#define	DISK_DELTA(x) (disk->new_kios.x - disk->old_kios.x)

#define	CPU_DELTA(x) (new_cpu_stat.x - old_cpu_stat.x)

#define	PRINT_TTY_DATA(sys, time) \
				(void) printf(" %3.0f %4.0f",\
				(float)CPU_DELTA(sys.rawch) / time, \
				(float)CPU_DELTA(sys.outch) / time);

#define	PRINT_CPU_DATA(sys, pcnt) \
				(void) printf(" %2.0f %2.0f %2.0f %2.0f", \
			CPU_DELTA(sys.cpu[CPU_USER]) * pcnt, \
			CPU_DELTA(sys.cpu[CPU_KERNEL]) * pcnt, \
			CPU_DELTA(sys.cpu[CPU_WAIT]) * pcnt, \
			CPU_DELTA(sys.cpu[CPU_IDLE]) * pcnt);

#define	PRINT_CPU_HDR1	(void) printf("%12s", "cpu");
#define	PRINT_CPU_HDR2	(void) printf(" us sy wt id");
#define	PRINT_TTY_HDR1	(void) printf("%9s", "tty");
#define	PRINT_TTY_HDR2	(void) printf(" tin tout");
#define	PRINT_ERR_HDR	(void) printf(" ---- errors --- ");

static	char	*cmdname = "iostat";

static	int	tohdr = 1;

static	int	do_tty = 0;
static	int	do_disk = 0;
static	int	do_cpu = 0;
static	int	do_interval = 0;
static	int	do_partitions = 0;	/* collect per-partition stats */
static	int	do_partitions_only = 0;	/* collect per-partition stats only */
					/* no per-device stats for disks */
static	int	do_conversions = 0;	/* display disks as cXtYdZ */
static	int	do_megabytes = 0;	/* display data in MB/sec */
static	char	disk_header[132];
#define	DEFAULT_LIMIT	4
static	int	limit = 0;		/* limit for drive display */
static	int	ndrives = 0;

struct disk_selection {
	struct disk_selection *next;
	char ks_name[KSTAT_STRLEN];
};

static	struct disk_selection *disk_selections = (struct disk_selection *)NULL;

static	void	printhdr(int);
static	void	printxhdr(void);
static	void	show_disk(struct diskinfo *);
static	void	cpu_stat_init(void);
static	int	cpu_stat_load(void);
static	void	usage(void);
static	void	fail(int, char *, ...);
static	void	safe_zalloc(void **, int, int);
static	void	init_disks(void);
static	void	select_disks(void);
static	int	diskinfo_load(void);
static	void 	init_disk_errors(void);
static	void 	show_disk_errors(struct diskinfo *);
static	void	find_disk(kstat_t *);

void
main(int argc, char **argv)
{
	int i, c;
	int iter = 0;
	int interval = 0, poll_interval = 0;	/* delay between display */
	char ch, iosz;
	struct diskinfo *disk;
	char	err_header[20];
	int errflg = 0;
	double	etime;		/* elapsed time */
	double	percent;	/* 100 / etime */
	int	hz;
	extern char *optarg;
	extern int optind;

	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");

	while ((c = getopt(argc, argv, "tdDxcIpPnMeEl:")) != EOF)
		switch (c) {
		case 't':
			do_tty++;
			break;
		case 'd':
			do_disk |= DISK_OLD;
			break;
		case 'D':
			do_disk |= DISK_NEW;
			break;
		case 'x':
			do_disk |= DISK_EXTENDED;
			break;
		case 'c':
			do_cpu++;
			break;
		case 'I':
			do_interval++;
			break;
		case 'p':
			do_partitions++;
			break;
		case 'P':
			do_partitions_only++;
			break;
		case 'n':
			do_conversions++;
			break;
		case 'M':
			do_megabytes++;
			break;
		case 'e':
			do_disk |= DISK_ERRORS;
			break;
		case 'E':
			do_disk |= DISK_EXTENDED_ERRORS;
			break;
		case 'l':
			limit = atoi(optarg);
			if (limit < 1)
				usage();
			break;
		case '?':
			errflg++;
		}

	if (errflg) {
		usage();
	}

	/* if no output classes explicity specified, use defaults */
	if (do_tty == 0 && do_disk == 0 && do_cpu == 0)
		do_tty = do_cpu = 1, do_disk = DISK_OLD;

	/*
	 * If conflicting options take the prefered
	 */
	if ((do_disk & DISK_EXTENDED) && (do_disk & DISK_NORMAL))
		do_disk &= ~DISK_NORMAL;
	if ((do_disk & DISK_NORMAL) && (do_disk & DISK_ERROR_MASK))
		do_disk &= ~DISK_ERROR_MASK;

	/*
	 * If limit == 0 then no command line limit was set, else if any of
	 * the flags that cause unlimited disks were not set,
	 * use the default of 4
	 */
	if (limit == 0) {
		if (!(do_disk & (DISK_EXTENDED |
					DISK_ERRORS | DISK_EXTENDED_ERRORS)))
			limit = DEFAULT_LIMIT;
	}

	ch = (do_interval ? 'i' : 's');
	iosz = (do_megabytes ? 'M' : 'k');
	if (do_disk & DISK_ERRORS)
		(void) sprintf(err_header, "s/w h/w trn tot ");
	else
		*err_header = '\0';
	switch (do_disk & DISK_IO_MASK) {
	    case DISK_OLD:
		(void) sprintf(disk_header, " %cp%c tp%c serv ", iosz, ch, ch);
		break;
	    case DISK_NEW:
		(void) sprintf(disk_header, " rp%c wp%c util ", ch, ch);
		break;
	    case DISK_EXTENDED:
		if (!do_conversions)
			(void) sprintf(disk_header, "device    "
			    "r/%c  w/%c   %cr/%c   %cw/%c "
			    "wait actv  svc_t  %%%%w  %%%%b %s",
			    ch, ch, iosz, ch, iosz, ch, err_header);
		else
			(void) sprintf(disk_header, "  "
			    "r/%c  w/%c   %cr/%c   %cw/%c "
			    "wait actv wsvc_t asvc_t  "
			    "%%%%w  %%%%b %sdevice",
			    ch, ch, iosz, ch, iosz, ch, err_header);
		break;
	    default:
		break;
	}
	if (do_disk == DISK_ERRORS) {
		if (!do_conversions)
			(void) sprintf(disk_header, "device    %s",
					err_header);
		else
			(void) sprintf(disk_header, "  %s", err_header);
	}

	hz = sysconf(_SC_CLK_TCK);
	cpu_stat_init();

	if (do_disk) {
		/*
		 * Choose drives to be displayed.  Priority
		 * goes to (in order) drives supplied as arguments,
		 * then any other active drives that fit.
		 */
		struct disk_selection **dsp = &disk_selections;
		while (optind < argc && !isdigit(argv[optind][0])) {
			safe_zalloc((void **)dsp, sizeof (**dsp), 0);
			(void) strncpy((*dsp)->ks_name, argv[optind],
						KSTAT_STRLEN - 1);
			dsp = &((*dsp)->next);
			optind++;
		}
		*dsp = (struct disk_selection *)NULL;
		init_disks();
	}

	if (optind < argc) {
		if ((interval = atoi(argv[optind])) <= 0)
			fail(0, "negative interval");
		poll_interval = 1000 * interval;
		optind++;
	}
	if (optind < argc) {
		if ((iter = atoi(argv[optind])) <= 0)
			fail(0, "negative count");
		optind++;
	}
	if (optind < argc)
		usage();

	if (!(do_disk & PRINT_VERTICAL))
		(void) signal(SIGCONT, printhdr);
loop:

	while (
		kstat_chain_update(kc) ||
			cpu_stat_load() || diskinfo_load()) {
		(void) printf("<<State change>>\n");
		cpu_stat_init();
		if (do_disk)
			init_disks();
		if (do_disk & PRINT_VERTICAL)
			printxhdr();
		else
			printhdr(0);
	}
	etime = 0.0;
	for (i = 0; i < CPU_STATES; i++)
		etime += CPU_DELTA(cpu_sysinfo.cpu[i]);

	percent = (etime > 0.0) ? 100.0 / etime : 0.0;
	etime = (etime / ncpus) / hz;
	if (etime == 0.0)
		etime = (double)interval;
	if (etime == 0.0)
		etime = 1.0;

	if (do_interval)
		etime = 1.0;

	if (do_conversions && do_disk & PRINT_VERTICAL) {
		if (do_tty)
			PRINT_TTY_HDR1;
		if (do_cpu)
			PRINT_CPU_HDR1;
		if (do_tty || do_cpu)
			(void) putchar('\n');
		if (do_tty)
			PRINT_TTY_HDR2;
		if (do_cpu)
			PRINT_CPU_HDR2;
		if (do_tty || do_cpu)
			(void) putchar('\n');
		if (do_tty)
			PRINT_TTY_DATA(cpu_sysinfo, etime);
		if (do_cpu)
			PRINT_CPU_DATA(cpu_sysinfo, percent);
		if (do_tty || do_cpu)
			(void) putchar('\n');
		printxhdr();
		for (disk = firstdisk; disk; disk = disk->next) {
			if (disk->selected) {
				show_disk(disk);
				(void) putchar('\n');
			}
		}
	} else {
		if (do_disk & PRINT_VERTICAL)
			printxhdr();
		else if (--tohdr == 0)
			printhdr(0);

		/* show data for the first disk */
		if (!do_conversions && do_disk & PRINT_VERTICAL)
			show_disk(firstdisk);

		if (do_tty)
			PRINT_TTY_DATA(cpu_sysinfo, etime);

		if (do_disk & DISK_NORMAL)
			for (disk = firstdisk; disk; disk = disk->next)
				if (disk->selected)
					show_disk(disk);

		if (do_cpu)
			PRINT_CPU_DATA(cpu_sysinfo, percent);

		(void) putchar('\n');
		/* show data for the rest of the disks */
		if (do_disk & PRINT_VERTICAL) {
			for (disk = firstdisk->next; disk; disk = disk->next) {
				if (disk->selected) {
					show_disk(disk);
					(void) putchar('\n');
				}
			}
		}
	}
	(void) fflush(stdout);

	/*
	 * Dump total error statistics
	 */
	if (do_disk & DISK_EXTENDED_ERRORS) {
		for (disk = firstdisk; disk; disk = disk->next) {
			if (disk->selected && disk->disk_errs) {
				show_disk_errors(disk);
			}
		}
	}

	if (--iter && interval > 0) {
		(void) poll(NULL, 0, poll_interval);
		goto loop;
	}

	exit(0);

	/* NOTREACHED */
}

/* ARGSUSED */
static void
printhdr(int sig)
{
	struct diskinfo *disk;

	/*
	 * Horizontal mode headers
	 *
	 * First line
	 */
	if (do_tty)
		PRINT_TTY_HDR1;

	if (do_disk & DISK_NORMAL) {

		for (disk = firstdisk; disk; disk = disk->next) {
			if (disk->selected) {
				if (disk->device_name)
					(void) printf(" %12.8s ",
							disk->device_name);
				else
					(void) printf(" %12.8s ",
							disk->ks->ks_name);
			}
		}
	}

	if (do_cpu)
		PRINT_CPU_HDR1;
	(void) putchar('\n');

	/*
	 * Second line
	 */
	if (do_tty)
		PRINT_TTY_HDR2;

	if (do_disk & DISK_NORMAL) {
		for (disk = firstdisk; disk; disk = disk->next)
			if (disk->selected)
				(void) printf(disk_header);
	}

	if (do_cpu)
		PRINT_CPU_HDR2;
	(void) putchar('\n');

	tohdr = REPRINT;
}

/*
 * In extended mode headers, we don't want to reprint the header on
 * signals, they are printed every time anyways.
 */
static void
printxhdr(void)
{
	/*
	 * Vertical mode headers
	 */
	if (do_disk & DISK_EXTENDED) {
		if (!do_conversions)
			(void) printf("%1s", " ");
		(void) printf("%56s", "extended device statistics");
	}

	if (do_disk & DISK_ERRORS) {
		if (!(do_disk & DISK_EXTENDED) && !do_conversions)
				(void) printf("%9s", " ");
		PRINT_ERR_HDR;
	}

	if (do_conversions) {
		(void) putchar('\n');
		if (do_disk & (DISK_EXTENDED | DISK_ERRORS))
			(void) printf(disk_header);
		(void) putchar('\n');
		return;
	}

	if (do_tty)
		PRINT_TTY_HDR1;
	if (do_cpu)
		PRINT_CPU_HDR1;

	(void) putchar('\n');

	if (do_disk & (DISK_EXTENDED | DISK_ERRORS))
		(void) printf(disk_header);

	if (do_tty)
		PRINT_TTY_HDR2;

	if (do_cpu)
		PRINT_CPU_HDR2;

	(void) putchar('\n');
}

static void
show_disk(struct diskinfo *disk)
{
	double rps, wps, tps, krps, kwps, kps, avw, avr, w_pct, r_pct;
	double wserv, rserv, serv;
	double iosize;	/* kb/sec or MB/sec */
	double etime, hr_etime;
	char *disk_name;

	kstat_named_t *knp;
	int i, toterrs;

	hr_etime = (double)DISK_DELTA(wlastupdate);
	if (hr_etime == 0.0)
		hr_etime = (double)NANOSEC;
	etime = hr_etime / (double)NANOSEC;

	rps	= (double)DISK_DELTA(reads) / etime;
		/* reads per second */

	wps	= (double)DISK_DELTA(writes) / etime;
		/* writes per second */

	tps	= rps + wps;
		/* transactions per second */

	/*
	 * report throughput as either kb/sec or MB/sec
	 */
	if (!do_megabytes) {
		iosize = 1024.0;
	} else {
		iosize = 1048576.0;
	}
	krps	= (double)DISK_DELTA(nread) / etime / iosize;
		/* block reads per second */

	kwps	= (double)DISK_DELTA(nwritten) / etime / iosize;
		/* blocks written per second */

	kps	= krps + kwps;
		/* blocks transferred per second */

	avw	= (double)DISK_DELTA(wlentime) / hr_etime;
		/* average number of transactions waiting */

	avr	= (double)DISK_DELTA(rlentime) / hr_etime;
		/* average number of transactions running */

	wserv	= tps > 0 ? (avw / tps) * 1000.0 : 0.0;
		/* average wait service time in milliseconds */

	rserv	= tps > 0 ? (avr / tps) * 1000.0 : 0.0;
		/* average run service time in milliseconds */

	serv	= tps > 0 ? ((avw + avr) / tps) * 1000.0 : 0.0;
		/* average service time in milliseconds */

	w_pct	= (double)DISK_DELTA(wtime) / hr_etime * 100.0;
		/* % of time there is a transaction waiting for service */

	r_pct	= (double)DISK_DELTA(rtime) / hr_etime * 100.0;
		/* % of time there is a transaction running */

	if (do_interval) {
		rps	*= etime;
		wps	*= etime;
		tps	*= etime;
		krps	*= etime;
		kwps	*= etime;
		kps	*= etime;
	}

	if (do_disk & (DISK_EXTENDED | DISK_ERRORS)) {
		if (disk->device_name != (char *)0)
			disk_name = disk->device_name;
		else
			disk_name = disk->ks->ks_name;
		if (!do_conversions)
			(void) printf("%-8.8s", disk_name);
	}
	switch (do_disk & DISK_IO_MASK) {
	    case DISK_OLD:
		(void) printf(" %3.0f %3.0f %4.0f ", kps, tps, serv);
		break;
	    case DISK_NEW:
		(void) printf(" %3.0f %3.0f %4.1f ", rps, wps, r_pct);
		break;
	    case DISK_EXTENDED:
		if (!do_conversions)
			(void) printf(" %4.1f %4.1f %6.1f %6.1f %4.1f %4.1f"
			    " %6.1f %3.0f %3.0f ",
			    rps, wps, krps, kwps, avw, avr,
			    serv, w_pct, r_pct);
		else
			(void) printf(" %4.1f %4.1f %6.1f %6.1f %4.1f %4.1f"
			    " %6.1f %6.1f %3.0f %3.0f ",
			    rps, wps, krps, kwps, avw, avr,
			    wserv, rserv, w_pct, r_pct);
		break;
	}
	if (do_disk & DISK_ERRORS) {
		if (!(do_disk & DISK_IO_MASK))
			(void) printf("  ");
		if (disk->disk_errs) {
			toterrs = 0;
			knp = KSTAT_NAMED_PTR(disk->disk_errs);
			for (i = 0; i < 3; i++) {
				switch (knp[i].data_type) {
					case KSTAT_DATA_ULONG:
						(void) printf("%3d ",
							(int)knp[i].value.ui32);
						toterrs += knp[i].value.ui32;
						break;
					case KSTAT_DATA_ULONGLONG:
						(void) printf("%3d ",
							(int)knp[i].value.ui64);
						toterrs += knp[i].value.ui64;
						break;
					default:
						break;
				}
			}
			(void) printf("%3d ", toterrs);
		} else {
			(void) printf("  0   0   0   0 ");
		}
	}
	if ((do_disk & (DISK_EXTENDED | DISK_ERRORS)) && do_conversions)
		(void) printf("%s", disk_name);
}

/*
 * Get list of cpu_stat KIDs for subsequent cpu_stat_load operations.
 */

static void
cpu_stat_init(void)
{
	kstat_t *ksp;

	ncpus = 0;
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next)
		if (strncmp(ksp->ks_name, "cpu_stat", 8) == 0)
			ncpus++;

	safe_zalloc((void **)&cpu_stat_list, ncpus * sizeof (kstat_t *), 1);

	ncpus = 0;
	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next)
		if (strncmp(ksp->ks_name, "cpu_stat", 8) == 0 &&
		    kstat_read(kc, ksp, NULL) != -1)
			cpu_stat_list[ncpus++] = ksp;

	if (ncpus == 0)
		fail(1, "can't find any cpu statistics");

	(void) memset(&new_cpu_stat, 0, sizeof (cpu_stat_t));
}

static int
cpu_stat_load(void)
{
	int i, j;
	cpu_stat_t cs;
	ulong *np, *tp;

	old_cpu_stat = new_cpu_stat;
	(void) memset(&new_cpu_stat, 0, sizeof (cpu_stat_t));

	/* Sum across all cpus */

	for (i = 0; i < ncpus; i++) {
		if (kstat_read(kc, cpu_stat_list[i], (void *)&cs) == -1)
			return (1);
		np = (ulong *)&new_cpu_stat.cpu_sysinfo;
		tp = (ulong *)&cs.cpu_sysinfo;
		for (j = 0; j < sizeof (cpu_sysinfo_t); j += sizeof (ulong_t))
			*np++ += *tp++;
		np = (ulong *)&new_cpu_stat.cpu_vminfo;
		tp = (ulong *)&cs.cpu_vminfo;
		for (j = 0; j < sizeof (cpu_vminfo_t); j += sizeof (ulong_t))
			*np++ += *tp++;
	}
	return (0);
}

static void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage: iostat [-tdDxcIpPeEnM] ");
	(void) fprintf(stderr,
	    " [-l n] [disk ...] [interval [count]]\n");
	(void) fprintf(stderr,
		"\t\t-t: 	display chars read/written to terminals\n");
	(void) fprintf(stderr,
		"\t\t-d: 	display disk Kb/sec, transfers/sec, avg. \n");
	(void) fprintf(stderr,
		"\t\t\tservice time in milliseconds  \n");
	(void) fprintf(stderr,
		"\t\t-D: 	display disk reads/sec, writes/sec, \n");
	(void) fprintf(stderr,
		"\t\t\tpercentage disk utilization \n");
	(void) fprintf(stderr,
		"\t\t-x: 	display extended disk statistics\n");
	(void) fprintf(stderr,
		"\t\t-c: 	report percentage of time system has spent\n");
	(void) fprintf(stderr,
		"\t\t\tin user/system/wait/idle mode\n");
	(void) fprintf(stderr,
		"\t\t-I: 	report the counts in each interval,\n");
	(void) fprintf(stderr,
		"\t\t\tinstead of rates, where applicable\n");
	(void) fprintf(stderr,
		"\t\t-p: 	report per-partition disk statistics\n");
	(void) fprintf(stderr,
		"\t\t-P: 	report per-partition disk statistics only,\n");
	(void) fprintf(stderr,
		"\t\t\tno per-device disk statistics\n");
	(void) fprintf(stderr,
		"\t\t-e: 	report device error summary statistics\n");
	(void) fprintf(stderr,
		"\t\t-E: 	report extended device error statistics\n");
	(void) fprintf(stderr,
		"\t\t-n: 	convert device names to cXdYtZ format\n");
	(void) fprintf(stderr,
		"\t\t-M: 	Display data throughput in MB/sec "
				"instead of Kb/sec\n");
	exit(1);
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	(void) fprintf(stderr, "%s: ", cmdname);
	(void) vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		(void) fprintf(stderr, ": %s", strerror(errno));
	(void) fprintf(stderr, "\n");
	exit(2);
}

static void
safe_zalloc(void **ptr, int size, int free_first)
{
	if (free_first && *ptr != NULL)
		free(*ptr);
	if ((*ptr = (void *)malloc(size)) == NULL)
		fail(1, "malloc failed");
	(void) memset(*ptr, 0, size);
}


/*
 * Sort based on ks_class, ks_module, ks_instance, ks_name
 */
static int
kscmp(struct diskinfo *ks1, struct diskinfo *ks2)
{
	int cmp;

	cmp = ks1->class - ks2->class;
	if (cmp != 0)
		return (cmp);

	cmp = strcmp(ks1->ks->ks_module, ks2->ks->ks_module);
	if (cmp != 0)
		return (cmp);
	cmp = ks1->ks->ks_instance - ks2->ks->ks_instance;
	if (cmp != 0)
		return (cmp);

	if (ks1->device_name && ks2->device_name)
		return (strcmp(ks1->device_name,  ks2->device_name));
	else
		return (strcmp(ks1->ks->ks_name, ks2->ks->ks_name));
}

static void
init_disks(void)
{
	struct diskinfo *disk, *prevdisk, *comp;
	kstat_t *ksp;

	if (do_conversions)
		dl = (void *)build_disk_list(dl);

	zerodisk.next = NULLDISK;
	disk = &zerodisk;

	/*
	 * Patch the snip in the diskinfo list (see below)
	 */
	if (snip)
		lastdisk->next = snip;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		int i;

		if (ksp->ks_type != KSTAT_TYPE_IO)
			continue;

		for (i = 0; io_class[i].class_name != NULL; i++) {
			if (strcmp(ksp->ks_class, io_class[i].class_name) == 0)
				break;
		}
		if (io_class[i].class_name == NULL)
			continue;

		if (do_partitions_only &&
			(strcmp(ksp->ks_class, "disk") == 0))
				continue;

		if (!do_partitions && !do_partitions_only &&
			(strcmp(ksp->ks_class, "partition") == 0))
				continue;

		prevdisk = disk;
		if (disk->next)
			disk = disk->next;
		else {
			safe_zalloc((void **)&disk->next,
				sizeof (struct diskinfo), 0);
			disk = disk->next;
			disk->next = NULLDISK;
		}
		disk->ks = ksp;
		(void) memset((void *)&disk->new_kios, 0, sizeof (kstat_io_t));
		disk->new_kios.wlastupdate = disk->ks->ks_crtime;
		disk->new_kios.rlastupdate = disk->ks->ks_crtime;
		if (do_conversions && dl) {
			if (strcmp(ksp->ks_class, "nfs") == 0)
				disk->device_name =
					lookup_nfs_name(ksp->ks_name);
			else
				disk->device_name =
					lookup_ks_name(ksp->ks_name, dl);
		} else {
			disk->device_name = (char *)0;
		}

		disk->disk_errs = (kstat_t *)NULL;
		disk->class = io_class[i].class_priority;

		/*
		 * Insertion sort on (ks_class, ks_module, ks_instance, ks_name)
		 */
		comp = &zerodisk;
		while (kscmp(disk, comp->next) > 0)
			comp = comp->next;
		if (prevdisk != comp) {
			prevdisk->next = disk->next;
			disk->next = comp->next;
			comp->next = disk;
			disk = prevdisk;
		}
	}
	/*
	 * Put a snip in the linked list of diskinfos.  The idea:
	 * If there was a state change such that now there are fewer
	 * disks, we snip the list and retain the tail, rather than
	 * freeing it.  At the next state change, we clip the tail back on.
	 * This prevents a lot of malloc/free activity, and it's simpler.
	 */
	lastdisk = disk;
	snip = disk->next;
	disk->next = NULLDISK;

	firstdisk = zerodisk.next;
	if (firstdisk == NULLDISK)
		fail(0, "No disks to measure");
	select_disks();

	if (do_disk & DISK_ERROR_MASK)
		init_disk_errors();
}

static void
select_disks(void)
{
	struct diskinfo *disk;
	struct disk_selection *ds;

	ndrives = 0;
	for (disk = firstdisk; disk; disk = disk->next) {
		disk->selected = 0;
		for (ds = disk_selections; ds; ds = ds->next) {
			if (strcmp(disk->ks->ks_name, ds->ks_name) == 0) {
				disk->selected = 1;
				ndrives++;
				break;
			}
		}
	}
	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected)
			continue;
		if (limit && ndrives >= limit)
			break;
		disk->selected = 1;
		ndrives++;
	}
}

static int
diskinfo_load(void)
{
	struct diskinfo *disk;

	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected) {
			disk->old_kios = disk->new_kios;
			if (kstat_read(kc, disk->ks,
			    (void *)&disk->new_kios) == -1)
				return (1);
			if (disk->disk_errs) {
				if (kstat_read(kc, disk->disk_errs, NULL) == -1)
					return (1);
			}
		}
	}
	return (0);
}
static void
init_disk_errors()
{
	kstat_t *ksp;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if ((ksp->ks_type == KSTAT_TYPE_NAMED) &&
			(strncmp(ksp->ks_class, "device_error", 12) == 0)) {
				find_disk(ksp);
			}
	}
}
static void
find_disk(ksp)
kstat_t	*ksp;
{
	struct diskinfo *disk;
	char	kstat_name[KSTAT_STRLEN];
	char	*dname = kstat_name;
	char	*ename = ksp->ks_name;

	while (*ename != ',') {
		*dname = *ename;
		dname++;
		ename++;
	}
	*dname = '\0';

	for (disk = firstdisk; disk; disk = disk->next) {
		if (disk->selected) {
			if (strcmp(disk->ks->ks_name, kstat_name) == 0) {
				disk->disk_errs = ksp;
				return;
			}
		}
	}

}
static void
show_disk_errors(disk)
struct diskinfo *disk;
{
	kstat_named_t *knp;
	int	i, col;
	char	*dev_name;

	if (disk->device_name != (char *)0)
		dev_name = disk->device_name;
	else
		dev_name = disk->ks->ks_name;
	if (do_conversions)
		(void) printf("%-16.16s", dev_name);
	else
		(void) printf("%-8.8s", dev_name);

	col = 0;
	knp = KSTAT_NAMED_PTR(disk->disk_errs);
	for (i = 0; i < disk->disk_errs->ks_ndata; i++) {
		col += strlen(knp[i].name);
		switch (knp[i].data_type) {
			case KSTAT_DATA_CHAR:
				(void) printf("%s: %-.16s ", knp[i].name,
					&knp[i].value.c[0]);
				col += strlen(&knp[i].value.c[0]);
				break;
			case KSTAT_DATA_ULONG:
				(void) printf("%s: %d ", knp[i].name,
					(int)knp[i].value.ui32);
				col += 4;
				break;
			case KSTAT_DATA_ULONGLONG:
				if (strcmp(knp[i].name, "Size") == 0) {
					(void) printf(
						"%s: %2.2fGB <%lld bytes>\n",
					knp[i].name,
					(float)knp[i].value.ui64 /
						DISK_GIGABYTE,
					knp[i].value.ui64);
					col = 0;
					break;
				}
				(void) printf("%s: %d ", knp[i].name,
					(int)knp[i].value.ui64);
				col += 4;
				break;
			}
		if ((col >= 62) || (i == 2)) {
			(void) printf("\n");
			col = 0;
		}
	}
	if (col > 0) {
		(void) printf("\n");
	}
	(void) printf("\n");
}
