/* Copyright 08/12/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)snmp_dpi.h	1.2 96/08/12 Sun Microsystems"

/*
\begin{verbatim}
*/

#ifndef snmp_dpiH
#define snmp_dpiH

/*********************************************************************/
/*                                                                   */
/* SNMP-DPI API - SNMP Distributed Protocol Interface                */
/*                Application Programming Interface                  */
/*                                                                   */
/* Oct 27, 1994 - Version 0.14i                                      */
/*                                                                   */
/* Copyright    - (C) International Business Machines Corp. 1994     */
/*                                                                   */
/*   Permission to use, copy, modify, and distribute this software   */
/*   and its documentation for any lawful purpose and without fee is */
/*   hereby granted, provided that this notice be retained unaltered,*/
/*   and that the names of IBM and all other contributors shall not  */
/*   be used in advertising or publicity pertaining to distribution  */
/*   of the software without specific written prior permission.      */
/*   No contributor makes any representations about the suitability  */
/*   of this software for any purpose.  It is provided "as is"       */
/*   without express or implied warranty.                            */
/*                                                                   */
/*   IBM AND ALL OTHER CONTRIBUTORS DISCLAIM ALL WARRANTIES WITH     */
/*   REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF    */
/*   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, TITLE,    */
/*   AND NON-INFRINGEMENT.                                           */
/*                                                                   */
/*   IN NO EVENT SHALL IBM OR ANY OTHER CONTRIBUTOR BE LIABLE FOR    */
/*   ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES, WHETHER IN      */
/*   CONTRACT, TORT, OR OTHER FORM OF ACTION, ARISING OUT OF OR IN   */
/*   CONNECTION WITH, THE USE OR PERFORMANCE OF THIS SOFTWARE.       */
/*                                                                   */
/* snmp_dpi.h   - Base include file for SNMP DPI subagent support    */
/*              - This file defines the DPI interface for the DPI    */
/*                subagent programmer. This interface should be the  */
/*                same on all platforms and in all implementations   */
/*                                                                   */
/*********************************************************************/

/* Change activitity:
 *
 * $Log: snmp_dpi.h,v $
 * Revision 2.0.1.1  1994/10/31  19:37:41  pederson
 * netview/os2 and os2/tcp distribution
 *
 * Revision 2.0  1994/10/29  01:18:36  pederson
 * Revision 2.0
 *
 *
 */

/*********************************************************************/
/* Some of the compilers we used want a special LINKAGE keyword for  */
/* functions so they can do compiler independent linking. So we can  */
/* accomodate for that with e LINKAGE definition that is a no-op by  */
/* default. Specifically OS/2 needs this.                            */
/*********************************************************************/
#ifndef LINKAGE
#if defined(OS2) | defined(__OS2__)       /* for OS/2 we must define */
#define LINKAGE  _System                  /* _System Linkage because */
#else  /* not OS2 */                      /* DLLs/LIBs use that, else*/
#define LINKAGE                           /* don't show to compiler  */
#endif /* OS2 */
#endif /* ndef LINKAGE */

/*********************************************************************/
/* Some of the compilers we used seem to have trouble with the const */
/* qualifier (or I do not understand yet all its implications).      */
/* Anyways, I have used CONST qualifier where I want to use const.   */
/* If you compile with the -DCONST option, then the const qualifier  */
/* is used. Otherwise, it is just ignored.                           */
/*********************************************************************/
#ifdef CONST
#  ifndef CONST_ALREADY_DEFINED  /* CONST not defined in other h file*/
#    undef CONST                          /* undo and define our way */
#    define CONST const                   /* show it to compiler     */
#    define CONST_ALREADY_DEFINED  /* indicate CONST defined         */
#  endif
#else
#  define CONST                           /* don't show compiler     */
#  define CONST_ALREADY_DEFINED    /* indicate CONST defined         */
#endif

/*********************************************************************/
/* This is the size of the (static) buffer in which DPI packets are  */
/* created by the mkDPIxxxx functions. The variable is defined here  */
/* so that DPI subagent programmers can inspect it.                  */
/*********************************************************************/
#ifndef SNMP_DPI_BUFSIZE
#define SNMP_DPI_BUFSIZE    4096       /* max size of a DPI packet   */
#endif

/*********************************************************************/
/* These are options still under discussion for DPI 2.0              */
/* But for time being they are included.                             */
/*********************************************************************/
#define SNMP_DPI_VIEW_SELECTION
#define SNMP_DPI_BULK_SELECTION
/*#define SNMP_DPI_REGISTER_WITH_COMMUNITY*//* removed in revision 5 */

