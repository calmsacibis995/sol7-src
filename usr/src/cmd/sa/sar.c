/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1992 by Sun Microsystems, Inc.
 */

#pragma	ident	"@(#)sar.c	1.22	97/01/23 SMI"

/*
	sar.c - It generates a report either
		from an input data file or
		by invoking sadc to read system activity counters
		at the specified intervals.
	usage:  sar [-ubdycwaqvmpgrkA] [-o file] t [n]    or
		sar [-ubdycwaqvmpgrkA][-s hh:mm][-e hh:mm][-i ss][-f file]
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <sys/fcntl.h>

#include "sa.h"

static	void	prtopt(void);
static	void	prtavg(void);
static	void	prpass(void);
static	void	prttim(void);
static	void	prthdg(void);
static	void	tsttab(void);
static	void	update_counters(void);
static	void	usage(void);
static	void	fail(int, char *, ...);
static	void	safe_zalloc(void **, int, int);
static	void	ulong_delta(ulong_t *, ulong_t *, ulong_t *, ulong_t *,
	int, int);

static	struct sa nx, ox, ax, dx;
static	iodevinfo_t *nxio, *oxio, *axio, *dxio;

static	struct	tm *curt, args, arge;
static	struct	utsname name;
static	int	sflg, eflg, iflg, oflg, fflg;
static	float	Isyscall, Isysread, Isyswrite, Isysexec, Ireadch, Iwritech;
static	float	Osyscall, Osysread, Osyswrite, Osysexec, Oreadch, Owritech;
static	float 	Lsyscall, Lsysread, Lsyswrite, Lsysexec, Lreadch, Lwritech;

static	int	realtime, passno = 0, do_disk;
static	int	t = 0, n = 0, lines = 0;
static	int	hz;
static	int	niodevs;
static	int	tabflg;
static	char	options[30], fopt[30];
static	float	tdiff, sec_diff, totsec_diff = 0.0, percent;
static	time_t	ts, te;			/* time interval start and end */
static	float	start_time, end_time, isec;
static	int	fin, fout;
static	pid_t	childid;
static	int	pipedes[2];
static	char	arg1[10], arg2[10];
static	int	pagesize;

#define	PGTOBLK(x)	((x) * (pagesize >> 9))
#define	BLKTOPG(x)	((x) / (pagesize >> 9))
#define	BLKS(x)		((x) >> 9)

static	void ulong_delta();
static	float fcap(float);
static	float fmax(float, float);
static	float denom(float);
static	float freq(float, float);

