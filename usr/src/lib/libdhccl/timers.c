/*
 * timers.c: "Adminster multiple alarm timers".
 *
 * SYNOPSIS
 *    int schedule      (time_t at, void (*callback)(void *),
 *					   void *data, const sigset_t *b)
 *    int cancelWakeup  (int cancelid)
 *    void showWakeups  ()
 *
 * DESCRIPTION
 *    These routines allow a process to install an arbitrary number of
 *    timers, thus extending the capability of the alarm() system
 *    call. Like alarm() timers will only work at the granularity
 *    of seconds. A new timer can be installed by calling schedule()
 *    specifying a time offset from the current time, a callback
 *    function, a pointer to data which which will be the argument
 *    to the callback, and a signal mask specifying the signals to
 *    be blocked when the callback occurs.
 *
 *    This kind of code is trickier than it might appear. Points to
 *    consider are
 *    (a) The callback function may itself request a new timer
 *    (b) The callback function may take so long to run that
 *        other timers "miss". When this happens, each timer
 *        that is delayed will be called in order, with a delay
 *        of one second between each, until the backlog has
 *        been cleared.
 *    (c) The caller has to ensure that it does not free
 *        the memory that "data" points to before the callback
 *        is activated.
 *    (d) Ultimately, the asynchronous alarm relies on the
 *        use of the system call alarm(). You can't, therefore,
 *        have any calls to alarm() from within code that
 *        uses this mechanism.
 *
 *    The offset specified is in seconds from the current time, and
 *    must be positive or it will be ignored (you can't install a
 *    timer which should already have fired). An existing timer
 *    may be cancelled by calling cancelWakeups() with the integer
 *    ID that was returned by schedule.
 *
 *    ShowWakeups() displays the current timers which are queued
 *    for execution.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)timers.c 1.2 96/11/21 SMI"

#include "catype.h"
#include "ca_time.h"
#include <signal.h>
#include <malloc.h>
#include <stdio.h>
#include <unistd.h>
#include "utils.h"
#include "unixgen.h"

typedef struct Wakeups {
	time_t wake;
	void (*callback)(void *);
	void *data;
	struct Wakeups *next;
	int id;
	sigset_t blocked;
} Wakeups;


static Wakeups *firstWakeup = NULL;
static int id;
static struct sigaction s;

/*
 * This flag is so that user code which calls cancelWakeup() will not
 * corrupt the ordering of timeouts by scheduling the next alarm()
 */
static int processingTimeout = FALSE;

static void
catch_alarm(void)
{
	Wakeups *pw = firstWakeup;
	int newInterval;
	int now;

	processingTimeout = TRUE;
	if (pw == 0)
		return;
	now = GetCurrentSecond();
	if (pw->wake > now) { /* can happen due to race condition */
		if (pw->id != 0) {
			newInterval = pw->wake - now;
			alarm((unsigned)newInterval);
		}
		processingTimeout = FALSE;
		return;
	}

	/* do this here in case current callback sets new timer(s) */
	firstWakeup = pw->next;
	if (pw->callback)
		pw->callback(pw->data);
	else
		logb("wakeup at %ld\n", (long)now);
	free(pw);
	pw = firstWakeup;
	if (pw != 0) {
		s.sa_mask = pw->blocked;
		sigaction(SIGALRM, &s, 0);

		/* in case callback was time consuming !! */
		now = GetCurrentSecond();

		newInterval = pw->wake - now > 0 ? pw->wake - now : 1;
		alarm((unsigned)newInterval);
	}
	processingTimeout = FALSE;
}

int
schedule(time_t at, void (*callback)(void *), void *data, const sigset_t *b)
{
	Wakeups *pw, *neww;
	time_t wake;
	sigset_t block, oldmask;
	int now;

	if (at <= 0)
		return (0);

	now = GetCurrentSecond();
	wake = at + now;
	sigemptyset(&block);
	sigaddset(&block, SIGALRM);
	sigprocmask(SIG_BLOCK, &block, &oldmask);

	if (firstWakeup == 0) {
		pw = (Wakeups *)xmalloc(sizeof (Wakeups));
		if (!pw)
			return (-1);
		firstWakeup = pw;
		neww = 0;
	} else {
		for (pw = firstWakeup; pw->wake <= wake && pw->next;
		    pw = pw->next)
			/* NULL statement */;

		neww = (Wakeups *)xmalloc(sizeof (Wakeups));
		if (pw->wake > wake)
			*neww = *pw;
		else {
			pw->next = neww;
			pw = neww;
			neww = 0;
		}
	}
	pw->wake = wake;
	pw->callback = callback;
	pw->data = data;
	pw->next = neww;
	pw->id = ++id;

	if (b)
		pw->blocked = *b;
	else
		sigemptyset(&pw->blocked);

	if (pw == firstWakeup) {
		if (b)
			s.sa_mask = *b;
		else {
			sigemptyset(&s.sa_mask);
			sigaddset(&s.sa_mask, SIGALRM);
		}
		s.sa_handler = catch_alarm;
		s.sa_flags = 0;
		s.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &s, 0);
		alarm((unsigned)at);
	}
	sigprocmask(SIG_SETMASK, &oldmask, 0);
	return (id);
}

int
cancelWakeup(int cancelid)
{
	Wakeups *pw;

	if (firstWakeup == 0) /* empty list */
		return (-1);

	if (firstWakeup->id == cancelid) {
		pw = firstWakeup->next;
		free(firstWakeup);
		firstWakeup = pw;
		if (!processingTimeout) {
			if (pw == 0)
				alarm(0);
			else {
				int newInterval, now;
				s.sa_mask = pw->blocked;
				sigaction(SIGALRM, &s, 0);
				now = GetCurrentSecond();
				newInterval =
				    pw->wake - now > 0 ? pw->wake - now : 1;
				alarm((unsigned)newInterval);
			}
		}
		return (0);
	}

	/* Not the first: */

	for (pw = firstWakeup; pw->next && pw->next->id != cancelid;
	    pw = pw->next)
		/* NULL statement */;
	if (pw->next == 0)
		return (-1);
	else {
		Wakeups *ppw = pw->next->next;
		free(pw->next);
		pw->next = ppw;
		return (0);
	}
}

void
showWakeups(void)
{
	Wakeups *pw;
	int now = GetCurrentSecond();
	struct tm *tm;
	char buf[64];

	logb("First wakeup = %#lx\n", (u_long)firstWakeup);
	logb("%-20s %-11s\n%-20s %-11s %-10s %-8s %-8s %-8s %-8s\n", "Wakeup",
	    "Seconds", "UCT", "Left", "ID", "Callback", "Data", "This", "Next");

	for (pw = firstWakeup; pw != NULL; pw = pw->next) {
		tm = localtime(&pw->wake);
		strftime(buf, sizeof (buf), "%m/%d/%Y %T", tm);
		logb("%-20.20s %-11d %-10d %-#8lx %-#8lx %-#8lx %-#8lx\n",
		    buf, (long)(pw->wake - now), pw->id, (u_long)pw->callback,
		    (u_long)pw->data, (u_long)pw, (u_long)pw->next);
		logb("\t=%ld\n", (long)pw->wake);
	}
}