/*********************************************************************/
/* These are the codes for the SNMP_DPI packet types.                */
/* They come from RFC 1228 plus DPI 2.0 additions as defined in the  */
/* RFC 1592 for DPI version 2.0                                      */
/*********************************************************************/
#define SNMP_DPI_GET            1
#define SNMP_DPI_GETNEXT        2
#define SNMP_DPI_SET            3
#define SNMP_DPI_TRAP           4
#define SNMP_DPI_TRAPV1         4      /* is the same as TRAP        */
#define SNMP_DPI_RESPONSE       5
#define SNMP_DPI_REGISTER       6
#define SNMP_DPI_UNREGISTER     7
#define SNMP_DPI_OPEN           8
#define SNMP_DPI_CLOSE          9
#define SNMP_DPI_COMMIT        10
#define SNMP_DPI_UNDO          11
#define SNMP_DPI_GETBULK       12
#define SNMP_DPI_TRAPV2        13      /* reserved, not implemented  */
#define SNMP_DPI_INFORM        14      /* reserved, not implemented  */
#define SNMP_DPI_ARE_YOU_THERE 15

/*********************************************************************/
/* If a user a DPI subagent programmer wants to use an older level   */
/* of the DPI protocol, then he/she can specify that in his source   */
/* by these C-preprocessor define statements:                        */
/*   #define SNMP_DPI_VERSION 1                                      */
/*   #define SNMP_DPI_RELEASE 1        ** or 0 is also valid         */
/* This may be useful when you want to use existing DPI subagent     */
/* code with the DPI 2.0 library and include files.                  */
/* The default is Version 2 Release 0.                               */
/*********************************************************************/
#define SNMP_DPI_PROTOCOL   2          /* This is SNMP-DPI protocol  */
#ifndef SNMP_DPI_VERSION               /* if user did not specify a  */
#define SNMP_DPI_VERSION    2          /* version, then default is 2 */
#endif
#ifndef SNMP_DPI_RELEASE               /* if user did not specify a  */
#define SNMP_DPI_RELEASE    0          /* release, then default is 0 */
#endif

#if (SNMP_DPI_VERSION == 1)            /* if caller wants DPI 1.x    */

#include "snmp_dp1.h"                  /* compatibility, include it  */

#else /* no SNMP_DPI_VERSION 1 */      /* else define them for 2.x   */

#define mkDPIAreYouThere       mkDPIayt
#define mkDPIregister(a,b,c,d) mkDPIreg((a),(b),(c),(d),0)
#define mkDPIunregister(a,b)   mkDPIureg((a),(b))
#define mkDPIresponse(a,b,c,d) mkDPIresp((a),(b),(c),(d))
#define mkDPItrap(a,b,c,d)     mkDPItrape((a),(b),(c),(d))
#define mkDPIget(a,b,c)        mkDPIget_packet((a),(b),(c))
#define mkDPIset(a,b,c,d,e,f)  mkDPIset_packet((a),(b),(c),(d),(e),(f))
#define mkDPInext(a,b,c)       mkDPInext_packet((a),(b),(c))
#define fDPIset(a)             fDPIset_packet((a))
#define query_DPI_port(a,b,c)  qDPIport((a),(b),(c))
#define DPI_PACKET_LEN(packet) (((packet) == (void *)0) ? 0 : \
                               (*((unsigned char *)(packet)) * 256 + \
                                 *(((unsigned char *)(packet))+1)) + 2 )

#endif /* (SNMP_DPI_VERSION == 1) */

#ifdef DPI_INCLUDE_SNMP_CONFIG
#include "snmp_config.h"
#endif

