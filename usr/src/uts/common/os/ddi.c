/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ddi.c	1.91	97/10/22 SMI"	/* from SVr4.0 1.39 */

/*
 * UNIX Device Driver Interface functions
 *
 * This file contains functions that are to be added to the kernel
 * to put the interface presented to drivers in conformance with
 * the DDI standard. Of the functions added to the kernel, 17 are
 * function equivalents of existing macros in sysmacros.h, map.h,
 * stream.h, and param.h
 *
 * 17 additional functions -- drv_getparm(), drv_setparm(), setmapwant(),
 * getrbuf(), freerbuf(),
 * getemajor(), geteminor(), etoimajor(), itoemajor(), drv_usectohz(),
 * drv_hztousec(), drv_usecwait(), drv_priv(), and kvtoppid() --
 * are specified by DDI to exist in the kernel and are implemented here.
 *
 * Note that putnext() and put() are not in this file. The C version of
 * these routines are in uts/common/os/putnext.c and assembly versions
 * might exist for some architectures.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/cpuvar.h>
#include <sys/map.h>
#include <sys/signal.h>
#include <sys/pcb.h>
#include <sys/user.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/cmn_err.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/poll.h>
#include <sys/session.h>
#include <sys/ddi.h>
#include <sys/mkdev.h>
#include <sys/debug.h>
#include <sys/vtrace.h>

/*
 * local version of RD(), WR(), SAMESTR()
 */
#define	_RD(qp)	((qp)-1)
#define	_WR(qp)	((qp)+1)
#define	_SAMESTR(q)	(!((q)->q_flag & QEND))

/*
 * return internal major number corresponding to device
 * number (new format) argument
 */
major_t
getmajor(dev_t dev)
{
#ifdef _LP64
	return ((major_t)((dev >> NBITSMINOR64) & MAXMAJ64));
#else
	return ((major_t)((dev >> NBITSMINOR) & MAXMAJ));
#endif
}

/*
 * return external major number corresponding to device
 * number (new format) argument
 */
major_t
getemajor(dev_t dev)
{
#ifdef _LP64
	return ((major_t)((dev >> NBITSMINOR64) & MAXMAJ64));
#else
	return ((major_t)((dev >> NBITSMINOR) & MAXMAJ));
#endif
}

/*
 * return internal minor number corresponding to device
 * number (new format) argument
 */
minor_t
getminor(dev_t dev)
{
#ifdef _LP64
	return ((minor_t)(dev & MAXMIN64));
#else
	return ((minor_t)(dev & MAXMIN));
#endif
}

/*
 * return external minor number corresponding to device
 * number (new format) argument
 */
minor_t
geteminor(dev_t dev)
{
#ifdef _LP64
	return ((minor_t)(dev & MAXMIN64));
#else
	return ((minor_t)(dev & MAXMIN));
#endif
}

/*
 * return internal major number corresponding to external
 * major number.
 */
int
etoimajor(major_t emajnum)
{
#ifdef _LP64
	if (emajnum > MAXMAJ64 || emajnum >= devcnt)
		return (-1); /* invalid external major */
#else
	if (emajnum > MAXMAJ || emajnum >= devcnt)
		return (-1); /* invalid external major */
#endif
	return ((int)emajnum);
}

/*
 * return external major number corresponding to internal
 * major number argument or -1 if no external major number
 * can be found after lastemaj that maps to the internal
 * major number. Pass a lastemaj val of -1 to start
 * the search initially. (Typical use of this function is
 * of the form:
 *
 *	lastemaj = -1;
 *	while ((lastemaj = itoemajor(imag, lastemaj)) != -1)
 *		{ process major number }
 */
int
itoemajor(major_t imajnum, int lastemaj)
{
	if (imajnum >= devcnt)
		return (-1);

	/*
	 * if lastemaj == -1 then start from beginning of
	 * the (imaginary) MAJOR table
	 */
	if (lastemaj < -1)
		return (-1);

	/*
	 * given that there's a 1-1 mapping of internal to external
	 * major numbers, searching is somewhat pointless ... let's
	 * just go there directly.
	 */
	if (++lastemaj < devcnt && imajnum < devcnt)
		return (imajnum);
	return (-1);
}

