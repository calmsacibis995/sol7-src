/* Copyright 10/11/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmiskeyn.c	1.6 96/10/11 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmiskeyn.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions to support DMI access.             */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        doGetNext(...)              */
/*                                        gnGetNextMifObject(...)     */
/*                                        gnParseInstance(...)        */
/*                                        gnFindNextObject(...)       */
/*                                        gnFindGroup(...)            */
/*                                        gnFindAttribute(...)        */
/*                                        gnFindComponent(...)        */
/*                                        gnFindKey(...)              */
/*                                        gnGetObject(...)            */
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
/*                  940802    LESKA     compareKeyValues              */
/*                  940818    LAUBLI    All other functions           */
/*                  950317    LAUBLI    Changed malloc check          */
/*                                                                    */
/* End Change Log *****************************************************/

#include "dmisamap.h"
#include "dmisa.h"

typedef struct {        /* 8 byte integer, used for signed & unsigned */
      unsigned long int high;              /*    - Most Significant   */
      unsigned long int low;               /*    - Least Significant  */
   } _Int64;            /* 8 byte integer, used for signed & unsigned */

extern char logbuffer[];
#ifdef DMISA_TRACE
void TraceReqBuf(DMI_MgmtCommand_t *mgmtreq);
extern unsigned char logbuffer[];
#endif

static int  translateInternalKeyToDmi(GCPair_t *gcpair, char *thiskeyvalue,
                  ULONG keysize, DMI_GroupKeyData_t *keyblock);
static int  translateDmiKeyToInternal(KeyValueList_t **internalkey,
                  DMI_GroupKeyData_t *pattern, GCPair_t *gcpair);
static int  getInternalKeySize(KeyValueList_t *internalkey, ULONG *keyvaluesize,
                  GCPair_t *gcpair);
static int  getDmiKeySize(DMI_GroupKeyData_t *dmikey, int *keyvaluesize,
                  GCPair_t *gcpair);
static int  pullKeyValues(GCPair_t *gcpair);
static int  compareKeyValues(GCPair_t *, void *, void *);
static int  fillDummyKey(GCPair_t *gcpair, ULONG *keysize,
                  DMI_GroupKeyData_t **keyblock);

/**********************************************************************/
/* Function: getKeyValue()      (next key or lowest key)              */
/*                                                                    */
/* Input parameters:                                                  */
/*    command: Next key or Lowest key                                 */
/*    ptr. to current group/component structure in XlateList          */
/*    ptr. to pattern key - needed only if command = Next.            */
/*                  It is a structure of key attribute values,        */
/*                  translated from the OID (may be longer than full  */
/*                  DMI key).  It is output from keyFromDpi.          */
/*                                                                    */
/* Output parameters:                                                 */
/*    ptr. to DpiKey string, in DMI format.                           */
/*                                                                    */
/* Description:                                                       */
/*   Gets the lexicographically lowest or next key value, whichever   */
/*   specified, and returns it in dotted decimal format.              */
/*   1. Get all key values from the DMI table using pullKeyValues.    */
/*   2. a) If the lowest key is wanted, compare keys in pairs (using  */
/*         compareKeyValues), maintaining a ptr. to lowest key found. */
/*      b) If the next key beyond the pattern key is wanted, compare  */
/*         each key value to the pattern key (using compareKeyValues).*/
/*         - If DMI key value is lower (or equal), continue.          */
/*         - If DMI key value is higher, compare it to the            */
/*           next-higher key, and if lower, point nexthigher ptr.     */
/*           to it.                                                   */
/*   3. Return the selected key and appropriate status.               */
/*                                                                    */
/* Notes:                                                             */
/*   1. The keys could be sorted in lexi-order rather than simply     */
/*      searching for the next or first key.  This would cut          */
/*      key comparison processing time to 1/3 the time (1.5n x n      */
/*      vs. (n x n)/2) for a MIB walk.  However, this overhead is     */
/*      not spread out over time.                                     */
/*                                                                    */
/*   2. *wantedkey points to storage allocated but not freed          */
/*      by this function.  It must be freed by the caller.            */
/*                                                                    */
/* Return codes: GET_KEY_VALUE_noError                                */
/*               GET_KEY_VALUE_noneHigher                             */
/*               GET_KEY_VALUE_otherError                             */
/*               GET_KEY_VALUE_outOfMemory                            */
/*               PULL_KEY_VALUES_otherError                           */
/*               PULL_KEY_VALUES_dmiBroke                             */
/*               PULL_KEY_VALUES_cannotGetToThem                      */
/*               PULL_KEY_VALUES_outOfMemory                          */
/*               ISSUE_GET_ROW_outOfMemory                            */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed                */
/*               TRANSLATE_DMI_KEY_TO_INTERNAL_otherError             */
/*               GET_DMI_KEY_SIZE_badAttributeType                    */
/**********************************************************************/
int getKeyValue(int command, GCPair_t *gcpair,
            DMI_GroupKeyData_t *pattern, DMI_GroupKeyData_t **wantedkey,
            ULONG *keysize)
{
   KeyValueList_t *best, *next, *internalpattern;
   int             rc;
   int             higherfound = DMISA_FALSE;

   *wantedkey = NULL;  /* Initialize                                  */
   rc = pullKeyValues(
           gcpair);     /* Implicitly pass KeyValueList, too          */

   if (rc != PULL_KEY_VALUES_noError &&
       rc != PULL_KEY_VALUES_noErrorOldListStillGood) {
      if (KeyValueList) freeKeyValueList(); /* Free bad KeyValueList  */
      return rc;
   }

   if (command == DMISA_LOWEST) {
      best = KeyValueList;          /* initialize                     */
      if (KeyValueList->pNextValue) {  /* if more than one key value, find the one we need*/
         next = KeyValueList->pNextValue;  /* to compare 1st to 2nd in list*/
         for (; next != NULL; next = next->pNextValue) {
            rc = compareKeyValues(
                      gcpair,
                      (void *)(next + 1),   /* (incremented to get past the pointer)*/
                      (void *)(best + 1));  /* lowest so far          */
            if (rc == COMPARE_KEY_VALUES_lower) best = next;  /* to get lowest value*/
         }
      }
   } else if (command == DMISA_NEXT) {  /* it must be either next or lowest*/
      rc = translateDmiKeyToInternal(
              &internalpattern,    /* Internal key format             */
              pattern,             /* DMI key format                  */
              gcpair);
      if (rc != TRANSLATE_DMI_KEY_TO_INTERNAL_noError) {
         if (internalpattern) free(internalpattern);
         return rc;
      }

      next = KeyValueList;          /* initialize                     */
      if (KeyValueList->pNextValue) {  /* if more than one key value, find the one we need*/
         for (; next != NULL; next = next->pNextValue) {
            rc = compareKeyValues(
                   gcpair,
                   (void *)(next + 1),
                   (void *)(internalpattern + 1));  /* Our starting-point key*/
            if (!higherfound && rc == COMPARE_KEY_VALUES_higher) {
               best = next;
               higherfound = DMISA_TRUE;
            } else if (higherfound && rc == COMPARE_KEY_VALUES_higher) {  /* if next is higher*/
               rc = compareKeyValues(
                      gcpair,
                     (void *)(next + 1),
                     (void *)(best + 1));  /* next-higher             */
               if (rc == COMPARE_KEY_VALUES_lower) best = next;
                        /* to get the lower of the two keys, both higher than pattern*/
            }
         } /* endfor */
      }
      free(internalpattern);
   } else {
      return GET_KEY_VALUE_otherError;  /* Should never get here      */
   }

   if (command == DMISA_NEXT && !higherfound) {
      return GET_KEY_VALUE_noneHigher;
   }

   getInternalKeySize(
             best,  /* the key we want the size of                    */
             keysize,
             gcpair);
   *keysize += gcpair->iKeyCount * sizeof(DMI_GroupKeyData_t);

   *wantedkey = (DMI_GroupKeyData_t *)malloc(*keysize); /* Allocate space for key in DMI format*/
   if (!*wantedkey) return GET_KEY_VALUE_outOfMemory;  /* changed 950317.gwl */

   rc = translateInternalKeyToDmi(
           gcpair,
           (char *)(best + 1), /* values of lowest or next-highest key (see above)*/
           *keysize,       /* length of key values                    */
           *wantedkey);    /* destination of key in GroupKeyData type form*/

   return GET_KEY_VALUE_noError;
}