/*********************************************************************/
/* SNMP_DPI variable types from RFC1592 for SNMP DPI Version 2.0     */
/* - The 32-bit numeric-type TYPEs have the high order bit set so    */
/*   you can quickly check for a 4-byte numeric value.               */
/*   These have their value in the DPI packet in network byte order  */
/*   (MSB first, LSB last), but they have the value in host byte     */
/*   order when passed as arguments to functions or when present in  */
/*   a parse tree. So a DPI sub-agent programmer when using the API  */
/*   need not be concerned with the byte order in the DPI packet.    */
/* - Counter 64 is currently implemented such that the the value is  */
/*   in 8-byte, network byte order in the DPI packet and that the    */
/*   value pointed to by the snmp_dpi_set_structure is a ptr to a    */
/*   snmp_dpi_u64 structure that has 2 unsigned long integers, one   */
/*   for the high order (Most significant) and one for the low order */
/*   (Least significant) 32-bit pieces. The DPI sub-agent programmer */
/*   need only be concerned with the snmp_dpi_u64 structure.         */
/* - Beware that Textual Conventions are not known on the wire       */
/*   (in SNMP PDUs), so that is why we do not provide types for them.*/
/*   You must use the base type in which the data is represented.    */
/*   The one exception is a DisplayString. If the agent knows the    */
/*   textual convention of an object to be DisplayString (from some  */
/*   form of a compiled MIB), then it passes SNMP_TYPE_DisplayString */
/*   The reason for this is that DisplayString is special in the     */
/*   sense that it often used to determine how to print/display the  */
/*   data (always printable).                                        */
/* - Values for DisplayString and OBJECT_IDENTIFIERS are represented */
/*   as strings in the character set selected at DPI OPEN time.      */
/* - In DPI 1.x the IP address was (128|5), and now it is just 5.    */
/*   In both cases it is an IP address, 4 bytes, network byte order. */
/*   The high order bit (128) should not have been used in DPI 1.x   */
/* - It is recommended to use these DPI 2.0 names for SNMP_TYPE_xxxx */
/*   They are based on the names used in SNMPv2 (RFC1442)            */
/* - The BIT_STRING is currently implemented such that you get an    */
/*   octet string of the form 0xuubbbb....bb where the first octet   */
/*   uu has a value in the range 0-7 indicating how many unused bits */
/*   there are in the last byte bb. The bb bytes represent the bit   */
/*   string itself, where bit zero (0) comes fist and so on. This is */
/*   the case both in the DPI packet and in the snmp_dpi_set_packet  */
/*   structure.                                                      */
/*********************************************************************/
#define SNMP_TYPE_MASK           0x7f  /* mask to isolate type       */
#define SNMP_TYPE_Integer32    (128|1) /* 32-bit INTEGER             */
#define SNMP_TYPE_OCTET_STRING      2  /* OCTET STRING (ASN.1)       */
#define SNMP_TYPE_OBJECT_IDENTIFIER 3  /* OBJECT IDENTIFIER (ASN.1)  */
#define SNMP_TYPE_NULL              4  /* NULL (ASN.1)               */
#define SNMP_TYPE_IpAddress         5  /* IMPLICIT OCTET STRING (4)  */
#define SNMP_TYPE_Counter32    (128|6) /* 32-bit Counter (unsigned)  */
#define SNMP_TYPE_Gauge32      (128|7) /* 32-bit Gauge   (unsigned)  */
#define SNMP_TYPE_TimeTicks    (128|8) /* 32-bit TimeTicks (unsigned)*/
                                       /* in hundreths of a second   */
#define SNMP_TYPE_DisplayString     9  /* DisplayString (Textual Con)*/
#define SNMP_TYPE_BIT_STRING        10 /* BIT STRING (ASN.1)         */
#define SNMP_TYPE_NsapAddress       11 /* IMPLICIT OCTET STRING      */
#define SNMP_TYPE_UInteger32   (128|12)/* 32-bit INTEGER (unsigned)  */
#define SNMP_TYPE_Counter64         13 /* 64-bit Counter (unsigned)  */
#define SNMP_TYPE_Opaque            14 /* IMPLICIT OCTET STRING      */
#define SNMP_TYPE_noSuchObject      15 /* IMPLICIT NULL              */
#define SNMP_TYPE_noSuchInstance    16 /* IMPLICIT NULL              */
#define SNMP_TYPE_endOfMibView      17 /* IMPLICIT NULL              */

/*********************************************************************/
/* These codes should be used in the ret_code field of an SNMP DPI   */
/* reponse packet. There are 2 types of error codes:                 */
/* - Error codes as defined in RFC1448, section 3, page 9. These are */
/*   to be used when sending a response to an original SNMP request  */
/*   (e.g. GET, GETNEXT, SET, COMMIT, UNDO).                         */
/* - Additional SNMP DPI error codes, to be used for responses to    */
/*   DPI specific requests (e.g. OPEN, REGISTER, UNREGISTER).        */
/*********************************************************************/
#ifndef snmp_baseH
#define SNMP_ERROR_noError             0
#define SNMP_ERROR_tooBig              1
#define SNMP_ERROR_noSuchName          2
#define SNMP_ERROR_badValue            3
#define SNMP_ERROR_readOnly            4
#define SNMP_ERROR_genErr              5
#define SNMP_ERROR_noAccess            6
#define SNMP_ERROR_wrongType           7
#define SNMP_ERROR_wrongLength         8
#define SNMP_ERROR_wrongEncoding       9
#define SNMP_ERROR_wrongValue          10
#define SNMP_ERROR_noCreation          11
#define SNMP_ERROR_inconsistentValue   12
#define SNMP_ERROR_resourceUnavailable 13
#define SNMP_ERROR_commitFailed        14
#define SNMP_ERROR_undoFailed          15
#define SNMP_ERROR_authorizationError  16
#define SNMP_ERROR_notWritable         17
#define SNMP_ERROR_inconsistentName    18
#endif /* ndef snmp_baseH */

