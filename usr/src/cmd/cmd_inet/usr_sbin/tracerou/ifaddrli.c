/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 * Copyright (c) 1997
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the Computer Systems
 *	Engineering Group at Lawrence Berkeley Laboratory.
 * 4. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
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
 *
 * @(#) $Header: ifaddrlist.c,v 1.2 97/04/22 13:31:05 leres Exp $ (LBL)
 */

#pragma ident   "@(#)ifaddrlist.c 1.1     98/01/12 SMI"

#include <sys/param.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>				/* concession to AIX */

#include <net/if.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libintl.h>

#include "ifaddrlist.h"


/* Not all systems have IFF_LOOPBACK */
#define	ISLOOPBACK(p) ((p)->ifr_flags & IFF_LOOPBACK)

/*
 * construct the interface list of this host
 */
int
ifaddrlist(struct ifaddrlist **ipaddrp, char *errbuf)
{
	int fd;
	int nipaddr;
	struct ifreq *ifrp, *ifend, *ifnext;
	struct sockaddr_in *sin;
	struct ifaddrlist *al;
	struct ifconf ifc;
	struct ifreq *ibuf, ifr;
	char device[IFNAMSIZ + 1];
	struct ifaddrlist *ifaddrlist;


	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		(void) snprintf(errbuf, ERRBUFSIZE, "socket: %s",
				strerror(errno));
		return (-1);
	}

	if (ioctl(fd, SIOCGIFNUM, (char *)&nipaddr) < 0) {
		(void) snprintf(errbuf, ERRBUFSIZE, "SIOCGIFNUM: %s",
				strerror(errno));
		return (-1);
	}

	ifaddrlist = calloc((size_t)nipaddr,
				(size_t) sizeof (struct ifaddrlist));
	if (ifaddrlist == NULL) {
		(void) snprintf(errbuf, ERRBUFSIZE, "calloc: %s",
				strerror(errno));
		(void) close(fd);
		return (-1);
	}

	ibuf = calloc((size_t)nipaddr, (size_t) sizeof (struct ifreq));
	if (ibuf == NULL) {
		(void) snprintf(errbuf, ERRBUFSIZE, "calloc: %s",
				strerror(errno));
		(void) close(fd);
		return (-1);
	}

	ifc.ifc_len = (int) (nipaddr * sizeof (struct ifreq));
	ifc.ifc_buf = (caddr_t)ibuf;

	if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0 ||
	    ifc.ifc_len < sizeof (struct ifreq)) {
		(void) snprintf(errbuf, ERRBUFSIZE, "SIOCGIFCONF: %s",
				strerror(errno));
		(void) close(fd);
		return (-1);
	}

	ifrp = ibuf;
	ifend = (struct ifreq *)((char *)ibuf + ifc.ifc_len);

	al = ifaddrlist;
	nipaddr = 0;

	for (; ifrp < ifend; ifrp = ifnext) {
		ifnext = ifrp + 1;

		/*
		 * Need a template to preserve address info that is
		 * used below to locate the next entry.  (Otherwise,
		 * SIOCGIFFLAGS stomps over it because the requests
		 * are returned in a union.)
		 */
		(void) strncpy(ifr.ifr_name, ifrp->ifr_name,
				sizeof (ifr.ifr_name));
		if (ioctl(fd, SIOCGIFFLAGS, (char *)&ifr) < 0) {
			if (errno == ENXIO)
				continue;
			(void) snprintf(errbuf, ERRBUFSIZE,
				"SIOCGIFFLAGS: %.*s: %s",
				(int)sizeof (ifr.ifr_name),
				ifr.ifr_name, strerror(errno));
			(void) close(fd);
			return (-1);
		}

		/* Must be up and not the loopback */
		if ((ifr.ifr_flags & IFF_UP) == 0 || ISLOOPBACK(&ifr))
			continue;

		(void) strncpy(device, ifr.ifr_name, IFNAMSIZ);
		device[sizeof (device) - 1] = '\0';
		if (ioctl(fd, SIOCGIFADDR, (char *)&ifr) < 0) {
			(void) snprintf(errbuf, ERRBUFSIZE,
				"SIOCGIFADDR: %s: %s", device, strerror(errno));
			(void) close(fd);
			return (-1);
		}

		sin = (struct sockaddr_in *)&ifr.ifr_addr;
		al->addr = sin->sin_addr.s_addr;
		(void) strncpy(al->device, device, IFNAMSIZ+1);

		++al;
		++nipaddr;
	}

	(void) close(fd);

	*ipaddrp = ifaddrlist;
	return (nipaddr);
}
