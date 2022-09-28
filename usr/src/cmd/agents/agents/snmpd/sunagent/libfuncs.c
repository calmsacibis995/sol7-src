/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)libfuncs.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)libfuncs.c	2.15 96/07/23 Sun Microsystems";
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
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header:   D:/snmpv2/agent/sun/libfuncs.c_v   2.2   28 Aug 1990 11:26:28  $	*/
/*
 * $Log:   D:/snmpv2/agent/sun/libfuncs.c_v  $
 * 
 *    Rev 2.2   28 Aug 1990 11:26:28
 * Fixed the previous fix -- forgot to let the "set" c-string also
 * allow "get" type operations.
 * 
 *    Rev 2.1   27 Aug 1990 11:55:30
 * Fixed a problem in which a community string which was valid for GET
 * operations would also allow SET operations.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:12
 * Initial revision.
 * 
 *    Rev 1.6   27 Apr 1989 15:56:36
 * Removed unused variables
 * 
 *    Rev 1.5   17 Mar 1989 22:50:16
 * Memory allocation now fails if an attempt is made to allocate zero bytes.
 * 
 *    Rev 1.4   17 Mar 1989 21:41:38
 * Calls to memcpy/memset protected against zero lengths
 * 
 *    Rev 1.3   11 Jan 1989 13:24:32
 * 
 *    Rev 1.2   10 Oct 1988 21:49:36
 * Source reorganization
 * 
 *    Rev 1.1   14 Sep 1988 17:57:10
 * Moved includes of system include files into libfuncs.h.
 * 
 *    Rev 1.0   12 Sep 1988 10:47:00
 * Initial revision.
*/

#include <stdio.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#include <libfuncs.h>

#include <asn1.h>
#include <snmp.h>
#include "snmpvars.h"
#include "agent.h"
#include "general.h"

#define	then

#if !defined(NO_PP)
extern void input_pkt(SNMP_PKT_T *);	/****<<<<ADDED AT SUN>>>****/
extern void output_pkt(SNMP_PKT_T *);	/****<<<<ADDED AT SUN>>>****/
#else	/* NO_PP */
extern void input_pkt();		/****<<<<ADDED AT SUN>>>****/
extern void output_pkt();		/****<<<<ADDED AT SUN>>>****/
#endif	/* NO_PP */

/****************************************************************************
 This module contains user provided/modifiable procedures.
****************************************************************************/

/**********************************************************************
 Dynamic memory allocation interface -- the following routines interface
 with the dynamic memory allocation system.
 **********************************************************************/
char *
SNMP_mem_alloc(need)
	unsigned int	need;
{
char	*cp;

/* Don't allocate zero length blocks */
if (need == 0) then return (char *)0;

if ((cp = (char *)malloc(need)) == (char *)0)
   then {
#if defined(MEM_TRACE)
	PRNTF1("SNMP_mem_alloc for %u bytes failed\n", need);
#endif
	return (char *)0;
	}
   else {
#if defined(MEM_TRACE)
	PRNTF2("SNMP_mem_alloc for %u bytes suceeded at location %08.8X\n",
		need, cp);
#endif
	return cp;
	}
/*NOTREACHED*/
}

void
SNMP_mem_free(cp)
	char *cp;
{
if (cp != (char *)0)
   then {
#if defined(MEM_TRACE)
	PRNTF2("SNMP_mem_free releasing memory at %08.8X\n", cp);
#endif
	free(cp);
	}
}

/**********************************************************************
 release_private -- Get rid of a any private data attached to the packet
		    structure.

	 The parameters to this procedure are:
		 SNMP_PKT_T *	The packet itself
 **********************************************************************/
void
release_private(pktp)
	SNMP_PKT_T *	pktp;
{
if (pktp->private != (char *)0)
  then SNMP_mem_free(pktp->private);
}

