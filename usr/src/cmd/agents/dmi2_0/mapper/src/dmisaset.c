/* Copyright 12/03/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmisaset.c	1.9 96/12/03 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmisaset.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions which support GETs and SETs of     */
/*  attributes within tables (i.e., that are associated with a key).  */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                      doSet(...)                    */
/*                                      setCleanReserveList(...)      */
/*                                      setAttribute(...)             */
/*                                      setExtractType(...)           */
/*                                      setCheckIllegal(...)          */
/*                                      setAddToReserveList(...)      */
/*                                      setRemoveFromReserveList(...) */
/*                                      setWatchReserveList(...)      */
/*                                                                    */
/* End Module Description *********************************************/
/*                                                                    */
/* Copyright **********************************************************/
/*                                                                    */
/* Copyright:                                                         */
/*   Licensed Materials - Program property of IBM                     */
/*   This product contains "Restricted Materials of IBM"              */
/*   xxxx-xxx (C) Copyright IBM Corp. 1994.                           */
/*   All rights reserved.                                             */
/*   US Government Users Restricted Rights -                          */
/*   Use, duplication or disclosure restricted by GSA ADP Schedule    */
/*   Contract with IBM Corp.                                          */
/*   See IBM Copyright Instructions form no. G120-2083.               */
/*   Classification: IBM Confidential                                 */
/*                                                                    */
/* End Copyright ******************************************************/
/*                                                                    */
/*                                                                    */
/* Change Log *********************************************************/
/*                                                                    */
/*  Flag  Reason    Date      Userid    Description                   */
/*  ----  --------  --------  --------  -----------                   */
/*                  941214    LAUBLI    New module                    */
/*                  950120    LAUBLI    Made free of autorelease safer*/
/*                  950317    LAUBLI    Add check on malloc           */
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

static int setAttribute(snmp_dpi_hdr *phdr, snmp_dpi_set_packet *pack,
      ULONG compi, ULONG groupi, ULONG attributei,
      ULONG keycount, ULONG keyblocklen, DMI_GroupKeyData_t *pkeyblock,
      int *dpierr);
static int setExtractType(ULONG *type, ULONG compi, ULONG groupi, ULONG attributei);
static int setCheckIllegal(int *error, ULONG reqlength, ULONG reqtype,
      ULONG compi, ULONG groupi, ULONG attributei);
static int setRemoveFromReserveList(snmp_dpi_set_packet *pack);
static int setAddToReserveList(snmp_dpi_set_packet *pack,
      ULONG attributetype, DmiSetAttributeIN *setattribin);
#if defined(OS2) || defined(AIX325) || defined(WIN32)
static void setWatchReserveList(void *pvoid);
#else
void *setWatchReserveList(void *pvoid);
#endif

