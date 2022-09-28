/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)getpdu.c	2.16 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)getpdu.c	2.16 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/GETPDU.C_V   2.0   31 Mar 1990 15:06:34  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/GETPDU.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:34
 * Release 2.00
 * 
 *    Rev 1.9   27 Apr 1989 15:56:24
 * Removed unused variables
 * 
 *    Rev 1.8   19 Mar 1989 12:28:28
 * A call to memcpy was being made when the correct routine was memset.
 * This resulted in a copy of 4 bytes from address zero.  This occurred
 * when an attempt to get an IP address failed.
 * 
 *    Rev 1.7   17 Mar 1989 21:41:38
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.6   19 Sep 1988 17:26:06
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.5   19 Sep 1988  9:24:02
 * Reversed change made in revision 1.2
 * 
 *    Rev 1.4   15 Sep 1988 21:35:40
 * On the previous update, not all of the get procedures had the necessary
 * casts added.  This update fixes that.
 * 
 *    Rev 1.3   15 Sep 1988 21:22:06
 * Added casts to calls to the getproc to avoid improper type conversions
 * when "int" and pointers are of different sizes.
 * 
 *    Rev 1.2   15 Sep 1988 20:03:50
 * Recalculated size of outgoing packet before transmission.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:06
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:58
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <decode.h>
#include <snmp.h>
#include <mib.h>
#include <objectid.h>

#define	then

#define	ENTER_CRITICAL_SECTION
#define	LEAVE_CRITICAL_SECTION

#if !defined(NO_PP)
static	int	call_the_get_routine(VB_T *, SNMP_PKT_T *);
#else	/* NO_PP */
static	int	call_the_get_routine();
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
NAME:  Process_SNMP_GetClass_PDU

PURPOSE:  Process a Get Class PDU -- given a decoded Get or GetNext PDU,
	  obtain the requested variables and generate a response PDU.

PARAMETERS:
	SNMP_PKT_T *	The decoded Get PDU
	SNMP_PKT_T *	The pre-formed GET RESPONSE PDU
	EBUFFER_T *	Buffer where the resulting PDU should be placed.
			The memory to hold the PDU will be dynamically
			allocated.

RETURNS:  int		0 if things went OK and a reasonable packet is
			in the result buffer.  This DOES NOT mean that
			no errors have occurred -- the result buffer might
			contain an error response packet according to the
			SNMP protocol.
			-1 some sort of error has occurred, the result
			buffer should not be considered to hold a valid
			SNMP packet.

NOTE: For GET_NEXT_REQUEST PDUs there is a critical zone between the
      time that the "next" object identifier is ascertained and the time
      that the value for that object is obtained.  It is important that
      the identifier remain valid during this interval.

      The ENTER_CRITICAL_SECTION and LEAVE_CRITICAL_SECTION macros
      designate when this interval exists.  The macros are a bit over-
      inclusive -- For GET_REQUEST PDUs the interval exists only between
      the time of the call to testproc and the time the value is obtained.
****************************************************************************/
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
int
Process_SNMP_GetClass_PDU(SNMP_PKT_T	*pktp,
			  SNMP_PKT_T	*new_pkt,
			  EBUFFER_T	*ebuffp)
#else	/* NO_PP */
/*ARGSUSED*/
int
Process_SNMP_GetClass_PDU(pktp, new_pkt, ebuffp)
	SNMP_PKT_T	*pktp;
	SNMP_PKT_T	*new_pkt;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
VB_T		*vbp, *new_vbp;