void
main(int argc, char **argv)
{
	char    flnm[50], ofile[50];
	char	ccc;
	time_t    temp;
	int	i, jj = 0;

	pagesize = sysconf(_SC_PAGESIZE);

	/*
	 * Process options with arguments and pack options
	 * without arguments.
	 */
	while ((i = getopt(argc, argv, "ubdycwaqvmpgrkAo:s:e:i:f:")) != EOF)
		switch (ccc = i) {
		    case 'o':
			oflg++;
			sprintf(ofile, "%s", optarg);
			break;
		    case 's':
			if (sscanf(optarg, "%d:%d:%d",
			&args.tm_hour, &args.tm_min, &args.tm_sec) < 1)
				fail(0, "-%c %s -- illegal option argument",
					ccc, optarg);
			else {
				sflg++,
				start_time = args.tm_hour*3600.0 +
					args.tm_min*60.0 +
					args.tm_sec;
			}
			break;
		    case 'e':
			if (sscanf(optarg, "%d:%d:%d",
			&arge.tm_hour, &arge.tm_min, &arge.tm_sec) < 1)
				fail(0, "-%c %s -- illegal option argument",
					ccc, optarg);
			else {
				eflg++;
				end_time = arge.tm_hour*3600.0 +
					arge.tm_min*60.0 +
					arge.tm_sec;
			}
			break;
		    case 'i':
			if (sscanf(optarg, "%f", &isec) < 1)
				fail(0, "-%c %s -- illegal option argument",
					ccc, optarg);
			else {
				if (isec > 0.0)
					iflg++;
			}
			break;
		    case 'f':
			fflg++;
			sprintf(flnm, "%s", optarg);
			break;
		    case '?':
			usage();
			exit(1);
			break;
		default:
			strncat(options, &ccc, 1);
			break;
		}

	/*   Are starting and ending times consistent?  */
	if ((sflg) && (eflg) && (end_time <= start_time))
		fail(0, "ending time <= starting time");

	/*
	 * Determine if t and n arguments are given,
	 * and whether to run in real time or from a
	 * file.
	 */
	switch (argc - optind) {
	    case 0:		/*   Get input data from file   */
		if (fflg == 0) {
			temp = time((time_t *)0);
			curt = localtime(&temp);
			sprintf(flnm, "/var/adm/sa/sa%.2d", curt->tm_mday);
		}
		if ((fin = open(flnm, 0)) == -1)
			fail(1, "can't open %s", flnm);
		break;
	    case 1:		/*   Real time data; one cycle   */
		realtime++;
		t = atoi(argv[optind]);
		n = 2;
		break;
	    case 2:		/*   Real time data; specified cycles   */
	default:
		realtime++;
		t = atoi(argv[optind]);
		n = 1 + atoi(argv[optind+1]);
		break;
	}

	/*
	 * "u" is default option to display cpu utilization.
	 */
	if (strlen(options) == 0)
		strcpy(options, "u");
	/*    'A' means all data options   */

	if (strchr(options, 'A') != NULL)
		strcpy(options, "udqbwcayvmpgrk");

	if (realtime) {
		/*
		 * Get input data from sadc via pipe.
		 */
		if (t <= 0)
			fail(0, "sampling interval t <= 0 sec");
		if (n < 2)
			fail(0, "number of sample intervals n <= 0");
		sprintf(arg1, "%d", t);
		sprintf(arg2, "%d", n);
		if (pipe(pipedes) == -1)
			fail(1, "pipe failed");
		if ((childid = fork()) == 0) {	/*  child:   */
			close(1);	/*  shift pipedes[write] to stdout  */
			dup(pipedes[1]);
			if (execlp("/usr/lib/sa/sadc",
			    "/usr/lib/sa/sadc", arg1, arg2, 0) == -1)
				fail(1, "exec of /usr/lib/sa/sadc failed");
		} else if (childid == -1) {
			fail(1, "Could not fork to exec sadc");
		}		/*   parent:   */
		fin = pipedes[0];
		close(pipedes[1]);	/*   Close unused output   */
	}

	if (oflg) {
		if (strcmp(ofile, flnm) == 0)
			fail(0, "output file name same as input file name");
		fout = creat(ofile, 00644);
	}

	hz = sysconf(_SC_CLK_TCK);

	nxio = oxio = dxio = axio = NULL;

	if (realtime) {
		/*   Make single pass, processing all options   */
		strcpy(fopt, options);
		passno++;
		prpass();
		kill(childid, 2);
		wait((int *)0);
	} else {
		/*   Make multiple passes, one for each option   */
		while (strlen(strncpy(fopt, &options[jj++], 1))) {
			lseek(fin, 0, SEEK_SET);
			passno++;
			prpass();
		}
	}
	exit(0);
}

/*
 * Read records from input, classify, and decide on printing.
 */
