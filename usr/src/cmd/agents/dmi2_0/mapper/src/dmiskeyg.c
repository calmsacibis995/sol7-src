/* Copyright 10/11/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmiskeyg.c	1.5 96/10/11 Sun Microsystems"

/* Module Description *************************************************/
/*                                                                    */
/*  Name:  dmiskeyg.c                                                 */
/*                                                                    */
/*  Description:                                                      */
/*  This module contains functions which support GETs and SETs of     */
/*  attributes within tables (i.e., that are associated with a key).  */
/*                                                                    */
/*  Notes: This file contains the following functions:                */
/*                                        keyFromDpi(...)             */
/*                                        keyToDpi(...)               */
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
/*                  940727    PAXHIA    New module                    */
/*                  940818    LAUBLI    Changed atoi to strtoul,      */
/*                                      initialize correctly          */
/*                  941025    LAUBLI    Corrected translation of DATE,*/
/*                                      DISPLAYSTRING, & OCTETSTRING  */
/*                  950330    LAUBLI    Add support for INTEGER64 keys*/
/*                                      if INT64 is xlated to OCTETSTR*/
/* End Change Log *****************************************************/

#include "dmisa.h"     /* global header file for DMI Sub-agent        */

extern char logbuffer[];
#ifdef DMISA_TRACE
extern unsigned char logbuffer[];
void traceReqBuf(DMI_MgmtCommand_t *mgmtreq);
#endif

/**********************************************************************/
/*  Function: keyFromDpi()                                            */
/*                                                                    */
/*  Input parameters:                                                 */
/*    ptr. to DpiKey string                                           */
/*    ptr. to current Group/Component structure                       */
/*                                                                    */
/*  Output parameters:                                                */
/*    ptr. to ptr. to DmiGroupKeyList structure                       */
/*    ptr. to number of bytes in DmiGroupKeyList structure            */
/*                                                                    */
/*  Description:                                                      */
/*   While not at end of DpiKey and not past the last iKeyType,       */
/*    walk thru iKeyType values (starting with the first one), and    */
/*    use iKeyType to determine the string length to be moved         */
/*    from the DpiKey to DmiKey.  If the key type is a string,        */
/*    use the first byte to determine the length to put into DmiKey.  */
/*                                                                    */
/* Notes:                                                             */
/*   1. If a partially-specified string is found, null out remainder  */
/*      remainder of the string.  Any other OIDs remaining will be:   */
/*         Integer, Counter, Gauge = 0                                */
/*         Dates will be nulled for 28 bytes                          */
/*         Strings will be zero lengthed                              */
/*         return code is set to  KEY_FROM_DPI_tooShort.              */
/*         Translation continues on rest of OIDs.                     */
/*                                                                    */
/*   2. If an integer or a counter is:                                */
/*      '\0', return code will be set to KEY_FROM_DPI_tooShort        */
/*      and processing will countinue per the rules above when        */
/*      an error such as this occurs.                                 */
/*      If an integer or a counter or a gauge starts out ok, but      */
/*      has a '\0' within it, and is not the last OID to be           */
/*      processed, that value (up to the '\0') will be stuffed        */
/*      in the DMI Key, return code will be set to                    */
/*      KEY_FROM_DPI_tooShort,                                        */
/*      and processing will countinue per the rules above when        */
/*      an error such as this occurs.                                 */
/*      If it's the last OID, we assume this to be ok.                */
/*                                                                    */
/*   3. If a partially-specified DATE is found, null out remainder    */
/*      of the DATE.                                                  */
/*      Return code is set to  KEY_FROM_DPI_tooShort.                 */
/*      Processing will countinue per the rules above when            */
/*      an error such as this occurs.                                 */
/*                                                                    */
/*   4.  *rtrn_ptr points to storage allocated but not freed          */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/*                                                                    */
/*   integer   - move the next sub-id, in 4-byte hex representation,  */
/*               from DpiKey into DmiKey                              */
/*   integer64 - translated only if DMI INT64 is SNMP(8byte)OCTETSTR; */
/*               otherwise if viewed as counter64, NOT TRANSLATED.9503*/
/*               (See MIF2MIB ASSUMPTIONS block comment.)             */
/*   gauge     - move the next sub-id, in 4-byte hex representation,  */
/*               from DpiKey into DmiKey                              */
/*   counter   - move the next sub-id, in 4-byte hex representation,  */
/*               from DpiKey into DmiKey                              */
/*   counter64 - NOT TRANSLATED, key translation is aborted.          */
/*               (See MIF2MIB ASSUMPTIONS block comment.)             */
/*   displaystring - use the next sub-id in DpiKey as the number of   */
/*               sub-id's, each holding a byte, that follow.          */
/*               These are concatenated to form the string for DmiKey.*/
/*   octetstring - use the next sub-id in DpiKey as the number of     */
/*               sub-id's, each holding a byte, that follow.          */
/*               These are concatenated to form the string for DmiKey.*/
/*   date      - compile the next 28 sub-id's, each a single byte,    */
/*               into a string and move into DmiKey.                  */
/*               Otherwise, return "key translation error."           */
/*                                                                    */
/* Return codes: KEY_FROM_DPI_noError     - no error                  */
/*               KEY_FROM_DPI_tooShort    - DpiKey too short          */
/*               KEY_FROM_DPI_tooLong     - DpiKey too long           */
/*               KEY_FROM_DPI_illegalKey  - Illegal key               */
/*               KEY_FROM_DPI_otherError  - key translation error     */
/*               KEY_FROM_DPI_outOfMemory - error occurred from malloc*/
/**********************************************************************/

int keyFromDpi( GCPair_p cur_gc_ptr, char *start_here_ptr,
                DMI_GroupKeyData_t **rtrn_ptr, ULONG *number_of_bytes )

