/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)libfuncs.h	2.16 96/07/25 Sun Microsystems"
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

/* $Header:   E:/SNMPV2/H/LIBFUNCS.H_V   2.0   31 Mar 1990 15:11:22  $	*/
/*
 * $Log:   E:/SNMPV2/H/LIBFUNCS.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:22
 * Release 2.00
 * 
 *    Rev 1.8   17 Mar 1989 20:49:24
 * Inclusion of stdlib.h now controlled by preprocessor flag
 * 
 *    Rev 1.7   10 Oct 1988 21:46:58
 * Reorganized source modules
 * 
 *    Rev 1.6   19 Sep 1988 17:27:10
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.5   17 Sep 1988 20:43:10
 * Added null macro for user to use to provide a routine to generate
 * the authentication failure trap.
 * 
 *    Rev 1.4   17 Sep 1988 13:07:30
 * Further correction of the comments.
 * 
 *    Rev 1.3   17 Sep 1988 12:31:54
 * Added comments to describe the parameters to the validation macros.
 * 
 *    Rev 1.2   17 Sep 1988 12:21:08
 * Moved packet validation macros out of rcv_pkt.c into libfuncs.h.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:44
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:16
 * Initial revision.
*/

#if (!defined(libfuncs_inc))
#define libfuncs_inc

#include <string.h>
#if (!defined(NO_STDLIB))
#include <stdlib.h>
#endif

/* Some compilers use <memory.h> and others <mem.h>	*/
#if (!defined(NO_MEMH))
#if !defined(MEMH)
#include <memory.h>
#else	/* MEMH */
#include <mem.h>
#endif	/* MEMH */
#endif	/* NO_MEMH */

#if (!defined(snmp_inc))
#include <snmp.h>
#endif

#if (!defined(localio_inc))
#include <localio.h>
#endif

/* min and max macros */
#if (!defined(max))
#define max(a,b)    (((a) > (b)) ? (a) : (b))
#endif
#if (!defined(min))
#define min(a,b)    (((a) < (b)) ? (a) : (b))
#endif

/**********************************************************************
 validate_SNMP_version --
	 
	 The parameter to this macro/procedure is an INT_32_T which
	 represents the SNMP version field in a received SNMP packet.
		 
	 This macro (or procedure replacing the macro) should return
	 the integer value 1 if the version is OK, 0 if NOT OK.
 **********************************************************************/
#define	validate_SNMP_version(P)	((P) == VERSION_RFC1067)

/**********************************************************************
 validate_SNMP_community -- Check an operation against the community name.
	 Get class operations can use either the set or get community.
	 Set operations may use only the set community.
	 
	 The parameters to this procedure are:
		 SNMP_PKT_T *	The received packet (decoded format)
		 SNMPADDR_T *	Source of the packet
		 SNMPADDR_T *	Destination of the packet (most likely
				the address of the machine on which this
				code is running.)
		 LCL_FILE *	Input local stream.  This stream is positioned
				at the first byte of the after the community
				in the received packet.  In other words, the
				stream is positioned either at the PDU or the
				authentication data.

	 This procedure should return a pointer to a local i/o stream
	 containing the ASN.1 PDU.

	 This routine may hang additional data onto the "private" field of
	 the packet structure.
	 The user will be given the opportinity to release that memory via
	 release_private().

	 (LCL_FILE *)0 should be returned if the validation fails.
 **********************************************************************/
#if !defined(NO_PP)
extern	LCL_FILE * validate_SNMP_community(SNMP_PKT_T *, SNMPADDR_T *,
					   SNMPADDR_T *, LCL_FILE *);
extern	void	release_private(SNMP_PKT_T *);
#else	/* NO_PP */
extern	LCL_FILE *	validate_SNMP_community();
extern	void	release_private();
#endif	/* NO_PP */

/**********************************************************************
 gen_auth_fail_trap -- Issue an authentication failure trap.
 **********************************************************************/
#if !defined(NO_PP)
extern	void	gen_auth_fail_trap(void);
#else	/* NO_PP */
extern	void	gen_auth_fail_trap();
#endif	/* NO_PP */

#if !defined(NO_PP)
extern	char *		SNMP_mem_alloc(unsigned int);
extern	void		SNMP_mem_free(char *);
extern	int		validate_set_pdu(SNMP_PKT_T *);
extern	int		user_pre_set(SNMP_PKT_T *);
extern	void		user_post_set(SNMP_PKT_T *);
extern	ALENGTH_T	user_auth_encode(SNMP_PKT_T *, EBUFFER_T *);
#else	/* NO_PP */
extern	char *		SNMP_mem_alloc();
extern	void		SNMP_mem_free();
extern	int		validate_set_pdu();
extern	int		user_pre_set();
extern	void		user_post_set();
extern	ALENGTH_T	user_auth_encode();
#endif	/* NO_PP */

#endif	/* libfuncs_inc */