static void
prpass(void)
{
	int i, size, state_change, recno = 0;
	float trec, tnext = 0;
	ulong_t old_niodevs = 0;

	do_disk = (strchr(fopt, 'd') != NULL);

	if (sflg)
		tnext = start_time;
	while (read(fin, &nx, sizeof (struct sa)) > 0) {
		state_change = 0;
		niodevs = nx.niodevs;
		if (niodevs != old_niodevs) {
			size = niodevs * sizeof (iodevinfo_t);
			safe_zalloc((void *)&nxio, size, 1);
			safe_zalloc((void *)&oxio, size, 1);
			safe_zalloc((void *)&dxio, size, 1);
			safe_zalloc((void *)&axio, size, 1);
			old_niodevs = niodevs;
			state_change = 1;
		}
		for (i = 0; i < niodevs; i++) {
			read(fin, &nxio[i], sizeof (iodevinfo_t));
			if (nxio[i].ks.ks_kid != oxio[i].ks.ks_kid)
				state_change = 1;
		}
		curt = localtime(&nx.ts);
		trec =	curt->tm_hour * 3600.0 +
			curt->tm_min * 60.0 +
			curt->tm_sec;
		if ((recno == 0) && (trec < start_time))
			continue;
		if ((eflg) && (trec > end_time))
			break;
		if ((oflg) && (passno == 1)) {
			write(fout, &nx, sizeof (struct sa));
			for (i = 0; i < niodevs; i++) {
				write(fout, &nxio[i], sizeof (iodevinfo_t));
			}
		}
		if (recno == 0) {
			if (passno == 1) {
				uname(&name);
				printf("\n%s %s %s %s %s    %.2d/%.2d/%.2d\n",
					name.sysname,
					name.nodename,
					name.release,
					name.version,
					name.machine,
					curt->tm_mon + 1,
					curt->tm_mday,
					(curt->tm_year % 100));
			}
			prthdg();
			recno = 1;
			if ((iflg) && (tnext == 0))
				tnext = trec;
		}
		if (nx.valid == 0) {
			/*
			 * This dummy record signifies system restart
			 * New initial values of counters follow in next
			 * record.
			 */
			if (!realtime) {
				prttim();
				printf("\tunix restarts\n");
				recno = 1;
				continue;
			}
		}
		if ((iflg) && (trec < tnext))
			continue;
		if (state_change && recno > 1 && do_disk) {
			prttim();
			printf("\t<<State change>>\n\n");
		} else {
			if (recno++ > 1) {
				ts = ox.csi.cpu[0] + ox.csi.cpu[1] +
					ox.csi.cpu[2] + ox.csi.cpu[3];
				te = nx.csi.cpu[0] + nx.csi.cpu[1] +
					nx.csi.cpu[2] + nx.csi.cpu[3];
				tdiff = (float)(te - ts);
				sec_diff = tdiff / hz;
				percent = 100.0 / tdiff;
				if (tdiff <= 0)
					continue;
				update_counters();
				prtopt();
				lines++;
				if (passno == 1)
					totsec_diff += sec_diff;
			}
		}
		ox = nx;		/*  Age the data	*/
		for (i = 0; i < niodevs; i++)
			oxio[i] = nxio[i];
		if (isec > 0)
			while (tnext <= trec)
				tnext += isec;
	}
	if (lines > 1)
		prtavg();
	memset(&ax, 0, sizeof (ax));	/*  Zero out the accumulators   */
	for (i = 0; i < niodevs; i++)
		memset(&axio[i], 0, sizeof (iodevinfo_t));
}

/*
 * Print time label routine.
 */
static void
prttim(void)
{
	curt = localtime(&nx.ts);
	printf("%.2d:%.2d:%.2d", curt->tm_hour, curt->tm_min, curt->tm_sec);
	tabflg = 1;
}

/*
 * Test if 8-spaces to be added routine.
 */
static void
tsttab(void)
{
	if (tabflg == 0)
		printf("        ");
	else
		tabflg = 0;
}

/*
 * Print report heading routine.
 */
