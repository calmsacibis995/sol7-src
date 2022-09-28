/* Copyright 10/08/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisaget.c	1.11 96/10/08 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisaget.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support DMI access.             */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        doGet(...)                  */
/*                                        getParseInstance(...)       */
/*                                        getFindGCPair(...)          */
/*                                        getTranslateKey(...)        */
/*                                        getAttribute(...)           */
/*                                        noSuchInstance(...)         */
/*                                        noSuchObject(...)           */
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
/*                  941215    LAUBLI    Fixed byte ordering on 64bit  */
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

void traceReqBuf(DMI_MgmtCommand_t *mgmtreq);
static int getAttribute(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_get_packet *ppack,
           ULONG compi, ULONG groupi, ULONG attributei,
           ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *pkeyblock);
static int noSuchInstance(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_get_packet *ppack);
static int noSuchObject(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_get_packet *ppack);

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
/*      type counter64 or 8-byte OCTETSTRINGs.  If counter64, the     */
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
/*   1. noSuchInstance is returned in cases where noSuchName          */
/*      should be returned.  This occurs when an attribute within     */
/*      a given group (i.e., an SNMP column) is not found, and        */
/*      no another group under this OID prefix (i.e., SNMP instance)  */
/*      contains the same attribute ID.                               */
/*      The current sub-agent design, does not know whether another   */
/*      instance exists, so it assumes it does.                       */
/*      (See note at bottom of DpiGet prolog.)                        */
/*      Reason: Thought it was too expensive to implement correctly.  */
/*              However, it would require only a relatively minor     */
/*              change to correct this.                               */
/*                                                                    */
/*   2. If an attribute is missing from a MIF/MIB (e.g.,              */
/*      customer removed it), the sub-agent cannot report             */
/*      even an empty string to the manager to make his view          */
/*      of the MIB complete.  Rather, noSuchInstance is returned.     */
/*      Reason: Impossible to complete the MIB, but it is unexpected  */
/*              that users will alter the portions of the MIF that    */
/*              comprise MIB definition.                              */
/*                                                                    */
/*   3. TooBig is not returned in some cases where it should be.      */
/*      This is due to a deficiency in the DPI interface.             */
/*      Bert says he may fix it in the next release, but not before,  */
/*      as it would be a major change.                                */
/*      Reason: Too cumbersome for me to calculate the exact size of  */
/*              the SNMP packet.  Can be done if absolutely necessary.*/
/*                                                                    */
/**********************************************************************/
/***********************************************************************/
/* Function: doGet()                                                   */
/*                                                                     */
/* Input parameters:                                                   */
/*   ptr. to OID prefix                                                */
/*   ptr. to OID instance                                              */
/*   ptr. to XlateList                                                 */
/*                                                                     */
/* Output parameters:                                                  */
/*   Packet to send back to SNMP                                       */
/*   return code: 0 = success                                          */
/*                x = nnnnn                                            */
/* Description:                                                        */
/* 1. Start at beginning of XlateList.                                 */
/* 2. Search for an occurrence of the OID prefix in XlateList.         */
/*    If not found, return noError/noSuchObject.                       */
/* 3. Under the current OID prefix, search for the specified group.    */
/*    Format of the OID beyond the prefix is .1.G.1.A.C.k.k. ... .k    */
/*    If not found, return noError/noSuchObject.                       */
/* 4. Search for the current component ID.                             */
/*    If not found, return noError/noSuchInstance.                     */
/* 5. If:(1) neither the OID nor XlateList have a key for              */
/*           the current group, issue GetAttribute.                    */
/*       (2) both OID & XlateList contain a key for the current group  */
/*           convert key using KeyFromDpi and issue GetAttribute.      */
/*       (3) key in OID but not in XlateList, return noError/noSuchInst*/
/*       (4) key not in OID but in XlateList, return noError/noSuchInst*/
/* 6. On return from GetAttribute, if iStatus:                         */
/*                                       v2 Error-status / Value-type  */
/*                                       ----------------------------  */
/*   = (000) Success,                    return noError/attrib.value   */
/*           (If type is INTEGER64, then change type to OCTETSTRING)   */
/*   = (001) More data is available,     return tooBig/empty value     */
/*                                   because bufferSize > maxDpiPpuSize*/
/*   = (100) Attribute not found,        return noError/noSuchInstance */
/*   = (101) Value exceeds maximum size, return genErr                 */
/*   = (102) Component instru.not found, return noError/noSuchInstance */
/*   = (103) Enumeration error,          return genErr                 */
/*   = (104) Group not found,            return noError/noSuchObject   */
/*   = (105) Illegal keys,               return noError/noSuchInstance */
/*   = (106) Illegal to set,             return genErr                 */
/*ge = (107) Can't resolve attribute function name, return noErr/noInst*/
/*   = (108) Illegal to get,             return genErr                 */
/*     (except if DMI access is write-only, return noError/NULL string)*/
/*   = (109) No description,             return genErr                 */
/*   = (10a) Row not found,              return noError/noSuchInstance */
/*ge = (10b) Direct i/f not registered,  return genErr                 */
/*   = (10c) MIF database is corrupt,    return genErr                 */
/*   = (10d) Attribute is not supported, return noError/NULL string    */
/*ge = (1001) Overlay not found          return noError/noSuchInstance */
/*ge = (10004) Component get error       return genErr                 */
/*ge = (10005) Component key error       return noError/noSuchInstance */
/*   = (else)any other error             return genErr                 */
/*   If iStatus = 0x0203, then reinitialize the sub-agent.             */
/*   Note: As of 9/94, the Intel implementation of the Service Layer   */
/*         should never show any iStatus but the following             */
/*         on issueGetAttribute: 0x0, 0x1, 0x100, 0x105, 0x107,        */
/*         0x108, 0x10b, 0x10c, 0x200, 0x203, 0x204, 0x205, 0x206,     */
/*         0x208, 0x211, 0x1000, 0x1001, 0x1002, 0x1101, 0x2000,       */
/*         0x3004, 0x3005, 0x3006, 0x10004, and 0x10005.               */
/*                                                                     */
/* NOTE: Assumption has been made that DPI DLL does OID dotted-decimal */
/*       verification.                                                 */
/*                                                                     */
/* Return codes:  DO_GET_noError                                       */
/*                DO_GET_dpiBroke                                      */
/*                KEY_FROM_DPI_outOfMemory                             */
/*                GET_PARSE_INSTANCE_programmingError                  */
/***********************************************************************/
int doGet(snmp_dpi_hdr *phdr, snmp_dpi_get_packet *ppack, OidPrefix_t *XlateList)
{
   snmp_dpi_set_packet  *pvarbind;
   unsigned char        *ppacket;
   int                   rc, rcdpi, generr = DMISA_FALSE, toobig = DMISA_FALSE;
   int                   nosuchName = DMISA_FALSE;
   long int              i, error;
   ULONG  compi, groupi, attributei;  /* requested component, group & attribute IDs*/
   GCPair_t             *gcpair;
   char                 *keystring;
   ULONG                 keyblocklen;
   DMI_GroupKeyData_t   *pkeyblock = NULL;
#ifdef DMISA_TRACE
   int                   ln;
#endif
#ifdef SOLARIS2
   SNMP_pdu   *pdu;
#endif


   pvarbind = snmp_dpi_set_packet_NULL_p;  /* initialize varBind chain*/

   /* Could adjust this loop to support multi var-bind PDUs           */
   for (i=1;ppack/*i<=1*/;i++) {
#ifdef DMISA_TRACE
      ln = sprintf(logbuffer,"Get OID ");
      strncpy(logbuffer + ln, ppack->object_p, (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      DMISA_TRACE_LOGBUF(LEVEL2);
#endif
      if (!strncmp(ppack->instance_p,"1.1" /*SUN_ARCH_DMI_COMPMIB*/,
                   3/*strlen(SUN_ARCH_DMI_COMPMIB)*/)) {  /* Component MIB*/
         rc = dpiGetCompMib(
                 &pvarbind,
                 ppack);  /* be able to handle NULL OID instance string*/
         if (rc != GET_COMP_MIB_getOk) {  /* if search not successful */
            if (rc == GET_COMP_MIB_noSuchInstance) {
               rc = noSuchInstance(&pvarbind, ppack);  /* Return noError/noSuchInstance*/
               if (rc == NO_SUCH_INSTANCE_dpiBroke) {
                     return rc;
               }
               nosuchName = DMISA_TRUE;
               /*continue; Previously used to get the next var */
               break;
            } else if (rc == GET_COMP_MIB_noSuchObject) {
               rc = noSuchObject(&pvarbind, ppack);  /* Return noError/noSuchObject*/
               if (rc == NO_SUCH_OBJECT_dpiBroke) {
                     return rc;
               }
               nosuchName = DMISA_TRUE;
               /*continue; */
               break;
            } else if (rc == GET_COMP_MIB_genErr) {
               generr = DMISA_TRUE;
               break;
            } else if (rc == GET_COMP_MIB_tooBig) {
               toobig = DMISA_TRUE;
               break;
            } else {
               return rc;
            }
         }
      } else {

         rc = getParseInstance(
                 ppack->instance_p,
                 &groupi,
                 &attributei,
                 &compi,
                 &keystring);
         if (rc != GET_PARSE_INSTANCE_fullySpecified) {  /* if parse not successful*/
            if (rc == GET_PARSE_INSTANCE_notFullySpecified ) {
               DMISA_TRACE_LOG(LEVEL2, "Object identifier not fully specified.");
               rc = noSuchObject(&pvarbind, ppack);  /* Return noError/noSuchObject*/
               if (rc == NO_SUCH_OBJECT_dpiBroke) {
                   return rc;
               }
               nosuchName = DMISA_TRUE;
               break;
               /*continue; */
            }
            return rc;  /* to cover programming error case            */
         } else {

            rc = getFindGCPair(
                    &gcpair,
                    XlateList,
                    ppack->group_p,
                    groupi,
                    compi);
            if (rc != GET_FIND_GC_PAIR_pairFound) {  /* if search not successful*/
               DMISA_TRACE_LOG(LEVEL2, "Group/component pair not successfully found during GET operation.");
               if (rc == GET_FIND_GC_PAIR_pairNotFound_groupMatch) {
                  rc = noSuchInstance(&pvarbind, ppack);  /* Return noError/noSuchInstance*/
                  if (rc == NO_SUCH_INSTANCE_dpiBroke) {
                        return rc;
                  }
                  nosuchName = DMISA_TRUE;
                  break;
                  /*continue; */
               } else {
                  rc = noSuchObject(&pvarbind, ppack);  /* Return noError/noSuchObject*/
                  if (rc == NO_SUCH_OBJECT_dpiBroke) {
                       return rc;
                  }
                  nosuchName = DMISA_TRUE;
                  break;
                  /*continue; */
               }
            } else {

               rc = getTranslateKey(
                       gcpair,
                       keystring,
                       &keyblocklen,
                       &pkeyblock);
               if (rc != GET_TRANSLATE_KEY_noError) {  /* if parse not successful*/
                  if (pkeyblock) free(pkeyblock);      /* Allocated by keyFromDpi()*/
                  DMISA_TRACE_LOG(LEVEL2, "Key translation prevents normal completion of the GET operation.");
                  if (rc == DMISA_ERROR_noSuchInstanceOrObject) {
                     rc = noSuchInstance(&pvarbind, ppack);  /* Return noError/noSuchInstance*/
                         /* noSuchObject should be returned if no such G/A exists under this OID prefix*/
                     if (rc == NO_SUCH_INSTANCE_dpiBroke) {
                             return rc;
                     }
                     nosuchName = DMISA_TRUE;
                     break;
                  }
                  return rc;  /* to cover programming error case      */
               } else {

                  rc = getAttribute(  /* could reduce # of arguments by using GCpair (noC,noG,noKC)*/
                          &pvarbind,
                          ppack,
                          compi,
                          groupi,
                          attributei,
                          gcpair->iKeyCount,
                          keyblocklen,
                          pkeyblock);
                  if (pkeyblock) free(pkeyblock);   /* Allocated by keyFromDpi()*/
                  if (rc != GET_ATTRIBUTE_getOk) {  /* if get not successful*/
                     if (rc == DMISA_ERROR_noSuchInstance ||
                         rc == DMISA_ERROR_noSuchInstanceOrObject) {
                        DMISA_TRACE_LOG(LEVEL2, "Error on GET operation: no such instance exists.");
                        rc = noSuchInstance(&pvarbind, ppack);  /* Return noError/noSuchInstance*/
                        if (rc == NO_SUCH_INSTANCE_dpiBroke) {
                                 return rc;
                        }
                        nosuchName = DMISA_TRUE;
                        break;
                     } else if (rc == DMISA_ERROR_noSuchObject) {
                        DMISA_TRACE_LOG(LEVEL2, "Error on GET operation: no such object exists.");
                        rc = noSuchObject(&pvarbind, ppack);  /* Return noError/noSuchObject*/
                        if (rc == NO_SUCH_OBJECT_dpiBroke) {
                                return rc;
                        }
                        nosuchName = DMISA_TRUE;
                        break;
                     } else if (rc == GET_ATTRIBUTE_genErr) {
                        DMISA_TRACE_LOG(LEVEL2, "Error on GET operation: general error.");
                        generr = DMISA_TRUE;
                        break;
                     } else if (rc == GET_ATTRIBUTE_tooBig) {
                        DMISA_TRACE_LOG(LEVEL2, "Error on GET operation: value too long to fit into buffer.");
                        toobig = DMISA_TRUE;
                        break;
                     } else {   /* Handle these cases:                */
                        DMISA_TRACE_LOG1(LEVEL2, "Error on GET operation: return code = %d.", rc);
/*                        GET_ATTRIBUTE_genErr    \__combine?                 */
/*                        GET_ATTRIBUTE_dmiBroke  /                           */
                   generr = DMISA_TRUE;
                   break;
                     }
                  } /* endif on getAttribute */
               } /* endif on getTranslateKey */
            } /* endif on getFindGCPair */
         } /* endif on getParseInstance */
      } /* endif on non-CompMib get operations */
       ppack = ppack->next_p;
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
   ppacket = mkDPIresponse(             /* Make DPIresponse packet */
           phdr,                        /* ptr parsed request      */
           error,                       /* error code              */
           i,                           /* index of object in error*/
           pvarbind);                   /* varBind response data   */
   if (!ppacket) {
      DMISA_TRACE_LOG(LEVEL1,"mkDPIresponse failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed   */
   }

   rcdpi = DPIsend_packet_to_agent(     /* send OPEN packet        */
           DpiHandle,                   /* on this connection      */
           ppacket,                     /* this is the packet      */
           DPI_PACKET_LEN(ppacket));    /* and this is its length  */
   if (rcdpi != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG(LEVEL1,"DPISend_packet_to_agent failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed   */
   }
#else
      pdu = dpihdr_to_pdu(phdr, pvarbind, error, i);
      if (!pdu) {
     DMISA_TRACE_LOG(LEVEL1,"dpihdr_to_pdu failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed   */
      }

      if (TraceLevel == 5) {
      printf("Response PDU ..................\n");
      trace_snmp_pdu(pdu);
      }

      if (snmp_pdu_send(socket_handle, &address, pdu, error_label) == -1)  {
     DMISA_TRACE_LOG(LEVEL1,"snmp_pdu_send failed in doGet function.");
      return DO_GET_dpiBroke;      /* Return if failed  */
      }

      snmp_pdu_free(pdu);

#endif
   if (generr) return rc;

   return DO_GET_noError;
}


/*********************************************************************/
/* Function: getParseInstance()       (used for both GET and SET)    */
/*                                                                   */
/* Parse the OID instance for the Get operation                      */
/*                                                                   */
/* Return codes:  GET_PARSE_INSTANCE_fullySpecified                  */
/*                GET_PARSE_INSTANCE_notFullySpecified               */
/*                GET_PARSE_INSTANCE_programmingError                */
/*********************************************************************/
int getParseInstance(char *instance, ULONG *groupi, ULONG *attributei,
                     ULONG *compi, char **keystring)
{
   int    rc = GET_PARSE_INSTANCE_notFullySpecified;
   int    i, j;
   ULONG  uli;
   char  *restofit;

   *keystring = NULL;

   /* Parse OID instance                                              */
   errno = 0;   /* initialize                                         */
   if (instance) {
      for (i=1, j=0; i <= 5 && instance; i++) {
         uli = strtoul(instance + j, &restofit, 10);
         if (errno) return rc;  /* could be ERANGE or EDOM            */
         j = restofit - instance + 1;  /* incremented to get beyond '.'*/

         switch (i) {
         case 1:
            if (1 != uli) {  /* must always be a 1 immediately preceding G in OID*/
               DMISA_TRACE_LOG(LEVEL2,"OID instance lacks a 1 immediately preceding group ID.");
               return rc;
            }
            break;
         case 2:
            *groupi = uli;
            break;
         case 3:
            if (1 != uli) {  /* must always be a 1 between G & A in OID, handles leading zeroes*/
               DMISA_TRACE_LOG(LEVEL2,"OID instance lacks the 1 between group ID & attribute ID.");
               return rc;
            }
            break;
         case 4:
            *attributei = uli;
            break;
         case 5:
            rc = GET_PARSE_INSTANCE_fullySpecified;
            *compi = uli;
            break;
         default:
            DMISA_TRACE_LOG(LEVEL1,"Switch statement failed in getParseInstance function.");
            return GET_PARSE_INSTANCE_programmingError;  /* error - should never reach here*/
         /* break;  // Note: This line of code has been commented out */
         }
         if (*(instance + j) == '\0') break;
      }
   }

   if (rc == GET_PARSE_INSTANCE_fullySpecified &&
             restofit != NULL &&
             restofit[0] != '\0' &&
             restofit[1] != '\0') {
      *keystring = &restofit[1];    /* Point to the to key portion of the OID*/
   }
   return rc;
}


/*********************************************************************/
/* Function: getFindGCPair()         (used for both GET and SET)     */
/*                                                                   */
/* Find the Group/Component pair for the OID in the current request. */
/*                                                                   */
/* Return NULL in GCPair ptr. if pair not found                      */
/*                                                                   */
/* Return codes:  GET_FIND_GC_PAIR_pairFound                         */
/*                GET_FIND_GC_PAIR_pairNotFound_groupMatch           */
/*                GET_FIND_GC_PAIR_pairNotFound_noGroupMatch         */
/*********************************************************************/
int getFindGCPair(GCPair_t **ppGCPair, OidPrefix_t *xlatelist,
                  char *pgroup, ULONG groupi, ULONG compi)
{
   int   groupmatch = DMISA_FALSE;
   int   compmatch = DMISA_FALSE;

   *ppGCPair = NULL;

   /* Find associated Group/Component pair                            */
   if (!pgroup) return GET_FIND_GC_PAIR_pairNotFound_noGroupMatch;

   while (xlatelist &&
          (strcmp(xlatelist->pOidPrefix, pgroup)) ) {
      xlatelist = xlatelist->pNextOidPre;  /* position to matching OID prefix*/
   }
   if (xlatelist == NULL) return GET_FIND_GC_PAIR_pairNotFound_noGroupMatch;

   *ppGCPair = xlatelist->pNextGCPair;
   while (*ppGCPair &&                        /* find matching group/component pair;*/
          (!(((*ppGCPair)->iGroupId > groupi) ||       /* stop if group not found or*/
             (((*ppGCPair)->iGroupId == groupi) && /*if group found and*/
              ((*ppGCPair)->iCompId >= compi)))) ) {   /* component found or passed*/
      *ppGCPair = (*ppGCPair)->pNextGCPair;
   }
   if ((*ppGCPair)->iGroupId == groupi)
         groupmatch = DMISA_TRUE;
   else
         groupmatch = DMISA_FALSE;
   if (*ppGCPair && (((*ppGCPair)->iGroupId == groupi) &&
                     ((*ppGCPair)->iCompId == compi) )) {
      compmatch = DMISA_TRUE;   /* set if both group & component were found*/
   }

   if (!groupmatch) return GET_FIND_GC_PAIR_pairNotFound_noGroupMatch;
   if (!compmatch) return GET_FIND_GC_PAIR_pairNotFound_groupMatch;
   return GET_FIND_GC_PAIR_pairFound;
}


/*********************************************************************/
/* Function: getTranslateKey()        (used for both GET and SET)    */
/*                                                                   */
/* Translate the key, if one exists, from a portion of the OID to    */
/* a DMI key control block                                           */
/*                                                                   */
/* Return codes: GET_TRANSLATE_KEY_noError                           */
/*               DMISA_ERROR_noSuchInstanceOrObject                  */
/*               KEY_FROM_DPI_outOfMemory                            */
/*********************************************************************/
int getTranslateKey(GCPair_t *gcpair, char *keystring,
                    ULONG *keyblocklen, DMI_GroupKeyData_t **ppkeyblock)
{
   int  rc;
   int  rcsub;

   *keyblocklen = 0;

   /* Translate key                                                   */
   if (keystring && gcpair->iKeyCount) { /* xlate key if specified & exists*/
      rc = GET_TRANSLATE_KEY_noError;  /* _keyPresent;                */
      rcsub = keyFromDpi(
              gcpair,          /* ptr to appropriate group/comp. pair */
              keystring,       /* ptr to key string from DPI          */
              ppkeyblock,      /* ptr to ptr to key structures for DMI*/
              keyblocklen);    /* ptr to length of key structures for DMI*/
      if (rcsub != KEY_FROM_DPI_noError) {
         if (rcsub == KEY_FROM_DPI_tooShort   ||
             rcsub == KEY_FROM_DPI_tooLong    ||
             rcsub == KEY_FROM_DPI_illegalKey ||
             rcsub == KEY_FROM_DPI_otherError) {
            return DMISA_ERROR_noSuchInstanceOrObject;
         }
         return rc;  /* for outOfMemory et al.                        */
      }
   } else if (keystring && !(gcpair->iKeyCount) ||
              !keystring && gcpair->iKeyCount  )  {
      return DMISA_ERROR_noSuchInstanceOrObject;
   }
   return GET_TRANSLATE_KEY_noError;
}

/*********************************************************************/
/* Function: getAttribute()                                          */
/*                                                                   */
/* Get the specified attribute from DMI                              */
/*                                                                   */
/* Return codes: GET_ATTRIBUTE_getOk                                 */
/*               DMISA_ERROR_noSuchInstance                          */
/*               DMISA_ERROR_noSuchInstanceOrObject                  */
/*               DMISA_ERROR_noSuchObject                            */
/*               DMISA_ERROR_cnfBufAddressability                    */
/*               GET_ATTRIBUTE_tooBig                                */
/*               GET_ATTRIBUTE_genErr    \__combine?                 */
/*               GET_ATTRIBUTE_dmiBroke  /                           */
/*               ISSUE_GET_ATTRIBUTE_outOfMemory                     */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*********************************************************************/
static int getAttribute(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_get_packet *pack,
           ULONG compi, ULONG groupi, ULONG attributei,
           ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *pkeyblock)
{
   DmiGetAttributeOUT    *getattrout=NULL;
   DmiDataUnion_t        *value_ptr;
   DmiString_t           *work;
   void                  *pvalue;
   int                    access, rcsub, rc = GET_ATTRIBUTE_getOk;
   ULONG                  type, length, dmistat;
   snmp_dpi_u64           loc64;
   snmp_dpi_u64          *snmp64;
   DmiInt64Str           *dmi64;

#ifdef INT64_IS_OCTETSTRING
   char                  *snmp64int, *dmi64int; /* Added 12/15/94.gwl */
   int                    i;                    /* Added 12/15/94.gwl */

   snmp64int = (char *)&loc64; /* Initialize       Added 12/15/94.gwl */
#endif
   snmp64 = &(loc64);  /* Initialize                                  */

   rcsub = issueGetAttribute(
                &getattrout,
                compi,
                groupi,
                attributei,
                keycount,
                keyblocklen,
                pkeyblock);
   if (rcsub != ISSUE_GET_ATTRIBUTE_noError) {
          if (getattrout->error_status != DMIERR_SP_INACTIVE)
                  rcsub = DMISA_ERROR_noSuchInstanceOrObject;
      if (getattrout) {
         free_getattrout(getattrout);
      }
      if (rcsub == ISSUE_GET_ATTRIBUTE_failed) {
         return GET_ATTRIBUTE_dmiBroke;
      } else return rcsub;
   }

   switch (rcsub) {
   case ISSUE_GET_ATTRIBUTE_noError:    /* No error (0x0000)           */
                                       /* Return noError & attribute value.*/
                                       /* If integer64, change to counter64*/
      value_ptr = getattrout->value;
      rcsub = xlateType(&type, value_ptr->type ,TODPI, &length);
      if (rcsub != XLATE_TYPE_noError) {
         rc = GET_ATTRIBUTE_genErr;
         break;
      }
      if (length == 0) {  /* attribute is a string             */
         work = value_ptr->DmiDataUnion_u.str;
         length = work->body.body_len;
         pvalue = work->body.body_val;
#ifdef INT64_IS_OCTETSTRING
      } else if (length == sizeof(snmp_dpi_u64) &&
                 type == SNMP_TYPE_OCTET_STRING) {     /* Attribute is a 64-bit integer*/
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
      } else if (length != 0 && type == SNMP_TYPE_Counter64) {
                       /* Attribute is 64-bit counter                 */
         dmi64 = (DmiInt64Str *)((char *)&(value_ptr->DmiDataUnion_u.counter64));
         snmp64->high = dmi64->high;
         snmp64->low  = dmi64->low;
         pvalue = snmp64;
#endif
      } else {  /* attribute is not a string                          */
         pvalue = ((char *)&(value_ptr->DmiDataUnion_u.integer));
                        /* length already set by xlateType function call*/
      }
      if (length > (SNMP_DPI_BUFSIZE - 0)) {  /* Replace 0 with better value*/
                                              /* See comment above in SNMP Deviations section*/
         if (getattrout)
            free_getattrout(getattrout);
         return GET_ATTRIBUTE_tooBig;
      }
                         /* Too big because bufferSize > maxDpiPpuSize*/
      *ppvarbind = mkDPIset(           /* Make DPI set packet  */
              *ppvarbind,              /* ptr to varBind chain */
              pack->group_p,           /* ptr to subtree       */
              pack->instance_p,        /* ptr to rest of OID   */
              (unsigned char)type,     /* value type           */
              (int)length,             /* length of value      */
              pvalue);                 /* ptr to value         */
      if (!ppvarbind) {
         rc = GET_ATTRIBUTE_dpiBroke; /* If it failed, return*/
      }
      break;

   case ISSUE_GET_ATTRIBUTE_failed:
      rc = DMISA_ERROR_noSuchInstanceOrObject;
      break;
   default:
      rc = GET_ATTRIBUTE_genErr;
     break;
   }

   if (getattrout)
      free_getattrout(getattrout);

   return rc;
}


/*********************************************************************/
/* Function:  noSuchInstance()                                       */
/*                                                                   */
/* Make the DPI packet to return noSuchInstance                      */
/*                                                                   */
/* Return codes:  NO_SUCH_INSTANCE_noError                           */
/*                NO_SUCH_INSTANCE_dpiBroke                          */
/*********************************************************************/
static int noSuchInstance(snmp_dpi_set_packet **ppvarbind,
                          snmp_dpi_get_packet *ppack)
{

   *ppvarbind = mkDPIset(               /* Make DPI set packet     */
           *ppvarbind,                  /* ptr to varBind chain    */
           ppack->group_p,              /* ptr to subtree          */
           ppack->instance_p,           /* ptr to rest of OID      */
           SNMP_TYPE_noSuchInstance,    /* value type              */
           0L,                          /* length of value         */
           (unsigned char *)0);         /* ptr to value            */

   if (!*ppvarbind) return NO_SUCH_INSTANCE_dpiBroke; /* Return if failed*/

   return NO_SUCH_INSTANCE_noError;
}


/*********************************************************************/
/* Function:  noSuchObject()                                         */
/*                                                                   */
/* Make the DPI packet to return noSuchObject                        */
/*                                                                   */
/* Return codes:  NO_SUCH_OBJECT_noError                             */
/*                NO_SUCH_OBJECT_dpiBroke                            */
/*********************************************************************/
static int noSuchObject(snmp_dpi_set_packet **ppvarbind,
                        snmp_dpi_get_packet *ppack)
{

   *ppvarbind = mkDPIset(               /* Make DPI set packet     */
           *ppvarbind,                  /* ptr to varBind chain    */
           ppack->group_p,              /* ptr to subtree          */
           ppack->instance_p,           /* ptr to rest of OID      */
           SNMP_TYPE_noSuchObject,      /* value type              */
           0L,                          /* length of value         */
           (unsigned char *)0);         /* ptr to value            */

   if (!*ppvarbind) return NO_SUCH_OBJECT_dpiBroke; /* Return if failed  */

   return NO_SUCH_OBJECT_noError;
}
