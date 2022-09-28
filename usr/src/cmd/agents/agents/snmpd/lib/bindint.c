/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)bindint.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)bindint.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/BINDINT.C_V   2.0   31 Mar 1990 15:06:36  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/BINDINT.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:36
 * Release 2.00
 * 
 *    Rev 1.0   11 Jan 1989 12:11:52
 * Initial revision.
 *
 * Separated from buildpkt.c on January 11, 1989.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <snmp.h>
#include <objectid.h>
#include <buildpkt.h>

#define	then

/****************************************************************************
NAME:  SNMP_Bind_Integer

PURPOSE:  Bind an integer into a VarBindList

PARAMETERS:
	SNMP_PKT_T *	Packet structure on which the data is to be bound
	int		Index to the particular VarBind to be used.
				NOTE: This index zero based.
	int		Number of components in the data's object identifier
	OIDC_T *	Components of the data's object identifier
	INT_32_T	The value to be bound.

RETURNS:  int	0 if OK, -1 if failed
****************************************************************************/
#if !defined(NO_PP)
int
SNMP_Bind_Integer(
	SNMP_PKT_T	*pktp,
	int		indx,
	int		compc,
	OIDC_T		*compl,
	INT_32_T	value)
#else	/* NO_PP */
int
SNMP_Bind_Integer(pktp, indx, compc, compl, value)
	SNMP_PKT_T	*pktp;
	int		indx;
	int		compc;
	OIDC_T		*compl;
	INT_32_T	value;
#endif	/* NO_PP */
{
VB_T	*vbp;

if ((vbp = locate_vb(pktp, indx)) == (VB_T *)0)
  then return -1;

if (build_object_id(compc, compl, &(vbp->vb_obj_id)) == -1)
   then return -1;

vbp->vb_data_flags_n_type = VT_NUMBER;
vbp->value_u.v_number = value;

return 0;
}
