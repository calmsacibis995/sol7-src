/*
 * Copyright (c) 1984, 1991, 1992 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)rstat_proc.c	1.18	97/04/03 SMI"

/*
 * rstat service:  built with rstat.x
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <nlist.h>
#include <fcntl.h>
#include <syslog.h>
#include <kstat.h>

#include <rpc/rpc.h>

#include <sys/socket.h>
#include <sys/cpuvar.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/sysmacros.h>

#include <net/if.h>
#include <inet/common.h>
#include <inet/mib2.h>

#include "rstat.h"
#include "rstat_v2.h"

/*
 *	system and cpu stats
 */
static	kstat_ctl_t	*kc;		/* libkstat cookie */
static	int	ncpus;
static	kstat_t	**cpu_stat_list = NULL;
static	kstat_t	*system_misc_ksp;
static	kstat_named_t *boot_time_knp;
static	kstat_named_t *avenrun_1min_knp, *avenrun_5min_knp, *avenrun_15min_knp;
static	int	hz;
static	struct	timeval btm;		/* boottime */

/*
 *	network interface stats
 */

typedef struct mib_item_s {
	struct mib_item_s	*next_item;
	long			group;
	long			mib_id;
	long			length;
	char			*valp;
} mib_item_t;

mib_item_t	*netstat_item;

/*
 * disk stats
 */

struct diskinfo {
	struct diskinfo *next;
	kstat_t *ks;
	kstat_io_t kios;
};

#define	NULLDISK (struct diskinfo *)0
static	struct diskinfo zerodisk;
static	struct diskinfo *firstdisk = NULLDISK;
static	struct diskinfo *lastdisk = NULLDISK;
static	struct diskinfo *snip = NULLDISK;
static	int ndisks;

/*
 * net stats
 */

struct netinfo {
	struct netinfo *next;
	kstat_t	*ks;
	kstat_named_t *ipackets;
	kstat_named_t *opackets;
	kstat_named_t *ierrors;
	kstat_named_t *oerrors;
	kstat_named_t *collisions;
};

#define	NULLNET (struct netinfo *)0
static	struct netinfo zeronet;
static	struct netinfo *firstnet = NULLNET;
static	struct netinfo *lastnet = NULLNET;
static	struct netinfo *netsnip = NULLNET;
static	int nnets;

/*
 *  Define EXIT_WHEN_IDLE if you are able to have this program invoked
 *  automatically on demand (as from inetd).  When defined, the service
 *  will terminated after being idle for 120 seconds.
 */

#define	EXIT_WHEN_IDLE	1

int sincelastreq = 0;		/* number of alarms since last request */
#ifdef EXIT_WHEN_IDLE
#define	CLOSEDOWN 120		/* how long to wait before exiting */
#endif /* def EXIT_WHEN_IDLE */

statstime stats_s3;
statsvar stats_s4;
/* V2 support for backwards compatibility to pre-5.0 systems */
statsswtch stats_s2;

static stat_is_init = 0;
extern int errno;

static	void	fail(int, char *, ...);
static	void	safe_zalloc(void **, int, int);
static	kid_t	safe_kstat_read(kstat_ctl_t *, kstat_t *, void *);
static	kstat_t	*safe_kstat_lookup(kstat_ctl_t *, char *, int, char *);
static	void	*safe_kstat_data_lookup(kstat_t *, char *);
static	void	system_stat_init(void);
static	int	system_stat_load(void);
static	void	init_disks(void);
static	int	diskinfo_load(void);
static	void	init_net(void);
static	int	netinfo_load(void);

static	void	updatestat(int);

static	mib_item_t	*mibget(int sd);
static	int	mibopen(void);
static  char	*octetstr(char *buf, Octet_t *op, int code);

static	char	*cmdname = "rpc.rstatd";

static	cpu_sysinfo_t	cpu_sysinfo_all;
static	cpu_vminfo_t	cpu_vminfo_all;

