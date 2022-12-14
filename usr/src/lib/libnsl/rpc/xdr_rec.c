
/*
 * Copyright (c) 1984 - 1991 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)xdr_rec.c	1.25	97/12/17 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)xdr_rec.c	1.25	97/12/17 SMI";
#endif

/*
 * xdr_rec.c, Implements (TCP/IP based) XDR streams with a "record marking"
 * layer above connection oriented transport layer (e.g. tcp) (for rpc's use).
 *
 *
 * These routines interface XDRSTREAMS to a (tcp/ip) connection transport.
 * There is a record marking layer between the xdr stream
 * and the (tcp) cv transport level.  A record is composed on one or more
 * record fragments.  A record fragment is a thirty-two bit header followed
 * by n bytes of data, where n is contained in the header.  The header
 * is represented as a htonl(u_long).  The order bit encodes
 * whether or not the fragment is the last fragment of the record
 * (1 => fragment is last, 0 => more fragments to follow.
 * The other 31 bits encode the byte length of the fragment.
 */

#include "rpc_mt.h"
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <syslog.h>
#include <memory.h>
#include <stdlib.h>

extern long	lseek();

static u_int	fix_buf_size();
static struct	xdr_ops *xdrrec_ops();
static bool_t	xdrrec_getbytes();
static bool_t	flush_out();
static bool_t	get_input_bytes();
static bool_t	set_input_fragment();
static bool_t	skip_input_bytes();

/*
 * A record is composed of one or more record fragments.
 * A record fragment is a four-byte header followed by zero to
 * 2**32-1 bytes.  The header is treated as a long unsigned and is
 * encode/decoded to the network via htonl/ntohl.  The low order 31 bits
 * are a byte count of the fragment.  The highest order bit is a boolean:
 * 1 => this fragment is the last fragment of the record,
 * 0 => this fragment is followed by more fragment(s).
 *
 * The fragment/record machinery is not general;  it is constructed to
 * meet the needs of xdr and rpc based on tcp.
 */

#define	LAST_FRAG (((uint32_t)1 << 31))

typedef struct rec_strm {
	caddr_t tcp_handle;
	caddr_t the_buffer;
	/*
	 * out-going bits
	 */
	int (*writeit)();
	caddr_t out_base;	/* output buffer (points to frag header) */
	caddr_t out_finger;	/* next output position */
	caddr_t out_boundry;	/* data cannot up to this address */
	uint32_t *frag_header;	/* beginning of current fragment */
	bool_t frag_sent;	/* true if buffer sent in middle of record */
	/*
	 * in-coming bits
	 */
	int (*readit)();
	u_int in_size;		/* fixed size of the input buffer */
	caddr_t in_base;
	caddr_t in_finger;	/* location of next byte to be had */
	caddr_t in_boundry;	/* can read up to this location */
	int fbtbc;		/* fragment bytes to be consumed */
	bool_t last_frag;
	u_int sendsize;
	u_int recvsize;
	/*
	 * Is this the first time that the
	 * getbytes routine has been called ?
	 */
	u_int firsttime;
} RECSTREAM;


/*
 * Create an xdr handle for xdrrec
 * xdrrec_create fills in xdrs.  Sendsize and recvsize are
 * send and recv buffer sizes (0 => use default).
 * vc_handle is an opaque handle that is passed as the first parameter to
 * the procedures readit and writeit.  Readit and writeit are read and
 * write respectively. They are like the system calls expect that they
 * take an opaque handle rather than an fd.
 */

static const char mem_err_msg_rec[] = "xdrrec_create: out of memory";

