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

#pragma ident   "@(#)eventlib_p.h 1.1     97/12/03 SMI"

/* eventlib_p.h - private interfaces for eventlib
 * vix 09sep95 [initial]
 *
 * $Id: eventlib_p.h,v 1.18 1997/05/21 19:26:58 halley Exp $
 */

#ifndef _EVENTLIB_P_H
#define _EVENTLIB_P_H

#include <sys/param.h>
#include <sys/types.h>

#define EVENTLIB_DEBUG 1

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc/heap.h>


#define ERR(e)		return (errno = (e), -1)
#define OK(x)		if ((x) < 0) ERR(errno); else (void)NULL

#define	NEW(p)		if (((p) = malloc(sizeof *(p))) != NULL) \
				FILL(p); \
			else \
				(void)NULL;
#define OKNEW(p)	if (!((p) = malloc(sizeof *(p)))) { \
				errno = ENOMEM; \
				return (-1); \
			} else \
				FILL(p)

#if EVENTLIB_DEBUG
#define FILL(p)		memset((p), 0xF5, sizeof *(p))
#else
#define FILL(p)
#endif

#ifdef SUNW_POLL
#include <stropts.h>
#include <poll.h>
#endif

/* These are potentially bad assumptions about the underlying system. */
#define FIRST_SIG	1
#define LAST_SIG	31

typedef struct evConn {
	evConnFunc	func;
	void *		uap;
	int		fd;
	enum {Listen, Connect}
			type;
	evFileID	file;
	struct evConn *	next;
} evConn;

typedef struct evFile {
	evFileFunc	func;
	void *		uap;
	int		fd;
	int		eventmask;
	int		preemptive;
	struct evFile *	next;
} evFile;

typedef struct evStream {
	evStreamFunc	func;
	void *		uap;
	evFileID	file;
	int		fd;
	struct iovec *	iovOrig;
	int		iovOrigCount;
	struct iovec *	iovCur;
	int		iovCurCount;
	int		ioTotal;
	int		ioDone;
	int		ioErrno;
	struct evStream	*nextDone;
	struct evStream	*next;
} evStream;

typedef struct evTimer {
	evTimerFunc	func;
	void *		uap;
	struct timespec	due, inter;
	int		index;
} evTimer;

typedef struct evWait {
	evWaitFunc	func;
	void *		uap;
	const void *	tag;
	struct evWait *	next;
} evWait;

typedef struct evWaitList {
	evWait *		first;
	evWait *		last;
	struct evWaitList *	prev;
	struct evWaitList *	next;
} evWaitList;

typedef struct evEvent_p {
	enum {  Conn, File, Stream, Timer, Wait, Free, Null  } type;
	union {
		struct {  evConn *this;  }			conn;
		struct {  evFile *this; int eventmask;  }	file;
		struct {  evStream *this;  }			stream;
		struct {  evTimer *this;  }			timer;
		struct {  evWait *this;  }			wait;
		struct {  struct evEvent_p *next;  }		free;
		struct {  const void *placeholder;  }		null;
	} u;
} evEvent_p;

typedef struct {
	/* Debugging. */
	int		debug;
	FILE		*output;
	/* Connections. */
	evConn		*conns;
	/* Files. */
	evFile		*files, *fdNext;
	fd_set		rdLast, rdNext;
	fd_set		wrLast, wrNext;
	fd_set		exLast, exNext;
	fd_set		nonblockBefore;
	int		fdMax, fdCount;
	/* Streams. */
	evStream	*streams, *strFree;
	evStream	*strDone, *strLast;
	/* Timers. */
	heap_context	timers;
	/* Waits. */
	evWaitList	*waitLists;
	evWaitList	waitDone;
	evWaitList	*waitListFree;
	evWait		*waitFree;
	/* Housekeeping. */
	evEvent_p *	evFree;
#ifdef SUNW_POLL
	struct pollfd           *pollfds;
	int			maxnfds;
	int			firstfd;
	int			lastfd;
#endif

} evContext_p;

/* eventlib.c */
#define evPrintf __evPrintf
void evPrintf(const evContext_p *ctx, int level, const char *fmt, ...);

/* ev_timers.c */
#define evCreateTimers __evCreateTimers
heap_context evCreateTimers(const evContext_p *);
#define evDestroyTimers __evDestroyTimers
void evDestroyTimers(const evContext_p *);

/* ev_waits.c */
#define evFreeWait __evFreeWait
evWait *evFreeWait(evContext_p *ctx, evWait *old);

#endif /*_EVENTLIB_P_H*/
