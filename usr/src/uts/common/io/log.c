/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1991-1995,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)log.c	1.41	97/10/22 SMI"	/* SVr4.0 1.14	*/

/*
 * Streams log driver.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strlog.h>
#include <sys/debug.h>
#include <sys/cred.h>
#include <sys/file.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/log.h>
#include <sys/msgbuf.h>

#include <sys/conf.h>
#include <sys/sunddi.h>


static int log_errseq, log_trcseq, log_conseq;  /* logger sequence numbers */
static int numlogtrc;		/* number of processes reading trace log */
static int numlogerr;		/* number of processes reading error log */
static int numlogcons;		/* number of processes reading console log */

/* now defined in space.c because log is loadable */
extern int conslogging;		/* set when someone is logging console output */

static kmutex_t log_lock;

extern kmutex_t logdq_lock;	/* defined in space.c because log is loadable */
extern queue_t *log_d_q;

static int logopen(queue_t *, dev_t *, int, int, cred_t *);
static int logclose(queue_t *, int, cred_t *);
static int logwput(queue_t *, mblk_t *);
static int logrsrv(queue_t *);
static int shouldtrace(short, short, char);
static int logtrace(struct log *, short, short, signed char);
static int log_internal(mblk_t *, int, int);
static int log_sendmsg(struct log *, mblk_t *);
static int process_msg(queue_t *);

static struct module_info logm_info = {
	LOG_MID,
	LOG_NAME,
	LOG_MINPS,
	LOG_MAXPS,
	LOG_HIWAT,
	LOG_LOWAT
};

static struct qinit logrinit = {
	NULL,
	logrsrv,
	logopen,
	logclose,
	NULL,
	&logm_info,
	NULL
};

static struct qinit logwinit = {
	logwput,
	NULL,
	NULL,
	NULL,
	NULL,
	&logm_info,
	NULL
};

static struct streamtab loginfo = {
	&logrinit,
	&logwinit,
	NULL,
	NULL
};

static int log_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result);
static int log_identify(dev_info_t *devi);
static int log_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static dev_info_t *log_dip;		/* private copy of devinfo pointer */

#define	LOG_CONF_FLAG		(D_NEW | D_MP)
	DDI_DEFINE_STREAM_OPS(log_ops, log_identify, nulldev,	\
			log_attach, nodev, nodev,		\
			log_info, LOG_CONF_FLAG, &loginfo);

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/modctl.h>

/*
 * Module linkage information for the kernel.
 */

static struct modldrv modldrv = {
	&mod_driverops, /* Type of module.  This one is a pseudo driver */
	"streams log driver 'log'",
	&log_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};


int
_init()
{
	int retval = mod_install(&modlinkage);

	if (retval == 0) {
		int i;

		numlogtrc = 0;
		numlogerr = 0;
		numlogcons = 0;
		log_errseq = 0;
		log_trcseq = 0;
		log_conseq = 0;
		for (i = 0; i < log_cnt; i++) {
			log_log[i].log_state = 0;
			cv_init(&log_log[i].log_cv, NULL, CV_DEFAULT, NULL);
		}
		mutex_init(&log_lock, NULL, MUTEX_DEFAULT, NULL);
		mutex_init(&logdq_lock, NULL, MUTEX_DEFAULT, NULL);
	}
	return (retval);
}

int
_fini()
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
log_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "log") == 0)
		return (DDI_IDENTIFIED);
	return (DDI_NOT_IDENTIFIED);
}