if ((vbp = pktp->pdu.std_pdu.std_vbl.vblist) != (VB_T *)0)
   then {
	int indx;
	new_vbp = new_pkt->pdu.std_pdu.std_vbl.vblist;
	for (indx = 0;
	     indx < pktp->pdu.std_pdu.std_vbl.vbl_count;
	     indx++, vbp++, new_vbp++)
	   {
	   ENTER_CRITICAL_SECTION;

	   if (pktp->pdu_type == GET_REQUEST_PDU)
	      then {
		   if (clone_object_id(&(vbp->vb_obj_id),
				        &(new_vbp->vb_obj_id)) == -1)
		      then {
			   LEAVE_CRITICAL_SECTION;
			   pktp->pdu_type = GET_RESPONSE_PDU;
			   pktp->pdu.std_pdu.error_status = GEN_ERR;
			   pktp->pdu.std_pdu.error_index =
					   SNMP_ERROR_INDEX(indx);
#if defined(SGRP)
			   inc_counter(snmp_stats.snmpOutGenErrs);
#endif

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
			   output_pkt(pktp);

			   return SNMP_Encode_Packet(pktp, ebuffp);
			   }
		   }
	      else { /* Assume GET_NEXT_REQUEST_PDU */
		   if (find_next_object(&(vbp->vb_obj_id),
				        &(new_vbp->vb_obj_id), pktp) == -1)
		      then {
			   LEAVE_CRITICAL_SECTION;
			   pktp->pdu_type = GET_RESPONSE_PDU;
			   pktp->pdu.std_pdu.error_status = NO_SUCH_NAME;
			   pktp->pdu.std_pdu.error_index =
					   SNMP_ERROR_INDEX(indx);

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
			   output_pkt(pktp);

			   return SNMP_Encode_Packet(pktp, ebuffp);
			   }
		   }

	   if (find_object_node(new_vbp, pktp) == 0)
	      then {
		   if (new_vbp->vb_ml.ml_flags & ML_IS_LEAF)
		      then {
#if defined(DDEBUG)
			   printf("Found name=%s#$#\n",
				   new_vbp->vb_ml.ml_leaf->object_name);
#endif

			   /* Check whether the object is accessable	*/
			   /* For tabular variables, the object may not	*/
			   /* even exist.  We only do this for GET	*/
			   /* REQUEST PDUs and not GET NEXT REQUESTs	*/
			   /* because we assume that the find_next has	*/
			   /* returned a valid identifier.		*/
			   /* It is expected that the object will	*/
			   /* remain accessable until the actual value	*/
			   /* is retrieved by call_the_get_routine.	*/
			   if (pktp->pdu_type == GET_REQUEST_PDU)
			      then {
				   if ((int)(*((new_vbp->vb_ml.ml_leaf)->
					   testproc))
				       (TEST_GET,
				        new_vbp->vb_ml.ml_last_match,
				        new_vbp->vb_ml.ml_remaining_objid.num_components,
				        new_vbp->vb_ml.ml_remaining_objid.component_list,
				        new_vbp->vb_ml.ml_leaf->user_cookie,
					pktp, indx)
				      != 0)
			              then {
					   LEAVE_CRITICAL_SECTION;
					   pktp->pdu_type = GET_RESPONSE_PDU;
					   pktp->pdu.std_pdu.error_status =
							   NO_SUCH_NAME;
					   pktp->pdu.std_pdu.error_index =
						       SNMP_ERROR_INDEX(indx);

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
					   output_pkt(pktp);

					   return SNMP_Encode_Packet(pktp,
								ebuffp);
					   }
				   }

			   new_vbp->vb_data_flags_n_type =
				   new_vbp->vb_ml.ml_leaf->expected_tag;

			   if (call_the_get_routine(new_vbp, pktp)
			       == -1)
			      then {
				   LEAVE_CRITICAL_SECTION;
				   pktp->pdu_type = GET_RESPONSE_PDU;
				   pktp->pdu.std_pdu.error_status = GEN_ERR;
				   pktp->pdu.std_pdu.error_index =
						   SNMP_ERROR_INDEX(indx);
#if defined(SGRP)
				   inc_counter(snmp_stats.snmpOutGenErrs);
#endif

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
				   output_pkt(pktp);

				   return SNMP_Encode_Packet(pktp, ebuffp);
				   }
			   LEAVE_CRITICAL_SECTION;
			   continue;
			   }
		   }

	   /* Couldn't locate a node or the found node is NOT a leaf */
	   LEAVE_CRITICAL_SECTION;
	   pktp->pdu_type = GET_RESPONSE_PDU;
	   pktp->pdu.std_pdu.error_status = NO_SUCH_NAME;
	   pktp->pdu.std_pdu.error_index = SNMP_ERROR_INDEX(indx);
#if defined(SGRP)
	inc_counter(snmp_stats.snmpOutNoSuchNames);
#endif

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
	   output_pkt(pktp);

	   return SNMP_Encode_Packet(pktp, ebuffp);
           }
        }

if (SNMP_Bufsize_For_Packet(new_pkt) > SNMP_MAX_PACKET_SIZE)
   then {
        pktp->pdu_type = GET_RESPONSE_PDU;
	pktp->pdu.std_pdu.error_status = TOO_BIG;
	pktp->pdu.std_pdu.error_index = 0;
#if defined(SGRP)
	inc_counter(snmp_stats.snmpOutTooBigs);
#endif

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
	output_pkt(pktp);

	return SNMP_Encode_Packet(pktp, ebuffp);
	}