void
xdrrec_create(xdrs, sendsize, recvsize, tcp_handle, readit, writeit)
	register XDR *xdrs;
	register u_int sendsize;
	register u_int recvsize;
	caddr_t tcp_handle;
	int (*readit)();  /* like read, but pass it a vc_handle, not fd */
	int (*writeit)(); /* like write, but pass it a vc_handle, not fd */
{
	register RECSTREAM *rstrm =
		(RECSTREAM *)mem_alloc(sizeof (RECSTREAM));

	trace3(TR_xdrrec_create, 0, sendsize, recvsize);
	if (rstrm == NULL) {
		(void) syslog(LOG_ERR, mem_err_msg_rec);
		/*
		 *  XXX: This is bad.  Should rework xdrrec_create to
		 *  return a handle, and in this case return NULL
		 */
		trace1(TR_xdrrec_create, 1);
		return;
	}
	/*
	 * adjust sizes and allocate buffer quad byte aligned
	 */
	rstrm->sendsize = sendsize = fix_buf_size(sendsize);
	rstrm->recvsize = recvsize = fix_buf_size(recvsize);
	rstrm->the_buffer = (caddr_t)mem_alloc(sendsize +
					recvsize + BYTES_PER_XDR_UNIT);
	if (rstrm->the_buffer == NULL) {
		(void) syslog(LOG_ERR, mem_err_msg_rec);
		(void) mem_free((char *) rstrm, sizeof (RECSTREAM));
		trace1(TR_xdrrec_create, 1);
		return;
	}
	for (rstrm->out_base = rstrm->the_buffer;
		(u_int)rstrm->out_base % BYTES_PER_XDR_UNIT != 0;
		rstrm->out_base++);
	rstrm->in_base = rstrm->out_base + sendsize;
	/*
	 * now the rest ...
	 */

	xdrs->x_ops = xdrrec_ops();
	xdrs->x_private = (caddr_t)rstrm;
	rstrm->tcp_handle = tcp_handle;
	rstrm->readit = readit;
	rstrm->writeit = writeit;
	rstrm->out_finger = rstrm->out_boundry = rstrm->out_base;
	rstrm->frag_header = (uint32_t *)rstrm->out_base;
	rstrm->out_finger += sizeof (u_int);
	rstrm->out_boundry += sendsize;
	rstrm->frag_sent = FALSE;
	rstrm->in_size = recvsize;
	rstrm->in_boundry = rstrm->in_base;
	rstrm->in_finger = (rstrm->in_boundry += recvsize);
	rstrm->fbtbc = 0;
	rstrm->last_frag = TRUE;
	rstrm->firsttime = 0;

	trace1(TR_xdrrec_create, 1);
}

/*
 * Align input stream.  If all applications behaved correctly, this
 * defensive procedure will not be necessary, since received data will be
 * aligned correctly.
 */
static void
align_instream(register RECSTREAM *rstrm)
{
	int current = rstrm->in_boundry - rstrm->in_finger;

	(void) memcpy(rstrm->in_base, rstrm->in_finger, current);
	rstrm->in_finger = rstrm->in_base;
	rstrm->in_boundry = rstrm->in_finger + current;
}

/*
 * The routines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */
static bool_t
xdrrec_getlong(xdrs, lp)
	XDR *xdrs;
	long *lp;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register long *buflp = (long *)(rstrm->in_finger);
	long mylong;

	trace1(TR_xdrrec_getlong, 0);
	/* first try the inline, fast case */
	if ((rstrm->fbtbc >=  sizeof (long)) &&
		(((long *)rstrm->in_boundry - buflp) >=  sizeof (long))) {
		/*
		 * Check if buflp is longword aligned.  If not, align it.
		 */
		if (((int)buflp) & ((int) sizeof (long) - 1)) {
			align_instream(rstrm);
			buflp = (long *)(rstrm->in_finger);
		}
		*lp = (long)ntohl((u_long)(*buflp));
		rstrm->fbtbc -= (int) sizeof (long);
		rstrm->in_finger += sizeof (long);
	} else {
		if (! xdrrec_getbytes(xdrs, (caddr_t)&mylong, sizeof (long))) {
			trace1(TR_xdrrec_getlong, 1);
			return (FALSE);
		}
		*lp = (long)ntohl((u_long)mylong);
	}
	trace1(TR_xdrrec_getlong, 1);
	return (TRUE);
}

static bool_t
xdrrec_putlong(xdrs, lp)
	XDR *xdrs;
	long *lp;
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register long *dest_lp = ((long *)(rstrm->out_finger));

	trace1(TR_xdrrec_putlong, 0);
	if ((rstrm->out_finger += sizeof (long)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof (long);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE)) {
			trace1(TR_xdrrec_putlong, 1);
			return (FALSE);
		}
		dest_lp = ((long *)(rstrm->out_finger));
		rstrm->out_finger += sizeof (long);
	}
	*dest_lp = (long)htonl((u_long)(*lp));
	trace1(TR_xdrrec_putlong, 1);
	return (TRUE);
}

#if defined(_LP64)

/*
 * The routines defined below are the xdr ops which will go into the
 * xdr handle filled in by xdrrec_create.
 */