/*
 * encode external major and minor number arguments into a
 * new format device number
 */
dev_t
makedevice(major_t maj, minor_t minor)
{
#ifdef _LP64
	return (((dev_t)maj << NBITSMINOR64) | (minor & MAXMIN64));
#else
	return (((dev_t)maj << NBITSMINOR) | (minor & MAXMIN));
#endif
}

/*
 * cmpdev - compress new device format to old device format
 */
o_dev_t
cmpdev(dev_t dev)
{
	major_t major_d;
	minor_t minor_d;

#ifdef _LP64
	major_d = dev >> NBITSMINOR64;
	minor_d = dev & MAXMIN64;
#else
	major_d = dev >> NBITSMINOR;
	minor_d = dev & MAXMIN;
#endif
	if (major_d > OMAXMAJ || minor_d > OMAXMIN)
		return ((o_dev_t)NODEV);
	return ((o_dev_t)((major_d << ONBITSMINOR) | minor_d));
}

dev_t
expdev(dev_t dev)
{
	major_t major_d;
	minor_t minor_d;

	major_d = ((dev >> ONBITSMINOR) & OMAXMAJ);
	minor_d = (dev & OMAXMIN);
#ifdef _LP64
	return ((((dev_t)major_d << NBITSMINOR64) | minor_d));
#else
	return ((((dev_t)major_d << NBITSMINOR) | minor_d));
#endif
}

/*
 * return true (1) if the message type input is a data
 * message type, 0 otherwise
 */
#undef datamsg
int
datamsg(unsigned char db_type)
{
	return (db_type == M_DATA || db_type == M_PROTO ||
		db_type == M_PCPROTO || db_type == M_DELAY);
}

/*
 * return a pointer to the other queue in the queue pair of qp
 */
queue_t *
OTHERQ(queue_t *q)
{
	return ((q)->q_flag & QREADR ? (q) + 1 : (q) - 1);
}

/*
 * return a pointer to the read queue in the queue pair of qp.
 */
queue_t *
RD(queue_t *q)
{
	if (q->q_flag & QREADR)
		return (q);
	else
		return (q - 1);
}

/*
 * return a pointer to the write queue in the queue pair of qp.
 */
int
SAMESTR(queue_t *q)
{
	return (_SAMESTR(q));
}

/*
 * return a pointer to the write queue in the queue pair of qp.
 */
queue_t *
WR(queue_t *q)
{
	if (q->q_flag & QREADR)
		return (q + 1);
	else
		return (q);
}

/*
 * store value of kernel parameter associated with parm
 */
int
drv_getparm(unsigned int parm, void *valuep)
{
	switch (parm) {
	case UPROCP:
		*(proc_t **)valuep = ttoproc(curthread);
		break;
	case PPGRP:
		*(pid_t *)valuep = ttoproc(curthread)->p_pgrp;
		break;
	case LBOLT:
		*(clock_t *)valuep = lbolt;
		break;
	case TIME:
		if (hrestime.tv_sec == 0) {
			timestruc_t ts;
			mutex_enter(&tod_lock);
			ts = tod_get();
			mutex_exit(&tod_lock);
			*(time_t *)valuep = ts.tv_sec;
		} else {
			*(time_t *)valuep = hrestime.tv_sec;
		}
		break;
	case PPID:
		*(pid_t *)valuep = ttoproc(curthread)->p_pid;
		break;
	case PSID:
		*(pid_t *)valuep = ttoproc(curthread)->p_sessp->s_sid;
		break;
	case UCRED:
		*(cred_t **)valuep = CRED();
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * set value of kernel parameter associated with parm
 */
int
drv_setparm(unsigned int parm, unsigned long value)
{
	switch (parm) {
	case SYSRINT:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.rcvint, value);
		break;
	case SYSXINT:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.xmtint, value);
		break;
	case SYSMINT:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.mdmint, value);
		break;
	case SYSRAWC:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.rawch, value);
		break;
	case SYSCANC:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.canch, value);
		break;
	case SYSOUTC:
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.outch, value);
		break;
	default:
		return (-1);
	}

	return (0);
}

