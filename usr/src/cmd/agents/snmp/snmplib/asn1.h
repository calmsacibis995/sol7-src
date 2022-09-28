/* Copyright 04/24/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)asn1.h	1.4 97/04/24 Sun Microsystems"

#ifndef _ASN1_H_
#define _ASN1_H_


#include "impl.h"


/***** GLOBAL CONSTANTS *****/

#define MAX_SUBID   0xFFFFFFFF

#define MAX_OID_LEN	    64	/* max subid's in an oid */

#define ASN_BOOLEAN	    (0x01)
#define ASN_INTEGER	    (0x02)
#define ASN_BIT_STR	    (0x03)
#define ASN_OCTET_STR	    (0x04)
#define ASN_NULL	    (0x05)
#define ASN_OBJECT_ID	    (0x06)
#define ASN_SEQUENCE	    (0x10)
#define ASN_SET		    (0x11)

#define ASN_UNIVERSAL	    (0x00)
#define ASN_APPLICATION     (0x40)
#define ASN_CONTEXT	    (0x80)
#define ASN_PRIVATE	    (0xC0)

#define ASN_PRIMITIVE	    (0x00)
#define ASN_CONSTRUCTOR	    (0x20)

#define ASN_LONG_LEN	    (0x80)
#define ASN_EXTENSION_ID    (0x1F)
#define ASN_BIT8	    (0x80)

#define IS_CONSTRUCTOR(byte)	((byte) & ASN_CONSTRUCTOR)
#define IS_EXTENSION_ID(byte)	(((byte) & ASN_EXTENSION_ID) == ASN_EXTENSION_ID)


#define INTEGER     ASN_INTEGER
#define STRING      ASN_OCTET_STR
#define OBJID       ASN_OBJECT_ID
#define NULLOBJ     ASN_NULL

/* defined types (from the SMI, RFC 1065) */
#define IPADDRESS   (ASN_APPLICATION | 0)
#define COUNTER     (ASN_APPLICATION | 1)
#define GAUGE       (ASN_APPLICATION | 2)
#define TIMETICKS   (ASN_APPLICATION | 3)
#define OPAQUE      (ASN_APPLICATION | 4)


/***** GLOBAL FUNCTIONS *****/

extern u_char *asn_parse_int(register u_char *data, register int *datalength,
	u_char *type, long *intp, int intsize, char *error_label);
extern u_char *asn_parse_unsigned_int(register u_char *data, register int *datalength,
	u_char *type, unsigned long *intp, int intsize, char *error_label);
extern u_char *asn_build_int(register u_char *data, register int *datalength,
	u_char type, register long *intp, register int intsize, char *error_label);
extern u_char *asn_build_unsigned_int(register u_char *data, register int *datalength,
	u_char type, register unsigned long *intp, register int intsize, char *error_label);
extern u_char *asn_parse_string(u_char *data, register int *datalength,
	u_char *type, u_char *string, register int *strlength, char *error_label);
extern u_char *asn_build_string(u_char *data, register int *datalength,
	u_char type, u_char *string, register int strlength, char *error_label);
extern u_char *asn_parse_header(u_char *data, int *datalength, u_char *type, char *error_label);
extern u_char *asn_build_header(register u_char *data, int *datalength,
	u_char type, int length, char *error_label);
extern u_char *asn_parse_length(u_char *data, u_long *length, char *error_label);
extern u_char *asn_build_length(register u_char *data, int *datalength,
	register int length, char *error_label);
extern u_char *asn_parse_objid(u_char *data, int *datalength,
	u_char *type, Subid *objid, int *objidlength, char *error_label);
extern u_char *asn_build_objid(register u_char *data, int *datalength,
	u_char type, Subid *objid, int objidlength, char *error_label);
extern u_char *asn_parse_null(u_char *data, int *datalength, u_char *type, char *error_label);
extern u_char *asn_build_null(u_char *data, int *datalength, u_char type, char *error_label);


#endif
