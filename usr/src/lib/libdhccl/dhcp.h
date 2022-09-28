/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_DHCP_H
#define	_CA_DHCP_H

#pragma ident	"@(#)dhcp.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* the DHCP operations */
#define	DHCPDISCOVER	1
#define	DHCPOFFER	2
#define	DHCPREQUEST	3
#define	DHCPDECLINE	4
#define	DHCPACK	5
#define	DHCPNAK	6
#define	DHCPRELEASE	7
#define	DHCPINFORM	8
#define	DHCPREVALIDATE	9
#define	BOOTP_BROADCAST_BIT	((unsigned short)0x8000) /* host byte order */
#define	MIN_BOOTP_PACKET_SIZE	(300)

/* minimum lease duration (secs) as specified in RFC */
#define	MINIMUM_LEASE	(3600)

/* option overload indicators: */

#define	OVERLOAD_NONE	0
#define	OVERLOAD_FILE	1
#define	OVERLOAD_SNAME	2
#define	OVERLOAD_BOTH	3

#ifdef	__cplusplus
}
#endif

#endif /* _CA_DHCP_H */
