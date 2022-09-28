/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)rcv_pkt.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)rcv_pkt.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/RCV_PKT.C_V   2.0   31 Mar 1990 15:06:52  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/RCV_PKT.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:52
 * Release 2.00
 * 
 *    Rev 1.9   14 Dec 1989 16:00:54
 * Added support for Borland Turbo C compiler
 * 
 *    Rev 1.8   27 Apr 1989 15:56:28
 * Removed unused variables
 * 
 *    Rev 1.7   04 Mar 1989 13:22:58
 * When SNMP_Decode_Packet returned an error, an attempt was made to
 * free a non-existent packet structure, causing chaos.
 * 
 *    Rev 1.6   11 Jan 1989 12:27:40
 * Fix Process_Received_SNMP_Packet to reject any but REQUEST packets.
 * 
 *    Rev 1.5   19 Sep 1988 17:26:44
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.4   17 Sep 1988 20:41:54
 * A user-supplied routine is now called to generate the authentication
 * trap packet.
 * 
 *    Rev 1.3   17 Sep 1988 12:20:44
 * Moved packet validation macros out of rcv_pkt.c into libfuncs.h.
 * 
 *    Rev 1.2   15 Sep 1988 20:05:06
 * After parsing an erroneous packet, the packet structure was not being freed.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:20
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:06
 * Initial revision.
*/

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <decode.h>
#include <snmp.h>

#include <libfuncs.h>

#define	then

#if !defined(NO_PP)
static	int	Get_PDU_Common(SNMP_PKT_T *, EBUFFER_T *);
#else	/* NO_PP */
static	int	Get_PDU_Common();
#endif	/* NO_PP */

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
#if !defined(NO_PP)
extern void input_pkt(SNMP_PKT_T *);	/****<<<<ADDED AT SUN>>>****/
extern void output_pkt(SNMP_PKT_T *);	/****<<<<ADDED AT SUN>>>****/
#else	/* NO_PP */
extern void input_pkt();		/****<<<<ADDED AT SUN>>>****/
extern void output_pkt();		/****<<<<ADDED AT SUN>>>****/
#endif	/* NO_PP */

/****************************************************************************
NAME:  Process_Received_SNMP_Packet

PURPOSE:  Manage the decoding of an incoming SNMP packet.

PARAMETERS:
	unsigned char *	Address of the packet
	int		length of the packet
	SNMPADDR_T *	Source of the packet
	SNMPADDR_T *	Destination of the packet (most likely
			the address of the machine on which this
			code is running.)
	EBUFFER_T *	Buffer to hold any generated result packet.
			Space to hold the packet will be allocated if
			necessary.
			The EBUFFER_T structure must have been initialized
			using EBufferInitialize() or been setup using
			EBufferSetup().

RETURNS:  int		0: A good response of some type is in the result
			buffer.
			-1: An error, the result buffer contents should be
			discarded.

NOTE: The result packet may be any valid SNMP response packet.
****************************************************************************/
#if !defined(NO_PP)
int
Process_Received_SNMP_Packet(unsigned char *	pktp,
			     int		pktl,
			     SNMPADDR_T * 	pktsrc,
			     SNMPADDR_T * 	pktdst,
			     EBUFFER_T *	rebuffp)
#else	/* NO_PP */
int
Process_Received_SNMP_Packet(pktp, pktl, pktsrc, pktdst, rebuffp)
	unsigned char	*pktp;
	int		pktl;
	SNMPADDR_T * 	pktsrc;
	SNMPADDR_T * 	pktdst;
	EBUFFER_T	*rebuffp;