static void
prthdg(void)
{
	int	jj = 0;
	char	ccc;

	printf("\n");
	prttim();
	while ((ccc = fopt[jj++]) != NULL) {
		tsttab();
		switch (ccc) {
		    case 'u':
			printf(" %7s %7s %7s %7s\n",
				"%usr",
				"%sys",
				"%wio",
				"%idle");
			break;
		    case 'b':
			printf(" %7s %7s %7s %7s %7s %7s %7s %7s\n",
				"bread/s",
				"lread/s",
				"%rcache",
				"bwrit/s",
				"lwrit/s",
				"%wcache",
				"pread/s",
				"pwrit/s");
			break;
		    case 'd':
			printf("   %-8.8s    %7s %7s %7s %7s %7s %7s\n",
				"device",
				"%busy",
				"avque",
				"r+w/s",
				"blks/s",
				"avwait",
				"avserv");
			break;
		    case 'y':
			printf(" %7s %7s %7s %7s %7s %7s\n",
				"rawch/s",
				"canch/s",
				"outch/s",
				"rcvin/s",
				"xmtin/s",
				"mdmin/s");
			break;
		    case 'c':
			printf(" %7s %7s %7s %7s %7s %7s %7s\n",
				"scall/s",
				"sread/s",
				"swrit/s",
				"fork/s",
				"exec/s",
				"rchar/s",
				"wchar/s");
			break;
		    case 'w':
			printf(" %7s %7s %7s %7s %7s\n",
				"swpin/s",
				"bswin/s",
				"swpot/s",
				"bswot/s",
				"pswch/s");
			break;
		    case 'a':
			printf(" %7s %7s %7s\n",
				"iget/s",
				"namei/s",
				"dirbk/s");
			break;
		    case 'q':
			printf(" %7s %7s %7s %7s\n",
				"runq-sz",
				"%runocc",
				"swpq-sz",
				"%swpocc");
			break;
		    case 'v':
			printf("  %s  %s  %s   %s\n",
				"proc-sz    ov",
				"inod-sz    ov",
				"file-sz    ov",
				"lock-sz");
			break;
		    case 'm':
			printf(" %7s %7s\n",
				"msg/s",
				"sema/s");
			break;
		    case 'p':
			printf(" %7s %7s %7s %7s %7s %7s\n",
				"atch/s",
				"pgin/s",
				"ppgin/s",
				"pflt/s",
				"vflt/s",
				"slock/s");
			break;
		    case 'g':
			printf(" %8s %8s %8s %8s %8s\n",
				"pgout/s",
				"ppgout/s",
				"pgfree/s",
				"pgscan/s",
				"%ufs_ipf");
			break;
		    case 'r':
			printf(" %7s %8s\n",
				"freemem",
				"freeswap");
			break;
		    case 'k':
			printf(" %7s %7s %5s %7s %7s %5s %11s %5s\n",
				"sml_mem",
				"alloc",
				"fail",
				"lg_mem",
				"alloc",
				"fail",
				"ovsz_alloc",
				"fail");
			break;
		}
	}
	if (jj > 2 || do_disk)
		printf("\n");
}

/*
 * compute deltas and update accumulators
 */
static void
update_counters(void)
{
	int i;
	iodevinfo_t *nio, *oio, *aio, *dio;

	ulong_delta((ulong_t *)&nx.csi, (ulong_t *)&ox.csi,
		(ulong_t *)&dx.csi, (ulong_t *)&ax.csi, 0, sizeof (ax.csi));
	ulong_delta((ulong_t *)&nx.si, (ulong_t *)&ox.si,
		(ulong_t *)&dx.si, (ulong_t *)&ax.si, 0, sizeof (ax.si));
	ulong_delta((ulong_t *)&nx.cvmi, (ulong_t *)&ox.cvmi,
		(ulong_t *)&dx.cvmi, (ulong_t *)&ax.cvmi, 0,
		sizeof (ax.cvmi));

	ax.vmi.freemem += dx.vmi.freemem = nx.vmi.freemem - ox.vmi.freemem;
	ax.vmi.swap_avail += dx.vmi.swap_avail =
		nx.vmi.swap_avail - ox.vmi.swap_avail;

	nio = nxio;
	oio = oxio;
	aio = axio;
	dio = dxio;
	for (i = 0; i < niodevs; i++) {
		aio->kios.wlastupdate += dio->kios.wlastupdate
			= nio->kios.wlastupdate - oio->kios.wlastupdate;
		aio->kios.reads += dio->kios.reads
			= nio->kios.reads - oio->kios.reads;
		aio->kios.writes += dio->kios.writes
			= nio->kios.writes - oio->kios.writes;
		aio->kios.nread += dio->kios.nread
			= nio->kios.nread - oio->kios.nread;
		aio->kios.nwritten += dio->kios.nwritten
			= nio->kios.nwritten - oio->kios.nwritten;
		aio->kios.wlentime += dio->kios.wlentime
			= nio->kios.wlentime - oio->kios.wlentime;
		aio->kios.rlentime += dio->kios.rlentime
			= nio->kios.rlentime - oio->kios.rlentime;
		aio->kios.wtime += dio->kios.wtime
			= nio->kios.wtime - oio->kios.wtime;
		aio->kios.rtime += dio->kios.rtime
			= nio->kios.rtime - oio->kios.rtime;
		nio++;
		oio++;
		aio++;
		dio++;
	}
}

static void
prt_u_opt(struct sa *xx)
{
	printf(" %7.0f %7.0f %7.0f %7.0f\n",
		(float)xx->csi.cpu[1] * percent,
		(float)xx->csi.cpu[2] * percent,
		(float)xx->csi.cpu[3] * percent,
		(float)xx->csi.cpu[0] * percent);
}

