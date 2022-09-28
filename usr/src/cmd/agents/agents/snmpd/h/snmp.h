/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)snmp.h	2.16 96/07/25 Sun Microsystems"
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

/* $Header:   D:/snmpv2/h/snmp.h_v   2.3   17 Jun 1990 10:39:42  $	*/
/*
 * $Log:   D:/snmpv2/h/snmp.h_v  $
 * 
 *    Rev 2.3   17 Jun 1990 10:39:42
 * Corrected erroneous definition of inc_gauge and dec_gauge macros.
 * 
 *    Rev 2.2   06 Jun 1990  5:32:40
 * Added definition of RFC1157.
 * 
 *    Rev 2.1   24 May 1990 16:40:52
 * Changed the authentication enable flag in snmp_stats from a long to a short.
 * 
 *    Rev 2.0   31 Mar 1990 15:11:24
 * Release 2.00
 * 
 *    Rev 1.9   24 Apr 1989 18:42:00
 * Added definition of VERSION_RFC1098 as a synonym for VERSION_RFC1067.
 * 
 *    Rev 1.8   24 Mar 1989 17:25:30
 * Removed reference to the variable named "etc_copyright".  The in-core
 * copyright notice now comes in via module "snmp_d.c".
 * 
 *    Rev 1.7   17 Mar 1989 21:42:02
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.6   11 Jan 1989 13:23:32
 * Added a reference to the copyright string to force it into memory.
 * 
 *    Rev 1.5   11 Jan 1989 12:46:44
 * Moved Clean_Obj_ID() to objectid.c
 * 
 *    Rev 1.4   19 Sep 1988 17:27:12
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.3   17 Sep 1988 14:40:56
 * Corrected the SNMP version number -- the format in RFC1067 is poorly
 * done!!!!!!!
 * 
 *    Rev 1.2   15 Sep 1988 20:04:22
 * Added macro to convert VBL offset into an error index.
 * 
 *    Rev 1.1   14 Sep 1988 19:23:30
 * Added definitions for the UDP port numbers assigned to SNMP.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:22
 * Initial revision.
*/

#if (!defined(snmp_inc))
#define snmp_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if (!defined(buffer_inc))
#include <buffer.h>
#endif

#if (!defined(mib_inc))
#include <mib.h>
#endif

/* Define the maximum packet size this implementation will accept.	*/
/* There is no hard upper limit.  SNMP_MAX_PACKET_SIZE should not be	*/
/* reduced below the value of SNMP_MIN_PACKET_SIZE.			*/
#define	SNMP_MAX_PACKET_SIZE		1400
#define	SNMP_MIN_PACKET_SIZE		484

/* Say what is the maximum number of components of an object identifer	*/
/* which we can handle, including those in the instance.		*/
#define	MAX_OID_COUNT			32

#define	VERSION_RFC1067			0
/* RFC1067, RFC1098, and RFC1157 are equivalent	*/
#define	VERSION_RFC1098			0
#define	VERSION_RFC1157			0

/* Define the ports assigned to SNMP for the reception of various types	*/
/* of packets.								*/
#define	SNMP_REQUEST_PORT		161
#define	SNMP_TRAP_PORT			162

/* The following represents a source or destination address in	*/
/* a generalized form (which happens to be isomorphic to the	*/
/* familiar sockaddr structure found with the socket interface.)*/
typedef struct SNMPADDR_S
	{
	unsigned short	snmp_family;
	unsigned char	snmp_data[14];
	} SNMPADDR_T;

/* Define the internal shape of a VarBind	*/
typedef	struct VB_S
	{
	struct VB_S *	vb_link;	  /* For the user to use, usually   */
					  /*  to build a list of related    */
					  /*  VB_Ts.			    */
	ALENGTH_T	vb_seq_size;	  /* Overall length of the VarBind  */
					  /* sequence when encoded.	    */
	OBJ_ID_T	vb_obj_id;	  /* Object id for this VarBind	    */
	OCTET_T		vb_data_flags_n_type;  /* Class form & type of data */
	OCTET_T		vb_flags;	  /* See below			    */
	ALENGTH_T	vb_data_length;	  /* Length of the data when encode */
	MIBLOC_T	vb_ml;		  /* The mib-leaf associated with   */
					  /* this VarBind.		    */
	union	{
		INT_32_T	v_number;	/* Integer kinds of things  */
		UINT_32_T	v_counter;	/* Unsigned int things	    */
		EBUFFER_T	v_string;	/* Octetstring things	    */
		OBJ_ID_T	v_object;	/* Object id things	    */
		unsigned char	v_network_address[4];
		} value_u;
	} VB_T;
