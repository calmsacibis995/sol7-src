/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)mibutils.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)mibutils.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/MIBUTILS.C_V   2.0   31 Mar 1990 15:06:50  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/MIBUTILS.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:50
 * Release 2.00
 * 
 *    Rev 1.4   24 Sep 1989 22:06:38
 * Renamed mib_root to mib_root_node to support the MIB compiler.
 * 
 *    Rev 1.3   27 Apr 1989 15:55:58
 * Removed unused variables
 * 
 *    Rev 1.2   19 Sep 1988 17:26:34
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:16
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:02
 * Initial revision.
*/

#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <decode.h>
#include <snmp.h>
#include <mib.h>
#include <objectid.h>

#define	then

#if !defined(NO_PP)
static	int	find_next_helper(MIBNODE_T *, OIDC_T, int,
				 OIDC_T *, OIDC_T *, SNMP_PKT_T *);
#else	/* NO_PP */
static	int	find_next_helper();
#endif	/* NO_PP */

/****************************************************************************
NAME:  find_object_node

PURPOSE:  Locate a node in the MIB tree corresponding to a given object id.
	  The search terminates sucessfully when all the object identifer
	  components have been consumed or a leaf node is encountered.
	  (One of the status flags in the MIBLOC_T structure indicates
	  whether the search terminated at an inner node or a leaf.)

	  The search terminates unsucessfully (return code -1) if an
	  object identifier component does not match those available at
	  a given inner node.

PARAMETERS:
	OBJ_ID_T *	Object ID to be used
	SNMP_PKT_T *	The received packet

RETURNS:  int		Zero if node located
			vbp->vb_ml structure will be filled in with lots
			of information about the found object.
			-1 if nothing located (MIBLOC_T is not valid)
****************************************************************************/
#if !defined(NO_PP)
int
find_object_node(VB_T * vbp,
		 SNMP_PKT_T * pktp)
#else	/* NO_PP */
int
find_object_node(vbp, pktp)
	VB_T *		vbp;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
OBJ_ID_T	*objp;
MIBLOC_T	*mlp;
MIBNODE_T	*np;
struct MIBARC_S	*ap;
OIDC_T		*compp;		/* Current object id component of interest */
int		comp_num;	/* Index of current object id component	   */

objp = &(vbp->vb_obj_id);
mlp = &(vbp->vb_ml);
for(comp_num = objp->num_components,
    compp = objp->component_list,
    np = &mib_root_node;
    (np->node_type == INNER_NODE) && (comp_num > 0);
    comp_num--, compp++)
   {
   for(ap = np->arcs;
       (MIBNODE_T *)(ap->nodep) != (MIBNODE_T *)0;
       ap++)
      {
      if (ap->id == *compp) then goto found_arc;
      }
   /* If we fall through the arcs, then we have failed in the search */
   return -1;

found_arc:
   /* Save the value of the matched arc so that later, when we call	*/
   /* the procs for the various objects we can tell them the last arc	*/
   /* matched.  This will allow the routines which access tabular	*/
   /* objects to know which column (attribute) of the table is being	*/
   /* used.								*/
   mlp->ml_last_match = *compp;
   np = (MIBNODE_T *)(ap->nodep);
   }

/* Here we have either run out of object identifiers or we have hit	   */
/* a leaf node (in which case there may be zero or more identifers left.)  */
if (np->node_type == LEAF_NODE)
   then {
	/* Is this variable even within the given view of the MIB? */
	if ((((MIBLEAF_T *)np)->view_mask & pktp->mib_view) == 0)
	   then return -1;
	mlp->ml_flags = ML_IS_LEAF;
	}
   else mlp->ml_flags = 0;

mlp->ml_remaining_objid.num_components = comp_num;
mlp->ml_remaining_objid.component_list = compp;
mlp->ml_node = np;

return 0;
}

/****************************************************************************
NAME:  find_next_object

PURPOSE:  Given an object identifier, find the "next" SNMP object using
	  SNMP's notion of lexicographic ordering of object identifiers.
	  The "next" object may be a leaf node of the MIB tree or an object
	  within a tabular structure.  In the latter case, located object
	  id will pass through the leaf node representing the table.

	  The returned object id, unless null, will be a valid id for
	  the get/set/test routines named in the leaf node for the object.

PARAMETERS:
	OBJ_ID_T *	Object ID to be used
	OBJ_ID_T *	Object ID structure to be loaded with the result.
	SNMP_PKT_T *	The received packet

RETURNS:  int		> 0 for success,
			0 if nothing found,
			-1 for internal failure.
			On success, the object identifier referenced by
			rslt_oidp will contain the resulting object id.
****************************************************************************/
#if !defined(NO_PP)
int
find_next_object(OBJ_ID_T * targetp,
		 OBJ_ID_T * rslt_oidp,
		 SNMP_PKT_T * pktp)
