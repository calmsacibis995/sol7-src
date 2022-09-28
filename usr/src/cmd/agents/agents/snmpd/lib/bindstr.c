/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)bindstr.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)bindstr.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/BINDSTR.C_V   2.0   31 Mar 1990 15:06:38  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/BINDSTR.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:38
 * Release 2.00
 * 
 *    Rev 1.2   17 Mar 1989 23:00:34
 * Handling of zero length strings was modified so that they are treated
 * as if they were statically allocated, but at address zero.
 * 
 *    Rev 1.1   17 Mar 1989 21:41:54
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.0   11 Jan 1989 12:11:58
 * Initial revision.
 *
 * Separated from buildpkt.c on January 11, 1989.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <buffer.h>
#include <snmp.h>
#include <objectid.h>
#include <buildpkt.h>

#define	then

/****************************************************************************
NAME:  SNMP_Bind_String

PURPOSE:  Bind a string into a VarBindList

PARAMETERS:
	SNMP_PKT_T *	Packet structure on which the data is to be bound
	int		Index to the particular VarBind to be used.
				NOTE: This index zero based.
	int		Number of components in the data's object identifier
	OIDC_T *	Components of the data's object identifier
	OCTET_T		Data type
	int		Number of octets in the string
	OCTET_T *	The string
	int		Flag indicating whether the string must be copied
			or whether it may be used in its current location.
			A value of 0 means "copy", 1 means "use as is".

RETURNS:  int	0 if OK, -1 if failed
****************************************************************************/
#if !defined(NO_PP)
int
SNMP_Bind_String(
	SNMP_PKT_T *	pktp,
	int		indx,
	int		compc,
	OIDC_T *	compl,
	OCTET_T		flags_n_type,
	int		leng,
	OCTET_T *	strp,
	int		statflg)
#else	/* NO_PP */
int
SNMP_Bind_String(pktp, indx, compc, compl, flags_n_type, leng, strp, statflg)
	SNMP_PKT_T	*pktp;
	int		indx;
	int		compc;
	OIDC_T		*compl;
	OCTET_T		flags_n_type;
	int		leng;
	OCTET_T		*strp;
	int		statflg;
#endif	/* NO_PP */
{
VB_T	*vbp;
OCTET_T	*buffp;

if ((vbp = locate_vb(pktp, indx)) == (VB_T *)0)
  then return -1;

if (build_object_id(compc, compl, &(vbp->vb_obj_id)) == -1)
   then return -1;

if ((statflg == 0) && (leng != 0))
   then {
	if ((buffp = (OCTET_T *)SNMP_mem_alloc((unsigned int)leng))
	     == (OCTET_T *)0)
	   then {
		Clean_Obj_ID(&(vbp->vb_obj_id));
		return -1;
		}
	(void) memcpy(buffp, strp,
		      (unsigned int)leng); /* Length known to be != zero */
	EBufferPreLoad(BFL_IS_DYNAMIC, &(vbp->value_u.v_string), buffp, leng);
	}
   else { /* Buffer is either static or has zero length */
	EBufferPreLoad(BFL_IS_STATIC, &(vbp->value_u.v_string),
		       leng != 0 ? strp : (OCTET_T *)0, leng);
	}

vbp->vb_data_flags_n_type = flags_n_type;

return 0;
}
