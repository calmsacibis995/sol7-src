/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)creatrap.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)creatrap.c	2.15 96/07/23 Sun Microsystems";
#endif
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

/* $Header:   E:/SNMPV2/SNMP/CREATRAP.C_V   2.0   31 Mar 1990 15:06:42  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/CREATRAP.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:42
 * Release 2.00
 * 
 *    Rev 1.1   17 Mar 1989 21:41:50
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.0   11 Jan 1989 12:11:22
 * Initial revision.
 *
 * Separated from buildpkt.c on January 11, 1989.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <buffer.h>
#include <snmp.h>
#include <objectid.h>
#include <buildpkt.h>

#define	then

/****************************************************************************
NAME:  SNMP_Create_Trap

PURPOSE:  Begin building an SNMP trap packet.

PARAMETERS:
	int		Protocol version -- must be VERSION_RFC1067 as
			defined in snmp.h
	int		Community name length
	char *		Community name (Must be static or global)
	int		Number of components in enterprise object id
	OIDC_T *	Components of the enterprise id
	OCTET_T *	An IP address of this device, consists of four
			bytes of IP address in standard network order
	int		Generic trap type
	INT_32_T	Specific trap type
	UINT_32_T	Time of trap
	int		Number of VarBindList elements needed (may be 0)

RETURNS:  SNMP_PKT_T *	SNMP Packet structure, (SNMP_PKT_T *)0 on failure
****************************************************************************/
#if !defined(NO_PP)
SNMP_PKT_T *
SNMP_Create_Trap(
	int		version,
	int		commleng,
	char		*community,
	int		enterprz_c,
	OIDC_T		*enterprz_l,
	OCTET_T		*agent_ip,
	int		generic,
	INT_32_T	specific,
	UINT_32_T	timestamp,
	int		num_vb)
#else	/* NO_PP */
SNMP_PKT_T *
SNMP_Create_Trap(version, commleng, community, enterprz_c, enterprz_l,
		 agent_ip, generic, specific, timestamp, num_vb)
	int		version;
	int		commleng;
	char		*community;
	int		enterprz_c;
	OIDC_T		*enterprz_l;
	OCTET_T		*agent_ip;
	int		generic;
	INT_32_T	specific;
	UINT_32_T	timestamp;
	int		num_vb;
#endif	/* NO_PP */
{
SNMP_PKT_T	*rp;

if ((rp = SNMP_Allocate()) == (SNMP_PKT_T *)0)
   then {
	return (SNMP_PKT_T *)0;
	}

rp->pdu_type = TRAP_PDU;

rp->snmp_version = version;
if (build_object_id(enterprz_c, enterprz_l,
		    &(rp->pdu.trap_pdu.enterprise_objid)) == -1)
   then {
	SNMP_Free(rp);
	return (SNMP_PKT_T *)0;
	}

(void) memcpy(rp->pdu.trap_pdu.net_address, agent_ip, 4);
rp->pdu.trap_pdu.generic_trap = generic;
rp->pdu.trap_pdu.specific_trap = specific;
rp->pdu.trap_pdu.trap_time_ticks = (INT_32_T)timestamp;

EBufferPreLoad(BFL_IS_STATIC, &(rp->community), community, commleng);

if ((rp->pdu.trap_pdu.trap_vbl.vbl_count = num_vb) == 0)
   then { /* Handle case where the VarBindList is empty */
	rp->pdu.trap_pdu.trap_vbl.vblist = (VB_T *)0;
	}
   else { /* The VarBindList has contents */
        if ((rp->pdu.trap_pdu.trap_vbl.vblist = VarBindList_Allocate(num_vb))
		== (VB_T *)0)
	   then {
		SNMP_Free(rp);
		return (SNMP_PKT_T *)0;
		}
	}
return rp;
}
