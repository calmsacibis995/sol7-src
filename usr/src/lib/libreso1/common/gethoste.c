/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1996 by Internet Software Consortium.
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
#pragma ident   "@(#)gethostent.c 1.2     97/12/18 SMI"

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: gethostent.c,v 1.10 1997/01/30 20:29:24 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <string.h>

#include "port_after.h"

#include "irs_p.h"
#include "irs_data.h"

/* Definitions */

struct pvt {
	char *		aliases[1];
	char *		addrs[2];
	char		addr[IN6ADDRSZ];
	char		name[MAXDNAME + 1];
	struct hostent	host;
};

/* Forward */

static struct irs_ho *	init(void);
static void		freepvt(void);
static struct hostent *	fakeaddr(const char *, int);

/* Public */

struct hostent *
gethostbyname(const char *name) {
	struct hostent *hp;

	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (_res.options & RES_USE_INET6) {
		hp = gethostbyname2(name, AF_INET6);
		if (hp)
			return (hp);
	}
	return (gethostbyname2(name, AF_INET));
}

struct hostent *
gethostbyname2(const char *name, int af) {
	struct irs_ho *ho = init();
	struct hostent *hp;
	const char *cp;
	char **hap;

	if (!ho)
		return (NULL);
	if (net_data.ho_stayopen && net_data.ho_last) {
		if (!strcasecmp(name, net_data.ho_last->h_name))
			return (net_data.ho_last);
		for (hap = net_data.ho_last->h_aliases; hap && *hap; hap++)
			if (!strcasecmp(name, *hap))
				return (net_data.ho_last);
	}
	if (!strchr(name, '.') && (cp = hostalias(name)))
		name = cp;
	if ((hp = fakeaddr(name, af)) != NULL)
		return (hp);
	net_data.ho_last = (*ho->byname2)(ho, name, af);
	if (!net_data.ho_stayopen)
		endhostent();
	return (net_data.ho_last);
}

struct hostent *
gethostbyaddr(const char *addr, int len, int af) {
	struct irs_ho *ho = init();
	char **hap;

	if (!ho)
		return (NULL);
	if (net_data.ho_stayopen && net_data.ho_last &&
	    net_data.ho_last->h_length == len)
		for (hap = net_data.ho_last->h_addr_list;
		     hap && *hap;
		     hap++)
			if (!memcmp(addr, *hap, len))
				return (net_data.ho_last);
	net_data.ho_last = (*ho->byaddr)(ho, addr, len, af);
	if (!net_data.ho_stayopen)
		endhostent();
	return (net_data.ho_last);
}

struct hostent *
gethostent() {
	struct irs_ho *ho = init();

	if (!ho)
		return (NULL);
	net_data.ho_last = (*ho->next)(ho);
	return (net_data.ho_last);
}

int
sethostent(int stayopen) {
	struct irs_ho *ho = init();

	if (!ho)
		return(0);
	freepvt();
	(*ho->rewind)(ho);
	net_data.ho_stayopen = (stayopen != 0);
	return(0);
}

int
endhostent() {
	struct irs_ho *ho = init();

	if (ho != NULL)
		(*ho->minimize)(ho);
	return(0);
}

/* Private */

static struct irs_ho *
init() {
	if ((_res.options & RES_INIT) == 0 && res_init() == -1)
		return (NULL);
	if (!net_data_init())
		goto error;
	if (!net_data.ho)
		net_data.ho = (*net_data.irs->ho_map)(net_data.irs);
	if (!net_data.ho) {
 error:		errno = EIO;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	return (net_data.ho);
}

static void
freepvt() {
	if (net_data.ho_data) {
		free(net_data.ho_data);
		net_data.ho_data = NULL;
	}
}

static struct hostent *
fakeaddr(const char *name, int af) {
	struct pvt *pvt;
	const char *cp;

	if (!isascii(name[0]) || !isdigit(name[0]))
		return (NULL);

	if (name[0] == '0' && (name[1] == 'x' || name[1] == 'X')) {
		for (cp = name+2; *cp; ++cp)
			if (!isascii(*cp) || !isxdigit(*cp))
				return (NULL);
	} else {
		for (cp = name; *cp; ++cp)
			if (!isascii(*cp) || (!isdigit(*cp) && *cp != '.'))
				return (NULL);
	}
	if (*--cp == '.')
		return (NULL);

	/*
	 * All-numeric, no dot at the end.
	 * Fake up a hostent as if we'd actually
	 * done a lookup.
	 */
	freepvt();
	net_data.ho_data = malloc(sizeof(struct pvt));
	if (!net_data.ho_data) {
		errno = ENOMEM;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt = net_data.ho_data;
	if (inet_aton(name, (struct in_addr *)pvt->addr) <= 0) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	}
	strncpy(pvt->name, name, MAXDNAME);
	pvt->name[MAXDNAME] = '\0';
	pvt->host.h_addrtype = af;
	switch (af) {
	case AF_INET:
		pvt->host.h_length = INADDRSZ;
		break;
	case AF_INET6:
		pvt->host.h_length = IN6ADDRSZ;
		break;
	default:
		errno = EAFNOSUPPORT;
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}
	pvt->host.h_name = pvt->name;
	pvt->host.h_aliases = pvt->aliases;
	pvt->aliases[0] = NULL;
	pvt->addrs[0] = (char *)pvt->addr;
	pvt->addrs[1] = NULL;
	pvt->host.h_addr_list = pvt->addrs;
	if (af == AF_INET && (_res.options & RES_USE_INET6))
		map_v4v6_address(pvt->addr, pvt->addr);
	h_errno = NETDB_SUCCESS;
	return (&pvt->host);
}

#ifdef grot	/* for future use in gethostbyaddr(), for "SUNSECURITY" */
	struct hostent *rhp;
	char **haddr;
	u_int old_options;
	char hname2[MAXDNAME+1];

	if (af == AF_INET) {
	    /*
	     * turn off search as the name should be absolute,
	     * 'localhost' should be matched by defnames
	     */
	    strncpy(hname2, hp->h_name, MAXDNAME);
	    hname2[MAXDNAME] = '\0';
	    old_options = _res.options;
	    _res.options &= ~RES_DNSRCH;
	    _res.options |= RES_DEFNAMES;
	    if (!(rhp = gethostbyname(hname2))) {
		_res.options = old_options;
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	    _res.options = old_options;
	    for (haddr = rhp->h_addr_list; *haddr; haddr++)
		if (!memcmp(*haddr, addr, INADDRSZ))
			break;
	    if (!*haddr) {
		h_errno = HOST_NOT_FOUND;
		return (NULL);
	    }
	}
#endif /* grot */