/* ARGSUSED */
static int
log_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
    void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if (log_dip == NULL) {
			error = DDI_FAILURE;
		} else {
			*result = (void *)log_dip;
			error = DDI_SUCCESS;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/* ARGSUSED */
static int
log_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "conslog", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE ||
	    ddi_create_minor_node(devi, "log", S_IFCHR,
	    5, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	log_dip = devi;
	return (DDI_SUCCESS);
}


int logdevflag = 0;		/* new driver interface */

/*
 * Log_init() is now in strlog.c as it is called early on in main() via
 * the init table in param.c.
 */

/*
 * Log driver open routine.  Only two ways to get here.  Normal
 * access for loggers is through the clone minor.  Only one user
 * per clone minor.  Access to writing to the console log is
 * through the console minor.  Users can't read from this device.
 * Any number of users can have it open at one time.
 */
/* ARGSUSED */
static int
logopen(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *cr)
{
	int i;
	struct log *lp;

	/*
	 * A MODOPEN is invalid and so is a CLONEOPEN.
	 * This is because a clone open comes in as a CLONEMIN device open!!
	 */
	if (sflag)
		return (ENXIO);

	mutex_enter(&log_lock);
	switch (getminor(*devp)) {

	case CONSWMIN:
		if (flag & FREAD) {	/* you can only write to this minor */
			mutex_exit(&log_lock);
			return (EINVAL);
		}
		if (q->q_ptr) {		/* already open */
			mutex_exit(&log_lock);
			return (0);
		}
		lp = &log_log[CONSWMIN];
		break;

	case DEDICATED_Q:
		/*
		 * This open should be done only once and through strplumb
		 */
		if (log_d_q != NULL) {		/* already open */
			mutex_exit(&log_lock);
			return (ENXIO);
		}
		lp = &log_log[DEDICATED_Q];
		log_d_q = q;
		break;

	case CLONEMIN:
		/*
		 * Find an unused minor > CLONEMIN.
		 */
		i = CLONEMIN + 1;
		for (lp = &log_log[i]; i < log_cnt; i++, lp++) {
			if (!(lp->log_state & LOGOPEN))
				break;
		}
		if (i >= log_cnt) {
			mutex_exit(&log_lock);
			return (ENXIO);
		}
		*devp = makedevice(getmajor(*devp), i);	/* clone it */
		break;

	default:
		mutex_exit(&log_lock);
		return (ENXIO);
	}

	/*
	 * Finish device initialization.
	 */
	lp->log_state = LOGOPEN;
	lp->log_rdq = q;
	q->q_ptr = (caddr_t)lp;
	WR(q)->q_ptr = (caddr_t)lp;
	mutex_exit(&log_lock);
	qprocson(q);
	return (0);
}

/*
 * Log driver close routine.
 */
/* ARGSUSED */
static int
logclose(queue_t *q, int flag, cred_t *cr)
{
	struct log *lp;
	mblk_t	*mp = NULL;

	ASSERT(q->q_ptr);

	mutex_enter(&logdq_lock);
	if (q == log_d_q)	/* dedicated queue ? */
		log_d_q = NULL;
	mutex_exit(&logdq_lock);

	/*
	 * No more threads while we tear down the struct.
	 */
	qprocsoff(q);

	mutex_enter(&log_lock);
	lp = (struct log *)q->q_ptr;
	while (lp->log_refcnt > 0) {
		lp->log_state |= LOGCLOSE;
		cv_wait(&lp->log_cv, &log_lock);
	}

	lp->log_state &= ~LOGCLOSE;

	if (lp->log_state & LOGTRC) {
		mp = lp->log_tracemp;
		lp->log_tracemp = NULL;
		numlogtrc--;
	}
	if (lp->log_state & LOGERR)
		numlogerr--;
	if (lp->log_state & LOGCONS) {
		numlogcons--;
		if (numlogcons == 0)
			conslogging = 0;
	}
	lp->log_state = 0;
	lp->log_rdq = NULL;
	q->q_ptr = NULL;
	WR(q)->q_ptr = NULL;
	mutex_exit(&log_lock);

	if (mp)
		freemsg(mp);

	/*
	 * Do not worry about msgs queued on the q, the framework
	 * will free them up.
	 */
	return (0);
}

/*
 * Write queue put procedure.
 */
static int
logwput(queue_t *q, mblk_t *bp)
{
	unsigned int s;
	struct iocblk *iocp;
	struct log *lp;
	struct log_ctl *lcp;
	mblk_t *cbp, *pbp;
	size_t size;
	clock_t c;
	time_t t;

	lp = (struct log *)q->q_ptr;
	switch (bp->b_datap->db_type) {
	case M_FLUSH:
		if (*bp->b_rptr & FLUSHW) {
			flushq(q, FLUSHALL);
			*bp->b_rptr &= ~FLUSHW;
		}
		if (*bp->b_rptr & FLUSHR) {
			flushq(RD(q), FLUSHALL);
			qreply(q, bp);
		} else {
			freemsg(bp);
		}
		break;

	case M_IOCTL:
		mutex_enter(&log_lock);
		if (lp == &log_log[CONSWMIN]) {	/* can not ioctl CONSWMIN */
			mutex_exit(&log_lock);
			goto lognak;
		}
		mutex_exit(&log_lock);
		iocp = (struct iocblk *)bp->b_rptr;
		if (iocp->ioc_count == TRANSPARENT)
			goto lognak;
		switch (iocp->ioc_cmd) {

		case I_CONSLOG:
			mutex_enter(&log_lock);
			if (lp->log_state & LOGCONS) {
				iocp->ioc_error = EBUSY;
				mutex_exit(&log_lock);
				goto lognak;
			}
			++numlogcons;

			lp->log_state |= LOGCONS;


			mutex_exit(&log_lock);

			if (!(cbp = allocb(sizeof (struct log_ctl), BPRI_HI))) {
				iocp->ioc_error = EAGAIN;
				mutex_enter(&log_lock);
				lp->log_state &= ~LOGCONS;
				numlogcons--;
				mutex_exit(&log_lock);
				goto lognak;
			}
			size = msgbuf_size();

			if (!(pbp = allocb(size, BPRI_HI))) {
				freeb(cbp);
				iocp->ioc_error = EAGAIN;
				mutex_enter(&log_lock);
				lp->log_state &= ~LOGCONS;
				numlogcons--;
				mutex_exit(&log_lock);
				goto lognak;
			}
			cbp->b_datap->db_type = M_PROTO;
			cbp->b_cont = pbp;
			cbp->b_wptr += sizeof (struct log_ctl);
			lcp = (struct log_ctl *)cbp->b_rptr;
			lcp->mid = LOG_MID;
			lcp->sid = (short)(lp - log_log);
			(void) drv_getparm(LBOLT, &c);
			lcp->ltime = c;
			(void) drv_getparm(TIME, &t);
			lcp->ttime = t;
			lcp->level = 0;
			lcp->flags = SL_CONSOLE;
			lcp->seq_no = log_conseq;
			lcp->pri = LOG_KERN|LOG_INFO;

			pbp->b_wptr = (u_char *)
			    msgbuf_get((caddr_t)pbp->b_wptr, size);

			mutex_enter(&log_lock);
			conslogging = 1;
			s = CLONEMIN + 1;
			for (lp = &log_log[s]; s < log_cnt; s++, lp++)
			    if (lp->log_state & LOGCONS) {
				++lp->log_refcnt;
				mutex_exit(&log_lock);
				(void) log_sendmsg(lp, cbp);
				mutex_enter(&log_lock);
				if ((--lp->log_refcnt == 0) &&
				    (lp->log_state & LOGCLOSE))
					cv_broadcast(&lp->log_cv);
			    }
			mutex_exit(&log_lock);
			freemsg(cbp);
			goto logack;

		case I_TRCLOG:
			mutex_enter(&log_lock);
			if (!(lp->log_state & LOGTRC) && bp->b_cont) {
				lp->log_tracemp = bp->b_cont;
				bp->b_cont = NULL;
				numlogtrc++;
				lp->log_state |= LOGTRC;
				mutex_exit(&log_lock);
				goto logack;
			}
			mutex_exit(&log_lock);
			iocp->ioc_error = EBUSY;
			goto lognak;

		case I_ERRLOG:
			mutex_enter(&log_lock);
			if (!(lp->log_state & LOGERR)) {
				numlogerr++;
				lp->log_state |= LOGERR;
				mutex_exit(&log_lock);

logack:
				iocp->ioc_count = 0;
				bp->b_datap->db_type = M_IOCACK;
				qreply(q, bp);
				break;
			}
			mutex_exit(&log_lock);
			iocp->ioc_error = EBUSY;
			goto lognak;

		default:
lognak:
			bp->b_datap->db_type = M_IOCNAK;
			qreply(q, bp);
			break;
		}
		break;

	case M_PROTO:
		if (((bp->b_wptr - bp->b_rptr) != sizeof (struct log_ctl)) ||
		    !bp->b_cont) {
			freemsg(bp);
			break;
		}
		lcp = (struct log_ctl *)bp->b_rptr;
		if (lcp->flags & SL_ERROR) {
			if (numlogerr == 0) {
				lcp->flags &= ~SL_ERROR;
			} else {
				log_errseq++;
			}
		}
		if (lcp->flags & SL_TRACE) {
			if ((numlogtrc == 0) || !shouldtrace(LOG_MID,
			    (short)((struct log *)(q->q_ptr) - log_log),
				lcp->level)) {
				lcp->flags &= ~SL_TRACE;
			} else {
				log_trcseq++;
			}
		}
		if (!(lcp->flags & (SL_ERROR|SL_TRACE|SL_CONSOLE))) {
			freemsg(bp);
			break;
		}

		(void) drv_getparm(LBOLT, &c);
		lcp->ltime = c;
		(void) drv_getparm(TIME, &t);
		lcp->ttime = t;
		lcp->mid = LOG_MID;
		lcp->sid = (short)((struct log *)q->q_ptr - log_log);
		if (lcp->flags & SL_TRACE) {
			(void) log_internal(bp, log_trcseq, LOGTRC);
		}
		if (lcp->flags & SL_ERROR) {
			(void) log_internal(bp, log_errseq, LOGERR);
		}
		if (lcp->flags & SL_CONSOLE) {
			log_conseq++;
			if ((lcp->pri & LOG_FACMASK) == LOG_KERN)
				lcp->pri |= LOG_USER;
			(void) log_internal(bp, log_conseq, LOGCONS);
		}
		freemsg(bp);
		break;

	case M_DATA:
		if (lp != &log_log[CONSWMIN]) {
			bp->b_datap->db_type = M_ERROR;
			if (bp->b_cont) {
				freemsg(bp->b_cont);
				bp->b_cont = NULL;
			}
			bp->b_rptr = bp->b_datap->db_base;
			bp->b_wptr = bp->b_rptr + sizeof (char);
			*bp->b_rptr = EIO;
			qreply(q, bp);
			break;
		}

		/*
		 * allocate message block for proto
		 */
		if (!(cbp = allocb(sizeof (struct log_ctl), BPRI_HI))) {
			freemsg(bp);
			break;
		}
		cbp->b_datap->db_type = M_PROTO;
		cbp->b_cont = bp;
		cbp->b_wptr += sizeof (struct log_ctl);
		lcp = (struct log_ctl *)cbp->b_rptr;
		lcp->mid = LOG_MID;
		lcp->sid = CONSWMIN;
		(void) drv_getparm(LBOLT, &c);
		lcp->ltime = c;
		(void) drv_getparm(TIME, &t);
		lcp->ttime = t;
		lcp->level = 0;
		lcp->flags = SL_CONSOLE;
		lcp->pri = LOG_USER|LOG_INFO;
		log_conseq++;
		(void) log_internal(cbp, log_conseq, LOGCONS);
		freemsg(cbp);
		break;

	default:
		freemsg(bp);
		break;
	}
	return (0);
}

/*
 * Send a log message up a given log stream.
 */
static int
logrsrv(queue_t *q)
{
	mblk_t *mp;

	if (q == log_d_q) /* dedicated Q ? */
		/*
		 * put the message on the queue(s) corresponding to
		 * the interested readers.
		 */
		return (process_msg(q));

	while (mp = getq(q)) {
		if (!canput(q->q_next)) {
			(void) putbq(q, mp);
			break;
		}
		putnext(q, mp);
	}
	return (0);
}


/*
 * Check mid, sid, and level against list of values requested by
 * processes reading trace messages.
 */
static int
logtrace(struct log *lp, short mid, short sid, signed char level)
{
	struct trace_ids *tid;
	int i;
	int ntid;

	ASSERT(lp->log_tracemp);
	tid = (struct trace_ids *)lp->log_tracemp->b_rptr;
	ntid = (intptr_t)(lp->log_tracemp->b_wptr - lp->log_tracemp->b_rptr) /
	    sizeof (struct trace_ids);
	for (i = 0; i < ntid; tid++, i++) {
		if (((signed char)tid->ti_level < level) &&
		    ((signed char)tid->ti_level >= 0))
			continue;
		if ((tid->ti_mid != mid) && (tid->ti_mid >= 0))
			continue;
		if ((tid->ti_sid != sid) && (tid->ti_sid >= 0))
			continue;
		return (1);
	}
	return (0);
}

/*
 * Returns 1 if someone wants to see the trace message for the
 * given module id, sub-id, and level.  Returns 0 otherwise.
 */
int
shouldtrace(short mid, short sid, char level)
{
	struct log *lp;
	int i;

	i = CLONEMIN + 1;
	mutex_enter(&log_lock);
	for (lp = &log_log[i]; i < log_cnt; i++, lp++)
		if ((lp->log_state & LOGTRC) && logtrace(lp, mid, sid,
		    (signed char)level)) {
			mutex_exit(&log_lock);
			return (1);
		}
	mutex_exit(&log_lock);
	return (0);
}

/*
 * Send a log message to a reader.  Returns 1 if the
 * message was sent and 0 otherwise. The caller has incremented
 * the ref count (ie. lp->log_refcnt) before dropping the log_lock
 * mutex so that a close on the log device will have to wait and
 * get a wakeup call from the caller when the ref count is decremented
 * to zero. Since the log_lock mutex is being dropped there is a chance
 * that that sequence numbers can appear out of order when reading
 * from the log driver. But this condition also exists with the existing
 * implementation when several threads contend for the log_lock mutex
 * to send out the log messages and one gets the lock indeterminately.
 */
int
log_sendmsg(struct log	*lp, mblk_t *mp)
{
	mblk_t		*mp2;

	if ((mp2 = dupmsg(mp)) != NULL) {
		while (!canput(lp->log_rdq)) {
			/*
			 * Try to keep the q from getting too full.
			 * It is OK to drop messages in busy
			 * conditions. Get in the most recent message
			 * by trimming the q size down to below the
			 * low-water mark.
			 */
			freemsg(getq(lp->log_rdq));
		}
		(void) putq(lp->log_rdq, mp2);
		return (1);
	}
	return (0);
}

/*
 * This is a driver internal function to send messages to the appropriate
 * reader depending on the type of log.
 *
 * Log a trace/error/console message.  Returns 1 if everyone sees the message
 * and 0 otherwise.
 */
int
log_internal(
	mblk_t			*mp,
	int			seq_no,
	int			type_flag)	/* what type of trace */
{
	int			i;
	struct log		*lp;
	mblk_t			*bp;
	struct log_ctl		*lcp;
	int			nlog = 0;
	int			didsee = 0;

	bp = mp;
	lcp = (struct log_ctl *)bp->b_rptr;

	lcp->seq_no = seq_no;
	i = CLONEMIN + 1;

	mutex_enter(&log_lock);
	for (lp = &log_log[i]; i < log_cnt; i++, lp++)
		if ((lp->log_state & type_flag) &&
		    (type_flag != LOGTRC || logtrace(lp, lcp->mid, lcp->sid,
		    (signed char)lcp->level))) {
			nlog++;
			++lp->log_refcnt;
			mutex_exit(&log_lock);
			didsee += log_sendmsg(lp, bp);
			mutex_enter(&log_lock);
			if ((--lp->log_refcnt == 0) &&
			    (lp->log_state & LOGCLOSE))
				cv_broadcast(&lp->log_cv);
		}

	mutex_exit(&log_lock);

	if (! didsee && (type_flag & LOGCONS) &&
	    ((lcp->pri & LOG_FACMASK) == LOG_KERN))
		for (mp = mp->b_cont; mp; mp = mp->b_cont)
			msgbuf_puts((caddr_t)mp->b_rptr);

	return ((nlog == didsee) ? 1 : 0);
}

/*
 * Originally this functionality was in strlog and has been moved to the
 * dedicated Q of the log driver to get better performance for strlog.
 */
static int
process_msg(queue_t *q)
{
	mblk_t *mp;
	struct log_ctl *lcp;

	while ((mp = getq(q)) != NULL) {
		lcp = (struct log_ctl *)mp->b_rptr;

		if (!(lcp->flags & (SL_ERROR|SL_TRACE|SL_CONSOLE))) {
			freemsg(mp);
			return (0);
		}

		if (lcp->flags & SL_ERROR) {
			if (numlogerr == 0)
				lcp->flags &= ~SL_ERROR;
			else
				log_errseq++;
		}

		if (lcp->flags & SL_TRACE) {
			if ((numlogtrc == 0) || !shouldtrace(lcp->mid, lcp->sid,
								lcp->level))
				lcp->flags &= ~SL_TRACE;
			else
				log_trcseq++;
		}

		if (lcp->flags & SL_TRACE) {
			lcp->pri = LOG_KERN|LOG_DEBUG;
			(void) log_internal(mp, log_trcseq, LOGTRC);
		}
		if (lcp->flags & SL_ERROR) {
			lcp->pri = LOG_KERN|LOG_ERR;
			(void) log_internal(mp, log_errseq, LOGERR);
		}
		if (lcp->flags & SL_CONSOLE) {
			log_conseq++;
			if (lcp->flags & SL_FATAL)
				lcp->pri = LOG_KERN|LOG_CRIT;
			else if (lcp->flags & SL_ERROR)
				lcp->pri = LOG_KERN|LOG_ERR;
			else if (lcp->flags & SL_WARN)
				lcp->pri = LOG_KERN|LOG_WARNING;
			else if (lcp->flags & SL_NOTE)
				lcp->pri = LOG_KERN|LOG_NOTICE;
			else if (lcp->flags & SL_TRACE)
				lcp->pri = LOG_KERN|LOG_DEBUG;
			else
				lcp->pri = LOG_KERN|LOG_INFO;
			(void) log_internal(mp, log_conseq, LOGCONS);
		}
		freemsg(mp);
	}
	return (0);
}