{

int                 i,j,k,rc=KEY_FROM_DPI_noError; /* will return "noError" if not set during translate */
int                 tmp_int, left_over, null_uhoh = 0; /* temporary holder for a counter */
int                 current_oid_number; /* holder for what oid we're working on */
ULONG               number_of_keys;   /* holder for the number of keys to play with */
GCKey_p             keypair_ptr;      /* holder for key pair pointer */
DMI_GroupKeyData_t *groupkeydata_ptr; /*ptr for group key data structures */
char               *GroupKeyBlock_ptr; /* char ptr to the block of data we need to build */
char               *tmp_dpi_ptr;      /* temporary holder for DPI pointer */
char               *pre_tmp_dpi_ptr;  /* temporary holder for previous  DPI pointer */
unsigned char      *tmp_key_value_ptr; /* temporary holder for key pointer */
char                fake_int[20];     /* temporary holder for fake integers, will use atoi */
char               *restofit;         /* ptr to unload strings and DMI dates from OID */
ULONG               len, uli;         /* used to unload strings and DMI dates from OID */
DMI_STRING         *dmi_string_ptr;   /* useful for pointing to dmi strings */
DMI_INT            *dmi_integer_ptr;    /* for PITOA */
DMI_UNSIGNED       *dmi_counter_ptr;    /* for PITOA */
int                tmpsize;

/* DMI_BYTE is an unsigned char  - like date, octet string    */
/* DMI_INT is a signed long int - like integer                */
/* DMI_OFFSET is a unsigned long int -  like all offsets      */
/* DMI_UNSIGNED is an unsigned long int - like counter, guage */

/**************************************************************/
/* end of declares etc                                        */
/**************************************************************/

/* This routine will first calculate the amount of storage */
/* necessary to create the DMI Group Key data area         */
/* and also check to see if there's a valid DPI string     */
/* to translate */

   *rtrn_ptr = NULL;
/* extract the number of keys from the first GCPair */
/* initialize temporary pointers */
   tmp_dpi_ptr = start_here_ptr;
   *number_of_bytes = 0;  /* initialize total number of bytes for data passed back */

/* to get to the first keypair, you must increment past */
/* the first GC block (by adding 1 to the structure pointer */

   keypair_ptr = (GCKey_p )(cur_gc_ptr + 1);

   number_of_keys = cur_gc_ptr->iKeyCount;

   for (i=0; i<number_of_keys; i++){

      switch (keypair_ptr->iKeyAttrType){ /*determine what iKeyType we're dealing with */

         case MIF_INTEGER:
            *number_of_bytes += sizeof(DMI_INT);

/* as far as incrementing tmp_dpi_ptr */
/* things are a bit more complicated than just adding sizeof(DMI_INT) here */
/* The integer is represented in "stacked" decimal bytes... */
/* therefore we have to increment up to another "." which terminates the integer*/
/* and then add one more to get past that last little "." */
/*            tmp_dpi_ptr += sizeof(DMI_INT) + 1;*/

            while (*tmp_dpi_ptr != '.' && *tmp_dpi_ptr != '\0'){  /* while not at a . or null */
               tmp_dpi_ptr++;       /* increment the tmp_dpi_ptr */
            }
            tmp_dpi_ptr++;              /* get it past the . */
            break;

/* guages and counters for the time being are treated the same */

         case MIF_GAUGE:
         case MIF_COUNTER:
            *number_of_bytes += sizeof(DMI_UNSIGNED);
            while (*tmp_dpi_ptr != '.' && *tmp_dpi_ptr != '\0'){  /* while not at a . or null */
               tmp_dpi_ptr++;       /* increment the tmp_dpi_ptr */
            }
            tmp_dpi_ptr++;              /* get it past the . */
            break;

         case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
/* number of bytes bypassed for the integer64 is 8 */
            *number_of_bytes += sizeof(DmiInt64Str);                 /* added 950330.gwl */
            for (j=1; j<=sizeof(DmiInt64Str); ++j) {                 /* added 950330.gwl */
               if (isdigit(*tmp_dpi_ptr)) {                          /* added 950330.gwl */
                  strtoul(tmp_dpi_ptr, &restofit, 10);               /* added 950330.gwl */
                  tmp_dpi_ptr = restofit + 1;   /* Bypass the dot       added 950330.gwl */
                  if (*restofit == '\0') break;                      /* added 950330.gwl */
                                       /* From the parse function we know there is a dot here*/
               }
            }
            break;
#endif
         case MIF_COUNTER64:
/* *64 counter isn't supported so we'll return an error */
            rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
            break;

         case MIF_DISPLAYSTRING:
         case MIF_OCTETSTRING:

/* need to go to the DPIKey to see exactly how many bytes we need to allocate */
/* number of bytes should be equal to the value of what the current DPI PTR */
/* is pointing to, plus one  for the first value has the length of the string */
/* tmp_dpi_ptr must be incremented for all of those bytes, times 2 since there's a  */
/* "." between every byte, plus the end "." , plus the length */

/* Need to increment by "sizeof(DMI_BYTE) for machine independence */
/* to get the value of the number of characters, need to increment */
/* until we run into a '.' or a '/0'                              */
/*          j=0;                                                      */
/*          while (*tmp_dpi_ptr != '.' && *tmp_dpi_ptr != '\0'){  ** while not at a . or null */
/*             fake_int[j++] = *tmp_dpi_ptr++;       ** increment the tmp_dpi_ptr */
/*          }                                                         */
/*          fake_int[j]='\0';                                         */
            len = strtoul(tmp_dpi_ptr, &restofit, 10);  /* Was of fake_int string*/
            *number_of_bytes += (len + sizeof(DMI_UNSIGNED)) *   /* Extend for length field*/
                                sizeof(DMI_BYTE);                    /* (changed 941025.GWL)*/
            tmp_dpi_ptr = ++restofit;  /* Bypass dot                 // (added 941025.GWL)*/

/* number of bytes bypassed for the date is 25 separating dots plus */
/* the number of decimal digits per each of the 25 numbers.         */
/* Not 28, since the last 3 hex bytes are always filled in as zeros.*/
            for (j=1; j<=len; ++j) {                                 /* (added 941025.GWL)*/
               if (isdigit(*tmp_dpi_ptr)) {                          /* (added 941025.GWL)*/
                  strtoul(tmp_dpi_ptr, &restofit, 10);               /* (added 941025.GWL)*/
                  tmp_dpi_ptr = restofit + 1;   /* Bypass the dot       (added 941025.GWL)*/
                  if (*restofit == '\0') break;                      /* (added 941025.GWL)*/
                                            /* From parse function we know there is a dot here*/
               }
            }
            break;

         case MIF_DATE:

/* number of bytes for the date is fixed at 28             */
/* but the tmp_dpi_ptr must increment past those 28, plus  */
/* another 28 for the "."s inbetween                       */
            *number_of_bytes += DATE_LENGTH_dmi*(sizeof(DMI_BYTE));  /* (changed 941025.GWL)*/
/* number of bytes bypassed for the date is 25 separating dots plus */
/* the number of decimal digits per each of the 25 numbers.         */
/* Not 28, since the last 3 hex bytes are always filled in as zeros.*/
            for (j=1; j<=DATE_LENGTH_snmp; ++j) {                    /* (added 941925.GWL)*/
               if (isdigit(*tmp_dpi_ptr)) {                          /* (added 941025.GWL)*/
                  strtoul(tmp_dpi_ptr, &restofit, 10);               /* (added 941025.GWL)*/
                  tmp_dpi_ptr = restofit + 1;   /* Bypass the dot       (added 941025.GWL)*/
                  if (*restofit == '\0') break;                      /* (added 941025.GWL)*/
                                            /* From parse function we know there is a dot here*/
               }
            }
            break;

         case MIF_UNKNOWN_DATA_TYPE:
            rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
            break;

         default:
            rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
            break;
      } /*end switch for iKeyType  */

/* must update the pointer to the next key pair before continuing with the FOR loop */

      keypair_ptr++;

      if(rc != KEY_FROM_DPI_noError){
         return(rc);
      }/*end of check for xlate error */

   }/* end of for loop for caluculation of number of bytes in key values */

/***** Now that we have the number of bytes for the data area, we need */
/***** to check and see if there's a null terminator in the string     */
/***** somewhere.  If there's one before the presumed end, then        */
/***** the error checking below should catch it.                       */
/*****  but if there's not one at the                                  */
/***** end, we return a "key to long", and if there's one exactly we   */
/***** we suspect it to be, then everything's hunky dory               */
/***** the number of bytes we should look at should be equal to where  */
/***** we started from (the start_here_ptr) to where we finished up    */
/***** (where tmp_dpi_ptr ended                                        */

/* now check the last value and it should be a null terminator */
/* if it isn't, return key to long                             */
/* because of the way we increment the pointer, we need to back */
/* up one */

   if ( *(--tmp_dpi_ptr) != '\0' ){
      rc = KEY_FROM_DPI_tooLong;
   }/* end of if key to short */

/* now we have to add in the number of bytes for the DMIgroupkeydata blocks */
/* that appear at the top of the group key list */
/* that's simple - just multiply the number of keys by the size of each block */

   *number_of_bytes += number_of_keys*sizeof(DMI_GroupKeyData_t);

/* debug printf */
/*printf("the number of bytes needed for malloc is: %i\n",*number_of_bytes);*/

   if( NULL == (GroupKeyBlock_ptr = (char *) malloc(*number_of_bytes)) )
      return(KEY_FROM_DPI_outOfMemory); /* couldn't malloc any space, go back */

/* now we have the block of data to do operations with, build the key control block */

/* set up some pointers to this block that will help us fill out storage */
/* malloc returns a characterpointer that points to the top of the block */
/* also need to set the pointer to a pointer equal to the top of the     */
/* block as this is what is returned by the program                      */

   *rtrn_ptr = (DMI_GroupKeyData_t *)GroupKeyBlock_ptr;

/* begin by casting the top of the block to the group key data structure */
/* and then assign to a temporary variable the start of the key value area */

   groupkeydata_ptr = (DMI_GroupKeyData_t *)GroupKeyBlock_ptr;
   tmp_key_value_ptr = (unsigned char*) (groupkeydata_ptr + number_of_keys);

/* now the real work begins in filling out the group key data block */
/* and then calculating the offsets for the key values */
/* and then filling in the key values */

/* initialize temporary pointers */
   tmp_dpi_ptr = start_here_ptr;

/* to get to the first keypair, you must increment past */
/* the first GC block (by adding 1 to the structure pointer */

   keypair_ptr = (GCKey_p )(cur_gc_ptr + 1);
   current_oid_number = number_of_keys; /* use this to make sure last oid can end in '\0' */

   for(i=0; i<number_of_keys; i++){

      groupkeydata_ptr->iAttributeId = keypair_ptr->iKeyAttrId; /* move the 1st */
      groupkeydata_ptr->iAttributeType = keypair_ptr->iKeyAttrType; /* move the 2nd */

/*calculate the offset, it should be the difference between the  */
/* current key value pointer minus the top of the block pointer */
/*  (I hope ) */

      groupkeydata_ptr->oKeyValue = (char *)tmp_key_value_ptr-GroupKeyBlock_ptr;

/* now that the group key data block is filled out, fill out the key values */
/* we'll have to check some values before we fill out the key values, since */
/* there could be errors about as described in the prolog                   */
/* A simple test of null_uhoh will determine if we have to take radical     */
/* action and falsify a key                                                 */

      if ( null_uhoh ){

/* if we got here, there's a null early on in the DPI key                  */
/* which isn't too good a situation if you know what I mean                */
/* SOP for this situation is to zero ints, counters and gauges,            */
/* and set strings to zero length and null out the date fields             */
/* and they should have been set anyway in the previous switch block       */

         switch (keypair_ptr->iKeyAttrType){ /*determine what iKeyType we're dealing with */

            case MIF_INTEGER:
               dmi_integer_ptr = (DMI_INT *)tmp_key_value_ptr;
               /*   *dmi_integer_ptr = 0; */
               memset((char *)tmp_key_value_ptr, 0, sizeof(int));
               tmp_key_value_ptr  += sizeof(DMI_INT);
               break;

            case MIF_GAUGE:
            case MIF_COUNTER:
/* treat guages and counters the same */
               dmi_counter_ptr = (DMI_UNSIGNED *)tmp_key_value_ptr;
               /**dmi_counter_ptr = 0;*/
               memset((char *)tmp_key_value_ptr, 0, sizeof(int));
               tmp_key_value_ptr  += sizeof(DMI_UNSIGNED);
               break;

            case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
               for (j=1; j<=sizeof(DmiInt64Str); ++j) {                 /* added 950330.gwl */
                  *tmp_key_value_ptr++ = '\0'; /* move the null value into int64.950330.gwl */
               }
               break;
#endif
            case MIF_COUNTER64:
/* counter64 cannot be translated into an index so set keyvalue = 0 and return translation error */
               rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
               break;


            case MIF_DISPLAYSTRING:
            case MIF_OCTETSTRING:
/* treat display and octet strings the same */
/* first move the length field */
               dmi_string_ptr = (DMI_STRING *)tmp_key_value_ptr;
               /*dmi_string_ptr->length = 0;*/
               memset((char *)&dmi_string_ptr->length, 0, sizeof(int));
               tmp_key_value_ptr += sizeof(DMI_UNSIGNED);
               break;

            case MIF_DATE:

               for (j=1; j<=DATE_LENGTH_dmi; ++j) {                     /* (changed 941025.GWL)*/
                  *tmp_key_value_ptr++ = '\0'; /* move the null value into the date */
               }
               /* the for loop should have incremented tmp_key_value_ptr to the right val */
               break;

            case MIF_UNKNOWN_DATA_TYPE:

/* should never get an unknown data type... I hope */
               rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
               break;

            default:
               rc = KEY_FROM_DPI_otherError;  /* key translation error */
               break;
         } /*end switch for iKeyType  */

      }/* end of if there's a null_uhoh */
      else{

         switch (keypair_ptr->iKeyAttrType){ /*determine what iKeyType we're dealing with */

            case MIF_INTEGER:

               if ( *tmp_dpi_ptr == '\0' ){ /* if there's a null in the first position */
                  null_uhoh = 1; /* set the null_uhoh so the rest of the key gets filled */
                  rc = KEY_FROM_DPI_tooShort;
                  dmi_integer_ptr = (DMI_INT *)tmp_key_value_ptr;
                  /**dmi_integer_ptr = 0;*/ /*stuff a zero */
                  memset((char *)tmp_key_value_ptr, 0, sizeof(int));
                  tmp_key_value_ptr  += sizeof(DMI_INT);
                  break;
               }/* end of if there's a null in the first position */
               else{ /* continue processing */
                  if ( *tmp_dpi_ptr == '.' ){/* there's a '.' in the first position */
                     rc = KEY_FROM_DPI_otherError;  /* key translation error */
                     return(rc); /* get way out of here */
                  } /* end of if there's a '.' in the first position */
               }/* end of else if there's a null in the first position */

               j=0;
               do {

/* ok, this is confusing so here goes:  When we get here, if we get here */
/* we've already checked for a '\0', and a ',', so it's gotta be ok for  */
/* at least the first character in this do-while loop                    */
/* The loop first checks if it's a '\0', remember the first time  through*/
/* it can't possibly be, and if it isn't, will copy the value into       */
/* fake_int[j].  now there's the additional check at the while, and      */
/* the pointers are incremented.  The next time through, the next value */
/* could be '\0' so if it is, it's checked to see if this is the last   */
/* OID.  If it is, we just copy it in fake_int and let the while        */
/* at the bottom break us out.                                          */
/* if it isn't the last OID, we set the null_uhoh and key to short      */
/* and let whatever value is in fake_int get stored and then ....       */
/* In order to use the do-while loop, we have to set the present value  */
/* of the tmp_dpi_ptr, but have to increment tmp_dpi_ptr to point to    */
/* the next value so it's processed.  The draw back is when we break out */
/* of the do-while, we've got to decrement the points since they overshot */

                  if (  *tmp_dpi_ptr == '\0' && current_oid_number != 1){
                     null_uhoh = 1; /* set the null_uhoh so  the key gets filled */
                     rc = KEY_FROM_DPI_tooShort;
                  /* the do while should now break us out of here when it discovers */
                  /* the '\0'  */
                  }/* end of if we've really got an error */

                  pre_tmp_dpi_ptr = tmp_dpi_ptr; /* slight of hand here, for check */
                  fake_int[j++]=*tmp_dpi_ptr++;

               } while (*pre_tmp_dpi_ptr != '.' && *pre_tmp_dpi_ptr != '\0');

               tmp_dpi_ptr--; /* adjust the pointer since we overshot by 1 */

               fake_int[--j]='\0';
               dmi_integer_ptr = (DMI_INT *)tmp_key_value_ptr;
               tmpsize = atoi(fake_int); /*convert char to int */
               memcpy((char*)tmp_key_value_ptr, (char *)&tmpsize, sizeof(int));
               tmp_dpi_ptr++; /* to get past the "." */
               tmp_key_value_ptr  += sizeof(DMI_INT);
               break;

/* again, treat guages and counters the same */
            case MIF_GAUGE:
            case MIF_COUNTER:

               if ( *tmp_dpi_ptr == '\0' ){ /* if there's a null in the first position */

                  null_uhoh = 1; /* set the null_uhoh so the rest of the key gets filled */
                  rc = KEY_FROM_DPI_tooShort;
                  dmi_counter_ptr = (DMI_UNSIGNED *)tmp_key_value_ptr;
                  /**dmi_counter_ptr = 0UL;*/ /* zero out, changed 8/18/94 */
                  memset((char *)tmp_key_value_ptr, 0, sizeof(int));
                  tmp_key_value_ptr  += sizeof(DMI_UNSIGNED);
                  break;
               }/* end of if there's a null in the first position */

               else{ /* continue processing */
                  if ( *tmp_dpi_ptr == '.' ){/* there's a '.' in the first position */
                     rc = KEY_FROM_DPI_otherError;  /* key translation error */
                     return(rc); /* get way out of here */
                  } /* end of if there's a '.' in the first position */
               }/* end of else if there's a null in the first position */

               j=0;
               do {

/* ok, this is confusing so here goes:  When we get here, if we get here */
/* we've already checked for a '\0', and a ',', so it's gotta be ok for  */
/* at least the first character in this do-while loop                    */
/* The loop first checks if it's a '\0', remember the first time  through*/
/* it can't possibly be, and if it isn't, will copy the value into       */
/* fake_int[j].  now there's the additional check at the while, and      */
/* the pointers are incremented.  The next time through, the next value */
/* could be '\0' so if it is, it's checked to see if this is the last   */
/* OID.  If it is, we just copy it in fake_int and let the while        */
/* at the bottom break us out.                                          */
/* if it isn't the last OID, we set the null_uhoh and key to short      */
/* and let whatever value is in fake_int get stored and then ....       */

                  if (  *tmp_dpi_ptr == '\0' && current_oid_number != 1){
                     null_uhoh = 1; /* set the null_uhoh so  the key gets filled */
                     rc = KEY_FROM_DPI_tooShort;
                  }/* end of if we've really got an error */

                  pre_tmp_dpi_ptr = tmp_dpi_ptr; /* slight of hand here, for check */
                  fake_int[j++]=*tmp_dpi_ptr++;

               } while (*pre_tmp_dpi_ptr != '.' && *pre_tmp_dpi_ptr != '\0');

               tmp_dpi_ptr--; /* adjust the pointer since we overshot by 1 */
               fake_int[--j]='\0';

               dmi_counter_ptr = (DMI_UNSIGNED *)tmp_key_value_ptr;
               tmpsize = strtoul(fake_int, (char **)NULL, 10);
                             /* convert char to int, changed 8/18/94 */
               memcpy((char *)tmp_key_value_ptr, (char *)&tmpsize, sizeof(int));
               tmp_dpi_ptr++; /* to get past the "." */
               tmp_key_value_ptr  += sizeof(DMI_UNSIGNED);
               break;

            case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
/* Copy in 8 bytes from OID          */
               left_over = tmp_int = sizeof(DmiInt64Str);               /* added 950330.gwl */

               for (j=0; j<sizeof(DmiInt64Str); j++){
                  if ( *tmp_dpi_ptr != '\0'){
                     if (isdigit(*tmp_dpi_ptr)) {                       /* added 950330.gwl */
                        uli = strtoul(tmp_dpi_ptr, &restofit, 10);      /* added 950330.gwl */
                        if (uli <= 0xFF) {                            /* added 950330.gwl */
                           *(tmp_key_value_ptr++) = uli;                /* added 950330.gwl */
                        } else {
                           *(tmp_key_value_ptr++) = 0xFF;             /* added 950330.gwl */
                        }
                        tmp_dpi_ptr = restofit + 1;   /* Bypass the dot    added 950330.gwl */
                        if (*restofit == '\0') break;                   /* added 950330.gwl */
                                    /* From parse function we know there is a dot here      */
                     }
                     left_over--; /* decrement this in case we run into a '\0' added 950330.gwl */
                  } else {
                     null_uhoh = 1; /* set null_uhoh so the rest of key filled.added 950330.gwl */
                     for (k=0;k<left_over ;k++ ) {
                        *tmp_key_value_ptr++ = '\0';  /* null out rest of key. added 950330.gwl */
                     }
                     rc = KEY_FROM_DPI_tooShort;                        /* added 950330.gwl */
                     break;                                             /* added 950330.gwl */
                  }
               }
               break;                                                   /* added 950330.gwl */
#endif
            case MIF_COUNTER64:
/* SNMPV1 and doesn't support int64 so set keyvalue = 0 and return translation*/
/* error */
               rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
               break;

/* treat display and octet strings the same */

            case MIF_DISPLAYSTRING:
            case MIF_OCTETSTRING:

/* need to get the number of bytes to move first */

               if ( *tmp_dpi_ptr == '\0' ){ /* if there's a null in the first position */

                  null_uhoh = 1; /* set the null_uhoh so the rest of the key gets filled */
                  rc = KEY_FROM_DPI_tooShort;
                  dmi_string_ptr = (DMI_STRING *)tmp_key_value_ptr;
                  /*dmi_string_ptr->length = 0UL;*/
                  memset((char *)&dmi_string_ptr->length, 0, sizeof(int));
                    /* zero length when no string in OID, changed 8/18/94 */
                  tmp_key_value_ptr += sizeof(DMI_UNSIGNED);
                  break;

               }/* end of if there's a null in the first position */

               else{ /* continue processing */
                  if ( *tmp_dpi_ptr == '.' ){/* there's a '.' in the first position */
                      rc = KEY_FROM_DPI_otherError;  /* key translation error */
                     return(rc); /* get way out of here */
                  } /* end of if there's a '.' in the first position */
               }/* end of else if there's a null in the first position */

               j=0;
               do {

/* ok, this is confusing so here goes:  When we get here, if we get here */
/* we've already checked for a '\0', and a ',', so it's gotta be ok for  */
/* at least the first character in this do-while loop                    */
/* The loop first checks if it's a '\0', remember the first time  through*/
/* it can't possibly be, and if it isn't, will copy the value into       */
/* fake_int[j].  now there's the additional check at the while, and      */
/* the pointers are incremented.  The next time through, the next value */
/* could be '\0' so if it is, it's checked to see if this is the last   */
/* OID.  If it is, we just copy it in fake_int and let the while        */
/* at the bottom break us out.                                          */
/* if it isn't the last OID, we set the null_uhoh and key to short      */
/* and let whatever value is in fake_int get stored and then ....       */

                  if (  *tmp_dpi_ptr == '\0' ){ /* length should always end in a '.' */
                     null_uhoh = 1; /* set the null_uhoh so  the key gets filled */
                     rc = KEY_FROM_DPI_tooShort;
                  }/* end of if we've really got an error */

                  pre_tmp_dpi_ptr = tmp_dpi_ptr; /* slight of hand here, for check */
                  fake_int[j++]=*tmp_dpi_ptr++;

               } while (*pre_tmp_dpi_ptr != '.' && *pre_tmp_dpi_ptr != '\0');

/*             tmp_dpi_ptr--; ** adjust the pointer since we overshot by 1 */
               fake_int[--j]='\0';

/* A DMI String has the first 4 octects as the length - */
/* the standard structure specifies DMI_UNSIGNED as the */
/* holder of the length                                 */

/* first move the length field */

               dmi_string_ptr = (DMI_STRING *)tmp_key_value_ptr;
               tmpsize = strtoul(fake_int, (char **)NULL, 10);  /* (changed 941025.GWL)*/
               memcpy((char *)&dmi_string_ptr->length ,
                      (char *)&tmpsize, sizeof(int));
               tmp_key_value_ptr += sizeof(DMI_UNSIGNED);

/* the number of bytes to move will be the atoi of the length field */
/* also bump the tmp_dpi_ptr by one to get past the '.' */
/* left_over will decrement so if we run into a '\0' we */
/* can recover by putting '\0''s in the DMI key value   */

/*             tmp_dpi_ptr++;                                         */
               left_over = tmp_int = tmpsize*sizeof(DMI_BYTE);  /* (changed 941025.GWL)*/

               for (j=0; j<tmp_int; j++){
                  if ( *tmp_dpi_ptr != '\0'){
                     if (isdigit(*tmp_dpi_ptr)) {                          /* (added 941025.GWL)*/
                        uli = strtoul(tmp_dpi_ptr, &restofit, 10);         /* (added 941025.GWL)*/
                        if (uli <= 0xFF) {                               /* (added 941025.GWL)*/
                           *(tmp_key_value_ptr++) = uli;                   /* (added 941025.GWL)*/
                        } else {
                           *(tmp_key_value_ptr++) = 0xFF;                /* (added 941025.GWL)*/
                        }
                        if (*restofit == '\0') break;                      /* (added 941025.GWL)*/
                        tmp_dpi_ptr = restofit + 1;   /* Bypass the dot       (added 941025.GWL)*/
                                         /* From parse function we know there is a dot here*/
                     }
/*                   *tmp_key_value_ptr++ = *tmp_dpi_ptr; ** move the value */
/*                   tmp_dpi_ptr++; ** will point to "."            */
/*                   tmp_dpi_ptr++; ** will point to next value */
                     left_over--; /* decrement this in case we run into a '\0' */
                  }/* end of if tmp_dpi_ptr != '\0' */

                  else { /* we've got a null here, take action */
                     null_uhoh = 1; /* set the null_uhoh so the rest of the key gets filled */

                     for (k=0;k<left_over ;k++ ) {
                        *tmp_key_value_ptr++ = '\0'; /* null out the rest of the key */
                     } /* endfor that nulls out the remainder of the string */

                     rc = KEY_FROM_DPI_tooShort; /* set the return code */
                     break; /* get the heck out of here */

                  }/* end of else we've got a null here ... */
               }/* for loop to move string data from dpikey to dmikey */

            /* the for loop should have incremented tmp_key_value_ptr to the right val */
            /* tmp_dpi_ptr also points to the next value to process */
               break;

            case MIF_DATE:
/* Copy in 25 bytes from OID, NULL out last 3 of 28 bytes.          */
/* number of bytes bypassed for the date is 25 separating dots plus */
/* the number of decimal digits per each of the 25 numbers.         */
/* Not 28, since the last 3 hex bytes are always filled in as zeros.*/
               left_over = tmp_int = DATE_LENGTH_dmi*sizeof(DMI_BYTE);

               for (j=0; j<DATE_LENGTH_snmp; j++){

                  if ( *tmp_dpi_ptr != '\0'){
                     if (isdigit(*tmp_dpi_ptr)) {                          /* (added 941025.GWL)*/
                        uli = strtoul(tmp_dpi_ptr, &restofit, 10);         /* (added 941025.GWL)*/
                        if (uli <= 0xFF) {                               /* (added 941025.GWL)*/
                           *(tmp_key_value_ptr++) = uli;                   /* (added 941025.GWL)*/
                        } else {
                           *(tmp_key_value_ptr++) = 0xFF;                /* (added 941025.GWL)*/
                        }
                        tmp_dpi_ptr = restofit + 1;   /* Bypass the dot       (added 941025.GWL)*/
                        if (*restofit == '\0') break;                      /* (added 941025.GWL)*/
                                         /* From parse function we know there is a dot here*/
                     }
                     left_over--; /* decrement this in case we run into a '\0' */
                  } else {
                     null_uhoh = 1; /* set the null_uhoh so the rest of the key filled */

                     for (k=0;k<left_over ;k++ ) {
                        *tmp_key_value_ptr++ = '\0'; /* null out the rest of the key */
                     }

                     rc = KEY_FROM_DPI_tooShort; /* set the return code */
                     break;
                  }/* end of else */
               }/* for loop to move data from dpikey to dmikey */
               *(tmp_key_value_ptr++) = 0;                /* (added 941025.GWL)*/
               *(tmp_key_value_ptr++) = 0;                /* (added 941025.GWL)*/
               *(tmp_key_value_ptr++) = 0;                /* (added 941025.GWL)*/


            /* the for loop should have incremented tmp_key_value_ptr to the right val */
            /* tmp_dpi_ptr also points to the next value to process */
               break;

            case MIF_UNKNOWN_DATA_TYPE:
/* should never get an unknown data type... I hope */
               rc = KEY_FROM_DPI_illegalKey;  /* key translation error */
               break;

            default:
               rc = KEY_FROM_DPI_otherError;  /* key translation error */
               break;
         } /*end switch for iKeyType  */

      }/* end of else of if there's a null_uhoh */

      keypair_ptr++;      /* move to the next key pair */
      groupkeydata_ptr++; /* move to the next block of keys */
      current_oid_number--; /* keep track of what oid we're working on */

   }/* end of for that moves the data into the group key block */

   return(rc);

}/* end of keyFromDpi */


