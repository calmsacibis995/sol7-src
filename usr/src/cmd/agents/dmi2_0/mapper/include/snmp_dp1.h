/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)snmp_dp1.h	1.1 96/08/05 Sun Microsystems"

/*
\begin{verbatim}
*/

#ifndef snmp_dp1H
#define snmp_dp1H

/*********************************************************************/
/*                                                                   */
/* SNMP-DPI API - SNMP Distributed Protocol Interface                */
/*                Application Programming Interface                  */
/*                                                                   */
/* Jun 19, 1994 - Version 0.14i                                      */
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
/* snmp_dp1.h   - Include file for SNMP DPI 1.x compatibility mode   */
/*                                                                   */
/*-------------------------------------------------------------------*/

/* Change activitity:
 *
 * $Log: snmp_dp1.h,v $
 * Revision 2.0.1.1  1994/10/31  19:37:41  pederson
 * netview/os2 and os2/tcp distribution
 *
 * Revision 2.0  1994/10/20  19:58:37  pederson
 * new base
 *
 *
 */

/*********************************************************************/
/* SNMP_DPI variable types from RFC1228 for DPI version 1.0 and 1.x  */
/* - SNMP_TYPE_TEXT is never used in the SNMP_DPI protocol.          */
/* - The numeric-type TYPEs have the high order bit set, so that you */
/*   can quickly check for a 4-byte numeric. Those types have their  */
/*   value in the DPI packet in network byte order but they have     */
/*   value in host byte order when passed as arguments to functions  */
/*   or when present in a parse tree. The one exception is the       */
/*   INTERNET type, which is by definition in network byte order. It */
/*   would have been better to have that type without the high order */
/*   bit set, so that is what we have done for DPI 2.0               */
/* - Beware that Textual Conventions are not known on the wire, so   */
/*   that is why we do not provide types for them. You must use the  */
/*   base type in which the data is represented.                     */
/* - It is recommended to use the SNMP DPI 2.0 names as defined in   */
/*   the snmp_dpi.h include file.                                    */
/*   These names below are defined for compatibility with DPI 1.x    */
/*********************************************************************/
#define SNMP_TYPE_TEXT      0          /* textual representation,    */
#define SNMP_TYPE_MASK           0x7f  /* mask to isolate type       */
#define SNMP_TYPE_NUMBER       (128|1) /* number (INTEGER)           */
#define SNMP_TYPE_STRING            2  /* text string (OCTET STRING  */
#define SNMP_TYPE_OBJECT            3  /* object (ONJECT IDENTIFIER) */
#define SNMP_TYPE_EMPTY             4  /* no value (NULL)            */
#define SNMP_TYPE_INTERNET     (128|5) /* InternetAddress (IpAddress)*/
#define SNMP_TYPE_COUNTER      (128|6) /* counter (Counter)          */
#define SNMP_TYPE_GAUGE        (128|7) /* gauge   (Gauge)            */
#define SNMP_TYPE_TICKS        (128|8) /* time ticks (0.01 seconds)  */

/*********************************************************************/
/* Following SNMP error codes are from RFC 1157 (1098, 1067)         */
/* These codes are only defined for DPI 1.x compatibility.           */
/* For DPI 2.x and up, it is recommended to use the codes defined    */
/* in the snmp_dpi.h include file.                                   */
/*********************************************************************/
#define SNMP_NO_ERROR       0
#define SNMP_TOO_BIG        1
#define SNMP_NO_SUCH_NAME   2
#define SNMP_BAD_VALUE      3
#define SNMP_READ_ONLY      4
#define SNMP_GEN_ERR        5

/*********************************************************************/
/* Old style DPI 1.x used GET_NEXT instead of GETNEXT.               */
/* This one must match the SNMP_DPI_GETNEXT definition in snmp_dpi.h */
/*********************************************************************/
#define SNMP_DPI_GET_NEXT   2          /* old DPI 1.x style          */

/*********************************************************************/
/* These defines allow existing DPI 1.0 and DPI 1.1 function calls   */
/* to work with DPI 2.0 code. This to make conversion to 2.0 easier. */
/*********************************************************************/
#define mkDPIregister(a)      mkDPIreg(0, -1L, (a), 0, 0)
#define mkDPIresponse(a,b)    mkDPIresp(snmp_dpi_hdr_NULL_p,          \
                                        (a), 0L, (b))
#define mkDPItrap(a,b,c)      mkDPItrape((a),(b),(c),(char *)0)
#define mkDPIget(a)           mkDPIget_packet(                        \
                                   snmp_dpi_get_packet_NULL_p,        \
                                   (a),(char *)0)
#define mkDPIset(a,b,c,d)     mkDPIset_packet(                        \
                                   snmp_dpi_set_packet_NULL_p,        \
                                   (a),(char *)0,(b),(c),(d))
#define mkDPIlist(a,b,c,d,e)  mkDPIset_packet((a),(b),(char *)0,(c),  \
                                   (d),(e))
#define query_DPI_port(a,b)   (int)qDPIport((a),(b),1)

#endif /* snmp_dp1H */

/*
\end{verbatim}
*/
