/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)print.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)print.c	2.15 96/07/23 Sun Microsystems";
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

/* $Header:   E:/SNMPV2/SNMP/PRINT.C_V   2.0   31 Mar 1990 15:06:52  $	*/
/*
 * $Log:   E:/SNMPV2/SNMP/PRINT.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:06:52
 * Release 2.00
 * 
 *    Rev 1.4   23 Aug 1989 23:42:00
 * Corrected printf format strings to match data types in VB list.
 * 
 *    Rev 1.3   27 Apr 1989 15:56:04
 * Removed unused variables
 * 
 *    Rev 1.2   19 Sep 1988 17:26:40
 * Made changes to make the Sun C compiler happy.
 * 
 *    Rev 1.1   14 Sep 1988 17:57:18
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:04
 * Initial revision.
*/

#include <stdio.h>
#include <ctype.h>
#include <libfuncs.h>

#include <asn1.h>
#include <localio.h>
#include <buffer.h>
#include <decode.h>
#include <snmp.h>
#include <print.h>
#include <objectid.h>

#define	then

/****************************************************************************
This module contains various utility routines that print things
****************************************************************************/

#if !defined(NO_PP)
void
print_obj_id_list(
	int	obj_cnt,
	OIDC_T	*objp)
#else	/* NO_PP */
void
print_obj_id_list(obj_cnt, objp)
	int	obj_cnt;
	OIDC_T	*objp;
#endif	/* NO_PP */
{
(void)printf("OBJ_ID: ");
#if defined(OIDC_32)
for(; obj_cnt > 1; obj_cnt--) (void)printf("%lu.", *objp++);
(void)printf("%lu", *objp);
#else	/* OIDC_32 */
for(; obj_cnt > 1; obj_cnt--) (void)printf("%u.", *objp++);
(void)printf("%u", *objp);
#endif	/* OIDC_32 */
(void)printf("\n");
}

#if !defined(NO_PP)
void
print_obj_id(OBJ_ID_T * objp)
#else	/* NO_PP */
void
print_obj_id(objp)
	OBJ_ID_T	*objp;
#endif	/* NO_PP */
{
print_obj_id_list(objp->num_components, objp->component_list);
}

#if !defined(NO_PP)
void
print_ebuffer(EBUFFER_T	* ebp)
#else	/* NO_PP */
void
print_ebuffer(ebp)
	EBUFFER_T	*ebp;
#endif	/* NO_PP */
{
char *cp;
for (cp = (char *)(ebp->start_bp); cp != (char *)(ebp->next_bp); cp++)
   {
   if (isprint(*cp))
      then (void)putchar(*cp);
      else printf("\\%02.2X", (unsigned char)*cp);
   }
(void)putchar('\n');
}

#if !defined(NO_PP)
void
print_ipaddress(unsigned char * ipa)
#else	/* NO_PP */
void
print_ipaddress(ipa)
	unsigned char *ipa;
#endif	/* NO_PP */
{
(void)printf("IP Address: %d.%d.%d.%d\n", ipa[0], ipa[1], ipa[2], ipa[3]);
}

#if !defined(NO_PP)
void
print_vblist(VBL_T * vblp)
#else	/* NO_PP */
void
print_vblist(vblp)
	VBL_T	*vblp;
