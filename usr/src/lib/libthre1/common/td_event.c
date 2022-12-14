/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma ident	"@(#)td_event.c	1.59	97/11/12 SMI"

/*
* Description:
* This module contains the functions for accessing events.
* The primary functions provide an address at which an event occurs
* and a message when the event occurs.
*
*/

#ifdef __STDC__
#pragma weak td_ta_event_addr = __td_ta_event_addr
#pragma weak td_thr_event_getmsg = __td_thr_event_getmsg
#pragma weak td_ta_event_getmsg = __td_ta_event_getmsg
#pragma weak td_thr_event_enable = __td_thr_event_enable
#pragma weak td_thr_clear_event = __td_thr_clear_event
#pragma weak td_thr_set_event = __td_thr_set_event
#pragma weak td_ta_clear_event = __td_ta_clear_event
#pragma weak td_ta_set_event = __td_ta_set_event
#endif				/* __STDC__ */

#include <thread_db.h>

#include "td.h"
#include "xtd.extdcl.h"
#include "td_event.h"
#include "td.pubdcl.h"

static void
eventsetaddset(td_thr_events_t *event1_p, td_thr_events_t *event2_p);
static void
eventsetdelset(td_thr_events_t *event1_p, td_thr_events_t *event2_p);
static td_err_e
td_ta_mod_event(const td_thragent_t *ta_p, td_thr_events_t *events, int onoff);


/*
* Description:
*   Given a process and an event number, return
* information about an address in the process or
* system call at which a breakpoint can be set to monitor
* the event.
*
* Input:
*   *ta_p - thread agent
*   event - Integer value corresponding to event of interest
*
* Output:
*   *notify_p - information on type of event notification
* 	and corresponding event information(e.g., address)
*   td_ta_event_addr - return value
*
* Side effects:
*   none.
*   Imported functions called: none
*/
td_err_e
__td_ta_event_addr(const td_thragent_t *ta_p, td_event_e event,
	td_notify_t *notify_p)
{
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (ta_p == NULL) {
		return (TD_BADTA);
	}
	if (event < TD_MIN_EVENT_NUM || event > TD_MAX_EVENT_NUM)
		return (TD_NOEVENT);

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock((rwlock_t *)&(ta_p->rwlock))) {
		return (TD_ERR);
	}
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}
	if (__td_pdmodel(ta_p->ph_p, &model) != PS_OK) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_ERR);
	}

	notify_p->type = NOTIFY_BPT;
	if (model == PR_MODEL_NATIVE) {
		notify_p->u.bptaddr =
		    (psaddr_t) ta_p->tdb_invar.tdb_events[event];
	}
#ifdef  _SYSCALL32_IMPL
	else {
		notify_p->u.bptaddr =
		    (psaddr_t) ta_p->tdb_invar32.tdb_events[event];
	}
#endif /* _SYSCALL32_IMPL */

	rw_unlock((rwlock_t *) &ta_p->rwlock);
	return (TD_OK);
}


/*
 * td_thr_event_getmsg
 *
 * Description:
 *
 * This function returns the most recent event message, if any,
 * associated with a thread.  Given a thread handle, return the message
 * corresponding to the event encountered by the thread.  Only one
 * message per thread is saved.  Messages from earlier events are lost
 * when later events occur.
 *
 * Input:
 *   *th_p - thread handle
 *
 * Output:
 *   *msg - event message for most recent event encountered
 * by thread corresponding to *th_p
 *   td_thr_event_getmsg - return value
 *
 */
