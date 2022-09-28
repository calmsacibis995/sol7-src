/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)buffer.h	2.16 96/07/25 Sun Microsystems"
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

/* $Header:   E:/SNMPV2/H/BUFFER.H_V   2.0   31 Mar 1990 15:11:22  $	*/
/*
 * $Log:   E:/SNMPV2/H/BUFFER.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:22
 * Release 2.00
 * 
 *    Rev 1.1   19 Sep 1988 17:27:04
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:12
 * Initial revision.
*/

#if (!defined(buffer_inc))
#define buffer_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

/* The EBUFFER_T structure is used to control encoding into a buffer	*/
typedef struct EBUFFER_S
	{
	unsigned short int	bflags;
	OCTET_T     *start_bp;	/* Start of the buffer.			    */
	OCTET_T     *next_bp;   /* Next location in buffer to be filled     */
	ALENGTH_T    remaining; /* Number of empty spots remaining in buffer*/
	} EBUFFER_T;

/* Values for bflags	*/
#define	BFL_IS_DYNAMIC		0x0001	/* Buffer was obtained by malloc    */
#define	BFL_IS_STATIC		0x0000	/* Buffer is statically allocated   */

/****************************************************************************

NAME:  EBufferInitialize

PURPOSE:  Initialize a buffer to a know, but not usable state
	  EBufferSetup must be used to make the buffer ready for use.

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferInitialize(E) (((EBUFFER_T *)(E))->bflags=0,   \
			      ((EBUFFER_T *)(E))->next_bp=(OCTET_T *)0,  \
			      ((EBUFFER_T *)(E))->start_bp=(OCTET_T *)0,  \
			      ((EBUFFER_T *)(E))->remaining=(ALENGTH_T)0)

/****************************************************************************

NAME:  EBufferSetup

PURPOSE:  Setup a buffer to receive ASN.1 encoded data

PARAMETERS:
	    unsigned int    BFL_xxx flags from buffer.h
	    EBUFFER_T *	    Buffer descriptor
            OCTET_T *       Address of the buffer
            ALENGTH_T       Length of the buffer

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferSetup(F,E,B,L) (((EBUFFER_T *)(E))->bflags=(F),  \
			       ((EBUFFER_T *)(E))->start_bp=(OCTET_T *)(B),  \
			       ((EBUFFER_T *)(E))->next_bp=(OCTET_T *)(B),  \
			       ((EBUFFER_T *)(E))->remaining=(ALENGTH_T)(L))

/****************************************************************************

NAME:  EBufferPreLoad

PURPOSE:  Pre-load data into a buffer

PARAMETERS:
	    unsigned int    BFL_xxx flags from buffer.h
	    EBUFFER_T *	    Buffer descriptor
            OCTET_T *       Address of the buffer containing the data
            ALENGTH_T       Length of the data

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferPreLoad(F,E,B,L) (	\
	((EBUFFER_T *)(E))->bflags=(F),	\
	((EBUFFER_T *)(E))->start_bp=(OCTET_T *)(B),	\
	((EBUFFER_T *)(E))->next_bp=((OCTET_T *)(B)+(ALENGTH_T)(L)),	\
	((EBUFFER_T *)(E))->remaining=(ALENGTH_T)0)

/****************************************************************************

NAME:  EBufferNext

PURPOSE:  Obtain address of next free byte in a buffer

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    OCTET_T *	    Buffer descriptor

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferNext(E)	(((EBUFFER_T *)(E))->next_bp)

/****************************************************************************

NAME:  EBufferUsed

PURPOSE:  Indicate how many octets are currently in the buffer.

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    ALENGTH_T	    Number of octets used.

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferUsed(E)	((ALENGTH_T)((((EBUFFER_T *)(E))->next_bp) -	\
				     (((EBUFFER_T *)(E))->start_bp)))

/****************************************************************************

NAME:  EBufferRemaining

PURPOSE:  Indicate how many octets are currently unused in the buffer.

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    ALENGTH_T	    Number of octets unused.

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferRemaining(E)	(((EBUFFER_T *)(E))->remaining)

/****************************************************************************

NAME:  EBufferReset

PURPOSE:  Reset the buffer to an empty state, just like after EBufferSetup

PARAMETERS:
	    EBUFFER_T *	    Buffer descriptor

RETURNS:    Nothing

RESTRICTIONS:  

BUGS:  
****************************************************************************/
#define EBufferReset(E)	(((EBUFFER_T *)(E))->next_bp =	\
				 ((EBUFFER_T *)(E))->start_bp)

#if !defined(NO_PP)
extern	int	EBufferClone(EBUFFER_T *, EBUFFER_T *);
extern	void	EBufferClean(EBUFFER_T *);
extern	void	EBufferAppend(EBUFFER_T *, EBUFFER_T *);
#else	/* NO_PP */
extern	int	EBufferClone();
extern	void	EBufferClean();
extern	void	EBufferAppend();
#endif	/* NO_PP */

#endif	/* buffer_inc */