static void
prt_b_opt(struct sa *xx)
{
	printf(" %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
		(float)xx->csi.bread / sec_diff,
		(float)xx->csi.lread / sec_diff,
		freq((float)xx->csi.lread, (float)xx->csi.bread),
		(float)xx->csi.bwrite / sec_diff,
		(float)xx->csi.lwrite / sec_diff,
		freq((float)xx->csi.lwrite, (float)xx->csi.bwrite),
		(float)xx->csi.phread / sec_diff,
		(float)xx->csi.phwrite / sec_diff);
}

static void
prt_d_opt(int ii, iodevinfo_t *xio)
{
	double etime, hr_etime, tps, avq, avs;

	tsttab();

	hr_etime = (double)xio[ii].kios.wlastupdate;
	if (hr_etime == 0.0)
		hr_etime = (double)NANOSEC;
	etime = hr_etime / (double)NANOSEC;
	tps = (double)(xio[ii].kios.reads + xio[ii].kios.writes) / etime;
	avq = (double)xio[ii].kios.wlentime / hr_etime;
	avs = (double)xio[ii].kios.rlentime / hr_etime;

	printf("   %-8.8s    ", nxio[ii].ks.ks_name);
	printf("%7.0f %7.1f %7.0f %7.0f %7.1f %7.1f\n",
		(double)xio[ii].kios.rtime * 100.0 / hr_etime,
		avq + avs,
		tps,
		BLKS(xio[ii].kios.nread + xio[ii].kios.nwritten) / etime,
		(tps > 0 ? avq / tps * 1000.0 : 0.0),
		(tps > 0 ? avs / tps * 1000.0 : 0.0));
}

static void
prt_y_opt(struct sa *xx)
{
	printf(" %7.0f %7.0f %7.0f %7.0f %7.0f %7.0f\n",
		(float)xx->csi.rawch / sec_diff,
		(float)xx->csi.canch / sec_diff,
		(float)xx->csi.outch / sec_diff,
		(float)xx->csi.rcvint / sec_diff,
		(float)xx->csi.xmtint / sec_diff,
		(float)xx->csi.mdmint / sec_diff);
}

static void
prt_c_opt(struct sa *xx)
{
	printf(" %7.0f %7.0f %7.0f %7.2f %7.2f %7.0f %7.0f\n",
		(float)xx->csi.syscall / sec_diff,
		(float)xx->csi.sysread / sec_diff,
		(float)xx->csi.syswrite / sec_diff,
		(float)(xx->csi.sysfork + xx->csi.sysvfork) / sec_diff,
		(float)xx->csi.sysexec / sec_diff,
		(float)xx->csi.readch / sec_diff,
		(float)xx->csi.writech / sec_diff);
}

static void
prt_w_opt(struct sa *xx)
{
	printf(" %7.2f %7.1f %7.2f %7.1f %7.0f\n",
		(float)xx->cvmi.swapin / sec_diff,
		(float)PGTOBLK(xx->cvmi.pgswapin) / sec_diff,
		(float)xx->cvmi.swapout / sec_diff,
		(float)PGTOBLK(xx->cvmi.pgswapout) / sec_diff,
		(float)xx->csi.pswitch / sec_diff);
}

static void
prt_a_opt(struct sa *xx)
{
	printf(" %7.0f %7.0f %7.0f\n",
		(float)xx->csi.ufsiget / sec_diff,
		(float)xx->csi.namei / sec_diff,
		(float)xx->csi.ufsdirblk / sec_diff);
}

static void
prt_q_opt(struct sa *xx)
{
	if (xx->si.runocc == 0)
		printf(" %7s %7s", "  ", "  ");
	else {
		printf(" %7.1f %7.0f",
			(float)xx->si.runque / (float)xx->si.runocc,
			(float)xx->si.runocc / sec_diff * 100.0);
	}
	if (xx->si.swpocc == 0)
		printf(" %7s %7s\n", "  ", "  ");
	else {
		printf(" %7.1f %7.0f\n",
			(float)xx->si.swpque / (float)xx->si.swpocc,
			(float)xx->si.swpocc / sec_diff * 100.0);
	}
}

