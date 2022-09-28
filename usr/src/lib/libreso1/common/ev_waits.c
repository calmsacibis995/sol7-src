/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium
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
#pragma ident   "@(#)ev_waits.c 1.1     97/12/03 SMI"

/* ev_waits.c - implement deferred function calls for the eventlib
 * vix 05dec95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_waits.c,v 8.3 1997/05/21 19:26:33 halley Exp $";
#endif

#include "port_before.h"

#include <assert.h>
#include <errno.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

/*hidden*/ evWait *	evFreeWait(evContext_p *ctx, evWait *old);
static evWait *		evNewWait(evContext_p *ctx);
static void		print_waits(evContext_p *ctx);
static evWaitList *	evNewWaitList(evContext_p *);
static void		evFreeWaitList(evContext_p *, evWaitList *);
static evWaitList *	evGetWaitList(evContext_p *, const void *, int);


/* Public. */

/*
 * Enter a new wait function on the queue.
 */
int
evWaitFor(evContext opaqueCtx, const void *tag,
       evWaitFunc func, void *uap, evWaitID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evWait *new = evNewWait(ctx);
	evWaitList *wl = evGetWaitList(ctx, tag, 1);

	if (!new || !wl) {
		errno = ENOMEM;
		return (-1);
	}
	new->func = func;
	new->uap = uap;
	new->tag = tag;
	new->next = NULL;
	if (wl->last != NULL) {
		wl->last->next = new;
	} else {
		wl->first = new;
	}
	wl->last = new;
	if (id != NULL)
		id->opaque = new;
	if (ctx->debug >= 9)
		print_waits(ctx);
	return (0);
}

/*
 * Mark runnable all waiting functions having a certain tag.
 */
int
evDo(evContext opaqueCtx, const void *tag) {
	evContext_p *ctx = opaqueCtx.opaque;
	evWaitList *wl = evGetWaitList(ctx, tag, 0);
	evWait *first;

	if (!wl) {
		errno = ENOENT;
		return (-1);
	}

	first = wl->first;
	assert(first != NULL);

	if (ctx->waitDone.last != NULL)
		ctx->waitDone.last->next = first;
	else
		ctx->waitDone.first = first;
	ctx->waitDone.last = wl->last;
	evFreeWaitList(ctx, wl);

	return (0);
}

/*
 * Remove a waiting (or ready to run) function from the queue.
 */
int
evUnwait(evContext opaqueCtx, evWaitID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evWait *this, *prev;
	evWaitList *wl;
	int found = 0;

	this = id.opaque;
	assert(this != NULL);
	wl = evGetWaitList(ctx, this->tag, 0);
	if (wl != NULL) {
		for (prev = NULL, this = wl->first;
		     this != NULL;
		     prev = this, this = this->next)
			if (this == id.opaque) {
				found = 1;
				if (prev != NULL)
					prev->next = this->next;
				else
					wl->first = this->next;
				if (wl->last == this)
					wl->last = prev;
				if (wl->first == NULL)
					evFreeWaitList(ctx, wl);
				break;
			}
	}

	if (!found) {
		/* Maybe it's done */
		for (prev = NULL, this = ctx->waitDone.first;
		     this != NULL;
		     prev = this, this = this->next)
			if (this == id.opaque) {
				found = 1;
				if (prev != NULL)
					prev->next = this->next;
				else
					ctx->waitDone.first = this->next;
				if (ctx->waitDone.last == this)
					ctx->waitDone.last = prev;
				break;
			}
	}

	if (!found) {
		errno = ENOENT;
		return (-1);
	}

	evFreeWait(ctx, id.opaque);

	if (ctx->debug >= 9)
		print_waits(ctx);

	return (0);
}

/* Private. */

static evWait *
evNewWait(evContext_p *ctx) {
	evWait *new;

	if ((new = ctx->waitFree) != NULL) {
		ctx->waitFree = new->next;
		FILL(new);
	} else
		NEW(new);
	return (new);
}

/*hidden*/ evWait *
evFreeWait(evContext_p *ctx, evWait *old) {
	evWait *next = old->next;

	old->next = ctx->waitFree;
	ctx->waitFree = old;
	return (next);
}

static void
print_waits(evContext_p *ctx) {
	evWaitList *wl;
	evWait *this;

	evPrintf(ctx, 9, "wait waiting:\n");
	for (wl = ctx->waitLists; wl != NULL; wl = wl->next) {
		assert(wl->first != NULL);
		evPrintf(ctx, 9, "  tag %#x:", wl->first->tag);
		for (this = wl->first; this != NULL; this = this->next)
			evPrintf(ctx, 9, " %#x", this);
		evPrintf(ctx, 9, "\n");
	}
	evPrintf(ctx, 9, "wait done:");
	for (this = ctx->waitDone.first; this != NULL; this = this->next)
		evPrintf(ctx, 9, " %#x", this);
	evPrintf(ctx, 9, "\n");
	if (ctx->debug >= 99) {
		evPrintf(ctx, 99, "wait list free:");
		for (wl = ctx->waitListFree;
		     wl != NULL;
		     wl = wl->next)
			evPrintf(ctx, 99, " %#x", wl);
		evPrintf(ctx, 99, "\n");
		evPrintf(ctx, 99, "wait free:");
		for (this = ctx->waitFree;
		     this != NULL;
		     this = this->next)
			evPrintf(ctx, 99, " %#x", this);
		evPrintf(ctx, 99, "\n");
	}
}

static evWaitList *
evNewWaitList(evContext_p *ctx) {
	evWaitList *new;

	if (ctx->waitListFree != NULL) {
		new = ctx->waitListFree;
		ctx->waitListFree = ctx->waitListFree->next;
		FILL(new);
	} else {
		NEW(new);
		if (new == NULL)
			return (NULL);
	}
	new->first = new->last = NULL;
	new->prev = NULL;
	new->next = ctx->waitLists;
	ctx->waitLists = new;
	return (new);
}

static void
evFreeWaitList(evContext_p *ctx, evWaitList *this) {
	evWaitList *prev;

	assert(this != NULL);

	if (this->prev != NULL)
		this->prev->next = this->next;
	else
		ctx->waitLists = this->next;
	if (this->next != NULL)
		this->next->prev = this->prev;
	this->prev = NULL;	
	this->next = ctx->waitListFree;
	ctx->waitListFree = this;
}

static evWaitList *
evGetWaitList(evContext_p *ctx, const void *tag, int should_create) {
	evWaitList *this;

	for (this = ctx->waitLists; this != NULL; this = this->next) {
		if (this->first != NULL && this->first->tag == tag)
			break;
	}
	if (this == NULL && should_create)
		this = evNewWaitList(ctx);
	return (this);
}
