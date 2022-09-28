/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _IFINST_H
#define	_IFINST_H

#pragma ident	"@(#)ifinst.h	1.3	96/11/25 SMI"

#include <sys/types.h>
#include <catype.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <lli.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ADDR_BOGUS	0x7f000001 /* loopback address in host byte order */
#define	FSWRITE_RETRY_TIME	20 /* time allowed for / to be mounted rw */

typedef struct dhcptimer {
	time_t wakeup;
	time_t granted;
	time_t T0;
	time_t T1;
	time_t T2;
} dhcptimer;

/* Forward declarations: */
struct Host;

typedef struct IFINSTANCE IFINSTANCE;
struct IFINSTANCE {
	char ifName[IFNAMSIZ];		/* interface name */
	HFLAGS ifRequestVector;		/* options that client will request */
	struct in_addr ifServer;	/* TBD: send all DHCP traffic here */
	struct Host *ifsendh;		/* contains "hints" sent to server */
	uint32_t ifXid;			/* XID of current transaction */
	struct sockaddr_in ifsaib;
	int32_t ifSent;			/* # packets sent to server */
	int32_t ifRetries;		/* # retries of current transaction */
	int32_t ifReceived;		/* # replies to current transaction */
	int32_t ifOffers;		/* # offers received by DISCOVER */
	int32_t ifBadOffers;		/* total # bad offers */
	int32_t ifState;		/* DHCP state of this interface */
	int32_t ifBusy;			/* is transaction in progess? */
	int32_t ifTimerID;		/* next timer to fire for this if */
	int32_t ifOrd;			/* unused */
	int32_t ifError;		/* last error */
	int32_t ifRWID;			/* ID for read-write timer */
	struct haddr ifMacAddr;		/* mac addr associated with interface */
	struct Host *ifOldConfig;	/* last configuration considered good */
	struct Host *ifConfig;		/* latest configuration received */
#if EMPLOY_BOOTP
	struct Host *ifBootp;		/* temp. placeholder for BOOTP config */
#endif
	time_t ifBegin;			/* beginning of current transaction */
	dhcptimer ifTimer;		/* times converted */
	IFINSTANCE *ifNext;		/* next interface */
	int connfd;			/* file descriptor for IPC */
	int32_t ctrl_flags;		/* flags from controller */
	int32_t ifPrimary;		/* is this the primary interface ? */
	int32_t ctrl_id;		/* id of controller txn */
	int32_t ifSavedFlags;		/* interface flags before DHCP */
	int32_t ifRestore;		/* restore state of interface ? */
	int fd;				/* fd for DHCP traffic */
	LLinfo lli;			/* link level interface data */
#ifdef DHCP_OVER_PPP
	int ifInformOnly;		/* Interface used to inform only? */
#endif /* PPP */
};

/*
 * List of all DHCP clients and a pointer to the currently active one.
 */
extern IFINSTANCE *ifList;

#ifdef	__cplusplus
}
#endif

#endif /* _IFINST_H */
