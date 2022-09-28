/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)snmp_d.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)snmp_d.c	2.15 96/07/23 Sun Microsystems";
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
 *     Copyright (c) 1988  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/SNMP/SNMP_D.C_V   2.0   31 Mar 1990 15:06:56  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/SNMP_D.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:56
 * Release 2.00
 * 
 *    Rev 1.12   16 Dec 1989 14:07:48
 * A check added to snmp_free() to avoid freeing a non-allocated block.
 * 
 *    Rev 1.11   14 Dec 1989 16:01:00
 * Added support for Borland Turbo C compiler
 * 
 *    Rev 1.10   27 Apr 1989 15:56:30
 * Removed unused variables
 * 
 *    Rev 1.9   24 Mar 1989 17:26:30
 * Module "copyrite.c" included to create in-core image of Epilogue
 * copyright notice.  Old method via "snmp.h" did not work for all
 * compilers.
 * 
 *    Rev 1.8   23 Mar 1989 12:03:58
 * Added more checks to protect against mis-encoded ASN.1.
 * 
 *    Rev 1.7   19 Mar 1989 13:04:52
 * Added protection against zero length, short, or overlength IP addresses.
 * 
 *    Rev 1.6   18 Mar 1989 11:58:42
 * Made changes to conform to string decoding simplification in decode.c.
 * 
 *    Rev 1.5   17 Mar 1989 21:41:46
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.4   04 Mar 1989 10:35:06
 * Added cast to actual parameter on call to memset to avoid warnings on
 * some compilers.
 * 
 *    Rev 1.3   11 Jan 1989 12:46:42
 * Moved Clean_Obj_ID() to objectid.c
 * 
 *    Rev 1.2   19 Sep 1988 17:26:50
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:22
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:06
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <decode.h>
#include <objectid.h>
#include <snmp.h>

#define	then

#if !defined(NO_PP)
static	void	Initialize_Pkt(SNMP_PKT_T *);
static	void	Clean_vb_list(VBL_T *);
static	void	Clean_Pkt(SNMP_PKT_T *);
static	int	decode_pkt_to_vblist(LCL_FILE *, VBL_T *, int);
static	int	decode_pkt_to_vb(LCL_FILE *, VB_T *);
static	int	count_var_binds(LCL_FILE *, ALENGTH_T);
#else	/* NO_PP */
static	void	Initialize_Pkt();
static	void	Clean_vb_list();
static	void	Clean_Pkt();
static	int	decode_pkt_to_vblist();
static	int	decode_pkt_to_vb();
static	int	count_var_binds();
#endif	/* NO_PP */

/*lint -e617	*/
#include "copyrite.c"
/*lint +e617	*/

/****************************************************************************
NAME:  SNMP_Decode_Packet

PURPOSE:  Decode an SNMP packet.

PARAMETERS:
	unsigned char *	Address of the packet
	int		length of the packet
	SNMPADDR_T *	Source of the packet
	SNMPADDR_T *	Destination of the packet (most likely
			the address of the machine on which this
			code is running.)

RETURNS:  SNMP_PKT_T *	SNMP Packet structure, (SNMP_PKT_T *)0 on failure
****************************************************************************/
#if !defined(NO_PP)
SNMP_PKT_T *
SNMP_Decode_Packet(unsigned char *	pktp,
		  int			pktl,
     		  SNMPADDR_T * 		pktsrc,
		  SNMPADDR_T * 		pktdst)
#else	/* NO_PP */
SNMP_PKT_T *
SNMP_Decode_Packet(pktp, pktl, pktsrc, pktdst)
	unsigned char	*pktp;
	int		pktl;
	SNMPADDR_T * 	pktsrc;
	SNMPADDR_T * 	pktdst;
#endif	/* NO_PP */
{
SNMP_PKT_T	*rp;
LCL_FILE	in_pkt_stream;
LCL_FILE	*in_stream;
ATVALUE_T	type;
ATVALUE_T	ptype;
ALENGTH_T	plength;
int		asn1leng;
int		asn1err = 0;
OCTET_T		flags;

if ((in_stream = Lcl_Open(&in_pkt_stream, pktp, pktl)) == (LCL_FILE *)0)
   then return (SNMP_PKT_T *)0;

/* Decode the top-level message sequence... */
flags = A_DecodeTypeClass(in_stream);
type = A_DecodeTypeValue(in_stream, &asn1err);
if (asn1err ||
    ((flags != (A_UNIVERSAL | A_CONSTRUCTOR)) && (type != A_SEQUENCE)))
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

if ((rp = SNMP_Allocate()) == (SNMP_PKT_T *)0)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInGenErrs);
#endif
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

