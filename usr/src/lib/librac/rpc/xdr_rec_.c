/*
 * Copyright (c) 1991-1996 by Sun Microsystems Inc.
 */
#ident	"@(#)xdr_rec_subr.c	1.6	97/09/24 SMI"

#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)xdr_rec_subr.c 1.4 91/03/11 Copyr 1988 Sun Micro";
#endif

/*
 * xdr_rec_subr.c, Copyright (C) 1990, Sun Microsystems, Inc.
 */

#include	<rpc/rpc.h>
#include	"rac_private.h"
#include	<sys/param.h>
#include	<sys/syslog.h>
#include	<sys/stropts.h>
#include	<sys/time.h>
#include	<assert.h>
#include	<unistd.h>
#include  <errno.h>
#ifndef	NDEBUG
#include	<stdio.h>
#endif
#include	<malloc.h>
#include	<tiuser.h>

/*
 *	This file supports the reading of packets for multiple recipients on a
 *	single virtual circuit.  Demultiplexing is done at a higher level based
 *	on RPC XIDs.  All packets are assumed to be in RPC record marking
 *	format (see the ``RECORD MARKING STANDARD'' in RFC 1057).
 *
 *	We export three functions:
 *
 *		pkt_vc_poll():	assemble a packet by fetching each fragment
 *				header, then the data associated with the
 *				fragment.  Returns (void *) 0 when the packet
 *				is not yet complete, and an opaque handle for
 *				use by pkt_vc_read() when a complete packet
 *				has been collected.
 *
 *		pkt_vc_read():	read from a packet (whose representation is
 *				described below) constructed by pkt_vc_poll().
 *
 *		free_pkt():	free a packet constructed by pkt_vc_poll().
 */

#define	FRAGHEADER_SIZE		(sizeof (int))	/* size of XDR frag header */
#define	FH_LASTFRAG		(((u_int) 1) << 31)

/*
 *	A packet consists of one or more RPC record marking fragments.
 *	We represent this structure with a packet header and one or more
 *	fragment headers.
 *
 *	Buffer headers are allocated on each t_rcv() and contain information
 *	about that t_rcv() (such as the amount and location of the data).
 *	They in turn point to buffers, which are shared and reference-counted.
 *
 *				...			...
 *				    ^			^
 *				    |  fh_next		|  bh_next
 *				    |			|
 *				frag header	-->  buf header	-->	buffer
 *				    ^			^
 *				    |  fh_next		|  bh_next
 *				    |			|
 *	packet header	-->	frag header	-->  buf header	-->	buffer
 *		pkt_fragp		fh_bhp		bh_bufp
 *
 */
struct pkthdr {
	struct fraghdr	*pkt_fragp;	/* first fragment in this packet */
	struct pkthdr	*pkt_next;	/* next packet */
};

struct fraghdr {
	bool_t		fh_eof;		/* did EOF occur reading this frag? */
	bool_t		fh_error;
	/* did an error occur reading this frag? */
	int		fh_terrno;	/* copy of t_errno from read error */
	bool_t		fh_morefrags;	/* set from XDR record frag header */
	u_int		fh_fragsize;	/* set from XDR record frag header */
	u_int		fh_nbytes;	/* # bytes currently in this frag */
	struct fraghdr	*fh_next;	/* next frag in chain */
	struct bufhdr	*fh_bhp;	/* first buffer in this frag */
};

struct bufhdr {
	u_int		bh_nbytes;	/* # bytes currently in this buffer */
	char		*bh_begin;	/* first byte of buffer */
	char		*bh_end;	/* next read position */
	struct buf	*bh_bufp;	/* pointer to buffer itself */
	struct bufhdr	*bh_next;	/* next bufhdr in this chain */
};

struct buf {
	u_int		buf_refcnt;	/* # bufhdrs referencing this buffer */
	u_int		buf_size;	/* size of this buffer */
	u_int		buf_bytesused;	/* current number of bytes in use */
	u_int		buf_bytesfree;	/* current number of bytes available */
	char		*buf_buf;	/* pointer to the actual data area */
};

