/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)objectid.h	2.16 96/07/25 Sun Microsystems"
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

/* $Header:   E:/SNMPV2/H/OBJECTID.H_V   2.0   31 Mar 1990 15:11:20  $	*/
/*
 * $Log:   E:/SNMPV2/H/OBJECTID.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:20
 * Release 2.00
 * 
 *    Rev 1.2   11 Jan 1989 12:46:46
 * Moved Clean_Obj_ID() to objectid.c
 * 
 *    Rev 1.1   19 Sep 1988 17:27:08
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:20
 * Initial revision.
*/

#if (!defined(objectid_inc))
#define objectid_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#define	clone_object_id(O, N) build_object_id(	\
				((OBJ_ID_T *)(O))->num_components,	\
				((OBJ_ID_T *)(O))->component_list, N)

#if !defined(NO_PP)
extern	int		build_object_id(int, OIDC_T *, OBJ_ID_T *);
extern	void		Clean_Obj_ID(OBJ_ID_T *);
#else	/* NO_PP */
extern	int		build_object_id();
extern	void		Clean_Obj_ID();
#endif	/* NO_PP */

#endif	/* objectid_inc */