rp->overall_length = A_DecodeLength(in_stream, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

/* Validate that the length provided by the caller is consistent with	*/
/* the length given by the ASN.1 wrapper...				*/
/* If necessary, shrink the local I/O buffer to match.			*/
asn1leng = Lcl_Tell(in_stream) + (int)(rp->overall_length);

if (asn1leng < pktl)
   then {
	(void) Lcl_Resize(in_stream, asn1leng, 0);
	}
   else {
	if (asn1leng > pktl)
	   then {
#if defined(SGRP)
	        inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}
	}

rp->snmp_version = A_DecodeInteger(in_stream, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

if (!validate_SNMP_version(rp->snmp_version))
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInBadVersions);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

/* Extract the community string */
A_DecodeOctetString(in_stream, &rp->community, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

   {
   LCL_FILE	*new_stream;
   if ((new_stream = validate_SNMP_community(rp, pktsrc, pktdst, in_stream))
       == (LCL_FILE *)0)
      then {
	   SNMP_Free(rp);
	   Lcl_Close(in_stream);
	   return (SNMP_PKT_T *)0;
           }
   in_stream = new_stream;
   }

/* Decode the packet type						*/
/* Since all of the PDUs follow the same form, we can decode them	*/
/* without being concerned as to which PDU we are decoding.		*/
/* Furthermore, since the VarBindList is the last thing in the PDU, we	*/
/* can ignore the overall length of the sequence forming the PDU.	*/

if (A_DecodeTypeClass(in_stream) != (A_DEFAULT_SCOPE | A_CONSTRUCTOR))
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

ptype = A_DecodeTypeValue(in_stream, &asn1err);
plength = A_DecodeLength(in_stream, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

/* Check whether there is an inconsistency between the PDU length and	*/
/* the overall length indicated by the outermost ASN.1 wrapper.		*/
/* If so, the packet is ill-formed and must be rejected.		*/
if (asn1leng != (Lcl_Tell(in_stream) + (int)plength))
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	SNMP_Free(rp);
	Lcl_Close(in_stream);
	return (SNMP_PKT_T *)0;
	}

if (ptype != TRAP_PDU)
   then { /* Its a non-trap form of pdu */
	rp->pdu_type = ptype;
	rp->pdu_length = plength;
	rp->pdu.std_pdu.request_id = A_DecodeInteger(in_stream, &asn1err);
	rp->pdu.std_pdu.error_status = A_DecodeInteger(in_stream, &asn1err);
	rp->pdu.std_pdu.error_index = A_DecodeInteger(in_stream, &asn1err);
	if (asn1err)
	   then {
#if defined(SGRP)
	        inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}

	/* Now deal with the VarBindList */
	if (decode_pkt_to_vblist(in_stream, &(rp->pdu.std_pdu.std_vbl),
				 asn1leng) == -1)
           then {
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}
	}
   else { /* It's a trap pdu */
	EBUFFER_T net_addr;
	unsigned int used;

	rp->pdu_type = TRAP_PDU;
	rp->pdu_length = plength;
	rp->pdu.trap_pdu.trap_vbl.vblist = (VB_T *)0;

	A_DecodeObjectId(in_stream,
	                 &(rp->pdu.trap_pdu.enterprise_objid),
			 &asn1err);
	if (asn1err)
	   then {
#if defined(SGRP)
	        inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}

	(void) memset(rp->pdu.trap_pdu.net_address, 0, 4);
	EBufferInitialize(&net_addr);
        A_DecodeOctetString(in_stream, &net_addr, &asn1err);
	if (asn1err)
	   then {
		EBufferClean(&net_addr);
#if defined(SGRP)
		inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}
	
	used = min(4, EBufferUsed(&net_addr));
	if (used != 0)
	   then {
		(void) memcpy(rp->pdu.trap_pdu.net_address, net_addr.start_bp,
			      used);
		EBufferClean(&net_addr);
		}

	rp->pdu.trap_pdu.generic_trap = A_DecodeInteger(in_stream, &asn1err);
	rp->pdu.trap_pdu.specific_trap = A_DecodeInteger(in_stream, &asn1err);
	rp->pdu.trap_pdu.trap_time_ticks = A_DecodeInteger(in_stream,
							   &asn1err);
	if (asn1err)
	   then {
#if defined(SGRP)
	        inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}

	/* Now deal with the VarBindList */
	if (decode_pkt_to_vblist(in_stream, &(rp->pdu.trap_pdu.trap_vbl),
				 asn1leng) == -1)
           then {
		SNMP_Free(rp);
		Lcl_Close(in_stream);
		return (SNMP_PKT_T *)0;
		}
	}

Lcl_Close(in_stream);
return rp;
}

/****************************************************************************
NAME:  count_var_binds

PURPOSE:  To figure out how many VarBind items are in a VarBindList
	  On entry, the input stream should be positioned to the start of the
	  data (contents) part of the VarBindList.
	  On exit, the input stream will be positioned to the start of the
	  data (contents) part of the VarBindList.

PARAMETERS:
	LCL_FILE *	The input stream

RETURNS:  int		Count of the entries
			-1 on error

Note:	This routine correctly handles the case where the VarBindList is
	empty.
****************************************************************************/
#if !defined(NO_PP)
static
int
count_var_binds(LCL_FILE *	stream,
		ALENGTH_T	leng)
#else	/* NO_PP */
static
int
count_var_binds(stream, leng)
	LCL_FILE	*stream;
	ALENGTH_T	leng;
#endif	/* NO_PP */
{
ALENGTH_T	used;
int tell_place;	/* Offset in stream to VarBind data */
int items;
int asn1err = 0;

tell_place = Lcl_Tell(stream);

for(items = 0, used = 0; used < leng;)
   {
   ALENGTH_T	alength;
   int start_place, end_place;

   start_place = Lcl_Tell(stream);
   if (Lcl_Eof(stream)) break;

   /* Skip over the VarBind sequence */
   (void) A_DecodeTypeValue(stream, &asn1err);
   alength = A_DecodeLength(stream, &asn1err);
   if (asn1err)
      then {
#if defined(SGRP)
	   inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	   items = -1;
	   break;
	   }
   if (Lcl_Seek(stream, (int)alength, 1) == -1)
      then {
#if defined(SGRP)
	   inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	   items = -1;
	   break;
	   }

   end_place = Lcl_Tell(stream);
   used = used + (ALENGTH_T)end_place - (ALENGTH_T)start_place;
   items++;
   }

(void) Lcl_Seek(stream, tell_place, 0);
return items;
}

/****************************************************************************
NAME:  decode_pkt_to_vblist

PURPOSE:  Decode a packet's VarBindList
	  On entry the input stream should be positioned to the tag field
	  of the first VarBind entry.
	  On exit, the stream pointer will be positioned to at the start
	  of the ASN.1 type field of AFTER the VarBindList, normally this
	  will be at the end of the packet.

PARAMETERS:
	LCL_FILE *	The input stream
	VBL_T *		The VarBindList header to be filled-in
	int		The overall size of the packet as indicated by
			the outermost ASN.1 wrapper for the SNMP packet
			(plus the size of the outermost tag/length fields.)

RETURNS:  0 for sucess, -1 for failure.
****************************************************************************/
#if !defined(NO_PP)
static
int
decode_pkt_to_vblist(LCL_FILE *	stream,
		     VBL_T *	vblp,
		     int	asn1leng)
#else	/* NO_PP */
static
int
decode_pkt_to_vblist(stream, vblp, asn1leng)
	LCL_FILE	*stream;
	VBL_T 		*vblp;
	int		asn1leng;
#endif	/* NO_PP */
{
VB_T	*vbp;
int	vbcnt;
int	i;
int	asn1err = 0;

/* Now deal with the VarBindList */
(void) A_DecodeTypeValue(stream, &asn1err);
vblp->vbl_length = A_DecodeLength(stream, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	return -1;
        }

/* Check whether there is an inconsistency between the VBL length and	*/
/* the overall length indicated by the outermost ASN.1 wrapper.		*/
/* If so, the packet is ill-formed and must be rejected.		*/

if (asn1leng != (Lcl_Tell(stream) + (int)(vblp->vbl_length)))
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	return -1;
        }

/* Count the number of VarBinds.			*/
vblp->vbl_count = 0;	/* Just in case things fail	*/
if ((vbcnt = count_var_binds(stream, vblp->vbl_length)) == -1)
   then return -1;

if (vbcnt == 0)
   then { /* Handle case where the VarBindList is empty */
	vblp->vblist = (VB_T *)0;
	}
   else { /* The VarBindList has contents */
	vblp->vbl_count = vbcnt;
	if ((vblp->vblist = VarBindList_Allocate(vblp->vbl_count))
		== (VB_T *)0)
	   then {
#if defined(SGRP)
	        inc_counter(snmp_stats.snmpInGenErrs);
#endif
		return -1;
	         }

        for (vbp = vblp->vblist, i = 0; i < vblp->vbl_count; i++, vbp++)
	   {
	   (void) A_DecodeTypeValue(stream, &asn1err);
	   vbp->vb_seq_size = A_DecodeLength(stream, &asn1err);
	   if (asn1err)
	      then {
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
		   return -1;
		   }
	   if (decode_pkt_to_vb(stream, vbp) == -1)
	      then return -1;
	   }
        }
return 0;
}

