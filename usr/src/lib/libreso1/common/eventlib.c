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
#pragma ident   "@(#)eventlib.c 1.2     98/01/26 SMI"

/* eventlib.c - implement glue for the eventlib
 * vix 09sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: eventlib.c,v 1.24 1997/05/21 19:26:34 halley Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

static struct evEvent_p	*evNew(evContext_p *ctx);
static void		evFree(evContext_p *ctx, struct evEvent_p *old);

#ifdef NEED_PSELECT
#ifdef SUNW_POLL
static int 		pselect(evContext_p *ctx, struct timespec *);
#else
static int		pselect(int, void *, void *, void *, struct timespec*);
#endif
#endif


/* Public. */

int
evCreate(evContext *opaqueCtx) {
	evContext_p *ctx;

	OKNEW(ctx);

	/* Debugging. */
	ctx->debug = 0;
	ctx->output = NULL;

	/* Files. */
	ctx->files = NULL;
	FD_ZERO(&ctx->rdNext);
	FD_ZERO(&ctx->wrNext);
	FD_ZERO(&ctx->exNext);
	ctx->fdMax = -1;
	ctx->fdNext = NULL;
	ctx->fdCount = 0;	/* Invalidate {rd,wr,ex}Last. */

	/* Streams. */
	ctx->streams = NULL;
	ctx->strFree = NULL;
	ctx->strDone = NULL;
	ctx->strLast = NULL;

	/* Timers. */
	ctx->timers = evCreateTimers(ctx);
	if (ctx->timers == NULL)
		return (-1);

	/* Waits. */
	ctx->waitLists = NULL;
	ctx->waitDone.first = ctx->waitDone.last = NULL;
	ctx->waitDone.prev = ctx->waitDone.next = NULL;
	ctx->waitListFree = NULL;
	ctx->waitFree = NULL;

	/* Housekeeping. */
	ctx->evFree = NULL;

#ifdef SUNW_POLL
	/* Poll stuff - don't touch if you know not what you are doing*/
	ctx->pollfds = NULL;
	ctx->maxnfds = 0;
	ctx->firstfd = 0;
	ctx->lastfd = 0;
#endif
	opaqueCtx->opaque = ctx;
	return (0);
}

void
evSetDebug(evContext opaqueCtx, int level, FILE *output) {
	evContext_p *ctx = opaqueCtx.opaque;

	ctx->debug = level;
	ctx->output = output;
}

int
evDestroy(evContext opaqueCtx) {
	evContext_p *ctx = opaqueCtx.opaque;
	int revs = 424242;	/* Doug Adams. */
	evWaitList *this_wl, *next_wl;
	evWait *this_wait, *next_wait;

	/* Files. */
	while (revs-- > 0 && ctx->files != NULL) {
		evFileID id;

		id.opaque = ctx->files;
		(void) evDeselectFD(opaqueCtx, id);
	}
	assert(revs >= 0);

	/* Streams. */
	while (revs-- > 0 && ctx->streams != NULL) {
		evStreamID id;

		id.opaque = ctx->streams;
		(void) evCancelRW(opaqueCtx, id);
	}
	while (revs-- > 0 && ctx->strFree != NULL) {
		evStream *next = ctx->strFree->next;

		(void) free(ctx->strFree);
		ctx->strFree = next;
	}

	/* Timers. */
	evDestroyTimers(ctx);

	/* Waits. */
	for (this_wl = ctx->waitLists;
	     revs-- > 0 && this_wl != NULL;
	     this_wl = next_wl) {
		next_wl = this_wl->next;
		for (this_wait = this_wl->first;
		     revs-- > 0 && this_wait != NULL;
		     this_wait = next_wait) {
			next_wait = this_wait->next;
			(void) free(this_wait);
		}
		(void) free(this_wl);
	}
	for (this_wait = ctx->waitDone.first;
	     revs-- > 0 && this_wait != NULL;
	     this_wait = next_wait) {
		next_wait = this_wait->next;
		(void) free(this_wait);
	}
	for (this_wait = ctx->waitFree;
	     revs-- > 0 && this_wait != NULL;
	     this_wait = next_wait) {
		next_wait = this_wait->next;
		(void) free(this_wait);
	}

	/* Housekeeping. */
	while (revs-- > 0 && ctx->evFree != NULL) {
		struct evEvent_p *next = ctx->evFree->u.free.next;

		(void) free(ctx->evFree);
		ctx->evFree = next;
	}
	assert(revs >= 0);

	(void) free(ctx);
	return (0);
}

