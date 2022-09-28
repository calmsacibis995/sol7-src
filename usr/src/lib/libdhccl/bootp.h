/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef	_BOOTP_H
#define	_BOOTP_H

#pragma ident	"@(#)bootp.h	1.3	96/11/21 SMI"

#include <catype.h>    /* for uint16_t.. defns. */
#include <netinet/in.h>   /* for struct in_addr defn. */

#ifdef	__cplusplus
extern "C" {
#endif

#define	BP_CHADDR_LEN		16	/* Size of 'bp_chaddr' field */
#define	BP_SNAME_LEN		64	/* Size of 'bp_sname' field */
#define	BP_FILE_LEN		128	/* Size of 'bp_file' field */
#define	BP_MIN_VEND_LEN		64	/* min 'bp_vend' field size */
#define	BP_MAX_VEND_LEN		1236	/* max 'bp_vend' field size */
#define	DHCP_MIN_VEND_LEN	312

#define	VBPLEN			236	/* length of all 'fixed' fields */

struct bootp {
	unsigned char op;			/* packet opcode type */
	unsigned char htype;			/* hardware addr type */
	unsigned char hlen;			/* hardware addr length */
	unsigned char hops;			/* gateway hops */
	uint32_t xid;				/* transaction ID */
	uint16_t secs;				/* seconds since boot began */
	uint16_t flags;				/* broadcast bit (and others) */
	struct in_addr ciaddr;			/* client IP address */
	struct in_addr yiaddr;			/* 'your' IP address */
	struct in_addr siaddr;			/* server IP address */
	struct in_addr giaddr;			/* gateway IP address */
	unsigned char chaddr[BP_CHADDR_LEN];	/* client hardware address */
	unsigned char sname[BP_SNAME_LEN];	/* server host name */
	unsigned char file[BP_FILE_LEN];	/* boot file name */
	unsigned char vend[DHCP_MIN_VEND_LEN];	/* vendor-specific area */
};

union bigbootp {
	struct bootp bp;
	char buf[1472]; /* ethernet MTU less IP and UDP headers */
};

#define	IPPORT_BOOTPS	67 /* default UDP port numbers, server .. */
#define	IPPORT_BOOTPC	68 /* ..and client. */

#define	BOOTREPLY	2
#define	BOOTREQUEST	1

#define	VM_RFC1048	{ 99, 130, 83, 99 } /* magic cookie for RFC1048 */
#define	VM_CMU	"CMU"		/* Vendor magic cookie for CMU */

#ifdef	__cplusplus
}
#endif

#endif /* _BOOTP_H */