enum recv_state { BEGINNING_OF_FRAG, NORMAL };
struct readinfo {
	struct pollinfo	*ri_pip;	/* pollinfo pointer for mem_free() */
	struct pkthdr	*ri_php;	/* packet we're currently reading */
	struct fraghdr	*ri_fhp;	/* fragment within packet */
	struct bufhdr	*ri_bhp;	/* buffer header within fragment */
};
struct pollinfo {
	enum recv_state	pi_rs;		/* our receive state */
	struct pkthdr	*pi_curphdr;	/* the packet we're collecting */
	struct fraghdr	*pi_curfhdr;	/* ... its current fragment */
	struct bufhdr	*pi_curbhdr;	/* ... results of last read */
	struct buf	*pi_curbuf;	/* ... and the shared buffer area */
	struct readinfo	pi_ri;		/* information for pkt_vc_read() */
};

static struct pollinfo	*alloc_pollinfo();
static struct pkthdr	*alloc_pkthdr();
static void		free_pkthdr();
static struct fraghdr	*alloc_fraghdr();
static void		free_fraghdr();
static struct bufhdr	*alloc_bufhdr();
static void		free_bufhdr();
static struct buf	*alloc_buf();
static void		free_buf();

extern void		free_pollinfo();
extern void		free_pkt();
/* extern int		t_errno; */
/* extern char		*malloc(); */

#define	bzero(ptr, size) memset(ptr, 0, size)

