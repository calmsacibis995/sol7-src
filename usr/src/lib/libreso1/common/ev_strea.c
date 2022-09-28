/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/* Copyright (c) 1996, 1997 by Internet Software Consortium
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
#pragma ident   "@(#)ev_streams.c 1.1     97/12/03 SMI"

/* ev_streams.c - implement asynch stream file IO for the eventlib
 * vix 04mar96 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_streams.c,v 8.15 1997/05/21 19:26:32 halley Exp $";
#endif

#include "port_before.h"

#include <sys/types.h>
#include <sys/uio.h>

#include <assert.h>
#include <errno.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

static evStream	*newStream(evContext_p *);
static evStream	*freeStream(evContext_p *, evStream *);
static int	copyvec(evStream *str, const struct iovec *iov, int iocnt);
static void	consume(evStream *str, size_t bytes);
static void	done(evContext opaqueCtx, evStream *str);
static void	writable(evContext opaqueCtx, void *uap, int fd, int evmask);
static void	readable(evContext opaqueCtx, void *uap, int fd, int evmask);

struct iovec
evConsIovec(void *buf, size_t cnt) {
	struct iovec ret;

	memset(&ret, 0xf5, sizeof ret);
	ret.iov_base = buf;
	ret.iov_len = cnt;
	return (ret);
}

int
evWrite(evContext opaqueCtx, int fd, const struct iovec *iov, int iocnt,
	evStreamFunc func, void *uap, evStreamID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evStream *new = newStream(ctx);
	int save;

	if (!new) {
		errno = ENOMEM;
		goto err;
	}
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	if (evSelectFD(opaqueCtx, fd, EV_WRITE, writable, new, &new->file) < 0)
		goto free;
	if (copyvec(new, iov, iocnt) < 0)
		goto free;
	new->nextDone = NULL;
	new->next = ctx->streams;
	ctx->streams = new;
	if (id != NULL)
		id->opaque = new;
	return (0);
 free:
	save = errno;
	(void) freeStream(ctx, new);
	errno = save;
 err:
	return (-1);
}

int
evRead(evContext opaqueCtx, int fd, const struct iovec *iov, int iocnt,
       evStreamFunc func, void *uap, evStreamID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evStream *new = newStream(ctx);
	int save;

	if (!new) {
		errno = ENOMEM;
		goto err;
	}
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	if (evSelectFD(opaqueCtx, fd, EV_READ, readable, new, &new->file) < 0)
		goto free;
	if (copyvec(new, iov, iocnt) < 0)
		goto free;
	new->nextDone = NULL;
	new->next = ctx->streams;
	ctx->streams = new;
	if (id)
		id->opaque = new;
	return (0);
 free:
	save = errno;
	(void) freeStream(ctx, new);
	errno = save;
 err:
	return (-1);
}

int
evCancelRW(evContext opaqueCtx, evStreamID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evStream *old = id.opaque, *this, *prev;

	/*
	 * The streams list is doubly threaded.  First, there's ctx->streams
	 * that's used by evDestroy() to find and cancel all streams.  Second,
	 * there's ctx->strDone (head) and ctx->strLast (tail) which thread
	 * through the potentially smaller number of "IO completed" streams,
	 * used in evGetNext() to avoid scanning the entire list.
	 */

	/* Unlink from ctx->streams (head). */
	for (prev = NULL, this = ctx->streams;
	     this != NULL;
	     prev = this, this = this->next)
		if (this == old) {
			if (prev)
				prev->next = this->next;
			else
				ctx->streams = this->next;
			break;
		}
	if (this == NULL) {
		errno = ENOENT;
		return (-1);
	}

	/* Unlink (maybe) from ctx->strDone (head) and ctx->strLast (tail). */
	for (prev = NULL, this = ctx->strDone;
	     this != NULL;
	     prev = this, this = this->next)
		if (this == old) {
			if (prev)
				prev->nextDone = this->nextDone;
			else
				ctx->strDone = this->nextDone;
			if (ctx->strLast == this) {
				assert(this->nextDone == NULL);
				ctx->strLast = prev;
			}
		}

	/* Deallocate the stream. */
	if (old->file.opaque)
		evDeselectFD(opaqueCtx, old->file);
	free(old->iovOrig);
	freeStream(ctx, old);
	return (0);
}

static evStream *
newStream(evContext_p *ctx) {
	evStream *new;

	if ((new = ctx->strFree) != NULL) {
		ctx->strFree = new->next;
		FILL(new);
	} else
		NEW(new);
	return (new);
}

static evStream *
freeStream(evContext_p *ctx, evStream *old) {
	evStream *next = old->next;

	old->next = ctx->strFree;
	ctx->strFree = old;
	return (next);
}

/* Copy a scatter/gather vector and initialize a stream handler's IO. */
static int
copyvec(evStream *str, const struct iovec *iov, int iocnt) {
	int i;

	str->iovOrig = (struct iovec *)malloc(sizeof(struct iovec) * iocnt);
	if (!str->iovOrig) {
		errno = ENOMEM;
		return (-1);
	}
	str->ioTotal = 0;
	for (i = 0; i < iocnt; i++) {
		str->iovOrig[i] = iov[i];
		str->ioTotal += iov[i].iov_len;
	}
	str->iovOrigCount = iocnt;
	str->iovCur = str->iovOrig;
	str->iovCurCount = str->iovOrigCount;
	str->ioDone = 0;
	return (0);
}

/* Pull off or truncate lead iovec(s). */
static void
consume(evStream *str, size_t bytes) {
	while (bytes > 0) {
		if (bytes < str->iovCur->iov_len) {
			str->iovCur->iov_len -= bytes;
			str->iovCur->iov_base = (void *)
				((u_char *)str->iovCur->iov_base + bytes);
			str->ioDone += bytes;
			bytes = 0;
		} else {
			bytes -= str->iovCur->iov_len;
			str->ioDone += str->iovCur->iov_len;
			str->iovCur++;
			str->iovCurCount--;
		}
	}
}

/* Add a stream to Done list and deselect the FD. */
static void
done(evContext opaqueCtx, evStream *str) {
	evContext_p *ctx = opaqueCtx.opaque;

	if (ctx->strLast)
		ctx->strLast->nextDone = str;
	else {
		assert(ctx->strDone == NULL);
		ctx->strDone = ctx->strLast = str;
	}
	evDeselectFD(opaqueCtx, str->file);
	str->file.opaque = NULL;
	/* evDrop() will call evCancelRW() on us. */
}

/* Dribble out some bytes on the stream.  (Called by evDispatch().) */
static void
writable(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evStream *str = uap;
	int bytes;

	bytes = writev(fd, str->iovCur, str->iovCurCount);
	if (bytes > 0)
		consume(str, bytes);
	else {
		if (bytes < 0 && errno != EINTR) {
			str->ioDone = -1;
			str->ioErrno = errno;
		}
	}
	if (str->ioDone == -1 || str->ioDone == str->ioTotal)
		done(opaqueCtx, str);
}

/* Scoop up some bytes from the stream.  (Called by evDispatch().) */
static void
readable(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evStream *str = uap;
	int bytes;

	bytes = readv(fd, str->iovCur, str->iovCurCount);
	if (bytes > 0)
		consume(str, bytes);
	else {
		if (bytes == 0)
			str->ioDone = 0;
		else {
			if (errno != EINTR) {
				str->ioDone = -1;
				str->ioErrno = errno;
			}
		}
	}
	if (str->ioDone <= 0 || str->ioDone == str->ioTotal)
		done(opaqueCtx, str);
}