/****************************************************************************
NAME:  decode_pkt_to_vb

PURPOSE:  Decode a VarBind from a packet
	  On entry the input stream should be positioned to the tag field
	  of the VarBind entry.
	  On exit, the stream pointer will be positioned to at the start
	  of the ASN.1 type field of AFTER the VarBind.

PARAMETERS:
	LCL_FILE *	The input stream
	VB_T	*	The VB_T element to be filled.

RETURNS:  0 for sucess, -1 for failure.
****************************************************************************/
#if !defined(NO_PP)
static
int
decode_pkt_to_vb(LCL_FILE *	stream,
		 VB_T *		vbp)
#else	/* NO_PP */
static
int
decode_pkt_to_vb(stream, vbp)
	LCL_FILE	*stream;
	VB_T 		*vbp;
#endif	/* NO_PP */
{
OCTET_T		flags;
ATVALUE_T	id;
ALENGTH_T	leng;
int		asn1err = 0;

A_DecodeObjectId(stream, &(vbp->vb_obj_id), &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	return -1;
        }
flags = A_DecodeTypeClass(stream);
id = A_DecodeTypeValue(stream, &asn1err);
leng = A_DecodeLength(stream, &asn1err);
if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	return -1;
        }

vbp->vb_data_length = leng;
vbp->vb_data_flags_n_type = flags | (OCTET_T)id;