/*
 * allocate space for buffer header and return pointer to it.
 * preferred means of obtaining space for a local buf header.
 * returns pointer to buf upon success, NULL for failure
 */
struct buf *
getrbuf(int sleep)
{
	struct buf *bp;

	bp = kmem_alloc(sizeof (struct buf), sleep);
	if (bp == NULL)
		return (NULL);
	bioinit(bp);

	return (bp);
}

/*
 * free up space allocated by getrbuf()
 */
void
freerbuf(struct buf *bp)
{
	biofini(bp);
	kmem_free(bp, sizeof (struct buf));
}

/*
 * convert byte count input to logical page units
 * (byte counts that are not a page-size multiple
 * are rounded down)
 */
pgcnt_t
btop(size_t numbytes)
{
	return (numbytes >> PAGESHIFT);
}

/*
 * convert byte count input to logical page units
 * (byte counts that are not a page-size multiple
 * are rounded up)
 */
pgcnt_t
btopr(size_t numbytes)
{
	return ((numbytes + PAGEOFFSET) >> PAGESHIFT);
}

/*
 * convert size in pages to bytes.
 */
size_t
ptob(pgcnt_t numpages)
{
	return (numpages << PAGESHIFT);
}

#define	MAXCLOCK_T LONG_MAX

/*
 * Convert from system time units (hz) to microseconds.
 *
 * If ticks <= 0, return 0.
 * If converting ticks to usecs would overflow, return MAXCLOCK_T.
 * Otherwise, convert ticks to microseconds.
 */
clock_t
drv_hztousec(clock_t ticks)
{
	if (ticks <= 0)
		return (0);

	if (ticks > MAXCLOCK_T / usec_per_tick)
		return (MAXCLOCK_T);

	return (TICK_TO_USEC(ticks));
}


/*
 * Convert from microseconds to system time units (hz), rounded up.
 *
 * If ticks <= 0, return 0.
 * Otherwise, convert microseconds to ticks, rounding up.
 */
clock_t
drv_usectohz(clock_t microsecs)
{
	if (microsecs <= 0)
		return (0);

	return (USEC_TO_TICK_ROUNDUP(microsecs));
}

/*
 * determine if the supplied credentials identify a privileged
 * process.  To be used only when file access modes and
 * special minor device numbers are insufficient to provide
 * protection for the requested driver function.  Returns 0
 * if the privilege is granted, otherwise EPERM.
 */
int
drv_priv(cred_t *cr)
{
	return ((cr->cr_uid == 0) ? 0 : EPERM);
}


#ifdef	sun
/*
 * drv_usecwait implemented in each architecture's machine
 * specific code somewhere. For sparc, it is the alternate entry
 * to usec_delay (eventually usec_delay goes away). See
 * sparc/os/ml/sparc_subr.s
 */
#endif

/*
 * bcanputnext, canputnext assume called from timeout, bufcall,
 * or esballoc free routines.  since these are driven by
 * clock interrupts, instead of system calls the appropriate plumbing
 * locks have not been acquired.
 */
int
bcanputnext(queue_t *q, unsigned char band)
{
	int	ret;

	claimstr(q);
	ret = bcanput(q->q_next, band);
	releasestr(q);
	return (ret);
}