td_err_e
__td_thr_event_getmsg(const td_thrhandle_t *th_p, td_event_msg_t *msg)
{
	td_err_e td_return;
	uthread_t thr_struct;
	td_eventbuf_t evbuf;
	struct ps_prochandle *ph_p;
	psaddr_t td_eventbuf;
#ifdef  _SYSCALL32_IMPL
	uthread32_t thr_struct32;
	td_eventbuf32_t evbuf32;
#endif  /* _SYSCALL32_IMPL */
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		return (TD_OK);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL) {
		return (TD_BADTH);
	}
	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTA);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&(th_p->th_ta_p->rwlock))) {
		td_return = TD_ERR;
		return (td_return);
	}

	if ((ph_p = th_p->th_ta_p->ph_p) == NULL || ps_pstop(ph_p) != PS_OK) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		td_return = TD_BADTA;
		return (td_return);
	}

	if (__td_pdmodel(ph_p, &model) != PS_OK) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_ERR);
	}
	if (model == PR_MODEL_NATIVE) {
		if (__td_read_thr_struct(th_p->th_ta_p, th_p->th_unique,
		    &thr_struct) != TD_OK) {
			(void) ps_pcontinue(ph_p);
			rw_unlock(&(th_p->th_ta_p->rwlock));
			return (TD_ERR);
		}
		td_eventbuf = (psaddr_t) thr_struct.t_td_evbuf;
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (__td_read_thr_struct32(th_p->th_ta_p, th_p->th_unique,
		    &thr_struct32) != TD_OK) {
			(void) ps_pcontinue(ph_p);
			rw_unlock(&(th_p->th_ta_p->rwlock));
			return (TD_ERR);
		}
		td_eventbuf = (psaddr_t) thr_struct32.t_td_evbuf;
	}
#endif /* _SYSCALL32_IMPL */

	if (td_eventbuf == NULL) {
		(void) ps_pcontinue(ph_p);
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_NOEVENT);
	}
	if (model == PR_MODEL_NATIVE) {
		if (ps_pdread(ph_p, td_eventbuf,
		    (char *) &evbuf, sizeof (evbuf)) == PS_OK) {
			if (evbuf.eventnum == TD_EVENT_NONE)
				td_return = TD_NOEVENT;
			else {
				msg->event = evbuf.eventnum;
				msg->th_p = (td_thrhandle_t *) th_p;
				msg->msg.data = (uintptr_t) evbuf.eventdata;
				/* "Consume" the message */
				evbuf.eventnum = TD_EVENT_NONE;
				if (ps_pdwrite(ph_p, td_eventbuf,
				    (char *) &evbuf, sizeof (evbuf)) != PS_OK)
					td_return = TD_DBERR;
			}
		} else {
			td_return = TD_DBERR;
		}
	}
#ifdef  _SYSCALL32_IMPL
	else {
		if (ps_pdread(ph_p, td_eventbuf,
		    (char *) &evbuf32, sizeof (evbuf32)) == PS_OK) {
			if (evbuf32.eventnum == TD_EVENT_NONE)
				td_return = TD_NOEVENT;
			else {
				msg->event = evbuf32.eventnum;
				msg->th_p = (td_thrhandle_t *) th_p;
				msg->msg.data = (uintptr_t) evbuf32.eventdata;
				/* "Consume" the message */
				evbuf32.eventnum = TD_EVENT_NONE;
				if (ps_pdwrite(ph_p, td_eventbuf,
				    (char *) &evbuf32, sizeof (evbuf32))
				    != PS_OK)
					td_return = TD_DBERR;
			}
		} else {
			td_return = TD_DBERR;
		}
	}
#endif /* _SYSCALL32_IMPL */

	if (ps_pcontinue(ph_p) != PS_OK && td_return == TD_OK)
		td_return = TD_DBERR;
	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (td_return);
}

/*
 * The callback function td_ta_event_getmsg uses when looking for
 * a thread with an event.  A thin wrapper around td_thr_event_getmsg.
 */
static int
event_msg_cb(const td_thrhandle_t *th_p, void *arg)

{
	td_event_msg_t *msg = arg;

	if (__td_thr_event_getmsg(th_p, msg) == TD_OK) {
		/* Got an event, stop iterating */
		return (1);
	}
	return (0);
}

/*
 * td_ta_event_getmsg
 *
 * This function is just like td_thr_event_getmsg, except that it is
 * passed a process handle rather than a thread handle, and returns
 * an event message for some thread in the process that has an event
 * message pending.  If no thread has an event message pending, this
 * routine returns TD_NOEVENT.  Thus, all pending event messages may
 * be collected from a process by repeatedly calling this routine
 * until it returns TD_NOEVENT.
 */
td_err_e
__td_ta_event_getmsg(const td_thragent_t *ta_p, td_event_msg_t *msg)

