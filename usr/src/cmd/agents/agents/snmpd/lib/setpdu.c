/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)setpdu.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)setpdu.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/snmpv2/snmp/setpdu.c_v   2.4   09 Aug 1990 13:40:36  $	*/
/*
 * $Log:   E:/snmpv2/snmp/setpdu.c_v  $
 * 
 *    Rev 2.4   09 Aug 1990 13:40:36
 * Added -3 as a new potential return from testproc.  This allows a badValue
 * error to be returned.
 * Also fixed a misleading comment on an endif.
 * 
 *    Rev 2.3   22 Jun 1990 22:04:36
 * Removed a call to LEAVE_CRITICAL_SECTION which was misplaced during
 * a recent reorganization of the code.
 * 
 *    Rev 2.2   06 Jun 1990 23:58:36
 * Corrected a misleading comment.
 * 
 *    Rev 2.1   30 May 1990 11:32:52
 * Fixed a comment to properly describe the range of error return values
 * from testproc().
 * 
 *    Rev 2.0   31 Mar 1990 15:06:54
 * Release 2.00
 * 
 *    Rev 1.8   27 Apr 1989 15:56:06
 * Removed unused variables
 * 
 *    Rev 1.7   17 Mar 1989 21:41:30
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.6   11 Jan 1989 12:41:06
 * Added critical section macros.
 * 
 *    Rev 1.5   20 Sep 1988 18:35:32
 * Made note of the fact that under RFC1067, an attempt to set a read-only
 * variable should cause a noSuchName error rather than a readOnly error.
 * 
 *    Rev 1.4   19 Sep 1988 17:26:48
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.3   19 Sep 1988  9:24:00
 * Reversed change made in revision 1.2
 * 
 *    Rev 1.2   15 Sep 1988 20:04:08
 * Recalculated size of outgoing packet before transmission.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:20
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
#include <snmp.h>
#include <mib.h>

#define	then

#define	ENTER_CRITICAL_SECTION
#define	LEAVE_CRITICAL_SECTION

#if !defined(NO_PP)
static	void	call_the_set_routine(VB_T *, SNMP_PKT_T *, int);
#else	/* NO_PP */
static	void	call_the_set_routine();
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
error_response -- a local utility routine to generate an error response.
****************************************************************************/
#if !defined(NO_PP)
static
int
error_response(SNMP_PKT_T *	pktp,
	       EBUFFER_T *	ebuffp,
	       int		indx,
	       int		err_code)
#else	/* NO_PP */
static
int
error_response(pktp, ebuffp, indx, err_code)
               SNMP_PKT_T *	pktp;
	       EBUFFER_T *	ebuffp;
	       int		indx;
	       int		err_code;
#endif	/* NO_PP */
{
pktp->pdu_type = GET_RESPONSE_PDU;
pktp->pdu.std_pdu.error_status = (INT_32_T)err_code;
pktp->pdu.std_pdu.error_index = SNMP_ERROR_INDEX(indx);

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
output_pkt(pktp);	/****<<<<ADDED AT SUN>>>****/

return SNMP_Encode_Packet(pktp, ebuffp);
}

/****************************************************************************
NAME:  Process_SNMP_Set_PDU

PURPOSE:  Process a Set PDU -- given a decoded Set PDU, validate that
	  the named variables exist, are of the correct type, and are
	  writable.  If so, set all the variables.  In either event,
	  generate the response packet.

PARAMETERS:
	SNMP_PKT_T *	The decoded Set PDU, this will be used for the
			response.
	EBUFFER_T *	Buffer where the resulting PDU should be placed.
			The memory to hold the PDU will be dynamically
			allocated if necessary.

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
int
Process_SNMP_Set_PDU(SNMP_PKT_T	*	pktp,
		     EBUFFER_T *	ebuffp)
#else	/* NO_PP */
int
Process_SNMP_Set_PDU(pktp, ebuffp)
		     SNMP_PKT_T	*	pktp;
		     EBUFFER_T *	ebuffp;
