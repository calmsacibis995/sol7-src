/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1995, 1996, 1997 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#pragma ident   "@(#)ev_timers.c 1.1     97/12/03 SMI"

/* ev_timers.c - implement timers for the eventlib
 * vix 09sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_timers.c,v 1.17 1997/05/21 19:26:33 halley Exp $";
#endif

/* Import. */

#include "port_before.h"

#include <assert.h>
#include <errno.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Constants. */

#define BILLION 1000000000

/* Forward. */

static int due_sooner(void *, void *);
static void set_index(void *, int);
static void free_timer(void *, void *);
static void print_timer(void *, void *);

/* Public. */

struct timespec
evConsTime(time_t sec, long nsec) {
	struct timespec x;

	x.tv_sec = sec;
	x.tv_nsec = nsec;
	return (x);
}

struct timespec
evAddTime(struct timespec addend1, struct timespec addend2) {
	struct timespec x;

	x.tv_sec = addend1.tv_sec + addend2.tv_sec;
	x.tv_nsec = addend1.tv_nsec + addend2.tv_nsec;
	if (x.tv_nsec >= BILLION) {
		x.tv_sec++;
		x.tv_nsec -= BILLION;
	}
	return (x);
}

struct timespec
evSubTime(struct timespec minuend, struct timespec subtrahend) {
	struct timespec x;

	x.tv_sec = minuend.tv_sec - subtrahend.tv_sec;
	if (minuend.tv_nsec >= subtrahend.tv_nsec)
		x.tv_nsec = minuend.tv_nsec - subtrahend.tv_nsec;
	else {
		x.tv_nsec = BILLION - subtrahend.tv_nsec + minuend.tv_nsec;
		x.tv_sec--;
	}
	return (x);
}

int
evCmpTime(struct timespec a, struct timespec b) {
	long x = a.tv_sec - b.tv_sec;

	if (x == 0L)
		x = a.tv_nsec - b.tv_nsec;
	return (x < 0L ? (-1) : x > 0L ? (1) : (0));
}

struct timespec
evNowTime() {
	struct timeval now;
	struct timespec ret;

	if (gettimeofday(&now, NULL) < 0)
		return (evConsTime(0L, 0L));
	return (evTimeSpec(now));
}

struct timespec
evTimeSpec(struct timeval tv) {
	struct timespec ts;

	ts.tv_sec = tv.tv_sec;
	ts.tv_nsec = tv.tv_usec * 1000;
	return (ts);
}

struct timeval
evTimeVal(struct timespec ts) {
	struct timeval tv;

	tv.tv_sec = ts.tv_sec;
	tv.tv_usec = ts.tv_nsec / 1000;
	return (tv);
}

int
evSetTimer(evContext opaqueCtx,
	   evTimerFunc func,
	   void *uap,
	   struct timespec due,
	   struct timespec inter,
	   evTimerID *opaqueID
) {
	evContext_p *ctx = opaqueCtx.opaque;
	evTimer *id;

	evPrintf(ctx, 1,
"evSetTimer(ctx %#x, func %#x, uap %#x, due %d.%09ld, inter %d.%09ld)\n",
		 ctx, func, uap,
		 due.tv_sec, due.tv_nsec,
		 inter.tv_sec, inter.tv_nsec);

	/* due={0,0} is a magic cookie meaning "now." */
	if (due.tv_sec == 0 && due.tv_nsec == 0L)
		due = evNowTime();

	/* Allocate and fill. */
	OKNEW(id);
	id->func = func;
	id->uap = uap;
	id->due = due;
	id->inter = inter;

	if (heap_insert(ctx->timers, id) < 0)
		return (-1);

	/* Remember the ID if the caller provided us a place for it. */
	if (opaqueID)
		opaqueID->opaque = id;

	if (ctx->debug > 7) {
		evPrintf(ctx, 7, "timers after evSetTimer:\n");
		(void) heap_for_each(ctx->timers, print_timer, (void *)ctx);
	}

	return (0);
}

int
evClearTimer(evContext opaqueCtx, evTimerID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evTimer *del = id.opaque;

	if (heap_element(ctx->timers, del->index) != del)
		ERR(ENOENT);

	if (heap_delete(ctx->timers, del->index) < 0)
		return (-1);
	(void) free(del);

	if (ctx->debug > 7) {
		evPrintf(ctx, 7, "timers after evClearTimer:\n");
		(void) heap_for_each(ctx->timers, print_timer, (void *)ctx);
	}

	return (0);
}

int
evResetTimer(evContext opaqueCtx,
	     evTimerID id,
	     evTimerFunc func,
	     void *uap,
	     struct timespec due,
	     struct timespec inter
) {
	evContext_p *ctx = opaqueCtx.opaque;
	evTimer *timer = id.opaque;
	struct timespec old_due;
	int result=0;

	if (heap_element(ctx->timers, timer->index) != timer)
		ERR(ENOENT);

	old_due = timer->due;

	timer->func = func;
	timer->uap = uap;
	timer->due = due;
	timer->inter = inter;

	switch (evCmpTime(due, old_due)) {
	case -1:
		result = heap_increased(ctx->timers, timer->index);
		break;
	case 0:
		result = 0;
		break;
	case 1:
		result = heap_decreased(ctx->timers, timer->index);
		break;
	}

	if (ctx->debug > 7) {
		evPrintf(ctx, 7, "timers after evResetTimer:\n");
		(void) heap_for_each(ctx->timers, print_timer, (void *)ctx);
	}

	return (result);
}

/* Public to the rest of eventlib. */

heap_context evCreateTimers(const evContext_p *ctx) {
	return (heap_new(due_sooner, set_index, 0));
}

void evDestroyTimers(const evContext_p *ctx) {
	(void) heap_for_each(ctx->timers, free_timer, NULL);
	(void) heap_free(ctx->timers);
}

/* Private. */

static int
due_sooner(void *a, void *b) {
	evTimer *a_timer, *b_timer;

	a_timer = a;
	b_timer = b;
	return (evCmpTime(a_timer->due, b_timer->due) < 0);
}

static void
set_index(void *what, int index) {
	evTimer *timer;

	timer = what;
	timer->index = index;
}

static void
free_timer(void *what, void *uap) {
	free(what);
}

static void
print_timer(void *what, void *uap) {
	evTimer *cur = what;
	evContext_p *ctx = uap;

	cur = what;
	evPrintf(ctx, 7,
	    "  func %p, uap %p, due %d.%09ld, inter %d.%09ld\n",
		 cur->func, cur->uap,
		 cur->due.tv_sec, cur->due.tv_nsec,
		 cur->inter.tv_sec, cur->inter.tv_nsec);
}