/**********************************************************************/
/*  Function: keyToDpi()                                              */
/*                                                                    */
/*  Input parameters:                                                 */
/*    ptr. to DmiKey structure                                        */
/*    number of DMI keys expected to translate                        */
/*                                                                    */
/*                                                                    */
/*  Output parameters:                                                */
/*    ptr. to DpiKey string  (char *)                                 */
/* return code = KEY_TO_DPI_noError     - no error                    */
/*               KEY_TO_DPI_otherError  - key translation error       */
/*               KEY_TO_DPI_outOfMemory - error occurred from malloc  */
/*                                                                    */
/*  Description:                                                      */
/*   1. For each key, append to the string the appropriate sub-id's,  */
/*      separated by periods, to form a dotted-decimal index.         */
/*   2. Append a string termination character.                        */
/*                                                                    */
/* Notes:                                                             */
/*   1.  *dpi_string_ptr points to storage allocated but not freed    */
/*       by this function.  It must be freed by the caller.           */
/*                                                                    */
/*   integer   - append a 4-byte sub-id to DmiKey                     */
/*   integer64 - translated only when made into octet(8byte),   950330*/
/*               otherwise NOT TRANSLATED.                      950330*/
/*               (See MIF2MIB ASSUMPTIONS block comment.)             */
/*   gauge     - append a 4-byte sub-id to DmiKey                     */
/*   counter   - append a 4-byte sub-id to DmiKey                     */
/*   counter64 - NOT TRANSLATED, key translation is aborted.          */
/*               (See MIF2MIB ASSUMPTIONS block comment.)             */
/*   displaystring - append a sub-id of n, where n is the length of   */
/*               the key's current value, and n sub-id's,             */
/*               each one byte long, one per byte in the key.         */
/*   octetstring - append a sub-id of n, where n is the length of     */
/*               the key's current value, and n sub-id's,             */
/*               each one byte long, one per byte in the key.         */
/*   date      - append a sub-id of 28, followed by 28 sub-id's,      */
/*               one for each byte in the date.                       */
/*                                                                    */
/**********************************************************************/