#endif	/* NO_PP */
{
VB_T	*vbp;
int	indx;

/* We will be using the request PDU to hold the response	*/
/* There is no need to really do much work on the PDU because	*/
/* the returned form is almost exactly the received form.	*/
/* Also, no check will be made for oversize response, because	*/
/* of the simularity: a no-error response will be exactly the	*/
/* same size as the GET REQUEST, an error response may be a bit	*/
/* larger (perhaps by a byte or two), but we are forced to send	*/
/* it.								*/
pktp->pdu_type = GET_RESPONSE_PDU;

/* Make some basic checks on the items in the varBindList:	*/
/* Check whether each item is in the basic view.		*/
/* Check whether the item is, in fact, an attribute.		*/
/* Check that the item is writable.				*/

/* BELIEVE IT OR DON'T: RFC1067 wants an attempt to set a read	*/
/* only variable failed with a noSuchName error, not a readOnly */
/* error!!!!  For proof see section 4.1.5 (1) of RFC1098.  See	*/
/* #defined ENABLE_RO_ERROR sections below for how the code	*/
/* would look if a readOnly error were to be returned.		*/

if ((vbp = pktp->pdu.std_pdu.std_vbl.vblist) != (VB_T *)0)
   then {
	for (indx = 0;
	     indx < pktp->pdu.std_pdu.std_vbl.vbl_count;
	     indx++, vbp++)
	   {
	   /* Check whether the varBind is in the view */
	   if (find_object_node(vbp, pktp) == -1)
	      then {
#if (!defined(ENABLE_RO_ERROR))
		   /* This is the "official" RFC1067 way. */
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutNoSuchNames);
#endif
		   return error_response(pktp, ebuffp, indx, NO_SUCH_NAME);
#else	/* ENABLE_RO_ERROR */
		   /* This is the way one would think it*/
		   /* ought to be done.			*/
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutReadOnlys);
#endif
		   return error_response(pktp, ebuffp, indx, READ_ONLY);
#endif	/* ENABLE_RO_ERROR */
		   }

	   /* Make sure it is a leaf (i.e. an attribute) */
	   if (!(vbp->vb_ml.ml_flags & ML_IS_LEAF))
	      then {
#if (!defined(ENABLE_RO_ERROR))
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutNoSuchNames);
#endif
		   return error_response(pktp, ebuffp, indx, NO_SUCH_NAME);
#else	/* ENABLE_RO_ERROR */
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutReadOnlys);
#endif
		   return error_response(pktp, ebuffp, indx, READ_ONLY);
#endif	/* ENABLE_RO_ERROR */
		   }

	   /* Is it writable in this view? */
	   if (!((vbp->vb_ml.ml_leaf)->access_type & WRITE_ACCESS) ||
	       (((vbp->vb_ml.ml_leaf)->write_mask & pktp->mib_view) == 0))
	      then {
#if (!defined(ENABLE_RO_ERROR))
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutNoSuchNames);
#endif
		   return error_response(pktp, ebuffp, indx, NO_SUCH_NAME);
#else	/* ENABLE_RO_ERROR */
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutReadOnlys);
#endif
		   return error_response(pktp, ebuffp, indx, READ_ONLY);
#endif	/* ENABLE_RO_ERROR */
		   }

	   /* Make sure the proposed type matches what the MIB says */
	   if (vbp->vb_data_flags_n_type !=
	       (vbp->vb_ml.ml_leaf)->expected_tag)
	      then { /* Type mismatch */
#if defined(SGRP)
		   inc_counter(snmp_stats.snmpOutBadValues);
#endif
		   return error_response(pktp, ebuffp, indx, BAD_VALUE);
		   }
	   }
        }

ENTER_CRITICAL_SECTION;