static void
stat_init(void)
{
	stat_is_init = 1;

	if ((kc = kstat_open()) == NULL)
		fail(1, "kstat_open(): can't open /dev/kstat");

	/*
	 * Preallocate minimal set of drive entries.
	 */

	if (stats_s4.dk_xfer.dk_xfer_val == NULL) {
		stats_s4.dk_xfer.dk_xfer_len = RSTAT_DK_NDRIVE;
		stats_s4.dk_xfer.dk_xfer_val =
		    (int *)calloc(RSTAT_DK_NDRIVE, sizeof (int));
	}

	system_stat_init();
	init_disks();
	init_net();

	btm.tv_sec = boot_time_knp->value.ul;
	btm.tv_usec = 0;	/* don't bother with usecs for boot time */
	stats_s4.boottime.tv_sec =
		stats_s2.boottime.tv_sec =
		stats_s3.boottime.tv_sec = btm.tv_sec;
	stats_s4.boottime.tv_usec =
		stats_s2.boottime.tv_usec =
		stats_s3.boottime.tv_usec = btm.tv_usec;

	updatestat(0);
	alarm(1);
	signal(SIGALRM, updatestat);
	sleep(2);		/* allow for one wake-up */
}

statsvar *
rstatproc_stats_4_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	if (! stat_is_init)
		stat_init();
#ifdef EXIT_WHEN_IDLE
	sincelastreq = 0;
#endif
	return (&stats_s4);
}

statstime *
rstatproc_stats_3_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	if (! stat_is_init)
		stat_init();
#ifdef EXIT_WHEN_IDLE
	sincelastreq = 0;
#endif
	return (&stats_s3);
}

statsswtch *
rstatproc_stats_2_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	if (! stat_is_init)
		stat_init();
#ifdef EXIT_WHEN_IDLE
	sincelastreq = 0;
#endif
	return (&stats_s2);
}


u_int *
rstatproc_havedisk_4_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	return (rstatproc_havedisk_3_svc(argp, svcrq));
}

u_int *
rstatproc_havedisk_3_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	static u_int have;

	if (! stat_is_init)
		stat_init();
#ifdef EXIT_WHEN_IDLE
	sincelastreq = 0;
#endif
	have = (ndisks != 0);
	return (&have);
}

u_int *
rstatproc_havedisk_2_svc(argp, svcrq)
void *argp;
struct svc_req *svcrq;
{
	return (rstatproc_havedisk_3_svc(argp, svcrq));
}