typedef struct {
unsigned int length;
char     body[500];
}TMPDMI_STRING;

int keyToDpi( DMI_GroupKeyData_t *cur_gc_ptr, int number_of_keys,
              char **dpi_string_ptr )

{

int                 i,j,rc=KEY_TO_DPI_noError; /* will return 0 if not set during translate */
ULONG               number_of_bytes = 0;   /* number of bytes for all group key data */
/*GCKey_p keypair_ptr;     */    /* holder for key pair pointer */
char               *GroupKeyBlock_ptr; /* char ptr to the block of data we need to build */
DMI_GroupKeyData_t *groupkeydata_ptr;  /*ptr for group key data structures */
char               *tmp_dpi_ptr ;      /* holder for DPI key string pointer */
char               *tmp_key_value_ptr; /* temporary holder for key pointer */
char                fake_int[20];  /* temporary holder for fake integers, will use pitoa */
DMI_STRING         *dmi_string_ptr;    /* useful for pointing to dmi strings */
DMI_INT            *dmi_integer_ptr;   /* for PITOA */
DMI_UNSIGNED       *dmi_counter_ptr;   /* for PITOA */
TMPDMI_STRING       dmistring;
int                 tmplen;

/**************************************************************/
/* end of declares etc                                        */
/**************************************************************/

   *dpi_string_ptr = NULL;
/* first calculate how much storage we'll need for the  DPI key */

   groupkeydata_ptr = cur_gc_ptr; /* we'll end up wasting this pointer walking through keys */
   GroupKeyBlock_ptr = (char *)cur_gc_ptr; /* need a top of block pointer for offset calcs */
   for (i=0; i<number_of_keys; i++){

         /* in any case, we need to add an offset to get to the value */
         tmp_key_value_ptr = GroupKeyBlock_ptr + groupkeydata_ptr->oKeyValue;

         switch (groupkeydata_ptr->iAttributeType){ /*determine what iKeyType we're dealing with */

         case MIF_INTEGER:
            dmi_integer_ptr = (DMI_INT *)tmp_key_value_ptr;
            memcpy((char *)&tmplen, 
                   (char *)tmp_key_value_ptr,
                           sizeof(int)); 
            pitoa( tmplen , fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               j++;
            }
/* number of bytes to hold this integer depends on how many "bytes" to hold */
/* the character representation of it, and don't forget the ending '.'      */
            number_of_bytes += j*sizeof(DMI_BYTE)+1;
            break;

         case MIF_GAUGE:
         case MIF_COUNTER:
/* guages and counters for the time being are treated the same */
            dmi_counter_ptr = (DMI_UNSIGNED *)tmp_key_value_ptr;
            memcpy((char *)&tmplen, 
                   (char *)tmp_key_value_ptr,
                           sizeof(int)); 
            pitoa( tmplen , fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               j++;
            }
/* number of bytes to hold this integer depends on how many "bytes" to hold */
/* the character representation of it  and don't forget the ending '.'      */
            number_of_bytes += j*sizeof(DMI_BYTE)+1;
            break;

         case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
            for (j=0; j<sizeof(DmiInt64Str); ++j, tmp_key_value_ptr++) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int);      /* added 950330.gwl */
               number_of_bytes += strlen(fake_int) * sizeof(DMI_BYTE) +
                                  1;    /* 1 for the separating dot. added 950330.gwl */
            }
            break;                                                /* added 950330.gwl */
#endif
         case MIF_COUNTER64:
/* counter64 isn't supported so we'll return an error */
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;

         case MIF_DISPLAYSTRING:
         case MIF_OCTETSTRING:

/* need to go to the tmpKeyval_p to see exactly how many bytes we need to allocate */
/* number of bytes should be equal to the value of what the current key value */
/* is pointing to times 2 since there's an '.' inbetween all the bytes      */
/* plus the number of digits to hold the PITOA of the length field            */

/* Need to increment by "sizeof(DMI_BYTE) for machine independence */
            dmi_string_ptr = (DMI_STRING *)tmp_key_value_ptr;
 
            memcpy((char *)&tmplen, tmp_key_value_ptr, sizeof(ULONG));
            memcpy((char *)&dmistring, tmp_key_value_ptr, tmplen+sizeof(ULONG));
/* A DMI String has the first 4 octects as the length - */
/* the standard structure specifies DMI_UNSIGNED as the */
/* holder of the length - must covert this to a string */
/* and count it to see how many digits we need to hold */

            pitoa( dmistring.length, fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               j++;
            }
            number_of_bytes += (j + 1) * sizeof(DMI_BYTE);  /* 1 for the dot (added 941024.GWL)*/

            tmp_key_value_ptr = dmistring.body;  /* (added 941024.GWL)*/
            for (j=0; j<=(dmistring.length - 1);
                 ++j, tmp_key_value_ptr++) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int); /* (added 941024.GWL)*/
               number_of_bytes += strlen(fake_int) * sizeof(DMI_BYTE) +
                                  1;    /* 1 for the separating dot (added 941024.GWL)*/
            }
            break;

         case MIF_DATE:
/* number of bytes for the date is calculated for first 25 octets  */
/* of the date.  Space is included for 25 separating dots.         */
            for (j=0; j<=(DATE_LENGTH_snmp - 1);
                 ++j, tmp_key_value_ptr++) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int); /* (added 941024.GWL)*/
               number_of_bytes += strlen(fake_int) * sizeof(DMI_BYTE) +
                                  1;    /* 1 for the separating dot (added 941024.GWL)*/
            }
            break;

         case MIF_UNKNOWN_DATA_TYPE:
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;

         default:
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;
      } /*end switch for iKeyType  */

/* must update the pointer to the next key pair before continuing with the FOR loop */

      groupkeydata_ptr++;

      if(rc != KEY_TO_DPI_noError){
         return(rc);
      }/*end of check for xlate error */

   }/* end of for loop for calculation of number of bytes in key values */


/* debug printf */
/*printf("the number of bytes needed for malloc for keyToDpi is: %i\n",number_of_bytes);*/

   if( NULL == (tmp_dpi_ptr = (char *) malloc(number_of_bytes)) )
      return(KEY_TO_DPI_outOfMemory); /* couldn't malloc any space, go back */

/* set the return value pointer to a pointer so the caller */
/* can get the dpi string we build here                    */

   *dpi_string_ptr = tmp_dpi_ptr;

   groupkeydata_ptr = cur_gc_ptr; /* we'll end up wasting this pointer walking through keys */


   for(i=0; i<number_of_keys; i++){

/* switch to the current GC block's attribute type */

      /* in any case, we need to add an offset to get to the value */
      tmp_key_value_ptr = GroupKeyBlock_ptr + groupkeydata_ptr->oKeyValue;

      switch (groupkeydata_ptr->iAttributeType){ /*determine what iKeyType we're dealing with */

         case MIF_INTEGER:
            dmi_integer_ptr = (DMI_INT *)tmp_key_value_ptr;
            memcpy((char *)&tmplen,
                   (char *)tmp_key_value_ptr,
                           sizeof(int));
            pitoa( tmplen , fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               *tmp_dpi_ptr++ = fake_int[j++];  /* stuff the current digit */
            }
            *tmp_dpi_ptr++ = '.';  /* increment 1 more to hold a "." */
            break;

         case MIF_GAUGE:
         case MIF_COUNTER:
/* treat guages and counters the same */
            dmi_counter_ptr = (DMI_UNSIGNED *)tmp_key_value_ptr;
            memcpy((char *)&tmplen,
                   (char *)tmp_key_value_ptr,
                           sizeof(int));
            pitoa( tmplen , fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               *tmp_dpi_ptr++ = fake_int[j++];  /* stuff the current digit */
            }
            *tmp_dpi_ptr++ = '.';  /* increment 1 more to hold a "." */
            break;

         case MIF_INTEGER64:
#ifdef INT64_IS_OCTETSTRING
            for (j=0; j<sizeof(DmiInt64Str); ++j, ++tmp_key_value_ptr) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int);       /* added 950330.gwl */
               strcpy(tmp_dpi_ptr,fake_int);                       /* added 950330.gwl */
               tmp_dpi_ptr += strlen(fake_int) * sizeof(DMI_BYTE); /* added 950330.gwl */
               *tmp_dpi_ptr++ = '.';                 /* move a "." in the string */
            }
            break; /* tmp_dpi_ptr points to next location to add to.  added 950330.gwl */
