/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _HOSTDEFS_H
#define	_HOSTDEFS_H

#pragma ident	"@(#)hostdefs.h	1.3	96/11/27 SMI"

#include <haddr.h>
#include <hflags.h>
#include <catype.h> /* for uint16_t.. defns. */
#include <netinet/in.h> /* for struct in_addr defn. */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * RFC1533 and DHCP tag values used to specify what information
 * is being supplied in the vendor field of the packet:
 */
#define	TAG_PAD			0
#define	TAG_SUBNET_MASK		1
#define	TAG_TIME_OFFSET		2
#define	TAG_GATEWAYS		3
#define	TAG_TIME_SERVERS	4
#define	TAG_NAME_SERVERS	5
#define	TAG_DOMAIN_SERVERS	6
#define	TAG_LOG_SERVERS		7
#define	TAG_COOKIE_SERVERS	8
#define	TAG_LPR_SERVERS		9
#define	TAG_IMPRESS_SERVERS	10
#define	TAG_RLP_SERVERS		11
#define	TAG_HOSTNAME		12
#define	TAG_BOOTSIZE		13
#define	TAG_DUMP_FILE		14
#define	TAG_DNS_DOMAIN		15
#define	TAG_SWAP_SERVER		16
#define	TAG_ROOT_PATH		17
#define	TAG_EXTENSIONS_PATH	18
#define	TAG_BEROUTER		19
#define	TAG_FORWARD_NL_DATAGRAM	20
#define	TAG_NL_POLICY_FILTERS	21
#define	TAG_MAX_REASSEMBLY_SIZE	22
#define	TAG_IP_TTL		23
#define	TAG_PMTU_TIMEOUT	24
#define	TAG_PMTU_PLATEAUS	25
#define	TAG_MTU			26
#define	TAG_SUBNETS_LOCAL	27
#define	TAG_BROADCAST_FLAVOR	28
#define	TAG_MASK_DISCOVER	29
#define	TAG_MASK_SUPPLY		30
#define	TAG_ROUTE_DISCOVER	31
#define	TAG_ROUTE_SOLICIT	32
#define	TAG_STATIC_ROUTES	33
#define	TAG_TRAILERS		34
#define	TAG_ARP_TIMEOUT		35
#define	TAG_ENCAPSULATE_FLAVOR	36
#define	TAG_TCP_TTL		37
#define	TAG_TCP_KA_INTERVAL	38
#define	TAG_TCP_KA_OCTET	39
#define	TAG_NIS_DOMAIN		40
#define	TAG_NIS_SERVERS		41
#define	TAG_NTP_SERVERS		42
#define	TAG_VENDOR_SPECIFIC	43
#define	TAG_NETBIOS_NBNS	44
#define	TAG_NETBIOS_NBDD	45
#define	TAG_NETBIOS_NODE_TYPE	46
#define	TAG_NETBIOS_SCOPE	47
#define	TAG_X_FONT_SERVERS	48
#define	TAG_X_DISPLAY_MANAGERS	49
#define	TAG_IP_REQUEST		50
#define	TAG_LEASETIME		51
#define	TAG_OVERLOAD		52
#define	TAG_DHCP_MESSAGE_TYPE	53
#define	TAG_DHCP_SERVER		54
#define	TAG_REQUEST_LIST	55
#define	TAG_DHCP_ERROR_TEXT	56
#define	TAG_DHCP_SIZE		57
#define	TAG_DHCP_T1		58
#define	TAG_DHCP_T2		59
#define	TAG_CLASS_ID		60
#define	TAG_CLIENT_ID		61
#define	TAG_NETWARE_DOMAIN	62
#define	TAG_NETWARE_OPTIONS	63
#define	TAG_NISPLUS_DOMAIN	64
#define	TAG_NISPLUS_SERVERS	65
#define	TAG_TFTP_SERVER_NAME	66
#define	TAG_BOOTFILE		67
#define	TAG_HOME_AGENTS		68
#define	TAG_SMTP_SERVERS	69
#define	TAG_POP3_SERVERS	70
#define	TAG_NNTP_SERVERS	71
#define	TAG_WWW_SERVERS		72
#define	TAG_FINGER_SERVERS	73
#define	TAG_IRC_SERVERS		74
#define	TAG_STREETTALK_SERVERS	75
#define	TAG_STDA_SERVERS	76