{
	td_err_e return_val;

	if (ta_p == NULL)
		return (TD_BADTA);
	if (ta_p->ph_p == NULL)
		return (TD_BADPH);
	if (msg == NULL)
		return (TD_ERR);
	msg->event = TD_EVENT_NONE;
	if ((return_val = __td_ta_thr_iter(ta_p, event_msg_cb, msg,
	    TD_THR_ANY_STATE, TD_THR_LOWEST_PRIORITY, TD_SIGNO_MASK,
	    TD_THR_ANY_USER_FLAGS)) != TD_OK)
		return (return_val);
	if (msg->event == TD_EVENT_NONE)
		return (TD_NOEVENT);
	return (TD_OK);
}


/*
 * Either add or delete the given event set from a thread's
 * event mask.
 */
static td_err_e
mod_eventset(const td_thrhandle_t *th_p, td_thr_events_t *events, int onoff)

{
	td_err_e	return_val;
	uthread_t	thr_struct;
	td_eventbuf_t	evbuf;
#ifdef  _SYSCALL32_IMPL
	uthread32_t	thr_struct32;
	td_eventbuf32_t	evbuf32;
#endif  /* _SYSCALL32_IMPL */
	struct ps_prochandle *ph_p;
	psaddr_t	td_eventbuf;
	int		model = PR_MODEL_NATIVE;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		ps_pstop(0);
		ps_pcontinue(0);
		ps_pdread(0, 0, 0, 0);
		if (return_val != TD_NOT_DONE) {
			ps_pdwrite(0, 0, 0, 0);
		}
		return (return_val);
	}
#endif

	/*
	 * Sanity checks.
	 */
	if (th_p == NULL)
		return (TD_BADTH);

	if ((th_p->th_ta_p == NULL) || (th_p->th_unique == NULL)) {
		return (TD_BADTH);
	}

	/*
	 * Get a reader lock.
	 */
	if (rw_rdlock(&th_p->th_ta_p->rwlock)) {
		return_val = TD_ERR;
		return (return_val);
	}

	if ((ph_p = th_p->th_ta_p->ph_p) == NULL || ps_pstop(ph_p) != PS_OK) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return_val = TD_BADTA;
		return (return_val);
	}
	if (__td_pdmodel(ph_p, &model) != PS_OK) {
		rw_unlock(&(th_p->th_ta_p->rwlock));
		return (TD_ERR);
	}

	if (model == PR_MODEL_NATIVE) {
		return_val = __td_read_thr_struct(th_p->th_ta_p,
		    th_p->th_unique, &thr_struct);
		td_eventbuf = (psaddr_t) thr_struct.t_td_evbuf;

		if (return_val == TD_OK && td_eventbuf == NULL) {
			/*
			 * Have to get an event buffer allocated in the
			 * target process.
			 */
			tdb_ctl_t agent_msg;
			agent_msg.opcode = THREAD_ALLOC_EVENT_STRUCT;
			agent_msg.u.thr_p = (uthread_t *) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p,
			    &agent_msg);
			if (return_val == TD_OK)
				return_val = agent_msg.result;
		}
		if (return_val == TD_OK && ps_pdread(ph_p,
		    td_eventbuf,
		    (char *) &evbuf, sizeof (evbuf)) != PS_OK)
			return_val = TD_DBERR;
		if (return_val == TD_OK) {
			if (onoff)
				eventsetaddset(&evbuf.eventmask, events);
			else
				eventsetdelset(&evbuf.eventmask, events);
			if (ps_pdwrite(ph_p,
			    td_eventbuf,
			    (char *) &evbuf, sizeof (evbuf)) != PS_OK)
				return_val = TD_DBERR;
		}
	}
