/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* Copyright (c) 1995, 1996, 1997 by Internet Software Consortium
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
#pragma ident   "@(#)ev_files.c 1.1     97/12/03 SMI"

/* ev_files.c - implement asynch file IO for the eventlib
 * vix 11sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_files.c,v 1.11 1997/05/21 19:26:32 halley Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

static evFile *FindFD(const evContext_p *ctx, int fd);

int
evSelectFD(evContext opaqueCtx,
	   int fd,
	   int eventmask,
	   evFileFunc func,
	   void *uap,
	   evFileID *opaqueID
) {
	evContext_p *ctx = opaqueCtx.opaque;
	evFile *id;
	int mode;

	evPrintf(ctx, 1,
		 "evSelectFD(ctx %#x, fd %d, mask 0x%x, func %#x, uap %#x)\n",
		 ctx, fd, eventmask, func, uap);
	if (!eventmask || (eventmask & ~(EV_READ|EV_WRITE|EV_EXCEPT)))
		ERR(EINVAL);
	if (fd >= FD_SETSIZE)
		ERR(EINVAL);
	OK(mode = fcntl(fd, F_GETFL, NULL));	/* side effect: validate fd. */

	/*
	 * The first time we touch a file descriptor, we need to check to see
	 * if the application already had it in O_NONBLOCK mode and if so, all
	 * of our deselect()'s have to leave it in O_NONBLOCK.  If not, then
	 * all but our last deselect() has to leave it in O_NONBLOCK.
	 */
	if (! (id = FindFD(ctx, fd))) {
		if (mode & O_NONBLOCK)
			FD_SET(fd, &ctx->nonblockBefore);
		else {
			OK(fcntl(fd, F_SETFL, mode | O_NONBLOCK));
			FD_CLR(fd, &ctx->nonblockBefore);
		}
	}

	/* Allocate and fill. */
	OKNEW(id);
	id->func = func;
	id->uap = uap;
	id->fd = fd;
	id->eventmask = eventmask;

	/*
	 * Insert at head.  Order could be important for performance if we
	 * believe that evGetNext()'s accesses to the fd_sets will be more
	 * serial and therefore more cache-lucky if the list is ordered by
	 * ``fd.''  We do not believe these things, so we don't do it.
	 *
	 * The interesting sequence is where GetNext() has cached a select()
	 * result and the caller decides to evSelectFD() on some descriptor.
	 * Since GetNext() starts at the head, it can miss new entries we add
	 * at the head.  This is not a serious problem since the event being
	 * evSelectFD()'d for has to occur before evSelectFD() is called for
	 * the file event to be considered "missed" -- a real corner case.
	 * Maintaining a "tail" pointer for ctx->files would fix this, but I'm
	 * not sure it would be ``more correct.''
	 */
	id->next = ctx->files;
	ctx->files = id;

	/* Turn on the appropriate bits in the {rd,wr,ex}Next fd_set's. */
	if (eventmask & EV_READ)
		FD_SET(fd, &ctx->rdNext);
	if (eventmask & EV_WRITE)
		FD_SET(fd, &ctx->wrNext);
	if (eventmask & EV_EXCEPT)
		FD_SET(fd, &ctx->exNext);

	/* Update fdMax. */
	if (fd > ctx->fdMax)
		ctx->fdMax = fd;

	/* Remember the ID if the caller provided us a place for it. */
	if (opaqueID)
		opaqueID->opaque = id;

	evPrintf(ctx, 5,
		"evSelectFD(fd %d, mask 0x%x): new masks: 0x%lx 0x%lx 0x%lx\n",
		 fd, eventmask,
		 (u_long)ctx->rdNext.fds_bits[0],
		 (u_long)ctx->wrNext.fds_bits[0],
		 (u_long)ctx->exNext.fds_bits[0]);

	return (0);
}

int
evDeselectFD(evContext opaqueCtx, evFileID opaqueID) {
	evContext_p *ctx = opaqueCtx.opaque;
	evFile *del = opaqueID.opaque;
	evFile *old, *cur;
	int mode, eventmask;

	if (!del) {
		evPrintf(ctx, 11, "evDeselectFD(NULL) ignored\n");
		errno = EINVAL;
		return (-1);
	}

	evPrintf(ctx, 1, "evDeselectFD(fd %d, mask 0x%x)\n",
		 del->fd, del->eventmask);

	/* Get the mode.  Unless the file has been closed, errors are bad. */
	mode = fcntl(del->fd, F_GETFL, NULL);
	if (mode == -1 && errno != EBADF)
		ERR(errno);

	/* Remove this ID.  Its absense is an ENOENT error. */
	for (old = NULL, cur = ctx->files;
	     cur != NULL && cur != del;
	     old = cur, cur = cur->next)
		(void)NULL;
	if (! cur)
		ERR(ENOENT);
	if (! old)
		ctx->files = del->next;
	else
		old->next = del->next;

	/*
	 * If the file descriptor does not appear in any other select() entry,
	 * and if !EV_WASNONBLOCK, and if we got no EBADF when we got the mode
	 * earlier, then: restore the fd to blocking status.
	 */
	if (!(cur = FindFD(ctx, del->fd)) &&
	    FD_ISSET(del->fd, &ctx->nonblockBefore) &&
	    mode != -1) {
		/*
		 * Note that we won't return an error status to the caller if
		 * this fcntl() fails since (a) we've already done the work
		 * and (b) the caller didn't ask us anything about O_NONBLOCK.
		 */
		(void) fcntl(del->fd, F_SETFL, mode & ~O_NONBLOCK);
	}

	/*
	 * Now find all other uses of this descriptor and OR together an event
	 * mask so that we don't turn off {rd,wr,ex}Next bits that some other
	 * file event is using.  As an optimization, stop if the event mask
	 * fills.
	 */
	eventmask = 0;
	for ((void)NULL;
	     cur != NULL && eventmask != (EV_READ|EV_WRITE|EV_EXCEPT);
	     cur = cur->next)
		if (cur->fd == del->fd)
			eventmask |= cur->eventmask;

	/* OK, now we know which bits we can clear out. */
	if (!(eventmask & EV_READ))
		FD_CLR(del->fd, &ctx->rdNext);
	if (!(eventmask & EV_WRITE))
		FD_CLR(del->fd, &ctx->wrNext);
	if (!(eventmask & EV_EXCEPT))
		FD_CLR(del->fd, &ctx->exNext);

	/* If this was the maxFD, find the new one. */
	if (del->fd == ctx->fdMax) {
		ctx->fdMax = -1;
		for (cur = ctx->files; cur; cur = cur->next)
			if (cur->fd > ctx->fdMax)
				ctx->fdMax = cur->fd;
	}

	/* If this was the fdNext, cycle that to the next entry. */
	if (del == ctx->fdNext)
		ctx->fdNext = del->next;

	evPrintf(ctx, 5,
	      "evDeselectFD(fd %d, mask 0x%x): new masks: 0x%lx 0x%lx 0x%lx\n",
		 del->fd, eventmask,
		 (u_long)ctx->rdNext.fds_bits[0],
		 (u_long)ctx->wrNext.fds_bits[0],
		 (u_long)ctx->exNext.fds_bits[0]);

	/* Couldn't free it before now since we were using fields out of it. */
	(void) free(del);

	return (0);
}

static evFile *
FindFD(const evContext_p *ctx, int fd) {
	evFile *id;

	for (id = ctx->files; id != NULL && id->fd != fd; id = id->next)
		(void)NULL;
	return (id);
}