/* Here the user gets a chance to take a global look at the PDU.	*/
/* The vb_ml structure in each varBind has already been set.		*/
/* The user's routine can do one of three things:			*/
/*   1) It can determine that the PDU is somehow inconsistent and should*/
/*	be bypassed (with an error PDU returned.)			*/
/*	Return code == -1						*/
/*   2) It can determine that the PDU is consistent and that the normal	*/
/*	handling of set pdus should proceed.				*/
/*	The user's routine may *NOT* any set operations.  To do so would*/
/*	invite the possiblity that some other set operation may fail its*/
/*	test function, thus causing the SET PDU to have only partial	*/
/*	effect, a violation of the SNMP specification.			*/
/*	However, the user's routine may indicate that it has validated	*/
/*	certain of the varBind entries and that the call to the testproc*/
/*	should be bypassed for those entries.  To do this, the user's	*/
/*	routine must set the VFLAG_ALREADY_TEST bit in the vb_flags	*/
/*	field of each of the VT_Ts that has been validated.		*/
/*	Return code == 0						*/
/*   3) It can determine that the PDU is consistent and perform *ALL* of*/
/*	the set operations itself, bypassing the normal set handling.	*/
/*	The user's routine should bump snmp_stats.snmpInTotalSetVars	*/
/*	by the appropriate amount to keep the SNMP statistics straight.	*/
/*	Return code == +1						*/
switch (validate_set_pdu(pktp))
   {
   case -1:
	/* The user's routine says that the PDU is bad.	*/
	LEAVE_CRITICAL_SECTION;
#if defined(SGRP)
	inc_counter(snmp_stats.snmpOutGenErrs);
#endif
	return error_response(pktp, ebuffp, 0, GEN_ERR);

   case 0:
	/* The user's routine says that the PDU is OK and that	*/
	/* normal handling of sets is to proceed.		*/
	break;	/* Go and do call the test routines */

   case 1:
   default:
	/* The user's routine says that the PDU is OK and that	*/
	/* it has performed the set operations.			*/
	LEAVE_CRITICAL_SECTION;

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
	output_pkt(pktp);	/****<<<<ADDED AT SUN>>>****/

	return SNMP_Encode_Packet(pktp, ebuffp);
   }

/* Ask each varBind whether it is willing to be set.	*/
if ((vbp = pktp->pdu.std_pdu.std_vbl.vblist) != (VB_T *)0)
   then {
	for (indx = 0;
	     indx < pktp->pdu.std_pdu.std_vbl.vbl_count;
	     indx++, vbp++)
	   {
	   int rcode;	/* The return code from the testproc */
	   /* See if this VB has already been tested */
	   if (vbp->vb_flags & VFLAG_ALREADY_TEST)
	      then continue;

	   /* Check whether the object is accessable	*/
	   /* For tabular variables, the object may not	*/
	   /* even exist.				*/
	   /* The testproc returns the following values:*/
           /*   0:  The variable is happy to be set.	*/
           /*   -1: A nosuch (or readonly) error.	*/
           /*   -2: A genErr.				*/
           /*   -3: A badValue.				*/
           /*   other values are reserved but cause	*/
           /*   nosuch/readonly.			*/
           if ((rcode = ((int)(*((vbp->vb_ml.ml_leaf)->testproc))
		     (TEST_SET,
		      vbp->vb_ml.ml_last_match,
		      vbp->vb_ml.ml_remaining_objid.num_components,
		      vbp->vb_ml.ml_remaining_objid.component_list,
		      vbp->vb_ml.ml_leaf->user_cookie, pktp, indx)))
		     != 0)
	      then {
		   switch (rcode)
		      {
		      default:
		      case -1: /* Do a nosuch or readonly error return */
#if (!defined(ENABLE_RO_ERROR))
			/* This is the "official" RFC1067 way. */
#if defined(SGRP)
			inc_counter(snmp_stats.snmpOutNoSuchNames);
#endif
			LEAVE_CRITICAL_SECTION;
			return error_response(pktp, ebuffp, indx,
					      NO_SUCH_NAME);
#else	/* ENABLE_RO_ERROR */
			/* This is the way one would think it*/
			/* ought to be done.			*/
#if defined(SGRP)
			inc_counter(snmp_stats.snmpOutReadOnlys);
#endif
			LEAVE_CRITICAL_SECTION;
			return error_response(pktp, ebuffp, indx, READ_ONLY);
#endif	/* ENABLE_RO_ERROR */

		      case -2: /* Do a genErr return */
#if defined(SGRP)
			inc_counter(snmp_stats.snmpOutGenErrs);
#endif
			LEAVE_CRITICAL_SECTION;
			return error_response(pktp, ebuffp, indx,
					      GEN_ERR);

		      case -3: /* Do a badValue return */
#if defined(SGRP)
			inc_counter(snmp_stats.snmpOutBadValues);
#endif
			LEAVE_CRITICAL_SECTION;
			return error_response(pktp, ebuffp, indx,
					      BAD_VALUE);
		      }
		   }
	   vbp->vb_flags |= VFLAG_ALREADY_TEST;
	   }
        }