/**********************************************************************/
/* Function: getSequentialKeyValue()      (next key or lowest key)    */
/*                                                                    */
/* Input parameters:                                                  */
/*    command: Next key or Lowest key                                 */
/*    ptr. to current group/component structure in XlateList          */
/*    ptr. to pattern key - needed only if command = Next.            */
/*                  It is a structure of key attribute values,        */
/*                  translated from the OID (may be longer than full  */
/*                  DMI key).  It is output from keyFromDpi.          */
/*                                                                    */
/* Output parameters:                                                 */
/*    ptr. to DpiKey string, in DMI format.                           */
/*                                                                    */
/* Description:                                                       */
/*   Gets the lexicographically lowest or next key value, whichever   */
/*   specified, and returns it in dotted decimal format, in the case  */
/*   where we are assuming sequential keys in the group.              */
/*                                                                    */
/* Return codes: GET_KEY_VALUE_noError                                */
/*               GET_KEY_VALUE_noneHigher                             */
/*               GET_KEY_VALUE_otherError                             */
/*               GET_KEY_VALUE_outOfMemory                            */
/*               PULL_KEY_VALUES_otherError                           */
/*               PULL_KEY_VALUES_dmiBroke                             */
/*               PULL_KEY_VALUES_cannotGetToThem                      */
/*               PULL_KEY_VALUES_outOfMemory                          */
/*               ISSUE_GET_ROW_outOfMemory                            */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed                */
/*               TRANSLATE_DMI_KEY_TO_INTERNAL_otherError             */
/*               GET_DMI_KEY_SIZE_badAttributeType                    */
/**********************************************************************/

