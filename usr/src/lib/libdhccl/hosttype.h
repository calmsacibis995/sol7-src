/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _HOSTTYPE_H
#define	_HOSTTYPE_H

#pragma ident	"@(#)hosttype.h	1.4	96/11/25 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Identifiers for various types used in host structure: */

#define	TYPE_NULL		0
#define	TYPE_U_CHAR		-1
#define	TYPE_INT16		-2
#define	TYPE_INT32		-3
#define	TYPE_CHAR_PTR		-4
#define	TYPE_ADDR		-5
#define	TYPE_ADDR_LIST		-6
#define	TYPE_INT16_LIST		-7
#define	TYPE_BINDATA		-8
#define	TYPE_BOOLEAN		-10
#define	TYPE_STRING_LIST	-11
#define	TYPE_TAG_LIST		-12
#define	TYPE_LEASE_ID		-13
#define	TYPE_TIME		-14
#define	TYPE_UNKNOWN		-16

#define	IS_LIST(type) \
	((type) == TYPE_ADDR_LIST || (type) == TYPE_INT16_LIST || \
	(type) == TYPE_STRING_LIST || (type) == TYPE_TAG_LIST)

#define	DHCP_PSEUDO		(uint16_t)0
#define	VENDOR_INDEPENDANT	(uint16_t)1
#define	THIS_VENDOR		(uint16_t)2 /* the index for this vendor */
#define	FIRST_VENDOR		(uint16_t)2
#define	VENDOR_UNSPECIFIED	-1

typedef struct HTstruct {
	int16_t tag;		/* BOOTP/DHCP tag number */
	int16_t type;		/* data type (see TYPE_* define's above) */
	u_char scope;		/* PER_INTERFACE or PER_HOST defined below */
	char *symbol;		/* two letter abbreviation */
	char *longname;		/* ASCII string of parameter name */

	/* SUN's "official" name, used in boot scripts */
	char *offname;

	uint16_t group;		/* parameter group (see define's below) */
	u_char numvalues;	/* number of values to display in UI */
	uint16_t vindex;	/* vendor class number */
} HTSTRUCT;

typedef struct VTstruct {
	char *vendorClass;
	int16_t hightag;
	int16_t *htindex;
	int16_t selfindex;
} VTSTRUCT;

typedef struct TFstruct {
	VTSTRUCT *vtp;
	HTSTRUCT *htp;
	int16_t vt_count; /* # of tag scopes: = 2 + #vendors */
	int16_t ht_slots; /* # elements in array pointed to by htp */
	int16_t ht_count; /* # elements used in array pointed to by htp */
} TFSTRUCT;

typedef struct TSstruct {
	char *str;
	char val;
} TSSTRUCT;

#define	PER_INTERFACE		1
#define	PER_HOST		2
#define	HOST_SCOPE(scope)	(((scope) & PER_HOST) == PER_HOST)

HTSTRUCT *find_bylongname(const char *);
HTSTRUCT *find_bysym(const char *);
HTSTRUCT *find_bytag(int, int);
const HTSTRUCT *find_byindex(int);
const VTSTRUCT *findVT(const char *);
void setDefaultVendor(const char *);
const VTSTRUCT *findVTbyindex(int);

#ifdef	__cplusplus
}
#endif

#endif /* _HOSTTYPE_H */
