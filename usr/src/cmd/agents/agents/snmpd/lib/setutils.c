/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)setutils.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)setutils.c	2.15 96/07/23 Sun Microsystems";
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
 *     Copyright (c) 1990  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/SNMP/SETUTILS.C_V   2.0   31 Mar 1990 15:07:06  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/SETUTILS.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:07:06
 * Release 2.00
*/

#include <libfuncs.h>

#include <asn1.h>
#include <snmp.h>
#include <mib.h>

#define	then

/****************************************************************************
NAME:  index_to_vbp

PURPOSE:  Given a packet structure and the zero-based index of a varBind
	  element in that packet, return a pointer to that VB_T

PARAMETERS:
	  SNMP_PKT_T *	The whole packet.
	  int		The index of the VB_T desired.
			(0 is the index of the first VB_T).

RETURNS:  VB_T *	The address of the desired varBind element
			(VB_T *)0 returned if out of range.
****************************************************************************/
#if !defined(NO_PP)
VB_T *
index_to_vbp(SNMP_PKT_T * pktp,
	     int	  index)
#else	/* NO_PP */
VB_T *
index_to_vbp(pktp, index)
	     SNMP_PKT_T * pktp;
	     int	  index;
#endif	/* NO_PP */
{
VBL_T *	vblp;

if (pktp->pdu_type != TRAP_PDU)
   then vblp = &(pktp->pdu.std_pdu.std_vbl);
   else vblp = &(pktp->pdu.trap_pdu.trap_vbl);

if (index < vblp->vbl_count)
   then return &(vblp->vblist[index]);
   else return (VB_T *)0;
}

/****************************************************************************
NAME:  scan_vb_for_locator

PURPOSE:  scan the VB list of a packet structure looking for a particular
	  locator value.

PARAMETERS:
	  SNMP_PKT_T *	The whole received packet.
	  int		The index of the VB_T where to start the search
			(0 is the index of the first VB_T).
	  UINT_16_T	The desired locator.

RETURNS:  int		The index in the varBindList of the VB_T which
			references the MIB leaf with the specified locator.
			-1 if none are found.

CAVEAT: It is assumed that the vb_ml field of the various VB_Ts has been
	already set and has valid contents.

NOTE: It is possible for a VB list to have more multiple VB_Ts which
reference the same MIB leaf.

****************************************************************************/
#if !defined(NO_PP)
int
scan_vb_for_locator(SNMP_PKT_T *	pktp,
		    int			index,
		    UINT_16_T		loc)
#else	/* NO_PP */
int
scan_vb_for_locator(pktp, index, loc)
		    SNMP_PKT_T *	pktp;
		    int			index;
		    UINT_16_T		loc;
#endif	/* NO_PP */
{
VB_T *	vbp;
VBL_T *	vblp;

if (pktp->pdu_type != TRAP_PDU)
   then vblp = &(pktp->pdu.std_pdu.std_vbl);
   else vblp = &(pktp->pdu.trap_pdu.trap_vbl);

for (vbp = &(vblp->vblist[index]); index < vblp->vbl_count; index++,  vbp++)
   {
   if ((vbp->vb_ml.ml_leaf)->locator == loc) then return index;
   }

return -1;	/* Not found */
}

/****************************************************************************
NAME:  oidcmp

PURPOSE:  Compare two object identifiers

PARAMETERS:
	int		OID #1 number of elements
	OIDC_T *	OID #1 list of elements
	int		OID #2 number of elements
	OIDC_T *	OID #2 list of elements

RETURNS:  int	1 if o1 == o2
		0 if o1 != o2

****************************************************************************/
#if !defined(NO_PP)
int
oidcmp(int	compc_1,
       OIDC_T *	compl_1,
       int	compc_2,
       OIDC_T *	compl_2)
#else	/* NO_PP */
int
oidcmp(compc_1, compl_1, compc_2, compl_2)
       int	compc_1;
       OIDC_T *	compl_1;
       int	compc_2;
       OIDC_T *	compl_2;
#endif	/* NO_PP */
{
if (compc_1 != compc_2) then return 0;

while (compc_1-- > 0)
   {
   if (*compl_1++ != *compl_2++)
      then return 0;	/* OIDs are different */
   }

return 1;
}
