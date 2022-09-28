/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1985, 1993
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996 by Internet Software Consortium.
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
#pragma ident   "@(#)res_mkquery.c 1.7     97/12/03 SMI"

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_mkquery.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "$Id: res_mkquery.c,v 8.9 1997/04/24 22:22:36 vixie Exp $";
#endif /* LIBC_SCCS and not lint */

#include "port_before.h"
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/nameser.h>

#ifdef SUNW_CONFCHECK
#include <sys/socket.h>
#include <errno.h>
#include <sys/stat.h>
#endif

#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "port_after.h"

/* Options.  Leave them on. */
#define DEBUG

#ifdef SUNW_CONFCHECK
static int      _confcheck(void);
#endif



/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
int
res_mkquery(op, dname, class, type, data, datalen, newrr_in, buf, buflen)
	int op;			/* opcode of query */
	const char *dname;	/* domain name */
	int class, type;	/* class and type of query */
	const u_char *data;	/* resource record data */
	int datalen;		/* length of data */
	const u_char *newrr_in;	/* new rr for modify or append */
	u_char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
{
	register HEADER *hp;
	register u_char *cp;
	register int n;
	u_char *dnptrs[20], **dpp, **lastdnptr;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NETDB_INTERNAL;
		return (-1);
	}
#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf(";; res_mkquery(%d, %s, %d, %d)\n",
		       op, dname, class, type);
#endif

#ifdef SUNW_CONFCHECK
        /*
         * 1247019, 1265838, and 4034368: Check to see if we can
         * bailout quickly.
         */
        if (_confcheck() == -1) {
                _res.options &= ~RES_INIT;
                h_errno = NO_RECOVERY;
                return(-1);
        }
#endif
       
	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < HFIXEDSZ))
		return (-1);
	memset(buf, 0, HFIXEDSZ);
	hp = (HEADER *) buf;
	hp->id = htons(++_res.id);
	hp->opcode = op;
	hp->rd = (_res.options & RES_RECURSE) != 0;
	hp->rcode = NOERROR;
	cp = buf + HFIXEDSZ;
	buflen -= HFIXEDSZ;
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof dnptrs / sizeof dnptrs[0];
	/*
	 * perform opcode specific processing
	 */
	switch (op) {
	case QUERY:	/*FALLTHROUGH*/
	case NS_NOTIFY_OP:
		if ((buflen -= QFIXEDSZ) < 0)
			return (-1);
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(type, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		hp->qdcount = htons(1);
		if (op == QUERY || data == NULL)
			break;
		/*
		 * Make an additional record for completion domain.
		 */
		buflen -= RRFIXEDSZ;
		n = dn_comp((char *)data, cp, buflen, dnptrs, lastdnptr);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= n;
		__putshort(T_NULL, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		__putlong(0, cp);
		cp += INT32SZ;
		__putshort(0, cp);
		cp += INT16SZ;
		hp->arcount = htons(1);
		break;

	case IQUERY:
		/*
		 * Initialize answer section
		 */
		if (buflen < 1 + RRFIXEDSZ + datalen)
			return (-1);
		*cp++ = '\0';	/* no domain name */
		__putshort(type, cp);
		cp += INT16SZ;
		__putshort(class, cp);
		cp += INT16SZ;
		__putlong(0, cp);
		cp += INT32SZ;
		__putshort(datalen, cp);
		cp += INT16SZ;
		if (datalen) {
			memcpy(cp, data, datalen);
			cp += datalen;
		}
		hp->ancount = htons(1);
		break;

	default:
		return (-1);
	}
	return (cp - buf);
}

#ifdef SUNW_CONFCHECK
/*
 * Kludge to time out quickly if there is no /etc/resolv.conf
 * and a TCP connection to the local DNS server fails.
 *
 * Moved function from res_send.c to res_mkquery.c.  This
 * solves a long timeout problem with nslookup.
 */

static int _confcheck(void)
{
        int ns;
        struct stat rc_stat;
        struct sockaddr_in ns_sin;


        /* First, we check to see if /etc/resolv.conf exists.
         * If it doesn't, then it is likely that the localhost is
         * the nameserver.
         */
        if (stat(_PATH_RESCONF, &rc_stat) == -1 && errno == ENOENT) {

                /* Next, we check to see if _res.nsaddr is set to loopback.
                 * If it isn't, it has been altered by the application
                 * explicitly and we then want to bail with success.
                 */
                if (_res.nsaddr.sin_addr.S_un.S_addr == htonl(INADDR_LOOPBACK))
{

                        /* Lastly, we try to connect to the TCP port of the
                         * nameserver.  If this fails, then we know that
                         * DNS is misconfigured and we can quickly exit.
                         */
                        ns = socket(AF_INET, SOCK_STREAM, 0);
                        IN_SET_LOOPBACK_ADDR(&ns_sin);
                        ns_sin.sin_port = htons(NAMESERVER_PORT);
                        if (connect(ns, (struct sockaddr *) &ns_sin,
                                    sizeof ns_sin) == -1) {
                                close(ns);
                                return(-1);
                        }
                        else {
                                close(ns);

                                return(0);
                        }
                }

                return(0);
        }

        return (0);
}
#endif

