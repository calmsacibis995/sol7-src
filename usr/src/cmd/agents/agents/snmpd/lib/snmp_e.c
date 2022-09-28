/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)snmp_e.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)snmp_e.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   C:/snmpv2/snmp/snmp_e.c_v   2.1   17 May 1990 16:02:44  $	*/
/*
 * $Log:   C:/snmpv2/snmp/snmp_e.c_v  $
 * 
 *    Rev 2.1   17 May 1990 16:02:44
 * On traps, the timestamp was being mis-encoded as an integer rather than
 * as an unsigned integer.
 * 
 *    Rev 2.0   31 Mar 1990 15:06:58
 * Release 2.00
 * 
 *    Rev 1.6   27 Apr 1989 15:56:08
 * Removed unused variables
 * 
 *    Rev 1.5   19 Sep 1988 17:26:46
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.4   17 Sep 1988 16:53:42
 * The agent-addr and time-stamp fields in trap pdus were being assigned
 * an incorrect ASN.1 tag.
 * 
 *    Rev 1.3   16 Sep 1988 15:23:28
 * Had "then" where "else" should have been!
 * 
 *    Rev 1.2   15 Sep 1988 18:48:06
 * Removed comment block for a non-existant procedure.
 * 
 *    Rev 1.1   14 Sep 1988 17:55:00
 * Removed redundant + signs that older C compilers complain about.
 * Also moved includes of system includes into libfunc.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:08
 * Initial revision.
*/

#include <stdio.h>

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <encode.h>
#include <snmp.h>

#define	then

#if !defined(NO_PP)
static	void		encode_snmp_normal_pdu(SNMP_PKT_T *, EBUFFER_T *);
static	void		encode_snmp_trap_pdu(SNMP_PKT_T *, EBUFFER_T *);
static	int		encode_snmp_common(unsigned int, EBUFFER_T *,
					   ALENGTH_T, INT_32_T,
					   EBUFFER_T *);
static	unsigned int	bufsize_for_normal_pkt(SNMP_PKT_T *);
static	unsigned int	bufsize_for_trap_pkt(SNMP_PKT_T *);
static	void		encode_var_bind_list(VBL_T *, EBUFFER_T *);
static	ALENGTH_T	set_vbl_sizes(VBL_T *);
static	int		auth_encode(SNMP_PKT_T *, EBUFFER_T *);
#else	/* NO_PP */
static	void		encode_snmp_normal_pdu();
static	void		encode_snmp_trap_pdu();
static	int		encode_snmp_common();
static	unsigned int	bufsize_for_normal_pkt();
static	unsigned int	bufsize_for_trap_pkt();
static	void		encode_var_bind_list();
static	ALENGTH_T	set_vbl_sizes();
static	int		auth_encode();
#endif	/* NO_PP */

/****************************************************************************
NAME:  SNMP_Bufsize_For_Packet

PURPOSE:  Compute how much buffer space is needed to hold a packet if
	  it were encoded.

PARAMETERS:
	SNMP_PKT_T *	SNMP Packet structure

RETURNS:  unsigned int	The buffer size required.

NOTE:	This routine does not account for any size differences which may
	occur due to any special authentication encoding.
****************************************************************************/
#if !defined(NO_PP)
unsigned int
SNMP_Bufsize_For_Packet(SNMP_PKT_T *	rp)
#else	/* NO_PP */
unsigned int
SNMP_Bufsize_For_Packet(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
if (rp->pdu_type != TRAP_PDU)
   then return bufsize_for_normal_pkt(rp);
   else return bufsize_for_trap_pkt(rp);
}

/****************************************************************************
NAME:  SNMP_Encode_PDU

PURPOSE:  Encode the PDU portion of a packet

PARAMETERS:
	SNMP_PKT_T *	SNMP Packet structure
	EBUFFER_T *	The buffer to receive the packet

RETURNS:  nothing
****************************************************************************/
#if !defined(NO_PP)
void
SNMP_Encode_PDU(SNMP_PKT_T *	rp,
		EBUFFER_T *	ebuffp)
