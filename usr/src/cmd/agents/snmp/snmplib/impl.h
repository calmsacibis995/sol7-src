/* Copyright 07/22/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)impl.h	1.6 96/07/22 Sun Microsystems"

/* HISTORY
 * 7-11-96	Jerry Yeung
 */

#ifndef _IMPL_H_
#define _IMPL_H_


/***** GLOBAL CONSTANTS *****/

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE	1
#endif
#ifndef FALSE
#define FALSE	0
#endif

#ifndef True
#define True	1
#endif
#ifndef False
#define False	0
#endif

#ifndef MIN
#define MIN(x, y)	((x) < (y)? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y)	((x) > (y)? (x) : (y))
#endif


#define NOT_IMPLEMENTED		-1
#define END_OF_TABLE		-2
#define OTHER_ERROR		-3

#define EXACT_ENTRY		1
#define FIRST_ENTRY		2
#define	NEXT_ENTRY		3

#define	FIRST_PASS		1
#define	SECOND_PASS		2


/***** GLOBAL TYPES *****/

typedef long Integer;


typedef struct _String {
	u_char *chars;
	int len;
} String;


typedef unsigned long Subid;

typedef struct _Oid {
	Subid *subids;
	int len;
} Oid;

typedef struct _IndexType {
	int type;
	int len;
	int* value;
} IndexType;

typedef struct in_addr IPAddress;
typedef struct sockaddr_in Address;


/***** GLOBAL FUNCTIONS *****/

extern char *pdu_type_string(u_char type);
extern char *asn1_type_string(u_char type);
extern char *error_status_string(int status);
extern char *generic_trap_string(int generic);
extern char *SSAOidString(Oid *oid);
extern char *timeval_string(struct timeval *tv);
extern char *ip_address_string(IPAddress *ip_address);
extern char *address_string(Address *address);

/*** conversion routines ***/
extern char *SSAStringToChar(String str);
extern Oid *SSAOidStrToOid(char* name, char *error_label);

extern void SSAStringZero(String *string);
extern int SSAStringInit(String *string, u_char *chars, int len, char *error_label);
extern int SSAStringCpy(String *string1, String *string2, char *error_label);

extern Oid *SSAOidNew();
extern void SSAOidZero(Oid *oid);
extern void SSAOidFree(Oid *oid);
extern int SSAOidInit(Oid *oid, Subid *subids, int len, char *error_label);
extern int SSAOidCpy(Oid *oid1, Oid *oid2, char *error_label);
extern Oid *SSAOidDup(Oid *oid, char *error_label);
extern int SSAOidCmp(Oid *oid1, Oid *oid2);

extern int name_to_ip_address(char *name, IPAddress *ip_address, char *error_label);

extern int get_my_ip_address(IPAddress *my_ip_address, char *error_label);


#endif

