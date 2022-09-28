/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)strerr.c	1.7	97/08/12 SMI"	/* SVr4.0 1.2.1.1	*/

#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <stropts.h>
#include <sys/types.h>
#include <sys/strlog.h>

#define	CTLSIZE sizeof (struct log_ctl)
#define	DATSIZE LOGMSGSZ
#define	TIMESIZE 26
#define	ADMSTR "root"
#define	LOGDEV "/dev/log"
#define	LOGNAME "STR.LOGGER"
#define	ERRFILE "/var/adm/streams/error.xxxxx"
#define	NSECDAY 86400

static void prlog(FILE *log, struct log_ctl *lp, char *dp, int flag);
static void makefile(char *name, time_t time);
static FILE *logfile(FILE *log, struct log_ctl *lp);
static int *logadjust(char *dp);
static void prlog(FILE *log, struct log_ctl *lp, char *dp, int flag);


static void
makefile(char *name, time_t time)
{
	char *r;
	struct tm *tp;

	tp = localtime(&time);
	r = &(name[strlen(name) - 5]);
	sprintf(r, "%02d-%02d", (tp->tm_mon+1), tp->tm_mday);
}

static FILE *
logfile(FILE *log, struct log_ctl *lp)
{
	static time_t lasttime = 0;
	char *errfile = ERRFILE;
	time_t newtime;

	newtime = lp->ttime - timezone;

	/*
	 * If it is a new day make a new log file
	 */
	if (((newtime/NSECDAY) != (lasttime/NSECDAY)) || !log) {
		if (log) fclose(log);
		lasttime = newtime;
		makefile(errfile, lp->ttime);
		return (fopen(errfile, "a+"));
	}
	lasttime = newtime;
	return (log);
}


int
main(int ac, char *av[])
{
	int fd, n;
	char cbuf[CTLSIZE];
	char dbuf[DATSIZE];	/* must start on word boundary */
	char mailcmd[40];
	int flag;
	struct strbuf ctl;
	struct strbuf dat;
	struct strioctl istr;
	struct log_ctl *lp;
	FILE *pfile;
	FILE *log;

	ctl.buf = cbuf;
	ctl.maxlen = CTLSIZE;
	dat.buf = dbuf;
	dat.maxlen = dat.len = DATSIZE;
	fd = open(LOGDEV, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "ERROR: unable to open %s\n", LOGDEV);
		return (1);
	}

	istr.ic_cmd = I_ERRLOG;
	istr.ic_timout = istr.ic_len = 0;
	istr.ic_dp = NULL;
	if (ioctl(fd, I_STR, &istr) < 0) {
		fprintf(stderr, "ERROR: error logger already exists\n");
		return (1);
	}

	log = NULL;
	flag = 0;
	while (getmsg(fd, &ctl, &dat, &flag) >= 0) {
		flag = 0;
		lp = (struct log_ctl *)cbuf;
		log = logfile(log, lp);
		prlog(log, lp, dbuf, 1);
		fflush(log);

		if (!(lp->flags & SL_NOTIFY)) continue;
		sprintf(mailcmd, "mail %s", ADMSTR);
		if ((pfile = popen(mailcmd, "w")) != NULL) {
			fprintf(pfile, "Streams Error Logger message "
			    "notification:\n\n");
			prlog(pfile, lp, dbuf, 0);
			pclose(pfile);
		}
	}

	return (0);
}


/*
 * calculate the address of the log printf arguments.  The pointer lp MUST
 * start on a word boundary.  This also assumes that struct log_msg is
 * an integral number of words long.  If either of these is violated,
 * an alignment fault will result.
 */
static int *
logadjust(char *dp)
{
	while (*dp++ != 0);
	dp = (char *)(((unsigned long)dp + sizeof (int) - 1) &
	    ~(sizeof (int) - 1));
	return ((int *)dp);
}


static void
prlog(FILE *log, struct log_ctl *lp, char *dp, int flag)
{
	char *ts;
	int *args;
	char *ap;
	time_t t = (time_t)lp->ttime;

	ts = ctime(&t);
	ts[19] = '\0';
	if (flag) {
		fprintf(log, "%06d %s %08x %s%s%s ", lp->seq_no, (ts+11),
		    lp->ltime,
		    ((lp->flags & SL_FATAL) ? "F" : "."),
		    ((lp->flags & SL_NOTIFY) ? "N" : "."),
		    ((lp->flags & SL_TRACE) ? "T" : "."));
		fprintf(log, "%d %d ", lp->mid, lp->sid);
	} else {
		fprintf(log, "%06d ", lp->seq_no);
	}
	args = logadjust((char *)dp);

	fprintf(log, dp, args[0], args[1], args[2]);
	putc('\n', log);
}
