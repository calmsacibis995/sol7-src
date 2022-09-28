/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)etc-encode.h	2.16 96/07/25 Sun Microsystems"
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
 *     Copyright (c) 1986, 1988  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   E:/SNMPV2/H/ENCODE.H_V   2.0   31 Mar 1990 15:11:24  $	*/
/*
 * $Log:   E:/SNMPV2/H/ENCODE.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:24
 * Release 2.00
 * 
 *    Rev 1.1   19 Sep 1988 17:27:06
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:16
 * Initial revision.
*/

#if (!defined(encode_inc))
#define encode_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if (!defined(buffer_inc))
#include <buffer.h>
#endif

#if !defined(NO_PP)
extern	ALENGTH_T	A_SizeOfInt(INT_32_T);
extern	ALENGTH_T	A_SizeOfUnsignedInt(UINT_32_T);
extern	ALENGTH_T	A_SizeOfObjectId(OBJ_ID_T *);
extern	void		A_EncodeType(ATVALUE_T, OCTET_T,
				     ALENGTH_T (*)(), OCTET_T *);
extern	void		A_EncodeLength(ALENGTH_T,
				     ALENGTH_T (*)(), OCTET_T *);
extern	void		A_EncodeInt(ATVALUE_T, OCTET_T, INT_32_T,
				     ALENGTH_T (*)(), OCTET_T *);
extern	void		A_EncodeUnsignedInt(ATVALUE_T, OCTET_T, UINT_32_T,
					    ALENGTH_T (*)(), OCTET_T *);
extern	void		A_EncodeOctetString(ATVALUE_T, OCTET_T,
				     OCTET_T *, ALENGTH_T,
				     ALENGTH_T (*)(), OCTET_T *);
extern	void		A_EncodeObjectId(ATVALUE_T, OCTET_T,
					 OBJ_ID_T *,
					 ALENGTH_T (*)(), OCTET_T *);
extern	ALENGTH_T	A_EncodeHelper(EBUFFER_T *, OCTET_T *, ALENGTH_T);
#else	/* NO_PP */
extern	ALENGTH_T	A_SizeOfInt();
extern	ALENGTH_T	A_SizeOfUnsignedInt();
extern	ALENGTH_T	A_SizeOfObjectId();
extern	void		A_EncodeType();
extern	void		A_EncodeLength();
extern	void		A_EncodeInt();
extern	void		A_EncodeUnsignedInt();
extern	void		A_EncodeOctetString();
extern	void		A_EncodeObjectId();
extern	ALENGTH_T	A_EncodeHelper();
#endif	/* NO_PP */

#endif	/* encode_inc */