#else	/* NO_PP */
void
SNMP_Encode_PDU(rp, ebuffp)
	SNMP_PKT_T *	rp;
	EBUFFER_T *	ebuffp;
#endif	/* NO_PP */
{
if (rp->pdu_type != TRAP_PDU)
   then encode_snmp_normal_pdu(rp, ebuffp);
   else encode_snmp_trap_pdu(rp, ebuffp);
}

/****************************************************************************
NAME:  SNMP_Encode_Packet

PURPOSE:  Encode an SNMP packet.

PARAMETERS:
	SNMP_PKT_T *	SNMP Packet structure
	EBUFFER_T *	A buffer structure to receive the encoded packet

RETURNS:  0		Packet processed without error
	  -1		Error encountered during packet processing

	 On a sucessful return, the ebuffer passed as a parameter will
	 contain the encoded packet.  If necessary, space will be malloc-ed.

****************************************************************************/
#if !defined(NO_PP)
int
SNMP_Encode_Packet(SNMP_PKT_T *	rp,
		   EBUFFER_T *	ebuffp)
#else	/* NO_PP */
int
SNMP_Encode_Packet(rp, ebuffp)
	SNMP_PKT_T	*rp;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
unsigned int	need;

/* Is there any special encoding function that the user wants? */
if (rp->flags & ERT_MASK)
   then { /* Yup, the user needs to get involved. */
	return auth_encode(rp, ebuffp);
	}
   else {
	if (rp->pdu_type != TRAP_PDU)
	   then {
		need = bufsize_for_normal_pkt(rp);
		if (encode_snmp_common(need, ebuffp, rp->overall_length,
				       rp->snmp_version,
				       &(rp->community)) == -1)
		   then return -1;

		encode_snmp_normal_pdu(rp, ebuffp);
		return 0;
	        }
	   else {
		need = bufsize_for_trap_pkt(rp);
		if (encode_snmp_common(need, ebuffp, rp->overall_length,
				       rp->snmp_version,
				       &(rp->community)) == -1)
		   then return -1;

		encode_snmp_trap_pdu(rp, ebuffp);
		return 0;
	        }
	   }
}

/****************************************************************************
	ENCODE_SNMP_COMMON
****************************************************************************/
#if !defined(NO_PP)
static int
encode_snmp_common(unsigned int	buffl,
		   EBUFFER_T *	ebuffp,
		   ALENGTH_T	overall_length,
		   INT_32_T	snmp_version,
		   EBUFFER_T *	community)
#else	/* NO_PP */
static int
encode_snmp_common(buffl, ebuffp, overall_length, snmp_version, community)
	unsigned int	buffl;
	EBUFFER_T	*ebuffp;
	ALENGTH_T	overall_length;
	INT_32_T	snmp_version;
	EBUFFER_T	*community;
#endif	/* NO_PP */
{		
/* Allocate some space if necessary */
if (ebuffp->start_bp == (OCTET_T *)0)
   then { /* User is letting us get the space. */
	OCTET_T *buffp;
	/* Obtain space for the packet */
	if ((buffp = (OCTET_T *)SNMP_mem_alloc(buffl)) == (OCTET_T *)0)
	   then {
		return -1;
		}
	EBufferSetup(BFL_IS_DYNAMIC, ebuffp, buffp, buffl);
	}
   else { /* Make sure there is enough space in the buffer the user gave us */
	if (EBufferRemaining(ebuffp) < buffl)
	   then return -1;
	}

/* Generate the Message sequence header */
A_EncodeType(A_SEQUENCE, A_UNIVERSAL | A_CONSTRUCTOR,
	     A_EncodeHelper, (OCTET_T *)ebuffp);
A_EncodeLength(overall_length, A_EncodeHelper, (OCTET_T *)ebuffp);

A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE, snmp_version,
	    A_EncodeHelper, (OCTET_T *)ebuffp);
A_EncodeOctetString(A_OCTETSTRING, A_UNIVERSAL | A_PRIMITIVE,
		    community->start_bp, EBufferUsed(community),
		    A_EncodeHelper, (OCTET_T *)ebuffp);

return 0;
}