static void
prt_v_opt(struct sa *xx)
{
	printf(" %4lu/%-4lu %4lu %4lu/%-4lu %4lu %4lu/%-4lu %4lu %4lu/%-4lu\n",
		nx.szproc, nx.mszproc, xx->csi.procovf,
		nx.szinode, nx.mszinode, xx->csi.inodeovf,
		nx.szfile, nx.mszfile, xx->csi.fileovf,
		nx.szlckr, nx.mszlckr);
}

static void
prt_m_opt(struct sa *xx)
{
	printf(" %7.2f %7.2f\n",
		(float)xx->csi.msg / sec_diff,
		(float)xx->csi.sema / sec_diff);
}

static void
prt_p_opt(struct sa *xx)
{
	printf(" %7.2f %7.2f %7.2f %7.2f %7.2f %7.2f\n",
		(float)xx->cvmi.pgfrec / sec_diff,
		(float)xx->cvmi.pgin / sec_diff,
		(float)xx->cvmi.pgpgin / sec_diff,
		(float)(xx->cvmi.prot_fault + xx->cvmi.cow_fault) / sec_diff,
		(float)(xx->cvmi.hat_fault + xx->cvmi.as_fault) / sec_diff,
		(float)xx->cvmi.softlock / sec_diff);
}

static void
prt_g_opt(struct sa *xx)
{
	printf(" %8.2f %8.2f %8.2f %8.2f %8.2f\n",
		(float)xx->cvmi.pgout / sec_diff,
		(float)xx->cvmi.pgpgout / sec_diff,
		(float)xx->cvmi.dfree / sec_diff,
		(float)xx->cvmi.scan / sec_diff,
		(float)xx->csi.ufsipage * 100.0 /
			denom((float)xx->csi.ufsipage +
			(float)xx->csi.ufsinopage));
}

static void
prt_r_opt(struct sa *xx)
{
	printf(" %7.0f %8.0f\n",
		(double)xx->vmi.freemem / sec_diff,
		(double)PGTOBLK(xx->vmi.swap_avail) / sec_diff);
}

static void
prt_k_opt(struct sa *xx, int n)
{
	printf(" %7.0f %7.0f %5.0f %7.0f %7.0f %5.0f %11.0f %5.0f\n",
		(float)xx->kmi.km_mem[KMEM_SMALL] / n,
		(float)xx->kmi.km_alloc[KMEM_SMALL] / n,
		(float)xx->kmi.km_fail[KMEM_SMALL] / n,
		(float)xx->kmi.km_mem[KMEM_LARGE] / n,
		(float)xx->kmi.km_alloc[KMEM_LARGE] / n,
		(float)xx->kmi.km_fail[KMEM_LARGE] / n,
		(float)xx->kmi.km_alloc[KMEM_OSIZE] / n,
		(float)xx->kmi.km_fail[KMEM_OSIZE] / n);
}

/*
 * Print options routine.
 */
static void
prtopt(void)
{
	int	ii, jj = 0;
	char	ccc;

	prttim();

	while ((ccc = fopt[jj++]) != NULL) {
		if (ccc != 'd')
			tsttab();
		switch (ccc) {
		    case 'u':
			prt_u_opt(&dx);
			break;
		    case 'b':
			prt_b_opt(&dx);
			break;
		    case 'd':
			for (ii = 0; ii < niodevs; ii++)
				prt_d_opt(ii, dxio);
			break;
		    case 'y':
			prt_y_opt(&dx);
			break;
		    case 'c':
			prt_c_opt(&dx);
			break;
		    case 'w':
			prt_w_opt(&dx);
			break;
		    case 'a':
			prt_a_opt(&dx);
			break;
		    case 'q':
			prt_q_opt(&dx);
			break;
		    case 'v':
			prt_v_opt(&dx);
			break;
		    case 'm':
			prt_m_opt(&dx);
			break;
		    case 'p':
			prt_p_opt(&dx);
			break;
		    case 'g':
			prt_g_opt(&dx);
			break;
		    case 'r':
			prt_r_opt(&dx);
			break;
		    case 'k':
			prt_k_opt(&nx, 1);
			ax.kmi.km_mem[KMEM_SMALL] +=
				nx.kmi.km_mem[KMEM_SMALL];
			ax.kmi.km_alloc[KMEM_SMALL] +=
				nx.kmi.km_alloc[KMEM_SMALL];
			ax.kmi.km_fail[KMEM_SMALL] +=
				nx.kmi.km_fail[KMEM_SMALL];
			ax.kmi.km_mem[KMEM_LARGE] +=
				nx.kmi.km_mem[KMEM_LARGE];
			ax.kmi.km_alloc[KMEM_LARGE] +=
				nx.kmi.km_alloc[KMEM_LARGE];
			ax.kmi.km_fail[KMEM_LARGE] +=
				nx.kmi.km_fail[KMEM_LARGE];
			ax.kmi.km_alloc[KMEM_OSIZE] +=
				nx.kmi.km_alloc[KMEM_OSIZE];
			ax.kmi.km_fail[KMEM_OSIZE] +=
				nx.kmi.km_fail[KMEM_OSIZE];
			break;
		}
	}
	if (jj > 2 || do_disk)
		printf("\n");
}