int
evGetNext(evContext opaqueCtx, evEvent *opaqueEv, int options) {
	evContext_p *ctx = opaqueCtx.opaque;
	struct timespec now, nextTime;
	evTimer *nextTimer;
	evEvent_p *new;
	int x, pselect_errno, timerPast;

	/* Ensure that exactly one of EV_POLL or EV_WAIT was specified. */
	x = ((options & EV_POLL) != 0) + ((options & EV_WAIT) != 0);
	if (x != 1)
		ERR(EINVAL);

	/* Get the time of day.  We'll do this again after select() blocks. */
	now = evNowTime();

 again:
	/* Stream IO does not require a select(). */
	if (ctx->strDone != NULL) {
		new = evNew(ctx);
		new->type = Stream;
		new->u.stream.this = ctx->strDone;
		ctx->strDone = ctx->strDone->nextDone;
		if (ctx->strDone == NULL)
			ctx->strLast = NULL;
		opaqueEv->opaque = new;
		return (0);
	}

	/* Waits do not require a select(). */
	if (ctx->waitDone.first != NULL) {
		new = evNew(ctx);
		new->type = Wait;
		new->u.wait.this = ctx->waitDone.first;
		ctx->waitDone.first = ctx->waitDone.first->next;
		if (ctx->waitDone.first == NULL)
			ctx->waitDone.last = NULL;
		opaqueEv->opaque = new;
		return (0);
	}

	/* Get the status and content of the next timer. */
	if ((nextTimer = heap_element(ctx->timers, 1)) != NULL) {
		nextTime = nextTimer->due;
		timerPast = (evCmpTime(nextTime, now) <= 0);
	}

	evPrintf(ctx, 9, "evGetNext: fdCount %d\n", ctx->fdCount);
	if (ctx->fdCount == 0) {
		static const struct timespec NoTime = {0, 0L};
		enum { JustPoll, Block, Timer } m;
		struct timespec t, *tp;

		/* Are there any events at all? */
		if ((options & EV_WAIT) != 0 && !nextTimer && ctx->fdMax == -1)
			ERR(ENOENT);

		/* Figure out what select()'s timeout parameter should be. */
		if ((options & EV_POLL) != 0) {
			m = JustPoll;
			t = NoTime;
			tp = &t;
		} else if (nextTimer == NULL) {
			m = Block;
			/* ``t'' unused. */
			tp = NULL;
		} else if (timerPast) {
			m = JustPoll;
			t = NoTime;
			tp = &t;
		} else {
			m = Timer;
			/* ``t'' filled in later. */
			tp = &t;
		}

		do {
			ctx->rdLast = ctx->rdNext;
			ctx->wrLast = ctx->wrNext;
			ctx->exLast = ctx->exNext;

			if (m == Timer) {
				assert(tp == &t);
				t = evSubTime(nextTime, now);
			}
#ifdef SUNW_POLL
#else
			evPrintf(ctx, 4,
				"pselect(%d, 0x%lx, 0x%lx, 0x%lx, %d.%09ld)\n",
				 ctx->fdMax+1,
				 (u_long)ctx->rdLast.fds_bits[0],
				 (u_long)ctx->wrLast.fds_bits[0],
				 (u_long)ctx->exLast.fds_bits[0],
				 tp ? tp->tv_sec : -1,
				 tp ? tp->tv_nsec : -1);
#endif

#ifdef SUNW_POLL

			x = pselect(ctx, tp);
#else
			x = pselect(ctx->fdMax+1,
				    &ctx->rdLast, &ctx->wrLast, &ctx->exLast,
				    tp);
#endif /* SUNW_POLL */
			pselect_errno = errno;

#ifdef SUNW_POLL
			evPrintf(ctx, 4, "poll() returns %d (err: %s)\n",
				 x, (x == -1) ? strerror(errno) : "none");
#else
			evPrintf(ctx, 4, "select() returns %d (err: %s)\n",
				 x, (x == -1) ? strerror(errno) : "none");
#endif /* SUNW_POLL */

			/* Anything but a poll can change the time. */
			if (m != JustPoll)
				now = evNowTime();

			/* Select() likes to finish about 10ms early. */
		} while (x == 0 && m == Timer && evCmpTime(now, nextTime) < 0);
		if (x < 0) {
			if (pselect_errno == EINTR) {
				if ((options & EV_NULL) != 0)
					goto again;
				if (!(new = evNew(ctx)))
					return (-1);
				new->type = Null;
				/* No data. */
				opaqueEv->opaque = new;
				return (0);
			}
			ERR(pselect_errno);
		}
		if (x == 0 && (nextTimer && !timerPast) && (options & EV_POLL))
			ERR(EWOULDBLOCK);
		ctx->fdCount = x;
	}
	assert(nextTimer || ctx->fdCount);

	/* Timers go first since we'd like them to be accurate. */
	if (nextTimer && !timerPast) {
		/* Has anything happened since we blocked? */
		timerPast = (evCmpTime(nextTime, now) <= 0);
	}
	if (nextTimer && timerPast) {
		if (!(new = evNew(ctx)))
			return (-1);
		new->type = Timer;
		new->u.timer.this = nextTimer;
		opaqueEv->opaque = new;
		return (0);
	}

	/* No timers, so there should be a ready file descriptor. */
	x = 0;
	while (ctx->fdCount) {
		evFile *fid;
		int fd, eventmask;

		if (!ctx->fdNext) {
			if (++x == 2) {
				/*
				 * Hitting the end twice means that the last
				 * select() found some FD's which have since
				 * been deselected.
				 */
				ctx->fdCount = 0;
				break;
			}
			ctx->fdNext = ctx->files;
		}
		fid = ctx->fdNext;
		ctx->fdNext = fid->next;

		fd = fid->fd;
		eventmask = 0;
		if (FD_ISSET(fd, &ctx->rdLast))
			eventmask |= EV_READ;
		if (FD_ISSET(fd, &ctx->wrLast))
			eventmask |= EV_WRITE;
		if (FD_ISSET(fd, &ctx->exLast))
			eventmask |= EV_EXCEPT;
		eventmask &= fid->eventmask;
		if (eventmask != 0) {
			ctx->fdCount--;
			if (!(new = evNew(ctx)))
				return (-1);
			new->type = File;
			new->u.file.this = fid;
			new->u.file.eventmask = eventmask;
			opaqueEv->opaque = new;
			return (0);
		}
	}

	/* We get here if the caller deselect()'s an FD. Gag me with a goto. */
	goto again;
}