/****************************************************************************
	BUFSIZE_FOR_NORMAL_PKT
****************************************************************************/
#if !defined(NO_PP)
static
unsigned int
bufsize_for_normal_pkt(SNMP_PKT_T *	rp)
#else	/* NO_PP */
static
unsigned int
bufsize_for_normal_pkt(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
ALENGTH_T	alength;

/* In the following computations, the length of various tag and length	*/
/* fields are given as constants.  This is possible because the tag	*/
/* values are always low enough to fit into one octet.  And for various	*/
/* data types, in particular integers, the length field will always	*/
/* occupy one octet.							*/

rp->pdu_length = 2	/* Tag and length of request_id (an integer) */
		 + A_SizeOfInt(rp->pdu.std_pdu.request_id)
		 + 2	 /* Tag and length of error_status (an integer) */
		 + A_SizeOfInt(rp->pdu.std_pdu.error_status)
		 + 2	 /* Tag and length of error_index (an integer) */
		 + A_SizeOfInt(rp->pdu.std_pdu.error_index);

alength = set_vbl_sizes(&(rp->pdu.std_pdu.std_vbl));
rp->pdu_length += 1	/* Size of tag on VarBindList sequence */
		  + A_SizeOfLength(alength)
		  + alength;

alength = A_SizeOfOctetString(EBufferUsed(&(rp->community)));

rp->overall_length = 1	/* Size of tag on the PDU sequences */
		     + A_SizeOfLength(rp->pdu_length)
		     + rp->pdu_length
		     + 2    /* Tag and length of snmp_version (an integer) */
		     + A_SizeOfInt(rp->snmp_version)
		     + 1    /* Tag for the community octetstring */
		     + A_SizeOfLength(alength)
		     + alength;

rp->buffer_needed = rp->overall_length
		    + 1 /* Size of tag for overall Message sequence */
		    + A_SizeOfLength(rp->overall_length);

return rp->buffer_needed;
}

/****************************************************************************
	ENCODE_SNMP_NORMAL_PDU
****************************************************************************/
#if !defined(NO_PP)
static void
encode_snmp_normal_pdu(SNMP_PKT_T *	rp,
		       EBUFFER_T *	ebuffp)
#else	/* NO_PP */
static void
encode_snmp_normal_pdu(rp, ebuffp)
	SNMP_PKT_T	*rp;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
/* Generate the PDU header */
A_EncodeType(rp->pdu_type, A_DEFAULT_SCOPE | A_CONSTRUCTOR,
	     A_EncodeHelper, (OCTET_T *)ebuffp);
A_EncodeLength(rp->pdu_length, A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode request-id */
A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE, rp->pdu.std_pdu.request_id,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode error-status */
A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE,rp->pdu.std_pdu.error_status,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode error-index */
A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE, rp->pdu.std_pdu.error_index,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

encode_var_bind_list(&(rp->pdu.std_pdu.std_vbl), ebuffp);
}

/****************************************************************************
	BUFSIZE_FOR_TRAP_PKT
****************************************************************************/
#if !defined(NO_PP)
static
unsigned int
bufsize_for_trap_pkt(SNMP_PKT_T *	rp)
#else	/* NO_PP */
static
unsigned int
bufsize_for_trap_pkt(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
ALENGTH_T	alength;
/* In the following computations, the length of various tag and length	*/
/* fields are given as constants.  This is possible because the tag	*/
/* values are always low enough to fit into one octet.  And for various	*/
/* data types, in particular integers, the length field will always	*/
/* occupy one octet.							*/

rp->pdu_length = 2	/* Tag and length of request_id (an integer) */
		 + A_SizeOfObjectId(&(rp->pdu.trap_pdu.enterprise_objid))
		 + 2	 /* Tag and length of net_address (a string) */
		 + 4	 /* Size of IP address in SMI */
		 + 2	 /* Tag and length of generic_trap (an integer) */
		 + A_SizeOfInt(rp->pdu.trap_pdu.generic_trap)
		 + 2	 /* Tag and length of specific_trap (an integer) */
		 + A_SizeOfInt(rp->pdu.trap_pdu.specific_trap)
		 + 2	 /* Tag and length of trap_time_ticks (an integer) */
		 + A_SizeOfInt(rp->pdu.trap_pdu.trap_time_ticks);

alength = set_vbl_sizes(&(rp->pdu.trap_pdu.trap_vbl));
rp->pdu_length += 1	/* Size of tag on VarBindList sequence */
		  + A_SizeOfLength(alength)
		  + alength;

alength = A_SizeOfOctetString(EBufferUsed(&(rp->community)));

rp->overall_length = 1	/* Size of tag on the PDU sequences */
		     + A_SizeOfLength(rp->pdu_length)
		     + rp->pdu_length
		     + 2    /* Tag and length of snmp_version (an integer) */
		     + A_SizeOfInt(rp->snmp_version)
		     + 1    /* Tag for the community octetstring */
		     + A_SizeOfLength(alength)
		     + alength;

alength = rp->overall_length
		    + 1 /* Size of tag for overall Message sequence */
		    + A_SizeOfLength(rp->overall_length);

return alength;
}