#define	LAST_TAG		254

/* and the end of options: */
#define	TAG_END 		255

/* Internally vendor specific tags are numberered from 256 to 511: */
#define	VENDOR_TAG(tag) (tag+256)

/*
 * Pseudo tags. Used to retrieve data in the host structure
 * which is either not present in BOOTP packets, or occupies
 * fixed fields in the header (these don't have tags). An
 * important example of the latter is yiaddr which client
 * applications always want to retrieve!!
 */

#define	TAG_YIADDR		513
#define	TAG_SIADDR		514
#define	TAG_HWADDR		515
#define	TAG_HWTYPE		516
#define	TAG_NWADDR		517
#define	TAG_LEASE_EXPIRES	518
#define	TAG_TC			519
#define	TAG_HOMEDIR		520
#define	TAG_TFTP_ROOT		521
#define	TAG_WANTS_NAME		522
#define	TAG_VENDOR_COOKIE	523
#define	TAG_CIADDR		524
#define	TAG_GIADDR		525
#define	TAG_REPLY_OVERRIDE	526
#define	LAST_PSEUDO_TAG		526 /* change if it changes */

#define	PSEUDO_TAG(tag)		(tag + 512)
#define	IS_PSEUDO(tag)		(tag >= 512)
#define	ISNT_PSEUDO(tag)	(tag < 512)
#define	NORMALIZE_TAG(tag)	(tag % 256)

/* various Bootp "magic" cookies: */
#define	NO_COOKIE	0
#define	RFC_COOKIE	1
#define	CMU_COOKIE	2
#define	AUTO_COOKIE	3

/*
 * Data structure used to hold an arbitrary-length list of IP addresses.
 * The list may be shared among multiple hosts by setting the linkcount
 * appropriately.
 */

struct in_addr_list {
	int linkcount;
	int addrcount;
	struct in_addr addr[1]; /* Dynamically extended */
};

struct shared_bindata {
	int linkcount;
	int length;
	unsigned char data[1]; /* Dynamically extended */
};

struct int16_list {
	int   count;
	int16_t  shary[1]; /* Dynamically extended */
};

struct string_list {
	int count;
	int linkcount;
	char *string[1]; /* Dynamically extended */
};

typedef union dhcp_types {
	u_char dhcu1;
	int16_t dhcd2;
	char *dhccp;
	int32_t dhcd4;
	struct in_addr dhcia;
	struct int16_list *dhcd2l;
	struct string_list *dhcsl;
	struct in_addr_list *dhcial;
	struct shared_bindata *dhcsb;
} DHCP_TYPES;

typedef struct Host {
	HFLAGS flags;		/* bit vector of all valid fields */
	HFLAGS request_vector;	/* bit vector of options requested */
	HADDR  hw;
	DHCP_TYPES stags[256];	/* standard and site specific */
	DHCP_TYPES vtags[256];	/* vendor specific */
	DHCP_TYPES ptags[16];	/* pseudo */
	char *id;
	int32_t lease_expires;
} Host;

#define	Ciaddr	stags[TAG_CIADDR].dhcia
#define	Giaddr	stags[TAG_GIADDR].dhcia
#define	Yiaddr	stags[TAG_YIADDR].dhcia
#define	Siaddr	stags[TAG_SIADDR].dhcia
#define	Hwaddr	stags[TAG_HWADDR].dhccp

#ifdef	__cplusplus
}
#endif

#endif /* _HOSTDEFS_H */