static bool_t
xdrrec_getint32(XDR *xdrs, int32_t *ip)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int32_t *buflp = (int32_t *)(rstrm->in_finger);
	int32_t mylong;

	trace1(TR_xdrrec_getint32, 0);
	/* first try the inline, fast case */
	if ((rstrm->fbtbc >= (int) sizeof (int32_t)) &&
		((uintptr_t)((int32_t *)rstrm->in_boundry - buflp) >=
					(u_int) sizeof (int32_t))) {
		/*
		 * Check if buflp is longword aligned.  If not, align it.
		 */
		if (((uintptr_t)buflp) & ((int) sizeof (int32_t) - 1)) {
			align_instream(rstrm);
			buflp = (int32_t *)(rstrm->in_finger);
		}
		*ip = (int32_t)ntohl((uint32_t)(*buflp));
		rstrm->fbtbc -= (int) sizeof (int32_t);
		rstrm->in_finger += sizeof (int32_t);
	} else {
		if (!xdrrec_getbytes(xdrs, &mylong, sizeof (int32_t))) {
			trace1(TR_xdrrec_getint32_t, 1);
			return (FALSE);
		}
		*ip = (int32_t)ntohl((uint32_t)mylong);
	}
	trace1(TR_xdrrec_getint32, 1);
	return (TRUE);
}

static bool_t
xdrrec_putint32(XDR *xdrs, int32_t *ip)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int32_t *dest_lp = ((int32_t *)(rstrm->out_finger));

	trace1(TR_xdrrec_putint32, 0);
	if ((rstrm->out_finger += sizeof (int32_t)) > rstrm->out_boundry) {
		/*
		 * this case should almost never happen so the code is
		 * inefficient
		 */
		rstrm->out_finger -= sizeof (int32_t);
		rstrm->frag_sent = TRUE;
		if (! flush_out(rstrm, FALSE)) {
			trace1(TR_xdrrec_putint32_t, 1);
			return (FALSE);
		}
		dest_lp = ((int32_t *)(rstrm->out_finger));
		rstrm->out_finger += sizeof (int32_t);
	}
	*dest_lp = (int32_t)htonl((uint32_t)(*ip));
	trace1(TR_xdrrec_putint32, 1);
	return (TRUE);
}

#endif /* _LP64 */


static bool_t	/* must manage buffers, fragments, and records */
xdrrec_getbytes(XDR *xdrs, caddr_t addr, int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int current;

	trace2(TR_xdrrec_getbytes, 0, len);
	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag) {
				trace1(TR_xdrrec_getbytes, 1);
				return (FALSE);
			}
			if (! set_input_fragment(rstrm)) {
				trace1(TR_xdrrec_getbytes, 1);
				return (FALSE);
			}
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current, FALSE)) {
			trace1(TR_xdrrec_getbytes, 1);
			return (FALSE);
		}
		addr += current;
		rstrm->fbtbc -= current;
		len -= current;
	}
	trace1(TR_xdrrec_getbytes, 1);
	return (TRUE);
}

static bool_t
xdrrec_putbytes(XDR *xdrs, caddr_t addr, int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int current;

	trace2(TR_xdrrec_putbytes, 0, len);
	while (len > 0) {

		current = (u_int)rstrm->out_boundry - (u_int)rstrm->out_finger;
		current = (len < current) ? len : current;
		(void) memcpy(rstrm->out_finger, addr, current);
		rstrm->out_finger += current;
		addr += current;
		len -= current;
		if (rstrm->out_finger == rstrm->out_boundry) {
			rstrm->frag_sent = TRUE;
			if (! flush_out(rstrm, FALSE)) {
				trace1(TR_xdrrec_putbytes, 1);
				return (FALSE);
			}
		}
	}
	trace1(TR_xdrrec_putbytes, 1);
	return (TRUE);
}
/*
 * This is just like the ops vector x_getbytes(), except that
 * instead of returning success or failure on getting a certain number
 * of bytes, it behaves much more like the read() system call against a
 * pipe -- it returns up to the number of bytes requested and a return of
 * zero indicates end-of-record.  A -1 means something very bad happened.
 */
u_int /* must manage buffers, fragments, and records */
xdrrec_readbytes(XDR *xdrs, caddr_t addr, u_int l)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register int current, len;

	len = l;
	while (len > 0) {
		current = rstrm->fbtbc;
		if (current == 0) {
			if (rstrm->last_frag)
				return (l - len);
			if (! set_input_fragment(rstrm))
				return ((u_int) -1);
			continue;
		}
		current = (len < current) ? len : current;
		if (! get_input_bytes(rstrm, addr, current, FALSE))
			return ((u_int) -1);
		addr += current;
		rstrm->fbtbc -= current;
		len -= current;
	}
	return (l - len);
}