int
canputnext(queue_t *q)
{
	queue_t	*qofsq = q;
	struct stdata *stp = STREAM(q);

	TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_IN,
	    "canputnext?:%s(%p)\n", QNAME(q), q);

	mutex_enter(&stp->sd_reflock);

	/* get next module forward with a service queue */
	q = q->q_next->q_nfsrv;
	ASSERT(q != NULL);

	/* this is for loopback transports, they should not do a canputnext */
	ASSERT(STRMATED(q->q_stream) || STREAM(q) == STREAM(qofsq));

	if (!(q->q_flag & QFULL)) {
		mutex_exit(&stp->sd_reflock);
		TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%p %d", q, 1);
		return (1);
	}

	/* the above is the most frequently used path */
	stp->sd_refcnt++;
	ASSERT(stp->sd_refcnt != 0);	/* Wraparound */
	mutex_exit(&stp->sd_reflock);

	mutex_enter(QLOCK(q));
	if (q->q_flag & QFULL) {
		q->q_flag |= QWANTW;
		mutex_exit(QLOCK(q));
		TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%p %d", q, 0);
		releasestr(qofsq);

		return (0);
	}
	mutex_exit(QLOCK(q));
	TRACE_2(TR_FAC_STREAMS_FR, TR_CANPUT_OUT, "canput:%p %d", q, 1);
	releasestr(qofsq);

	return (1);
}

int
putnextctl1(queue_t *q, int type, int param)
{
	int	ret;

	claimstr(q);
	ret = putctl1(q->q_next, type, param);
	releasestr(q);
	return (ret);
}

int
putnextctl(queue_t *q, int type)
{
	int	ret;

	claimstr(q);
	ret = putctl(q->q_next, type);
	releasestr(q);
	return (ret);
}

/*
 * Open has progressed to the point where it is safe to send/receive messages.
 *
 * "qprocson enables the put and service routines of the driver
 * or module... Prior to the call to qprocson, the put and service
 * routines of a newly pushed module or newly opened driver are
 * disabled.  For the module, messages flow around it as if it
 * were not present in the stream... qprocson must be called by
 * the first open of a module or driver after allocation and
 * initialization of any resource on which the put and service
 * routines depend."
 *
 * Note that before calling qprocson a module/driver could itself cause its
 * put or service procedures to be run by using put() or qenable().
 */
void
qprocson(queue_t *q)
{
	ASSERT(q->q_flag & QREADR);
	/*
	 * could be re-open
	 */
	if (q->q_next == NULL && WR(q)->q_next == NULL)
		insertq(STREAM(q), q);
}

/*
 * Close has reached a point where it can no longer allow put/service
 * into the queue.
 *
 * "qprocsoff disables the put and service routines of the driver
 * or module... When the routines are disabled in a module, messages
 * flow around the module as if it were not present in the stream.
 * qprocsoff must be called by the close routine of a driver or module
 * before deallocating any resources on which the driver/module's
 * put and service routines depend.  qprocsoff will remove the
 * queue's service routines from the list of service routines to be
 * run and waits until any concurrent put or service routines are
 * finished."
 *
 * Note that after calling qprocsoff a module/driver could itself cause its
 * put procedures to be run by using put().
 */
void
qprocsoff(queue_t *q)
{
	ASSERT(q->q_flag & QREADR);
	if (q->q_flag & QWCLOSE) {
		/* Called more than once */
		return;
	}
	disable_svc(q);
	removeq(q);
}

/*
 * "freezestr() freezes the state of the entire STREAM  containing
 *  the  queue  pair  q.  A frozen STREAM blocks any thread
 *  attempting to enter any open, close, put or service  routine
 *  belonging  to  any  queue instance in the STREAM, and blocks
 *  any thread currently within the STREAM if it attempts to put
 *  messages  onto  or take messages off of any queue within the
 *  STREAM (with the sole exception  of  the  caller).   Threads
 *  blocked  by  this  mechanism  remain  so until the STREAM is
 *  thawed by a call to unfreezestr().
 *
 * Use strblock to set SQ_FROZEN in all syncqs in the stream (prevents
 * further entry into put, service, open, and close procedures) and
 * grab (and hold) all the QLOCKs in the stream (to block putq, getq etc.)
 *
 * Note: this has to be the only code that acquires one QLOCK while holding
 * another QLOCK (otherwise we would have locking hirarchy/ordering violations.)
 */