#define SNMP_ERROR_DPI_noError                             0
#define SNMP_ERROR_DPI_otherError                        101
#define SNMP_ERROR_DPI_notFound                          102
#define SNMP_ERROR_DPI_alreadyRegistered                 103
#define SNMP_ERROR_DPI_higherPriorityRegistered          104
#define SNMP_ERROR_DPI_mustOpenFirst                     105
#define SNMP_ERROR_DPI_notAuthorized                     106
#define SNMP_ERROR_DPI_viewSelectionNotSupported         107
#define SNMP_ERROR_DPI_getBulkSelectionNotSupported      108
#define SNMP_ERROR_DPI_duplicateSubAgentIdentifier       109
#define SNMP_ERROR_DPI_invalidDisplayString              110
#define SNMP_ERROR_DPI_characterSetSelectionNotSupported 111

/*********************************************************************/
/* SNMP DPI UNREGISTER reason codes                                  */
/*********************************************************************/
#define SNMP_UNREGISTER_otherReason                1
#define SNMP_UNREGISTER_goingDown                  2
#define SNMP_UNREGISTER_justUnregister             3
#define SNMP_UNREGISTER_newRegistration            4
#define SNMP_UNREGISTER_higherPriorityRegistered   5
#define SNMP_UNREGISTER_byManager                  6
#define SNMP_UNREGISTER_timeout                    7

/*********************************************************************/
/* SNMP DPI CLOSE reason codes                                       */
/*********************************************************************/
#define SNMP_CLOSE_otherReason                     1
#define SNMP_CLOSE_goingDown                       2
#define SNMP_CLOSE_unsupportedVersion              3
#define SNMP_CLOSE_protocolError                   4
#define SNMP_CLOSE_authenticationFailure           5
#define SNMP_CLOSE_byManager                       6
#define SNMP_CLOSE_timeout                         7
#define SNMP_CLOSE_openError                       8

/*********************************************************************/
/* These are structures for a (partial) parse tree for DPI packets.  */
/*********************************************************************/
struct dpi_reg_packet {
  unsigned short          timeout;     /* in seconds; 16-bit u_int   */
  long int                priority;    /* priority for this subagent */
  char                   *group_p;     /* ptr to group OIDstring     */
  struct dpi_reg_packet  *next_p;      /* ptr to next in chain       */
#ifdef SNMP_DPI_VIEW_SELECTION
  char                    view;        /* for BNR DPI version 1.2    */
#endif
#ifdef SNMP_DPI_BULK_SELECTION
  char                    bulk;
#endif
#ifdef SNMP_DPI_REGISTER_WITH_COMMUNITY
  unsigned short          community_len;  /* BNR DPI version 1.2     */
  unsigned char          *community_p     /* compatibility           */
#endif
};

struct dpi_ureg_packet {
  char                    reason_code; /* unregister reason code     */
  char                   *group_p;     /* ptr to group OIDstring     */
  struct dpi_ureg_packet *next_p;      /* ptr to next in chain       */
};

struct dpi_get_packet {
  char                   *object_p;    /* ptr to OIDstring (DPI 1.x) */
  char                   *group_p;     /* ptr to group OIDstring     */
  char                   *instance_p;  /* ptr to group OIDstring     */
  struct dpi_get_packet  *next_p;      /* ptr to next in chain       */
};

struct dpi_next_packet {
  char                   *object_p;    /* ptr to OIDstring (DPI 1.x) */
  char                   *group_p;     /* ptr to group OIDstring     */
  char                   *instance_p;  /* ptr to group OIDstring     */
  struct dpi_next_packet *next_p;      /* ptr to next in chain       */
};

struct dpi_bulk_packet {
  long int               non_repeaters;   /* number of non-repeaters */
  long int               max_repetitions; /* max repeaters           */
  struct dpi_next_packet *varBind_p;      /* ptr to varBinds: chain  */
                                          /* of dpi_next_packets     */
};

struct snmp_dpi_u64 {                  /* for unsigned 64-bit int    */
  unsigned long high;                  /* - high order 32 bits       */
  unsigned long low;                   /* - low order  32 bits       */
};                                     /* for unsigned 64-bit int    */
typedef struct snmp_dpi_u64 snmp_dpi_u64;

struct dpi_set_packet {
  char                   *object_p;    /* ptr to OIDstring (DPI 1.x) */
  char                   *group_p;     /* ptr to group OIDstring     */
  char                   *instance_p;  /* ptr to group OIDstring     */
  unsigned char           value_type;  /* value type; SNMP_TYPE_xxxx */
  unsigned short          value_len;   /* value length               */
  char                   *value_p;     /* ptr to the value itself    */
  struct dpi_set_packet  *next_p;      /* ptr to next in chain       */
};

