/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _H_FLAGS_H
#define	_H_FLAGS_H

#pragma ident	"@(#)hflags.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * HFLAGS is long enough for 256 DHCP standard tags + 256 vendor option tags
 * + 16 "pseudo tags". Pseudo tags are items which occupy fixed fields in the
 * BOOTP packet, or which otherwise require special processing (e.g. the
 * "tc" capability in dhcpcap.
 */

typedef unsigned char HFLAGS[66];

#define	inhost(u, i)	(u[(i) / 8] & (1 << ((i) % 8)) ? 1 : 0)
#define	ofhost(u, i)	(u[(i) / 8] &= ~(1 << ((i) % 8)))
#define	onhost(u, i)	(u[(i) / 8] |= (1 << ((i) % 8)))
#define	orhost(u, v, w) { int i; for (i = 0; i < sizeof (HFLAGS); \
i++) (w)[i] = (u)[i] | (v)[i]; }

#define	FIRST_SITE_OCTET	15
#define	LAST_SITE_OCTET		31

#ifdef	__cplusplus
}
#endif

#endif /* _H_FLAGS_H */
