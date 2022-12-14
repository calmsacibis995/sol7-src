/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)res_mkquery.c	1.7	97/05/19 SMI"	/* SVr4.0 1.2 */

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 *	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *			All rights reserved.
 *
 */

#include "synonyms.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#include <netdb.h>

/*
 * Kludge to time out quickly if there is no /etc/resolv.conf
 * and a TCP connection to the local DNS server fails.
 *
 * Moved function from res_send.c to res_mkquery.c.  This
 * solves a long timeout problem with nslookup.
 */

static int _confcheck()
{
	int ns;
	struct stat rc_stat;
	struct sockaddr_in ns_sin;


	/* First, we check to see if /etc/resolv.conf exists.
	 * If it doesn't, then localhost is mostlikely to be
	 * the nameserver.
	 */
	if (stat(_PATH_RESCONF, &rc_stat) == -1 && errno == ENOENT) {

		/* Next, we check to see if _res.nsaddr is set to loopback.
		 * If it isn't, it has been altered by the application
		 * explicitly and we then want to bail with success.
		 */
		if (_res.nsaddr.sin_addr.S_un.S_addr == htonl(INADDR_LOOPBACK)) {

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

/*
 * Form all types of queries.
 * Returns the size of the result or -1.
 */
res_mkquery(op, dname, class, type, data, datalen, newrr, buf, buflen)
	int op;			/* opcode of query */
	char *dname;		/* domain name */
	int class, type;	/* class and type of query */
	char *data;		/* resource record data */
	int datalen;		/* length of data */
	struct rrec *newrr;	/* new rr for modify or append */
	char *buf;		/* buffer to put query */
	int buflen;		/* size of buffer */
{
	register HEADER *hp;
	register char *cp;
	register int n;
	char *dnptrs[10], **dpp, **lastdnptr;

#ifdef DEBUG
	if (_res.options & RES_DEBUG)
		printf("res_mkquery(%d, %s, %d, %d)\n", op, dname, class, type);
#endif DEBUG
	
	/*
	 * Check to see if we can bailout quickly.
	 * Also rerun res_init if we failed in the past.
	 */

	if ((_res.options & RES_INIT) == 0 && res_init() == -1) {
		h_errno = NO_RECOVERY;
		return(-1);
	}
	
	if (_confcheck() == -1) {
		_res.options &= ~RES_INIT;
		h_errno = NO_RECOVERY;
		return(-1);
	}

	/*
	 * Initialize header fields.
	 */
	if ((buf == NULL) || (buflen < sizeof (HEADER)))
		return (-1);
#ifdef SYSV
	memset(buf, 0, sizeof (HEADER));
#else
	bzero(buf, sizeof (HEADER));
#endif
	hp = (HEADER *) buf;
	hp->id = htons(++_res.id);
	hp->opcode = op;
	hp->pr = (_res.options & RES_PRIMARY) != 0;
	hp->rd = (_res.options & RES_RECURSE) != 0;
	hp->rcode = NOERROR;
	cp = buf + sizeof (HEADER);
	buflen -= sizeof (HEADER);
	dpp = dnptrs;
	*dpp++ = buf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof (dnptrs) / sizeof (dnptrs[0]);
	/*
	 * perform opcode specific processing
	 */
	switch (op) {
	case QUERY:
		if ((buflen -= QFIXEDSZ) < 0)
			return (-1);
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		putshort(type, cp);
		cp += sizeof (u_short);
		putshort(class, cp);
		cp += sizeof (u_short);
		hp->qdcount = htons(1);
		if (op == QUERY || data == NULL)
			break;
		/*
		 * Make an additional record for completion domain.
		 */
		buflen -= RRFIXEDSZ;
		if ((n = dn_comp(data, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		buflen -= n;
		putshort(T_NULL, cp);
		cp += sizeof (u_short);
		putshort(class, cp);
		cp += sizeof (u_short);
		putlong(0, cp);
		cp += sizeof (u_long);
		putshort(0, cp);
		cp += sizeof (u_short);
		hp->arcount = htons(1);
		break;

	case IQUERY:
		/*
		 * Initialize answer section
		 */
		if (buflen < 1 + RRFIXEDSZ + datalen)
			return (-1);
		*cp++ = '\0';	/* no domain name */
		putshort(type, cp);
		cp += sizeof (u_short);
		putshort(class, cp);
		cp += sizeof (u_short);
		putlong(0, cp);
		cp += sizeof (u_long);
		putshort(datalen, cp);
		cp += sizeof (u_short);
		if (datalen) {
#ifdef SYSV
			memcpy((void *)cp, (void *)data, datalen);
#else
			bcopy(data, cp, datalen);
#endif
			cp += datalen;
		}
		hp->ancount = htons(1);
		break;

#ifdef ALLOW_UPDATES
	/*
	 * For UPDATEM/UPDATEMA, do UPDATED/UPDATEDA followed by UPDATEA
	 * (Record to be modified is followed by its replacement in msg.)
	 */
	case UPDATEM:
	case UPDATEMA:

	case UPDATED:
		/*
		 * The res code for UPDATED and UPDATEDA is the same; user
		 * calls them differently: specifies data for UPDATED; server
		 * ignores data if specified for UPDATEDA.
		 */
	case UPDATEDA:
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		putshort(type, cp);
		cp += sizeof (u_short);
		putshort(class, cp);
		cp += sizeof (u_short);
		putlong(0, cp);
		cp += sizeof (u_long);
		putshort(datalen, cp);
		cp += sizeof (u_short);
		if (datalen) {
#ifdef SYSV
			memcpy((void *)cp, (void *)data, datalen);
#else
			bcopy(data, cp, datalen);
#endif
			cp += datalen;
		}
		if ((op == UPDATED) || (op == UPDATEDA)) {
			hp->ancount = htons(0);
			break;
		}
		/* Else UPDATEM/UPDATEMA, so drop into code for UPDATEA */

	case UPDATEA:	/* Add new resource record */
		buflen -= RRFIXEDSZ + datalen;
		if ((n = dn_comp(dname, cp, buflen, dnptrs, lastdnptr)) < 0)
			return (-1);
		cp += n;
		putshort(newrr->r_type, cp);
		cp += sizeof (u_short);
		putshort(newrr->r_class, cp);
		cp += sizeof (u_short);
		putlong(0, cp);
		cp += sizeof (u_long);
		putshort(newrr->r_size, cp);
		cp += sizeof (u_short);
		if (newrr->r_size) {
#ifdef SYSV
			memcpy((void *)cp, newrr->r_data, newrr->r_size);
#else
			bcopy(newrr->r_data, cp, newrr->r_size);
#endif
			cp += newrr->r_size;
		}
		hp->ancount = htons(0);
		break;

#endif ALLOW_UPDATES
	}
	return (cp - buf);
}