void
updatestat(int ignored)
{
extern int _rpcpmstart;		 /* Started by a port monitor ? */
extern int _rpcsvcdirty;	 /* Still serving ? */

#ifdef DEBUG
	fprintf(stderr, "entering updatestat\n");
#endif
#ifdef EXIT_WHEN_IDLE
	if (_rpcpmstart && sincelastreq >= CLOSEDOWN && !_rpcsvcdirty) {
#ifdef DEBUG
		fprintf(stderr, "about to closedown\n");
#endif
		exit(0);
	}
	sincelastreq++;
#endif /* def EXIT_WHEN_IDLE */

	(void) alarm(0);
#ifdef DEBUG
	fprintf(stderr, "boottime: %d %d\n", stats_s3.boottime.tv_sec,
		stats_s3.boottime.tv_usec);
#endif
	while (kstat_chain_update(kc) ||
	    system_stat_load() || diskinfo_load() || netinfo_load()) {
		system_stat_init();
		init_disks();
		init_net();
	}
	stats_s4.cp_time.cp_time_len = CPU_STATES;
	if (stats_s4.cp_time.cp_time_val == NULL)
		stats_s4.cp_time.cp_time_val =
		(int *)malloc(stats_s4.cp_time.cp_time_len * sizeof (int));
	stats_s2.cp_time[RSTAT_CPU_USER] =
	stats_s3.cp_time[RSTAT_CPU_USER] =
	stats_s4.cp_time.cp_time_val[RSTAT_CPU_USER] =
		cpu_sysinfo_all.cpu[CPU_USER];
	stats_s2.cp_time[RSTAT_CPU_NICE] =
	stats_s3.cp_time[RSTAT_CPU_NICE] =
	stats_s4.cp_time.cp_time_val[RSTAT_CPU_NICE] =
		cpu_sysinfo_all.cpu[CPU_WAIT];
	stats_s2.cp_time[RSTAT_CPU_SYS] =
	stats_s3.cp_time[RSTAT_CPU_SYS] =
	stats_s4.cp_time.cp_time_val[RSTAT_CPU_SYS] =
		cpu_sysinfo_all.cpu[CPU_KERNEL];
	stats_s2.cp_time[RSTAT_CPU_IDLE] =
	stats_s3.cp_time[RSTAT_CPU_IDLE] =
	stats_s4.cp_time.cp_time_val[RSTAT_CPU_IDLE] =
		cpu_sysinfo_all.cpu[CPU_IDLE];

#ifdef DEBUG
	fprintf(stderr, "cpu: %d %d %d %d\n", cpu_sysinfo_all.cpu[CPU_USER],
		cpu_sysinfo_all.cpu[CPU_WAIT], cpu_sysinfo_all.cpu[CPU_KERNEL],
		cpu_sysinfo_all.cpu[CPU_IDLE]);
	fprintf(stderr, "cp_time: %d %d %d %d\n",
		stats_s3.cp_time[RSTAT_CPU_USER],
		stats_s3.cp_time[RSTAT_CPU_NICE],
		stats_s3.cp_time[RSTAT_CPU_SYS],
		stats_s3.cp_time[RSTAT_CPU_IDLE]);
#endif

	/* current time */
	gettimeofday((struct timeval *)&stats_s3.curtime, NULL);
	stats_s4.curtime = stats_s3.curtime;

	stats_s2.v_pgpgin =
	stats_s3.v_pgpgin =
	stats_s4.v_pgpgin = cpu_vminfo_all.pgpgin;
	stats_s2.v_pgpgout =
	stats_s3.v_pgpgout =
	stats_s4.v_pgpgout = cpu_vminfo_all.pgpgout;
	stats_s2.v_pswpin =
	stats_s3.v_pswpin =
	stats_s4.v_pswpin = cpu_vminfo_all.pgswapin;
	stats_s2.v_pswpout =
	stats_s3.v_pswpout =
	stats_s4.v_pswpout = cpu_vminfo_all.pgswapout;
	stats_s3.v_intr = cpu_sysinfo_all.intr;
	stats_s3.v_intr -= hz*(stats_s3.curtime.tv_sec - btm.tv_sec) +
		hz*(stats_s3.curtime.tv_usec - btm.tv_usec)/1000000;
	stats_s2.v_intr =
	stats_s4.v_intr = stats_s3.v_intr;
	/* swtch not in V1 */
	stats_s2.v_swtch =
	stats_s3.v_swtch =
	stats_s4.v_swtch = cpu_sysinfo_all.pswitch;

#ifdef DEBUG
	fprintf(stderr,
		"pgin: %d pgout: %d swpin: %d swpout: %d intr: %d swtch: %d\n",
		stats_s3.v_pgpgin,
		stats_s3.v_pgpgout,
		stats_s3.v_pswpin,
		stats_s3.v_pswpout,
		stats_s3.v_intr,
		stats_s3.v_swtch);
#endif
	/*
	 * V2 and V3 of rstat are limited to RSTAT_DK_NDRIVE drives
	 */
	memcpy(stats_s3.dk_xfer, stats_s4.dk_xfer.dk_xfer_val,
		RSTAT_DK_NDRIVE * sizeof (int));
	memcpy(stats_s2.dk_xfer, stats_s4.dk_xfer.dk_xfer_val,
		RSTAT_DK_NDRIVE * sizeof (int));
#ifdef DEBUG
	fprintf(stderr, "dk_xfer: %d %d %d %d\n",
		stats_s4.dk_xfer.dk_xfer_val[0],
		stats_s4.dk_xfer.dk_xfer_val[1],
		stats_s4.dk_xfer.dk_xfer_val[2],
		stats_s4.dk_xfer.dk_xfer_val[3]);
#endif

	stats_s2.if_ipackets =
	stats_s3.if_ipackets = stats_s4.if_ipackets;
	/* no s2 opackets */
	stats_s3.if_opackets = stats_s4.if_opackets;
	stats_s2.if_ierrors =
	stats_s3.if_ierrors = stats_s4.if_ierrors;
	stats_s2.if_oerrors =
	stats_s3.if_oerrors = stats_s4.if_oerrors;
	stats_s2.if_collisions =
	stats_s3.if_collisions = stats_s4.if_collisions;

	stats_s2.avenrun[0] =
	stats_s3.avenrun[0] =
	stats_s4.avenrun[0] = avenrun_1min_knp->value.ul;
	stats_s2.avenrun[1] =
	stats_s3.avenrun[1] =
	stats_s4.avenrun[1] = avenrun_5min_knp->value.ul;
	stats_s2.avenrun[2] =
	stats_s3.avenrun[2] =
	stats_s4.avenrun[2] = avenrun_15min_knp->value.ul;
#ifdef DEBUG
	fprintf(stderr, "avenrun: %d %d %d\n", stats_s3.avenrun[0],
		stats_s3.avenrun[1], stats_s3.avenrun[2]);
#endif
	signal(SIGALRM, updatestat);
	alarm(1);
}

