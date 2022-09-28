/* Copyright 1988 - 09/20/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)mib.h	2.17 96/09/20 Sun Microsystems"
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

/* $Header:   E:/SNMPV2/H/MIB.H_V   2.0   31 Mar 1990 15:11:24  $	*/
/*
 * $Log:   E:/SNMPV2/H/MIB.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:24
 * Release 2.00
 * 
 *    Rev 1.8   24 Sep 1989 22:06:34
 * Renamed mib_root to mib_root_node to support the MIB compiler.
 * 
 *    Rev 1.7   11 Jan 1989 11:59:48
 * Updated copyright dates.
 * 
 *    Rev 1.6   11 Jan 1989 11:09:20
 * Added definitions of find_object_node() and find_next_object() for non-
 * ANSI compilers.
 * 
 *    Rev 1.5   10 Oct 1988 21:47:00
 * Reorganized source modules
 * 
 *    Rev 1.4   21 Sep 1988 16:34:00
 * Revised spelling to conform to the usage "MGMT" in the MIB.
 * 
 *    Rev 1.3   20 Sep 1988 15:50:10
 * Revised END_OF_ARC macro to avoid an improper type cast.
 * 
 *    Rev 1.2   19 Sep 1988 19:56:06
 * Revised MIBARC structure and ARC macro to avoid casting structure
 * pointers to a (char *) which, on machines like the Cray, could
 * destroy the meaning of the pointer.
 * 
 *    Rev 1.1   19 Sep 1988 17:27:06
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:18
 * Initial revision.
*/

#if (!defined(mib_inc))
#define mib_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

/* Definitions for "form" parameter to the "testproc" found in leaf nodes */
#define	TEST_GET	0
#define	TEST_SET	1

/* Definitions for node_type in MIBNODE_T and MIBLEAF_T */
#define	INNER_NODE	0x00
#define	LEAF_NODE	0x01

/* Define a MIB leaf node */
typedef	struct	MIBLEAF_S
	{
	unsigned short	node_type;	/* See above			    */
	OCTET_T		expected_tag;	/* VT_xxx from snmp.h		    */
	OCTET_T		access_type;	/* See below			    */
	int		(*testproc)();	/* Routine to check whether the     */
					/* indicated data object exists	    */
	int		(*getproc)();	/* Routine to get the data	    */
	int		(*setproc)();	/* Routine to set the data	    */
	int		(*nextproc)();	/* Routine to locate "next" obj id  */
	char *		user_cookie;	/* Value to pass to unchanged	    */
	UINT_16_T	locator;	/* The user can define any value for*/
					/*  this field.  It is useful to    */
					/*  give easy to handle names to    */
					/*  leaves.			    */
	OCTET_T		view_mask;	/* A mask of the views of which this*/
					/* leaf is a part.		    */
	OCTET_T		write_mask;	/* That subset of view_mask in which*/
					/* is potentially writable.	    */
#if defined(DEBUG)
	char *		object_name;	/* FOR DEBUGGING		    */
#endif
	} MIBLEAF_T;

/* Bit values for access_type */
#define	READ_ACCESS	0x01
#define	WRITE_ACCESS	0x02
#define	RO	READ_ACCESS			/* READ ONLY ACCESS	*/
#define	WO	WRITE_ACCESS			/* WRITE ONLY ACCESS	*/
#define	RW	(READ_ACCESS | WRITE_ACCESS)	/* READ/WRITE ACCESS	*/

/* Define an MIB internal (or root) node */
typedef	struct	MIBNODE_S
	{
	unsigned short	node_type;	/* See above			    */
	struct MIBARC_S	*arcs;		/* Descendents from this node	    */
	} MIBNODE_T;

/* Define a pointer from a node to subsidiary node or a leaf	*/
/* These structures are aggregated into an array.		*/
/* THE LAST ELEMENT IN SUCH AN ARRAY MUST HAVE A NULL POINTER.	*/
typedef	struct	MIBARC_S
	{
	OIDC_T		id;		/* Object identifier component	*/
	struct MIBARC_S *nodep;		/* The descendent node/leaf	*/
	} MIBARC_T;