#else	/* NO_PP */
int
find_next_object(targetp, rslt_oidp, pktp)
	OBJ_ID_T	*targetp;
	OBJ_ID_T	*rslt_oidp;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
OIDC_T		result_list[MAX_OID_COUNT];
OBJ_ID_T	result;

result.num_components = 0;
result.component_list = result_list;

if ((result.num_components = find_next_helper(&mib_root_node, 0,
					      targetp->num_components,
					      targetp->component_list,
					      result_list, pktp)) == 0)
   then {
	rslt_oidp->num_components = 0;
	rslt_oidp->component_list = (OIDC_T *)0;
	return 0;
	}

if (clone_object_id(&result, rslt_oidp) == 0)
   then return rslt_oidp->num_components;
   else return -1;
}

#if !defined(NO_PP)
static
int
find_next_helper(MIBNODE_T *	np,
		OIDC_T		lastmatch,
		int		tcount,
		OIDC_T		*tlist,
		OIDC_T		*rlist,
		SNMP_PKT_T *	pktp)
#else	/* NO_PP */
static
int
find_next_helper(np, lastmatch, tcount, tlist, rlist, pktp)
	MIBNODE_T	*np;
	OIDC_T		lastmatch;
	int		tcount;
	OIDC_T		*tlist;
	OIDC_T		*rlist;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
if (np->node_type == INNER_NODE)
   then {
	struct MIBARC_S	*ap;
	int		ocount;
	if (tcount <= 0)
	   then {
		/* If we have no target object id at this level, then	*/
		/* we need to scan the available arcs, from the lowest	*/
		/* to the highest, to see if any of them can provide	*/
		/* a usable "next".					*/
		for(ap = np->arcs;
		    (MIBNODE_T *)(ap->nodep) != (MIBNODE_T *)0;
		    ap++)
		   {
		   if ((ocount = find_next_helper((MIBNODE_T *)(ap->nodep),
						  ap->id,
						  tcount, tlist,
						  rlist + 1, pktp)) > 0)
		      then {
			   *rlist = ap->id;
			   return ocount + 1;
			   }
		   }
		}
	   else { /* tcount > 0 */
		/* If we have a target object id at this level, then	*/
		/* we need to scan the available arcs.  We ignore any	*/
		/* which come "before" the target.  If one matches the	*/
		/* target, we need to check wither it has a "next" at	*/
		/* a lower level.  For those "after" the target, we need*/
		/* to check whether any of those can provide a usable	*/
		/* "next".						*/
		for(ap = np->arcs;
		   (MIBNODE_T *)(ap->nodep) != (MIBNODE_T *)0;
		   ap++)
		   {
		   if (ap->id < *tlist) then continue;

		   if (ap->id == *tlist)
		      then {
			   if ((ocount = find_next_helper(
						       (MIBNODE_T *)ap->nodep,
						       ap->id,
						       tcount - 1,
						       tlist + 1,
						       rlist + 1, pktp))
					  > 0)
			      then {
				   *rlist = ap->id;
				   return ocount + 1;
				   }
			   continue;
			   }

		   if (ap->id > *tlist)
		      then {
			   if ((ocount = find_next_helper(
						     (MIBNODE_T *)(ap->nodep),
						     ap->id,
						     0,
						     (OIDC_T *)0,
						     rlist + 1, pktp)) > 0)
			      then {
				   *rlist = ap->id;
				   return ocount + 1;
				   }
			   continue;
			   }
		   }
		}
	return 0;
	}

   else { /* node_type != INNER_NODE, i.e. it must be of type LEAF_NODE */
	/* Is this variable even within the given view of the MIB? */
	if ((((MIBLEAF_T *)np)->view_mask & pktp->mib_view) == 0)
	   then return 0;

	return (*(((MIBLEAF_T *)np)->nextproc))(lastmatch, tcount, tlist,
				rlist, ((MIBLEAF_T *)np)->user_cookie,
					pktp);
	}

/*NOTREACHED*/
}

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
int
std_next(OIDC_T		lastmatch,
	 int		tcount,
	 OIDC_T *	tlist,
	 OIDC_T *	rlist,
	 char *		cookie,
	 SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
int
std_next(lastmatch, tcount, tlist, rlist, cookie, pktp)
	OIDC_T		lastmatch;
	int		tcount;
	OIDC_T *	tlist;
	OIDC_T *	rlist;
	char *		cookie;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
if (tcount > 0)
   then return 0;
   else {
	*rlist = 0;
	return 1;
	}
}
/*lint +e715	*/
