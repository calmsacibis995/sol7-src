/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sleepq.c	1.39	98/02/01 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/thread.h>
#include <sys/proc.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/sleepq.h>

/*
 * Operations on sleepq_t structures.
 */

sleepq_head_t sleepq_head[NSLEEPQ];

/*
 * Insert thread "c" into sleep queue "spq" in dispatch priority order.
 */
void
sleepq_insert(sleepq_t *spq, kthread_t *c)
{
	kthread_t	*tp;
	kthread_t	**tpp;
	pri_t		cpri;

	ASSERT(THREAD_LOCK_HELD(c));	/* holding the lock on the sleepq */
	cpri = DISP_PRIO(c);
	tpp = &spq->sq_first;
	while ((tp = *tpp) != NULL) {
		if (cpri > DISP_PRIO(tp))
			break;
		tpp = &tp->t_link;
	}
	*tpp = c;
	c->t_link = tp;
}


/*
 * Yank a particular thread out of sleep queue "spq" and wake it up.
 */
kthread_t *
sleepq_unsleep(sleepq_t *spq, kthread_t *t)
{
	kthread_t	*nt;
	kthread_t	**ptl;

	ASSERT(THREAD_LOCK_HELD(t));	/* thread locked via sleepq */

	ptl = &spq->sq_first;
	while ((nt = *ptl) != NULL) {
		if (nt == t) {
			*ptl = t->t_link;
			t->t_link = NULL;
			t->t_sobj_ops = NULL;
			t->t_wchan = NULL;
			t->t_wchan0 = NULL;
			ASSERT(t->t_state == TS_SLEEP);
			/*
			 * Change thread to transition state without
			 * dropping the sleep queue lock.
			 */
			THREAD_TRANSITION_NOLOCK(t);
			return (t);
		}
		ptl = &nt->t_link;
	}
	return (NULL);
}

/*
 * Yank a particular thread out of sleep queue "spq" but don't wake it up.
 */
kthread_t *
sleepq_dequeue(sleepq_t *spq, kthread_t *t)
{
	kthread_t	*nt;
	kthread_t	**ptl;

	ASSERT(THREAD_LOCK_HELD(t));	/* thread locked via sleepq */

	ptl = &spq->sq_first;
	while ((nt = *ptl) != NULL) {
		if (nt == t) {
			*ptl = t->t_link;
			t->t_link = NULL;
			return (t);
		}
		ptl = &nt->t_link;
	}
	return (NULL);
}

void *
sleepq_wakeone_chan(sleepq_t *spq, void *chan)
{
	kthread_t 	*tp;
	kthread_t	**tpp;

	tpp = &spq->sq_first;
	while ((tp = *tpp) != NULL) {
		if (tp->t_wchan == chan) {
			ASSERT(tp->t_wchan0 == 0);
			*tpp = tp->t_link;
			tp->t_wchan = NULL;
			tp->t_link = NULL;
			tp->t_sobj_ops = NULL;
			ASSERT(tp->t_state == TS_SLEEP);
			CL_WAKEUP(tp);
			thread_unlock_high(tp);		/* drop runq lock */
			return (tp);
		}
		tpp = &tp->t_link;
	}
	return (NULL);
}

void
sleepq_wakeall_chan(sleepq_t *spq, void *chan)
{
	kthread_t 	*tp;
	kthread_t	**tpp;

	tpp = &spq->sq_first;
	while ((tp = *tpp) != NULL) {
		if (tp->t_wchan == chan) {
			ASSERT(tp->t_wchan0 == 0);
			*tpp = tp->t_link;
			tp->t_wchan = NULL;
			tp->t_link = NULL;
			tp->t_sobj_ops = NULL;
			ASSERT(tp->t_state == TS_SLEEP);
			CL_WAKEUP(tp);
			thread_unlock_high(tp);		/* drop runq lock */
			continue;
		}
		tpp = &tp->t_link;
	}
}