#define	v_gauge		v_counter
#define	v_timeticks	v_counter
#define	v_opaque	v_string

/* Values for vb_data_flags_n_type	*/
#define	VT_NUMBER	A_INTEGER
#define	VT_STRING	A_OCTETSTRING
#define	VT_OBJECT	A_OBJECTID
#define	VT_EMPTY	A_NULL
#define	VT_IPADDRESS	(A_APPLICATION | 0)
#define	VT_COUNTER	(A_APPLICATION | 1)
#define	VT_GAUGE	(A_APPLICATION | 2)
#define	VT_TIMETICKS	(A_APPLICATION | 3)
#define	VT_OPAQUE	(A_APPLICATION | 4)

/* Values for vb_flags	*/
#define	VFLAG_ALREADY_TEST	0x01
#define	VFLAG_ALREADY_SET	0x02

/* Define the shape of the VarBindList */
typedef	struct VBL_S
	{
	ALENGTH_T	vbl_length;	/* Length of the VarBindList seq */
	int		vbl_count;	/* Number of Var Bind items	 */
	VB_T		*vblist;
	} VBL_T;

/* Define the internal representation of an SNMP packet */
typedef	struct SNMP_PKT_S
	{
	ALENGTH_T	buffer_needed;		/* Size of buffer needed   */
	ALENGTH_T	overall_length;		/* Message Sequence length */
	OCTET_T		mib_view;	/* Set of mib views which this	*/
					/* request can see. Use 0xFF	*/
					/* to participate in all views.	*/
	OCTET_T		flags;		/* See below.		        */
	UINT_16_T	lcl_ident;	/* Local transaction identifer	*/
	SNMPADDR_T	pkt_src;	/* Where this packet came from.	*/
	SNMPADDR_T	pkt_dst;	/* Where this packet came to.	*/
	char *		private;	/* Anything one wants -- but	*/
					/* remember, if you attach	*/
					/* something here, you got to	*/
					/* free it in release_private!!	*/
	INT_32_T	snmp_version;
	EBUFFER_T	community;
	ATVALUE_T	pdu_type;
	ALENGTH_T	pdu_length;
	union {
	      /* For GetRequest, GetNextRequest,	*/
	      /* GetResponse, and SetRequest PDUs.	*/
	      struct {
		     INT_32_T	request_id;
		     INT_32_T	error_status;
		     INT_32_T	error_index;
		     VBL_T	std_vbl;
		     } std_pdu;

	      /* For Trap PDU	*/
	      struct {
		     OBJ_ID_T		enterprise_objid;
		     unsigned char	net_address[4];
		     INT_32_T		generic_trap;
		     INT_32_T		specific_trap;
		     INT_32_T		trap_time_ticks;
		     VBL_T		trap_vbl;
		     } trap_pdu;
	      } pdu;
	} SNMP_PKT_T;


/* Values for pdu_type (class and form bits are not included) */
#define	GET_REQUEST_PDU			0
#define	GET_NEXT_REQUEST_PDU		1
#define	GET_RESPONSE_PDU		2
#define	SET_REQUEST_PDU			3
#define	TRAP_PDU			4
#define	NO_PDU				8

/* Values for error_status	*/
#define	NO_ERROR		0
#define	TOO_BIG			1
#define	NO_SUCH_NAME		2
#define	BAD_VALUE		3
#define	READ_ONLY		4
#define	GEN_ERR			5

/* Values for generic_trap	*/
#define	COLD_START		0
#define	WARM_START		1
#define	LINK_DOWN		2
#define	LINK_UP			3
#define	AUTH_FAILURE		4
#define	EGP_NEIGHBOR_LOSS	5
#define	ENTERPRISE_SPECIFIC	6

/* For flags in the SNMP_PKT_T */
/* Say what authentication to generate when encoding.	*/
#define	ERT_MASK	0x03
#define	ERT_STD		0x00		/* No authentication on response*/
#define	ERT_QUADSUM	0x01		/* Quadratic checksum method	*/
#define	ERT_DES		0x02		/* Do quadratic + DES		*/
#define	SNMP_ERROR_INDEX(I)	(I + 1)

