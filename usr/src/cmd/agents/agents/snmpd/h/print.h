/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)print.h	2.16 96/07/25 Sun Microsystems"
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

/* $Header:   E:/SNMPV2/H/PRINT.H_V   2.0   31 Mar 1990 15:11:22  $	*/
/*
 * $Log:   E:/SNMPV2/H/PRINT.H_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:11:22
 * Release 2.00
 * 
 *    Rev 1.1   19 Sep 1988 17:27:08
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.0   12 Sep 1988 10:46:20
 * Initial revision.
*/

#if (!defined(utils_inc))
#define utils_inc

#if (!defined(asn1_inc))
#include <asn1.h>
#endif

#if (!defined(snmp_inc))
#include <snmp.h>
#endif

#if (!defined(localio_inc))
#include <localio.h>
#endif

#if (!defined(buffer_inc))
#include <buffer.h>
#endif

#if !defined(NO_PP)
extern	void		print_obj_id_list(int, OIDC_T *);
extern	void		print_obj_id(OBJ_ID_T *);
extern	void		print_ebuffer(EBUFFER_T *);
extern	void		print_ipaddress(unsigned char *);
extern	void		print_pkt(SNMP_PKT_T *);
#else	/* NO_PP */
extern	void		print_obj_id_list();
extern	void		print_obj_id();
extern	void		print_ebuffer();
extern	void		print_ipaddress();
extern	void		print_pkt();
#endif	/* NO_PP */

#endif	/* utils_inc */
