/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _HADDR_H
#define	_HADDR_H

#pragma ident	"@(#)haddr.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAXHADDRLEN	32

typedef struct haddr {
	unsigned short htype;
	unsigned short hlen;
	unsigned char chaddr[MAXHADDRLEN];
} HADDR;

/* Hardware types from Assigned Numbers RFC:  */

#define	HTYPE_ETHERNET		1
#define	HTYPE_EXP_ETHERNET	2
#define	HTYPE_AX25		3
#define	HTYPE_PRONET		4
#define	HTYPE_CHAOS		5
#define	HTYPE_IEEE802		6
#define	HTYPE_ARCNET		7
#define	HTYPE_MAX		7

#ifdef	__cplusplus
}
#endif

#endif /* _HADDR_H */