#ifdef  _SYSCALL32_IMPL
	else {
		return_val = __td_read_thr_struct32(th_p->th_ta_p,
		    th_p->th_unique, &thr_struct32);
		td_eventbuf = (psaddr_t) thr_struct32.t_td_evbuf;

		if (return_val == TD_OK && td_eventbuf == NULL) {
			/*
			 * Have to get an event buffer allocated in the
			 * target process.
			 */
			tdb_ctl32_t agent_msg;
			agent_msg.opcode = THREAD_ALLOC_EVENT_STRUCT;
			/* XXXLP64 - is th_unique 32-bit? */
			agent_msg.u.thr_p = (caddr32_t) th_p->th_unique;
			return_val = __td_agent_send(th_p->th_ta_p,
			    (tdb_ctl_t *)&agent_msg);
			if (return_val == TD_OK)
				return_val = agent_msg.result;
		}
		if (return_val == TD_OK && ps_pdread(ph_p,
		    td_eventbuf,
		    (char *) &evbuf32, sizeof (evbuf32)) != PS_OK)
			return_val = TD_DBERR;
		if (return_val == TD_OK) {
			if (onoff)
				eventsetaddset(&evbuf32.eventmask, events);
			else
				eventsetdelset(&evbuf32.eventmask, events);
			if (ps_pdwrite(ph_p,
			    td_eventbuf,
			    (char *) &evbuf32, sizeof (evbuf32)) != PS_OK)
				return_val = TD_DBERR;
		}
	}
#endif /* _SYSCALL32_IMPL */

	if (ps_pcontinue(ph_p) != PS_OK)
		return_val = TD_DBERR;
	rw_unlock(&(th_p->th_ta_p->rwlock));
	return (return_val);
}

/*
* Description:
*   Enable or disable tracing for a given thread.  Tracing
* is filtered based on the event mask of each thread.  Tracing
* can be turned on/off for the thread without changing thread
* event mask.
*
* Input:
*   *th_p - thread handle
*   onoff - = 0 disables events in thread
* non_zero enables events in thread
*
* Output:
*   td_thr_event_enable - return value
*
* Side effect:
*   Thread data structures are updated to enable or disable
* events.
* Imported functions called: ps_pdwrite, ps_pglobal_lookup.
*/
td_err_e
__td_thr_event_enable(const td_thrhandle_t *th_p, int onoff)
{
	td_thr_events_t	evset;

#ifdef TEST_PS_CALLS
	if (td_noop) {
		ps_pdwrite(0, 0, 0, 0);
		ps_pglobal_lookup(0, 0, 0, 0);
		return (TD_OK);
	}
#endif
	td_event_emptyset(&evset);
	td_event_addset(&evset, TD_EVENTS_ENABLE);

	return (mod_eventset(th_p, &evset, onoff));
}


/*
 * Description:
 * Set event mask to disable the given event set; these events are cleared
 * from the event mask of the thread.  Events that occur for a thread
 * with the event masked off will not cause notification to be
 * sent to the debugger (see td_thr_set_event for fuller
 * description).
 *
 * Input:
 *   *th_p - thread handle
 *   event - event number(see thread_db.h)
 *
 * Output:
 *   td_thr_clear_event - return value
 *
 * Side effects:
 *   Event mask in thread structure is updated or global
 * event maks is updated.
 *   Imported functions called: ps_pstop, ps_pcontinue,
 * ps_pdread, ps_pdwrite.
 */
td_err_e
__td_thr_clear_event(const td_thrhandle_t *th_p, td_thr_events_t *events)
{
	return (mod_eventset(th_p, events, 0));
}

/*
* Description:
*   Set event mask to enable event. event is turned on in
* event mask for thread.  If a thread encounters an event
* for which its event mask is on, notification will be sent
* to the debugger.
*   Addresses for each event are provided to the
* debugger.  It is assumed that a breakpoint of some type will
* be placed at that address.  If the event mask for the thread
* is on, the instruction at the address will be executed.
* Otherwise, the instruction will be skipped.
*   If thread handle is NULL, event is set in global
* event mask.  The global event mask is applied to all threads.
*
* Input:
*   *th_p - thread handle
*   events - event set to enable
*
* Output:
*   td_thr_set_event - return value
*
* Side effects:
*   Event mask in thread structure is updated
* Imported functions called: ps_pstop, ps_pcontinue,
* ps_pdwrite, ps_pdread.
*/
td_err_e
__td_thr_set_event(const td_thrhandle_t *th_p, td_thr_events_t *events)
{
	return (mod_eventset(th_p, events, 1));
}