int
evDispatch(evContext opaqueCtx, evEvent opaqueEv) {
	evContext_p *ctx = opaqueCtx.opaque;
	evEvent_p *ev = opaqueEv.opaque;

	switch (ev->type) {
	    case File: {
		evFile *this = ev->u.file.this;
		int eventmask = ev->u.file.eventmask;

		evPrintf(ctx, 5,
			"Dispatch.File: fd %d, mask 0x%x, func %#x, uap %#x\n",
			 this->fd, this->eventmask, this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->fd, eventmask);
		break;
	    }
	    case Stream: {
		evStream *this = ev->u.stream.this;

		evPrintf(ctx, 5,
			 "Dispatch.Stream: fd %d, func %#x, uap %#x\n",
			 this->fd, this->func, this->uap);
		errno = this->ioErrno;
		(this->func)(opaqueCtx, this->uap, this->fd, this->ioDone);
		break;
	    }
	    case Timer: {
		evTimer *this = ev->u.timer.this;

		evPrintf(ctx, 5, "Dispatch.Timer: func %#x, uap %#x\n",
			 this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->due, this->inter);
		break;
	    }
	    case Wait: {
		evWait *this = ev->u.wait.this;

		evPrintf(ctx, 5,
			 "Dispatch.Wait: tag %#x, func %#x, uap %#x\n",
			 this->tag, this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->tag);
		break;
	    }
	    case Null: {
		/* No work. */
		break;
	    }
	    default: {
		abort();
	    }
	}
	evDrop(opaqueCtx, opaqueEv);
	return (0);
}

void
evDrop(evContext opaqueCtx, evEvent opaqueEv) {
	evContext_p *ctx = opaqueCtx.opaque;
	evEvent_p *ev = opaqueEv.opaque;

	switch (ev->type) {
	    case File: {
		/* No work. */
		break;
	    }
	    case Stream: {
		evStreamID id;

		id.opaque = ev->u.stream.this;
		(void) evCancelRW(opaqueCtx, id);
		break;
	    }
	    case Timer: {
		evTimer *this = ev->u.timer.this;
		evTimerID opaque;

		/* Check to see whether the user func cleared the timer. */
		if (heap_element(ctx->timers, this->index) != this) {
			evPrintf(ctx, 5, "Dispatch.Timer: timer rm'd?\n");
			break;
		}
		/*
		 * Timer is still there.  Delete it if it has expired,
		 * otherwise set it according to its next interval.
		 */
		if (this->inter.tv_sec == 0 && this->inter.tv_nsec == 0L) {
			opaque.opaque = this;			
			(void) evClearTimer(opaqueCtx, opaque);
		} else {
			opaque.opaque = this;
			(void) evResetTimer(opaqueCtx, opaque, this->func,
					    this->uap,
					    evAddTime(this->due, this->inter),
					    this->inter);
		}
		break;
	    }
	    case Wait: {
		(void) evFreeWait(ctx, ev->u.wait.this);
		break;
	    }
	    case Null: {
		/* No work. */
		break;
	    }
	    default: {
		abort();
	    }
	}
	evFree(ctx, ev);
}