static u_int
xdrrec_getpos(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	register int32_t pos;

	trace1(TR_xdrrec_getpos, 0);
	pos = lseek((int)rstrm->tcp_handle, (int32_t) 0, 1);
	if (pos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			pos += rstrm->out_finger - rstrm->out_base;
			break;

		case XDR_DECODE:
			pos -= rstrm->in_boundry - rstrm->in_finger;
			break;

		default:
			pos = (u_int) -1;
			break;
		}
	trace1(TR_xdrrec_getpos, 1);
	return ((u_int) pos);
}

static bool_t
xdrrec_setpos(XDR *xdrs, u_int pos)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	u_int currpos = xdrrec_getpos(xdrs);
	int delta = currpos - pos;
	caddr_t newpos;

	trace2(TR_xdrrec_setpos, 0, pos);
	if ((int)currpos != -1)
		switch (xdrs->x_op) {

		case XDR_ENCODE:
			newpos = rstrm->out_finger - delta;
			if ((newpos > (caddr_t)(rstrm->frag_header)) &&
				(newpos < rstrm->out_boundry)) {
				rstrm->out_finger = newpos;
				trace1(TR_xdrrec_setpos, 1);
				return (TRUE);
			}
			break;

		case XDR_DECODE:
			newpos = rstrm->in_finger - delta;
			if ((delta < (int)(rstrm->fbtbc)) &&
				(newpos <= rstrm->in_boundry) &&
				(newpos >= rstrm->in_base)) {
				rstrm->in_finger = newpos;
				rstrm->fbtbc -= delta;
				trace1(TR_xdrrec_setpos, 1);
				return (TRUE);
			}
			break;
		}
	trace1(TR_xdrrec_setpos, 1);
	return (FALSE);
}

static rpc_inline_t *
xdrrec_inline(XDR *xdrs, int len)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;
	rpc_inline_t * buf = NULL;

	trace2(TR_xdrrec_inline, 0, len);
	switch (xdrs->x_op) {

	case XDR_ENCODE:
		if ((rstrm->out_finger + len) <= rstrm->out_boundry) {
			buf = (rpc_inline_t *) rstrm->out_finger;
			rstrm->out_finger += len;
		}
		break;

	case XDR_DECODE:
		if ((len <= rstrm->fbtbc) &&
			((rstrm->in_finger + len) <= rstrm->in_boundry)) {
			/*
			 * Check if rstrm->in_finger is longword aligned;
			 * if not, align it.
			 */
			if (((int)rstrm->in_finger) &
					((int)sizeof (int32_t) - 1))
				align_instream(rstrm);
			buf = (rpc_inline_t *) rstrm->in_finger;
			rstrm->fbtbc -= len;
			rstrm->in_finger += len;
		}
		break;
	}
	trace1(TR_xdrrec_inline, 1);
	return (buf);
}

static void
xdrrec_destroy(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)xdrs->x_private;

	trace1(TR_xdrrec_destroy, 0);
	mem_free(rstrm->the_buffer,
		rstrm->sendsize + rstrm->recvsize + BYTES_PER_XDR_UNIT);
	mem_free((caddr_t)rstrm, sizeof (RECSTREAM));
	trace1(TR_xdrrec_destroy, 1);
}


/*
 * Exported routines to manage xdr records
 */

/*
 * Before reading (deserializing) from the stream, one should always call
 * this procedure to guarantee proper record alignment.
 */
bool_t
xdrrec_skiprecord(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	trace1(TR_xdrrec_skiprecord, 0);
	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc)) {
			trace1(TR_xdrrec_skiprecord, 1);
			return (FALSE);
		}
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm))) {
			trace1(TR_xdrrec_skiprecord, 1);
			return (FALSE);
		}
	}
	rstrm->last_frag = FALSE;
	trace1(TR_xdrrec_skiprecord, 1);
	return (TRUE);
}

/*
 * Look ahead fuction.
 * Returns TRUE iff there is no more input in the buffer
 * after consuming the rest of the current record.
 */