#endif
         case MIF_COUNTER64:
/* SNMPV1 and doesn't support int64 so set keyvalue = 0 and return translation*/
/* error */
            *tmp_key_value_ptr = 0;
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;

/* treat display and octet strings the same */

         case MIF_DISPLAYSTRING:
         case MIF_OCTETSTRING:

            dmi_string_ptr = (DMI_STRING *)tmp_key_value_ptr;

            memcpy((char *)&tmplen, tmp_key_value_ptr, sizeof(ULONG));
            memcpy((char *)&dmistring, tmp_key_value_ptr, tmplen+sizeof(ULONG));
/* A DMI String has the first 4 octects as the length - */
/* the standard structure specifies DMI_UNSIGNED as the */
/* holder of the length - must covert this to a string */
/* and count it to see how many digits we need to hold */

            pitoa( dmistring.length, fake_int ); /* convert the int to a string  */
            j=0;
            while (fake_int[j] != '\0'){ /* while not at the end of this string */
               *tmp_dpi_ptr++ = fake_int[j++];
            }
/* tack on a '.' between the length field and the string */
            *tmp_dpi_ptr++ = '.';

/* and increment the tmp_key_value_ptr past the length field */
            tmp_key_value_ptr += sizeof(DMI_UNSIGNED);
/* the number of bytes to move will be the length field */

            for (j=0; j<=(dmistring.length * sizeof(DMI_BYTE) - 1);
                 ++j, tmp_key_value_ptr++) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int); /* (added 941024.GWL)*/
               strcpy(tmp_dpi_ptr,fake_int);                  /* (changed 941024.GWL)*/
               tmp_dpi_ptr += strlen(fake_int) * sizeof(DMI_BYTE); /* (added 941024.GWL)*/
               *tmp_dpi_ptr++ = '.';                 /* move a "." in the string */
            }

            /* the for loop should have incremented tmp_key_value_ptr to the right val */
            /* tmp_dpi_ptr also points to the next value to process */
            break;

         case MIF_DATE:
            for (j=0; j<=(DATE_LENGTH_snmp - 1); ++j, ++tmp_key_value_ptr) {
               pitoa((ULONG)tmp_key_value_ptr[0], fake_int); /* (added 941024.GWL)*/
               strcpy(tmp_dpi_ptr,fake_int);                  /* (changed 941024.GWL)*/
               tmp_dpi_ptr += strlen(fake_int) * sizeof(DMI_BYTE); /* (added 941024.GWL)*/
               *tmp_dpi_ptr++ = '.';                 /* move a "." in the string */
            }
            break; /* tmp_dpi_ptr points to the next location to add to */

         case MIF_UNKNOWN_DATA_TYPE:
/* should never get an unknown data type... I hope */
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;

         default:
            rc = KEY_TO_DPI_otherError;  /* key translation error */
            break;
      } /*end switch for iKeyType  */

      groupkeydata_ptr++;       /* move to the next GroupKey pair */

      if(rc != KEY_TO_DPI_noError){
         return(rc);
      }/*end of check for xlate error */

   }/* end of for that moves the data into the group key block */

/* just one other piece of business before we leave */
/* that's to put a null terminating character on the */
/* end of the data.  We first need to back the tmp_dpi_ptr */
/* one since all cases move it to the next valid value */
/* then we overwrite the '.' that's there */

   *(--tmp_dpi_ptr) = '\0';

   return(rc);

}/* end of keyToDpi */