#endif	/* NO_PP */
{
VB_T	*vbp;

(void)printf("VBLlen=%d, ", vblp->vbl_length);
(void)printf("VBcount=%d\n", vblp->vbl_count);

if ((vbp = vblp->vblist) != (VB_T *)0)
   then {
	int i;
	(void)printf("VarBindList:\n");
	for (i = 0; i < vblp->vbl_count; i++, vbp++)
	   {
	   (void)printf("VarBind #%d, content size=%d, ",
		   i, vbp->vb_seq_size);
	   (void)printf("Flags & type=0x%02.2X, encoded length=%d\n",
		   vbp->vb_data_flags_n_type, vbp->vb_data_length);
	   print_obj_id(&(vbp->vb_obj_id));
	   switch (vbp->vb_data_flags_n_type)
	      {
	      case VT_NUMBER:
		   (void)printf("Number: %ld\n",
				  (long)(vbp->value_u.v_number));
	           break;
	      case VT_COUNTER:
		   (void)printf("Counter: %lu\n",
				  (unsigned long)(vbp->value_u.v_counter));
	           break;
	      case VT_GAUGE:
		   (void)printf("Gauge: %lu\n",
				  (unsigned long)(vbp->value_u.v_gauge));
	           break;
	      case VT_TIMETICKS:
		   (void)printf("Time Ticks: %lu\n",
			      (unsigned long)(vbp->value_u.v_timeticks));
		   break;
	      case VT_STRING:
		   (void)printf("String: Length=%d\n",
			  EBufferUsed(&(vbp->value_u.v_string)));
	           print_ebuffer(&(vbp->value_u.v_string));
		   break;
	      case VT_OPAQUE:
		   (void)printf("Opaque: Length=%d\n",
			  EBufferUsed(&(vbp->value_u.v_string)));
	           print_ebuffer(&(vbp->value_u.v_opaque));
		   break;
	      case VT_OBJECT:
		   (void)printf("Object ID: ");
		   print_obj_id(&(vbp->value_u.v_object));
		   break;
	      case VT_EMPTY:
		   (void)printf("Empty\n");
		   break;
	      case VT_IPADDRESS:
		   print_ipaddress(vbp->value_u.v_network_address);
		   break;
	      default:
		   (void)printf("UNKNOWN\n");
		   break;
	      }
#ifdef GRUNGUS
      	     {
	     OBJ_ID_T	nxt_objid;
	     nxt_objid.num_components = 0;
	     nxt_objid.component_list = (unsigned int *)0;
	     (void)printf("Finding next...");
	     if (find_next_object(&(vbp->vb_obj_id), &nxt_objid) <= 0)
	        then (void)printf("Find next failed\n");
	        else print_obj_id(&nxt_objid);
	     }
#endif
	   }
       }
}

#if !defined(NO_PP)
void
print_pkt(SNMP_PKT_T * rp)
#else	/* NO_PP */
void
print_pkt(rp)
	SNMP_PKT_T	*rp;
#endif	/* NO_PP */
{
(void)printf("Packet Length=%d, Version=%ld, ",
	rp->overall_length, rp->snmp_version);

(void)printf("Community: Length=%d, Value=", EBufferUsed(&(rp->community)));
print_ebuffer(&(rp->community));

(void)printf("PDU Type=0x%02.2X Length=%d, ",
	rp->pdu_type, rp->pdu_length);

if (rp->pdu_type != TRAP_PDU)
   then { /* Its a non-trap form of pdu */
	(void)printf("RQ_ID=%ld, Err_stat=%ld, Err_ndx=%ld\n",
		(INT_32_T)rp->pdu.std_pdu.request_id,
		(INT_32_T)rp->pdu.std_pdu.error_status,
		(INT_32_T)rp->pdu.std_pdu.error_index);

	print_vblist(&(rp->pdu.std_pdu.std_vbl));
	}
   else { /* It's a trap pdu */
	(void)printf("Enterprise id: \n");
	print_obj_id(&(rp->pdu.trap_pdu.enterprise_objid));
	print_ipaddress(rp->pdu.trap_pdu.net_address);
	(void)printf("Generic trap=%ld (%lX), specific trap=%ld (%lX), ",
	       rp->pdu.trap_pdu.generic_trap,
	       rp->pdu.trap_pdu.generic_trap,
	       rp->pdu.trap_pdu.specific_trap,
	       rp->pdu.trap_pdu.specific_trap);
        (void)printf("Trap time=%ld (%lX)\n",
	       rp->pdu.trap_pdu.trap_time_ticks,
	       rp->pdu.trap_pdu.trap_time_ticks);
	print_vblist(&(rp->pdu.trap_pdu.trap_vbl));
	}
}
