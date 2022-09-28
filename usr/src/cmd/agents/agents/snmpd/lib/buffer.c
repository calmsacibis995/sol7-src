/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)buffer.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)buffer.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/BUFFER.C_V   2.0   31 Mar 1990 15:06:40  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/BUFFER.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:40
 * Release 2.00
 * 
 *    Rev 1.4   27 Apr 1989 15:56:30
 * Removed unused variables
 * 
 *    Rev 1.3   17 Mar 1989 23:22:00
 * An attempt to clone a zero length buffer now results in a static buffer
 * with a length of zero at address zero.
 * 
 *    Rev 1.2   17 Mar 1989 21:41:42
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.1   14 Sep 1988 17:57:18
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:52
 * Initial revision.
*/

#include <libfuncs.h>

#include <buffer.h>

#define	then

/****************************************************************************
NAME:  EBufferClone

PURPOSE:  Clone an extended buffer

PARAMETERS:
	EBUFFER_T *	Source buffer structure
	EBUFFER_T *	Destination buffer structure

RETURNS:  0 if sucessful, -1 if not
****************************************************************************/
#if !defined(NO_PP)
int
EBufferClone(EBUFFER_T * srcp,
	     EBUFFER_T * dstp)
#else	/* NO_PP */
int
EBufferClone(srcp, dstp)
	EBUFFER_T	*srcp, *dstp;
#endif	/* NO_PP */
{
ALENGTH_T	need;

need = EBufferUsed(srcp);

if (need != 0)
   then {
	OCTET_T	*newbuffp;
	if ((newbuffp = (OCTET_T *)SNMP_mem_alloc(need)) == (OCTET_T *)0)
	   then { /* Allocation failed */
		return -1;
		}

	(void) memcpy(newbuffp, srcp->start_bp, need);
	EBufferPreLoad(BFL_IS_DYNAMIC, dstp, newbuffp, need);
	}
   else { /* Length is zero */
	EBufferPreLoad(BFL_IS_STATIC, dstp, (OCTET_T *)0, 0);
	}
return 0;
}

/****************************************************************************

NAME:  EBufferClean

PURPOSE:  Release the buffer memory if possible

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#if !defined(NO_PP)
void
EBufferClean(EBUFFER_T * ebuffp)
#else	/* NO_PP */
void
EBufferClean(ebuffp)
	EBUFFER_T	*ebuffp;
#endif	/* NO_PP */
{
if ((ebuffp->bflags & BFL_IS_DYNAMIC) && (ebuffp->start_bp != (OCTET_T *)0))
   then SNMP_mem_free((char *)(ebuffp->start_bp));

EBufferInitialize(ebuffp);
}

/****************************************************************************

NAME:  EBufferAppend

PURPOSE:  Append the contents of one buffer onto another.

PARAMETERS:
	    EBUFFER_T *	    First (and destination) buffer descriptor
	    EBUFFER_T *	    Second buffer descriptor

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#if !defined(NO_PP)
void
EBufferAppend(EBUFFER_T * b1p,
	      EBUFFER_T * b2p)
#else	/* NO_PP */
void
EBufferAppend(b1p, b2p)
	EBUFFER_T *	b1p;
	EBUFFER_T *	b2p;
#endif	/* NO_PP */
{
ALENGTH_T used;

used = min(EBufferUsed(b2p), b1p->remaining);

(void)memcpy(b1p->next_bp, b2p->start_bp, used);
b1p->next_bp += used;
b1p->remaining -= used;
}