int
evMainLoop(evContext opaqueCtx) {
	evEvent event;
	int x;

	while ((x = evGetNext(opaqueCtx, &event, EV_WAIT)) == 0)
		if ((x = evDispatch(opaqueCtx, event)) < 0)
			break;
	return (x);
}

void
evPrintf(const evContext_p *ctx, int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	if (ctx->output != NULL && ctx->debug >= level) {
		vfprintf(ctx->output, fmt, ap);
		fflush(ctx->output);
	}
	va_end(ap);
}

static struct evEvent_p	*
evNew(evContext_p *ctx) {
	struct evEvent_p *new;

	if ((new = ctx->evFree) != NULL) {
		assert(new->type == Free);
		ctx->evFree = new->u.free.next;
		FILL(new);
	} else
		NEW(new);
	return (new);
}

static void
evFree(evContext_p *ctx, struct evEvent_p *old) {
	old->type = Free;
	old->u.free.next = ctx->evFree;
	ctx->evFree = old;
}

#ifdef NEED_PSELECT
static int
#ifdef SUNW_POLL
pselect(evContext_p *ctx, struct timespec *tsp) {
#else
pselect(int nfds, void *rfds, void *wfds, void *efds, struct timespec *tsp) {
#endif
	struct timeval tv, *tvp;
	int n;
#ifdef SUNW_POLL
        int            polltimeout = INFTIM;
#endif
		if (tsp) {
			tvp = &tv;
			tv = evTimeVal(*tsp);
	#ifdef SUNW_POLL
			polltimeout = 1000*tv.tv_sec + tv.tv_usec/1000; 
	#endif
		} else {
	#ifdef SUNW_POLL
		polltimeout = INFTIM;
	#endif
			tvp = NULL;
		}
	#ifdef SUNW_POLL
		polltimeout = 1000*tv.tv_sec + tv.tv_usec/1000; 
		n = poll(&(ctx->pollfds[ctx->firstfd]), (ctx->lastfd - ctx->firstfd + 1),
						polltimeout);
	#else
		n = select(nfds, rfds, wfds, efds, tvp);
	#endif
		if (tsp)
			*tsp = evTimeSpec(tv);
		return (n);
	}
	#endif

	#ifdef SUNW_POLL
	void     
	evPollfdAdd(evContext opaqueCtx, int pollfd_chunk_size, int fd, short events)
	{
	    evContext_p *ctx = opaqueCtx.opaque;
	    int		old_maxnfds = ctx->maxnfds;
	    int 	i;
	    if (fd >= 0) {
		while (fd >= ctx->maxnfds) {
			ctx->maxnfds = ctx->maxnfds + pollfd_chunk_size;
		}
		if (ctx->maxnfds > old_maxnfds) {
		    if ((ctx->pollfds = (struct pollfd *)
			 realloc(ctx->pollfds,
				 ctx->maxnfds * sizeof(struct pollfd))) == NULL) {
			 evPrintf(ctx, 2, "pollfd() realloc (%lu) failed\n",
					ctx->maxnfds*sizeof(struct pollfd));
			 exit(1);
		    }
		    for (i = old_maxnfds; i < ctx->maxnfds; i++) {
			ctx->pollfds[i].fd = -1;
		    }
		}
		ctx->pollfds[fd].fd = fd;
		ctx->pollfds[fd].events = events;
		if (fd < ctx->firstfd || old_maxnfds == 0) ctx->firstfd = fd;
		if (fd > ctx->lastfd) ctx->lastfd = fd;
	    }
	    return;
	}

	void
	evPollfdDel(evContext opaqueCtx, int fd)
	{
	    evContext_p *ctx = opaqueCtx.opaque;
	    if (fd >= 0 && fd <= ctx->lastfd && ctx->pollfds[fd].fd == fd) {
		ctx->pollfds[fd].fd = -1;
		/* Make sure firstfd and lastfd really are */
		for ( ; ctx->pollfds[ctx->firstfd].fd < 0; ctx->firstfd++);
		for ( ; ctx->lastfd > 0 && ctx->pollfds[ctx->lastfd].fd < 0; 
								ctx->lastfd--);
	    }

	}
#endif