void
freezestr(queue_t *q)
{
	struct stdata *stp = STREAM(q);

	/*
	 * Increment refcnt to prevent q_next from changing during the strblock
	 * as well as while the stream is frozen.
	 */
	claimstr(RD(q));

	strblock(q);
	ASSERT(stp->sd_freezer == NULL);
	stp->sd_freezer = curthread;
	for (q = stp->sd_wrq; q != NULL; q = SAMESTR(q) ? q->q_next : NULL) {
		mutex_enter(QLOCK(q));
		mutex_enter(QLOCK(RD(q)));
	}
}

/*
 * Undo what freezestr did.
 * Have to drop the QLOCKs before the strunblock since strunblock will
 * potentially call other put procedures.
 */
void
unfreezestr(queue_t *q)
{
	struct stdata *stp = STREAM(q);
	queue_t	*q1;

	for (q1 = stp->sd_wrq; q1 != NULL;
	    q1 = SAMESTR(q1) ? q1->q_next : NULL) {
		mutex_exit(QLOCK(q1));
		mutex_exit(QLOCK(RD(q1)));
	}
	ASSERT(stp->sd_freezer == curthread);
	stp->sd_freezer = NULL;
	strunblock(q);
	releasestr(RD(q));
}

/*
 * Used by open and close procedures to "sleep" waiting for messages to
 * arrive. Note: can only be used in open and close procedures.
 *
 * Lower the gate and let in either messages on the syncq (if there are
 * any) or put/service procedures.
 *
 * If the queue has an outer perimeter this will not prevent entry into this
 * syncq (since outer_enter does not set SQ_WRITER on the syncq that gets the
 * exclusive access to the outer perimeter.)
 *
 * Return 0 is the cv_wait_sig was interrupted; otherwise 1.
 */
int
qwait_sig(queue_t *q)
{
	syncq_t		*sq, *outer;
	u_int		flags;
	int		ret = 1;

	/*
	 * Perform the same operations as a leavesq(sq, SQ_OPENCLOSE)
	 * while detecting all cases where the perimeter is entered
	 * so that qwait_sig can return to the caller.
	 *
	 * Drain the syncq if possible. Otherwise reset SQ_EXCL and
	 * wait for a thread to leave the syncq.
	 */
	sq = q->q_syncq;
	ASSERT(sq);
	ASSERT(sq->sq_outer == NULL || sq->sq_outer->sq_flags & SQ_WRITER);
	outer = sq->sq_outer;
	/*
	 * XXX this does not work if there is only an outer perimeter.
	 * The semantics of qwait/qwait_sig are undefined in this case.
	 */
	if (outer)
		outer_exit(outer);

	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	/*
	 * Drop SQ_EXCL and sq_count/sq_occount but hold the SQLOCK
	 * to prevent any undetected entry and exit into the perimeter.
	 */
	ASSERT(sq->sq_occount <= sq->sq_count);
	ASSERT(sq->sq_count > 0);
	sq->sq_count--;
	ASSERT(sq->sq_occount > 0);
	sq->sq_occount--;

	if (!(sq->sq_type & SQ_CIOC)) {
		ASSERT(flags & SQ_EXCL);
		flags &= ~SQ_EXCL;
	}
	/*
	 * Unblock any thread blocked in an entersq or outer_enter.
	 * Note: we do not unblock a thread waiting in qwait/qwait_sig,
	 * since that could lead to livelock with two threads in
	 * qwait for the same (per module) inner perimeter.
	 */
	if (flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		flags &= ~SQ_WANTWAKEUP;
	}
	sq->sq_flags = flags;
	if ((flags & SQ_QUEUED) && !(flags & SQ_STAYAWAY)) {
		drain_syncq(sq);
		mutex_exit(SQLOCK(sq));
		entersq(sq, SQ_OPENCLOSE);
		return (1);
	}
	/*
	 * Sleep on sq_exitwait to only be woken up when threads leave the
	 * put or service procedures. We can not sleep on sq_wait since an
	 * outer_exit in a qwait running in the same outer perimeter would
	 * cause a livelock "ping-pong" between two or more qwait'ers.
	 */
	do {
		sq->sq_flags |= SQ_WANTEXWAKEUP;
		ret = cv_wait_sig(&sq->sq_exitwait, SQLOCK(sq));
	} while (ret && (sq->sq_flags & SQ_WANTEXWAKEUP));
	mutex_exit(SQLOCK(sq));

	/*
	 * Re-enter the perimeters again
	 */
	entersq(sq, SQ_OPENCLOSE);
	return (ret);
}

