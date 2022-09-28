/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _DHCPSTATE_H
#define	_DHCPSTATE_H

#pragma ident	"@(#)dhcpstate.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * DHCP client state machine states. (See RFC 1541, Figure 5 for state
 * diagram.)
 */
typedef enum _dhcpstate {
	DHCP_NULL = 0, DHCP_SELECTING, DHCP_REQUESTING, DHCP_BOUND,
	DHCP_REBOOTING, DHCP_RENEWING, DHCP_REBINDING, DHCP_FAIL, BOOTP
} DHCPSTATE;

#ifdef	__cplusplus
}
#endif

#endif /* _DHCPSTATE_H */
