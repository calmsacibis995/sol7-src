/* Copyright 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmismibn.c	1.7 96/10/02 Sun Microsystems"

/*********************************************************************/
/* SNMP DPI to DMI Connection - MIB (contains Component ID Groups)   */
/* April 12, 1994 - Version 0.0                                      */
/* Copyright    - (C) International Business Machines Corp. 1994     */
/*                                                                   */
/*********************************************************************/
#include "dmisa.h"     /* global header file for DMI Sub-agent        */

#ifdef OS2
#elif defined WIN32
#define ultoa(p1,p2,p3) _ultoa(p1,p2,p3)
#define reverse(p1) _strrev(p1)
#else
void ultoa (ULONG n, char s[], ULONG n2);
void reverse (char s[]);
#endif

extern char logbuffer[];
#ifdef DMISA_TRACE
extern unsigned char logbuffer[];
void TraceReqBuf(DMI_MgmtCommand_t *mgmtreq);
#endif

int do_setup(void);

#define DOT "."

/**********************************************************************/
/* Function: dpiGetNextCompMib() - Get the next object in dmiComp MIB */
/*                                                                    */
/*  Input parameters:                                                 */
/*    OID prefix                                                      */
/*    OID instance                                                    */
/* Description:                                                       */
/* o Entered if OID prefix is that of the dmiComp MIB.                */
/*            (i.e., 1.3.6.1.4.1.2.5.11.1.1, where                    */
/*                              .2 = ibm                              */
/*                                .5 = ibmArchitectur                 */
/*                                  .11 = ibmDMI                      */
/*                                     .1 = ibmDMIMIB                 */
/*                                       .1 = dmiComp)                */
/*   Following the prefix, the MIB contains:                          */
/*                              .1.1.col.index, where                 */
/*                              .1 = dmiCompTable                     */
/*                                .1 = dmiCompEntry                   */
/*                                  .col = DmiCompEntry               */
/*                                      .index = dmiComponentID       */
/*                                                                    */
/*       Step 2 finds the next leaf and returns its value.            */
/* Note: Where a component exists but the attribute is not found,     */
/*       or illegal to get, return the NULL string.                   */
/* Note: The dmiComp MIB is individually registered with DPI          */
/*       under its own prefix with DPI.                               */
/* Note: When sub-ID's are set to zero, the GetNext process will      */
/*       result in the first instance of that sub-ID.                 */
/*                                                                    */
/* 1. COMPLETE OID SPECIFICATION (to a "leaf"):                       */
/*    a) If the value of dmiCompTable in the OID:                     */
/*        - is greater than 1, return the OID as endOfMibView.        */
/*        - is not present or equals zero, set it to 1 and            */
/*          set dmiCompEntry to zero.                                 */
/*       Otherwise, continue.                                         */
/*    b) If the value of dmiCompEntry in the OID:                     */
/*        - is greater than 1, return the OID as endOfMibView.        */
/*        - is not present or equals zero, set it to 1 and            */
/*          set DmiCompEntry to zero.                                 */
/*       Otherwise, continue.                                         */
/*    c) If the value of DmiCompEntry in the OID:                     */
/*        - is greater than 9, return the OID as endOfMibView.        */
/*        - is not present or equals zero, set it to 1 and            */
/*          set dmiComponentID to zero.                               */
/*       Otherwise, continue.                                         */
/*    d) If the value of dmiComponentID in the OID:                   */
/*        - is not present, set it to zero.                           */
/*                                                                    */
/* 2. FIND NEXT LEAF & RETURN VALUE:                                  */
/*    If DmiCompEntry in the OID:                                     */
/*    a) is less than or equal to six, issue ListNextComponent        */
/*       - if found, issue GetAttribute for attribute DmiCompEntry    */
/*         in group 1 of the component just found.                    */
/*         (Assumption: Service Layer guarantees group 1 of each comp-*/
/*         onent instantiation is a ComponentID group as specified.)  */
/*         - if found, return the object value and its OID to agent.  */
/*         - if not found and                                         */
/*              ( iStatus = (0x0100) Attribute not found or           */
/*                iStatus = (0x010d) Attribute not supported )        */
/*           return NULL string and the attribute OID to the agent.   */
/*         - otherwise, return genErr                                 */
/*       - if not found and                                           */
/*         iStatus = (0x0102) Component instrumentation not found     */
/*         (meaning all components in the S.L.database have been read)*/
/*         - increment DmiCompEntry and set dmiComponentID to zero,   */
/*         - if DmiCompEntry > 6, continue with step (2b).            */
/*         - start at step (2a) again.                                */
/*       - otherwise, return genErr.                                  */
/*    b) is 7, then issue ListNextComponent.                          */
/*       - if found, return component ID as the object value.         */
/*       - if not found & status is:                                  */
/*         (0x0102) Component instrumentation not found,              */
/*         increment DmiCompEntry and set dmiComponentID to zero,     */
/*         and continue with step (2c).                               */
/*       - otherwise, return genErr.                                  */
/*    c) is 8, then issue ListNextComponent.                          */
/*       - if found, extract ComponentName and return as object value */
/*       - if not found & status is:                                  */
/*         (0x0102) Component instrumentation not found,              */
/*         increment DmiCompEntry and set dmiComponentID to zero,     */
/*         and continue with step (2d).                               */
/*       - otherwise, return genErr.                                  */
/*    d) is 9, then issue ListNextComponent.                          */
/*       - if found, issue ListComponentDesc for the specified        */
/*         component ID.  If no error, extract the first 256 bytes    */
/*         of the component description and return as object value.   */
/*       - if not found & status is:                                  */
/*         - (0x0102) Component instrumentation not found,            */
/*           increment DmiCompEntry and continue with step (2e).      */
/*         - (0x0109) No description                                  */
/*           return the OID as NULL string to the agent.              */
/*       - otherwise, return genErr.                                  */
/*    e) is greater than 9, then return the OID as endOfMibView.      */
/*                                                                    */
/* Note: DMI error handling within this routine may be inadequate.    */
/**********************************************************************/
int dpiGetNextCompMib(snmp_dpi_set_packet **ppvarbind,
                      snmp_dpi_next_packet *pack_p)
{
char         *temp=0, *instance, componentName[256];
DMI_UNSIGNED  componentNameLength;
int           dmistat;
int           rc=0, error=GET_NEXT_COMP_MIB_noError, i, j;
ULONG         dmi[5]={0L,0L,0L,0L,0L}, maxvalue[5]={0L,1L,1L,12L,0L};
ULONG         dummy=0, componentId=0, groupId=0, attributeId=0;

void         *pvalue;
ULONG         type, length, keycount = 0UL, keyblocklen = 0UL;

DMI_GroupKeyData_t     *keyblock = NULL;
DmiListComponentsOUT   *listcomp=NULL;      /* pointer to request buffer   */
DmiGetAttributeOUT     *getattrout=NULL;
DmiDataUnion_t         *value_ptr;
DmiString_t            *work;            /* pointer to component name*/
char                   *pcompdesc=NULL;  /* pointer to conf description buf*/
/***********************************************************************/
/*                                                                                                        */
/*       -------------     dmi[5]                   maxvalue[5]                                      */
/*      ....not used..          [0]                        0                                          */
/*      dmiCompTable         [1]                       1                                            */
/*      dmiCompEntry         [2]                       1                                                */
/*      DmiCompEntry         [3]                      12   <---  AttributeID (1-6)             */
/*      dmiCompentID         [4]                      x   <--- ComponentID                  */
/*********************************************************************/

 /*------- Code Starts Here -------------------------------------------*/

/* parse the oid instance parameter to determine what is being requested*/
if (pack_p->instance_p != DMISA_NULL) {
  temp=strtok(pack_p->instance_p,DOT);   /* get first sub-id          */
} /* endif */
for (i=1;temp;++i)                     /* parse until nothing left    */
                                       /* i keeps track of the sub-id */
                                       /* currently being processed   */
  {
  if (i<=4)                            /* still processing valid sub-ids?*/
    {
    dmi[i]=strtoul(temp,&temp,10);     /* store value                 */
    }
  if (!error)                          /* no errors yet?              */
    {
    temp=strtok(0, DOT);               /* get next sub-id             */
    }
  else                                 /* error detected              */
    {
    temp=0;                            /* stop loop                   */
    }
  }                                    /* end of for loop             */

/* Loop through the stored sub-ids to validate them and to create the */
/* final instance string.The dmiComponentID & DmiCompEntry could change.*/
j=0;                                   /* place holder in local instance*/
instance=(char *)calloc(1, 16);           /* get space for local instance*/
if (!instance) return GET_NEXT_COMP_MIB_outOfMemory;

for (i=1; i<=4; ++i)
  {
  switch(i)           /* select action based on current sub-id        */
    {
    case 1:           /* dmiCompTable sub-id                          */
    case 2:           /* dmiCompEntry sub-id                          */
    case 3:           /* DmiCompEntry sub-id                          */
      if (dmi[i]>maxvalue[i]) /*too big?                              */
        {
        error=DMISA_ERROR_endOfMibView;   /* set error code           */
        }
      else
        if (dmi[i]==0) /* not specified or 0                          */
          {
          instance[j++]='1';   /* use value of 1                      */
          dmi[i+1]=0;          /* set next sub-id (dmiCompEntry,      */
                               /* DmiCompEntry, dmiComponentID) to 0  */
          }
        else          /* must be 1                                    */
          {
          ultoa(dmi[i],&(instance[j]),10); /* move value            */
          if (dmi[i] > 9) 
             j+= 2;
          else
             j++;
          }
      instance[j++]=*DOT;                    /* move in dot           */
      break;
    case 4:           /* dmiComponentID sub-id                        */
      ultoa(dmi[i],&(instance[j]),10); /* move value.  If value was   */
                                         /* not specified, it will be 0*/
      /* no j++ here? $MED */
      break;
    }                 /* end of switch statement                      */
  }

 /* Get the information requested                                     */
 while (!componentId && !error)
 {
    /* find the next component ID                                     */
    rc=issueListComp(&listcomp, /* request buffer                     */
             dummy,             /*                                    */
             dmi[4],            /* component ID                       */
             DMISA_NEXT,        /* next component                     */
             1);
    /* check results from issueListComp function                      */

    switch(rc)
    {
       case ISSUE_LIST_COMP_noError:          /* no error  */
            if (!listcomp) {
                 error = GET_NEXT_COMP_MIB_genErr;
                 break;
            }
            if (!listcomp->reply) {
                 error=GET_NEXT_COMP_MIB_genErr;
                 break;
            }
            if (!listcomp->reply->list.list_val) {
                 error=GET_NEXT_COMP_MIB_genErr;
                 break;
            }

         componentId=listcomp->reply->list.list_val->id; /* save component ID    */
         /* set pointer to location of component name information*/
           work= listcomp->reply->list.list_val->name;
           componentNameLength=(DMI_UNSIGNED)work->body.body_len;
           memcpy(componentName,
            work->body.body_val,
            componentNameLength);
           break;
       case ISSUE_LIST_COMP_failed:  /* component instrumentation not found*/
           ++dmi[3];       /* increment to the next DmiCompEntry      */
           if (dmi[3]-1 > 9) {
           ultoa (dmi[3],&instance[j-3],10);  /* change the instance we will be sending back*/
           instance[j-1] = *DOT;                 /* put back the dot  */
           }
           else
           {
            if (dmi[3]-1 == 9) {
           ultoa (dmi[3],&instance[j-2],10);  /* change the instance we will be sending back*/
           instance[j++] = *DOT;                 /* put back the dot  */
           ultoa (dmi[4],&instance[j],10);  /* change the instance we will be sending back*/
            }
            else {
           ultoa (dmi[3],&instance[j-2],10);  /* change the instance we will be sending back*/
           instance[j-1] = *DOT;                 /* put back the dot  */
            }
           }
           dmi[4]=0;       /* start back to the first component ID    */
           if (dmi[3] > 12 ) {
              error = DMISA_ERROR_endOfMibView;   /* Time to get out of this loop*/
           } /* endif */
           break;
       default:         /* some other status code was returned        */
           error=GET_NEXT_COMP_MIB_genErr;      /* set error code to genErr*/
           break;
    }                                 /* end of switch on List Comp Status*/
    if (listcomp) {
        free_listcompout(listcomp);
    }

 }                  /* end of while loop for component ID             */

 if (!error)                           /* if no error found           */
   {                                   /* perform the requested action*/
   ultoa (componentId, &instance[j],10); /* make sure we send back the correct compID*/
   switch(dmi[3])     /* select action based on DmiCompEntry          */
     {
     case 0:          /* not specified                                */
            dmi[3] = 1;  /* set the DmiCompEntry to 1, which is used as the attri ID*/
 /*         break;  //   note this line od code  commented out        */
     case 1:          /* manufacturer                                 */
     case 2:          /* product                                      */
     case 3:          /* version                                      */
     case 4:          /* serial number                                */
     case 5:          /* installation                                 */
     case 6:          /* verify                                       */
     case 7:
     case 8:
     case 9:
     case 10:
     case 11:
     case 12:
            groupId = 1;                      /* Group ID is always 1 */
            rc = issueGetAttribute(
                   &getattrout,           /* pointer to request buffer*/
                   componentId,                /* component ID        */
                   groupId,                    /* group ID            */
                   dmi[3],                     /* attribute ID        */
                   keycount,
                   keyblocklen,
                   keyblock);

            switch(rc)
            {
            case ISSUE_GET_ATTRIBUTE_noError:        /* no error  */
                   if (!getattrout) {
                       error = GET_NEXT_COMP_MIB_genErr;
                       break;
                   }
                   value_ptr = getattrout->value;
                   rc = xlateType(&type, value_ptr->type, TODPI, &length);
                                        /* get the type               */
                   if (rc != XLATE_TYPE_noError) {
                         error = GET_NEXT_COMP_MIB_genErr;  /* we got a xlate error*/
                   } else  {
                         if (length != 0) {            /* attribute is a string*/
                          pvalue=(void *) &(value_ptr->DmiDataUnion_u.integer);
                         } else   {                     /* treat as normal DMI string*/
                             work = value_ptr->DmiDataUnion_u.str;
                             length = work->body.body_len;
                             pvalue = work->body.body_val;
                         }                         /* end if          */
                        *ppvarbind = mkDPIset(      /* Make DPI set packet*/
                             *ppvarbind,          /* ptr to varBind chain*/
                             pack_p->group_p,     /* ptr to subtree   */
                             instance,            /* ptr to rest of OID*/
                             (unsigned char)type, /* value type       */
                             (int)length,         /* length of value  */
                             pvalue);             /* ptr to value     */
                   }
                   break;
             case ISSUE_GET_ATTRIBUTE_failed:
#if 0
                   *ppvarbind = mkDPIset(        /* Make DPI set packet*/
                   *ppvarbind,            /* ptr to varBind chain     */
                   pack_p->group_p,       /* ptr to subtree           */
                   instance,              /* ptr to rest of OID       */
                   SNMP_TYPE_DisplayString,/* value type              */
                   0,                     /* length of value=0        */
                   NULL);                 /* NULL pointer             */
#endif
                   error = GET_NEXT_COMP_MIB_genErr;
                   break;
            default:       /* some other status code was returned     */
                   error=GET_NEXT_COMP_MIB_genErr;    /* set error code to genErr*/
                   break;
            }              /* end of switch statement on Get Attribute  Status*/
            if (getattrout) {
               free_getattrout(getattrout);
            }

            break;
#if 0
     case 7:          /* component ID                                 */
                         /* set pointer to confirm buffer             */
             *ppvarbind = mkDPIset(            /* Make DPI set packet */
                    *ppvarbind,                     /* ptr to varBind chain*/
                    pack_p->group_p,           /* ptr to subtree      */
                    instance,                        /* ptr to rest of OID*/
                    SNMP_TYPE_Integer32,      /* value type           */
                   (DMI_UNSIGNED)sizeof(componentId), /* length of value*/
                    &(componentId));              /* ptr to value     */
             break;
     case 8:          /* component name                               */
             *ppvarbind = mkDPIset(          /* Make DPI set packet   */
                    *ppvarbind,                   /* ptr to varBind chain*/
                    pack_p->group_p,            /* ptr to subtree     */
                    instance,                        /* ptr to rest of OID*/
                    SNMP_TYPE_DisplayString, /* value type            */
                    componentNameLength,    /* length of value        */
                    &componentName);          /* ptr to value         */
            break;
     case 9:          /* description                                  */
            rc=issueListDesc(pcompdesc,    /* request buffer         */
                          componentId,     /* component ID            */
                          groupId,         /* group id                */
                          attributeId,     /* attribute id            */
                          DMISA_COMPDESC);


            switch(rc)
            {
            case ISSUE_LIST_DESCRIPTION_noError:     /* no error */
                                           /* set pointer to confirm buffer*/

                    *ppvarbind = mkDPIset(      /* Make DPI set packet*/
                    *ppvarbind,          /* ptr to varBind chain      */
                    pack_p->group_p,     /* ptr to subtree            */
                    instance,            /* ptr to rest of OID        */
                    SNMP_TYPE_DisplayString, /* value type            */
                    (strlen(pcompdesc) > 256) ? 256 :
                         strlen(pcompdesc)     ,/* length of value*/

                    pcompdesc);      /* ptr to value              */
                    break;


         case ISSUE_LIST_DESCRIPTION_failed:
                error=GET_NEXT_COMP_MIB_genErr;/* set error code   */
                break;

         case ISSUE_LIST_DESCRIPTION_outOfMemory:
            default:     /* some other status code was returned       */
                  error=GET_NEXT_COMP_MIB_genErr;  /* set error code to genErr*/
                  break;
            }                             /* end of switch on List Desc status*/
            if (pcompdesc) {
               free(pcompdesc);
            }
            break;
/*the following attribute types are dummy attributes meant for trap gen. */
/*they  have unknown value in the database */
 case 10:                       /*group id */
 case 11:                       /*group name */
 case 12:                       /*language details */
                *ppvarbind = mkDPIset(         /* Make DPI set packet */
                                  *ppvarbind,         /* ptr to varBind chain*/
                                   pack_p->group_p,    /* ptr to subtree*/
                                   pack_p->instance_p, /* ptr to rest of OID*/
                                   SNMP_TYPE_NULL, /* value type*/
                                   0,
                                   NULL);     /* ptr to value*/
                               break;
#endif
       default:         /* At the end of the component list and no more*/
                       /* attributes to display                       */
       error=DMISA_ERROR_endOfMibView; /* set error code              */
       break;
     }                                 /* end of switch statement     */
   }                                   /* end of if no error          */
free(instance);                        /* free dynamic storage        */
return(error);                         /* return error code           */
}

#ifdef OS2
#elif defined WIN32
#else
/* local routine to convert number to ascii since aix doesn't have */
void ultoa (ULONG n, char s[], ULONG n2)
{
  int i = 0;
  do {      /* generate digits in reverse order */
    s[i++] = n % 10 + '0';  /* get next digit   */
  } while ((n /= 10) > 0 ); /* delete it        */
  s[i] = '\0';
  reverse(s);
} /* end ultoa */
void reverse (char s[]) /* reverse string in place */
{
  int c, i, j;
  for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
    c = s[i];
    s[i] = s[j];
    s[j] = c;
  } /* end for */
} /* end reverse */
#endif