#if defined(SGRP)
typedef	struct	SNMP_STATS_S
	{
	unsigned long	snmpInPkts;
	unsigned long	snmpOutPkts;
	unsigned long	snmpInBadVersions;
	unsigned long	snmpInBadCommunityNames;
	unsigned long	snmpInBadCommunityUses;
	unsigned long	snmpInASNParseErrs;
	unsigned long	snmpInBadTypes;
	unsigned long	snmpInTooBigs;
	unsigned long	snmpInNoSuchNames;
	unsigned long	snmpInBadValues;
	unsigned long	snmpInReadOnlys;
	unsigned long	snmpInGenErrs;
	unsigned long	snmpInTotalReqVars;
	unsigned long	snmpInTotalSetVars;
	unsigned long	snmpInGetRequests;
	unsigned long	snmpInGetNexts;
	unsigned long	snmpInSetRequests;
	unsigned long	snmpInGetResponses;
	unsigned long	snmpInTraps;
	unsigned long	snmpOutTooBigs;
	unsigned long	snmpOutNoSuchNames;
	unsigned long	snmpOutBadValues;
	unsigned long	snmpOutReadOnlys;
	unsigned long	snmpOutGenErrs;
	unsigned long	snmpOutGetRequests;
	unsigned long	snmpOutGetNexts;
	unsigned long	snmpOutSetRequests;
	unsigned long	snmpOutGetResponses;
	unsigned long	snmpOutTraps;
	unsigned short	snmpEnableAuthTraps;
	} SNMP_STATS_T;

extern	SNMP_STATS_T	snmp_stats;
#endif	/* SGRP */

/* Macros to manipulate Integers, Counters, and Gauges.	*/
/* These assume that these are held as longs.		*/
/* Integers are signed, counters and gauges are not.	*/
#define	inc_integer(I)		((I)++)
#define	dec_integer(I)		((I)--)
#define	add_integer(I,V)	((I) += (V))
#define	sub_integer(I,V)	((I) -= (V))

/* Counter's can't be decremented */
#define	inc_counter(C)		(C++)
#define	add_counter(C,V)	(C += (V))

/* Gauges latch at the maximum value */
#define GGMAX	((unsigned long)0xFFFFFFFF)
#define	inc_gauge(G)	((unsigned long)(G) != GGMAX ? G++ : G)
#define	dec_gauge(G)	((unsigned long)(G) != GGMAX ? G-- : G)
#define	add_gauge(G,V)	(G =	\
			(unsigned long)(V) > (GGMAX - (unsigned long)(G)) ? \
				GGMAX :	\
				(unsigned long)(G) + (unsigned long)(V))
#define	sub_gauge(G,V)	(G -= ((unsigned long)(G) != GGMAX ?	\
					(unsigned long)(V) : 0))

#if !defined(NO_PP)
extern	int	Process_Received_SNMP_Packet(unsigned char *, int,
					     SNMPADDR_T *, SNMPADDR_T *,
					     EBUFFER_T *);

extern	SNMP_PKT_T *	SNMP_Decode_Packet(unsigned char *, int,
					   SNMPADDR_T *, SNMPADDR_T *);
extern	VB_T *		VarBindList_Allocate(int);
extern	SNMP_PKT_T *	SNMP_Allocate(void);
extern	void		SNMP_Free(SNMP_PKT_T *);

extern	unsigned int	SNMP_Bufsize_For_Packet(SNMP_PKT_T *);
extern	void		SNMP_Encode_PDU(SNMP_PKT_T *, EBUFFER_T *);
extern	int		SNMP_Encode_Packet(SNMP_PKT_T *, EBUFFER_T *);

extern	int		Process_SNMP_GetClass_PDU(SNMP_PKT_T *, SNMP_PKT_T *,
						  EBUFFER_T *);

extern	int		Process_SNMP_Set_PDU(SNMP_PKT_T *, EBUFFER_T *);
extern	VB_T *		index_to_vbp(SNMP_PKT_T *, int);
extern	int		scan_vb_for_locator(SNMP_PKT_T *, int, UINT_16_T);
extern	int		oidcmp(int, OIDC_T *, int, OIDC_T *);
extern	int		find_object_node(VB_T *, SNMP_PKT_T *);
extern	int		find_next_object(OBJ_ID_T *, OBJ_ID_T *, SNMP_PKT_T *);

#else	/* NO_PP */

extern	int	Process_Received_SNMP_Packet();
extern	SNMP_PKT_T *	SNMP_Decode_Packet();
extern	VB_T *		VarBindList_Allocate();
extern	SNMP_PKT_T *	SNMP_Allocate();
extern	void		SNMP_Free();

extern	unsigned int	SNMP_Bufsize_For_Packet();
extern	void		SNMP_Encode_PDU();
extern	int		SNMP_Encode_Packet();

extern	int		Process_SNMP_GetClass_PDU();

extern	int		Process_SNMP_Set_PDU();
extern	VB_T *		index_to_vbp();
extern	int		scan_vb_for_locator();
extern	int		oidcmp();
extern	int		find_object_node();
extern	int		find_next_object();
#endif	/* NO_PP */

#endif	/* snmp_inc */