struct dpi_resp_packet {
  char                     error_code; /* error code: SNMP_ERROR_xxx */
  unsigned long int        error_index;/* index 1st varBind in error */
  #define  resp_priority   error_index /* for response to register   */
  struct dpi_set_packet   *varBind_p;  /* ptr to varBinds: chain of  */
};                                     /* dpi_set_packets            */

struct dpi_trap_packet {
  long int                 generic;    /* long must be 4 bytes long  */
  long int                 specific;
  struct dpi_set_packet   *varBind_p;  /* ptr to varBinds: chain of  */
                                       /* SET structures             */
  char                    *enterprise_p; /* ptr to enterprise ID     */
};

struct dpi_open_packet {
  char                 *oid_p;         /* subagent ID, an OIDstring  */
  char                 *description_p; /* subagent descriptive name  */
  unsigned short        timeout;       /* in seconds, 16-bit u_int   */
  unsigned short        max_varBinds;  /* max varBinds I can handle  */
  char                  character_set; /* character set selection    */
  unsigned short        password_len;  /* length of password         */
  unsigned char        *password_p;    /* ptr to password itself     */
};

struct dpi_close_packet {
  char                  reason_code;   /* reason for closing         */
};

typedef struct snmp_dpi_hdr             snmp_dpi_hdr;
typedef struct dpi_open_packet          snmp_dpi_open_packet;
typedef struct dpi_close_packet         snmp_dpi_close_packet;
typedef struct dpi_reg_packet           snmp_dpi_reg_packet;
typedef struct dpi_ureg_packet          snmp_dpi_ureg_packet;
typedef struct dpi_get_packet           snmp_dpi_get_packet;
typedef struct dpi_next_packet          snmp_dpi_next_packet;
typedef struct dpi_bulk_packet          snmp_dpi_bulk_packet;
typedef struct dpi_set_packet           snmp_dpi_set_packet;
typedef struct dpi_resp_packet          snmp_dpi_resp_packet;
typedef struct dpi_trap_packet          snmp_dpi_trap_packet;

#define snmp_dpi_hdr_NULL_p             ((snmp_dpi_hdr *)0)
#define snmp_dpi_open_packet_NULL_p     ((snmp_dpi_open_packet *)0)
#define snmp_dpi_close_packet_NULL_p    ((snmp_dpi_close_packet *)0)
#define snmp_dpi_get_packet_NULL_p      ((snmp_dpi_get_packet *)0)
#define snmp_dpi_next_packet_NULL_p     ((snmp_dpi_next_packet *)0)
#define snmp_dpi_bulk_packet_NULL_p     ((snmp_dpi_bulk_packet *)0)
#define snmp_dpi_set_packet_NULL_p      ((snmp_dpi_set_packet *)0)
#define snmp_dpi_resp_packet_NULL_p     ((snmp_dpi_resp_packet *)0)
#define snmp_dpi_trap_packet_NULL_p     ((snmp_dpi_trap_packet *)0)
#define snmp_dpi_reg_packet_NULL_p      ((snmp_dpi_reg_packet *)0)
#define snmp_dpi_ureg_packet_NULL_p     ((snmp_dpi_ureg_packet *)0)

struct snmp_dpi_hdr {
  unsigned char  proto_major;
  unsigned char  proto_version;
  #define        proto_minor proto_version  /* for DPI 1.x */
  unsigned char  proto_release;
 /* unsigned short packet_id; */                /* 2 bytes; 16-bit int   */
  unsigned long  packet_id;      /* This type is lengthened to match SNMP_PDU */
  unsigned char  packet_type;
  union {
     snmp_dpi_reg_packet      *reg_p;
     snmp_dpi_ureg_packet     *ureg_p;
     snmp_dpi_get_packet      *get_p;
     snmp_dpi_next_packet     *next_p;
     snmp_dpi_next_packet     *bulk_p;
     snmp_dpi_set_packet      *set_p;
     snmp_dpi_resp_packet     *resp_p;
     snmp_dpi_trap_packet     *trap_p;
     snmp_dpi_open_packet     *open_p;
     snmp_dpi_close_packet    *close_p;
     unsigned char            *any_p;
  } data_u;
#ifdef SNMP_DPI_VIEW_SELECTION
  unsigned short community_len;             /* BNR DPI version 1.2   */
  unsigned char *community_p;               /* compatibility fields  */
#endif
};