/* --------------------------------- MIBGET -------------------------------- */

static mib_item_t *
mibget(int sd)
{
	int			flags;
	int			j, getcode;
	struct strbuf		ctlbuf, databuf;
	char			buf[512];
	struct T_optmgmt_req	*tor = (struct T_optmgmt_req *)buf;
	struct T_optmgmt_ack	*toa = (struct T_optmgmt_ack *)buf;
	struct T_error_ack	*tea = (struct T_error_ack *)buf;
	struct opthdr		*req;
	mib_item_t		*first_item = nilp(mib_item_t);
	mib_item_t		*last_item  = nilp(mib_item_t);
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
	if (putmsg(sd, &ctlbuf, nilp(struct strbuf), flags) == -1) {
		perror("mibget: putmsg(ctl) failed");
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
	/*CSTYLED*/
	for (j = 1; ; j++) {
		flags = 0;
		getcode = getmsg(sd, &ctlbuf, nilp(struct strbuf), &flags);
		if (getcode == -1) {
#ifdef DEBUG_MIB
			perror("mibget getmsg(ctl) failed");
			fprintf(stderr, "#   level   name    len\n");
			i = 0;
			for (last_item = first_item; last_item;
				last_item = last_item->next_item)
				fprintf(stderr, "%d  %4d   %5d   %d\n", ++i,
					last_item->group,
					last_item->mib_id,
					last_item->length);
#endif /* DEBUG_MIB */
			goto error_exit;
		}
		if (getcode == 0 &&
			(ctlbuf.len >= sizeof (struct T_optmgmt_ack)) &&
			(toa->PRIM_type == T_OPTMGMT_ACK) &&
			(toa->MGMT_flags == T_SUCCESS) &&
			req->len == 0) {
#ifdef DEBUG_MIB
			fprintf(stderr,
		"mibget getmsg() %d returned EOD (level %d, name %d)\n",
				j, req->level, req->name);
#endif /* DEBUG_MIB */
			return (first_item);		/* this is EOD msg */
		}

		if (ctlbuf.len >= sizeof (struct T_error_ack) &&
			(tea->PRIM_type == T_ERROR_ACK)) {
#ifdef DEBUG_MIB
			fprintf(stderr,
	"mibget %d gives T_ERROR_ACK: TLI_error = 0x%x, UNIX_error = 0x%x\n",
				j, getcode, tea->TLI_error, tea->UNIX_error);
#endif /* DEBUG_MIB */
			errno = (tea->TLI_error == TSYSERR)
				? tea->UNIX_error : EPROTO;
			goto error_exit;
		}

		if (getcode != MOREDATA ||
			(ctlbuf.len < sizeof (struct T_optmgmt_ack)) ||
			(toa->PRIM_type != T_OPTMGMT_ACK) ||
			(toa->MGMT_flags != T_SUCCESS)) {
#ifdef DEBUG_MIB
			fprintf(stderr,
	"mibget getmsg(ctl) %d returned %d, ctlbuf.len = %d, PRIM_type = %d\n",
				j, getcode, ctlbuf.len, toa->PRIM_type);
			if (toa->PRIM_type == T_OPTMGMT_ACK)
				fprintf(stderr,
	"T_OPTMGMT_ACK: MGMT_flags = 0x%x, req->len = %d\n",
					toa->MGMT_flags, req->len);
#endif /* DEBUG_MIB */
			errno = ENOMSG;
			goto error_exit;
		}

		temp = (mib_item_t *)malloc(sizeof (mib_item_t));
		if (!temp) {
			perror("mibget malloc failed");
			goto error_exit;
		}
		if (last_item)
			last_item->next_item = temp;
		else
			first_item = temp;
		last_item = temp;
		last_item->next_item = nilp(mib_item_t);
		last_item->group = req->level;
		last_item->mib_id = req->name;
		last_item->length = req->len;
		last_item->valp = (char *)malloc(req->len);
#ifdef DEBUG_MIB
		fprintf(stderr,
			"msg %d:  group = %4d   mib_id = %5d   length = %d\n",
			j, last_item->group, last_item->mib_id,
			last_item->length);
#endif /* DEBUG_MIB */
		databuf.maxlen = last_item->length;
		databuf.buf    = last_item->valp;
		databuf.len    = 0;
		flags = 0;
		getcode = getmsg(sd, nilp(struct strbuf), &databuf, &flags);
		if (getcode == -1) {
			perror("mibget getmsg(data) failed");
			goto error_exit;
		} else if (getcode != 0) {
			fprintf(stderr,
"mibget getmsg(data) returned %d, databuf.maxlen = %d, databuf.len = %d\n",
				getcode, databuf.maxlen, databuf.len);
			goto error_exit;
		}
	}

error_exit:
	while (first_item) {
		last_item = first_item;
		first_item = first_item->next_item;
		if (last_item->valp) {
			free(last_item->valp);
		}
		free(last_item);
	}
	return (first_item);
}

static int
mibopen(void)
{
	int	sd;

	sd = stream_open("/dev/ip", 2);
	if (sd == -1) {
		perror("ip open");
		stream_close(sd);
		return (-1);
	}
	if (stream_ioctl(sd, I_PUSH, "arp") == -1) {
		perror("arp I_PUSH");	/* must be above ip, below tcp */
		stream_close(sd);
		return (-1);
	}
	if (stream_ioctl(sd, I_PUSH, "tcp") == -1) {
		perror("tcp I_PUSH");
		stream_close(sd);
		return (-1);
	}
	if (stream_ioctl(sd, I_PUSH, "udp") == -1) {
		perror("udp I_PUSH");
		stream_close(sd);
		return (-1);
	}
	return (sd);
}

static char *
octetstr(char *buf, Octet_t *op, int code)
{
	int	i;
	char	*cp;

	cp = buf;
	if (op)
		for (i = 0; i < op->o_length; i++)
			switch (code) {
			case 'd':
				sprintf(cp, "%d.", 0xff & op->o_bytes[i]);
				cp = strchr(cp, '\0');
				break;
			case 'a':
				*cp++ = op->o_bytes[i];
				break;
			case 'h':
			default:
				sprintf(cp, "%02x:", 0xff & op->o_bytes[i]);
				cp += 3;
				break;
			}
	if (code != 'a' && cp != buf)
		cp--;
	*cp = '\0';
	return (buf);
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	fprintf(stderr, "%s: ", cmdname);
	vfprintf(stderr, message, args);
	va_end(args);
	if (do_perror)
		fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");
	exit(2);
}

static void
safe_zalloc(void **ptr, int size, int free_first)
{
	if (free_first && *ptr != NULL)
		free(*ptr);
	if ((*ptr = (void *)malloc(size)) == NULL)
		fail(1, "malloc failed");
	memset(*ptr, 0, size);
}

kid_t
safe_kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
	kid_t kstat_chain_id = kstat_read(kc, ksp, data);

	if (kstat_chain_id == -1)
		fail(1, "kstat_read(%x, '%s') failed", kc, ksp->ks_name);
	return (kstat_chain_id);
}