/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
output_pkt(new_pkt);

return SNMP_Encode_Packet(new_pkt, ebuffp);
}
/*lint +e715	*/

/****************************************************************************
NAME:  call_the_get_routine

PURPOSE:  Once a leaf node is located, this routine will invoke the get
	  routine referenced by the node.

	  The get routine reference by a node MUST match the data type
	  of the node as expressed by variable "expected_tag" in the node.

PARAMETERS:
	  VB_T *	The varBind list entry to be set
	  SNMP_PKT_T *	The received packet

RETURNS:  int		0 if things went OK.
			-1 if some sort of error has occurred.
****************************************************************************/
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
static
int
call_the_get_routine(VB_T *	 vbp,
		     SNMP_PKT_T *pktp)
#else	/* NO_PP */
/*ARGSUSED*/
static
int
call_the_get_routine(vbp, pktp)
	VB_T *		vbp;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
MIBLEAF_T	*leaf;
MIBLOC_T	*ml;

ml = &(vbp->vb_ml);
leaf = ml->ml_leaf;

switch (leaf->expected_tag)
   {
   case VT_NUMBER:
	   if (leaf->access_type & READ_ACCESS)
	      then {
		   vbp->value_u.v_number =
		      (*((INT_32_T (*)())(leaf->getproc)))(
				      ml->ml_last_match,
				      ml->ml_remaining_objid.num_components,
				      ml->ml_remaining_objid.component_list,
				      leaf->user_cookie, pktp);
		   }
	      else vbp->value_u.v_number = 0;
	   break;
   case VT_COUNTER:
   case VT_GAUGE:
   case VT_TIMETICKS:
	   if (leaf->access_type & READ_ACCESS)
	      then {
		   vbp->value_u.v_counter =
		      (*((UINT_32_T (*)())(leaf->getproc)))(
				      ml->ml_last_match,
				      ml->ml_remaining_objid.num_components,
				      ml->ml_remaining_objid.component_list,
				      leaf->user_cookie, pktp);
		   }
	      else vbp->value_u.v_number = 0;
	   break;
   case VT_STRING:
   case VT_OPAQUE:
	   if (leaf->access_type & READ_ACCESS)
	      then {
		   int		length;
		   OCTET_T	*cp;
		   cp = (*((OCTET_T * (*)())(leaf->getproc)))(
				      ml->ml_last_match,
				      ml->ml_remaining_objid.num_components,
				      ml->ml_remaining_objid.component_list,
			              leaf->user_cookie, &length, pktp);

		   EBufferPreLoad(BFL_IS_STATIC, &(vbp->value_u.v_string),
				   cp, length);
		   }
	      else { /* The string is not readable! */
		   EBufferInitialize(&(vbp->value_u.v_string));
		   }
	   break;
   case VT_OBJECT:
	   if (leaf->access_type & READ_ACCESS)
	      then {
		   OBJ_ID_T *objp;
		   objp = (*((OBJ_ID_T * (*)())(leaf->getproc)))(
				      ml->ml_last_match,
				      ml->ml_remaining_objid.num_components,
				      ml->ml_remaining_objid.component_list,
				      leaf->user_cookie, pktp);
		   if (clone_object_id(objp, &(vbp->value_u.v_object))
			== -1)
		      then {
			   vbp->value_u.v_object.num_components = 0;
			   vbp->value_u.v_object.component_list = (OIDC_T *)0;
			   }
		   }
	      else {
		   vbp->value_u.v_object.num_components = 0;
		   vbp->value_u.v_object.component_list = (OIDC_T *)0;
		   }
	   break;
   case VT_EMPTY:
	   break;
   case VT_IPADDRESS:
	   if (leaf->access_type & READ_ACCESS)
	      then {
		   OCTET_T	*cp;
		   cp = (*((OCTET_T * (*)())(leaf->getproc)))(
				      ml->ml_last_match,
				      ml->ml_remaining_objid.num_components,
				      ml->ml_remaining_objid.component_list,
				      leaf->user_cookie, pktp);
		   (void) memcpy(vbp->value_u.v_network_address, cp, 4);
		   }
	      else { /* The IP address is not readable! */
		   (void) memset(vbp->value_u.v_network_address, 0, 4);
		   }
	   break;
   default:
	   break;
   }

#if defined(SGRP)
inc_counter(snmp_stats.snmpInTotalReqVars);
#endif
return 0;
}
/*lint +e715	*/