void	*
pkt_vc_poll(fildes, pollinfop)
int fildes;
void	**pollinfop;
{
	register int	nread;
	register struct bufhdr	*bhdr;
	register struct pollinfo	*pi;
	struct pollfd readfd;
	int	flags;
	u_int	nreadable;
	u_int	fragheader;
	static u_int	bufsiz;
	static u_int	min_bytesfree;
	static bool_t	firstcall = TRUE;
	int selerr;
	extern int errno;


	if (firstcall == TRUE) {
		bufsiz = sysconf(_SC_PAGESIZE);	/* a convenient buffer size */
		min_bytesfree = bufsiz / 8;
		/* minimum usable buffer space */
		assert(min_bytesfree > 0);
		firstcall = FALSE;
	}

	if (*pollinfop == (void *) 0) {
		pi = alloc_pollinfo();
		if (pi == (struct pollinfo *) 0)
			return ((void *) 0);
		else
			*pollinfop = (void *) pi;
		pi->pi_rs = BEGINNING_OF_FRAG;
	} else
		pi = (struct pollinfo *) *pollinfop;


	readfd.fd = fildes;
	readfd.events = POLLRDNORM;
	readfd.revents = 0;
	while ((selerr = poll(&readfd, 1, INFTIM)) > 0) {
	if (!(readfd.revents & POLLRDNORM)) {
		errno = EBADF;
		selerr = -1;
		break;
	}
#ifdef	PRINTFS
printf("pkt_vc_poll:  poll returned > 0\n");
#endif
		switch ((int) pi->pi_rs) {
		/*
		 *	Either we've never read a fragment or we've finished
		 *	reading an entire one and are ready to start the
		 *	next one.  We stay in this state until we know we've
		 *	gotten an entire XDR record header.
		 */
		case (int) BEGINNING_OF_FRAG:
			/*
			 *	If there's no packet present (then why did
			 *	select() return positive status?), or if
			 *	the amount of data doesn't exceed the size
			 *	of the XDR record header size, try again later.
			 */
			if (ioctl(fildes, I_NREAD, (size_t) &nreadable) <= 0)
				return ((void *) 0);
			if (nreadable < FRAGHEADER_SIZE)
				return ((void *) 0);

			/*
			 *	Enough data have arrived to read a fragment
			 *	header.  If this is the first fragment, we
			 *	have to allocate a packet header.
			 */
			if (!pi->pi_curphdr) {
				pi->pi_curphdr = alloc_pkthdr();
				if (!pi->pi_curphdr)
					return ((void *) 0);
			}
			/*
			 *	Allocate a fragment header.  If this is not the
			 *	first fragment in this packet, add it on the
			 *	end of the fragment chain.
			 */
			if (!pi->pi_curfhdr) {
#ifdef	PRINTFS
printf("pkt_vc_poll (before alloc_fraghdr):  pi->pi_curphdr %#x\n",
	pi->pi_curphdr);
#endif
				pi->pi_curfhdr = alloc_fraghdr();
#ifdef	PRINTFS
printf("pkt_vc_poll (after alloc_fraghdr):  pi->pi_curphdr %#x\n",
	pi->pi_curphdr);
fflush(stdout);
#endif
				if (pi->pi_curfhdr)
					pi->pi_curphdr->pkt_fragp =
						pi->pi_curfhdr;
				else
					return ((void *) 0);
			} else {
				register struct fraghdr	*fhp;

				assert(pi->pi_curfhdr->fh_fragsize ==
					pi->pi_curfhdr->fh_nbytes);
				assert(pi->pi_curfhdr->fh_morefrags == TRUE);

				fhp = alloc_fraghdr();
				if (fhp) {
					pi->pi_curfhdr->fh_next = fhp;
					pi->pi_curfhdr = fhp;
				} else
					return ((void *) 0);
			}

			/*
			 *	We allocate a new buffer when there's less than
			 *	min_bytesfree bytes of data left in the current
			 *	buffer (or, of course, if there is no buffer at
			 *	all).
			 */
			if (!pi->pi_curbuf ||
				(pi->pi_curbuf->buf_bytesfree <
				min_bytesfree)) {
				struct buf	*buf;

				buf = alloc_buf(bufsiz);
				if (buf)
					pi->pi_curbuf = buf;
				else
					return ((void *) 0);
			}

			/*
			 *	A buffer header is allocated for each t_rcv()
			 *	we do.
			 */
			bhdr = alloc_bufhdr();
			if (!bhdr)
				return ((void *) 0);
			if (pi->pi_curfhdr->fh_bhp == (struct bufhdr *) 0)
				pi->pi_curfhdr->fh_bhp = bhdr;
			if (pi->pi_curbhdr) {
				pi->pi_curbhdr->bh_next = bhdr;
				bhdr->bh_begin = bhdr->bh_end =
					pi->pi_curbhdr->bh_end;
			} else {
	/* XXX why are these asserts commented out? */
/*
				assert(pi->pi_curbuf->buf_refcnt == 0);
				assert(pi->pi_curbuf->buf_bytesused == 0);
*/
				bhdr->bh_begin = bhdr->bh_end =
					pi->pi_curbuf->buf_buf +
					pi->pi_curbuf->buf_bytesused;
			}
			pi->pi_curbhdr = bhdr;
			pi->pi_curbuf->buf_refcnt++;
			bhdr->bh_bufp = pi->pi_curbuf;	/* XXX - unneeded? */

			/*
			 *	We read the fragment into a temporary because
			 *	we want to access it as a longword and data in
			 *	the buffer aren't guaranteed to be properly
			 *	aligned.  Later we'll copy it from the temp to
			 *	the buffer.
			 */
			nread = t_rcv(fildes, (char *) &fragheader,
					FRAGHEADER_SIZE, &flags);
#ifdef	PRINTFS
printf("pkt_vc_poll:  case BEGINNING_OF_FRAG:  t_rcv returned %d\n", nread);
#endif

			fragheader = (int) ntohl(fragheader);

			/*
			 *	Deal with short reads or errors.
			 */
			if (nread == 0) {
				struct pkthdr	*phdr = pi->pi_curphdr;

				pi->pi_curfhdr->fh_eof = TRUE;
				pi->pi_curphdr = (struct pkthdr *) 0;
				pi->pi_curfhdr = (struct fraghdr *) 0;
				pi->pi_curbhdr = (struct bufhdr *) 0;
				pi->pi_curbuf = (struct buf *) 0;
				pi->pi_ri.ri_pip = pi;
				pi->pi_ri.ri_php = phdr;
				pi->pi_ri.ri_fhp = (struct fraghdr *) 0;
				*pollinfop = (void *) 0;
				return ((void *) &pi->pi_ri);
			}
			if (nread == -1) {
				struct pkthdr	*phdr = pi->pi_curphdr;

				if (t_errno == TLOOK)
					switch (t_look(fildes)) {
					case T_DISCONNECT:
						t_rcvdis(fildes, NULL);
						t_snddis(fildes, NULL);
						break;
					case T_ORDREL:
				/* Received orderly release indication */
						t_rcvrel(fildes);
				/* Send orderly release indicator */
						(void) t_sndrel(fildes);
						break;
					default:
						break;
					}
				pi->pi_curfhdr->fh_error = TRUE;
				pi->pi_curfhdr->fh_terrno = t_errno;
				pi->pi_curphdr = (struct pkthdr *) 0;
				pi->pi_curfhdr = (struct fraghdr *) 0;
				pi->pi_curbhdr = (struct bufhdr *) 0;
				pi->pi_curbuf = (struct buf *) 0;
				pi->pi_ri.ri_pip = pi;
				pi->pi_ri.ri_php = phdr;
				pi->pi_ri.ri_fhp = (struct fraghdr *) 0;
				*pollinfop = (void *) 0;
				return ((void *) &pi->pi_ri);
			}
			assert(nread == FRAGHEADER_SIZE);

			pi->pi_curfhdr->fh_eof = 0;
			if (fragheader & FH_LASTFRAG)
				pi->pi_curfhdr->fh_morefrags = FALSE;
			else
				pi->pi_curfhdr->fh_morefrags = TRUE;

			/*
			 *	A fragment header's size doesn't include the
			 *	header itself, so we must manually adjust the
			 *	true size.
			 */
			pi->pi_curfhdr->fh_fragsize =
				(fragheader & ~FH_LASTFRAG) + FRAGHEADER_SIZE;

#ifdef	PRINTFS
printf("pkt_vc_poll:  morefrags %d, frag size %d\n",
	pi->pi_curfhdr->fh_morefrags, pi->pi_curfhdr->fh_fragsize);
#endif

			(void) memcpy(bhdr->bh_begin, (char *) &fragheader,
				FRAGHEADER_SIZE);
			pi->pi_curbuf->buf_bytesused += nread;
			pi->pi_curbuf->buf_bytesfree -= nread;
			bhdr->bh_nbytes += nread;
			bhdr->bh_end += nread;
			pi->pi_curfhdr->fh_nbytes += nread;

			pi->pi_rs = NORMAL;
			break;

		/*
		 *	We've received a complete RPC record header, and now
		 *	know how much more data to expect from this fragment.
		 */
		case (int) NORMAL:
			assert(pi->pi_curphdr);
			assert(pi->pi_curfhdr);
			assert(pi->pi_curfhdr->fh_bhp);
			assert(pi->pi_curbhdr);
			assert(pi->pi_curbuf);

			bhdr = alloc_bufhdr();
			if (!bhdr)
				return ((void *) 0);
			pi->pi_curbhdr->bh_next = bhdr;

			if (pi->pi_curbuf->buf_bytesfree < min_bytesfree) {
				struct buf	*buf;

				buf = alloc_buf(bufsiz);
				if (!buf)
					return ((void *) 0);
				pi->pi_curbuf = buf;
				bhdr->bh_begin = bhdr->bh_end = buf->buf_buf;
			} else
				bhdr->bh_begin = bhdr->bh_end =
					pi->pi_curbhdr->bh_end;
			pi->pi_curbhdr = bhdr;
			pi->pi_curbuf->buf_refcnt++;
			bhdr->bh_bufp = pi->pi_curbuf;

#ifdef	PRINTFS
printf("pkt_vc_poll:  case NORMAL:  t_rcv(%d, %#x, %d, &flags)",
	fildes, bhdr->bh_begin,
	MIN(pi->pi_curfhdr->fh_fragsize - pi->pi_curfhdr->fh_nbytes,
		pi->pi_curbuf->buf_bytesfree));
#endif
			nread = t_rcv(fildes, bhdr->bh_begin,
				MIN(pi->pi_curfhdr->fh_fragsize -
				pi->pi_curfhdr->fh_nbytes,
				pi->pi_curbuf->buf_bytesfree), &flags);
#ifdef	PRINTFS
printf(" returned %d (flags %#x)\n", nread, flags);
#endif
			/*
			 *	Deal with short reads or errors.
			 */
			if (nread == 0) {
				struct pkthdr	*phdr = pi->pi_curphdr;

				pi->pi_curfhdr->fh_eof = TRUE;
				free_bufhdr(bhdr);
				pi->pi_curbuf->buf_refcnt--;
				pi->pi_curphdr = (struct pkthdr *) 0;
				pi->pi_curfhdr = (struct fraghdr *) 0;
				pi->pi_curbhdr = (struct bufhdr *) 0;
				pi->pi_curbuf = (struct buf *) 0;
				pi->pi_ri.ri_pip = pi;
				pi->pi_ri.ri_php = phdr;
				pi->pi_ri.ri_fhp = (struct fraghdr *) 0;
				*pollinfop = (void *) 0;
				return ((void *) &pi->pi_ri);
			}
			if (nread == -1) {
				free_bufhdr(bhdr);
				pi->pi_curbuf->buf_refcnt--;
				return ((void *) 0);
			}

			pi->pi_curbuf->buf_bytesused += nread;
			pi->pi_curbuf->buf_bytesfree -= nread;
			bhdr->bh_nbytes += nread;
			bhdr->bh_end += nread;
			pi->pi_curfhdr->fh_nbytes += nread;

			/*
			 *	Got an entire fragment.  See whether we've got
			 *	the entire packet.
			 */
#ifdef	PRINTFS
printf("pkt_vc_poll:  fragsize %u, fh_nbytes %u\n",
	pi->pi_curfhdr->fh_fragsize, pi->pi_curfhdr->fh_nbytes);
#endif
			if (pi->pi_curfhdr->fh_fragsize ==
				pi->pi_curfhdr->fh_nbytes) {
				pi->pi_curbhdr = (struct bufhdr *) 0;
				pi->pi_rs = BEGINNING_OF_FRAG;
				if (pi->pi_curfhdr->fh_morefrags == FALSE) {
					struct pkthdr	*phdr = pi->pi_curphdr;

					pi->pi_curphdr = (struct pkthdr *) 0;
					pi->pi_curfhdr = (struct fraghdr *) 0;
					pi->pi_curbhdr = (struct bufhdr *) 0;
					pi->pi_curbuf = (struct buf *) 0;
					pi->pi_ri.ri_pip = pi;
					pi->pi_ri.ri_php = phdr;
					pi->pi_ri.ri_fhp = (struct fraghdr *) 0;
#ifdef	PRINTFS
print_pkt(phdr);
#endif
					*pollinfop = (void *) 0;
					return ((void *) &pi->pi_ri);
				}
			}

			break;
		}
	}
#ifdef	PRINTFS
	if (selerr == -1)
		printf("pkt_vc_poll:  poll returned -1 (errno %d)\n", errno);
	else
		printf("pkt_vc_poll:  poll returned %d\n", selerr);
#endif

	return ((void *) 0);
}