bool_t
xdrrec_eof(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	trace1(TR_xdrrec_eof, 0);
	while (rstrm->fbtbc > 0 || (! rstrm->last_frag)) {
		if (! skip_input_bytes(rstrm, rstrm->fbtbc)) {
			trace1(TR_xdrrec_eof, 1);
			return (TRUE);
		}
		rstrm->fbtbc = 0;
		if ((! rstrm->last_frag) && (! set_input_fragment(rstrm))) {
			trace1(TR_xdrrec_eof, 1);
			return (TRUE);
		}
	}
	if (rstrm->in_finger == rstrm->in_boundry) {
		trace1(TR_xdrrec_eof, 1);
		return (TRUE);
	}
	trace1(TR_xdrrec_eof, 1);
	return (FALSE);
}

/*
 * The client must tell the package when an end-of-record has occurred.
 * The second parameters tells whether the record should be flushed to the
 * (output) tcp stream.  (This let's the package support batched or
 * pipelined procedure calls.)  TRUE => immmediate flush to tcp connection.
 */
bool_t
xdrrec_endofrecord(XDR *xdrs, bool_t sendnow)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	register uint32_t len;	/* fragment length */
	bool_t dummy;

	trace1(TR_xdrrec_endofrecord, 0);
	if (sendnow || rstrm->frag_sent ||
		((uint32_t)rstrm->out_finger + (u_int) sizeof (uint32_t) >=
		(uint32_t)rstrm->out_boundry)) {
		rstrm->frag_sent = FALSE;
		dummy = flush_out(rstrm, TRUE);
		trace1(TR_xdrrec_endofrecord, 1);
		return (dummy);
	}
	len = (uint32_t)(rstrm->out_finger) - (uint32_t)(rstrm->frag_header) -
		(u_int) sizeof (uint32_t);
	*(rstrm->frag_header) = htonl((uint32_t)len | LAST_FRAG);
	rstrm->frag_header = (uint32_t *)rstrm->out_finger;
	rstrm->out_finger += sizeof (uint32_t);
	trace1(TR_xdrrec_endofrecord, 1);
	return (TRUE);
}


/*
 * Internal useful routines
 */
static bool_t
flush_out(RECSTREAM *rstrm, bool_t eor)
{
	register uint32_t eormask = (eor == TRUE) ? LAST_FRAG : 0;
	register uint32_t len = (uint32_t)(rstrm->out_finger) -
		(uint32_t)(rstrm->frag_header) - (uint32_t) sizeof (uint32_t);

	trace1(TR_flush_out, 0);
	*(rstrm->frag_header) = htonl(len | eormask);


	len = (uint32_t)(rstrm->out_finger) - (uint32_t)(rstrm->out_base);

	if ((*(rstrm->writeit))(rstrm->tcp_handle, rstrm->out_base, (int)len)
		!= (int)len) {
		trace1(TR_flush_out, 1);
		return (FALSE);
	}
	rstrm->frag_header = (uint32_t *)rstrm->out_base;
	rstrm->out_finger = (caddr_t)rstrm->out_base + sizeof (uint32_t);
	trace1(TR_flush_out, 1);
	return (TRUE);
}

/* knows nothing about records!  Only about input buffers */
static bool_t
fill_input_buf(RECSTREAM *rstrm, bool_t do_align)
{
	register caddr_t where;
	register int len;

	trace1(TR_fill_input_buf, 0);
	where = rstrm->in_base;
	if (do_align) {
		len = rstrm->in_size;
	} else {
		u_int i = (u_int)rstrm->in_boundry % BYTES_PER_XDR_UNIT;

		where += i;
		len = rstrm->in_size - i;
	}
	if ((len = (*(rstrm->readit))(rstrm->tcp_handle, where, len)) == -1) {
		trace1(TR_fill_input_buf, 1);
		return (FALSE);
	}
	rstrm->in_finger = where;
	where += len;
	rstrm->in_boundry = where;
	trace1(TR_fill_input_buf, 1);
	return (TRUE);
}

/* knows nothing about records!  Only about input buffers */
static bool_t
get_input_bytes(RECSTREAM *rstrm, caddr_t addr,
		int len, bool_t do_align)
{
	register int current;

	trace2(TR_get_input_bytes, 0, len);
	while (len > 0) {
		current = (int)rstrm->in_boundry - (int)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm, do_align)) {
				trace1(TR_get_input_bytes, 1);
				return (FALSE);
			}
			continue;
		}
		current = (len < current) ? len : current;
		(void) memcpy(addr, rstrm->in_finger, current);
		rstrm->in_finger += current;
		addr += current;
		len -= current;
		do_align = FALSE;
	}
	trace1(TR_get_input_bytes, 1);
	return (TRUE);
}