/*
 * Print average routine.
 */
static void
prtavg(void)
{
	int	ii, jj = 0;
	char	ccc;

	tdiff = ax.csi.cpu[0] + ax.csi.cpu[1] + ax.csi.cpu[2] + ax.csi.cpu[3];
	if (tdiff <= 0.0)
		return;

	sec_diff = tdiff / hz;
	percent = 100.0 / tdiff;
	printf("\n");

	while ((ccc = fopt[jj++]) != NULL) {
		if (ccc != 'v')
			printf("Average ");
		switch (ccc) {
		    case 'u':
			prt_u_opt(&ax);
			break;
		    case 'b':
			prt_b_opt(&ax);
			break;
		    case 'd':
			tabflg = 1;
			for (ii = 0; ii < niodevs; ii++)
				prt_d_opt(ii, axio);
			break;
		    case 'y':
			prt_y_opt(&ax);
			break;
		    case 'c':
			prt_c_opt(&ax);
			break;
		    case 'w':
			prt_w_opt(&ax);
			break;
		    case 'a':
			prt_a_opt(&ax);
			break;
		    case 'q':
			prt_q_opt(&ax);
			break;
		    case 'v':
			break;
		    case 'm':
			prt_m_opt(&ax);
			break;
		    case 'p':
			prt_p_opt(&ax);
			break;
		    case 'g':
			prt_g_opt(&ax);
			break;
		    case 'r':
			prt_r_opt(&ax);
			break;
		    case 'k':
			prt_k_opt(&ax, lines);
			break;
		}
	}
}

static void
ulong_delta(ulong_t *new, ulong_t *old, ulong_t *delta, ulong_t *accum,
	int begin, int end)
{
	int i;
	ulong *np, *op, *dp, *ap;

	np = new;
	op = old;
	dp = delta;
	ap = accum;
	for (i = begin; i < end; i += sizeof (ulong))
		*ap++ += *dp++ = *np++ - *op++;
}

/*
 * cap percentages at 100.0
 */
static float
fcap(float x)
{
	return ((x > 1.0) ? 100.0 : 100.0 * x);
}

/*
 * return the max of x and y
 */
static float
fmax(float x, float y)
{
	return ((x > y) ? x : y);
}

/*
 * used to prevent zero denominators
 */
static float
denom(float x)
{
	return ((x > 0.5) ? x : 1.0);
}

/*
 * a little calculation that comes up often when computing frequency
 * of one operation relative to another
 */
static float
freq(float x, float y)
{
	return ((x < 0.5) ? 100.0 : (x - y) / x * 100.0);
}

static void
usage(void)
{
	fprintf(stderr,
		"usage: sar [-ubdycwaqvmpgrkA][-o file] t [n]\n");
	fprintf(stderr,
		"\tsar [-ubdycwaqvmpgrkA]");
	fprintf(stderr,
		"[-s hh:mm][-e hh:mm][-i ss][-f file]\n");
}

static void
fail(int do_perror, char *message, ...)
{
	va_list args;

	va_start(args, message);
	fprintf(stderr, "sar: ");
	vfprintf(stderr, message, args);
	va_end(args);
	fprintf(stderr, "\n");
	if (do_perror)
		perror("");
	else
		usage();
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