kstat_t *
safe_kstat_lookup(kstat_ctl_t *kc, char *ks_module, int ks_instance,
	char *ks_name)
{
	kstat_t *ksp = kstat_lookup(kc, ks_module, ks_instance, ks_name);

	if (ksp == NULL)
		fail(0, "kstat_lookup('%s', %d, '%s') failed",
			ks_module == NULL ? "" : ks_module,
			ks_instance,
			ks_name == NULL ? "" : ks_name);
	return (ksp);
}

void *
safe_kstat_data_lookup(kstat_t *ksp, char *name)
{
	void *fp = kstat_data_lookup(ksp, name);

	if (fp == NULL)
		fail(0, "kstat_data_lookup('%s', '%s') failed",
			ksp->ks_name, name);
	return (fp);
}

/*
 * Get various KIDs for subsequent system_stat_load operations.
 */

static void
system_stat_init(void)
{
	kstat_t *ksp;

	/*
	 * Global statistics
	 */

	system_misc_ksp	= safe_kstat_lookup(kc, "unix", 0, "system_misc");

	safe_kstat_read(kc, system_misc_ksp, NULL);
	boot_time_knp = safe_kstat_data_lookup(system_misc_ksp, "boot_time");
	avenrun_1min_knp = safe_kstat_data_lookup(system_misc_ksp,
		"avenrun_1min");
	avenrun_5min_knp = safe_kstat_data_lookup(system_misc_ksp,
		"avenrun_5min");
	avenrun_15min_knp = safe_kstat_data_lookup(system_misc_ksp,
		"avenrun_15min");

	/*
	 * Per-CPU statistics
	 */

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
}