int do_setup(void);

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
/*   1. noSuchInstance is returned in cases where noSuchName          */
/*      should be returned.  This occurs when an attribute within     */
/*      a given group (i.e., an SNMP column) is not found, and        */
/*      no another group under this OID prefix (i.e., SNMP instance)  */
/*      contains the same attribute ID.                               */
/*      The current sub-agent design, does not know whether another   */
/*      instance exists, so it assumes it does.                       */
/*      (See note at bottom of DpiGet prolog.)                        */
/*      Reason: Too expensive to implement the SNMP specification.    */
/*                                                                    */
/*   2. If an attribute is missing from a MIF/MIB (e.g.,              */
/*      customer removed it), the sub-agent cannot report             */
/*      even an empty string to the manager to make his view          */
/*      of the MIB complete.  Rather, noSuchInstance is returned.     */
/*      Reason: Impossible to complete the MIB, but it is unexpected  */
/*              that users will alter the portions of the MIF that    */
/*              comprise MIB definition.                              */
/*                                                                    */
/**********************************************************************/
/***********************************************************************/
/*  Function: doSet()       for Set/Commit/Undo (Reserve/Set/Undo)     */
/*                                                                     */
/*  Input parameters:                                                  */
/*   ptr. to OID prefix                                                */
/*   ptr. to OID instance                                              */
/*   ptr. to XlateList                                                 */
/*                                                                     */
/*  Output parameters:                                                 */
/*   Packet to send back to SNMP                                       */
/*   return code: 0 = success                                          */
/*                x = nnnnn                                            */
/*                                                                     */
/*  Description:                                                       */
/*                             .-------------------------------.       */
/*                             |                               | 5     */
/*                             |         2.-.          4.-.    |       */
/*                             V     1    V |   3       V |    |       */
/*   SET STATE MACHINE:      noSet -----> SET ----> setCOMMIT -'       */
/*                             A           |6             |            */
/*                             |     9     V              | 7          */
/*                             '---------- setUNDO <------'            */
/*                                           A |                       */
/*                                           '-'8                      */
/*                                                                     */
/*                  SET STATE:   DMI Operation:                        */
/*                  ----------   --------------                        */
/*                         SET   SetReserveAttribute                   */
/*                   setCOMMIT   SetAttribute                          */
/*                     setUNDO   if attribute uncommitted,             */
/*                                  SetReleaseAttribute,               */
/*                               else SetReserveAttribute & SetAttribut*/
/*                       noSet   none                                  */
/*                                                                     */
/*  Transition: Condition:                  Action:                    */
/*       1      receipt of dpi SET          -                          */
/*       1a     receipt of a dpi command    Release all reserved attr's*/
/*              other than SET, COMMIT,     (log agent error)          */
/*              and UNDO.                   & make transition to noSet */
/*       2      receipt of dpi SET.         -                          */
/*       3      receipt of dpi COMMIT       -                          */
/*              for an object SET by                                   */
/*              the current PDU.                                       */
/*       4      receipt of dpi COMMIT       -                          */
/*              for an object SET by                                   */
/*              the current PDU but                                    */
/*              not yet COMMITted.                                     */
/*       5      receipt of a dpi command    - (If command received is  */
/*              other than COMMIT or UNDO,    SET, glide through noSet */
/*              and all objects SET have      right to SET state.)     */
/*              received subsequent COMMIT.                            */
/*       6      receipt of dpi UNDO         -                          */
/*              for an object SET by                                   */
/*              the current PDU.                                       */
/*       7      receipt of dpi UNDO         -                          */
/*              for an object SET by                                   */
/*              the current PDU.                                       */
/*       7      receipt of dpi COMMIT       Release all reserved attr's*/
/*              but not for an object       (log agent error)          */
/*              already SET yet unCOMMITted.                           */
/*       7      receipt of dpi command      Release all reserved attr's*/
/*              other than COMMIT or UNDO,  (log agent error)          */
/*              and COMMIT has not been                                */
/*              received for each object                               */
/*              SET by this PDU.                                       */
/*       8      receipt of dpi UNDO         -                          */
/*              for an object SET by                                   */
/*              the current PDU,                                       */
/*              for which UNDO has                                     */
/*              not yet been performed.                                */
/*       9      receipt of dpi command      -                          */
/*              other than UNDO and                                    */
/*              all objects SET have                                   */
/*              been UNDOne.                                           */
/*       9      receipt of dpi command      Release all reserved attr's*/
/*              other than UNDO and         (log agent error)          */
/*              not all objects SET have                               */
/*              been UNDOne.                                           */
/*       9      receipt of dpi UNDO         Release all reserved attr's*/
/*              for an object NOT SET by    (log agent error)          */
/*              the current PDU or                                     */
/*              for an object not yet UNDOne.                          */
/*                                                                     */
/*  Note: If any state changes occur other than those shown above,     */
/*        ensure any reserved attributes (listed in UndoList) are      */
/*        released.                                                    */
/*  Note: While in states subsequent to SET, agent commands and        */
/*        associated var-binds must be received for all and only       */
/*        those OIDs received during the SET state before proceeding   */
/*        the next state.                                              */
/*        Otherwise, abort the operation and reset the state to noSet. */
/*        (All Set var-binds received before a transition to another   */
/*        state represent one PDU.)                                    */
/*  Note: State change occurs by receiving an SNMP command and         */
/*        associated var-bind from the agent.  Expected command        */
/*        transitions are shown above, and under normal operation noSet*/
/*        is entered once all expected COMMIT or UNDO var-binds have   */
/*        been received.                                               */
/*                                                                     */
/* TIMERS:                                                             */
/* - AttributeReservedTImer                                            */
/*    Meaning - At least one attribute is currently reserved by this   */
/*              sub-agent.  Whenever this is true, the program must    */
/*              ensure the attribute is released, even if the agent    */
/*              becomes disabled and cannot complete the release       */
/*              through the Set operation.                             */
/*    Timer Started - step 1e.                                         */
/*    Timer Stopped & Reset - steps 3f, 2h or timer expiration.        */
/*    Expire Action - step 1k, if last Set sequence was aborted.       */
/* - UndoDataTimer                                                     */
/*    Meaning - Object (attribute) values are retained for a potential */
/*              UNDO, should another sub-agent attached to this agent  */
/*              fail on the COMMIT command within this PDU.  If this   */
/*              occurs, this sub-agent must attempt to restore object  */
/*              values as though the current PDU had never arrived.    */
/*              When this timer expires, the sub-agent assumes all     */
/*              other sub-agents have had sufficient time to complete  */
/*              their COMMIT commands and will discard the old object  */
/*              values.                                                */
/*    Timer Started - step 2g.                                         */
/*    Timer Stopped & Reset - on timer expiration.                     */
/*    Expire Action - step 4g, on entering Set after having traversed  */
/*                    through noSet.                                   */
/*                                                                     */
/* DATA STRUCTURES:                                                    */
/* - XlateList                                                         */
/*   - See DpiGet prolog.                                              */
/* - UndoList                                                          */
/*   - Purpose:  To save previous object values, in case an UNDO occurs*/
/*   - Contents: 1)OID                                                 */
/*               2)old (original) attribute value                      */
/*               3)a bit indicating whether this entry is DmiReserved  */
/*               4)a bit indicating whether this entry is DpiCommitted */
/*               5)a bit indicating whether this entry is DpiUndone    */
/*                                                                     */
/* When entering any state except noSet:                               */
/* a) Start at beginning of XlateList.                                 */
/* b) Search for the next occurrence of the OID prefix in tree.        */
/*    If not found, return noError/noSuchObject.                       */
/* c) Under the current OID prefix, search for the specified group.    */
/*    If not found, return noError/noSuchObject.                       */
/* d) Search for the current component ID.                             */
/*    If not found, return noError/noSuchInstance.                     */
/*                                                                     */
/* 1. SET:                                                             */
/* e)If (1) key neither in OID nor in MIF database for that group,     */
/*          (re)start AttributeReservedTimer and SetReserve the attrib.*/
/*      (2) key in OID and also in MIF database,                       */
/*          (re)start AttributeReservedTimer and SetReserve the attrib.*/
/*      (3) key in OID but not in MIF db, return noError/noSuchInstance*/
/*      (4) key not in OID but in MIF db, return noError/noSuchInstance*/
/*   Note: Multiple reserves can be made, followed by the same number  */
/*         of sets, to handle the case of multiple sets per PDU.       */
/*   Note: If type is OCTETSTRING and length = 8, check if DMI type    */
/*         is INTEGER64, in which case change the type to INTEGER64    */
/*         before making the request.                                  */
/* f)On return from SetReserve, if iStatus:                            */
/*                                       v2 Error-status               */
/*                                       ---------------               */
/*   = (000) Success,                    return noError                */
/*   = (001) More data is available,     return genErr                 */
/*   = (100) Attribute not found,        return noAccess               */
/*   = (101) Value exceeds maximum size, return wrongLength            */
/*   = (102) Component instru.not found, return noAccess               */
/*   = (103) Enumeration error,          return wrongValue             */
/*   = (104) Group not found,            return noAccess               */
/*   = (105) Illegal keys,               return noAccess               */
/*   = (106) Illegal to set,             issue ListAttribute to        */
/*                                       determine the type of error   */
/*                                       (see next 3 three substeps):  */
/*                                              notWritable,           */
/*                                              wrongType,             */
/*                                              wrongLength, or        */
/*                                              wrongValue.            */
/*   = (107) Can't resolve attribute function name, return genErr      */
/*   = (108) Illegal to get,             return genErr                 */
/*   = (109) No description,             return genErr                 */
/*   = (10a) Row not found,              return noAccess               */
/*   = (10b) Direct i/f not registered,  return resourceUnavailable    */
/*         (noAccess not used, since it refers to permanent conditions,*/
/*         where resourceUnavailable refers to transient. -SimpleTimes)*/
/*   = (10c) MIF database is corrupt,    return genErr                 */
/*   = (10d) Attribute is not supported, return noAccess               */
/*   = (200) Buffer full,                return genErr                 */
/*   = (201) Ill-formed command,         return genErr                 */
/*   = (202) Illegal command,            return genErr                 */
/*   = (203) Illegal handle,             return genErr                 */
/*   = (204) Out of memory,              return genErr                 */
/*   = (205) No confirm function,        return genErr                 */
/*   = (206) No response buffer,         return genErr                 */
/*   = (207) Cmd handle already in use,  return genErr                 */
/*   = (208) DMI version mismatch,       return genErr                 */
/*   = (209) Unknown CI registry,        return genErr                 */
/*   = (20a) Command has been canceled,  return genErr                 */
/*   = (20b) Insufficient privileges,    return noAccess               */
/*   = (20c) No access function provided,return resourceUnavailable    */
/*   = (20d) OS File I/O error,          return genErr                 */
/*   = (20e) Could not spawn a new task, return genErr                 */
/*   = (20f) Ill-formed MIF,             return genErr                 */
/*   = (210) Invalid file type,          return genErr                 */
/*   = (211) Service Layer is inactive,  return genErr                 */
/*   = (212) UNICODE not supported,      return genErr                 */
/*   = (1001) Overlay not found          return genErr                 */
/*   = (10005) Component key error       return genErr                 */
/*   = (10009) Component set error       issue ListAttribute to        */
/*                                       determine the type of error   */
/*                                       (see next 3 three substeps):  */
/*                                       if not notWritable,           */
/*                                              wrongType, or          */
/*                                              wrongLength,           */
/*                                       then genErr                   */
/*   = (else)any other error             return genErr                 */
/*     Note: As of 9/94, the Intel implementation of the Service Layer */
/*           should never show any iStatus but the following           */
/*           on issueGetAttribute: 0x0, 0x100, 0x101, 0x105, 0x106,    */
/*           0x107, 0x10b, 0x10c, 0x200, 0x203, 0x204, 0x205,          */
/*           0x208, 0x211, 0x1000, 0x1001, 0x1002, 0x1101, 0x2000,     */
/*           0x3004, 0x3005, 0x3006, 0x10005, and 0x10009.             */
/* g)If AttributeType does not match SNMP object type,                 */
/*   return error-status of wrongType.                                 */
/* h)If AttributeLength does not allow SNMP object length,             */
/*   return error-status of wrongLength.                               */
/* i)If AttributeValue does not allow SNMP object value,               */
/*   return error-status of wrongValue.                                */
/* j)Add an entry to UndoList.                                         */
/* k)If next DPI packet is SET, goto step (1a).  If an UNDO goto       */
/*   UNDObeforeCOMMIT below.  If COMMIT, goto COMMIT below.  Otherwise,*/
/*   it is another type of packet, so Release all Reserved attributes, */
/*   respond to the new DPI request with an error, free OID/old-value  */
/*   storage, and go to noSet.                                         */
/*   (Do this step also if AttributeReservedTimer expires.)            */
/*                                                                     */
/* 2. setCOMMIT:                                                       */
/* e)On COMMIT, issue SetAttribute. If returned iStatus:               */
/*                                       v2 Error-status               */
/*                                       ---------------               */
/*   = (000) Success,                    return noError                */
/*   = (else)any other error             return commitFailed           */
/* f)Update UndoList entry to indicate it is no longer DmiReserved.    */
/* g)If this is the first entry being committed, start UndoDataTimer.  */
/* h)If this is the last entry in UndoList, stop & reset               */
/*   AttributeReservedTimer.                                           */
/*                                                                     */
/* 3. setUNDObeforeCOMMIT:                                             */
/* Entered on UNDO if the OID's DmiReserved bit is set.                */
/* e)If UNDO before COMMIT, release the attribute in UndoList.         */
/*   If successful, return noError.  Otherwise, return undoFailed.     */
/* f)Invalidate the OID's entry in UndoList.                           */
/*   If this is the last one, stop & reset AttributeReservedTimer.     */
/*   Note: Multiple releases can occur for stacked reserves, useful    */
/*         when a PDU contains multiple Sets to any given attribute.   */
/*                                                                     */
/* 4. setUNDOafterCOMMIT:                                              */
/* Entered on UNDO if the OID's DmiReserved bit is reset.              */
/* e)Reserve the attribute as in Set state. If fails, return undoFailed*/
/* f)Set the attribute to its old value as prescribed above.           */
/*   If it fails, return undoFailed.                                   */
/* g)Invalidate the OID's entry in UndoList. (Free it.)                */
/*   (This is done for ALL entries if UndoDataTimer expires or if an   */
/*   unexpected PDU arrives within a Set operation, like Get,GetNext.  */
/*   If this does happen, respond to the request with genErr.)         */
/*                                                                     */
/* Notes:                                                              */
/* 1. Set VAR-BINDs with object type Opaque are processed as follows:  */
/*    a) If the attribute type is date or integer64, reject the request*/
/*    b) If the attribute type is not date or integer64, extract the   */
/*       attribute type from the MIF database and attempt the SET      */
/*       using the extracted type.                                     */
/***********************************************************************/
int doSet(snmp_dpi_hdr *phdr, snmp_dpi_set_packet *ppack, OidPrefix_t *XlateList)
{
   snmp_dpi_set_packet  *pvarbind;
   snmp_dpi_set_packet  *ppack_bak = ppack;
   unsigned char        *ppacket;
   int                   rc, rcdpi, dpierr, generr = DMISA_FALSE;
   long int              i, dpierror = SNMP_ERROR_noError;
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

   pvarbind = snmp_dpi_set_packet_NULL_p;  /* Initialize varBind chain*/

   /* Could adjust this loop to support multi var-bind PDUs           */
   for (i=1;ppack/*i<=1*/;i++) {
#ifdef DMISA_TRACE
      switch (phdr->packet_type) {
         case SNMP_DPI_SET:
            ln = sprintf(logbuffer,"Set OID ");
            break;
         case SNMP_DPI_COMMIT:
            ln = sprintf(logbuffer,"Commit OID ");
            break;
         case SNMP_DPI_UNDO:
            ln = sprintf(logbuffer,"Undo OID ");
            break;
         default:
            break;
      }
      strncpy(logbuffer + ln, ppack->object_p,
              (LOG_BUF_LEN - ln > 0) ? LOG_BUF_LEN - ln : 0);
      DMISA_TRACE_LOGBUF(LEVEL2);
#endif
      if (!strncmp(ppack->object_p,SUN_ARCH_DMI_COMPMIB,
                          strlen(SUN_ARCH_DMI_COMPMIB))) {  /* Component MIB*/
         rc = dpiSetCompMib(
                 &pvarbind,
                 ppack);  /* be able to handle NULL OID instance string*/
         if (rc != SET_COMP_MIB_noError) {  /* if search not successful*/
            if (rc == SET_COMP_MIB_notWritable) {
               /*dpierror = SNMP_ERROR_notWritable; */
                 dpierror = SNMP_ERROR_readOnly;
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
            if (rc == GET_PARSE_INSTANCE_notFullySpecified) {
               /*dpierror = SNMP_ERROR_noAccess; */
                 dpierror = SNMP_ERROR_noSuchName;
                 generr = DMISA_TRUE;
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
               /*dpierror = SNMP_ERROR_noAccess;*/  /* Return noAccess    */
                 dpierror = SNMP_ERROR_noSuchName;
                 generr = DMISA_TRUE;
            } else {

               rc = getTranslateKey(
                       gcpair,
                       keystring,
                       &keyblocklen,
                       &pkeyblock);
               if (rc != GET_TRANSLATE_KEY_noError) {  /* If parse not successful*/
                  if (pkeyblock) free(pkeyblock);      /* Allocated by keyFromDpi()*/
                  if (rc == DMISA_ERROR_noSuchInstanceOrObject) {
                     /*dpierror = SNMP_ERROR_noAccess;*/  /* Return noAccess*/
                       dpierror = SNMP_ERROR_noSuchName;
                       generr = DMISA_TRUE;
                       break;
                  } else return rc;  /* to cover programming error case*/
               } else {

                  rc = setAttribute(  /* could reduce # of arguments by using GCpair (noC,noG,noKC)*/
                          phdr,
                          ppack,
                          compi,
                          groupi,
                          attributei,
                          gcpair->iKeyCount,
                          keyblocklen,
                          pkeyblock,
                          &dpierr);
                  if (pkeyblock) free(pkeyblock);     /* Allocated by keyFromDpi()*/
                  if (rc != SET_ATTRIBUTE_noError) {  /* if set not successful*/
                     generr = DMISA_TRUE;
                     if (rc == SET_ATTRIBUTE_otherError) {
                        DMISA_TRACE_LOG1(LEVEL2, "Error on SET operation: return code = %d.", rc);
                        dpierror = dpierr;
                        break;
                        /*continue; */
                     } else  if (rc == SET_ATTRIBUTE_genErr) {
                        DMISA_TRACE_LOG(LEVEL2, "General error on SET operation.");
                        generr = DMISA_TRUE;
                        dpierror = SNMP_ERROR_genErr;
                        break;
                     } 
                  } /* endif on setAttribute */
               }  /* endif on getTranslateKey */
            } /* endif on getFindGCPair */
         } /* endif on getParseInstance */
      } /* endif on non-CompMib set operations */
      ppack = ppack->next_p;
   } /* endfor */

   if (dpierror == SNMP_ERROR_noError) i = 0L;
   else
   {
      /* Would need to adjust this loop to support multi var-bind PDUs    */
      /*    --i;  ** to identify correct var-bind                         */
      DMISA_TRACE_LOG2(LEVEL3, "Error on SET, dpierror = %ld, i = %ld",dpierror,i);
      /*i = 0L;  Error index  */
   }

#ifndef SOLARIS2
   ppacket = mkDPIresponse(                /* Make DPIresponse packet */
           phdr,                           /* ptr parsed request      */
           dpierror,                       /* error code              */
           i,                              /* index of object in error*/
           pvarbind);                      /* varBind response data   */
   if (!ppacket) {
      DMISA_TRACE_LOG(LEVEL1,"mkDPIresponse failed in doSet function.");
      return DO_SET_dpiBroke;      /* Return if failed   */
   }

   rcdpi = DPIsend_packet_to_agent(        /* send OPEN packet        */
           DpiHandle,                      /* on this connection      */
           ppacket,                        /* this is the packet      */
           DPI_PACKET_LEN(ppacket));       /* and this is its length  */
   if (rcdpi != SNMP_ERROR_DPI_noError) {
      DMISA_TRACE_LOG(LEVEL1,"DPISend_packet_to_agent failed in doSet function.");
      return DO_SET_dpiBroke;      /* Return if failed  */
   }
#else
      if ((phdr->packet_type == SNMP_DPI_SET) &&
         (dpierror == SNMP_ERROR_noError ))
         return  DO_SET_noError;
      pdu = dpihdr_to_pdu(phdr, ppack_bak, dpierror, i); 
      if (!pdu) {
     DMISA_TRACE_LOG(LEVEL1,"dpihdr_to_pdu failed in doSet function.");
      return DO_SET_dpiBroke;      /* Return if failed   */
      }

      if (TraceLevel == 5) {
      printf("Response PDU ..........\n");
      trace_snmp_pdu(pdu);
      }

      if (snmp_pdu_send(socket_handle, &address, pdu, error_label) == -1)  {
     DMISA_TRACE_LOG(LEVEL1,"snmp_pdu_send failed in doSet function.");
      return DO_SET_dpiBroke;      /* Return if failed  */
      }
      
      snmp_pdu_free(pdu);
#endif
   if (generr) return rc;

   return DO_SET_noError;
}


/*********************************************************************/
/* Function: setCleanReserveList()                                   */
/*                                                                   */
/* Input:                                                            */
/*   mode = DMISA_IMMEDIATE- attempt to release all attributes and   */
/*                            free associated storage immediately    */
/*          DMISA_TIMED    - attempt to release only those attributes*/
/*                           that have been reserved for the         */
/*                           specified time-period, and release      */
/*                           storage associated with those attributes*/
/*                                                                   */
/* Unreserve and remove all "old" entries from ReserveList.          */
/* Note that only a best-effort is made, no guarantee that           */
/* the unreserve took place.                                         */
/* All entries older than RESERVE_LIST_DECAY_PERIOD are considered   */
/* old.                                                              */
/* Free the associated OID strings and associated DMI request blocks.*/
/*                                                                   */
/* The 16-byte entry in ReserveList looks like this:                 */
/*      NextEntry  -------> ... -------> NextEntry ----||            */
/*      OidPtr                           OidPtr                      */
/*      TimeReserved                     TimeReserved                */
/*      DmiRequestBlock                  DmiRequestBlock             */
/*                                                                   */
/* Return codes: SET_CLEAN_RESERVE_LIST_noError                      */
/*********************************************************************/
int setCleanReserveList(int mode)
{
   ReserveList_t *entry, *prev, *next;
   time_t         curtime;

   /* Find the OID to remove                                          */
   prev =                /* Assign to remove compiler warning         */
   entry = ReserveList;  /* Global                                    */
   time(&curtime);
   while (entry) {
      next = entry->Next;
      if (mode == DMISA_IMMEDIATE ||
          (curtime - entry->TimeReserved > RESERVE_LIST_DECAY_PERIOD)) { /* If this entry is old*/
         /* Unreserve from DMI, then remove associated storage        */
         DMISA_TRACE_LOG3(LEVEL3, "Auto-release of DMI attribute: C = %ul, G = %ul, A = %ul.",
                    entry->DmiRequestBlock->compId,
                    entry->DmiRequestBlock->groupId,
                    entry->DmiRequestBlock->attribId);
         issueSetAttribute(  /* Return value of this function ignored */
                   &(entry->DmiRequestBlock),
                   entry->DmiRequestBlock->compId,
                   (ULONG)DmiSetReleaseAttributeCmd);
         DMISA_TRACE_LOG3(LEVEL3, "SET Unreserve command sent to Service Layer for"
                               " component %ul, group %ul, attribute %ul.",
                               entry->DmiRequestBlock->compId,
                               entry->DmiRequestBlock->groupId,
                               entry->DmiRequestBlock->attribId);
         if (entry == ReserveList) {              /* If we found a match with the 1st list entry*/
            if (entry->Next) {
               ReserveList = entry->Next;
            } else {
               ReserveList = NULL;                /* List has now become empty*/
            }
         } else {                                 /* Else the match is not on the 1st list entry*/
            if (entry->Next) {                    /* If a next entry exists,*/
               prev->Next = entry->Next;          /* bridge over this entry*/
            } else {
               prev->Next = NULL;                 /* else shorten list from end*/
            }
         }
         free(entry->OidPtr);
         if (entry->DmiRequestBlock) {
            free_setAttributeIN(entry->DmiRequestBlock);      /* moved 950120.gwl */
         }
         free(entry);
      }
      prev = entry;
      entry = next;
   }

   return SET_CLEAN_RESERVE_LIST_noError;
}


/*********************************************************************/
/* Function: setAttribute()                                          */
/*                                                                   */
/* Set the specified attribute from DMI                              */
/*                                                                   */
/* Return codes: SET_ATTRIBUTE_noError                               */
/*               SET_ATTRIBUTE_otherError                            */
/*               SET_ATTRIBUTE_badPacketType                         */
/*               SET_ATTRIBUTE_genErr    \__combine?                 */
/*               SET_ATTRIBUTE_dmiBroke  /                           */
/*               ISSUE_SET_ATTRIBUTE_outOfMemory                     */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*********************************************************************/
static int setAttribute(snmp_dpi_hdr *phdr, snmp_dpi_set_packet *pack,
           ULONG compi, ULONG groupi, ULONG attributei, ULONG keycount,
           ULONG keyblocklen, DMI_GroupKeyData_t *pkeyblock,
           int *snmperror)
{
   DmiSetAttributeIN      *setattribin;
   ULONG                   attributetype, dbtype;
   int                     rcsub, rc = SET_ATTRIBUTE_noError;
   ULONG                   length, dmistat, reqbuflen, cmd;
   int                     int32;
   DmiInt64Str            *dmi64, int64;
   char                    mifDate[28];
   char                   *pattributevalue;
#ifdef INT64_IS_OCTETSTRING
   int                     i;
   char                   *snmp64int, *dmi64int;
#endif
   snmp_dpi_u64           loc64;
   snmp_dpi_u64           *snmp64;
   snmp64 = &(loc64);  /* Initialize                                  */

   /* Type translation - General case                                 */
   *snmperror = SNMP_ERROR_noError;    /* Initialize                  */
   rcsub = xlateType(&attributetype, (ULONG)(pack->value_type), TODMI, &length);  /* length is not used here*/
   if (rcsub != XLATE_TYPE_noError) {
      return SET_ATTRIBUTE_genErr;
   }

   /* Type translation - Special cases (conversions to INTEGER64 and DATE)*/
   if (attributetype == MIF_OCTETSTRING
#ifndef INT64_IS_OCTETSTRING
       || attributetype == MIF_COUNTER64
#endif
 ) {
      rcsub = setExtractType(
                    &dbtype,
                    compi,
                    groupi,
                    attributei);
      if (rcsub == FIND_ATTRIBUTE_TYPE_notFound) {
         *snmperror = SNMP_ERROR_noAccess;
         DMISA_TRACE_LOG(LEVEL2, "Attribute not found.");
         return SET_ATTRIBUTE_otherError;
      } else if (rcsub == SET_EXTRACT_TYPE_unexpectedType) {
         *snmperror = SNMP_ERROR_wrongType;
         return SET_ATTRIBUTE_otherError;
      } else if (rcsub != SET_EXTRACT_TYPE_noError) return rcsub;

      if (
#ifdef INT64_IS_OCTETSTRING
          attributetype == MIF_OCTETSTRING &&
#else
          attributetype == MIF_COUNTER64 &&
#endif
          dbtype == MIF_INTEGER64) {
         if (pack->value_len == sizeof(snmp_dpi_u64)) {
            attributetype = MIF_INTEGER64;  /* Convert SNMP octet string to 64-bit integer*/
         } else {
            *snmperror = SNMP_ERROR_wrongLength;
            DMISA_TRACE_LOG(LEVEL2, "Object in SET operation has wrong length.");
            return SET_ATTRIBUTE_otherError;
         }
      } else if (attributetype == MIF_OCTETSTRING &&
                 dbtype == MIF_DATE) {
         if (pack->value_len == DATE_LENGTH_snmp) {
            attributetype = MIF_DATE;  /* Convert SNMP octet string to MIF date*/
         } else {
            *snmperror = SNMP_ERROR_wrongLength;
            DMISA_TRACE_LOG(LEVEL2, "Object in SET operation has wrong length.");
            return SET_ATTRIBUTE_otherError;
         }
      } else if (attributetype == MIF_OCTETSTRING &&
                 dbtype == MIF_DISPLAYSTRING) {
         attributetype = MIF_DISPLAYSTRING;  /* Convert SNMP octet string to display string*/
      }
   } /* end "Type translation - Special cases (conversions to INTEGER64 and DATE)" */
#ifdef COUNTER64_IS_COUNTER
   else if (attributetype == MIF_COUNTER ) {
      rcsub = findAttributeType( &dbtype, compi, groupi, attributei);
      if (rcsub == FIND_ATTRIBUTE_TYPE_noError) {
         if ((dbtype != MIF_COUNTER64) && (dbtype != MIF_COUNTER)) {
            DMISA_TRACE_LOG(LEVEL2, "Object in SET operation is of unexpected type.");
            return SET_EXTRACT_TYPE_unexpectedType;
         }
      } /* no error from findAttributeType */
      else if (rcsub == FIND_ATTRIBUTE_TYPE_notFound) {
         *snmperror = SNMP_ERROR_noAccess;
         DMISA_TRACE_LOG(LEVEL2, "Attribute not found.");
         return SET_ATTRIBUTE_otherError;
      } else if (rcsub == SET_EXTRACT_TYPE_unexpectedType) {
         *snmperror = SNMP_ERROR_wrongType;
         return SET_ATTRIBUTE_otherError;
      }
   } /* end counter special case */
#endif


   /* Move object value from SNMP to DMI.  This code converts the data from */
   /* SNMP format to DMI internal format where necessary.                   */
   if (attributetype == MIF_DATE) {  /* If target attribute is of date type*/
      memcpy(mifDate, pack->value_p, DATE_LENGTH_dmi);
      memset(mifDate + DATE_LENGTH_snmp, 0,
             DATE_LENGTH_dmi - DATE_LENGTH_snmp);
      pattributevalue = mifDate;
   } else if (attributetype == MIF_INTEGER64) {  /* target attribute is a 64-bit integer*/
#ifdef INT64_IS_OCTETSTRING
      dmi64int  = (char *)&int64;
      snmp64int = pack->value_p;
      swap64((char *)snmp64int,(char *)dmi64int);
      pattributevalue = dmi64int;
#else
      dmi64  = &int64;
      snmp64 = (snmp_dpi_u64 *)pack->value_p;
      dmi64->high = snmp64->high;  /* This swaps the two halves. (See structures.)*/
      dmi64->low  = snmp64->low;
      pattributevalue = (char *)dmi64;
#endif
#ifdef COUNTER64_IS_COUNTER
   } else if ((attributetype == MIF_COUNTER ) && (dbtype == MIF_COUNTER64)) {
        dmi64  = &int64;
#if defined(OS2) || defined(WIN32)
        dmi64->high = 0;
        dmi64->low = *((ULONG *)(pack->value_p));
#else
        snmp64->high = 0;
        snmp64->low = *((ULONG *)(pack->value_p));
        swap64((char *)snmp64,(char *)dmi64);
#endif
        pattributevalue = (char *)dmi64;
        attributetype = MIF_COUNTER64;
        pack->value_len = sizeof(DmiInt64Str);
#else
   } else if (attributetype == MIF_COUNTER64) {  /* If the target attribute is a 64-bit counter */
      dmi64  = &int64;
      snmp64 = (snmp_dpi_u64 *)pack->value_p;
      dmi64->high = snmp64->high;  /* This swaps the two halves. (See structures.)*/
      dmi64->low  = snmp64->low;
      pattributevalue = (char *)dmi64;
#endif
   } else {  /* attribute is not of 64-bit or date type               */
      pattributevalue = pack->value_p; 
   } /* end "Move object value from SNMP to DMI" */


   /* Now that we've settled on the type, provided we were given a unique */
   /* (i.e, non-opaque) SNMP type, check to see if this is a bad type.*/
   /* We cannot rely on the Service Layer to check this for us on the SET,*/
   /* since no type is specified in the PDU.                          */
   /* Additionally, since we need to report notWritable if both wrongType and notWritable,*/
   /* we need to explicitly check whether the attribute is writable before attempting a SET.*/
   /* May as well check for wrongLength now, since we've already got that information.*/
   /* Note: Since any of these errors prevents an attribute from being reserved, it is*/
   /* appropriate that the UNDOs, which would result in releases, are equally filtered*/
   /* from the Service Layer.                                         */
   rcsub = setCheckIllegal(
              snmperror,
              pack->value_len,       /* Attribute value length        */
              attributetype,         /* Attribute type                */
              compi,
              groupi,
              attributei);
   if (rcsub == SET_CHECK_ILLEGAL_gotError) {
      if (phdr->packet_type != SNMP_DPI_UNDO) {  /* If UNDO, then return noErr, since*/
                                                 /* neither reserve nor set occurred*/
         rc = SET_ATTRIBUTE_otherError;
      }
      return rc;
   } else if (rcsub != SET_CHECK_ILLEGAL_noError) {
      return rcsub;
   }
   switch (phdr->packet_type) {
   case SNMP_DPI_SET:
      cmd = DmiSetReserveAttributeCmd;
      break;
   case SNMP_DPI_COMMIT:
      cmd = DmiSetAttributeCmd;
      break;
   case SNMP_DPI_UNDO:
      cmd = DmiSetReleaseAttributeCmd;
      break;
   default:
      return SET_ATTRIBUTE_badPacketType;
   /* break; // This line is commented out                            */
   }

   rcsub = buildSetAttribute(
                &setattribin,
                groupi,
                attributei,
                keycount,
                keyblocklen,
                pkeyblock,
                pack->value_len +
                    (attributetype == MIF_DATE) *
                    (DATE_LENGTH_dmi - DATE_LENGTH_snmp),  /* Attribute value length*/
                attributetype,
                pattributevalue);
   if (rcsub != BUILD_SET_ATTRIBUTE_noError) {
      if (setattribin) 
           free_setAttributeIN(setattribin);
      return rcsub;
   }

   rcsub = issueSetAttribute(
                &setattribin,
                compi,
                cmd); /* command = Reserve, Set, Release               */
   if (rcsub != ISSUE_SET_ATTRIBUTE_noError) {
      if (setattribin) {
          free_setAttributeIN(setattribin);
      }
      return rcsub;
   }

   if (cmd == DmiSetAttributeCmd) {
      if (rcsub == ISSUE_SET_ATTRIBUTE_noError) {
         *snmperror = SNMP_ERROR_noError;
         rc = SET_ATTRIBUTE_noError;
      } else {
         *snmperror = SNMP_ERROR_commitFailed;
         DMISA_TRACE_LOG1(LEVEL2, "COMMIT phase of SET operation failed. iStatus = %u", *snmperror);
         rc = SET_ATTRIBUTE_otherError;
      }
   } else if (cmd == DmiSetReleaseAttributeCmd) {
      *snmperror = SNMP_ERROR_undoFailed;
      DMISA_TRACE_LOG(LEVEL2, "UNDO phase of SET operation failed.");
      rc = SET_ATTRIBUTE_otherError;
   } else if (cmd == DmiSetReserveAttributeCmd) {
      switch (rcsub) {
         case ISSUE_SET_ATTRIBUTE_noError:    /* No error (0x0000)     */
            *snmperror = SNMP_ERROR_noError;  /* Return noError       */
             rc = SET_ATTRIBUTE_noError;
            break;
         default:
            *snmperror = SNMP_ERROR_genErr;
            rc = SET_ATTRIBUTE_genErr;
            break;
      }
   }

   if (cmd == DmiSetReserveAttributeCmd && rcsub == ISSUE_SET_ATTRIBUTE_noError) {
      /* Add OID to SetReserveList & retain this DMI request buffer   */
      rcsub = setAddToReserveList(
                pack,
                attributetype,
                setattribin);    /* Note: Don't free the request buffer in this case*/
   } else {
      if (cmd != DmiSetReserveAttributeCmd) {
      /* Remove OID from SetReserveList & free up the original request buffer*/
      rcsub = setRemoveFromReserveList(
                pack);
      if (setattribin) {
         free_setAttributeIN(setattribin);
      }
   } else {                    /* If error on Reserve                 */
      if (setattribin) {
           free_setAttributeIN(setattribin);
      }
   }
   }
   return rc;
}


/*********************************************************************/
/* Function: setExtractType()                                        */
/*                                                                   */
/* Determine whether the attribute is a 64-bit integer, in which     */
/* case the calling function will change the type from OCTETSTRING   */
/* to INTEGER64.                                                     */
/*                                                                   */
/* Return codes: SET_EXTRACT_TYPE_noError                            */
/*               SET_EXTRACT_TYPE_unexpectedType                     */
/*               ISSUE_LIST_ATTRIBUTE_outOfMemory                    */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*********************************************************************/
static int setExtractType(ULONG *type, ULONG compi, ULONG groupi, ULONG attributei)
{
   int rc;

   rc = findAttributeType(
            type,
            compi,
            groupi,
            attributei);

   if (rc != FIND_ATTRIBUTE_TYPE_noError) return rc;

   if (*type != MIF_INTEGER64 &&
       *type != MIF_DATE &&
       *type != MIF_DISPLAYSTRING &&
       *type != MIF_OCTETSTRING &&
       *type != MIF_COUNTER64) {
      DMISA_TRACE_LOG(LEVEL2, "Object in SET operation is of unexpected type.");
      return SET_EXTRACT_TYPE_unexpectedType;
   }

   return SET_EXTRACT_TYPE_noError;
}


/*********************************************************************/
/* Function: setCheckIllegal()                                       */
/*                                                                   */
/* Determine the cause of error during SET, in this order:           */
/*   (1) the object is not writable                                  */
/*   (2) the type in the request is incompatible with the object's type*/
/*   (3) the length in the request is not allowed by this object     */
/*                                                                   */
/* Return codes: SET_CHECK_ILLEGAL_noError                           */
/*               SET_CHECK_ILLEGAL_gotError                          */
/*               SET_CHECK_ILLEGAL_dmiBroke                          */
/*               ISSUE_LIST_ATTRIBUTE_outOfMemory                    */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed               */
/*********************************************************************/
static int setCheckIllegal(int *error, ULONG reqlength, ULONG reqtype,
                      ULONG compi, ULONG groupi, ULONG attributei)
{
   DmiListAttributesOUT *listattrout;
   DmiAttributeInfo_t   *attrinfo;
   ULONG   dmistat;
   int     rcsub, rc = SET_CHECK_ILLEGAL_noError;

   rcsub = issueListAttribute(
           &listattrout,
           compi,
           groupi,
           attributei,
           DMISA_THIS,
           1);

   if (rcsub != ISSUE_LIST_ATTRIBUTE_noError) {
          if (listattrout->error_status != DMIERR_SP_INACTIVE)
                  rcsub = SET_CHECK_ILLEGAL_gotError;
             if (listattrout)
                free_listattrout(listattrout);

             if (rcsub == ISSUE_LIST_ATTRIBUTE_failed)
                 return SET_CHECK_ILLEGAL_dmiBroke;
             else return rcsub;
   } else {
      attrinfo = listattrout->reply->list.list_val;
      DMISA_TRACE_LOG2(LEVEL4, "Maximum allowed attribute size = %ul.  SET attempted with %ul size.", attrinfo->maxSize, reqlength);
         if((attrinfo->access & MIF_ACCESS_MODE_MASK) ==
                 MIF_READ_ONLY) {
            *error = SNMP_ERROR_notWritable;
            DMISA_TRACE_LOG(LEVEL1, "SET attempted to a read-only attribute");
            rc = SET_CHECK_ILLEGAL_gotError;
         } else if (reqtype != attrinfo->type) {
            *error = SNMP_ERROR_wrongType;
            DMISA_TRACE_LOG2(LEVEL1, "Expected attribute type is %u.  SET attempted with %u type.", attrinfo->type, reqtype);
            rc = SET_CHECK_ILLEGAL_gotError;
         } else if (reqlength > attrinfo->maxSize) {
            *error = SNMP_ERROR_wrongLength;
            DMISA_TRACE_LOG2(LEVEL1, "Maximum allowed attribute size = %ul.  SET attempted with %ul size.", attrinfo->maxSize, reqlength);
            rc = SET_CHECK_ILLEGAL_gotError;
         }
   }

   if (listattrout) {
      free_listattrout(listattrout);
   }

   return rc;
}


/*********************************************************************/
/* Function: setAddToReserveList()                                   */
/*                                                                   */
/* Add an entry containing this OID to the end of ReserveList.       */
/* Copy the associated OID string and do not free the associated     */
/* DMI request block.                                                */
/*                                                                   */
/* The 16-byte entry in ReserveList looks like this:                 */
/*      NextEntry  -------> ... -------> NextEntry ----||            */
/*      OidPtr                           OidPtr                      */
/*      TimeReserved                     TimeReserved                */
/*      DmiRequestBlock                  DmiRequestBlock             */
/*                                                                   */
/* Return codes: SET_ADD_TO_RESERVE_LIST_noError                     */
/*********************************************************************/
static int setAddToReserveList(snmp_dpi_set_packet *pack,
          ULONG attributetype, DmiSetAttributeIN  *setattribin)
{
   ReserveList_t *currententry, *entry;
   char          *currentoid;
   int            rc = 0;
#ifdef OS2
#elif defined WIN32
TID  ThreadId;
#else
thread_t WatchThread; /* thread id */
/*pthread_attr_t WatchThreadAttrs; Not reqd in Solaris threads */
#endif

   currentoid = (char *)malloc(strlen(pack->object_p) + 1);
   if (!currentoid) return SET_ADD_TO_RESERVE_LIST_outOfMemory;  /* added 950317.gwl */
   strcpy(currentoid, pack->object_p);    /* Assumption: pack->object_p will always be valid*/

   currententry = (ReserveList_t *)malloc(sizeof(ReserveList_t));
   if (!currententry) return SET_ADD_TO_RESERVE_LIST_outOfMemory;  /* added 950317.gwl */
   currententry->AttributeType = attributetype;  /* DMI attribute type*/
   currententry->Next = NULL;
   currententry->OidPtr = currentoid;
   time(&currententry->TimeReserved);
   currententry->DmiRequestBlock = setattribin;

   if (!ReserveList) {  /* Start ReserveList                          */
      ReserveList = currententry;
   } else {             /* Find end of ReserveList and tack onto end of list*/
      entry = ReserveList;
      while (entry != NULL && entry->Next != NULL) {
         entry = entry->Next;
      }
      if (entry) entry->Next = currententry;
   }

   if (!WatchReserveListFlag) {
#ifdef OS2
      rc = _beginthread(setWatchReserveList,
                   NULL,
                   8192,
                   NULL);
      if (rc != -1)
         rc = 0;
#elif defined WIN32         /* LLR 09-12-95  added for WIN32 */
      ThreadId = _beginthread(setWatchReserveList,
                         8192,
                         NULL);
          if (ThreadId == (TID)-1) {
            rc = 1;       /* Thread creation failed.*/
          } else {
            rc = 0;
          }
#elif AIX325
       rc = pthread_create(&WatchThread,
                           pthread_attr_default,
                           setWatchReserveList,
                           NULL);
#else
       rc = thr_create(NULL, NULL, setWatchReserveList, NULL,
                         NULL, &WatchThread);
         
#endif
      if (rc) {
         DMISA_TRACE_LOG(LEVEL1, "Unable to create thread for ReserveList maintenance.");
      } else {
         WatchReserveListFlag = DMISA_TRUE;
         DMISA_TRACE_LOG(LEVEL3, "Began new thread in the event any reserved attributes need to be unreserved.");
      }
   }

   return SET_ADD_TO_RESERVE_LIST_noError;
}


/*********************************************************************/
/* Function: setRemoveFromReserveList()                              */
/*                                                                   */
/* Remove the entry containing this OID, if exists, from ReserveList.*/
/* Free the associated OID string and the associated DMI request     */
/* block.                                                            */
/*                                                                   */
/* The 16-byte entry in ReserveList looks like this:                 */
/*      NextEntry  -------> ... -------> NextEntry ----||            */
/*      OidPtr                           OidPtr                      */
/*      TimeReserved                     TimeReserved                */
/*      DmiRequestBlock                  DmiRequestBlock             */
/*                                                                   */
/* Return codes: SET_REMOVE_FROM_RESERVE_LIST_noError                */
/*********************************************************************/
static int setRemoveFromReserveList(snmp_dpi_set_packet *pack)
{
   ReserveList_t *entry, *prev, *next;

   /* Find the OID to remove                                          */
   prev =                /* Assign to remove compiler warning         */
   entry = ReserveList;  /* Global                                    */

   while (entry) {
      next = entry->Next;
      if (!strcmp(entry->OidPtr,pack->object_p)) { /* If a match is found*/
         if (entry == ReserveList) {               /* If we found a match with the 1st list entry*/
            if (entry->Next) {
               ReserveList = entry->Next;
            } else {
               ReserveList = NULL;                 /* List has now become empty*/
            }
         } else {                                  /* Else the match is not on the 1st list entry*/
            if (entry->Next) {                     /* If a next entry exists,*/
               prev->Next = entry->Next;           /* bridge over this entry*/
            } else {
               prev->Next = NULL;                  /* else shorten list from end*/
            }
         }
         free(entry->OidPtr);
         if (entry->DmiRequestBlock) {
            free_setAttributeIN(entry->DmiRequestBlock);  /* Free old request block, moved 950120.gwl */
         }
         free(entry);
      }
      prev = entry;
      entry = next;
   }

   return SET_REMOVE_FROM_RESERVE_LIST_noError;
}


/*********************************************************************/
/* Function: setWatchReserveList()                                   */
/*                                                                   */
/* Run in its own thread, this function cleans out old entries in    */
/* ReserveList until there are no more entries.                      */
/*                                                                   */
/* The 16-byte entry in ReserveList looks like this:                 */
/*      NextEntry  -------> ... -------> NextEntry ----||            */
/*      OidPtr                           OidPtr                      */
/*      TimeReserved                     TimeReserved                */
/*      DmiRequestBlock                  DmiRequestBlock             */
/*                                                                   */
/* Return codes: none                                                */
/*********************************************************************/
#if defined(OS2) || defined(AIX325) || defined(WIN32)
static void setWatchReserveList(void *pvoid)
#else
void *setWatchReserveList(void *pvoid)
#endif
{
int permloop = DMISA_TRUE;
   int rcdos;

   while (permloop) {
      DMI_SLEEP(RESERVE_LIST_CLEANUP_PAUSE); /* Sleep before next connection attempt*/
      if (ReserveList) {
         /* Get mutex semaphore                                          */
#ifdef WIN32    /* LLR 09-13-95 added for WIN32 */

      rcdos = WaitForSingleObject(DmiAccessMutex, INFINITE);
          DMISA_TRACE_LOG1(logbuffer,"DosRequestMutexSem error: return code = %u.", rcdos);

          if (rcdos != WAIT_FAILED)
             rcdos = 0;

#elif defined OS2
         rcdos = DosRequestMutexSem(DmiAccessMutex, -1);  /* Wait forever*/
#else
         rcdos = mutex_lock(&DmiAccessMutex);
#endif
         if (rcdos != 0) {  /* can get 0, 6invhndl, 95interrupt, 103toomanysemreq,*/
                            /*         105semownerdied, 640timeout       */
            DMISA_TRACE_LOG1(LEVEL1,"DosRequestMutexSem error: return code = %u.", rcdos);
            DMISA_TRACE_LOG(LEVEL1, "Some attributes may not become unreserved.");
#if defined(OS2) || defined(AIX325) || defined(WIN32)
            return;
#else
            return NULL;
#endif
         }

         /* Clean up the list                                            */
         setCleanReserveList(DMISA_TIMED);

         /* Release mutex semaphore                                      */
#ifdef WIN32                                       /* LLR 09-13-95 for WIN32 */
      if (ReleaseMutex(DmiAccessMutex))
            rcdos = 0;
      else
            rcdos = 1;

#elif defined OS2
         rcdos = DosReleaseMutexSem(DmiAccessMutex);  /* Release Mutex*/
#else
         mutex_unlock(&DmiAccessMutex); /* Release Mutex*/
         /*pthread_yield(); */ /*This is not a valid call in POSIX threads*/
#endif
         if (rcdos != 0) {  /* can get 0, 6invhndl, 288notowner          */
            DMISA_TRACE_LOG1(LEVEL1,"DosRequestMutexSem error: return code = %u.", rcdos);
            DMISA_TRACE_LOG(LEVEL1, "Some attributes may not become unreserved.");
            DMISA_TRACE_LOG1(LEVEL1,"DosRequestMutexSem error: return code = %u.", rcdos);
            DMISA_TRACE_LOG(LEVEL1, "Some attributes may not become unreserved.");
#if defined(OS2) || defined(AIX325) || defined(WIN32)
            return;
#else
            return NULL;
#endif
         }
      } /* if (ReserveList) */
   } /* while (DMISA_TRUE) */
}