/****************************************************************************
	ENCODE_SNMP_TRAP_PDU
****************************************************************************/
#if !defined(NO_PP)
static void
encode_snmp_trap_pdu(SNMP_PKT_T *	rp,
		     EBUFFER_T *	ebuffp)
#else	/* NO_PP */
static void
encode_snmp_trap_pdu(rp, ebuffp)
	SNMP_PKT_T	*rp;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
/* Generate the PDU header */
A_EncodeType(rp->pdu_type, A_DEFAULT_SCOPE | A_CONSTRUCTOR,
	     A_EncodeHelper, (OCTET_T *)ebuffp);
A_EncodeLength(rp->pdu_length, A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode enterprise */
A_EncodeObjectId(A_OBJECTID, A_UNIVERSAL | A_PRIMITIVE,
                 &(rp->pdu.trap_pdu.enterprise_objid),
		 A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode agent-addr */
A_EncodeOctetString(VT_IPADDRESS & ~A_IDCF_MASK,
		    VT_IPADDRESS & A_IDCF_MASK,
		    rp->pdu.trap_pdu.net_address, 4,
		    A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode generic-trap */
A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE,
	    rp->pdu.trap_pdu.generic_trap,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode specific-trap */
A_EncodeInt(A_INTEGER, A_UNIVERSAL | A_PRIMITIVE,
	    rp->pdu.trap_pdu.specific_trap,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

/* Encode time-stamp */
A_EncodeUnsignedInt(VT_TIMETICKS & ~A_IDCF_MASK,
	    VT_TIMETICKS & A_IDCF_MASK,
	    rp->pdu.trap_pdu.trap_time_ticks,
	    A_EncodeHelper, (OCTET_T *)ebuffp);

encode_var_bind_list(&(rp->pdu.trap_pdu.trap_vbl), ebuffp);
}

/****************************************************************************
NAME:  encode_var_bind_list

PURPOSE:  Encode a VarBindList

PARAMETERS:
	VBL_T *		The VarBindList to be encoded

RETURNS:  Nothing
****************************************************************************/
#if !defined(NO_PP)
static
void
encode_var_bind_list(VBL_T *	vblp,
		     EBUFFER_T *ebuffp)
#else	/* NO_PP */
static
void
encode_var_bind_list(vblp, ebuffp)
	VBL_T		*vblp;
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
VB_T		*vbp;

/* Generate the VarBindList sequence header */
A_EncodeType(A_SEQUENCE, A_UNIVERSAL | A_CONSTRUCTOR,
	     A_EncodeHelper, (OCTET_T *)ebuffp);
A_EncodeLength(vblp->vbl_length, A_EncodeHelper, (OCTET_T *)ebuffp);

if ((vbp = vblp->vblist) != (VB_T *)0)
   then {
	int i;
	for (i = 0; i < vblp->vbl_count; i++, vbp++)
	   {
	   A_EncodeType(A_SEQUENCE, A_UNIVERSAL | A_CONSTRUCTOR,
		        A_EncodeHelper, (OCTET_T *)ebuffp);
	   A_EncodeLength(vbp->vb_seq_size,
		        A_EncodeHelper, (OCTET_T *)ebuffp);

	   A_EncodeObjectId(A_OBJECTID, A_UNIVERSAL | A_PRIMITIVE,
		            &(vbp->vb_obj_id),
			    A_EncodeHelper, (OCTET_T *)ebuffp);

	   switch (vbp->vb_data_flags_n_type)
	      {
	      case VT_NUMBER:
		   A_EncodeInt(VT_NUMBER & ~A_IDCF_MASK,
			       VT_NUMBER & A_IDCF_MASK,
			       vbp->value_u.v_number,
			       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_COUNTER:
		   A_EncodeUnsignedInt(VT_COUNTER & ~A_IDCF_MASK,
				       VT_COUNTER & A_IDCF_MASK,
				       vbp->value_u.v_counter,
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_GAUGE:
		   A_EncodeUnsignedInt(VT_GAUGE & ~A_IDCF_MASK,
				       VT_GAUGE & A_IDCF_MASK,
				       vbp->value_u.v_gauge,
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_TIMETICKS:
		   A_EncodeUnsignedInt(VT_TIMETICKS & ~A_IDCF_MASK,
				       VT_TIMETICKS & A_IDCF_MASK,
				       vbp->value_u.v_timeticks,
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_STRING:
		   A_EncodeOctetString(VT_STRING & ~A_IDCF_MASK,
				       VT_STRING & A_IDCF_MASK,
				       vbp->value_u.v_string.start_bp,
				       EBufferUsed(&(vbp->value_u.v_string)),
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_OPAQUE:
		    A_EncodeOctetString(VT_OPAQUE & ~A_IDCF_MASK,
				       VT_OPAQUE & A_IDCF_MASK,
				       vbp->value_u.v_string.start_bp,
				       EBufferUsed(&(vbp->value_u.v_string)),
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_OBJECT:
		    A_EncodeObjectId(A_OBJECTID, A_UNIVERSAL | A_PRIMITIVE,
				     &(vbp->value_u.v_object),
				     A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_EMPTY:
		   A_EncodeType(VT_EMPTY & ~A_IDCF_MASK,
			        VT_EMPTY & A_IDCF_MASK,
			        A_EncodeHelper, (OCTET_T *)ebuffp);	
		   A_EncodeLength(0, A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      case VT_IPADDRESS:
		    A_EncodeOctetString(VT_IPADDRESS & ~A_IDCF_MASK,
				       VT_IPADDRESS & A_IDCF_MASK,
				       vbp->value_u.v_network_address,
				       4,
				       A_EncodeHelper, (OCTET_T *)ebuffp);
		   break;
	      default:
		   break;
	      }
           }
        }
}

/****************************************************************************
NAME:  set_vbl_sizes

PURPOSE:  Scan a VarBindList, setting the internal lengths and computing
	  the total length.

PARAMETERS:
	VBL_T *		The VarBindList structure to be scanned and set

RETURNS:  ALENGTH_T	The number of octets the VarBindList contents would
			use if ASN.1 encoded.
****************************************************************************/
#if !defined(NO_PP)
static
ALENGTH_T
set_vbl_sizes(VBL_T *	vblp)
#else	/* NO_PP */
static
ALENGTH_T
set_vbl_sizes(vblp)
	VBL_T	*vblp;
#endif	/* NO_PP */
{
VB_T		*vbp;
ALENGTH_T	vblist_size;

vblist_size = 0;
if ((vbp = vblp->vblist) != (VB_T *)0)
   then {
	int i;
	for (i = 0; i < vblp->vbl_count; i++, vbp++)
	   {
	   ALENGTH_T vb_size;	/* Accumulator of size of VarBind sequence */
	   ALENGTH_T obj_size;

	   obj_size = A_SizeOfObjectId(&(vbp->vb_obj_id));
	   vb_size = 1 /* The object ID tag is always 1 octet long */
		     + A_SizeOfLength(obj_size)
		     + obj_size;

	   switch (vbp->vb_data_flags_n_type)
	      {
	      case VT_NUMBER:
		   vbp->vb_data_length = A_SizeOfInt(vbp->value_u.v_number);
		   break;
	      case VT_COUNTER:
	      case VT_GAUGE:
	      case VT_TIMETICKS:
		   vbp->vb_data_length = A_SizeOfUnsignedInt(
						   vbp->value_u.v_counter);
		   break;
	      case VT_STRING:
	      case VT_OPAQUE:
		   {
		   ALENGTH_T used;
		   used = EBufferUsed(&(vbp->value_u.v_string));
		   vbp->vb_data_length = A_SizeOfOctetString(used);
		   }
		   break;
	      case VT_OBJECT:
		   vbp->vb_data_length =
			   A_SizeOfObjectId(&(vbp->value_u.v_object));
		   break;
	      case VT_EMPTY:
		   vbp->vb_data_length = 0;
		   break;
	      case VT_IPADDRESS:
		   vbp->vb_data_length = 4;
		   break;
	      default:
		   break;
	      }
           vbp->vb_seq_size = vb_size
			      + 1 /* The data tag is always 1 octet */
			      + A_SizeOfLength(vbp->vb_data_length)
			      + vbp->vb_data_length;

	   vblist_size += vbp->vb_seq_size
		       + 1 /* The sequence tag is always 1 octet */
		       + A_SizeOfLength(vbp->vb_seq_size);
           }
        }

vblp->vbl_length = vblist_size;

return vblist_size;
}

/****************************************************************************
NAME:  auth_encode

PURPOSE:  Encode an "authenticated" packet

PARAMETERS:
	SNMP_PKT_T *	SNMP Packet structure
	EBUFFER_T *	A buffer structure to receive the encoded packet

RETURNS:  0		Packet processed without error
	  -1		Error encountered during packet processing

	 On a sucessful return, the ebuffer passed as a parameter will
	 contain the encoded packet.  If necessary, space will be malloc-ed.

****************************************************************************/
#if !defined(NO_PP)
static
int
auth_encode(SNMP_PKT_T *	rp,
	    EBUFFER_T *		ebuffp)
#else	/* NO_PP */
static
int
auth_encode(rp, ebuffp)
     	SNMP_PKT_T *	rp;
	EBUFFER_T *	ebuffp;
#endif	/* NO_PP */
{
unsigned int	need;
EBUFFER_T 	apdu;
ALENGTH_T	alength;
ALENGTH_T	pdu_contents;	/* # of contents of PDU excluding the	*/
				/* ASN.1 tag/length fields themselves.	*/

/* Compute a number of sizes for the PDU contents ... */
if (rp->pdu_type != TRAP_PDU)
   then need = bufsize_for_normal_pkt(rp);
   else need = bufsize_for_trap_pkt(rp);

EBufferInitialize(&apdu);

if ((pdu_contents = user_auth_encode(rp, &apdu)) == 0)
   then {
	EBufferClean(&apdu);
	return -1;
        }

if ((need = EBufferUsed(&apdu)) == 0)
   then {
	EBufferClean(&apdu);
	return -1;
        }

/* pdu_length doesn't include the tag &	length fields.	*/
rp->pdu_length = pdu_contents;

alength = A_SizeOfOctetString(EBufferUsed(&(rp->community)));

rp->overall_length = 1	/* Size of tag on the PDU sequences */
		     + A_SizeOfLength(rp->pdu_length)
		     + rp->pdu_length
		     + 2    /* Tag and length of snmp_version (an integer) */
		     + A_SizeOfInt(rp->snmp_version)
		     + 1    /* Tag for the community octetstring */
		     + A_SizeOfLength(alength)
		     + alength;

need = (unsigned int)(rp->overall_length
		    + 1 /* Size of tag for overall Message sequence */
		    + A_SizeOfLength(rp->overall_length));

if (encode_snmp_common(need, ebuffp, rp->overall_length, rp->snmp_version,
		       &(rp->community)) == -1)
   then {
	EBufferClean(&apdu);
	return -1;
	}

EBufferAppend(ebuffp, &apdu);
EBufferClean(&apdu);
return 0;
}