/**********************************************************************
 validate_SNMP_community -- Check an operation against the community name.
	 Get class operations can use either the set or get community.
	 Set operations may use only the set community.
	 
	 The parameters to this procedure are:
		 SNMP_PKT_T *	The received packet (decoded format)
		 SNMPADDR_T *	Source of the packet
		 SNMPADDR_T *	Destination of the packet (most likely
				the address of the machine on which this
				code is running.)
		 LCL_FILE *	Input local stream.  This stream is positioned
				at the first byte of the after the community
				in the received packet.  In other words, the
				stream is positioned either at the PDU or the
				authentication data.

	 This procedure should return a pointer to a local i/o stream
	 containing the ASN.1 PDU.

	 This routine may hang additional data onto the "private" field of
	 the packet structure.
	 The user will be given the opportinity to release that memory via
	 release_private().

	 (LCL_FILE *)0 should be returned if the validation fails.
 **********************************************************************/
/*ARGSUSED*/
/*lint -e715	*/
LCL_FILE *
validate_SNMP_community(rp, pktsrc, pktdst, in_stream)
	SNMP_PKT_T	*rp;
	SNMPADDR_T * 	pktsrc;
	SNMPADDR_T * 	pktdst;
        LCL_FILE *	in_stream;
{
static UINT_16_T  lcl_ident_source = 0;
unsigned int	pktcom_length;
unsigned char	tag_byte;

tag_byte = (unsigned char) Lcl_Peekc(in_stream);
rp->mib_view = 0x01;
rp->lcl_ident = lcl_ident_source++;
rp->private = (char *)0;
(void)memcpy((char *)&(rp->pkt_src), (char *)pktsrc, sizeof(SNMPADDR_T));
(void)memcpy((char *)&(rp->pkt_dst), (char *)pktdst, sizeof(SNMPADDR_T));

if (mgr_cnt > 0)
   then {	/* Gotta see if the source is one of our approved managers */
        int i;
	for(i = 0; i < mgr_cnt; i++)
	   {
	   if (((struct sockaddr_in *)pktsrc)->sin_addr.S_un.S_addr ==
	       mgr_list[i])
	      then goto manager_OK; /* We can take this query */
	   }
	/* We don't seem to have this guy on our list */
	/* Just ignore this packet */
	TRC_PRT1(0, "Ignoring query from %s\n",
		 inet_ntoa(((struct sockaddr_in *)pktsrc)->sin_addr));
	return (LCL_FILE *)0;
        }

manager_OK:
pktcom_length = EBufferUsed(&(rp->community));

switch (tag_byte)
   {
   case GET_REQUEST_PDU | A_CONTEXT | A_CONSTRUCTOR:
   case GET_NEXT_REQUEST_PDU | A_CONTEXT | A_CONSTRUCTOR:
   	/* Check the full mib community... */
	if ((strlen(snmp_fullmib_read_community) == pktcom_length) &&
	    (memcmp(snmp_fullmib_read_community,
	            rp->community.start_bp, pktcom_length) == 0))
	   then {
	        rp->mib_view = 0x04;
	        return in_stream;
		}

	/* Check the system group community... */
	if ((strlen(snmp_sysgrp_read_community) == pktcom_length) &&
	    (memcmp(snmp_sysgrp_read_community,
	            rp->community.start_bp, pktcom_length) == 0))
	   then {
	        rp->mib_view = 0x01;
	        return in_stream;
		}

   /* FALL-THROUGH -- Let GET and GET NEXT	*/
   /* take advantage of the SET c-strings.	*/

   case SET_REQUEST_PDU | A_CONTEXT | A_CONSTRUCTOR:
	if ((strlen(snmp_fullmib_write_community) == pktcom_length) &&
	    (memcmp(snmp_fullmib_write_community, rp->community.start_bp,
		    pktcom_length) == 0))
	   then {
	        if ((read_only == 0) &&
		    tag_byte == (SET_REQUEST_PDU | A_CONTEXT | A_CONSTRUCTOR))
	           then rp->mib_view = 0x08;
	           else rp->mib_view = 0x04;
	        return in_stream;
		}

	if ((strlen(snmp_sysgrp_write_community) == pktcom_length) &&
	    (memcmp(snmp_sysgrp_write_community, rp->community.start_bp,
		    pktcom_length) == 0))
	   then {
	        if ((read_only == 0) &&
		    tag_byte == (SET_REQUEST_PDU | A_CONTEXT | A_CONSTRUCTOR))
	           then rp->mib_view = 0x02;
	           else rp->mib_view = 0x01;
	        return in_stream;
		}
	break;

   /* Ignore trap PDUs */
   case TRAP_PDU | A_CONTEXT | A_CONSTRUCTOR:
	TRC_PRT0(0, "TRAP PDU ignorred\n");
#if defined(SGRP)
	inc_counter(snmp_stats.snmpInTraps);
#endif
	return (LCL_FILE *)0;

   /* Ignore tings we don't understand */
   default:
	TRC_PRT0(0, "Unknown PDU type\n");
	return (LCL_FILE *)0;
   }

/* Things just didn't work out... */
TRC_PRT0(0, "Bad Community Name\n");
input_pkt(rp);
#if defined(SGRP)
inc_counter(snmp_stats.snmpInBadCommunityNames);
#endif

gen_auth_fail_trap();

return (LCL_FILE *)0;
}
/*lint -e715	*/