#if defined(__cplusplus)               /* C++ compiler               */
#ifndef DPI_NO_EXTERN_C
extern "C" {                           /* sees them as extern C      */
#endif
#endif /* __cplusplus */

/*********************************************************************/
/* Following functions are common functions for DPI programmers:     */
/* - DPIdebug can be used to turn all internal DPI tracing/debugging */
/*   on or off. If turned on, the functions within the DPI library   */
/*   report what happens via messages to stdout and/or stderr.       */
/* - pDPIpacket can be used to create a parse tree from a DPI packet */
/*   received on the "wire" or "connection".                         */
/* - fDPIparse can be used to free the complete parse tree when it   */
/*   is no longer needed. It will free all allocated memory.         */
/* - fDPIset (defined as a macro above so that it translates into a  */
/*   fDPIset_packet) can be used to free a complete SET parse tree   */
/*   This was found to be needed/handy when preparing a SET tree for */
/*   a reponse or trap and then running into an error so that the    */
/*   tree is no longer needed, while there is no snmp_dpi_hdr (yet). */
/*********************************************************************/

void   LINKAGE        DPIdebug(        /* Set all DPI internal debug */
  int                   level);        /* zero=off; otherwise=on at  */
                                       /* specified level            */

snmp_dpi_hdr * LINKAGE pDPIpacket(     /* parse a DPI packet         */
  unsigned char        *packet_p);     /* ptr to the DPI packet      */

void     LINKAGE      fDPIparse(       /* free a DPI parse tree      */
  snmp_dpi_hdr         *hdr_p);        /* ptr to parse tree          */

void     LINKAGE      fDPIset_packet(  /* free a DPI SET parse tree  */
  snmp_dpi_set_packet  *packet_p);     /* ptr to SET packet (varBind)*/

/*********************************************************************/
/* Following functions create a serialized SNMP DPI packet in a      */
/* static buffer. There is only ONE such buffer, shared by all       */
/* packet types. So access to the buffer must be serialized.         */
/* - If success, a ptr to that buffer is returned.                   */
/* - if failure, a NULL ptr is returned.                             */
/*                                                                   */
/* Note(s):                                                          */
/* - If success, then the first 2 bytes (network byte order) of the  */
/*   packet contain the length of remaining packet. Beware that the  */
/*   complete packet (including the length bytes) must be sent over  */
/*   to the other side of a DPI "connection". To calculate the total */
/*   length, you can use the DPI_PACKET_LEN macro defined above.     */
/* - The complete or partial parse tree that is passed to the PACKET,*/
/*   RESP and TRAPe functions is ALWAYS freed by those functions.    */
/*   So upon return from these funcions (successful or unsuccessful) */
/*   one should not reference the parse tree anymore.                */
/*********************************************************************/
unsigned char * LINKAGE mkDPIpacket(   /* Make a DPI packet          */
  snmp_dpi_hdr         *hdr_p);        /* ptr to a parse tree        */

unsigned char * LINKAGE mkDPIopen(     /* Make a DPI open packet     */
  char                 *oid_p,         /* subagent Identifier (OID)  */
  char                 *description_p, /* subagent descriptive name  */
  unsigned long         timeout,       /* requested default timeout  */
  unsigned long         max_varBinds,  /* max varBinds per DPI packet*/
  char                  character_set, /* selected character set     */
  #define DPI_NATIVE_CSET  0           /*   0 = native character set */
  #define DPI_ASCII_CSET   1           /*   1 = ASCII  character set */
  unsigned long         password_len,  /* length of pasword (if any) */
  unsigned char        *password_p);   /* ptr to password (if any)   */

unsigned char * LINKAGE mkDPIclose(    /* Make a DPI close packet    */
  char                  reason_code);  /* reason for closing         */

unsigned char * LINKAGE mkDPIreg(      /* Make a DPI register packet */
  unsigned short        timeout,       /* in seconds (16-bit)        */
  long int              priority,      /* requested priority         */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                  bulk_select,   /* Bulk selection (GETBULK)   */
  #define DPI_BULK_NO   0              /*  map GETBULK into GETNEXTs */
  #define DPI_BULK_YES  1              /*  pass GETBULK to sub-agent */
  char                  view_select);  /* View selection yes(1)/no(0)*/
  #define DPI_VIEW_NO   0              /* (for BNR DPI version 1.2   */
  #define DPI_VIEW_YES  1              /*  compatibility only)       */

unsigned char * LINKAGE mkDPIureg(     /* Make DPI unregister packet */
  char                  reason_code,   /* subagent specific code     */
  char                 *group_p);      /* ptr to group ID (subtree)  */

unsigned char * LINKAGE mkDPIbulk(     /* Make a DPI response packet */
  long int             non_repeaters,  /* non repeaters              */
  long int             max_repetitions,/* maximum repetitions        */
  snmp_dpi_next_packet *packet_p);     /* ptr to chain of NEXT packts*/
                                       /* containing the varBinds    */

unsigned char * LINKAGE mkDPIayt(void);/* Make DPI AreYouThere packet*/

unsigned char * LINKAGE mkDPIresp(     /* Make a DPI response packet */
  snmp_dpi_hdr         *hdr_p,         /* ptr to packet to respond to*/
  long int              ret_code,      /* error code: SNMP_ERROR_xxx */
  long int              ret_index,     /* index to varBind in error  */
  snmp_dpi_set_packet  *packet_p);     /* ptr to chain of SET packets*/
                                       /* containing the varBinds    */

unsigned char * LINKAGE mkDPItrape(    /* Make a DPI trap packet     */
  long int              generic,       /* generic trap type  (32 bit)*/
  long int              specific,      /* specific trap type (32 bit)*/
  snmp_dpi_set_packet  *packet_p,      /* ptr to chain of SET packets*/
                                       /* containing the varBinds    */
  char                 *enterprise_p); /* ptr to enterprise OID      */

/*********************************************************************/
/* Following functions will create a complete or partial parse tree. */
/* - If success, a ptr to the (complete or partial) parse tree is    */
/*   returned.                                                       */
/* - If failure, a NULL ptr is returned.                             */
/*                                                                   */
/* Note(s):                                                          */
/* - All the callers other arguments are copied to newly and         */
/*   and dynamically allocated memory, so callers ptrs are always    */
/*   valid upon return (successful or unsuccessful).                 */
/* - All required memory is dynamically allocated.                   */
/* - The REG, UREG, GET, NEXT, SET, parse trees are formed by a      */
/*   chain of packet structures. For these functions:                */
/*   - When you pass a ptr to an existing chain of structures        */
/*     (partial parse tree), then a new structure will be added at   */
/*     the end of the chain.                                         */
/*     Upon success, the returned ptr will be the ptr to the first   */
/*     entry in the chain (e.g. the one you passed).                 */
/*     Upon failure, you always get a NULL ptr returned, so you must */
/*     ensure that you remember the ptr to the first entry.          */
/*   - When you pass a NULL ptr to indicate that this is the first   */
/*     structure you want to allocate, then a ptr to the new (and    */
/*     thus first entry) structure will be returned (if success).    */
/*   - All the callers other arguments are copied to newly and       */
/*     and dynamically allocated memory, so callers ptrs are always  */
/*     valid upon return (successful or unsuccessful).               */
/* - The mkDPIhdrv() and mkDPInext_packet() are meant for use by an  */
/*   agent and are most probably not needed by a subagent.           */
/*********************************************************************/
/* This macro for mkDPIhdr(a) provides the published mkDPIhdr()      */
/* function that normally should be used by subagents:               */
/*    snmp_dpi_hdr *mkDPIhdr(int dpi_packet_type)                    */
/*********************************************************************/
#define mkDPIhdr(a)   mkDPIhdr_version((a),SNMP_DPI_VERSION,         \
                                           SNMP_DPI_RELEASE)