/*
 * load statistics, summing across CPUs where needed
 */

static int
system_stat_load(void)
{
	int i, j;
	cpu_stat_t cs;
	ulong *np, *tp;

	memset((void *)&cpu_sysinfo_all, 0, sizeof (cpu_sysinfo_all));
	memset((void *)&cpu_vminfo_all, 0, sizeof (cpu_vminfo_all));

	/*
	 * Global statistics
	 */

	safe_kstat_read(kc, system_misc_ksp, NULL);

	/*
	 * Per-CPU statistics.
	 */

	for (i = 0; i < ncpus; i++) {
		if (kstat_read(kc, cpu_stat_list[i], (void *)&cs) == -1)
			return (1);
		np = (ulong *)&cpu_sysinfo_all;
		tp = (ulong *)&cs.cpu_sysinfo;
		for (j = 0; j < sizeof (cpu_sysinfo_t); j += sizeof (ulong_t))
			*np++ += *tp++;
		np = (ulong *)&cpu_vminfo_all;
		tp = (ulong *)&cs.cpu_vminfo;
		for (j = 0; j < sizeof (cpu_vminfo_t); j += sizeof (ulong_t))
			*np++ += *tp++;
	}
	return (0);
}

static int
kscmp(kstat_t *ks1, kstat_t *ks2)
{
	int cmp;

	cmp = strcmp(ks1->ks_module, ks2->ks_module);
	if (cmp != 0)
		return (cmp);
	cmp = ks1->ks_instance - ks2->ks_instance;
	if (cmp != 0)
		return (cmp);
	return (strcmp(ks1->ks_name, ks2->ks_name));
}

