/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)buildpkt.h	2.16 96/07/25 Sun Microsystems"
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/H/BUILDPKT.H_V   2.0   31 Mar 1990 15:11:20  $	*/
/*
 * $Log:   E:/SNMPV2/H/BUILDPKT.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:20
 * Release 2.00
 * 
 *    Rev 1.4   25 Jan 1989 11:55:50
 * Corrected definitions of macros SNMP_Bind_Counter, SNMP_Bind_Gauge, and
 * SNMP_Bind_Timeticks -- each of these had the type mis-defined.
 * Also added a new macro -- SNMP_Bind_Opaque.
 * 
 *    Rev 1.3   11 Jan 1989 12:10:10
 * Buildpkt.c split into multiple files to prevent linker from picking up
 * unused routines.
 * 
 *    Rev 1.2   22 Sep 1988 18:38:14
 * Added definitions for SNMP_Bind_Null().
 * 
 *    Rev 1.1   19 Sep 1988 17:27:04
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:14
 * Initial revision.
*/

#if (!defined(buildpkt_inc))
#define buildpkt_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if (!defined(snmp_inc))
#include <snmp.h>
#endif

#define SNMP_Bind_Counter(P,I,C,L,V)	SNMP_Bind_Unsigned_Integer(P, I, C, \
							L, VT_COUNTER, V)

#define SNMP_Bind_Gauge(P,I,C,L,V)	SNMP_Bind_Unsigned_Integer(P, I, C, \
							L, VT_GAUGE, V)

#define SNMP_Bind_Timeticks(P,I,C,L,V)	SNMP_Bind_Unsigned_Integer(P, I, C, \
							L, VT_TIMETICKS, V)

#define SNMP_Bind_Opaque(P,I,C,L,W,S,D)	SNMP_Bind_String(P, I, C, L, \
							VT_OPAQUE, W, S, D)

#if !defined(NO_PP)
extern	SNMP_PKT_T *	SNMP_Create_Request(int, int, int, char *, INT_32_T,
					    int);
extern	SNMP_PKT_T *	SNMP_Create_Trap(int, int, char *, int,
				         OIDC_T *, OCTET_T *, int,
					 INT_32_T, UINT_32_T, int);
extern	int		SNMP_Bind_Integer(SNMP_PKT_T *, int, int,
					  OIDC_T *, INT_32_T);

extern	int		SNMP_Bind_Unsigned_Integer(SNMP_PKT_T *, int,
					           int, OIDC_T *,
						   OCTET_T, UINT_32_T);

extern	int		SNMP_Bind_IP_Address(SNMP_PKT_T *, int,
					     int, OIDC_T *,
					     OCTET_T *);

extern	int		SNMP_Bind_Object_ID(SNMP_PKT_T *, int,
					    int, OIDC_T *,
					    int, OIDC_T *);

extern	int		SNMP_Bind_String(SNMP_PKT_T *, int,
					 int, OIDC_T *,
					 OCTET_T, int, OCTET_T *, int);

extern	int		SNMP_Bind_Null(SNMP_PKT_T *, int, int,
				       OIDC_T *);
extern	VB_T *		locate_vb(SNMP_PKT_T *, int);
#else	/* NO_PP */
extern	SNMP_PKT_T *	SNMP_Create_Request();
extern	SNMP_PKT_T *	SNMP_Create_Trap();
extern	int		SNMP_Bind_Integer();
extern	int		SNMP_Bind_Unsigned_Integer();
extern	int		SNMP_Bind_IP_Address();
extern	int		SNMP_Bind_Object_ID();
extern	int		SNMP_Bind_String();
extern	int		SNMP_Bind_Null();
extern	VB_T *		locate_vb();
#endif	/* NO_PP */

#endif	/* buildpkt_inc */