snmp_dpi_hdr * LINKAGE mkDPIhdr_version(/* Make DPIhdr for parse tree*/
  int                   type,          /* packet type; SNMP_DPI_xxxx */
  char                  version,       /* for this specific version  */
  char                  release);      /* for this specific release  */

snmp_dpi_reg_packet * LINKAGE mkDPIreg_packet(/* Make DPIreg_packet  */
  snmp_dpi_reg_packet  *packet_p,      /* ptr to REGISTER structure  */
  unsigned short        timeout,       /* in seconds (16-bit)        */
  long int              priority,      /* requested priority         */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                  bulk_select,   /* Bulk selection yes(1)/no(0)*/
  char                  view_select);  /* View selection yes(1)/no(0)*/
                                       /* (for BNR DPI version 1.2   */
                                       /*  compatibility only)       */

snmp_dpi_ureg_packet * LINKAGE mkDPIureg_packet(/* Make DPIureg packt*/
  snmp_dpi_ureg_packet *packet_p,      /* ptr to UNREGISTER structure*/
  char                  reason_code,   /* subagent specific code     */
  char                 *group_p);      /* ptr to group ID (subtree)  */

snmp_dpi_get_packet  * LINKAGE mkDPIget_packet(/* Make DPIget packet */
  snmp_dpi_get_packet  *packet_p,      /* ptr to GET structure       */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                 *instance_p);   /* ptr to instance OID string */

snmp_dpi_next_packet * LINKAGE mkDPInext_packet(/* Make DPInext packt*/
  snmp_dpi_next_packet *packet_p,      /* ptr to NEXT structure      */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                 *instance_p);   /* ptr to instance OID string */

snmp_dpi_set_packet  * LINKAGE mkDPIset_packet(/* Make DPIset packet */
  snmp_dpi_set_packet  *packet_p,      /* ptr to SET structure       */
  char                 *group_p,       /* ptr to group ID (subtree)  */
  char                 *instance_p,    /* ptr to instance OID string */
  int                   value_type,    /* value type (SNMP_TYPE_xxx) */
  int                   value_len,     /* length of value            */
  void                 *value_p);      /* ptr to value               */