switch (vbp->vb_data_flags_n_type)
   {
   case VT_NUMBER:
	vbp->value_u.v_number = A_DecodeIntegerData(stream, leng, &asn1err);
	break;
   case VT_COUNTER:
   case VT_GAUGE:
   case VT_TIMETICKS:
	vbp->value_u.v_counter = (UINT_32_T)A_DecodeIntegerData(stream, leng,
								&asn1err);
	break;
   case VT_STRING:
   case VT_OPAQUE:
	A_DecodeOctetStringData(stream, leng, &(vbp->value_u.v_string),
				&asn1err);
	break;
   case VT_OBJECT:
	A_DecodeObjectIdData(stream, leng, &(vbp->value_u.v_object), &asn1err);
	break;
   case VT_EMPTY:
	/* Empty has no contents to be decoded */
	break;
   case VT_IPADDRESS:
	{
	EBUFFER_T ipbuff;
	unsigned int used;

	(void) memset(vbp->value_u.v_network_address, 0, 4);
	EBufferInitialize(&ipbuff);
	A_DecodeOctetStringData(stream, leng, &ipbuff, &asn1err);
	used = min(4, EBufferUsed(&ipbuff));
	if (used != 0)
	   then {
		(void) memcpy(vbp->value_u.v_network_address, ipbuff.start_bp,
			      used);
		EBufferClean(&ipbuff);
		}
	}
	break;
   default:
	return -1;
   }

if (asn1err)
   then {
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInASNParseErrs);
#endif
	return -1;
        }
   else return 0;

/*NOTREACHED*/
}

/****************************************************************************
NAME:  Initialize_Pkt

PURPOSE:  Initialize an SNMP packet structure.

PARAMETERS:
	SNMP_PKT_T *	The packet structure to be initialized

RETURNS:  nothing
****************************************************************************/
#if !defined(NO_PP)
static
void
Initialize_Pkt(SNMP_PKT_T *	rp)
#else	/* NO_PP */
static
void
Initialize_Pkt(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
rp->pdu_type = NO_PDU;
rp->private = (char *)0;
EBufferInitialize(&rp->community);
rp->pdu.std_pdu.std_vbl.vblist = (VB_T *)0;
}