/* Here the user gets another chance to take a global look at the PDU	*/
/* or to act on something acquired in the test procedures.		*/
/* The user's routine can do one of three things:			*/
/*   1) It can determine that the PDU is somehow inconsistent and should*/
/*	be bypassed (with an error PDU returned.)			*/
/*	Return code == -1						*/
/*   2) It can determine that the PDU is consistent and that the normal	*/
/*	handling of set pdus should proceed.				*/
/*	The user's routine may perform any set operations it choses.	*/
/*	If so, the user's routine may indicate that it has set		*/
/*	certain of the varBind entries and that the call to the setproc	*/
/*	should be bypassed for those entries.  To do this, the user's	*/
/*	routine must set the VFLAG_ALREADY_SET bit in the vb_flags	*/
/*	field of each of the VT_Ts that has been set.			*/
/*	The user's routine should bump snmp_stats.snmpInTotalSetVars	*/
/*	by the appropriate amount to keep the SNMP statistics straight.	*/
/*	Return code == 0						*/
/*   3) It can determine that the PDU is consistent and perform *ALL* of*/
/*	the set operations itself, bypassing the normal set handling.	*/
/*	The user's routine should bump snmp_stats.snmpInTotalSetVars	*/
/*	by the appropriate amount to keep the SNMP statistics straight.	*/
/*	Return code == +1						*/
switch (user_pre_set(pktp))
   {
   case -1:
	/* The user's routine says that the PDU is bad.	*/
	LEAVE_CRITICAL_SECTION;
#if defined(SGRP)
	inc_counter(snmp_stats.snmpOutGenErrs);
#endif
	return error_response(pktp, ebuffp, 0, GEN_ERR);

   case 0:
	/* The user's routine says that the PDU is OK and that	*/
	/* normal handling of sets is to proceed.		*/
	break;	/* Go and do call the test routines */

   case 1:
   default:
	/* The user's routine says that the PDU is OK and that	*/
	/* it has performed the set operations.			*/
	LEAVE_CRITICAL_SECTION;

/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
	output_pkt(pktp);	/****<<<<ADDED AT SUN>>>****/

	return SNMP_Encode_Packet(pktp, ebuffp);
   }

/* Now actually go and set the variables */
if ((vbp = pktp->pdu.std_pdu.std_vbl.vblist) != (VB_T *)0)
   then {
	for (indx = 0;
	     indx < pktp->pdu.std_pdu.std_vbl.vbl_count;
	     indx++, vbp++)
	   {
	   /* See if this VB has already been set.			*/
	   /* This can happen if one of the setprocs takes it upon	*/
	   /* itself to scan through the varBind list and set some other*/
	   /* entry.  Alternatively, the user's pre-set routine, called	*/
	   /* above may have done some sets.  If either is the case, the*/
	   /* routine that did the set should have set the		*/
	   /* VFLAG_ALREADY_SET bit in the vb_flags field of the VB_Ts	*/
	   /* that it has set.						*/
	   /* A wise setproc should check the VLAG_ALREADY_SET bit.	*/
	   if (vbp->vb_flags & VFLAG_ALREADY_SET)
	      then continue;

	   /* Since all the variables have been validated, we assume	*/
	   /* that everything will be a perfect match.			*/
           call_the_set_routine(vbp, pktp, indx);
	   vbp->vb_flags |= VFLAG_ALREADY_SET;
           }
        }

