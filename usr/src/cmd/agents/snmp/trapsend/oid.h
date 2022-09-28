/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)oid.h	1.2 96/07/01 Sun Microsystems"

#ifndef _OID_H_
#define _OID_H_
extern Oid *get_oid(char *enterprise_str); 
extern SNMP_variable *get_variable(char *buf); 
#endif