int getSequentialKeyValue(int command, GCPair_t *gcpair,
            DMI_GroupKeyData_t *pattern, DMI_GroupKeyData_t **wantedkey,
            ULONG *pkeysize)
{
   DMI_GroupKeyData_t *keyblock;
   int                rc = GET_KEY_VALUE_noError;
   int                iAttribId = 0; /* i think this should be a parm $MED */
   DmiGetMultipleOUT  *getmultout;
   DmiRowData_t       *rowdata;
   DmiAttributeData_t *keyfromdmi;

   DMI_GroupKeyData_t *nextkey;
   DmiString_t        *keystring;
   GCKey_t            *gckey;
   char               *keyvalue, *keystart;
   LONG               *keyvalue_int;
   ULONG              *keyvalue_ul;
   DmiInt64Str        *keyvalue_64, *dmi64;
   int                 i,j,k, rcsub, cmd;
   ULONG               keysize = 0UL;
   ULONG               dmistat;

   *wantedkey = NULL;  /* Initialize                                  */

   if (command == DMISA_LOWEST) {
      cmd = DMISA_FIRST;
      /* fill key with zeroes for GetfirstRow, since SL will want a key */
      rcsub = fillDummyKey( gcpair, &keysize, &nextkey);
      if (rcsub != FILL_DUMMY_KEY_noError) {
         if (nextkey) free(nextkey);
         return rcsub;
      }
   }
   else { /* need to get next */
      cmd = DMISA_NEXT;
      nextkey = pattern;
      keysize = *pkeysize;
   }

   rc = issueGetRow(
           &getmultout,
           gcpair->iCompId,
           gcpair->iGroupId,
           iAttribId,
           gcpair->iKeyCount,
           keysize,
           nextkey,  /* Key to obtain, not set on 1st access    */
           cmd);

   if ( (cmd == DMISA_FIRST) && (nextkey) )
      free(nextkey);
   if (rc == ISSUE_GET_ROW_noError) {
         rowdata = getmultout->rowData->list.list_val;

         /* loop thru keys to find how much room they really need - is double-loop right?? $MED */
         for (k = 1; k <= getmultout->rowData->list.list_len;
              k++,rowdata++) {

            /* Calculate remaining (i.e, variable) portion of key size*/
            gckey = (GCKey_t *)(gcpair + 1);  /* to point to key attribute information*/

            keyfromdmi = rowdata->values->list.list_val;

#if 0
            /* why can't i just use ogroupkeylist here (not gckey) $MED */
            for(j=1; j<= rowdata->values->list.list_len; j++,keyfromdmi++) {
#endif

            for (i=1; i<=gcpair->iKeyCount; i++,keyfromdmi++,gckey++) {
               while((gckey->iKeyAttrType != keyfromdmi->data.type) ||
                     (gckey->iKeyAttrId != keyfromdmi->id))
                             keyfromdmi++;
               if (!keyfromdmi) {
                  if (getmultout)
                     free_getmultout(getmultout);
                     return GET_KEY_VALUE_otherError;
               }
                         
               if (gckey->iKeyAttrType == MIF_DISPLAYSTRING ||
                   gckey->iKeyAttrType == MIF_OCTETSTRING) {
#if 0
               if (keyfromdmi->data.type == MIF_DISPLAYSTRING ||
                   keyfromdmi->data.type == MIF_OCTETSTRING) {
#endif
               if (keyfromdmi->data.DmiDataUnion_u.str)
                     keysize += 
       (keyfromdmi->data.DmiDataUnion_u.str->body.body_len +sizeof(ULONG));
               }
               else
                  keysize += dmiLength(keyfromdmi->data.type); /* yes $MED */
            }
            keyblock = (DMI_GroupKeyData_t *)calloc(1,keysize+1); /* Allocate space for key in DMI format*/
            *wantedkey = keyblock;
            *pkeysize = keysize;

            /* now i've got the space... just fill it in */

            gckey = (GCKey_t *)(gcpair + 1);  /* to point to key attribute information*/
            keyfromdmi = rowdata->values->list.list_val;

            keystart   = (char *)keyblock;
            keyvalue   = (char *)keyblock + keysize;  /* initialize: just beyond allocated space*/

            for (i=1; i<=gcpair->iKeyCount; i++,keyfromdmi++,gckey++,keyblock++) {
               while((gckey->iKeyAttrType != keyfromdmi->data.type) ||
                     (gckey->iKeyAttrId != keyfromdmi->id))
                             keyfromdmi++;
               if (!keyfromdmi) {
                  if (getmultout)
                     free_getmultout(getmultout);
                     return GET_KEY_VALUE_otherError;
               }

                  keyblock->iAttributeId = gckey->iKeyAttrId;
                  keyblock->iAttributeType = gckey->iKeyAttrType;
                  switch (keyfromdmi->data.type) { /* Pull key from DMI one attribute at a time*/
                  case MIF_INTEGER:
                     keyvalue -= sizeof(LONG);
                     keyblock->oKeyValue = keyvalue - keystart;
                     keyvalue_int  = (LONG *)keyvalue;
#if 0
                     *keyvalue_int = *(LONG *)((char *)
                             &(keyfromdmi->data.DmiDataUnion_u.integer));
#endif
                     memcpy((char *)keyvalue,
                            (char *)&(keyfromdmi->data.DmiDataUnion_u.integer),
                             sizeof(LONG));
                     break;

                  case MIF_INTEGER64:  /* signed             */
                  case MIF_COUNTER64:  /* unsigned           */
                     keyvalue -= sizeof(DmiInt64Str);
                     keyblock->oKeyValue = keyvalue - keystart;
                     keyvalue_64 = (DmiInt64Str *)keyvalue;
                     dmi64 = (DmiInt64Str *)((char *)&(keyfromdmi->data.DmiDataUnion_u.integer64));
                     keyvalue_64->high = dmi64->high; /* works for signed and unsigned*/
                     keyvalue_64->low  = dmi64->low;
                     break;
                  case MIF_COUNTER:
                  case MIF_GAUGE:
                     keyvalue -= sizeof(ULONG);
                     keyblock->oKeyValue = keyvalue - keystart;
                     keyvalue_ul  = (ULONG *)keyvalue;
#if 0
                     *keyvalue_ul = *(ULONG *)((char *)&(keyfromdmi->data.DmiDataUnion_u.counter));
#endif
                     memcpy((char *)keyvalue,
                            (char *)&(keyfromdmi->data.DmiDataUnion_u.counter),
                             sizeof(LONG));
                     break;
                  case MIF_OCTETSTRING:
                  case MIF_DISPLAYSTRING:
                    if ((!keyfromdmi->data.DmiDataUnion_u.str) ||
                        (!keyfromdmi->data.DmiDataUnion_u.str->body.body_len)){
                         if (getmultout)
                             free_getmultout(getmultout);
                         return GET_KEY_VALUE_otherError;
                    }
                     keyvalue -= keyfromdmi->data.DmiDataUnion_u.str->body.body_len
                                 + sizeof(ULONG);
                     keyblock->oKeyValue = keyvalue - keystart;

                     memcpy((char *)keyvalue,
                            (char *)&keyfromdmi->data.DmiDataUnion_u.str->body.body_len,
                            sizeof(ULONG));
                     memcpy((char *)(keyvalue+sizeof(ULONG)), 
                            keyfromdmi->data.DmiDataUnion_u.str->body.body_val,
                            keyfromdmi->data.DmiDataUnion_u.str->body.body_len);
                     /*keyvalue[sizeof(ULONG)+keyfromdmi->data.DmiDataUnion_u.str->body.body_len]=0; */
                     break;
                  case MIF_DATE:
                    if (!keyfromdmi->data.DmiDataUnion_u.date) {
                         if (getmultout)
                             free_getmultout(getmultout);
                         return GET_KEY_VALUE_otherError;
                    }
                     keyvalue -= DATE_LENGTH_dmi;
                     keyblock->oKeyValue = keyvalue - keystart;

                     memcpy(keyvalue,
                            ((char *)&(keyfromdmi->data.DmiDataUnion_u.date)),
                            DATE_LENGTH_dmi);
                     keyvalue[DATE_LENGTH_dmi]=0;
                     break;
                  default:
                    rc = PULL_KEY_VALUES_otherError;
                 /* break;  // Note that this line of code is commented out. To clean up compilation*/
                  } /* endswitch */
            } /* end key-assignment loop */
         } /* end loop thru cnf-buffers -- needed? $MED */
   } /* got row ok */
#if 0
   else
      if ((rcsub != ISSUE_GET_ROW_noError) && (cmd == DMISA_FIRST))
          rc = rcsub;
#endif

   if (getmultout) {
      free_getmultout(getmultout);
   }

   /* do i need to free anything? $MED */

   return rc;
}


/**********************************************************************/
/* Function: freeKeyValueList()                                       */
/*                                                                    */
/* Input parameters:                                                  */
/*    ptr. to KeyValueList linked list                                */
/*                                                                    */
/* Output parameters: none                                            */
/* Return codes: none                                                 */
/*                                                                    */
/* Description:  Free KeyValueList linked list.                       */
/**********************************************************************/
void freeKeyValueList(void)
{
   KeyValueList_t *temp;

   while (KeyValueList != NULL) {
      temp = KeyValueList->pNextValue;
      free(KeyValueList);            /* Free this entry in the list   */
      KeyValueList = temp;
   }

   DMISA_TRACE_LOG(LEVEL4, "Freed KeyValueList.");
}


/**********************************************************************/
/* Function: pullKeyValues()                                          */
/*                                                                    */
/* Input parameters:                                                  */
/*    ptr. to current group/component structure in XlateList          */
/*                                                                    */
/* Output parameters:                                                 */
/*    ptr. to KeyValueList, a linked list (global variable!)          */
/*                                                                    */
/* Description:                                                       */
/*   1. If a valid KeyValueList exists, return ptr. to KeyValueList.  */
/*   2. Otherwise, create KeyValueList.                               */
/*    a) Issue GetFirstRow to the table.  Preserve the key value      */
/*       and lengths of key attributes.  (iKeyCount and iAttributeType*/
/*       are needed but are already available in XlateList.)          */
/*    b) Issue GetNextRow to all subsequent rows in the table.        */
/*       (Last row has been received once iStatus                     */
/*        = 0x010a, Row not found, received.)                         */
/*       Preserve each key value and lengths of key attributes.       */
/*    c) Return the key values as a linked list.                      */
/*                                                                    */
/*     pnextkeyvalue  -->  ...  --> pnextkeyvalue  --||               */
/*     KeyValue                     KeyValue                          */
/*     KeyValue                     KeyValue                          */
/*         .                            .                             */
/*         .                            .                             */
/*         .                            .                             */
/*     KeyValue                     KeyValue                          */
/*                                                                    */
/*                                                                    */
/* Notes:                                                             */
/*   1. KeyValues that are strings consist of a 4-byte (ULONG) length */
/*      followed by the value.                                        */
/*   2. KeyValueList is allocated but not freed within the same       */
/*      invocation of this function.  It is freed either by main()    */
/*      or by a subsequent invocation to this function.               */
/*   3. To assist performance, KeyValueList is retained for reuse     */
/*      in subsequent GetNext operations.                             */
/*      KeyValueList is freed as soon as:                             */
/*        - a command other than GetNext is received, or              */
/*        - a GetNext is received which involves a different          */
/*          group/component ID pair.                                  */
/*        - the age of the list is longer than a timeout constant     */
/*   4. Warning: Long function ahead.  Key translation has been       */
/*      integrated into this function to assist performance for       */
/*      accessing tables with many rows and wide keys.                */
/*                                                                    */
/* Return codes: PULL_KEY_VALUES_noError (list pulled)                */
/*               PULL_KEY_VALUES_noErrorOldListStillGood              */
/*                                       (list not pulled)            */
/*               PULL_KEY_VALUES_dmiBroke                             */
/*               PULL_KEY_VALUES_cnfBufAddressability                 */
/*               PULL_KEY_VALUES_outOfMemory                          */
/*               PULL_KEY_VALUES_cannotGetToThem                      */
/*               PULL_KEY_VALUES_otherError                           */
/*               ISSUE_GET_ROW_outOfMemory                            */
/*               PREPARE_FOR_DMI_INVOKE_resetSemFailed                */
/**********************************************************************/
static int pullKeyValues(GCPair_t *gcpair)
{
   DmiGetMultipleOUT  *getmultout;
   DmiRowData_t       *rowdata;
   DmiAttributeData_t *keyfromdmi;

   DMI_GroupKeyData_t *nextkey;
   DmiString_t        *keystring;
   GCKey_t            *gckey, *tempkey;
   KeyValueList_t     *thiskey = NULL, *previous = NULL;  /* Initialize to squelch compiler warning*/
   char               *keyvalue;
   LONG               *keyvalue_int;
   ULONG              *keyvalue_ul;
   DmiInt64Str        *keyvalue_64, *dmi64;
   int                 i, rc = PULL_KEY_VALUES_noError, rcsub,rcsub1, cmd = DMISA_FIRST;
   ULONG               dmistat;
   ULONG               keysize = 0UL, fixedsize = 0UL;  /* Must be initialized for GetFirstRow*/
   static int          lastgroup, lastcomponent;
   static time_t       keyvaluebuildtime;
   time_t              currenttime;

   if (!gcpair || gcpair->iKeyCount == 0) return PULL_KEY_VALUES_otherError; /* to be safe*/

   time(&currenttime);  /* calculate time since KeyValueList last built*/
   if (&currenttime != NULL && currenttime != -1L) {  /* then we retrieved the current time*/
      if (currenttime - keyvaluebuildtime < KEY_LIST_DECAY_TIME &&  /* If KeyValueList was built recently*/
          KeyValueList &&                       /*  and KeyValueList is still valid*/
          gcpair->iGroupId == lastgroup &&      /*  and the group ID is the same as the last one*/
          gcpair->iCompId == lastcomponent ) {  /*  and the component ID is the same as the last one*/
          DMISA_TRACE_LOG(LEVEL4,"Use old KeyValueList for this GetNext operation.");
          return PULL_KEY_VALUES_noErrorOldListStillGood;
      }
   }

   lastgroup     = gcpair->iGroupId;  /* save for next GetNext request*/
   lastcomponent = gcpair->iCompId;   /* save for next GetNext request*/
   if (KeyValueList) freeKeyValueList(); /* free old KeyValueList     */

   rcsub = fillDummyKey(
                gcpair,
                &keysize,
                &nextkey);  /* Fill key with zeroes for GetFirstRow,  */
                            /* since S.L. is counting on seeing a key */
   if (rcsub != FILL_DUMMY_KEY_noError) {
      if (nextkey) free(nextkey);
      return rcsub;
   }

   /* Calculate non-variable portion of key length                    */
   tempkey = gckey = (GCKey_t *)(gcpair + 1);  /* to point to key attribute information*/
   for (i=1; i<=gcpair->iKeyCount; i++,tempkey++) {
      fixedsize += dmiLength(tempkey->iKeyAttrType);
   }

   do {  /* Beyond this point, don't return until req. & cnf. buffers are freed!*/
      rcsub = issueGetRow(
                 &getmultout,
                 gcpair->iCompId,
                 gcpair->iGroupId,
                 0UL,      /* Attribute - get first one (or any one for that matter)*/
                 gcpair->iKeyCount,
                 keysize,
                 nextkey,  /* Key to obtain, not set on 1st access    */
                 cmd);
      if (nextkey) free(nextkey);  /* free buffer allocated for getting next key*/
      if (rcsub != ISSUE_GET_ROW_noError) {
           if (rcsub == ISSUE_GET_ROW_outOfMemory)
              rc = PULL_KEY_VALUES_dmiBroke;
      } else {
         if (rcsub == ISSUE_GET_ROW_noError) {
             
            rowdata = getmultout->rowData->list.list_val;

            for (i = 1; i <= getmultout->rowData->list.list_len;
                 i++,rowdata++) {  /* Pull out each key value in buffer*/

               if (rowdata->values->list.list_len < gcpair->iKeyCount) {  /* Just playing safe*/
                  rc = PULL_KEY_VALUES_otherError;
               } else {

                  /* Calculate remaining (i.e, variable) portion of key size*/
                  keysize = fixedsize + sizeof(KeyValueList_t);  /* add room for ptr. to next keyvalue*/
                  tempkey = gckey;
                  keyfromdmi = rowdata->values->list.list_val;

                for (i=1; i<=gcpair->iKeyCount; i++,keyfromdmi++,tempkey++) {
                   while((tempkey->iKeyAttrType != keyfromdmi->data.type) ||
                         (tempkey->iKeyAttrId != keyfromdmi->id))
                              keyfromdmi++;
                   if (!keyfromdmi) {
                       if (getmultout)
                           free_getmultout(getmultout);
                       return PULL_KEY_VALUES_otherError;
                   }

                        if (tempkey->iKeyAttrType == MIF_DISPLAYSTRING ||
                            tempkey->iKeyAttrType == MIF_OCTETSTRING) { 

#if 0
                for (i=1;i<=rowdata->keyList->list.list_len; 
                                       i++,keyfromdmi++,tempkey++) {
                     if (keyfromdmi->data.type == MIF_DISPLAYSTRING ||
                         keyfromdmi->data.type == MIF_OCTETSTRING) {

#endif
                          keysize += (keyfromdmi->data.DmiDataUnion_u.str->body.body_len + sizeof(ULONG));
                        }
                     }
                  }
                  thiskey = (KeyValueList_t *)malloc(keysize);
                        /* Allocate for this key in internal (KeyValueList) format*/
                  if (!thiskey) {
                     rc = PULL_KEY_VALUES_outOfMemory;
                  } else {
                     thiskey->pNextValue = DMISA_NULL;     /* In case this ends up being the last key*/
                     if (cmd == DMISA_FIRST) {
                        KeyValueList = thiskey;           /* Start of KeyValueList*/
                     } else {
                        previous->pNextValue = thiskey;    /* Continuation of KeyValueList*/
                     }

                     keyvalue = (char *)(thiskey + 1);  /* initialize: get me past pNextValue ptr.*/
                     tempkey = gckey;
                     keyfromdmi = rowdata->values->list.list_val;

                     for (i=1; i<=gcpair->iKeyCount; i++,keyfromdmi++,tempkey++) {
                       while((tempkey->iKeyAttrType != keyfromdmi->data.type) ||
                         (tempkey->iKeyAttrId != keyfromdmi->id))
                              keyfromdmi++;
                      if (!keyfromdmi) {
                       if (getmultout)
                           free_getmultout(getmultout);
                       return PULL_KEY_VALUES_otherError;
                      }


                           switch (keyfromdmi->data.type) { /* Pull key from DMI one attribute at a time*/
                           case MIF_INTEGER:
                              keyvalue_int  = (LONG *)keyvalue;
                              *keyvalue_int = *(LONG *)((char *)
                                  &(keyfromdmi->data.DmiDataUnion_u.integer));
                              keyvalue += sizeof(LONG);
                              break;
                           case MIF_INTEGER64:  /* signed             */
                           case MIF_COUNTER64:  /* unsigned           */
                              keyvalue_64 = (DmiInt64Str *)keyvalue;
                              dmi64 = (DmiInt64Str *)((char *)
                               &(keyfromdmi->data.DmiDataUnion_u.integer64));
                              keyvalue_64->high = dmi64->high; /* works for signed and unsigned*/
                              keyvalue_64->low  = dmi64->low;
                              keyvalue += sizeof(DmiInt64Str);
                              break;
                           case MIF_COUNTER:
                           case MIF_GAUGE:
                              keyvalue_ul  = (ULONG *)keyvalue;
                              *keyvalue_ul = *(ULONG *)((char *)
                               &(keyfromdmi->data.DmiDataUnion_u.counter));
                              keyvalue += sizeof(ULONG);
                              break;
                           case MIF_OCTETSTRING:
                           case MIF_DISPLAYSTRING:
                              keystring = keyfromdmi->data.DmiDataUnion_u.str; 
                              if ((!keystring) ||
                                  (!keystring->body.body_val)) {
                                  if (getmultout)
                                     free_getmultout(getmultout);
                                  return PULL_KEY_VALUES_otherError;
                              }
                              memcpy(keyvalue, keystring->body.body_val,
                                   keystring->body.body_len+ sizeof(ULONG));
                          keyvalue += keystring->body.body_len+ sizeof(ULONG);
                              break;
                           case MIF_DATE:
                              if (!keyfromdmi->data.DmiDataUnion_u.date) {
                                  if (getmultout)
                                     free_getmultout(getmultout);
                                  return PULL_KEY_VALUES_otherError;
                              }
                              memcpy(keyvalue,
                            ((char *)&(keyfromdmi->data.DmiDataUnion_u.date)),
                                     DATE_LENGTH_dmi);
                              keyvalue += DATE_LENGTH_dmi;
                              break;
                           default:
                             rc = PULL_KEY_VALUES_otherError;
                          /* break;  // Note that this line of code is commented out. To clean up compilation*/
                           } /* endswitch */
                     }
                  }
               previous = thiskey;
            } /* endfor each attribute within the key*/

            /* Set up GroupKeyList of the last seen key, to be used for the next GetRow*/
            if (rc == PULL_KEY_VALUES_noError) {  /* If abortive error condition has not occurred...*/
               keysize += sizeof(DMI_GroupKeyData_t) * gcpair->iKeyCount -
                          sizeof(KeyValueList);  /* transform from internal format to DMI format*/
               nextkey = (DMI_GroupKeyData_t *)malloc(keysize);
                                   /* Allocate for next key in DMI format*/
               if (!nextkey) {
                  rc = PULL_KEY_VALUES_outOfMemory;
               } else {
                  rcsub1 = translateInternalKeyToDmi(
                         gcpair,
                         (char *)(thiskey + 1), /* values of current key read from DMI*/
                         keysize,               /* length of current key*/
                         nextkey); /* destination of key in GroupKeyData type form*/
               }
            }
         } /*endif iStatus was good */
      }
      cmd = DMISA_NEXT;  /* get next row from this group (i.e., table)*/
      if (getmultout) {
           free_getmultout(getmultout);
           getmultout=NULL;
      }
   } while (rcsub == ISSUE_GET_ROW_noError);  /* while no abortive error condition has occurred*/

   if (rc != PULL_KEY_VALUES_noError) {  /* If no abortive error has occurred*/
         return PULL_KEY_VALUES_dmiBroke;
   }
   if (!KeyValueList) {
         return PULL_KEY_VALUES_cannotGetToThem;
   }
      time(&keyvaluebuildtime);  /* determine time that KeyValueList last built*/

   return rc;
}


/**********************************************************************/
/* Function: translateDmiKeyToInternal()                              */
/*                                                                    */
/* Inputs:                                                            */
/*                                                                    */
/* Outputs:                                                           */
/*                                                                    */
/* Note:  *internalkey points to storage allocated but not freed      */
/*        by this function.  It must be freed by the caller.          */
/*                                                                    */
/* Reason codes: TRANSLATE_DMI_KEY_TO_INTERNAL_noError                */
/*               TRANSLATE_DMI_KEY_TO_INTERNAL_otherError             */
/*               GET_DMI_KEY_SIZE_badAttributeType                    */
/**********************************************************************/
static int translateDmiKeyToInternal(KeyValueList_t **internalkey,
                          DMI_GroupKeyData_t *pattern, GCPair_t *gcpair)
{
   DMI_GroupKeyData_t *patternbase;
   DMI_STRING         *keystring;
   char               *keyvalue;
   LONG               *keyvalue_int;
   ULONG              *keyvalue_ul;
   DmiInt64Str        *keyvalue_64, *dmi64;
   int                 rc, i, keyvaluesize;

   *internalkey = NULL; /* Initialize                                 */

   rc = getDmiKeySize(
        pattern,
        &keyvaluesize,
        gcpair);
   if (rc != GET_DMI_KEY_SIZE_noError) return rc;

   *internalkey = (KeyValueList_t *)malloc(keyvaluesize +
                    sizeof(KeyValueList_t));  /* for key in internal (KeyValueList) format*/
   if (!(*internalkey)) return TRANSLATE_DMI_KEY_TO_INTERNAL_outOfMemory;

   (*internalkey)->pNextValue = DMISA_NULL;     /* Just for safety, expand for general use*/

   keyvalue = (char *)(*internalkey + 1);  /* initialize: get me past pNextValue ptr.*/
   patternbase = pattern;
   for (i=1; i<=gcpair->iKeyCount; i++,pattern++) {
      switch (pattern->iAttributeType) { /* Pull key from DMI one attribute at a time*/
      case MIF_INTEGER:
         keyvalue_int  = (LONG *)keyvalue;
         *keyvalue_int = *(LONG *)((char *)patternbase +
                                                 pattern->oKeyValue);
         keyvalue += sizeof(LONG);
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         keyvalue_64 = (DmiInt64Str *)keyvalue;
         dmi64 = (DmiInt64Str *)((char *)patternbase +
                                              pattern->oKeyValue);
         keyvalue_64->high = dmi64->high; /* works for signed and unsigned*/
         keyvalue_64->low  = dmi64->low;
         keyvalue += sizeof(DmiInt64Str);
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         keyvalue_ul  = (ULONG *)keyvalue;
         *keyvalue_ul = *(ULONG *)((char *)patternbase +
                                    pattern->oKeyValue);
         keyvalue += sizeof(ULONG);
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         keystring = (DMI_STRING *)((char *)patternbase + pattern->oKeyValue);
         memcpy(keyvalue, &(keystring->length),keystring->length + sizeof(ULONG));
         keyvalue += keystring->length + sizeof(ULONG);
         break;
      case MIF_DATE:
         memcpy(keyvalue,
                ((char *)patternbase + pattern->oKeyValue),
                DATE_LENGTH_dmi);
         keyvalue += DATE_LENGTH_dmi;
         break;
      default:
        return TRANSLATE_DMI_KEY_TO_INTERNAL_otherError;
     /* break;  // Note that this line of code is commented out. To clean up compilation*/
      } /* endswitch */
    }

    return TRANSLATE_DMI_KEY_TO_INTERNAL_noError;
}


/**********************************************************************/
/* Function: getDmiKeySize() (JUST THE KEY VALUE, NOT KEY DATA BLOCKS)*/
/*                                                                    */
/*                                                                    */
/*                                                                    */
/* Reason codes: GET_DMI_KEY_SIZE_noError                             */
/*               GET_DMI_KEY_SIZE_badAttributeType                    */
/*                                                                    */
/* Assumption: All offsets calculated from dmikey ptr.                */
/**********************************************************************/
static int getDmiKeySize(DMI_GroupKeyData_t *dmikey, int *keyvaluesize,
                         GCPair_t *gcpair)
{
   DMI_GroupKeyData_t *dmikeybase;
   GCKey_t *gckey;
   int      i;

   gckey = (GCKey_t *)(gcpair + 1);
   dmikeybase = dmikey;
   for (i=1,*keyvaluesize=0; i<=gcpair->iKeyCount; i++,gckey++,dmikey++) {
      switch (gckey->iKeyAttrType) {
      case MIF_INTEGER:
         *keyvaluesize += sizeof(LONG);
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         *keyvaluesize += sizeof(DmiInt64Str);
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         *keyvaluesize += sizeof(ULONG);
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         *keyvaluesize += ((DMI_STRING *)((char *)dmikeybase +
                           dmikey->oKeyValue))->length + sizeof(ULONG);
         break;
      case MIF_DATE:
         *keyvaluesize += DATE_LENGTH_dmi;
         break;
      default:
         return GET_DMI_KEY_SIZE_badAttributeType;
      /* break;  // Note that this line of code is commented out. To clean up compilation*/
      }
   }

   return GET_DMI_KEY_SIZE_noError;
}


/**********************************************************************/
/* Function: compareKeyValues()                                       */
/*                                                                    */
/* Input parameters:                                                  */
/*    ptr. to current group/component structure in XlateList          */
/*    ptr. to KeyValue structure 1 in KeyValueList                    */
/*    ptr. to KeyValue structure 2 in KeyValueList                    */
/*                                                                    */
/* Output parameters: none                                            */
/* Return codes: COMPARE_KEY_VALUES_equal - key 1 is equal to key 2   */
/*               COMPARE_KEY_VALUES_lower - key 1 is lower than key 2 */
/*               COMPARE_KEY_VALUES_higher- key 1 is higher than key 2*/
/*               COMPARE_KEY_VALUES_error - compare error             */
/*                                                                    */
/* Description:                                                       */
/*   Compare the pseudo dotted-decimal key, from the first byte       */
/*   towards the last.  As soon as one is detected as having a higher */
/*   value than the other, return with the appropriate return code.   */
/*   Compare 4 bytes at a time until a variable-length string is      */
/*   encountered, from which time only single bytes is compared.      */
/*                                                                    */
/*   iAttributeType  Action for pseudo dotted-decimal compare         */
/*   --------------  ----------------------------------------         */
/*     integer     - compare 4 bytes from each structure              */
/*     integer64   - compare 4-byte iKeyValueLen, then compare        */
/*                   n bytes, a byte at a time,                       */
/*                   where n is the value of iKeyValueLen (n = 8)     */
/*     gauge       - compare 4 bytes from each structure              */
/*     counter     - compare 4 bytes from each structure              */
/*     counter64   - compare 8 bytes from each structure              */
/*     displaystring - compare 4-byte iKeyValueLen, then compare      */
/*                   n bytes, a byte at a time,                       */
/*                   where n is the value of iKeyValueLen.            */
/*     octetstring - compare 4-byte iKeyValueLen, then compare        */
/*                   n bytes, a byte at a time,                       */
/*                   where n is the value of iKeyValueLen.            */
/*     date        - compare 4-byte iKeyValueLen, then compare        */
/*                   n bytes, a byte at a time,                       */
/*                   where n is the value of iKeyValueLen (n = 28)    */
/*                                                                    */
/*   Example:                                                         */
/*    iAttributeType: integer, integer, octetstring,         integer  */
/*    KeyValue A:     00000101 00000031 00000003 4D 61 79    00004000 */
/*           OID:          257   .   49    .   3.M .a .y    .    4000 */
/*                           =        =       !=                      */
/*    KeyValue B:     00000101 00000031 00000004 4A 75 6E 65 00004000 */
/*           OID:          257   .   49    .   4.J .u .n .e .    4000 */
/*                                                                    */
/*   Assumption: The nth attribute of each key is of the same type.   */
/**********************************************************************/
static int compareKeyValues(GCPair_t *l_grpcomp_pair,
                            void *key_p1, void *key_p2)
{

   int         i, comp_rc;
   GCKey_t    *keyattrtype;   /* pointer to current attribute type */
   void       *keyvalue_p1;
   void       *keyvalue_p2;
   ULONG      *keyint_p1;     /* 1st pointer to int in key struct */
   ULONG      *keyint_p2;     /* 2nd pointer to int in key struct */
   DmiInt64Str *keyi64_p1;     /* 1st pointer to int64 in key struct */
   DmiInt64Str *keyi64_p2;     /* 2nd pointer to int64 in key struct */
   char       *keydate_p1;    /* 1st pointer to date in key struct */
   char       *keydate_p2;    /* 2nd pointer to date in key struct */
   DMI_STRING *keydmis_p1;    /* 1st pointer to dmi str in key struct */
   DMI_STRING *keydmis_p2;    /* 2nd pointer to dmi str in key struct */
/* char       *tempattrptr;   ** temp pointer to attribute type */

   keyvalue_p1 = key_p1;
   keyvalue_p2 = key_p2;
   keyattrtype = (GCKey_t *)(l_grpcomp_pair + 1);
                      /* get ptr to first key pair in current grp-cmp list */

   for (i=0;i<l_grpcomp_pair->iKeyCount ;i++,keyattrtype++ ) /* loop through all key pairs*/
   {
                /* determine the case of the attr type and compare both keys*/
      switch (keyattrtype->iKeyAttrType)
      {

         case MIF_COUNTER:
         case MIF_GAUGE:
         case MIF_INTEGER:
            {   /* treat all as unsigned int 32...dotted dec doesn't recogn signs*/
             keyint_p1 = (ULONG *)keyvalue_p1;
             keyint_p2 = (ULONG *)keyvalue_p2;
             if (*keyint_p1 == *keyint_p2)
                {
                 keyvalue_p1 = (char *)keyvalue_p1 + 4;   /* incr by sz of int */
                 keyvalue_p2 = (char *)keyvalue_p2 + 4;
                }
             else if (*keyint_p1 < *keyint_p2)
                {
                 return COMPARE_KEY_VALUES_lower;
                }
             else
                {
                 return COMPARE_KEY_VALUES_higher;
                }
             break;
            }
         case MIF_COUNTER64:
         case MIF_INTEGER64:
            {  /* treat all as unsigned int 64...dotted dec doesn't recogn signs*/
             keyi64_p1 = (DmiInt64Str *)keyvalue_p1;
             keyi64_p2 = (DmiInt64Str *)keyvalue_p2;
             if (keyi64_p1->high == keyi64_p2->high)
                {
                 if (keyi64_p1->low == keyi64_p2->low)
                    {
                     keyvalue_p1 = (char *)keyvalue_p1 + sizeof(DmiInt64Str);
                        /* incr by sz of int64*/
                     keyvalue_p2 = (char *)keyvalue_p2 + sizeof(DmiInt64Str);
                    }
                 else if (keyi64_p1->low < keyi64_p2->low)
                    {
                     return COMPARE_KEY_VALUES_lower;
                    }
                 else
                    {
                     return COMPARE_KEY_VALUES_higher;
                    }
                 }
             else if (keyi64_p1->high < keyi64_p2->high)
                 {
                  return COMPARE_KEY_VALUES_lower;
                 }
             else
                 {
                  return COMPARE_KEY_VALUES_higher;
                 }
             break;
            }
         case MIF_OCTETSTRING:
         case MIF_DISPLAYSTRING:
            {
             keydmis_p1 = (DMI_STRING *)keyvalue_p1;
             keydmis_p2 = (DMI_STRING *)keyvalue_p2;
             if (keydmis_p1->length == keydmis_p2->length)
                {
                 comp_rc = memcmp(&keydmis_p1->body, &keydmis_p2->body,
                                  keydmis_p1->length);
                 if (comp_rc == 0)
                    {
                     keyvalue_p1 = (char *)keyvalue_p1 + sizeof(ULONG) +
                                   (keydmis_p1->length);
                         /* incr by sz of DMI_STRING*/
                     keyvalue_p2 = (char *)keyvalue_p2 + sizeof(ULONG) +
                                   (keydmis_p1->length);
                    }
                 else if (comp_rc < 0)
                    {
                     return COMPARE_KEY_VALUES_lower;
                    }
                 else
                    {
                     return COMPARE_KEY_VALUES_higher;
                    }
                }
             else if (keydmis_p1->length < keydmis_p2->length)
                {
                 return COMPARE_KEY_VALUES_lower;
                }
             else
                {
                 return COMPARE_KEY_VALUES_higher;
                }
             break;
            }
         case MIF_DATE:
            {
             keydate_p1 = (char *)keyvalue_p1;
             keydate_p2 = (char *)keyvalue_p2;
             comp_rc = memcmp(keydate_p1, keydate_p2, DATE_LENGTH_snmp);
             if (comp_rc == 0)
                {
                 keyvalue_p1 = (char *)keyvalue_p1 + DATE_LENGTH_dmi;
                         /* incr by sz of DATE_LENGTH*/
                 keyvalue_p2 = (char *)keyvalue_p2 + DATE_LENGTH_dmi;
                }
             else if (comp_rc < 0)
                {
                 return COMPARE_KEY_VALUES_lower;
                }
             else
                {
                 return COMPARE_KEY_VALUES_higher;
                }
             break;
            }
         default:
            {
             return COMPARE_KEY_VALUES_error;     /* compare error */
            }
      } /* endswitch */

/*    tempattrptr = (char *)keyattrtype + sizeof(GCKey_t);            */
/*    keyattrtype = (GCKey_t *)tempattrptr;                           */

   } /* endfor loop through all key pairs */

   return COMPARE_KEY_VALUES_equal;
} /* end compareKeyValues */


/**********************************************************************/
/* Function: getInternalKeySize()                                     */
/*                                                                    */
/*                                                                    */
/*                                                                    */
/* Return codes: GET_INTERNAL_KEY_SIZE_noError                        */
/*               GET_INTERNAL_KEY_SIZE_otherError                     */
/**********************************************************************/
static int getInternalKeySize(KeyValueList_t *internalkey,
                              ULONG *keyvaluesize, GCPair_t *gcpair)
{
   char     *internal;
   GCKey_t  *gckey;
   int       i;

   /* fixed bug changed internalkey to internal throughout routine $MED */
   internal = (char *)(internalkey + 1);
   gckey = (GCKey_t *)(gcpair + 1);  /* get past initial stuff to key values*/
   for (i=1,*keyvaluesize = 0; i<=gcpair->iKeyCount; i++, gckey++) {
      switch (gckey->iKeyAttrType) {
      case MIF_INTEGER:
         *keyvaluesize += sizeof(LONG);
         internal   += sizeof(LONG);
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         *keyvaluesize += sizeof(DmiInt64Str);
         internal   += sizeof(DmiInt64Str);
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         *keyvaluesize += sizeof(ULONG);
         internal   += sizeof(ULONG);
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         *keyvaluesize += ((DMI_STRING *)internal)->length + sizeof(ULONG);
         internal   += ((DMI_STRING *)internal)->length + sizeof(ULONG);
         break;
      case MIF_DATE:
         *keyvaluesize += DATE_LENGTH_dmi;
         internal   += DATE_LENGTH_dmi;
         break;
      default:
        return GET_INTERNAL_KEY_SIZE_otherError;
     /* break;  // Note that this line of code is commented out. To clean up compilation*/
      }
   }
   return GET_INTERNAL_KEY_SIZE_noError;
}


/**********************************************************************/
/* Function: translateInternalKeyToDmi()                              */
/*                                                                    */
/* Input parameters:                                                  */
/*    ptr. to current group/component structure in XlateList          */
/*    ptr. to attributes values of the key                            */
/*    size of key attribute values                                    */
/*    ptr. to where the DMI key block should begin                    */
/*                                                                    */
/* Output parameters: none                                            */
/*    but DMI key block is generated by the function                  */
/*                                                                    */
/* Return codes: TRANSLATE_INTERNAL_KEY_TO_DMI_noError                */
/*                                                                    */
/* Description:                                                       */
/**********************************************************************/
static int translateInternalKeyToDmi(GCPair_t *gcpair, char *keyinput,
                            ULONG keysize, DMI_GroupKeyData_t *keyblock)
{
   char         *keyvalue, *keystart;    /* of target key             */
   LONG         *keyvalue_int;
   ULONG        *keyvalue_ul;
   DmiInt64Str  *keyvalue_64, *dmi64;
   DMI_STRING   *keystring;
   GCKey_t      *gckey;
   int           i;

   keystart   = (char *)keyblock;
   gckey      = (GCKey_t *)(gcpair + 1);
   keyvalue   = (char *)keyblock + keysize;  /* initialize: just beyond allocated space*/
   for (i=1; i<=gcpair->iKeyCount; ++i,++gckey, ++keyblock) { /* Prepare each attribute in key*/
      keyblock->iAttributeId   = gckey->iKeyAttrId;
      keyblock->iAttributeType = gckey->iKeyAttrType;
      switch (keyblock->iAttributeType) {  /* pull key from DMI one attribute at a time*/
      case MIF_INTEGER:
         keyvalue -= sizeof(LONG);
         keyblock->oKeyValue = keyvalue - keystart;
         keyvalue_int  = (LONG *)keyvalue;
         *keyvalue_int = *(LONG *)keyinput;
         keyinput += sizeof(LONG);
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         keyvalue -= sizeof(DmiInt64Str);
         keyblock->oKeyValue = keyvalue - keystart;
         keyvalue_64 = (DmiInt64Str *)keyvalue;
         dmi64 = (DmiInt64Str *)keyinput;
         keyvalue_64->high = dmi64->high; /* works for signed and unsigned*/
         keyvalue_64->low  = dmi64->low;
         keyinput += sizeof(DmiInt64Str);
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         keyvalue -= sizeof(ULONG);
         keyblock->oKeyValue = keyvalue - keystart;
         keyvalue_ul  = (ULONG *)keyvalue;
         *keyvalue_ul = *(ULONG *)keyinput;
         keyinput += sizeof(ULONG);
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         keystring = (DMI_STRING *)keyinput;
         keyvalue -= keystring->length + sizeof(ULONG);
         keyblock->oKeyValue = keyvalue - keystart;
         memcpy(keyvalue, &(keystring->length), keystring->length + sizeof(ULONG));
         keyinput += keystring->length + sizeof(ULONG);
         break;
      case MIF_DATE:
         keyvalue -= DATE_LENGTH_dmi;
         keyblock->oKeyValue = keyvalue - keystart;
         memcpy(keyvalue, keyinput, DATE_LENGTH_dmi);
         keyinput += DATE_LENGTH_dmi;
         break;
      default:
        return TRANSLATE_INTERNAL_KEY_TO_DMI_otherError;
     /* break;  // Note that this line of code is commented out. To clean up compilation*/
      } /* endswitch */
   } /* endfor */

   return TRANSLATE_INTERNAL_KEY_TO_DMI_noError;
}


/**********************************************************************/
/* Function: fillDummyKey()                                           */
/*                                                                    */
/* Input parameters:                                                  */
/*    ptr. to current group/component structure in XlateList          */
/*                                                                    */
/* Output parameters:                                                 */
/*    ptr. to where DMI key block begins                              */
/*    size of key dummykey block, including DmiGroupKeyData blocks    */
/*                                                                    */
/* Note:  *keyblock points to storage allocated but not freed         */
/*        by this function.  It must be freed by the caller.          */
/*                                                                    */
/* Return codes: FILL_DUMMY_KEY_noError                               */
/*               FILL_DUMMY_KEY_otherError                            */
/*               FILL_DUMMY_KEY_outOfMemory                           */
/*                                                                    */
/* Description:                                                       */
/**********************************************************************/
static int fillDummyKey(GCPair_t *gcpair, ULONG *keyblocksize,
                        DMI_GroupKeyData_t **keyblock)
{
   #define ZERO_32 0L;
   #define ZERO_U32 0UL;
   DMI_GroupKeyData_t *tblock;
   char         *keyvalue, *keystart;    /* of target key             */
   LONG         *keyvalue_int;
   ULONG        *keyvalue_ul;
   DmiInt64Str  *keyvalue_64;
   GCKey_t      *gckey;
   int           i;

   *keyblock = NULL;  /* Initialize                                   */

   /* First calculate length we need to allocate                      */
   gckey = (GCKey_t *)(gcpair + 1);
   *keyblocksize = gcpair->iKeyCount * sizeof(DMI_GroupKeyData_t);
   for (i=1; i<=gcpair->iKeyCount; i++,gckey++) {  /* Prepare each attribute in key*/
      switch (gckey->iKeyAttrType) {  /* pull key from DMI one attribute at a time*/
      case MIF_INTEGER:
         *keyblocksize += sizeof(LONG);
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         *keyblocksize += sizeof(DmiInt64Str);
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         *keyblocksize += sizeof(ULONG);
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         *keyblocksize += sizeof(ULONG);
         break;
      case MIF_DATE:
         *keyblocksize += DATE_LENGTH_dmi;
         break;
      default:
        return FILL_DUMMY_KEY_otherError;
     /* break;  // Note that this line of code is commented out.  To clean up compilation.*/
      } /* endswitch */
   }

   /* Allocate key                                                    */
   tblock    =
   *keyblock = (DMI_GroupKeyData_t *)malloc(*keyblocksize);  /* for key in DMI format*/
   if (!(*keyblock)) return FILL_DUMMY_KEY_outOfMemory;

   /* Fill key                                                        */
   keystart   = (char *)tblock;
   gckey      = (GCKey_t *)(gcpair + 1);
   keyvalue   = (char *)tblock + *keyblocksize;  /* initialize: just beyond allocated space*/
   for (i=1; i<=gcpair->iKeyCount; ++i,++gckey, ++tblock) {  /* Prepare each attribute in key*/
      tblock->iAttributeId   = gckey->iKeyAttrId;
      tblock->iAttributeType = gckey->iKeyAttrType;
      switch (tblock->iAttributeType) {  /* pull key from DMI one attribute at a time*/
      case MIF_INTEGER:
         keyvalue -= sizeof(LONG);
         tblock->oKeyValue = keyvalue - keystart;
         keyvalue_int  = (LONG *)keyvalue;
         *keyvalue_int = ZERO_32;
         break;
      case MIF_INTEGER64:  /* signed                                  */
      case MIF_COUNTER64:  /* unsigned                                */
         keyvalue -= sizeof(DmiInt64Str);
         tblock->oKeyValue = keyvalue - keystart;
         keyvalue_64 = (DmiInt64Str *)keyvalue;
         keyvalue_64->high = ZERO_32; /* used for signed (integer) and unsigned (counter)*/
         keyvalue_64->low  = ZERO_U32;
         break;
      case MIF_COUNTER:
      case MIF_GAUGE:
         keyvalue -= sizeof(ULONG);
         tblock->oKeyValue = keyvalue - keystart;
         keyvalue_ul  = (ULONG *)keyvalue;
         *keyvalue_ul = ZERO_U32;
         break;
      case MIF_OCTETSTRING:
      case MIF_DISPLAYSTRING:
         keyvalue -= sizeof(ULONG);
         tblock->oKeyValue = keyvalue - keystart;
         keyvalue_int  = (LONG *)keyvalue;
         *keyvalue_int = ZERO_U32;
         break;
      case MIF_DATE:
         keyvalue -= DATE_LENGTH_dmi;
         tblock->oKeyValue = keyvalue - keystart;
         memset(keyvalue, '\0', DATE_LENGTH_dmi);
         break;
      default:
        return FILL_DUMMY_KEY_otherError;
     /* break;  // Note that this line of code is commented out. To clean up compilation*/
      } /* endswitch */
   } /* endfor */

   return FILL_DUMMY_KEY_noError;
}