/**********************************************************************
 validate_set_pdu -- Perform a global validation of a SET PDU.
	 
	 The parameters to this procedure are:
		 SNMP_PKT_T *	The packet itself

 Returns:
	 -1 If the PDU is bad and should be rejected with a GEN_ERR.
	  0 If the PDU is good and normal handling should proceed.
	 +1 If the PDU is good and this routine has performed all of
	    the set operations internally.
 **********************************************************************/
/*ARGSUSED*/
int
validate_set_pdu(pktp)
	SNMP_PKT_T *	pktp;
{
return 0;	/* Let set PDUs be handled normally.	*/
}

/**********************************************************************
 user_pre_set -- Perform a global validation of a SET PDU after all of the
		 test procedures have been called and given the "go ahead".
	 
	 The parameters to this procedure are:
		 SNMP_PKT_T *	The packet itself

 Returns:
	 -1 If the PDU is bad and should be rejected with a GEN_ERR.
	  0 If the PDU is good and normal handling should proceed.
	 +1 If the PDU is good and this routine has performed all of
	    the set operations internally.
 **********************************************************************/
/*ARGSUSED*/
int
user_pre_set(pktp)
	SNMP_PKT_T *	pktp;
{
return 0;	/* Let set PDUs be handled normally.	*/
}

/**********************************************************************
 user_post_set -- Perform any final activities after all of the set
		  procedures have been called.

	 The parameters to this procedure are:
		 SNMP_PKT_T *	The packet itself

 Returns: Nothing
 **********************************************************************/
/*ARGSUSED*/
void
user_post_set(pktp)
	SNMP_PKT_T *	pktp;
{
return;
}

/**********************************************************************
 gen_auth_fail_trap -- Issue an authentication failure trap.
 **********************************************************************/
void
gen_auth_fail_trap()
{
if (snmp_auth_traps == 0) then return;

send_traps(snmp_socket, AUTH_FAILURE, 0);
}

/**********************************************************************
 user_auth_encode -- Generate an authenticated PDU
	 
	 The parameters to this procedure are:
		 SNMP_PKT_T *	The packet itself
		 EBUFFER_T *	A buffer structure in which the encoded
		 		PDU is to be placed.  The code in this
				function is responsible for allocating
				the necessary memory.  The buffer
				structure is, however, already initialized.

 Returns:
	 The length of the contents field in the encoded PDU with zero
	 indicating an error.  This length is *NOT* the number of bytes
	 placed into the buffer, rather, the length is only the ASN.1
	 contents and excludes the bytes used for the ASN.1 tag (typically
	 one byte) and the ASN.1 length field (typically one, two, or
	 three bytes.)

 Note:	 If a problem does occur and the returned length is zero. This
 	 function may release any memory attached to the buffer structure.
	 If so, this function should use EBufferClean to release the memory.
	 The higher level code which calls this function will release any
	 such memory if the user fails to do so.

 **********************************************************************/
