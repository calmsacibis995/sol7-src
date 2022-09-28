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
#pragma ident   "@(#)ev_connects.c 1.1     97/12/03 SMI"

/* ev_connects.c - implement asynch connect/accept for the eventlib
 * vix 16sep96 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_connects.c,v 8.9 1997/05/21 19:25:36 halley Exp $";
#endif

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

static void	listener(evContext ctx, void *uap, int fd, int evmask);
static void	connector(evContext ctx, void *uap, int fd, int evmask);

/* Public. */

int
evListen(evContext opaqueCtx, int fd, int maxconn,
	 evConnFunc func, void *uap, evConnID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *new;

	OK(listen(fd, maxconn));
	OKNEW(new);
	if (evSelectFD(opaqueCtx, fd, EV_READ, listener, new, &new->file) < 0){
		int save = errno;

		free(new);
		errno = save;
		return (-1);
	}
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	new->type = Listen;
	new->next = ctx->conns;
	ctx->conns = new;
	if (id)
		id->opaque = new;
	return (0);
}

int
evConnect(evContext opaqueCtx, int fd, void *ra, int ralen,
	  evConnFunc func, void *uap, evConnID *id)
{
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *new;

	OKNEW(new);
	/* Do the select() first to get the socket into nonblocking mode. */
	if (evSelectFD(opaqueCtx, fd, EV_READ|EV_WRITE|EV_EXCEPT,
		       connector, new, &new->file) < 0 ||
	    (connect(fd, ra, ralen) < 0 &&
	     errno != EWOULDBLOCK &&
	     errno != EAGAIN &&
	     errno != EINPROGRESS)) {
		int save = errno;

		evDeselectFD(opaqueCtx, new->file);
		free(new);
		errno = save;
		return (-1);
	}
	/* No error, or EWOULDBLOCK.  select() tells when it's ready. */
	new->func = func;
	new->uap = uap;
	new->fd = fd;
	new->type = Connect;
	new->next = ctx->conns;
	ctx->conns = new;
	if (id)
		id->opaque = new;
	return (0);
}

int
evCancelConn(evContext opaqueCtx, evConnID id) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *prev, *this, *next;

	prev = NULL;
	for (this = ctx->conns; this != NULL; this = next) {
		next = this->next;
		if (this == id.opaque) {
			(void) evDeselectFD(opaqueCtx, this->file);
			(void) close(this->fd);
			free(this);
			if (prev)
				prev->next = next;
			else
				ctx->conns = next;
			return (0);
		}
		prev = this;
	}
	errno = ENOENT;
	return (-1);
}

/* Private. */

static void
listener(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *conn = uap;
	struct sockaddr la, ra;
	int new; 
	u_int	lalen, ralen;

	assert((evmask & EV_READ) != 0);
	ralen = sizeof ra;
	new = accept(fd, &ra, &ralen);
	if (new >= 0) {
		int save = errno;

		lalen = sizeof la;
		if (getsockname(new, &la, &lalen) < 0) {
			(void) close(new);
			new = -1;
		}
		errno = save;
	}
	(*conn->func)(opaqueCtx, conn->uap, new, &la, lalen, &ra, ralen);
}

static void
connector(evContext opaqueCtx, void *uap, int fd, int evmask) {
	evContext_p *ctx = opaqueCtx.opaque;
	evConn *conn = uap;
	struct sockaddr la, ra;
	u_int lalen, ralen;
	char buf[1];

	lalen = sizeof la;
	ralen = sizeof ra;
	if (evDeselectFD(opaqueCtx, conn->file) < 0 ||
	    getsockname(fd, &la, &lalen) < 0 ||
	    getpeername(fd, &ra, &ralen) < 0 ||
#ifdef NETREAD_BROKEN
	    0
#else
	    read(fd, buf, 0) < 0
#endif
	    ) {
		int save = errno;

		(void) close(fd);
		errno = save;
		fd = -1;
	}
	(*conn->func)(opaqueCtx, conn->uap, fd, &la, lalen, &ra, ralen);
}