/* next two bytes of the input stream are treated as a header */
static bool_t
set_input_fragment(RECSTREAM *rstrm)
{
	uint32_t header;

	trace1(TR_set_input_fragment, 0);
	if (! get_input_bytes(rstrm, (caddr_t)&header, (int) sizeof (header),
							rstrm->last_frag)) {
		trace1(TR_set_input_fragment, 1);
		return (FALSE);
	}
	header = (int32_t)ntohl(header);
	rstrm->last_frag = ((header & LAST_FRAG) == 0) ? FALSE : TRUE;
	rstrm->fbtbc = header & (~LAST_FRAG);
	trace1(TR_set_input_fragment, 1);
	return (TRUE);
}

/* consumes input bytes; knows nothing about records! */
static bool_t
skip_input_bytes(RECSTREAM *rstrm, int32_t cnt)
{
	register int current;

	trace2(TR_skip_input_bytes, 0, cnt);
	while (cnt > 0) {
		current = (int)rstrm->in_boundry - (int)rstrm->in_finger;
		if (current == 0) {
			if (! fill_input_buf(rstrm, FALSE)) {
				trace1(TR_skip_input_bytes, 1);
				return (FALSE);
			}
			continue;
		}
		current = (cnt < current) ? cnt : current;
		rstrm->in_finger += current;
		cnt -= current;
	}
	trace1(TR_skip_input_bytes, 1);
	return (TRUE);
}

__is_xdrrec_first(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	return ((rstrm->firsttime == TRUE)? 1 : 0);
}

__xdrrec_setfirst(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	/*
	 * Set rstrm->firsttime only if the input buffer is empty.
	 * Otherwise, the first read from the network could skip
	 * a poll.
	 */
	if (rstrm->in_finger == rstrm->in_boundry)
		rstrm->firsttime = TRUE;
	else
		rstrm->firsttime = FALSE;
	return (1);
}

__xdrrec_resetfirst(XDR *xdrs)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);

	rstrm->firsttime = FALSE;
	return (1);
}


static u_int
fix_buf_size(register u_int s)
{
	u_int dummy1;

	trace2(TR_fix_buf_size, 0, s);
	if (s < 100)
		s = 4000;
	dummy1 = RNDUP(s);
	trace1(TR_fix_buf_size, 1);
	return (dummy1);
}



static bool_t
xdrrec_control(XDR *xdrs, int request, void *info)
{
	register RECSTREAM *rstrm = (RECSTREAM *)(xdrs->x_private);
	xdr_bytesrec *xptr;

	switch (request) {

	case XDR_GET_BYTES_AVAIL:
		/* Check if at end of fragment and not last fragment */
		if ((rstrm->fbtbc == 0)	&& (!rstrm->last_frag))
			if (!set_input_fragment(rstrm)) {
				return (FALSE);
			};

		xptr = (xdr_bytesrec *) info;
		xptr->xc_is_last_record = rstrm->last_frag;
		xptr->xc_num_avail = rstrm->fbtbc;

		return (TRUE);
	default:
		return (FALSE);

	}

}

static struct xdr_ops *
xdrrec_ops()
{
	static struct xdr_ops ops;
	extern mutex_t	ops_lock;

/* VARIABLES PROTECTED BY ops_lock: ops */

	trace1(TR_xdrrec_ops, 0);
	mutex_lock(&ops_lock);
	if (ops.x_getlong == NULL) {
		ops.x_getlong = xdrrec_getlong;
		ops.x_putlong = xdrrec_putlong;
		ops.x_getbytes = xdrrec_getbytes;
		ops.x_putbytes = xdrrec_putbytes;
		ops.x_getpostn = xdrrec_getpos;
		ops.x_setpostn = xdrrec_setpos;
		ops.x_inline = xdrrec_inline;
		ops.x_destroy = xdrrec_destroy;
		ops.x_control = xdrrec_control;
#if defined(_LP64)
		ops.x_getint32 = xdrrec_getint32;
		ops.x_putint32 = xdrrec_putint32;
#endif
	}
	mutex_unlock(&ops_lock);
	trace1(TR_xdrrec_ops, 1);
	return (&ops);
}