/*
 * Used by open and close procedures to "sleep" waiting for messages to
 * arrive. Note: can only be used in open and close procedures.
 *
 * Lower the gate and let in either messages on the syncq (if there are
 * any) or put/service procedures.
 *
 * If the queue has an outer perimeter this will not prevent entry into this
 * syncq (since outer_enter does not set SQ_WRITER on the syncq that gets the
 * exclusive access to the outer perimeter.)
 */
void
qwait(queue_t *q)
{
	syncq_t		*sq, *outer;
	u_int		flags;

	/*
	 * Perform the same operations as a leavesq(sq, SQ_OPENCLOSE)
	 * while detecting all cases where the perimeter is entered
	 * so that qwait can return to the caller.
	 *
	 * Drain the syncq if possible. Otherwise reset SQ_EXCL and
	 * wait for a thread to leave the syncq.
	 */
	sq = q->q_syncq;
	ASSERT(sq);
	ASSERT(sq->sq_outer == NULL || sq->sq_outer->sq_flags & SQ_WRITER);
	outer = sq->sq_outer;
	/*
	 * XXX this does not work if there is only an outer perimeter.
	 * The semantics of qwait/qwait_sig are undefined in this case.
	 */
	if (outer)
		outer_exit(outer);

	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	/*
	 * Drop SQ_EXCL and sq_count/sq_occount but hold the SQLOCK
	 * to prevent any undetected entry and exit into the perimeter.
	 */
	ASSERT(sq->sq_occount <= sq->sq_count);
	ASSERT(sq->sq_count > 0);
	sq->sq_count--;
	ASSERT(sq->sq_occount > 0);
	sq->sq_occount--;

	if (!(sq->sq_type & SQ_CIOC)) {
		ASSERT(flags & SQ_EXCL);
		flags &= ~SQ_EXCL;
	}
	/*
	 * Unblock any thread blocked in an entersq or outer_enter.
	 * Note: we do not unblock a thread waiting in qwait/qwait_sig,
	 * since that could lead to livelock with two threads in
	 * qwait for the same (per module) inner perimeter.
	 */
	if (flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		flags &= ~SQ_WANTWAKEUP;
	}
	sq->sq_flags = flags;
	if ((flags & SQ_QUEUED) && !(flags & SQ_STAYAWAY)) {
		drain_syncq(sq);
		mutex_exit(SQLOCK(sq));
		entersq(sq, SQ_OPENCLOSE);
		return;
	}
	/*
	 * Sleep on sq_exitwait to only be woken up when threads leave the
	 * put or service procedures. We can not sleep on sq_wait since an
	 * outer_exit in a qwait running in the same outer perimeter would
	 * cause a livelock "ping-pong" between two or more qwait'ers.
	 */
	do {
		sq->sq_flags |= SQ_WANTEXWAKEUP;
		cv_wait(&sq->sq_exitwait, SQLOCK(sq));
	} while (sq->sq_flags & SQ_WANTEXWAKEUP);
	mutex_exit(SQLOCK(sq));

	/*
	 * Re-enter the perimeters again
	 */
	entersq(sq, SQ_OPENCLOSE);
}

/*
 * Used for the synchronous streams entrypoints when sleeping outside
 * the perimeters.
 * Consolidation private.
 */