/* Returns the number of bytes of PDU data, excluding the PDU tag and	*/
/* length.								*/
/* Returns 0 on error.							*/
#if !defined(NO_PP)
ALENGTH_T
user_auth_encode(SNMP_PKT_T *	rp,
		 EBUFFER_T *	apdup)
#else	/* NO_PP */
ALENGTH_T
user_auth_encode(rp, apdup)
		 SNMP_PKT_T *	rp;
		 EBUFFER_T *	apdup;
#endif	/* NO_PP */
{
unsigned int need;
OCTET_T *buffp;

need = SNMP_Bufsize_For_Packet(rp);

/* Obtain space for the pdu */
if ((buffp = (OCTET_T *)SNMP_mem_alloc(need)) == (OCTET_T *)0)
   then {
	return 0;
	}
EBufferSetup(BFL_IS_DYNAMIC, apdup, buffp, need);
SNMP_Encode_PDU(rp, apdup);
return rp->pdu_length;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_snmpEnableAuthTraps(unsigned int lastmatch, int compc,
			unsigned int *compl, char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
get_snmpEnableAuthTraps(lastmatch, compc, compl, cookie)
	unsigned int lastmatch;
	int compc;
	unsigned int *compl;
	char *cookie;
#endif  /* NO_PP */
{
return (INT_32_T)(snmp_auth_traps ? 1 : 2);
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_snmpEnableAuthTraps(unsigned int lastmatch, int compc,
			unsigned int *compl,
			char *cookie, INT_32_T value)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_snmpEnableAuthTraps(lastmatch, compc, compl, cookie, value)
	unsigned int lastmatch;
	int compc;
	unsigned int *compl;
	char *cookie;
	INT_32_T value;
#endif  /* NO_PP */
{
if (value == 1) /* 1 is for enable */
   then snmp_auth_traps = 1;
   else {
        if (value == 2) /* 2 is for disable */
           then snmp_auth_traps = 0;
        }
}

int
libfuncs_init()
{
return 0;
}

/****************************************************************************
Following are various utility routines which are useful when obtaining
values from memory.
****************************************************************************/
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
int
it_exists(int		form,		/* TEST_SET & TEST_GET from mib.h */
	  OIDC_T	lastmatch,	/* Last component matched */
	  int		compc,
	  OIDC_T *	compl,
	  char *	cookie,
	  SNMP_PKT_T *	pktp,
	  int		index)		/* Index (zero based) of VB_T	*/
#else	/* NO_PP */
/*ARGSUSED*/
int
it_exists(form, lastmatch, compc, compl, cookie, pktp, index)
	int		form;		/* TEST_SET & TEST_GET from mib.h */
	OIDC_T		lastmatch;	/* Last component matched */
	int		compc;
	OIDC_T *	compl;
	char *		cookie;
	SNMP_PKT_T *	pktp;
	int		index;		/* Index (zero based) of VB_T	*/
#endif	/* NO_PP */
{
/* For non-tabular parameters, the "object instance", i.e. the unused	*/
/* portion of the object identifier, must be a single component of value*/
/* zero.								*/
if ((compc == 1) && (*compl == 0))
   then return 0;
   else return -1;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_int(OIDC_T		lastmatch,	/* Last component matched */
	int		compc,
	OIDC_T *	compl,
	char *		cookie,
	SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
get_int(lastmatch, compc, compl, cookie, pktp)
	OIDC_T		lastmatch;	/* Last component matched */
	int		compc;
	OIDC_T *	compl;
	char *		cookie;
	SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (INT_32_T)*((int *)cookie);
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
get_uint(OIDC_T		lastmatch,	/* Last component matched */
	 int		compc,
	 OIDC_T *	compl,
	 char *		cookie,
	 SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
UINT_32_T
get_uint(lastmatch, compc, compl, cookie, pktp)
	 OIDC_T		lastmatch;	/* Last component matched */
	 int		compc;
	 OIDC_T *	compl;
	 char *		cookie;
	 SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (UINT_32_T)*((unsigned int *)cookie);
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_long(OIDC_T		lastmatch,	/* Last component matched */
	 int		compc,
	 OIDC_T *	compl,
	 char *		cookie,
	 SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
get_long(lastmatch, compc, compl, cookie, pktp)
	 OIDC_T		lastmatch;	/* Last component matched */
	 int		compc;
	 OIDC_T *	compl;
	 char *		cookie;
	 SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (INT_32_T)*((long int *)cookie);
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
UINT_32_T
get_ulong(OIDC_T	lastmatch,	/* Last component matched */
	  int		compc,
	  OIDC_T *	compl,
	  char *	cookie,
	  SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
UINT_32_T
get_ulong(lastmatch, compc, compl, cookie, pktp)
	  OIDC_T	lastmatch;	/* Last component matched */
	  int		compc;
	  OIDC_T *	compl;
	  char *	cookie;
	  SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (UINT_32_T)*((unsigned long int *)cookie);
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_string(OIDC_T	lastmatch,
	   int		compc,
	   OIDC_T *	compl,
	   char *	cookie,
	   int *	lengthp,
	   SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
unsigned char *
get_string(lastmatch, compc, compl, cookie, lengthp, pktp)
	   OIDC_T	lastmatch;
	   int		compc;
	   OIDC_T *	compl;
	   char *	cookie;
	   int *	lengthp;
	   SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
/* The address returned *MUST* reference a static location */
*lengthp = (int)strlen(cookie);
return (unsigned char *)cookie;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_ip_address(OIDC_T		lastmatch,
	       int		compc,
	       OIDC_T *		compl,
	       char *		cookie,
	       SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
unsigned char *
get_ip_address(lastmatch, compc, compl, cookie, pktp)
	       OIDC_T		lastmatch;
	       int		compc;
	       OIDC_T *		compl;
	       char *		cookie;
	       SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (unsigned char *)cookie;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
OBJ_ID_T *
get_object_identifier(OIDC_T		lastmatch,
		      int		compc,
		      OIDC_T *		compl,
		      char *		cookie,
		      SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
OBJ_ID_T *
get_object_identifier(lastmatch, compc, compl, cookie, pktp)
		      OIDC_T		lastmatch;
		      int		compc;
		      OIDC_T *		compl;
		      char *		cookie;
		      SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (OBJ_ID_T *)cookie;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_cookie(OIDC_T	lastmatch,
	   int		compc,
	   OIDC_T *	compl,
	   char *	cookie,
	   SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
get_cookie(lastmatch, compc, compl, cookie, pktp)
	   OIDC_T	lastmatch;
	   int		compc;
	   OIDC_T *	compl;
	   char *	cookie;
	   SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return (INT_32_T)cookie;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
null_get_proc(OIDC_T		lastmatch,
	      int		compc,
	      OIDC_T *		compl,
	      char *		cookie,
	      SNMP_PKT_T *	pktp)
#else	/* NO_PP */
/*ARGSUSED*/
INT_32_T
null_get_proc(lastmatch, compc, compl, cookie, pktp)
	      OIDC_T		lastmatch;
	      int		compc;
	      OIDC_T *		compl;
	      char *		cookie;
	      SNMP_PKT_T *	pktp;
#endif	/* NO_PP */
{
return 0;
}
/*lint +e715	*/

/****************************************************************************
Following are various utility routines which are useful when setting values
located in memory.
****************************************************************************/
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
void
set_int(OIDC_T		lastmatch,
	int		compc,
	OIDC_T		*compl,
	char		*cookie,
	INT_32_T	value,
	SNMP_PKT_T *	pktp,
	int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_int(lastmatch, compc, compl, cookie, value, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char		*cookie;
	INT_32_T	value;
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
*(int *)cookie = (int)value;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
void
set_uint(OIDC_T		lastmatch,
	int		compc,
	OIDC_T		*compl,
	char 		*cookie,
	UINT_32_T	value,
	SNMP_PKT_T *	pktp,
	int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_uint(lastmatch, compc, compl, cookie, value, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char 		*cookie;
	UINT_32_T	value;
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
*(unsigned int *)cookie = (unsigned int)value;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
void
set_long(OIDC_T		lastmatch,
	int		compc,
	OIDC_T		*compl,
	char 		*cookie,
	INT_32_T	value,
	SNMP_PKT_T *	pktp,
	int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_long(lastmatch, compc, compl, cookie, value, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char 		*cookie;
	INT_32_T	value;
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
*(long int *)cookie = (long int)value;
}
/*lint +e715	*/

/*lint -e715	*/
#if !defined(NO_PP)
void
set_ulong(OIDC_T	lastmatch,
	int		compc,
	OIDC_T		*compl,
	char 		*cookie,
	UINT_32_T	value,
	SNMP_PKT_T *	pktp,
	int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_ulong(lastmatch, compc, compl, cookie, value, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char 		*cookie;
	UINT_32_T	value;
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
*(unsigned long int *)cookie = (unsigned long int)value;
}
/*lint +e715	*/

/**********************************************************************/
/**********************************************************************/
/********* WARNING: THE FOLLOWING ROUTINE DOES NOT CHECK FOR   ********/
/********* STRINGS WHICH ARE OVERSIZED.  TO BE SAFE, A SPECIAL ********/
/********* ROUTINE SHOULD BE WRITTEN FOR EACH SETTABLE STRING, ********/
/********* AND THAT ROUTINE SHOULD LIMIT THE COPY AS NEEDED.   ********/
/**********************************************************************/
/**********************************************************************/
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
void
set_string(OIDC_T	lastmatch,
	int		compc,
	OIDC_T		*compl,
	char		*cookie,
	char		*cp,
	int		length,
	SNMP_PKT_T *	pktp,
	int		index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_string(lastmatch, compc, compl, cookie, cp, length, pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char		*cookie;
	char		*cp;
	int		length;
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
if (length != 0) then (void) memcpy(cookie, cp, (unsigned int)length);
}
/*lint +e715	*/

#if defined(GRUNGUS)
/*lint -e715	*/
#if !defined(NO_PP)
/*ARGSUSED*/
void
set_object_identifier(OIDC_T	  lastmatch,
		      int	  compc,
		      OIDC_T *	  compl,
		      char *	  cookie,
		      int	  numc,		/* Number of components */
		      OIDC_T *	  complist,	/* Component list	*/
		      SNMP_PKT_T * pktp,
		      int	  index)
#else	/* NO_PP */
/*ARGSUSED*/
void
set_object_identifier(lastmatch, compc, compl, cookie, numc, complist,
		      pktp, index)
	OIDC_T		lastmatch;
	int		compc;
	OIDC_T		*compl;
	char		*cookie;
	int		numc;		/* Number of components */
	OIDC_T		*complist;	/* Component list	*/
	SNMP_PKT_T *	pktp;
	int		index;
#endif	/* NO_PP */
{
}
/*lint +e715	*/
#endif	/* GRUNGUS */

void
null_set_proc()
{
}


static char line[] = "----------------------------------------\n";
#if !defined(NO_PP)
void
input_pkt(SNMP_PKT_T * pktp)
#else	/* NO_PP */
void
input_pkt(pktp)
SNMP_PKT_T * pktp;
#endif	/* NO_PP */
{
if (trace_level > 2)
   then {
	printf(line);
   	printf("Input packet:\n");
	print_pkt(pktp);
	printf(line);
	fflush(stdout);
	}
}

#if !defined(NO_PP)
void
output_pkt(SNMP_PKT_T * pktp)
#else	/* NO_PP */
void
output_pkt(pktp)
SNMP_PKT_T * pktp;
#endif	/* NO_PP */
{
if (trace_level > 2)
   then {
	printf(line);
	printf("Result packet:\n");
	print_pkt(pktp);
	printf(line);
	fflush(stdout);
	}
}