/*
 * Description:
 *
 * Enable a set of events in the process-global event mask.
 */

td_err_e
__td_ta_set_event(const td_thragent_t *ta_p, td_thr_events_t *events)

{
	return (td_ta_mod_event(ta_p, events, 1));
}

/*
 * Description:
 *
 * Disable a set of events in the process-global event mask.
 */

td_err_e
__td_ta_clear_event(const td_thragent_t *ta_p, td_thr_events_t *events)

{
	return (td_ta_mod_event(ta_p, events, 0));
}

/*
 * Enable or disable a set of events in the process-global event mask,
 * depending on the value of onoff.
 */
static td_err_e
td_ta_mod_event(const td_thragent_t *ta_p, td_thr_events_t *events, int onoff)

{
	td_thr_events_t targ_eventset;
	ps_err_e	ps_ret;
	int		model = PR_MODEL_NATIVE;

	if (ta_p == NULL)
		return (TD_BADTA);
	if (rw_rdlock((rwlock_t *) &ta_p->rwlock))
		return (TD_ERR);
	if (ta_p->ph_p == NULL) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_BADPH);
	}
	if (__td_pdmodel(ta_p->ph_p, &model) != PS_OK) {
		rw_unlock((rwlock_t *)&(ta_p->rwlock));
		return (TD_ERR);
	}
	if (ps_pstop(ta_p->ph_p) != PS_OK) {
		rw_unlock((rwlock_t *) &ta_p->rwlock);
		return (TD_DBERR);
	}
	if (model == PR_MODEL_NATIVE) {
		ps_ret = ps_pdread(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar.tdb_eventmask_addr,
		    (char *) &targ_eventset, sizeof (targ_eventset));
	}
#ifdef  _SYSCALL32_IMPL
	else {
		ps_ret = ps_pdread(ta_p->ph_p,
		    (psaddr_t) ta_p->tdb_invar32.tdb_eventmask_addr,
		    (char *) &targ_eventset, sizeof (targ_eventset));
	}
#endif /* _SYSCALL32_IMPL */
	if (ps_ret == PS_OK) {
		if (onoff)
			eventsetaddset(&targ_eventset, events);
		else
			eventsetdelset(&targ_eventset, events);
		if (model == PR_MODEL_NATIVE) {
			ps_ret = ps_pdwrite(ta_p->ph_p,
			    (psaddr_t) ta_p->tdb_invar.tdb_eventmask_addr,
			    (char *) &targ_eventset, sizeof (targ_eventset));
		}
#ifdef  _SYSCALL32_IMPL
		else {
			ps_ret = ps_pdwrite(ta_p->ph_p,
			    (psaddr_t) ta_p->tdb_invar32.tdb_eventmask_addr,
			    (char *) &targ_eventset, sizeof (targ_eventset));
		}
#endif /* _SYSCALL32_IMPL */
	}
	(void) ps_pcontinue(ta_p->ph_p);
	rw_unlock((rwlock_t *) &ta_p->rwlock);
	if (ps_ret == PS_OK)
		return (TD_DBERR);
	return (TD_OK);
}


/*
* Description:
*   OR the events in *event1_p and *event2_p and return in *event1_p.
*
* Input:
*   *event1_p - events in set 1
*   *event2_p - events in set 2
*
* Output:
*   *event1_p - OR'ed events
*
* Side effects:
*   none
*/
static void
eventsetaddset(td_thr_events_t *event1_p, td_thr_events_t *event2_p)
{
	int	i;

	for (i = 0; i < TD_EVENTSIZE; i++)
		event1_p->event_bits[i] |= event2_p->event_bits[i];
}

/*
* Description:
*   Delete the events in eventset 2 from eventset 1.
*
* Input:
*   *event1_p - events in set 1
*   *event2_p - events in set 2
*
* Output:
*   *event1_p - AND'ed events
*
* Side effects:
*   none
*/

static void
eventsetdelset(td_thr_events_t *event1_p, td_thr_events_t *event2_p)
{
	int	i;

	for (i = 0; i < TD_EVENTSIZE; i++)
		event1_p->event_bits[i] &= ~event2_p->event_bits[i];
}
