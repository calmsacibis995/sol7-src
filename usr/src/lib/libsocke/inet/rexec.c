/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)rexec.c	1.12	98/01/19 SMI"	/* SVr4.0 1.2	*/

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
 * 	(c) 1986,1987,1988.1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */


#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#ifdef SYSV
#define	bcopy(a, b, c)	(void) memcpy((b), (a), (c))
#endif

extern char *_dgettext();

void _ruserpass(const char *host, char **aname, char **apass);

int rexec(char **ahost, unsigned short rport, const char *name,
    const char *pass, const char *cmd, int *fd2p)
{
	int s, timo = 1, s3;
	struct sockaddr_in sin, sin2, from;
	char c;
	u_short port;
	struct hostent *hp;

	hp = gethostbyname(*ahost);
	if (hp == 0) { (void) fprintf(stderr, 
				_dgettext(TEXT_DOMAIN, "%s: unknown host\n"), 
				*ahost);

		return (-1);
	}
	*ahost = hp->h_name;
	_ruserpass(hp->h_name, (char **)&name, (char **)&pass);
retry:
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		perror("rexec: socket");
		return (-1);
	}
	sin.sin_family = hp->h_addrtype;
	sin.sin_port = rport;
	bcopy(hp->h_addr, (caddr_t)&sin.sin_addr, hp->h_length);
	if (connect(s, (struct sockaddr *)&sin,
		(socklen_t)sizeof (sin)) < 0) {
		if (errno == ECONNREFUSED && timo <= 16) {
			(void) close(s);
			(void) sleep(timo);
			timo *= 2;
			goto retry;
		}
		perror(hp->h_name);
		(void) close(s);
		return (-1);
	}
	if (fd2p == 0) {
		(void) write(s, "", 1);
		port = 0;
	} else {
		char num[8];
		int s2;
		socklen_t sin2len;

		s2 = socket(AF_INET, SOCK_STREAM, 0);
		if (s2 < 0) {
			(void) close(s);
			return (-1);
		}
		(void) listen(s2, 1);
		sin2len = (socklen_t)sizeof (sin2);
		if (getsockname(s2, (struct sockaddr *)&sin2, &sin2len) < 0 ||
		    sin2len != (socklen_t)sizeof (sin2)) {
			perror("getsockname");
			(void) close(s2);
			goto bad;
		}
		port = ntohs((u_short)sin2.sin_port);
		(void) sprintf(num, "%u", port);
		(void) write(s, num, strlen(num)+1);
		{
			socklen_t len = (socklen_t)sizeof (from);
			s3 = accept(s2, (struct sockaddr *)&from, &len);
			(void) close(s2);
			if (s3 < 0) {
				perror("accept");
				port = 0;
				goto bad;
			}
		}
		*fd2p = s3;
	}
	(void) write(s, name, strlen(name) + 1);
	/* should public key encypt the password here */
	(void) write(s, pass, strlen(pass) + 1);
	(void) write(s, cmd, strlen(cmd) + 1);
	if (read(s, &c, 1) != 1) {
		perror(*ahost);
		goto bad;
	}
	if (c != 0) {
		while (read(s, &c, 1) == 1) {
			(void) write(2, &c, 1);
			if (c == '\n')
				break;
		}
		goto bad;
	}
	return (s);
bad:
	if (port)
		(void) close(*fd2p);
	(void) close(s);
	return (-1);
}
