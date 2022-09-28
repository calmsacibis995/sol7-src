/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)creatreq.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)creatreq.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/CREATREQ.C_V   2.0   31 Mar 1990 15:06:42  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/CREATREQ.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:42
 * Release 2.00
 * 
 *    Rev 1.0   11 Jan 1989 12:11:20
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
NAME:  SNMP_Create_Request

PURPOSE:  Begin building an SNMP request packet.

PARAMETERS:
	int		Packet type: GET_REQUEST_PDU, GET_NEXT_REQUEST_PDU or
			SET_REQUEST_PDU as defined in snmp.h
	int		Protocol version -- must be VERSION_RFC1067 as
			defined in snmp.h
	int		Community name length
	char *		Community name (Must be static or global)
	INT_32_T	Request ID
	int		Number of VarBindList elements needed (may be 0)

RETURNS:  SNMP_PKT_T *	SNMP Packet structure, (SNMP_PKT_T *)0 on failure
****************************************************************************/
#if !defined(NO_PP)
SNMP_PKT_T *
SNMP_Create_Request(
	int		ptype,
	int		version,
	int		commleng,
	char		*community,
	INT_32_T	request_id,
	int		num_vb)
#else	/* NO_PP */
SNMP_PKT_T *
SNMP_Create_Request(ptype, version, commleng, community, request_id, num_vb)
	int		ptype;
	int		version;
	int		commleng;
	char		*community;
	INT_32_T	request_id;
	int		num_vb;
#endif	/* NO_PP */
{
SNMP_PKT_T	*rp;

if (!((ptype == GET_REQUEST_PDU) || (ptype == GET_NEXT_REQUEST_PDU) ||
      (ptype == SET_REQUEST_PDU)))
   then {
	return (SNMP_PKT_T *)0;
	}

if ((rp = SNMP_Allocate()) == (SNMP_PKT_T *)0)
   then {
	return (SNMP_PKT_T *)0;
	}

rp->pdu_type = (ATVALUE_T)ptype;

rp->snmp_version = version;

EBufferPreLoad(BFL_IS_STATIC, &(rp->community), community, commleng);

rp->pdu.std_pdu.request_id = request_id;
rp->pdu.std_pdu.error_status = 0;
rp->pdu.std_pdu.error_index = 0;

if ((rp->pdu.std_pdu.std_vbl.vbl_count = num_vb) == 0)
   then { /* Handle case where the VarBindList is empty */
	rp->pdu.std_pdu.std_vbl.vblist = (VB_T *)0;
	}
   else { /* The VarBindList has contents */
        if ((rp->pdu.std_pdu.std_vbl.vblist = VarBindList_Allocate(num_vb))
		== (VB_T *)0)
	   then {
		SNMP_Free(rp);
		return (SNMP_PKT_T *)0;
		}
	}
return rp;
}
