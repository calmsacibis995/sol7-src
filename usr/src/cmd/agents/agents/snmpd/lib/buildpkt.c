/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)buildpkt.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)buildpkt.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/BUILDPKT.C_V   2.0   31 Mar 1990 15:06:40  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/BUILDPKT.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:40
 * Release 2.00
 * 
 *    Rev 1.7   27 Apr 1989 15:55:58
 * Removed unused variables
 * 
 *    Rev 1.6   11 Jan 1989 12:11:22
 * Split into multiple files to prevent linker from picking up unused files.
 * 
 *    Rev 1.5   20 Sep 1988 22:51:38
 * Corrected erroneous call to EBufferInitialize (should have been
 * EBufferPreLoad.
 * 
 *    Rev 1.4   20 Sep 1988 19:11:50
 * Added procedure to build a VB list element containing a null value.
 * 
 *    Rev 1.3   19 Sep 1988 17:26:56
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.2   17 Sep 1988 18:57:14
 * Corrected miscalculation of whether a given var bind index was in-range.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:14
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:54
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <snmp.h>
#include <buildpkt.h>

#define	then

/****************************************************************************
NAME:  locate_vb

PURPOSE:  Find the address of a VarBind item in a packet structure

PARAMETERS:
	SNMP_PKT_T *	Packet structure on which the data is to be bound
	int		Index to the particular VarBind to be used.
				NOTE: This index zero based.

RETURNS:  VB_T *	(will be 0 on failure)
****************************************************************************/
#if !defined(NO_PP)
VB_T *
locate_vb(SNMP_PKT_T *	pktp,
	  int		indx)
#else	/* NO_PP */
VB_T *
locate_vb(pktp, indx)
	SNMP_PKT_T	*pktp;
	int		indx;
#endif	/* NO_PP */
{
VBL_T	*vblp;

if (pktp->pdu_type != TRAP_PDU)
   then vblp = &(pktp->pdu.std_pdu.std_vbl);
   else vblp = &(pktp->pdu.trap_pdu.trap_vbl);

/* Check whether the requested index is within the VarBindList	*/
if (vblp->vbl_count > indx)
   then return &(vblp->vblist[indx]);
   else return (VB_T *)0;

/*NOTREACHED*/
}