/****************************************************************************
NAME:  Clean_vb_list

PURPOSE:  Clean up any dynamically allocated memory which may be attached
	  to an VarBindList

PARAMETERS:
	VBL_T *		The VarBindList to be cleaned

RETURNS:  nothing
****************************************************************************/
#if !defined(NO_PP)
static void
Clean_vb_list(VBL_T *	vblp)
#else	/* NO_PP */
static void
Clean_vb_list(vblp)
	VBL_T	*vblp;
#endif	/* NO_PP */
{
VB_T	*vbp;

if ((vbp = vblp->vblist) != (VB_T *)0)
   then {
	int i;
	for (i = 0; i < vblp->vbl_count; i++, vbp++)
	   {
	   Clean_Obj_ID(&(vbp->vb_obj_id));

	   switch (vbp->vb_data_flags_n_type)
	      {
	      case VT_STRING:
	      case VT_OPAQUE:
		   if (vbp->value_u.v_string.start_bp != (OCTET_T *)0)
		      then EBufferClean(&(vbp->value_u.v_string));
		   break;
	      case VT_OBJECT:
		   Clean_Obj_ID(&(vbp->value_u.v_object));
		   break;
	      }
	   }
	SNMP_mem_free((char *)(vblp->vblist));
	}
}

/****************************************************************************
NAME:  Clean_Pkt

PURPOSE:  Clean up any dynamically allocated memory which may be attached
	  to an SNMP packet structure.

PARAMETERS:
	SNMP_PKT_T *	The packet structure to be cleaned

RETURNS:  nothing
****************************************************************************/
#if !defined(NO_PP)
static void
Clean_Pkt(SNMP_PKT_T *	rp)
#else	/* NO_PP */
static void
Clean_Pkt(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
if (rp->private != (char *)0)
   then release_private(rp);

if (rp->community.start_bp != (OCTET_T *)0)
   then EBufferClean(&(rp->community));

if (rp->pdu_type != TRAP_PDU)
   then { /* Its a non-trap form of pdu */
	Clean_vb_list(&(rp->pdu.std_pdu.std_vbl));
	}
   else { /* It's a TRAP PDU */
	Clean_Obj_ID(&(rp->pdu.trap_pdu.enterprise_objid));
	Clean_vb_list(&(rp->pdu.trap_pdu.trap_vbl));
	}

Initialize_Pkt(rp);
}

/****************************************************************************
NAME:  SNMP_Free

PURPOSE:  Release an SNMP packet structure.

PARAMETERS:
	SNMP_PKT_T *	The packet structure to be released

RETURNS:  nothing
****************************************************************************/
#if !defined(NO_PP)
void
SNMP_Free(SNMP_PKT_T *	rp)
#else	/* NO_PP */
void
SNMP_Free(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
if (rp != (SNMP_PKT_T *)0)
   then {
	Clean_Pkt(rp);
	SNMP_mem_free((char *)rp);
	}
}

/****************************************************************************
NAME:  VarBindList_Allocate

PURPOSE:  Allocate a fresh set of VarBind structures

PARAMETERS:	Number of VarBind entries

RETURNS:  VB_T *	List of VarBind structures, (VB_T *)0 on failure
****************************************************************************/
#if !defined(NO_PP)
VB_T *
VarBindList_Allocate(int elements)
#else	/* NO_PP */
VB_T *
VarBindList_Allocate(elements)
	int elements;
#endif	/* NO_PP */
{
VB_T	*vbp;
int	need;
need = sizeof(VB_T) * elements;
if ((vbp = (VB_T *)SNMP_mem_alloc((unsigned int)need)) != (VB_T *)0)
   then {
	if (need != 0) then (void) memset((char *)vbp, 0, (unsigned int)need);
	}
return vbp;
}

/****************************************************************************
NAME:  SNMP_Allocate

PURPOSE:  Allocate a fresh SNMP packet structure.

PARAMETERS:	None

RETURNS:  SNMP_PKT_T *	SNMP Packet structure, (SNMP_PKT_T *)0 on failure
****************************************************************************/
SNMP_PKT_T *
SNMP_Allocate()
{
SNMP_PKT_T	*rp;

if ((rp = (SNMP_PKT_T *)SNMP_mem_alloc(sizeof(SNMP_PKT_T))) ==(SNMP_PKT_T *)0)
   then return (SNMP_PKT_T *)0;

Initialize_Pkt(rp);
return rp;
}
