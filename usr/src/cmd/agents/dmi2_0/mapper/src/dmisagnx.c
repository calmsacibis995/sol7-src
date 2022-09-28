/* Copyright 10/11/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisagnx.c	1.11 96/10/11 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisagnx.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support DMI access.             */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        doGetNext(...)              */
/*                                        */
/*                                        gnParseInstance(...)        */
/*                                        gnFindNextObject(...)       */
/*                                        gnFindGroup(...)            */
/*                                        gnFindAttribute(...)        */
/*                                        gnFindComponent(...)        */
/*                                        gnFindKey(...)              */
/*                                        gnGetObject(...)            */
/*                                        endOfMibView(...)           */
/*                                                                    */
/* End Module Description *********************************************/
/*                                                                    */
/* Copyright **********************************************************/
/*                                                                    */
/* Copyright:                                                         */
/*   Licensed Materials - Property of IBM                             */
/*   This product contains "Restricted Materials of IBM"              */
/*   xxxx-xxx (C) Copyright IBM Corp. 1994.                           */
/*   All rights reserved.                                             */
/*   US Government Users Restricted Rights -                          */
/*   Use, duplication or disclosure restricted by GSA ADP Schedule    */
/*   Contract with IBM Corp.                                          */
/*   See IBM Copyright Instructions.                                  */
/*                                                                    */
/* End Copyright ******************************************************/
/*                                                                    */
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  941214    LAUBLI    New module                    */
/*                  950317    LAUBLI    Added malloc check            */
/*                                                                    */
/* End Change Log *****************************************************/

#include "dmisa.h"     /* global header file for DMI Sub-agent        */

#ifdef SOLARIS2
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pdu.h>
#include <trap.h>
#include <pagent.h>
#include <dpitosar.h>
extern int socket_handle;
extern int TraceLevel;
extern Address address;
extern char error_label[1000];
#endif

#define DOT "."

struct _getnextflags{
   short int neededgroup;      /* Which group within its component? First, next, or this one*/
   short int neededattribute;  /* Which attribute within this group? First, next, or this one*/
   short int neededcomponent;  /* Which component? First, next, or this one*/
   short int neededkey;        /* Which key (ie, row) within this group? Lexicographically-*/
                               /* lowest or lexicographically-next    */
   short int foundanextcomponent;  /* Flag to indicate whether the valid component ID*/
                                   /* is contained in the OID or in thisgcpair, since*/
                   /* thisgcpair may point to a component lower than that in the OID.*/
};

void       traceReqBuf(DMI_MgmtCommand_t *mgmtreq);
static int gnGetNextMifObject( snmp_dpi_set_packet **ppvarbind,
                  snmp_dpi_next_packet *pack, OidPrefix_t *xlatelist);
static int gnParseInstance(char *instance, ULONG *groupi,
                  ULONG *attributei, ULONG *compi, char **ppkeystring,
                  struct _getnextflags *gnflag);
static int gnFindNextObject(OidPrefix_t *xlatelist,
                  snmp_dpi_next_packet *pack, ULONG compi,
                  ULONG groupi, ULONG *attributei, char *keystring,
                  ULONG *pkeyblocklen, DMI_GroupKeyData_t **ppkeyblock,
                  GCPair_t **rtngcpair, struct _getnextflags *gnflag);
static int gnFindGroup(OidPrefix_t *xlatelist, snmp_dpi_next_packet *pack,
                  GCPair_t **gcpairseq, GCPair_t **thisgcpair,
                  ULONG *groupi, struct _getnextflags *gnflag, int *sequentialKey);
static int gnFindAttribute(GCPair_t *pgcpair, GCPair_t **thisgcpair,
                  ULONG *attributei, struct _getnextflags *gnflag);
static int gnFindComponent(GCPair_t *pgcpair, GCPair_t **thisgcpair,
                  ULONG *attributei, ULONG compi,
                  struct _getnextflags *gnflag);
static int gnFindKey(GCPair_t *thisgcpair, char *keystring,
                  ULONG *keyblocklen, DMI_GroupKeyData_t **ppkeyblock,
                  struct _getnextflags *gnflag, int sequentialKey);
static int gnGetObject(snmp_dpi_set_packet **ppvarbind,
                  snmp_dpi_next_packet *pack,
                  GCPair_t *targetgcpair, ULONG attributei,
                  ULONG keyblocklen, DMI_GroupKeyData_t *keyblock);
static int  endOfMibView(snmp_dpi_set_packet **ppvarbind,
                  snmp_dpi_next_packet *ppack);

extern char logbuffer[];
#ifdef DMISA_TRACE
extern unsigned char logbuffer[];
#endif

