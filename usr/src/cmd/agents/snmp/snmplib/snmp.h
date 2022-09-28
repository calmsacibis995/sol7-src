/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)snmp.h	1.3 96/07/01 Sun Microsystems"

#ifndef _SNMP_H_
#define _SNMP_H_

#include "asn1.h"


/***** GLOBAL CONSTANTS *****/

#define SNMP_PORT	    161
#define SNMP_TRAP_PORT	    162

#define SNMP_MAX_LEN	    484

#define SNMP_VERSION_1	    0

#define GET_REQ_MSG	    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x0)
#define GETNEXT_REQ_MSG	    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x1)
#define GET_RSP_MSG	    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x2)
#define SET_REQ_MSG	    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x3)
#define TRP_REQ_MSG	    (ASN_CONTEXT | ASN_CONSTRUCTOR | 0x4)

#define SNMP_ERR_NOERROR    (0x0)
#define SNMP_ERR_TOOBIG	    (0x1)
#define SNMP_ERR_NOSUCHNAME (0x2)
#define SNMP_ERR_BADVALUE   (0x3)
#define SNMP_ERR_READONLY   (0x4)
#define SNMP_ERR_GENERR	    (0x5)

#define SNMP_TRAP_COLDSTART		(0x0)
#define SNMP_TRAP_WARMSTART		(0x1)
#define SNMP_TRAP_LINKDOWN		(0x2)
#define SNMP_TRAP_LINKUP		(0x3)
#define SNMP_TRAP_AUTHFAIL		(0x4)
#define SNMP_TRAP_EGPNEIGHBORLOSS	(0x5)
#define SNMP_TRAP_ENTERPRISESPECIFIC	(0x6)


#endif