#define	MIB_ISO		1	/* Level 0 - ISO *IS* #1 by fiat (!!)	*/
#define	MIB_CCITT	2	/* Level 0				*/
#define	MIB_JOINT_ISO_CCITT	3	/* Level 0			*/
#define	MIB_ORG		3	/* Level 1 - {ISO 3}			*/
#define	MIB_DOD		6	/* Level 2 - {ORG 6}			*/
#define	MIB_INTERNET	1	/* Level 3 - {DOD 1}			*/


#define	MIB_DIRECTORY	1	/* Level 4 - {INTERNET 1}		*/
#define	MIB_MGMT	2	/* Level 4 - {INTERNET 2}		*/
#define	MIB_MIB2	1	/* Level 5 - {MGMT 1}			*/
#define	MIB_MIB1	1	/* Level 5 - {MGMT 1}			*/
#define	MIB_SYSTEM	1	/* Level 6 - {MIB 1}			*/
#define	MIB_INTERFACES	2	/* Level 6 - {MIB 2}			*/
#define	MIB_AT		3	/* Level 6 - {MIB 3}			*/
#define	MIB_IP		4	/* Level 6 - {MIB 4}			*/
#define	MIB_ICMP	5	/* Level 6 - {MIB 5}			*/
#define	MIB_TCP		6	/* Level 6 - {MIB 6}			*/
#define	MIB_UDP		7	/* Level 6 - {MIB 7}			*/
#define	MIB_EGP		8	/* Level 6 - {MIB 8}			*/

#define	MIB_EXPERIMENTAL	3	/* Level 4 - {INTERNET 3}	*/
#define	MIB_IETF	1	/* Level 5 - {EXPERIMENTAL 1}		*/ 
#define	MIB_PRIVATE	4	/* Level 4 - {INTERNET 4}		*/
#define	MIB_ENTERPRISE	1	/* Level 5 - {PRIVATE 1}		*/


#define ARC(ID,NODE)		{ID, (struct MIBARC_S *)&NODE}
#define END_OF_ARC_LIST		{0, (struct MIBARC_S *)0}
#define NODE(NAME, ARCLIST)	{INNER_NODE, ARCLIST}
/*#define NULLPROC		(int (*)())0 */

/* The LEAF macro is for release 1 mib.c and MIB Compiler */
#define	LEAF(NAME, VT, ACCESS, TESTP, GETP, SETP, NXTP, COOKIE)	\
				{LEAF_NODE, VT, ACCESS,		\
				TESTP, GETP, SETP, NXTP,	\
				(char *)COOKIE, 0, 0xFF, 0xFF}

/* The LEAFM macro is for release 2 mib.c and MIB Compiler */
#define	LEAFM(NM, VT, ACC, TSTP, GETP, SETP, NXTP, CKE, LOC, VMSK, WMSK)	\
				{LEAF_NODE, VT, ACC,			\
				TSTP, GETP, SETP, NXTP,			\
				(char *)CKE, LOC, VMSK, WMSK }


typedef	struct	MIBLOC_S
	{
	UINT_16_T		ml_flags;
	OIDC_T			ml_last_match;
	/* The following item splits out the "instance" part of the	*/
	/* full object identifier.  The number of components in the	*/
	/* "base" part (i.e. the path to the leaf may be derived by	*/
	/* vb_obj_id.num_components - ml_base_objid.num_components and	*/
	/* the list of components being taken directly from		*/
	/* vb_obj_id.component_list.					*/
	OBJ_ID_T		ml_remaining_objid;
	union {
	      MIBLEAF_T		*mlleaf_u;
	      MIBNODE_T		*mlnode_u;
	      } mbl_u;
	} MIBLOC_T;
#define	ml_leaf		mbl_u.mlleaf_u
#define	ml_node		mbl_u.mlnode_u

/* Values for ml_flags */
#define	ML_IS_LEAF	0x01

extern	MIBNODE_T	mib_root_node;

#endif	/* mib_inc */