int
pkt_vc_read(ripp, buf, len)
void	**ripp;
register char	*buf;
register int	len;
{
	register int	bytes_read, xferbytes;
	register struct readinfo *rip = *((struct readinfo **) ripp);
	register struct pkthdr *php = rip->ri_php;
	register struct fraghdr	*fhp, *lastfhp;
	register struct bufhdr *bhp;
	/* extern int	t_errno; */

#ifdef	PRINTFS
printf("pkt_vc_read(pkthdr %#x, %#x, %d)\n", php, buf, len);
#endif
	if (fhp = rip->ri_fhp)
		bhp = rip->ri_bhp;
	else {
		fhp = php->pkt_fragp;
		if (fhp == (struct fraghdr *) 0) {
#ifdef	PRINTFS
printf("pkt_vc_read:  no fragments;  returning 0\n");
printf("pkt_vc_read:  ci_readinfo <- 0\n");
#endif
			free_pkt((void *) rip);
			*ripp = (void *) 0;
			return (0);
		}
		bhp = (struct bufhdr *) 0;
	}
	if (bhp == (struct bufhdr *) 0) {
		bhp = fhp->fh_bhp;
		if (bhp == (struct bufhdr *) 0) {
#ifdef	PRINTFS
printf("pkt_vc_read:  no buf headers;  returning 0\n");
printf("pkt_vc_read:  ci_readinfo <- 0\n");
#endif
			free_pkt((void *) rip);
			*ripp = (void *) 0;
			return (0);
		}
	}

	for (bytes_read = 0; fhp && len;
		fhp = fhp->fh_next,
		bhp = fhp ?  fhp->fh_bhp : (struct bufhdr *) 0) {
/*		lastfhp = fhp; */
		rip->ri_fhp = fhp;
		for (; bhp && len; bhp = bhp->bh_next) {
			rip->ri_bhp = bhp;
			if (bhp->bh_nbytes) {
				xferbytes = MIN(len, bhp->bh_nbytes);
#ifdef	PRINTFS
printf("pkt_vc_read:  transferring %d bytes from bhdr %#x\n", xferbytes, bhp);
#endif
				(void) memcpy(buf, bhp->bh_begin, xferbytes);
				bhp->bh_nbytes -= xferbytes;
				bhp->bh_begin += xferbytes;
				assert(bhp->bh_begin <= bhp->bh_end);
				bytes_read += xferbytes;
				buf += xferbytes;
				len -= xferbytes;
			}
#ifdef	PRINTFS
			else
	printf("pkt_vc_read:  bhp %#x:  bh_nbytes == 0\n", bhp);
#endif
		}
	}
#ifdef	PRINTFS
printf("pkt_vc_read:  bytes_read:  %d, len:  %d\n", bytes_read, len);
#endif

	lastfhp = rip->ri_fhp;
	assert((len == 0) || ((fhp == (struct fraghdr *) 0) &&
		(bhp == (struct bufhdr *) 0)) || lastfhp->fh_eof ||
		(lastfhp->fh_error == TRUE));
	if (len > 0 && (lastfhp->fh_error == TRUE)) {
#ifdef	PRINTFS
printf("pkt_vc_read:  lastfhp %#x, lastfhp->fh_error TRUE,
	lastfhp->fh_terrno %d\n", lastfhp, lastfhp->fh_terrno);
#endif
		t_errno = lastfhp->fh_terrno;
#ifdef	PRINTFS
printf("pkt_vc_read:  ci_readinfo <- 0\n");
#endif
		free_pkt((void *) rip);
		*ripp = (void *) 0;
		return (-1);
	}

	if (len > 0) {
#ifdef	PRINTFS
printf("pkt_vc_read:  ci_readinfo <- 0\n");
#endif
		free_pkt((void *) rip);
		*ripp = (void *) 0;
	}
	return (bytes_read);
}

u_int
ri_to_xid(ri)
void	*ri;
{
	struct pkthdr	*php;
	struct fraghdr	*fhp;
	struct bufhdr	*bhp;
	register uint32_t	xid;

	if (ri)
		if (php = ((struct readinfo *) ri)->ri_php)
			if (fhp = php->pkt_fragp)
				if (bhp = fhp->fh_bhp) {
					assert(((((int) bhp->bh_begin)
					& (sizeof (u_int) - 1))) == 0);
					xid = (uint32_t) ntohl (*((uint32_t *)
						(bhp->bh_begin +
						FRAGHEADER_SIZE)));
					return (xid);
				}

	return (0);
}

void
free_pkt(rip)
void	*rip;
{
	struct pkthdr	*phdr = ((struct readinfo *) rip)->ri_php;
	register struct fraghdr	*fhp, *nextfhp;
	register struct bufhdr	*bhp, *nextbhp;
	register struct buf	*bp;

	assert(phdr);
	for (fhp = phdr->pkt_fragp; fhp; fhp = nextfhp) {
		nextfhp = fhp->fh_next;		/* save before freeing fhp */
		for (bhp = fhp->fh_bhp; bhp; bhp = nextbhp) {
			nextbhp = bhp->bh_next;	/* save before freeing bhp */
			bp = bhp->bh_bufp;
			if (bp == (struct buf *) 0) {
#ifdef	PRINTFS
	printf("free_pkt:  bufhdr@%#x(NULL buf):  nbytes %d, begin %#x,
		end %#x, next %#x\n", bhp, bhp->bh_nbytes, bhp->bh_begin,
		bhp->bh_end, bhp->bh_next);
#endif
			} else
				if (--bp->buf_refcnt == 0) {
#ifdef	PRINTFS
printf("free_pkt:  ref count for buf@%#x -> 0; freeing\n", bp);
#endif
					free_buf(bp);
				}
#ifdef	PRINTFS
				else
printf("free_pkt:  ref count for buf@%#x == %d\n", bp, bp->buf_refcnt);
printf("free_pkt:  freeing bufhdr@%#x\n", bhp);
#endif
			free_bufhdr(bhp);
		}
#ifdef	PRINTFS
printf("free_pkt:  freeing fraghdr@%#x\n", fhp);
#endif
		free_fraghdr(fhp);
	}

	assert(((struct readinfo *) rip)->ri_pip != (struct pollinfo *) 0);
#ifdef	PRINTFS
printf("free_pkt:  freeing pollinfo@%#x\n", ((struct readinfo *) rip)->ri_pip);
#endif
	free_pollinfo(((struct readinfo *) rip)->ri_pip);

#ifdef	PRINTFS
printf("free_pkt:  freeing pkthdr@%#x\n", phdr);
#endif
	free_pkthdr(phdr);
}

static struct pollinfo	*
alloc_pollinfo()
{
	struct pollinfo	*pi;

	pi = (struct pollinfo *) mem_alloc(sizeof (struct pollinfo));
	if (pi)
		(void) memset((char *) pi, 0, sizeof (struct pollinfo));

	return (pi);
}

void
free_pollinfo(pi)
void	*pi;
{
	bzero((char *) pi, sizeof (struct pollinfo));
	(void) mem_free((char *) pi, sizeof (struct pollinfo));
}

static struct pkthdr	*
alloc_pkthdr()
{
	register struct pkthdr	*phdr;

	phdr = (struct pkthdr *) mem_alloc(sizeof (struct pkthdr));
	if (phdr) {
		phdr->pkt_fragp = (struct fraghdr *) 0;
		phdr->pkt_next = (struct pkthdr *) 0;
	}

	return (phdr);
}

static void
free_pkthdr(phdr)
struct pkthdr	*phdr;
{
	bzero((char *) phdr, sizeof (struct pkthdr));
	(void) mem_free((char *) phdr, sizeof (struct pkthdr));
}

#ifdef	PRINTFS
print_pkt(phdr)
struct pkthdr	*phdr;
{
	register struct fraghdr	*fhp;
	register struct bufhdr	*bhp;


	printf("phdr:  %#x", phdr);
	for (fhp = phdr->pkt_fragp; fhp; fhp = fhp->fh_next) {
		printf("\tfhdr:  %#x\tbhdr:", fhp);
		for (bhp = fhp->fh_bhp; bhp; bhp = bhp->bh_next) {
			printf("\t%#x (nbytes %d)", bhp, bhp->bh_nbytes);
		}
		printf("\tNULL\n");
	}
}
#endif

static struct fraghdr	*
alloc_fraghdr()
{
	register struct fraghdr	*fhp;

	fhp = (struct fraghdr *) mem_alloc(sizeof (struct fraghdr));
	if (fhp) {
		fhp->fh_eof = FALSE;
		fhp->fh_error = FALSE;
		fhp->fh_nbytes = 0;
		fhp->fh_next = (struct fraghdr *) 0;
		fhp->fh_bhp = (struct bufhdr *) 0;
	}

	return (fhp);
}

static void
free_fraghdr(fhp)
struct fraghdr	*fhp;
{
	bzero((char *) fhp, sizeof (struct fraghdr));
	(void) mem_free((char *) fhp, sizeof (struct fraghdr));
}

static struct bufhdr	*
alloc_bufhdr()
{
	register struct bufhdr	*bhp;

	bhp = (struct bufhdr *) mem_alloc(sizeof (struct bufhdr));
	if (bhp) {
		bhp->bh_nbytes = 0;
		bhp->bh_bufp = (struct buf *) 0;
		bhp->bh_next = (struct bufhdr *) 0;
	}

	return (bhp);
}

static void
free_bufhdr(bhp)
struct bufhdr	*bhp;
{
	bzero((char *) bhp, sizeof (struct bufhdr));
	(void) mem_free((char *) bhp, sizeof (struct bufhdr));
}

static struct buf	*
alloc_buf(size)
u_int	size;
{
	register struct buf	*bp;

	bp = (struct buf *) mem_alloc(sizeof (struct buf) + size);
	if (bp) {
		bp->buf_refcnt = 0;
		bp->buf_bytesfree = bp->buf_size = size;
		bp->buf_bytesused = 0;
		bp->buf_buf = ((char *) bp) + sizeof (struct buf);
	}

	return (bp);
}

static void
free_buf(bp)
struct buf	*bp;
{
	int	size = bp->buf_size;
	bzero((char *) bp, sizeof (struct buf) + size);
	(void) mem_free((char *) bp, sizeof (struct buf) + size);
}