/*********************************************************************/
/* The following functions are defined as part of the DPI interface  */
/* for DPI subagents. They are system specific functions though, so  */
/* they may or may not be available in the exact same form on all    */
/* platforms or in all vendor provided DPI implementations. Check    */
/* you platform/vendor documentation for exact specification.        */
/*********************************************************************/
/* Prototypes for SNMP-DPI communication (to/from agent) related     */
/* functions. They invoke system dependent code that knows how to    */
/* actually make/close the connections and how to send/receive data  */
/* over these connections.                                           */
/* - The connect functions:                                          */
/*   - If success, return a positive handle that uniquely identifies */
/*     the connection. To be passed when calling send/await function.*/
/*   - If failure, return a negative error code (DPI_RC_xxxx)        */
/* - The send/await/disconnect functions need the handle that        */
/*   identifies the connection.                                      */
/* - The send/await functions:                                       */
/*   - If success, return a zero (DPI_RC_OK)                         */
/*   - If failure, return a negative error code (DPI_RC_xxxx)        */
/* - The qDPIport/lookup_host functions are for DPI 1.x compatibility*/
/*   and for those who want to do communications themselves instead  */
/*   of using DPIxxxx functions to connect/send/wait etc.            */
/*********************************************************************/
#define dpiPortForTCP               1  /* for use with qDPIport()    */
#define dpiPortForUDP               2  /* porttype argument          */
#define DPIconnect      DPIconnectTCP  /* default is TCP port        */
#define DPI_RC_OK                   0  /* all OK, no error           */
#define DPI_RC_NOK                 -1  /* some other error           */
#define DPI_RC_NO_PORT             -2  /* cannot figure out DPI port */
#define DPI_RC_NO_CONNECTION       -3  /* no connection to DPI agent */
#define DPI_RC_EOF                 -4  /* EOF received on connection */
#define DPI_RC_IO_ERROR            -5  /* Some I/O error on connect. */
#define DPI_RC_INVALID_HANDLE      -6  /* unknown/invalid handle     */
#define DPI_RC_TIMEOUT             -7  /* timeout occured            */
#define DPI_RC_PACKET_TOO_LARGE    -8  /* packed too large, dropped  */

int LINKAGE  DPIconnect_to_agent_TCP(  /* Connect to DPI TCP port    */
  char                 *hostname_p,    /* target hostname/IPaddress  */
  char                 *community_p);  /* communityname              */

int LINKAGE  DPIconnect_to_agent_UDP(  /* Connect to DPI UDP port    */
  char                 *hostname_p,    /* target hostname/IPaddress  */
  char                 *community_p);  /* communityname              */

int LINKAGE  DPIconnect_to_agent_NMQ(  /* Connect to DPI Named Queue */
  char                 *t_qname_p,     /* target (agent) queue name  */
  char                 *s_qname_p);    /* source (subagent) q name   */

int LINKAGE  DPIconnect_to_agent_SHM(  /* Connect to DPI Shared Mem  */
  int                   queue_id);     /* target (agent) queue id    */

int LINKAGE  DPIget_fd_for_handle(     /* get the filedescriptor for */
  int                   handle);       /* this handle                */

void LINKAGE DPIdisconnect_from_agent( /* disconnect from DPI (agent)*/
  int                   handle);       /* close this connection      */

int LINKAGE  DPIawait_packet_from_agent(  /* await a DPI packet      */
  int                   handle,        /* on this connection         */
  int                   timeout,       /* timeout in seconds.        */
  unsigned char       **message_p,     /* receives ptr to data       */
  unsigned long        *length);       /* receives length of data    */

int LINKAGE  DPIsend_packet_to_agent(  /* send a DPI packet          */
  int                   handle,        /* on this connection         */
  CONST unsigned char  *message_p,     /* the packet to send         */
  unsigned long         length);       /* length of packet           */

long int  LINKAGE     qDPIport(        /* Query (GET) SNMP_DPI port  */
  char                 *hostname_p,    /* target hostname/IPaddress  */
  char                 *community_p,   /* communityname for GET      */
  int                   porttype);     /* port instance, one of:     */
                                       /*   dpiPortForTCP            */
                                       /*   dpiPortForUDP            */

unsigned long  LINKAGE  lookup_host(   /* find IPaddress in network  */
  char                 *hostname_p);   /* byte order for this host   */

#if defined(__cplusplus)               /* close up the C++  scope    */
#ifndef DPI_NO_EXTERN_C
}
#endif
#endif /* __cplusplus) */

#pragma pack()

#endif /* snmp_dpiH */

/*
\end{verbatim}
*/
