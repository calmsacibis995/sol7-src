/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_ERROR_H
#define	_CA_ERROR_H

#pragma ident	"@(#)error.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ERR_FIRST		-1000

/* Interface errors: */
#define	ERR_IF_GET_FLAGS	-1000
#define	ERR_IF_SET_FLAGS	-1001
#define	ERR_IF_SET_ADDR		-1002
#define	ERR_IF_NOT_UP		-1003
#define	ERR_IF_GET_ADDR		-1004
#define	ERR_IF_ADDR_CHANGED	-1005
#define	ERR_IF_SET_BRDADDR	-1006
#define	ERR_NOT_IP_FAMILY	-1007
#define	ERR_IF_SET_BOGUS	-1008

/* file descriptor errors: */
#define	ERR_CREATE_SOCKET	-1100
#define	ERR_SOCKET_BROADCAST	-1101
#define	ERR_BIND_SOCKET		-1102
#define	ERR_SET_BROADCAST	-1103
#define	ERR_RECV		-1104
#define	ERR_SEND		-1105
#define	ERR_DONT_ROUTE		-1106
#define	ERR_ADD_ROUTE		-1107
#define	ERR_DELETE_ROUTE	-1108
#define	ERR_NO_PRIV_PORTS	-1109
#define	ERR_SOCKET_DONTROUTE	-1110
#define	ERR_CREATE_PIPE		-1111
#define	ERR_PUSH_CONNLD		-1112
#define	ERR_ATTACH_PIPE		-1113
#define	ERR_OPEN_MOUNTED_PIPE	-1114
#define	ERR_STAT_MOUNTED_PIPE	-1115

/* errors in configuration file(s): */
#define	ERR_NO_CONFIG		-1200
#define	ERR_BAD_CONFIG		-1201
#define	ERR_WRITE_CONFIG	-1202

/* errors related to DHCP protocol: */
#define	ERR_TIMED_OUT		-1300
#define	ERR_RETRIES		-1301
#define	ERR_WRONG_XID		-1302

/* errors in configuring client host: */
#define	ERR_SET_HOSTNAME	-1400
#define	ERR_SET_NETMASK		-1401
#define	ERR_SET_GATEWAY		-1402

/* system errors: */
#define	ERR_SELECT		-1500
#define	ERR_OUT_OF_MEMORY	-1501
#define	ERR_SETPOLL		-1502

/* link level interface errors: */
#define	ERR_LLI_PPA		-1600
#define	ERR_LLI_OPEN		-1601
#define	ERR_LLI_ATTACH		-1602
#define	ERR_LLI_BIND		-1603
#define	ERR_LLI_NOT_ETHERNET	-1604
#define	ERR_LLI_PROTO		-1605
#define	ERR_MEDIA_UNSUPPORTED	-1606

/* packet filtering errors: */
#define	ERR_PF_PUSH		-1700
#define	ERR_PF_SET		-1701

/* permission errors (not root) */
#define	ERR_NOT_ROOT		-1800

/* miscellaneous */
#define	ERR_DUPLICATE_AGENT	-1900

#define	ERR_LAST		-1900 /* same as previous line */

#ifdef	__cplusplus
}
#endif

#endif /* _CA_ERROR_H */
