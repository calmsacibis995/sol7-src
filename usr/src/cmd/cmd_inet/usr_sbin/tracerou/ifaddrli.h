/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */


/*
 * Copyright (c) 1997
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * @(#) $Header: traceroute.h,v 1.1 97/01/04 19:33:33 leres Locked $ (LBL)
 */


#ifndef _IFADDRLIST_H
#define	_IFADDRLIST_H

#pragma ident	"@(#)ifaddrlist.h	1.1	98/01/12 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <net/if.h>

#define	ERRBUFSIZE 128

struct ifaddrlist {
	in_addr_t addr;
	char device[IFNAMSIZ+1];
};

int	ifaddrlist(struct ifaddrlist **, char *);

#ifdef __cplusplus
}
#endif

#endif /* _IFADDRLIST_H */
