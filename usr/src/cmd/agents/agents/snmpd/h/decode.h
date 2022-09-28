/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)decode.h	2.16 96/07/25 Sun Microsystems"
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
 *     Copyright (c) 1986, 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/H/DECODE.H_V   2.0   31 Mar 1990 15:11:26  $	*/
/*
 * $Log:   E:/SNMPV2/H/DECODE.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:26
 * Release 2.00
 * 
 *    Rev 1.3   23 Mar 1989 11:55:58
 * Merged the decode helper into the one routine that used it.
 * 
 *    Rev 1.2   18 Mar 1989 11:57:26
 * Revised prototypes for A_DecodeOctetStringData and A_DecodeOctetString
 * 
 *    Rev 1.1   19 Sep 1988 17:27:04
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:14
 * Initial revision.
*/

#if (!defined(decode_inc))
#define decode_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if (!defined(localio_inc))
#include <localio.h>
#endif

#if (!defined(buffer_inc))
#include <buffer.h>
#endif

/****************************************************************************
NAME:  A_DecodeTypeClass

PURPOSE:  Decode the Class and Form bits from an ASN.1 type field.
	  The data stream is read using the local I/O package.
	  The stream pointer is NOT advanced, leaving it pointing to the
	  start of the type field, ready for a subsequent call to
	  A_DecodeTypeValue.

CAVEAT:	  The user should call this return *BEFORE* calling
	  A_DecodeTypeValue.

PARAMETERS:  LCL_FILE *	    A stream descriptor (already open)

RETURNS:  OCTET_T	    The Class and Form bits

RESTRICTIONS:  It is assumed that the stream is not at EOF.
****************************************************************************/

#define A_DecodeTypeClass(L) (((OCTET_T) Lcl_Peekc((LCL_FILE *)(L))) &	\
			      A_IDCF_MASK)

/****************************************************************************
Decoding errors
****************************************************************************/
#define	AE_PREMATURE_END		1
#define	AE_INDEFINITE_LENGTH		2

#if !defined(NO_PP)
extern	ATVALUE_T	A_DecodeTypeValue(LCL_FILE *, int *);
extern	ALENGTH_T	A_DecodeLength(LCL_FILE *, int *);
extern	void		A_DecodeOctetStringData(LCL_FILE *, ALENGTH_T,
						EBUFFER_T *, int *);
extern	void		A_DecodeOctetString(LCL_FILE *, EBUFFER_T *, int *);
extern	INT_32_T	A_DecodeIntegerData(LCL_FILE *, ALENGTH_T, int *);
extern	INT_32_T	A_DecodeInteger(LCL_FILE *, int *);
extern	void		A_DecodeObjectIdData(LCL_FILE *, ALENGTH_T,
					     OBJ_ID_T *, int *);
extern	void		A_DecodeObjectId(LCL_FILE *, OBJ_ID_T *, int *);
#else	/* NO_PP */
extern	ATVALUE_T	A_DecodeTypeValue();
extern	ALENGTH_T	A_DecodeLength();
extern	void		A_DecodeOctetStringData();
extern	void		A_DecodeOctetString();
extern	INT_32_T	A_DecodeIntegerData();
extern	INT_32_T	A_DecodeInteger();
extern	void		A_DecodeObjectIdData();
extern	void		A_DecodeObjectId();
#endif	/* NO_PP */

#endif	/* decode_inc */