static void
init_disks(void)
{
	struct diskinfo *disk, *prevdisk, *comp;
	kstat_t *ksp;

	ndisks = 0;
	zerodisk.next = NULLDISK;
	disk = &zerodisk;

	/*
	 * Patch the snip in the diskinfo list (see below)
	 */
	if (snip)
		lastdisk->next = snip;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {

		if (ksp->ks_type != KSTAT_TYPE_IO ||
		    strcmp(ksp->ks_class, "disk") != 0)
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
		memset((void *)&disk->kios, 0, sizeof (kstat_io_t));
		disk->kios.wlastupdate = disk->ks->ks_crtime;
		disk->kios.rlastupdate = disk->ks->ks_crtime;

		/*
		 * Insertion sort on (ks_module, ks_instance, ks_name)
		 */
		comp = &zerodisk;
		while (kscmp(disk->ks, comp->next->ks) > 0)
			comp = comp->next;
		if (prevdisk != comp) {
			prevdisk->next = disk->next;
			disk->next = comp->next;
			comp->next = disk;
			disk = prevdisk;
		}
		ndisks++;
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

	if (ndisks > stats_s4.dk_xfer.dk_xfer_len) {
		stats_s4.dk_xfer.dk_xfer_len = ndisks;
		safe_zalloc((void **)&stats_s4.dk_xfer.dk_xfer_val,
			ndisks * sizeof (int), 1);
	}
}

static int
diskinfo_load(void)
{
	struct diskinfo *disk;
	int i;

	for (disk = firstdisk, i = 0; disk; disk = disk->next, i++) {
		if (kstat_read(kc, disk->ks, (void *)&disk->kios) == -1)
			return (1);
		stats_s4.dk_xfer.dk_xfer_val[i] = disk->kios.reads +
			disk->kios.writes;
	}
	return (0);
}

static void
init_net(void)
{
	static int sd;
	mib_item_t *item;
	mib2_ipAddrEntry_t *ap;
	char namebuf[KSTAT_STRLEN];
	struct netinfo *net, *prevnet, *comp;
	kstat_t *ksp;

	if (sd) {
		stream_close(sd);
	}
	while (netstat_item) {
		item = netstat_item;
		netstat_item = netstat_item->next_item;
		if (item->valp) {
			free(item->valp);
		}
		free(item);
	}
	sd = mibopen();
	if (sd == -1) {
#ifdef DEBUG
		fprintf(stderr, "mibopen() failed\n");
#endif
		sd = 0;
	} else {
		if ((netstat_item = mibget(sd)) == nilp(mib_item_t)) {
#ifdef DEBUG
			fprintf(stderr, "mibget() failed\n");
#endif
			stream_close(sd);
			sd = 0;
		}
	}
#ifdef DEBUG
	fprintf(stderr, "mibget returned item: %x\n", netstat_item);
#endif

	nnets = 0;
	zeronet.next = NULLNET;
	net = &zeronet;

	if (netsnip)
		lastnet->next = netsnip;

	for (item = netstat_item; item; item = item->next_item) {
#ifdef DEBUG_MIB
		fprintf(stderr, "\n--- Item %x ---\n", item);
		fprintf(stderr,
		"Group = %d, mib_id = %d, length = %d, valp = 0x%x\n",
		item->group, item->mib_id, item->length,
		item->valp);
#endif
		if (item->group != MIB2_IP || item->mib_id != MIB2_IP_20)
			continue;
		ap = (mib2_ipAddrEntry_t *)item->valp;
		for (; (char *)ap < item->valp + item->length; ap++) {

			octetstr(namebuf, &ap->ipAdEntIfIndex, 'a');
#ifdef DEBUG
			fprintf(stderr, "%s ", namebuf);
#endif
			if (strlen(namebuf) == 0)
				continue;
			/*
			 * We found a device of interest.
			 * Now, let's see if there's a kstat for it.
			 */
			if ((ksp = kstat_lookup(kc, NULL, -1, namebuf)) == NULL)
				continue;
			if (ksp->ks_type != KSTAT_TYPE_NAMED)
				continue;
			if (kstat_read(kc, ksp, NULL) == -1)
				continue;
			prevnet = net;
			if (net->next)
				net = net->next;
			else {
				safe_zalloc((void **)&net->next,
					sizeof (struct netinfo), 0);
				net = net->next;
				net->next = NULLNET;
			}
			net->ks = ksp;
			net->ipackets	= kstat_data_lookup(net->ks,
				"ipackets");
			net->opackets	= kstat_data_lookup(net->ks,
				"opackets");
			net->ierrors	= kstat_data_lookup(net->ks,
				"ierrors");
			net->oerrors	= kstat_data_lookup(net->ks,
				"oerrors");
			net->collisions	= kstat_data_lookup(net->ks,
				"collisions");
			/*
			 * Insertion sort on the name
			 */
			comp = &zeronet;
			while (strcmp(net->ks->ks_name,
			    comp->next->ks->ks_name) > 0)
				comp = comp->next;
			if (prevnet != comp) {
				prevnet->next = net->next;
				net->next = comp->next;
				comp->next = net;
				net = prevnet;
			}
			nnets++;
		}
#ifdef DEBUG
		fprintf(stderr, "\n");
#endif
	}
	/*
	 * Put a snip in the linked list of netinfos.  The idea:
	 * If there was a state change such that now there are fewer
	 * nets, we snip the list and retain the tail, rather than
	 * freeing it.  At the next state change, we clip the tail back on.
	 * This prevents a lot of malloc/free activity, and it's simpler.
	 */
	lastnet = net;
	netsnip = net->next;
	net->next = NULLNET;

	firstnet = zeronet.next;
}

static int
netinfo_load(void)
{
	struct netinfo *net;

	if (netstat_item == nilp(mib_item_t)) {
#ifdef DEBUG
		fprintf(stderr, "No net stats\n");
#endif
		return (0);
	}

	stats_s4.if_ipackets =
	stats_s4.if_opackets =
	stats_s4.if_ierrors =
	stats_s4.if_oerrors =
	stats_s4.if_collisions = 0;

	for (net = firstnet; net; net = net->next) {
		if (kstat_read(kc, net->ks, NULL) == -1)
			return (1);
		if (net->ipackets)
			stats_s4.if_ipackets	+= net->ipackets->value.ul;
		if (net->opackets)
			stats_s4.if_opackets	+= net->opackets->value.ul;
		if (net->ierrors)
			stats_s4.if_ierrors	+= net->ierrors->value.ul;
		if (net->oerrors)
			stats_s4.if_oerrors	+= net->oerrors->value.ul;
		if (net->collisions)
			stats_s4.if_collisions	+= net->collisions->value.ul;
	}
#ifdef DEBUG
	fprintf(stderr,
	    "ipackets: %d opackets: %d ierrors: %d oerrors: %d colls: %d\n",
		stats_s4.if_ipackets,
		stats_s4.if_opackets,
		stats_s4.if_ierrors,
		stats_s4.if_oerrors,
		stats_s4.if_collisions);
#endif
	return (0);
}