/* Now give the user one more chance to get control.  This could be use	*/
/* if the user's set procs cached some data and were waiting for the	*/
/* final "go ahead and set it" message.  (That message would come from	*/
/* the user's post set procedure.					*/
user_post_set(pktp);
LEAVE_CRITICAL_SECTION;


/**********************************************************************/
/*	<<<<< ADDED AT SUN >>>>>				      */
/**********************************************************************/
output_pkt(pktp);	/****<<<<ADDED AT SUN>>>****/

return SNMP_Encode_Packet(pktp, ebuffp);
}

/****************************************************************************
NAME:  call_the_set_routine

PURPOSE:  Once a leaf node is located, this routine will invoke the set
	  routine referenced by the node.

	  The set routine reference by a node MUST match the data type
	  of the node as expressed by variable "expected_tag" in the node.

PARAMETERS:
	  VB_T *	The varBind list entry to be set
	  SNMP_PKT_T *	The whole received packet
	  int		Index (zero based) in the varBindList of the VB_T
			passed as the first parameter.

RETURNS:  int		0 if things went OK.
			-1 if some sort of error has occurred.
****************************************************************************/
#if !defined(NO_PP)
static
void
call_the_set_routine(VB_T *		vbp,
		     SNMP_PKT_T *	pktp,
		     int		index)
#else	/* NO_PP */
static
void
call_the_set_routine(vbp, pktp, index)
		     VB_T *		vbp;
		     SNMP_PKT_T *	pktp;
		     int		index;
#endif	/* NO_PP */
{
MIBLOC_T	*mlp;
MIBLEAF_T	*leaf;

mlp = &(vbp->vb_ml);
leaf = mlp->ml_leaf;

switch (leaf->expected_tag)
   {
   case VT_NUMBER:
   case VT_COUNTER:
   case VT_GAUGE:
   case VT_TIMETICKS:
	   (void)(*(leaf->setproc))(
				    mlp->ml_last_match,
				    mlp->ml_remaining_objid.num_components,
				    mlp->ml_remaining_objid.component_list,
				    leaf->user_cookie,
				    vbp->value_u.v_number, pktp, index);
	   break;
   case VT_STRING:
   case VT_OPAQUE:
	   (void)(*(leaf->setproc))(
				    mlp->ml_last_match,
				    mlp->ml_remaining_objid.num_components,
				    mlp->ml_remaining_objid.component_list,
				    leaf->user_cookie,
				    vbp->value_u.v_string.start_bp,
				    (int)EBufferUsed(&vbp->value_u.v_string),
				    pktp, index);
	   break;
   case VT_OBJECT:
	   (void)(*(leaf->setproc))(
				    mlp->ml_last_match,
				    mlp->ml_remaining_objid.num_components,
				    mlp->ml_remaining_objid.component_list,
			            leaf->user_cookie,
				    vbp->value_u.v_object.num_components,
				    vbp->value_u.v_object.component_list,
				    pktp, index);
	   break;
   case VT_EMPTY:
	   break;
   case VT_IPADDRESS:
	   /* Calling sequence to set IP address is same as for strings */
	   (void)(*(leaf->setproc))(
				    mlp->ml_last_match,
				    mlp->ml_remaining_objid.num_components,
				    mlp->ml_remaining_objid.component_list,
				    leaf->user_cookie,
				    vbp->value_u.v_network_address, 4,
				    pktp, index);
	   break;
   default:
	   break;
   }
#if defined(SGRP)
inc_counter(snmp_stats.snmpInTotalSetVars);
#endif
}