#endif	/* NO_PP */
{
SNMP_PKT_T	*rp;

#if defined(SGRP)
inc_counter(snmp_stats.snmpInPkts);
#endif

if ((rp = SNMP_Decode_Packet(pktp, pktl, pktsrc, pktdst)) == (SNMP_PKT_T *)0)
   then {
	return -1;
	}

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
input_pkt(rp);	/****<<<<ADDED AT SUN>>>****/

switch(rp->pdu_type)
   {
   case GET_REQUEST_PDU:
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInGetRequests);
#endif
	if (Get_PDU_Common(rp, rebuffp) == -1)
	   then {
		SNMP_Free(rp);
		return -1;
		}

	/* A valid response packet is in the result buffer */
	break;

   case GET_NEXT_REQUEST_PDU:
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInGetNexts);
#endif
	if (Get_PDU_Common(rp, rebuffp) == -1)
	   then {
		SNMP_Free(rp);
		return -1;
		}

	/* A valid response packet is in the result buffer */
	break;

   case SET_REQUEST_PDU:
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInSetRequests);
#endif
	if (Process_SNMP_Set_PDU(rp, rebuffp) == -1)
	   then {
		SNMP_Free(rp);
		return -1;
		}

	/* A valid response packet is in the result buffer */
#if defined(SGRP)
	inc_counter(snmp_stats.snmpOutGetResponses);
#endif
	break;

   default:
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInBadTypes);
#endif
	SNMP_Free(rp);
	return -1;
   }

SNMP_Free(rp);
#if defined(SGRP)
inc_counter(snmp_stats.snmpOutPkts);
#endif
return 0;
}

/****************************************************************************
NAME:  Get_PDU_Common

PURPOSE:  Do the common processing for GET REQUEST and GET NEXT REQUEST PDUs.

PARAMETERS:
	SNMP_PKT_T *	The decoded GET/GET NEXT PDU
	EBUFFER_T *	Buffer where the resulting PDU should be placed.
			If necessary, the memory to hold the PDU will be
			dynamically allocated.

RETURNS:  int		0 if things went OK and a reasonable packet is
			in the result buffer.  This DOES NOT mean that
			no errors have occurred -- the result buffer might
			contain an error response packet according to the
			SNMP protocol.
			-1 some sort of error has occurred, the result
			buffer should not be considered to hold a valid
			SNMP packet.
****************************************************************************/
#if !defined(NO_PP)
static
int
Get_PDU_Common(SNMP_PKT_T *	pktp,
	       EBUFFER_T *	ebuffp)
#else	/* NO_PP */
static
int
Get_PDU_Common(pktp, ebuffp)
	SNMP_PKT_T	*pktp;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
SNMP_PKT_T	*new_pkt;

if ((new_pkt = SNMP_Allocate()) == (SNMP_PKT_T *)0)
   then return -1;

/* Start building a response packet */
new_pkt->flags = pktp->flags;
new_pkt->snmp_version = pktp->snmp_version;

/* Copy the community */
if (EBufferClone(&(pktp->community), &(new_pkt->community)) == -1)
   then { /* Cloning failed */
	SNMP_Free(new_pkt);
	return -1;
	}

new_pkt->pdu_type = GET_RESPONSE_PDU;
new_pkt->pdu.std_pdu.request_id = pktp->pdu.std_pdu.request_id;
/* Assume that things are going to work */
new_pkt->pdu.std_pdu.error_status = NO_ERROR;
new_pkt->pdu.std_pdu.error_index = 0;

if ((new_pkt->pdu.std_pdu.std_vbl.vbl_count =
      pktp->pdu.std_pdu.std_vbl.vbl_count) == 0)
   then {
	new_pkt->pdu.std_pdu.std_vbl.vblist = (VB_T *)0;
	}
   else {
	if ((new_pkt->pdu.std_pdu.std_vbl.vblist =
	     VarBindList_Allocate(new_pkt->pdu.std_pdu.std_vbl.vbl_count))
	    == (VB_T *)0)
	   then {
		SNMP_Free(new_pkt);
		return -1;
		}
	}

if (Process_SNMP_GetClass_PDU(pktp, new_pkt, ebuffp) == -1)
   then {
	SNMP_Free(new_pkt);
	return -1;
	}

/* A valid response packet is now in the result e-buffer, although we	*/
/* don't know here whether the contents were generated using pktp or	*/
/* new_pkt.								*/

SNMP_Free(new_pkt);
#if defined(SGRP)
inc_counter(snmp_stats.snmpOutGetResponses);
#endif
return 0;
}