void
qwait_rw(queue_t *q)
{
	syncq_t		*sq;
	u_long		flags;

	/*
	 * Perform the same operations as a leavesq(sq, SQ_PUT)
	 * while detecting all cases where the perimeter is entered
	 * so that qwait_rw can return to the caller.
	 *
	 * Drain the syncq if possible. Otherwise reset SQ_EXCL and
	 * wait for a thread to leave the syncq.
	 */
	sq = q->q_syncq;
	ASSERT(sq);

	mutex_enter(SQLOCK(sq));
	flags = sq->sq_flags;
	/*
	 * Drop SQ_EXCL and sq_count but hold the SQLOCK until to prevent any
	 * undetected entry and exit into the perimeter.
	 */
	ASSERT(sq->sq_count > 0);
	sq->sq_count--;
	if (!(sq->sq_type & SQ_CIPUT)) {
		ASSERT(flags & SQ_EXCL);
		flags &= ~SQ_EXCL;
	}
	/*
	 * Unblock any thread blocked in an entersq or outer_enter.
	 * Note: we do not unblock a thread waiting in qwait/qwait_sig,
	 * since that could lead to livelock with two threads in
	 * qwait for the same (per module) inner perimeter.
	 */
	if (flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		flags &= ~SQ_WANTWAKEUP;
	}
	sq->sq_flags = flags;
	if ((flags & SQ_QUEUED) && !(flags & SQ_STAYAWAY)) {
		drain_syncq(sq);
		mutex_exit(SQLOCK(sq));
		entersq(sq, SQ_PUT);
		return;
	}
	/*
	 * Sleep on sq_exitwait to only be woken up when threads leave the
	 * put or service procedures. We can not sleep on sq_wait since an
	 * outer_exit in a qwait running in the same outer perimeter would
	 * cause a livelock "ping-pong" between two or more qwait'ers.
	 */
	do {
		sq->sq_flags |= SQ_WANTEXWAKEUP;
		cv_wait(&sq->sq_exitwait, SQLOCK(sq));
	} while (sq->sq_flags & SQ_WANTEXWAKEUP);
	mutex_exit(SQLOCK(sq));

	/*
	 * Re-enter the perimeters again
	 */
	entersq(sq, SQ_PUT);
}

/*
 * Asynchronously upgrade to exclusive access at either the inner or
 * outer perimeter.
 */
void
qwriter(queue_t *q, mblk_t *mp, void (*func)(), int perim)
{
	if (perim == PERIM_INNER)
		qwriter_inner(q, mp, func);
	else if (perim == PERIM_OUTER)
		qwriter_outer(q, mp, func);
	else
		panic("qwriter: wrong \"perimeter\" parameter");
}

/*
 * Schedule a synchronous streams timeout
 */
timeout_id_t
qtimeout(queue_t *q, void (*func)(void *), void *arg, clock_t tim)
{
	syncq_t		*sq;
	callbparams_t	*cbp;
	timeout_id_t	tid;

	sq = q->q_syncq;
	/* you don't want the timeout firing before its params are set up */
	cbp = callbparams_alloc(sq, func, arg);	/* acquires SQLOCK(sq) */
	/*
	 * the callbflags in the sq use the same flags. They get anded
	 * in the callbwrapper to determine if a qun* of this callback type
	 * is required. This is not a request to cancel.
	 */
	cbp->cbp_flags = SQ_CANCEL_TOUT;
	/* check new timeout version return codes */
	tid = timeout(qcallbwrapper, cbp, tim);
	cbp->cbp_id = (callbparams_id_t)tid;
	mutex_exit(SQLOCK(sq));
	/* use local id because the cbp memory could be free by now */
	return (tid);
}

bufcall_id_t
qbufcall(queue_t *q, size_t size, uint pri, void (*func)(void *), void *arg)
{
	syncq_t		*sq;
	callbparams_t	*cbp;
	bufcall_id_t	bid;

	sq = q->q_syncq;
	/* you don't want the timeout firing before its params are set up */
	cbp = callbparams_alloc(sq, func, arg);	/* acquires SQLOCK(sq) */
	/*
	 * the callbflags in the sq use the same flags. They get anded
	 * in the callbwrapper to determine if a qun* of this callback type
	 * is required. This is not a request to cancel.
	 */
	cbp->cbp_flags = SQ_CANCEL_BUFCALL;
	/* check new timeout version return codes */
	bid = bufcall(size, pri, qcallbwrapper, cbp);
	cbp->cbp_id = (callbparams_id_t)bid;
	if (bid == 0) {
		callbparams_free(sq, cbp);
	}
	mutex_exit(SQLOCK(sq));
	/* use local id because the params memory could be free by now */
	return (bid);
}

