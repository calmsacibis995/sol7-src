/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)objectid.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)objectid.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/OBJECTID.C_V   2.0   31 Mar 1990 15:06:50  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/OBJECTID.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:50
 * Release 2.00
 * 
 *    Rev 1.4   17 Mar 1989 21:41:28
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.3   04 Mar 1989 10:35:34
 * Added cast to actual parameter on call to memcpy to avoid warnings on
 * some compilers.
 * 
 *    Rev 1.2   11 Jan 1989 12:46:44
 * Moved Clean_Obj_ID() to objectid.c
 * 
 *    Rev 1.1   14 Sep 1988 17:57:16
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:04
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <snmp.h>
#include <objectid.h>

#define	then

/****************************************************************************
NAME:  build_object_id

PURPOSE:  Build an object id structure

PARAMETERS:
	int		Number of components in the old structure
	OIDC_T *	List of components in the old structure
	OBJ_ID_T *	The new Object Identifier structure

RETURNS:  0 if sucessful, -1 if not
****************************************************************************/
#if !defined(NO_PP)
int
build_object_id(
	int		oldc,
	OIDC_T		*oldl,
	OBJ_ID_T	*new)
#else	/* NO_PP */
int
build_object_id(oldc, oldl, new)
	int		oldc;
	OIDC_T		*oldl;
	OBJ_ID_T	*new;
#endif	/* NO_PP */
{
new->component_list = (OIDC_T *)0;  /* Just in case the list is empty */

if ((new->num_components = oldc) != 0)
   then {
	unsigned int need;

	need = (unsigned int)(sizeof(OIDC_T) * new->num_components);
	if ((new->component_list = (OIDC_T *)SNMP_mem_alloc(need)) ==
		(OIDC_T *)0)
	   then return -1;

	if (need != 0) then (void)memcpy((char *)(new->component_list),
					 (char *)oldl, need);
	}

return 0;
}

/****************************************************************************
NAME:  Clean_Obj_ID

PURPOSE:  Clean up an OBJ_ID_T structure

PARAMETERS:	OBJ_ID_T *

RETURNS:  Nothing
****************************************************************************/
#if !defined(NO_PP)
void
Clean_Obj_ID(OBJ_ID_T *	objp)
#else	/* NO_PP */
void
Clean_Obj_ID(objp)
	OBJ_ID_T *	objp;
#endif	/* NO_PP */
{
if (objp->component_list != (OIDC_T *)0)
   then {
	SNMP_mem_free((char *)(objp->component_list));
	objp->component_list = (OIDC_T *)0;
	}
objp->num_components = 0;
}