/**********************************************************************/
/*                                                                    */
/* MIF/COMPONENT ASSUMPTIONS:                                         */
/*                                                                    */
/*   1. Due to an SNMP restriction, all attributes of integer, gauge, */
/*      or counter type used in a key shall have non-negative values, */
/*      to be observable by SNMP.  If the sub-agent receives          */
/*      a request for a row and the key attribute value is a negative */
/*      integer, the sub-agent responds with noSuchInstance.          */
/*      (Ref: RFC 1212, p.9; RFC 1448, p.29)                          */
/*                                                                    */
/*   2. Key values will have an OID representation short enough       */
/*      so that, when combined with the OID prefix and remainder      */
/*      of the instance, it contains not more than 128 sub-id's       */
/*      (i.e., the SNMP v.2 maximum).                                 */
/*                                                                    */
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/* MIF2MIB ASSUMPTIONS:                                               */
/*                                                                    */
/*   1. Attributes of DMI type "date" are translated into             */
/*      fixed-length strings, not variable-length.  This is           */
/*      This is required for the current algorithm to complete        */
/*      an SNMP operation correctly when an attribute of this         */
/*      DMI type is used in a key.                                    */
/*                                                                    */
/*   2. Attributes of DMI type "integer64" are translated into        */
/*      type counter64 or 8-byte OCTETSTRINGs.  This means the        */
/*      attribute cannot be used to index into a DMI table in either  */
/*      SNMP v.1 and v.2.                                             */
/*                                                                    */
/*   3. All strings of DMI types "displaystring" and "octetstring"    */
/*      are translated into variable-length strings, not fixed-length.*/
/*      This is required for the current algorithm to complete        */
/*      an SNMP operation correctly when an attribute of these        */
/*      DMI types is used in a key.                                   */
/*                                                                    */
/*   4. An attribute of counter64 type used as a key aborts the       */
/*      MIB generation process, causing MIF2MIB to fail.              */
/*      (Reason: On a get of such a DMI table, the SNMP manager would */
/*      use the MIB INDEX clause to determine which columns, i.e.,    */
/*      attributes, comprise the index.  However, there is no way     */
/*      for it to put a Counter64 into the index portion of the OID.  */
/*      Ref: RFC 1442, p.29.)                                         */
/*      To complete handling this, probably the sub-agent should      */
/*      cause such a table to be invisible to SNMP, i.e., no Gets,    */
/*      no GetNexts, no Sets.  It currently does not.                 */
/*                                                                    */
/**********************************************************************/
/**********************************************************************/
/*                                                                    */
/* SNMP DEVIATIONS:                                                   */
/*                                                                    */
/*   1. On GETNEXT, in certain cases when the value of a particular   */
/*      object cannot be retrieved, a noError/NULLvalue is returned   */
/*      rather than the genErr prescribed by RFC 1442.                */
/*                                                                    */
/*   2. noSuchInstance is returned in cases where noSuchName          */
/*      should be returned.  This occurs when an attribute within     */
/*      a given group (i.e., an SNMP column) is not found, and        */
/*      no another group under this OID prefix (i.e., SNMP instance)  */
/*      contains the same attribute ID.                               */
/*      The current sub-agent design, does not know whether another   */
/*      instance exists, so it assumes it does.                       */
/*      (See note at bottom of DpiGet prolog.)                        */
/*      Reason: Too expensive to implement the SNMP specification.    */
/*                                                                    */
/*   3. If an attribute is missing from a MIF/MIB (e.g.,              */
/*      customer removed it), the sub-agent cannot report             */
/*      even an empty string to the manager to make his view          */
/*      of the MIB complete.  Rather, noSuchInstance is returned.     */
/*      Reason: Impossible to complete the MIB, but it is unexpected  */
/*              that users will alter the portions of the MIF that    */
/*              comprise MIB definition.                              */
/*                                                                    */
/**********************************************************************/
/**********************************************************************/
/*  Function: doGetNext()                                             */
/*                                                                    */
/*  Input parameters:                                                 */
/*   ptr. to OID prefix                                               */
/*   ptr. to OID instance                                             */
/*   ptr. to XlateList                                                */
/*                                                                    */
/*  Output parameters:                                                */
/*   Packet to send back to SNMP                                      */
/*   return code: 0 = success                                         */
/*                x = nnnnn                                           */
/*                                                                    */
/*  Description:                                                      */
/*                                                                    */
/*  Overview:                                                         */
/*  1. If the given OID is not fully specified, complete its          */
/*     specification (to a "leaf")                                    */
/*  2. Once the OID is fully specified, determine the lexicograph-    */
/*     ically next object OID.  (This is the fully-specified OID,     */
/*     if the OID was not fully specified.)                           */
/*  3. Get the identified object's value and return the value and OID */
/*     to the agent, unless the OID is beyond the highest attribute in*/
/*     in the highest group ID accessible under this OID prefix,      */
/*     in which case, return "endOfMibView."                          */
/*  Note: Each unique OID prefix found in .MAP files is individually  */
/*        registered with DPI.                                        */
/*                                                                    */
/*  Details:                                                          */
/*  1. ENSURE COMPLETE SPECIFICATION:                                 */
/*   a)Confirm the OID prefix is a prefix present in XlateList.       */
/*     If not, return a DPI SNMP value type of endOfMibView.          */
/*   b)If the group ID is not present in the OID,                     */
/*     find the lowest group ID under this OID prefix in XlateList.   */
/*     If none is found, return endOfMibView.                         */
/*   c)If the sub-id following group ID is present in the OID:        */
/*       - If zero, set it to "1" and set attribute ID, component ID, */
/*         and key count to zero.                                     */
/*       - If one, continue.                                          */
/*       - If greater than one, set it to "1", set attribute ID and   */
/*         component ID to zero and set key to NULL, and go to        */
/*         step (2e).                                                 */
/*       - Otherwise, do same as if this sub-id were zero.            */
/*   d)If the attribute ID is not present in the OID, then find the   */
/*     lowest attribute within all components containing the current  */
/*     group ID under this OID prefix in XlateList. (Uses DMI access.)*/
/*     If none is found, set attribute ID and component ID to zero,   */
/*     set the key to NULL, and go to step (2e).                      */
/*   e)If the component ID is not present in the OID, then find the   */
/*     lowest component containing the current group ID for those     */
/*     listed under this OID prefix in XlateList.                     */
/*     If none is found, set component ID to zero and key to NULL, and*/
/*     go to step (2d).                                               */
/*   f)If a key is present in the OID, translate it (using KeyFromDpi)*/
/*   g)If a key is not present in the OID, yet a key exists           */
/*     in the MIF database, extract the lexicographically lowest key  */
/*     value (using GetKeyValue).                                     */
/*                                                                    */
/*  2. WITH A COMPLETELY SPECIFIED OID, GET & RETURN NEXT OBJECT:     */
/*   a)If a component ID was found in step (1e) or a non-null key was */
/*     found in step (1g), the next object has been reached.          */
/*     If a key does not exist in the group/component ID pair, return */
/*     a NULL key in the OID.  Get the OID value (using GetAttribute) */
/*     and go to step 3.                                              */
/*   b)Do this step only if a key exists in the MIF database          */
/*     for this group/component ID pair.                              */
/*      - Pull all key values for the DMI table and sort them         */
/*        lexicographically (using GetKeyValue).                      */
/*      - If at least one key value exists beyond the                 */
/*        partially- or fully-specified key value in the OID,         */
/*        get the attribute value of that row, and                    */
/*        go to step 3.                                               */
/*   c)Search for the next-higher component ID:                       */
/*      - containing the current group ID and                         */
/*      - containing the current attribute ID.                        */
/*     If found,                                                      */
/*      - if a key exists in the MIF database for that group,         */
/*        extract the lexicographically lowest key value              */
/*        (using GetKeyValue)                                         */
/*      - go to step 3.                                               */
/*   d)Search for the next-higher attribute ID:                       */
/*      - within the current OID prefix,                              */
/*      - with the current group ID,                                  */
/*      - yet across all component ID containing this group ID.       */
/*     If found,                                                      */
/*      - determine the lowest component ID in which it exists,       */
/*      - if a key exists in the MIF database for that group,         */
/*        extract the lexicographically lowest key value              */
/*        (using GetKeyValue)                                         */
/*      - go to step 3.                                               */
/*   e)Search for the next-higher group ID:                           */
/*      - under the current prefix OID,                               */
/*      - having the lowest attribute ID of all components containing */
/*        this group,                                                 */
/*      - having the lowest component ID containing the current group */
/*        and attribute IDs.                                          */
/*     If found:                                                      */
/*      - if a key exists in the MIF database for that group,         */
/*        extract the lexicographically lowest key value              */
/*        (using GetKeyValue)                                         */
/*      - go to step 3.                                               */
/*   f)Return the received OID with value type endOfMibView to agent. */
/*                                                                    */
/*  3. GET THE OBJECT VALUE & RETURN IT:                              */
/*   a)Get the OID value of this attribute, identified as the next one*/
/*     (using GetAttribute). On return from GetAttribute, if iStatus: */
/*                                      v2 Error-status / Value-type  */
/*                                      ----------------------------  */
/*   = (000) Success,                   return noError & attrib. value*/
/*     (If type is INTEGER64, then change type to OCTETSTRING, 8-byte)*/
/*     (If too long, return tooBig)                                   */
/*   = (001) More data is available,    return tooBig  & empty value  */
/*                                 because bufferSize > maxDpiPpuSize */
/*   = (100) Attribute not found,       return genErr                 */
/*   = (101) Value exceeds maximum size,return genErr                 */
/*   = (102) Component instru.not found,return genErr                 */
/*   = (103) Enumeration error,         return genErr                 */
/*   = (104) Group not found,           return genErr                 */
/*   = (105) Illegal keys,              return noError/NULL string  ge*/
/*   = (106) Illegal to set,            return genErr                 */
/*   = (107) Can't resolve attribute function name, return noErr/NULge*/
/*   = (108) Illegal to get,            return genErr                 */
/*    (except if DMI access is write-only, return noError/NULL string)*/
/*   = (109) No description,            return genErr                 */
/*   = (10a) Row not found,             return genErr                 */
/*   = (10b) Direct i/f not registered, return noError/NULL string    */
/*   = (10c) MIF database is corrupt,   return noError/NULL string  ge*/
/*   = (10d) Attribute is not supported,return noError/NULL string    */
/*   = (200) Buffer full                return noError/NULL string  ge*/
/*   = (1001) Overlay not found         return noError/NULL string  ge*/
/*   = (10004) Component get error      return genErr                 */
/*   = (10005) Component key error      return noError/NULL string    */
/*   = (else)(01xxx)other Overlay Mgr,  return noError/NULL string  ge*/
/*   = (else)(1xxxx)other Component err,return noError/NULL string  ge*/
/*   = (else)any other error            return genErr                 */
/*     Determine if any action should be taken within DMI sub-agent if*/
/*     any of these errors are detected. If 0x0203, then reinitialize.*/
/*     Note: As of 9/94, the Intel implementation of the Service Layer*/
/*           should never show any iStatus but the following          */
/*           on issueGetAttribute: 0x0, 0x1, 0x100, 0x105, 0x107,     */
/*           0x108, 0x10b, 0x10c, 0x200, 0x203, 0x204, 0x205, 0x206,  */
/*           0x208, 0x211, 0x1000, 0x1001, 0x1002, 0x1101, 0x2000,    */
/*           0x3004, 0x3005, 0x3006, 0x10004, and 0x10005.            */
/*   b)If the attribute is in a table, use KeyToDpi to translate key  */
/*     to an SNMP index.                                              */
/*   c)Return the attribute value and its OID to the agent.           */
/*                                                                    */
/* Return codes:  DO_GET_NEXT_noError                                 */
/*                DO_GET_NEXT_dpiBroke                                */
/*                GN_GET_NEXT_MIF_OBJECT_dpiBroke                     */
/*                END_OF_MIB_VIEW_dpiBroke                            */
/*                GN_PARSE_INSTANCE_badOid                           */
/*                GN_FIND_NEXT_ATTRIBUTE_dmiBroke                     */
/*                GN_FIND_NEXT_COMPONENT_dmiBroke                     */
/*                GN_GET_OBJECT_genErr                                */
/*                GN_GET_OBJECT_dmiBroke                              */
/*                GN_GET_OBJECT_dpiBroke                              */
/*                GN_GET_OBJECT_outOfMemory                           */
/*                KEY_FROM_DPI_illegalKey  - Illegal key              */
/*                KEY_FROM_DPI_otherError  - key translation error    */
/*                KEY_FROM_DPI_outOfMemory - error occurred in malloc */
/*                GET_KEY_VALUE_otherError                            */
/*                GET_KEY_VALUE_outOfMemory                           */
/*                PULL_KEY_VALUES_otherError                          */
/*                PULL_KEY_VALUES_dmiBroke                            */
/*                PULL_KEY_VALUES_outOfMemory                         */
/*                TRANSLATE_DMI_KEY_TO_INTERNAL_otherError            */
/*                GET_DMI_KEY_SIZE_badAttributeType                   */
/*                KEY_TO_DPI_otherError  - key translation error      */
/*                KEY_TO_DPI_outOfMemory - error occurred from malloc */
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                   */
/*                ISSUE_GET_ATTRIBUTE_outOfMemory                    */
/*                PREPARE_FOR_DMI_INVOKE_resetSemFailed              */
/**********************************************************************/
int doGetNext(snmp_dpi_hdr *phdr, snmp_dpi_next_packet *pack, OidPrefix_t *xlatelist)
{
   snmp_dpi_set_packet *pvarbind = snmp_dpi_set_packet_NULL_p; /* initialize varBind chain*/
   unsigned char       *ppacket;
   int                  i, rc, rcdpi;
   int                  generr = DMISA_FALSE, toobig = DMISA_FALSE;
   int                  nosuchName = DMISA_FALSE;
   long int             error;
#ifdef DMISA_TRACE
   int                  ln;
#endif
#ifdef SOLARIS2
   SNMP_pdu   *pdu;
#endif


   /* Process Get-Next on an OID (Could adjust this loop to support multi var-bind PDUs)*/
   for (i=1;pack/*i<=1*/;i++) {
#ifdef DMISA_TRACE
      ln = sprintf(logbuffer,"Get next OID following ");
      strncpy(logbuffer + ln, pack->object_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      DMISA_TRACE_LOGBUF(LEVEL2);
#endif
#if 0
      if (!strcmp(pack->group_p,SUN_ARCH_DMI_COMPMIB)) {
      if (!strncmp(pack->object_p,SUN_ARCH_DMI_COMPMIB,
#endif
      if (!strncmp(pack->instance_p,"1.1",
                         3/*strlen(SUN_ARCH_DMI_COMPMIB)*/)) {
                         /* Agent always gives full prefix of registered point,*/
                         /* even when OID sent by manager does not.   */
         rc = dpiGetNextCompMib(  /* operation for Component MIB      */
                 &pvarbind,
                 pack);  /* be able to handle NULL OID instance string*/
         if (rc != GET_NEXT_COMP_MIB_noError) {
            if (rc == DMISA_ERROR_endOfMibView) {
               rc = endOfMibView(&pvarbind, pack);  /* Return endOfMibView*/
               if (rc == END_OF_MIB_VIEW_dpiBroke) return DO_GET_NEXT_dpiBroke;
               nosuchName = DMISA_TRUE;
               break;
               /*continue; */
            } else if (rc == GET_NEXT_COMP_MIB_genErr) {
               generr = DMISA_TRUE;
               break;
            } else if (rc == GET_NEXT_COMP_MIB_tooBig) {
               toobig = DMISA_TRUE;
               break;
            }
         }
      } else {
         rc = gnGetNextMifObject(
                 &pvarbind,
                 pack,
                 xlatelist);
         if (rc != GN_GET_NEXT_MIF_OBJECT_gottit) {  /* if parse not successful*/
            if (rc == DMISA_ERROR_endOfMibView) {
               rc = endOfMibView(&pvarbind, pack);  /* Return endOfMibView*/
               if (rc == END_OF_MIB_VIEW_dpiBroke) return DO_GET_NEXT_dpiBroke;
               nosuchName = DMISA_TRUE;
               break;
               /*continue; */
            } else if ((DMISA_ERROR_dmiBroke == rc % 10) ||
                       rc == GN_PARSE_INSTANCE_badOid) {
               generr = DMISA_TRUE;
               break;
            } else if (rc == GN_GET_OBJECT_tooBig) {
               toobig = DMISA_TRUE;
               break;
            }
            return rc;  /* to cover programming error case            */
         }
      }
      pack = pack->next_p;
   } /* endfor */
  
   if (nosuchName) {
      error = SNMP_ERROR_noSuchName;
   } else if (generr) {
      error = SNMP_ERROR_genErr;
   } else if (toobig) {
      error = SNMP_ERROR_tooBig;
   } else {
      error = SNMP_ERROR_noError;
      i = 0;           /* Index of object with error is 0, since no error*/
   }
#ifndef SOLARIS2
   ppacket = mkDPIresponse(                /* Make DPIresponse packet */
           phdr,                           /* ptr parsed request      */
           error,                          /* error code              */
           i,                              /* index of object w/error */
           pvarbind);                      /* varBind response data   */
   if (!ppacket) {
      DMISA_TRACE_LOG(LEVEL1,"mkDPIresponse failed in doGet function.");
      return DO_GET_NEXT_dpiBroke;      /* Return if failed   */
   }

   rcdpi = DPIsend_packet_to_agent(     /* send OPEN packet        */
           DpiHandle,                   /* on this connection      */
           ppacket,                     /* this is the packet      */
           DPI_PACKET_LEN(ppacket));    /* and this is its length  */
   if (rcdpi != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG(LEVEL1,"DPISend_packet_to_agent failed in doGet function.");
      return DO_GET_NEXT_dpiBroke;      /* Return if failed   */
   }
#else
      pdu = dpihdr_to_pdu(phdr, pvarbind, error, i);
      if (!pdu) {
     DMISA_TRACE_LOG(LEVEL1,"dpihdr_to_pdu failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed   */
      }

      if (TraceLevel == 5) {
      printf("GetNext Response PDU.................\n");
      trace_snmp_pdu(pdu);
      }


      if (snmp_pdu_send(socket_handle, &address, pdu, error_label) == -1)  {
     DMISA_TRACE_LOG(LEVEL1,"snmp_pdu_send failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed  */
      }

      snmp_pdu_free(pdu);
      if (pvarbind) fDPIset_packet(pvarbind);

#endif
   if (generr) return rc;

   return DO_GET_NEXT_noError;
}


/*********************************************************************/
/* Function:  gnGetNextMifObject()   -  Get Next MIF object          */
/*    (i.e, the lexicographically next object from the MIF database) */
/*                                                                   */
/* Return codes:  GN_GET_NEXT_MIF_OBJECT_gottit                      */
/*                GN_GET_NEXT_MIF_OBJECT_dpiBroke                    */
/*                DMISA_ERROR_endOfMibView                           */
/*                GN_PARSE_INSTANCE_badOid                           */
/*                GN_FIND_NEXT_ATTRIBUTE_dmiBroke                    */
/*                GN_FIND_NEXT_COMPONENT_dmiBroke                    */
/*                GN_GET_OBJECT_tooBig                               */
/*                GN_GET_OBJECT_genErr    \__combine?                */
/*                GN_GET_OBJECT_dmiBroke  /                          */
/*                GN_GET_OBJECT_dpiBroke  /                          */
/*                GN_GET_OBJECT_outOfMemory                          */
/*                KEY_FROM_DPI_illegalKey  - Illegal key             */
/*                KEY_FROM_DPI_otherError  - key translation error   */
/*                KEY_FROM_DPI_outOfMemory - error occurred in malloc*/
/*                GET_KEY_VALUE_otherError                           */
/*                GET_KEY_VALUE_outOfMemory                          */
/*                PULL_KEY_VALUES_otherError                         */
/*                PULL_KEY_VALUES_dmiBroke                           */
/*                PULL_KEY_VALUES_outOfMemory                        */
/*                TRANSLATE_DMI_KEY_TO_INTERNAL_otherError           */
/*                GET_DMI_KEY_SIZE_badAttributeType                  */
/*                KEY_TO_DPI_otherError  - key translation error     */
/*                KEY_TO_DPI_outOfMemory - error occurred from malloc*/
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                   */
/*                ISSUE_GET_ATTRIBUTE_outOfMemory                    */
/*                PREPARE_FOR_DMI_INVOKE_resetSemFailed              */
/*********************************************************************/
static int gnGetNextMifObject( snmp_dpi_set_packet **ppvarbind,
                     snmp_dpi_next_packet *pack, OidPrefix_t *xlatelist)
{
   DMI_GroupKeyData_t  *keyblock  = NULL;   /* Initialization necessary?*/
   char                *keystring = NULL;   /* initialization necessary?*/
   GCPair_t            *targetgcpair;
   int   rc;
   ULONG keyblocklen;
   ULONG compi, groupi, attributei; /* Used to get target component, group & attribute IDs*/
                                                     /* Possible get_next flag values:*/
   struct _getnextflags getflags = { DMISA_FIRST,    /*  First, Next, This*/
                                     DMISA_FIRST,    /*  First, Next, This*/
                                     DMISA_FIRST,    /*  First, Next, This*/
                                     DMISA_LOWEST,   /*  Lowest, Next, This*/
                                     DMISA_TRUE };  /* foundacomp value: TRUE or FALSE THIS IS CHANGED is was DMISA_FALSE, as in SNM componentID will always have to be retrieved from the GC pair and will not be available the OID*/
   struct _getnextflags *gnflag = &getflags;

   rc = gnParseInstance(
           pack->instance_p,
           &groupi,
           &attributei,
           &compi,
           &keystring,
           gnflag);
   if (rc != GN_PARSE_INSTANCE_parseOk) {  /* if parse not successful */
      return rc;
   }

   rc = gnFindNextObject(
           xlatelist,
           pack,
           compi,          /* Input component ID                      */
           groupi,         /* Input group ID                          */
           &attributei,    /* Input/output attribute ID               */
           keystring,      /* Input key                               */
           &keyblocklen,
           &keyblock,      /* Output key - allocated but not freed, must be freed*/
           &targetgcpair,  /* Output group ID, component ID, and keycount*/
           gnflag);
   if (rc != GN_FIND_NEXT_OBJECT_found) {  /* if search not successful*/
      if (keyblock) free(keyblock);
      return rc;
   }

   DMISA_TRACE_LOG3(LEVEL2,"Get object: g=%ul a=%ul c=%ul.\n", groupi, attributei, compi);
   rc = gnGetObject(
           ppvarbind,
           pack,
           targetgcpair,
           attributei,
           keyblocklen,
           keyblock);
   if (keyblock) free(keyblock);     /* Allocated by keyFromDpi       */
   if (rc != GN_GET_OBJECT_getOk) {  /* if get not successful         */
      DMISA_TRACE_LOG1(LEVEL2, "Error on GETNEXT operation: return code = %d.", rc);
      return rc;
   }

   return GN_GET_NEXT_MIF_OBJECT_gottit;
}


/*********************************************************************/
/* Function:  gnParseInstance()                                      */
/*                                                                   */
/* Description:                                                      */
/* Parse the OID instance for the Get operation.                     */
/*                                                                   */
/* Return codes:  GN_PARSE_INSTANCE_parseOk                          */
/*                GN_PARSE_INSTANCE_badOid                           */
/*                DMISA_ERROR_endOfMibView                           */
/*********************************************************************/
static int gnParseInstance(char *instance, ULONG *groupi,
                      ULONG *attributei, ULONG *compi, char **ppkeystring,
                      struct _getnextflags *gnflag)
{
   int   i, j;
   char *restofit;
   char *keystring = NULL;
   ULONG uli;

   *ppkeystring = NULL;                        /* initialize          */

   /* Parse OID instance                                              */
   errno = 0;   /* initialize                                         */
   if (!instance) return GN_PARSE_INSTANCE_parseOk;  /* no instance, so get 1st object*/

   if (!isdigit(*instance)) return GN_PARSE_INSTANCE_badOid; /* if 1st char not digit*/
   for (i=1, j=0; ; i++) {
      uli = strtoul(instance + j, &restofit, 10);
      if (errno) return GN_PARSE_INSTANCE_badOid;  /* could be ERANGE or EDOM*/
      j = restofit - instance;

      switch (i) {
      case 1:
         /* there is a 1 preceding G in the OID of every object       */
         if (uli > 1) return DMISA_ERROR_endOfMibView;
         if (uli < 1) {
            gnflag->neededgroup = DMISA_FIRST;
            i = 5;
         }
         break;
      case 2:
         *groupi = uli;
         gnflag->neededgroup = DMISA_THIS;  /* Either this component or the next one*/
         break;
      case 3:
         if (uli > 1) {
            gnflag->neededgroup = DMISA_NEXT; /* look for object in next group*/
            /* needed attribute & component already initialized to DMISA_FIRST*/
            i = 5;
         }
              /* there is a 1 between G and A in the OID of every object*/
         if (uli < 1) i = 5; /* So no more extraction is performed    */
            /* needed attribute & component already initialized to DMISA_FIRST*/
         break;
      case 4:
         *attributei = uli;
         gnflag->neededattribute = DMISA_THIS;  /* Either this component or the next one*/
         break;
      case 5:
         *compi = uli;
         gnflag->neededcomponent = DMISA_THIS;  /* Either this component or the next one*/
         gnflag->foundanextcomponent= DMISA_FALSE;
                    /* can be updated to NEXT by TranslateKey function*/
         if ((*restofit != '\0') && (*(restofit + 1) != '\0')) /* $MED */
            keystring = restofit + 1;              /* Increment to get past the dot*/
         break;
      default:
         break;                                 /* Used to validate key in dotted-decimal format*/
      }

      if (*(instance + j) == '\0') break;
      if (*(instance + j++) != '.') return GN_PARSE_INSTANCE_badOid;  /* if no dot*/
      if (!isdigit(*(instance + j))) return GN_PARSE_INSTANCE_badOid; /* if not digit*/
   }

   if (gnflag->neededcomponent == DMISA_THIS && keystring != NULL) {
      *ppkeystring = keystring;        /* Point to the to key portion of the OID*/
      gnflag->neededkey = DMISA_NEXT;
   }
   return GN_PARSE_INSTANCE_parseOk;
}


/*********************************************************************/
/* Function:  gnFindNextObject()                                     */
/*                                                                   */
/* Notes:                                                            */
/*   1.  *ppkeyblock points to storage allocated by a subfunction    */
/*       but not freed.  Ensure it Is freed after no longer needed.  */
/*                                                                   */
/* Return codes:  GN_FIND_NEXT_OBJECT_found                          */
/*                GN_FIND_NEXT_ATTRIBUTE_dmiBroke                    */
/*                GN_FIND_NEXT_COMPONENT_dmiBroke                    */
/*                DMISA_ERROR_endOfMibView                           */
/*                KEY_FROM_DPI_illegalKey  - Illegal key             */
/*                KEY_FROM_DPI_otherError  - key translation error   */
/*                KEY_FROM_DPI_outOfMemory - error occurred in malloc*/
/*                GET_KEY_VALUE_otherError                           */
/*                GET_KEY_VALUE_outOfMemory                          */
/*                PULL_KEY_VALUES_otherError                         */
/*                PULL_KEY_VALUES_dmiBroke                           */
/*                PULL_KEY_VALUES_outOfMemory                        */
/*                TRANSLATE_DMI_KEY_TO_INTERNAL_otherError           */
/*                GET_DMI_KEY_SIZE_badAttributeType                  */
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                   */
/*                PREPARE_FOR_DMI_INVOKE_resetSemFailed              */
/*********************************************************************/
static int gnFindNextObject(OidPrefix_t *xlatelist,
                   snmp_dpi_next_packet *pack, ULONG compi,
                   ULONG groupi, ULONG *attributei, char *keystring,
                   ULONG *pkeyblocklen, DMI_GroupKeyData_t **ppkeyblock,
                   GCPair_t **thisgcpair,
                   struct _getnextflags *gnflag)
{                                      /* Keep on looking for the next object*/
   GCPair_t  *gcpairseq = DMISA_NULL;
   int        rc, loopcount = 0;
   short int  ctemp;
   int sequentialKey;

   *thisgcpair = DMISA_NULL;
   while (!gcpairseq ||
              /* If we haven't started yet or                         */
          gnflag->neededgroup != DMISA_THIS ||
              /* If this group ID is not part of the next object's OID or*/
          gnflag->neededattribute != DMISA_THIS ||
              /* If this attribute ID is not part of the next object's OID or*/
          gnflag->neededcomponent != DMISA_THIS ||
              /* If this component ID is not part of the next object's OID or*/
          gnflag->neededkey != DMISA_THIS && (*thisgcpair)->iKeyCount ) {
              /* If this key value is not part of the next object's OID or*/

      /* Get 1st or next group from XlateList under the current OID prefix*/
      /* or initialize by finding the appropriate Group/Component pair*/
      loopcount++;
      if (gnflag->neededgroup != DMISA_THIS || !gcpairseq) {
         rc = gnFindGroup(
                 xlatelist,
                 pack,
                 &gcpairseq,    /* Ptr. to 1st GCPair of the sublist of pairs,*/
                                /* all of which contain the same group ID*/
                 thisgcpair,    /* Initialized to gcpairseq, but will be refined*/
                                /* to the one which contains both next group & component*/
                                /* by gnFindAttribute and gnFindComponent*/
                 &groupi,
                 gnflag,
                 &sequentialKey);
         if (rc == GN_FIND_GROUP_notFound) {  /* If search not successful*/
            return DMISA_ERROR_endOfMibView;
         } else if (rc == GN_FIND_GROUP_found) {
            gnflag->neededgroup     = DMISA_THIS;  /* Got the next group ID*/
            gnflag->neededattribute = DMISA_FIRST;
            gnflag->neededcomponent = DMISA_FIRST;
            gnflag->neededkey       = DMISA_LOWEST;
         } else if (rc == GN_FIND_GROUP_foundNoChange) {
            if ((!gnflag->foundanextcomponent) && (*thisgcpair)->iCompId > compi) {   /* assumed that neededgroup = THIS*/
               gnflag->neededcomponent     = DMISA_THIS; /* Got next component ID*/
               gnflag->foundanextcomponent = DMISA_TRUE; /* Found a next component*/
               gnflag->neededkey           = DMISA_LOWEST; /* Need to get lowest key*/
            }
         } /* else ignore the no_change case                          */
      }

      /* Get next attribute from XlateList if one exists w/ current groupID and this OID prefix*/
      ctemp = gnflag->neededcomponent;
      if (gnflag->neededattribute != DMISA_THIS) {
         rc = gnFindAttribute(
                 gcpairseq,
                 thisgcpair,  /* Contains returned component ID       */
                 attributei,
                 gnflag);
         if (rc == GN_FIND_ATTRIBUTE_notFound) {  /* If search not successful*/
            gnflag->neededgroup     = DMISA_NEXT;  /* Need next group ID*/
            gnflag->neededattribute = DMISA_FIRST;
            gnflag->neededcomponent = DMISA_FIRST;
            gnflag->neededkey       = DMISA_LOWEST;
            continue;  /* get next group;                             */
         } else if (rc == GN_FIND_ATTRIBUTE_found) {
            gnflag->neededattribute     = DMISA_THIS; /* Got next attribute ID*/
            gnflag->neededcomponent     = DMISA_THIS; /* and next component ID*/
            gnflag->foundanextcomponent = DMISA_TRUE; /* Found a next component*/
            gnflag->neededkey           = DMISA_LOWEST;
         } else {
            return rc; /* DMI_broke case                              */
         }
      }

      /* Get next component from XlateList if one exists w/ current groupID and OID prefix*/
      if (gnflag->neededcomponent != DMISA_THIS ||
          (!gnflag->foundanextcomponent && (*thisgcpair)->iCompId < compi)) { /* get gcpair based off compi*/
         rc = gnFindComponent(
                 gcpairseq,
                 thisgcpair,
                 attributei,
                 compi,
                 gnflag);
         if (rc == GN_FIND_COMPONENT_notFound) {  /* If search not successful*/
            gnflag->neededattribute = DMISA_NEXT;  /* Need next attribute ID*/
            gnflag->neededcomponent = DMISA_FIRST;
            gnflag->neededkey       = DMISA_LOWEST;
            continue;  /* get next attribute;                         */
         } else if (rc == GN_FIND_COMPONENT_found ||
                    rc == GN_FIND_COMPONENT_foundNextAfterCompi) {
            gnflag->neededcomponent     = DMISA_THIS;  /* Got next component ID*/
            gnflag->foundanextcomponent = DMISA_TRUE;
                /* When this variable changes from false to true, the current component ID*/
                /* is no longer contained in the OID, but rather, in thisgcpair.*/
            gnflag->neededkey           = DMISA_LOWEST;
         } else if (rc == GN_FIND_COMPONENT_foundEqualToCompi) {
            gnflag->foundanextcomponent = DMISA_TRUE;
         } else {
            return rc; /* DMI_broke case                              */
         }
      }

      /* Get next key from GetKeyValue for the current Group/Component pair*/
      if (gnflag->neededkey != DMISA_THIS) {    /* && mykeycount) {   */
         rc = gnFindKey(
                 *thisgcpair,  /* get groupi & compi from here        */
                 keystring,
                 pkeyblocklen,
                 ppkeyblock,
                 gnflag,
                 sequentialKey);
         if (rc == GN_FIND_KEY_notFound  ||
             (rc == GN_FIND_KEY_noKeyExists &&
              loopcount == 1 && ctemp == DMISA_THIS)) {  /* if key entry not found*/
               /* or first time thru the loop, if no keys, and OID specified a component ID*/
            gnflag->neededcomponent = DMISA_NEXT;  /* Need next component ID*/
            gnflag->neededkey       = DMISA_LOWEST;
         } else if (rc == GN_FIND_KEY_found) {
            gnflag->neededkey       = DMISA_THIS;  /* Got next key instance*/
         } else if (rc != GN_FIND_KEY_noKeyExists) {
            return rc; /* DMI_broke case                              */
         }
      }
   } /* endwhile */
   return GN_FIND_NEXT_OBJECT_found;
}


/*********************************************************************/
/* Function:  gnFindGroup()                                          */
/*                                                                   */
/* Description:                                                      */
/* Get the first or next Group ID                                    */
/*                                                                   */
/* Input:  Ptr. to first GC pair of series with this group ID        */
/* Output: Ptr. to first GC pair of series with next-higher group ID,*/
/*         if found                                                  */
/*         New group ID                                              */
/*         Updated getnext flags                                     */
/*                                                                   */
/* Return codes:  GN_FIND_GROUP_found                                */
/*                GN_FIND_GROUP_notFound                             */
/*                GN_FIND_GROUP_foundNoChange (used 1st time thru)   */
/*********************************************************************/
static int gnFindGroup(OidPrefix_t *xlatelist, snmp_dpi_next_packet *pack,
                       GCPair_t **gcpairseq, GCPair_t **thisgcpair,
                       ULONG *groupi, struct _getnextflags *gnflag,
                       int *sequentialKey)
{
   ULONG g;

   if (!*gcpairseq) {  /* Find the GC pair we need                    */
      if (!pack->group_p) return GN_FIND_GROUP_notFound;
      while (xlatelist &&
             (strcmp(xlatelist->pOidPrefix, pack->group_p)) ) {
         xlatelist = xlatelist->pNextOidPre;  /* Position to matching OID prefix*/
      }
      if (!xlatelist) return GN_FIND_GROUP_notFound;  /* No match in OID list*/

      *sequentialKey = xlatelist->sequentialKeys;

      /* Find the group/component pair                                */
      *gcpairseq = xlatelist->pNextGCPair;  /* Position to first GCpair in the row*/
      if (gnflag->neededgroup == DMISA_FIRST &&
          *gcpairseq) {  /* return 1st GC pair if none was in OID     */
         *groupi = (*gcpairseq)->iGroupId;
         *thisgcpair = *gcpairseq;
         return GN_FIND_GROUP_found;
      }
      while (gnflag->neededgroup == DMISA_THIS &&
             *gcpairseq && (*gcpairseq)->iGroupId < *groupi ||  /* Stop at specified group ID*/
             gnflag->neededgroup == DMISA_NEXT &&
             *gcpairseq && (*gcpairseq)->iGroupId <= *groupi) { /* or at next group ID*/
         *gcpairseq = (*gcpairseq)->pNextGCPair;
      }
      if (!*gcpairseq) return GN_FIND_GROUP_notFound;  /* No more GC pairs*/
      if ((*gcpairseq)->iGroupId == *groupi) {
         *thisgcpair = *gcpairseq;
         return GN_FIND_GROUP_foundNoChange;  /* used only for init.  */
      }
   } else {    /* gnflag->neededgroup should never be DMISA_FIRST or DMISA_THIS*/
      g = (*gcpairseq)->iGroupId;
      while ((*gcpairseq) &&
             (*gcpairseq)->iGroupId <= g) {  /* move along to next higher group ID*/
         (*gcpairseq) = (*gcpairseq)->pNextGCPair;
      }
      if (!*gcpairseq) return GN_FIND_GROUP_notFound;
   }

   *groupi = (*gcpairseq)->iGroupId;
   *thisgcpair = *gcpairseq;
   return GN_FIND_GROUP_found;
}


/*********************************************************************/
/* Function:  gnFindAttribute()                                      */
/*                                                                   */
/* Description:                                                      */
/* Find the first or next Attribute ID (& comp.ID, while you're atit)*/
/*                                                                   */
/* Input:  Ptr. to first GCPair of series with this group ID         */
/*         Current attribute ID & component ID                       */
/* Output: New attribute & component ID, if found                    */
/*         Updated getnext flags                                     */
/*                                                                   */
/* Return codes:  GN_FIND_ATTRIBUTE_found                            */
/*                GN_FIND_ATTRIBUTE_notFound                         */
/*                GN_FIND_ATTRIBUTE_dmiBroke                         */
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                   */
/*********************************************************************/
static int gnFindAttribute(GCPair_t *gcpairseq, GCPair_t **thisgcpair,
                           ULONG *attributei,
                           struct _getnextflags *gnflag)
{
   DmiListAttributesOUT   *listattrout=NULL;
   DmiAttributeInfo_t     *attrinfo;
   GCPair_t      *temp;
   int           rc, i, gotattrib;
   short int     cmd = DMISA_FIRST;
   ULONG         g, dmistat, bestc = DMISA_MAXID, besta = 0UL, nexta = 1UL;

   nexta = *attributei;
   cmd   = gnflag->neededattribute;

   temp = gcpairseq;
   g = gcpairseq->iGroupId;
   while (gcpairseq && gcpairseq->iGroupId == g) {
            /* Look at GC pairs under this OID with this group ID     */
         rc = issueListAttribute(
                 &listattrout,
                 gcpairseq->iCompId,
                 g,  /* can use this variable, since all GCpairs here have same group ID*/
                 nexta,  /* attribute                                 */
                 cmd,
                 0);  /*All attributes in this group */

        if (rc != ISSUE_LIST_ATTRIBUTE_noError) {
             if (listattrout->error_status != DMIERR_SP_INACTIVE)
                  rc = GN_FIND_ATTRIBUTE_notFound;
             if (listattrout)
                free_listattrout(listattrout);

             if (rc == ISSUE_LIST_ATTRIBUTE_failed)
                 return GN_FIND_ATTRIBUTE_dmiBroke;
             else return rc;
      }

      attrinfo = listattrout->reply->list.list_val;
            gotattrib = DMISA_FALSE;
            for (i = 1; i <= listattrout->reply->list.list_len &&    /* While still in buffer*/
                             gnflag->neededattribute != DMISA_FIRST && /* and getting NEXT attr, not 1st*/
                             attrinfo->id <= *attributei;   /* (stops when we find next attr.)*/
                 i++) {  /* find the next attribute                   */
               attrinfo++;
            }
            if (i <= listattrout->reply->list.list_len && /* Not past end of buffer*/
                attrinfo->id  > *attributei &&    /* and we got a NEXT attr*/
                (attrinfo->id <= besta || besta == 0) ||  /* and it's less than the best one so far*/
                gnflag->neededattribute == DMISA_FIRST &&   /* or, we're getting the 1ST attr*/
                cmd == DMISA_FIRST) {

               if (attrinfo->id == besta) {
                  bestc = (gcpairseq->iCompId < bestc) ?
                          gcpairseq->iCompId : bestc;  /* minimum     */
                           /* get the lowest component ID for this attribute*/
               } else {
                  bestc = gcpairseq->iCompId;  /* get new component ID*/
               }
               besta = attrinfo->id;  /* get new attribute ID*/
               gotattrib = DMISA_TRUE;
            }
         /*nexta = pdmireqLA->iAttributeId; */
               /* get more attributes starting with the last one received*/
         /*cmd = DMISA_NEXT;*/  /* get more attributes from this group    */
         if (listattrout) {
            free_listattrout(listattrout);
            listattrout=NULL;
         }

      cmd = DMISA_FIRST;  /* get attributes of another group          */
      gcpairseq = gcpairseq->pNextGCPair;
   } /* endwhile */

   if (besta == *attributei && gnflag->neededattribute == DMISA_NEXT ||
       bestc == DMISA_MAXID) {
      return GN_FIND_ATTRIBUTE_notFound;
   }
   *attributei = besta;
   gcpairseq = temp;
   while (gcpairseq && gcpairseq->iGroupId == g &&
          gcpairseq->iCompId < bestc) {
      gcpairseq = gcpairseq->pNextGCPair;
   }
   *thisgcpair = gcpairseq;  /* update thisgcpair, also returns current component ID*/
   return GN_FIND_ATTRIBUTE_found;
}


/*********************************************************************/
/* Function:  gnFindComponent()                                      */
/*                                                                   */
/* Description:                                                      */
/* Find the first or next Component ID                               */
/*                                                                   */
/* Input:  Ptr. to current GCPair                                    */
/*         Current attribute ID & component ID                       */
/* Output: New component ID, if found                                */
/*         Updated getnext flags                                     */
/*                                                                   */
/* Return codes:  GN_FIND_COMPONENT_found                            */
/*                GN_FIND_COMPONENT_foundNextAfterCompi              */
/*                GN_FIND_COMPONENT_foundEqualToCompi                */
/*                GN_FIND_COMPONENT_notFound                         */
/*                GN_FIND_COMPONENT_dmiBroke                         */
/*                ISSUE_LIST_ATTRIBUTE_outOfMemory                   */
/*********************************************************************/
static int gnFindComponent(GCPair_t *gcpairseq, GCPair_t **thisgcpair,
                        ULONG *attributei, ULONG compi,
                        struct _getnextflags *gnflag)
{
   DmiListAttributesOUT  *listattrout=NULL;
   DmiAttributeInfo_t    *attrinfo;
   GCPair_t    *temp;
   int          i, rc, cmd = DMISA_FIRST, gottit = DMISA_FALSE;
   ULONG        g, dmistat, bestc = DMISA_MAXID, nexta = 1UL;

   temp = gcpairseq;
   g = gcpairseq->iGroupId;
   while (!gottit && gcpairseq && gcpairseq->iGroupId == g) {
            /* Look at GC pairs under this OID with this group ID     */
      if (!gnflag->foundanextcomponent &&
          gnflag->neededcomponent != DMISA_FIRST &&
          (*thisgcpair)->iCompId < compi) {        /* For special case where thisgcpair*/
         if (gcpairseq->iCompId > compi) {         /* needs to be changed before entering*/
            *thisgcpair = gcpairseq;               /* gnFindKey due to value of compi*/
            return GN_FIND_COMPONENT_foundNextAfterCompi;
         } else if (gcpairseq->iCompId == compi) {
            *thisgcpair = gcpairseq;
            return GN_FIND_COMPONENT_foundEqualToCompi;
         }
      } else if (gnflag->neededcomponent == DMISA_FIRST ||
          (!gnflag->foundanextcomponent && gcpairseq->iCompId > compi) ||
          ( gnflag->foundanextcomponent && gcpairseq->iCompId >
            (*thisgcpair)->iCompId ) ) {           /* For general case*/
            rc = issueListAttribute(
                    &listattrout,
                    gcpairseq->iCompId,
                    g,  /* can use this variable, since all GCpairs here have same group ID*/
                    nexta,  /* attribute                              */
                    cmd,
                    0); /*All the attributes under this group */
        if (rc != ISSUE_LIST_ATTRIBUTE_noError) {
          if (listattrout->error_status != DMIERR_SP_INACTIVE)
                  rc = GN_FIND_COMPONENT_notFound;
             if (listattrout)
                free_listattrout(listattrout);

             if (rc == ISSUE_LIST_ATTRIBUTE_failed)
                 return GN_FIND_COMPONENT_dmiBroke;
             else return rc;
      }
      attrinfo = listattrout->reply->list.list_val;


      for (i = 1; i <= listattrout->reply->list.list_len &&
                       attrinfo->id  < *attributei;
                    i++) {  /* find the next attribute                */
                  attrinfo++;
               }
               if (i <= listattrout->reply->list.list_len &&
                   attrinfo->id == *attributei) {
                  bestc = gcpairseq->iCompId;  /* since component ID's already ordered*/
                  gottit = DMISA_TRUE;
                  /* continue;  causes memory leak, eh?               */
               }
           /* nexta = pdmireqLA->iAttributeId; */
                  /* get more attributes starting with the last one received*/
            /*cmd = DMISA_NEXT;*/  /* get more attributes from this group */
            if (listattrout) {
              free_listattrout(listattrout);
              listattrout = NULL;
            }

      } /* endif */

      cmd = DMISA_FIRST;  /* get attributes of another group          */
      gcpairseq = gcpairseq->pNextGCPair;
   } /* endwhile */

   if (((!gnflag->foundanextcomponent && bestc == compi) ||
        ( gnflag->foundanextcomponent && bestc == (*thisgcpair)->iCompId)) &&
       gnflag->neededcomponent == DMISA_NEXT ||
       !gottit) {
      return GN_FIND_COMPONENT_notFound;
   }
   gcpairseq = temp;
   while (gcpairseq && gcpairseq->iGroupId == g &&
          gcpairseq->iCompId < bestc) {
      gcpairseq = gcpairseq->pNextGCPair;
   }
   *thisgcpair = gcpairseq;  /* update thisgcpair                     */
   return GN_FIND_COMPONENT_found;
}


/*********************************************************************/
/* Function:  gnFindKey()                                            */
/*                                                                   */
/* Description:                                                      */
/* Find the appropriate key, if one exists, for the next object      */
/*                                                                   */
/* Notes:                                                            */
/*   1.  *ppkeyblock points to storage allocated by a subfunction    */
/*       but not freed.  Ensure it Is freed after no longer needed.  */
/*                                                                   */
/* Return codes:  GN_FIND_KEY_found                                  */
/*                GN_FIND_KEY_notFound                               */
/*                GN_FIND_KEY_noKeyExists                            */
/*                KEY_FROM_DPI_illegalKey  - Illegal key             */
/*                KEY_FROM_DPI_otherError  - key translation error   */
/*                KEY_FROM_DPI_outOfMemory - error occurred in malloc*/
/*                GET_KEY_VALUE_otherError                           */
/*                GET_KEY_VALUE_outOfMemory                          */
/*                PULL_KEY_VALUES_otherError                         */
/*                PULL_KEY_VALUES_dmiBroke                           */
/*                PULL_KEY_VALUES_outOfMemory                        */
/*                TRANSLATE_DMI_KEY_TO_INTERNAL_otherError           */
/*                GET_DMI_KEY_SIZE_badAttributeType                  */
/*********************************************************************/
static int gnFindKey(GCPair_t *thisgcpair, char *keystring,
                  ULONG *keyblocklen, DMI_GroupKeyData_t **ppkeyblock,
                  struct _getnextflags *gnflag, int sequentialKey)
{
   DMI_GroupKeyData_t *pattern;
   int                 rc;

   *keyblocklen = 0;   /* for default case of no keys                 */

   if (!thisgcpair->iKeyCount) {  /* no key for this attribute,       */
      return GN_FIND_KEY_noKeyExists;
   }

   /* Translate key                                                   */
   if (gnflag->neededkey == DMISA_NEXT) { /* translate given key if one exists in the MIF database*/
      rc = keyFromDpi(
               thisgcpair,      /* ptr to appropriate group/comp. pair*/
               keystring,       /* ptr to key string from DPI         */
               &pattern,        /* ptr to ptr to key description for DMI - needs to be freed*/
               keyblocklen);    /* length of keyblock, in bytes       */
      if (rc != KEY_FROM_DPI_noError &&
          rc != KEY_FROM_DPI_tooShort &&
          rc != KEY_FROM_DPI_tooLong) {
         if (pattern) free(pattern);
         return rc;
      }
   }

   if (sequentialKey)
      rc = getSequentialKeyValue(
               gnflag->neededkey,
               thisgcpair,
               pattern,
               ppkeyblock,         /* Needs to be freed                  */
               keyblocklen);
   else
      rc = getKeyValue(
               gnflag->neededkey,
               thisgcpair,
               pattern,
               ppkeyblock,         /* Needs to be freed                  */
               keyblocklen);

   if (gnflag->neededkey == DMISA_NEXT && pattern) free(pattern);  /* Allocated by keyFromDpi*/
#if 0
   if (rc == GET_KEY_VALUE_noneHigher ||
       rc == PULL_KEY_VALUES_cannotGetToThem) {  /* Currently skip over the OID.  No way*/
                                                 /* to predict the next object ID.*/
#endif
   if (rc != GET_KEY_VALUE_noError) {
      if (*ppkeyblock) free(*ppkeyblock);
      return GN_FIND_KEY_notFound;
   } else if (rc != GET_KEY_VALUE_noError) {
      if (*ppkeyblock) free(*ppkeyblock);
      return rc;
   }

   return GN_FIND_KEY_found;
}


/*********************************************************************/
/* Function:  gnGetObject()                                          */
/*                                                                   */
/* Description:                                                      */
/* Get the specified attribute from DMI                              */
/*                                                                   */
/* Return codes:  GN_GET_OBJECT_getOk                                */
/*                GN_GET_OBJECT_cnfBufAddressability                 */
/*                GN_GET_OBJECT_tooBig  <needs work>                 */
/*                GN_GET_OBJECT_genErr                               */
/*                GN_GET_OBJECT_dmiBroke (also means genErr sent out)*/
/*                GN_GET_OBJECT_dpiBroke                             */
/*                GN_GET_OBJECT_outOfMemory                          */
/*                KEY_TO_DPI_otherError  - key translation error     */
/*                KEY_TO_DPI_outOfMemory - error occurred from malloc*/
/*                ISSUE_GET_ATTRIBUTE_outOfMemory                    */
/*                PREPARE_FOR_DMI_INVOKE_resetSemFailed              */
/*********************************************************************/
static int gnGetObject(snmp_dpi_set_packet **ppvarbind,
                       snmp_dpi_next_packet *pack,
                       GCPair_t *targetgcpair, ULONG attributei,
                       ULONG keyblocklen, DMI_GroupKeyData_t *keyblock)
{
   DmiGetAttributeOUT    *getattrout=NULL;
   DmiDataUnion_t        *value_ptr;        
   
   DmiString_t           *work;
   void                  *pvalue;
   int                    access, keystrlen = 0;
   int                    rcsub, rc = GN_GET_OBJECT_getOk;
   ULONG                  type, length, dmistat;
   char                   idstr[11], *newoidinstance;
   char                  *keystring;
   snmp_dpi_u64           loc64, *snmp64;
   DmiInt64Str           *dmi64;

#ifdef INT64_IS_OCTETSTRING
   char                  *snmp64int, *dmi64int;
   int                    i;

   snmp64int = (char *)&loc64; /* Initialize                          */
#endif
   snmp64    = &loc64;         /* Initialize                          */

   /* Form new OID instance                                           */
   keystrlen = 0;
   if (targetgcpair->iKeyCount) {
      rcsub = keyToDpi(
                 keyblock,
                 targetgcpair->iKeyCount,
                 &keystring);    /* Allocated by keyToDpi, free it when done*/
      if (rcsub != KEY_TO_DPI_noError) {
         if (keystring) free(keystring);
         return rcsub;
      }
      if (keystring) keystrlen = strlen(keystring);
   }
   newoidinstance = malloc(keystrlen + OID_INSTANCE_LEN + 1);
   if (!newoidinstance) return GN_GET_OBJECT_outOfMemory;  /* added 950317.gwl */
       /* non-key instance length + key length + null-termination     */
   strcpy(newoidinstance,ONE_BEFORE_GROUPID); /* 1.                   */
   pitoa(targetgcpair->iGroupId,idstr);
   strcat(newoidinstance,idstr);              /*    G                 */
   strcat(newoidinstance,DOT);                /*     .                */
   strcat(newoidinstance,ONE_AFTER_GROUPID);  /*      1.              */
   pitoa(attributei,idstr);
   strcat(newoidinstance,idstr);              /*        A             */
   strcat(newoidinstance,DOT);                /*         .            */
   pitoa(targetgcpair->iCompId,idstr);
   strcat(newoidinstance,idstr);              /*          C           */
   if (keystrlen) {
      strcat(newoidinstance,DOT);             /*           .          */
      strcat(newoidinstance,keystring);       /*            k.k.k. ... .k*/
   }
#ifdef DMISA_TRACE
   DMISA_TRACE_LOG3(LEVEL2,"Get object: g=%d a=%d c=%d.",
              targetgcpair->iGroupId, attributei, targetgcpair->iCompId);
   strcpy(logbuffer, "            ...");
   strncat(logbuffer, newoidinstance, LOG_BUF_LEN - 15);
   DMISA_TRACE_LOGBUF(LEVEL2);
#endif
   if (targetgcpair->iKeyCount && keystring) free(keystring);

   rcsub = issueGetAttribute(
                &getattrout,
                targetgcpair->iCompId,
                targetgcpair->iGroupId,
                attributei,
                targetgcpair->iKeyCount,
                keyblocklen,
                keyblock);
   if (rcsub != ISSUE_GET_ATTRIBUTE_noError) {
          if (getattrout->error_status != DMIERR_SP_INACTIVE)
                  rcsub = GN_GET_OBJECT_genErr;
      if (getattrout) {
         free_getattrout(getattrout);
      }
      free(newoidinstance);
          if (rcsub == ISSUE_GET_ATTRIBUTE_failed) {  
              return GN_GET_OBJECT_dmiBroke;
      } else {
         return rcsub;
      }
   }

   switch (rcsub) {
   case ISSUE_GET_ATTRIBUTE_noError:    /* No error (0x0000)           */
                                       /* Return noError & attribute value.*/
                                       /* If integer64, change to counter64*/
      value_ptr = getattrout->value;
      rcsub = xlateType(
                  &type,
                  value_ptr->type,
                  TODPI,
                  &length);
      if (rcsub != XLATE_TYPE_noError) {
         rc = GN_GET_OBJECT_genErr;
         break;
      }

      if (length == 0) {  /* attribute is a string             */
            work = value_ptr->DmiDataUnion_u.str;
            length = work->body.body_len;
            if (length > (SNMP_DPI_BUFSIZE - 0)) { /* Replace 0 when you fix tooBig condition*/
               if (getattrout) 
                 free_getattrout(getattrout);
               free(newoidinstance);
               return GN_GET_OBJECT_tooBig;
            }
                            /* Too big because bufferSize > maxDpiPpuSize*/
            pvalue = work->body.body_val;
#ifdef INT64_IS_OCTETSTRING
      } else if (length == sizeof(snmp_dpi_u64) &&
                 type == SNMP_TYPE_OCTET_STRING) { /* attribute is 64-bit integer*/
         dmi64int = ((char *)&(value_ptr->DmiDataUnion_u.integer64));
         swap64((char *)dmi64int,(char *)snmp64int);
         pvalue = snmp64int;
#endif
#ifdef COUNTER64_IS_COUNTER
      } else if (value_ptr->type == MIF_COUNTER64) {
        dmi64 = (DmiInt64Str *)((char *)&(value_ptr->DmiDataUnion_u.counter64));
#if defined(OS2) || defined(WIN32)
         pvalue = &(dmi64->low);
#else
         swap64((char *)dmi64,(char *)snmp64);
         pvalue = &(snmp64->low);
#endif
#else
      } else if (length != 0 && type == SNMP_TYPE_Counter64) {  /* attribute is 64-bit integer*/
        dmi64 = (DmiInt64Str *)((char *)&(value_ptr->DmiDataUnion_u.counter64));
         snmp64->high = dmi64->high;
         snmp64->low  = dmi64->low;
         pvalue = snmp64;
#endif
      } else if (value_ptr->type == MIF_DATE) {  
         pvalue = ((char *)(value_ptr->DmiDataUnion_u.date));
      } else {
         pvalue = ((char *)&(value_ptr->DmiDataUnion_u.integer));
                        /* length already set by xlateType function call*/
      }
      *ppvarbind = mkDPIset(           /* Make DPI set packet  */
              *ppvarbind,              /* ptr to varBind chain */
              pack->group_p,           /* ptr to subtree       */
              newoidinstance,          /* ptr to rest of OID   */
              (unsigned char)type,     /* value type           */
              (short int)length,       /* length of value      */
              pvalue);                 /* ptr to value         */
      if (!*ppvarbind) rc = GN_GET_OBJECT_dpiBroke; /* If it failed, return*/
      break;

   case ISSUE_GET_ATTRIBUTE_failed:
   default:
         rc = GN_GET_OBJECT_genErr;
      break;
   }

   free(newoidinstance);
   if (getattrout)
     free_getattrout(getattrout);
   return rc;
}


/*********************************************************************/
/* Function:  endOfMibView()                                         */
/*                                                                   */
/* Make the DPI packet to return endOfMibView                        */
/*                                                                   */
/* Return codes:  END_OF_MIB_VIEW_noError                            */
/*                END_OF_MIB_VIEW_dpiBroke                           */
/*********************************************************************/
static int endOfMibView(snmp_dpi_set_packet **ppvarbind,
                        snmp_dpi_next_packet *ppack)
{

   *ppvarbind = mkDPIset(               /* Make DPI set packet     */
           *ppvarbind,                  /* ptr to varBind chain    */
           ppack->group_p,              /* ptr to subtree          */
           ppack->instance_p,           /* ptr to rest of OID      */
           SNMP_TYPE_endOfMibView,      /* value type              */
           0L,                          /* length of value         */
           (unsigned char *)0);         /* ptr to value            */

   if (!*ppvarbind) return END_OF_MIB_VIEW_dpiBroke; /* Return if failed*/

   return END_OF_MIB_VIEW_noError;
}


/*********************************************************************/
/* Function:  oidCmpOneToManyPrep()                                  */
/*                                                                   */
/* Description:                                                      */
/* Convert an OID from dotted-decimal to integer array format        */
/* for subsequent comparison to another OID.  A maximum of           */
/* 128 sub-IDs is translated, with any remainder ignored.            */
/* Allows a prefix and a suffix dot.                                 */
/*     Note: OID is already checked by agent to be dotted-decimal    */
/*     with numbers less than 2**32.                                 */
/*                                                                   */
/* Input:  char *oid                                                 */
/* Output: integer array with OIDs, up to 128 integers               */
/* Return: number of sub-IDs in the OID                              */
/*********************************************************************/
/* int *oidCmpOneToManyPrep(char *oid, ULONG *OidArr[]){

   ULONG oidarr[DMISA_MAX_SUBID_COUNT];
   int   i;
   ULONG n;

   for ( i = 0; i < DMISA_MAX_SUBID_COUNT; i++ ) {
      if (*oid != '\0') {        // convert ascii to ULONG integer
         if (*oid == '.') oid++; // get past the dot
         n = 0;
         for (; *oid >= '0' && *oid <= '9'; oid++) {
            n = 10 * n + (*oid - '0');
         }
         oidarr[i] = n;
         if (*oid == '.') oid++; // get past the dot
      }
   }
   *OidArr = &(oidarr[0]);       // point to beginning of array
   return i;                     // number of sub-id's
}
*/

/*********************************************************************/
/* Function:  oidCmpOneToMany()                                      */
/*                                                                   */
/* Description:                                                      */
/* Compare an OID in integer array format to another in dotted-decimal */
/* Note: OID is already checked by agent to be dotted-decimal          */
/*       with numbers less than 2**32                                  */
/* Return: DMISA_lessThan    array OID less than dotted-decimal OID    */
/*         DMISA_equalTo     array OID equal to dotted-decimal OID     */
/*         DMISA_greaterThan array OID greater than dotted-decimal OID */
/***********************************************************************/
/*int oidCmpOneToMany(int oidA[], int oidAlength, char *oidB){
   int   i;
   ULONG n;

   for ( i = 0; i < oidAlength; i++ ) {
      if (*oidB != '\0') {  // convert ascii to ULONG integer
         if (*oidB == '.') oidB++;  // get past the dot
         n = 0;
         for (; *oidB >= '0' && *oidB <= '9'; oidB++) {
            n = 10 * n + (*oidB - '0');
         }
         if (oidA[i] < n) return DMISA_lessThan;
         if (oidA[i] > n) return DMISA_greaterThan;

         if (*oidB == '.') oidB++;  // get past the dot
      } else {
         return DMISA_greaterThan;
      }
   }

   if (*oidB != '\0') return DMISA_lessThan;
   return DMISA_equalTo;
}
*/