/*
 * cancel a timeout callback which enters the inner perimeter.
 * cancelling of all callback types on a given syncq is serialized.
 * the SQ_CALLB_BYPASSED flag indicates that the callback fn did
 * not execute. The quntimeout return value needs to reflect this.
 * As with out existing callback programming model - callbacks must
 * be cancelled before a close completes - so ensuring that the sq
 * is valid when the callback wrapper is executed.
 */
clock_t
quntimeout(queue_t *q, timeout_id_t id)
{
	syncq_t *sq = q->q_syncq;
	clock_t ret;

	mutex_enter(SQLOCK(sq));
	/* callbacks are processed serially on each syncq */
	while (sq->sq_callbflags & SQ_CALLB_CANCEL_MASK) {
		sq->sq_flags |= SQ_WANTWAKEUP;
		cv_wait(&sq->sq_wait, SQLOCK(sq));
	}
	sq->sq_cancelid = (callbparams_id_t)id;
	sq->sq_callbflags = SQ_CANCEL_TOUT;
	if (sq->sq_flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		sq->sq_flags &= ~SQ_WANTWAKEUP;
	}
	mutex_exit(SQLOCK(sq));
	ret = untimeout(id);
	mutex_enter(SQLOCK(sq));
	if (ret != -1) {
		/* The wrapper was never called - need to free based on id */
		callbparams_free_id(sq, (callbparams_id_t)id, SQ_CANCEL_TOUT);
	}
	if (sq->sq_callbflags & SQ_CALLB_BYPASSED) {
		ret = 0;	/* this was how much time left */
	}
	sq->sq_callbflags = 0;
	if (sq->sq_flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		sq->sq_flags &= ~SQ_WANTWAKEUP;
	}
	mutex_exit(SQLOCK(sq));
	return (ret);
}


void
qunbufcall(queue_t *q, bufcall_id_t id)
{
	syncq_t *sq = q->q_syncq;

	mutex_enter(SQLOCK(sq));
	/* callbacks are processed serially on each syncq */
	while (sq->sq_callbflags & SQ_CALLB_CANCEL_MASK) {
		sq->sq_flags |= SQ_WANTWAKEUP;
		cv_wait(&sq->sq_wait, SQLOCK(sq));
	}
	sq->sq_cancelid = (callbparams_id_t)id;
	sq->sq_callbflags = SQ_CANCEL_BUFCALL;
	if (sq->sq_flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		sq->sq_flags &= ~SQ_WANTWAKEUP;
	}
	mutex_exit(SQLOCK(sq));
	unbufcall(id);
	mutex_enter(SQLOCK(sq));
	/*
	 * No indication from unbufcall if the callback has already run.
	 * Always attempt to free it.
	 */
	callbparams_free_id(sq, (callbparams_id_t)id, SQ_CANCEL_BUFCALL);
	sq->sq_callbflags = 0;
	if (sq->sq_flags & SQ_WANTWAKEUP) {
		cv_broadcast(&sq->sq_wait);
		sq->sq_flags &= ~SQ_WANTWAKEUP;
	}
	mutex_exit(SQLOCK(sq));
}

/*
 * This routine is the SVR4MP 'replacement' for
 * hat_getkpfnum.  The only major difference is
 * the return value for illegal addresses - since
 * sunm_getkpfnum() and srmmu_getkpfnum() both
 * return '-1' for bogus mappings, we can (more or
 * less) return the value directly.
 */
ppid_t
kvtoppid(caddr_t addr)
{
	return ((ppid_t)hat_getkpfnum(addr));
}

/*
 * This is used to set the timeout value for cv_timed_wait() or
 * cv_timedwait_sig().
 */
void
time_to_wait(clock_t *now, clock_t time)
{
	(void) drv_getparm(LBOLT, now);
	*now += time;
}
