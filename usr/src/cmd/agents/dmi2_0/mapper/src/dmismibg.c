/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmismibg.c	1.1 96/08/05 Sun Microsystems"

/*********************************************************************/
/* SNMP DPI to DMI Connection - MIB (contains Component ID Groups)   */
/* April 12, 1994 - Version 0.0                                      */
/* Copyright    - (C) International Business Machines Corp. 1994     */
/*                                                                   */
/*********************************************************************/

#include "dmisa.h"     /* global header file for DMI Sub-agent        */
int do_setup(void);

#define DOT "."

/**********************************************************************/
/* Function: dpiGetCompMib()  - Get the specified dmiComp MIB object  */
/*                                                                    */
/* Input parameters:                                                  */
/*   OID prefix                                                       */
/*   OID instance                                                     */
/*                                                                    */
/* Output:                                                            */
/*                                                                    */
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
/* 1) Confirm that dmiCompTable is present in the OID and that it     */
/*    equals 1.  Otherwise, return noError/noSuchObject.              */
/* 2) Confirm that dmiCompEntry is present in the OID and that it     */
/*    equals 1.  Otherwise, return noError/noSuchObject.              */
/* 3) Confirm that the DmiCompEntry is present in the OID and that    */
/*    1 <= DmiCompEntry <= 9.  Otherwise, return noError/noSuchObject.*/
/* 4) If any sub-id is specified beyond dmiComponentId, return        */
/*    noError/noSuchInstance.                                         */
/* 5) If attribute ID is 7, issue ListComponent for the specified     */
/*    component ID.                                                   */
/*    If no error, return component ID as the object value.           */
/*    if error, when status code is:                                  */
/*        (0x0102) Component instrumentation not found,               */
/*                  return noError/noSuchInstance.                    */
/*        (else)   Return genErr.                                     */
/* 6) If attribute ID is 8, issue ListComponent for the specified     */
/*    component ID.                                                   */
/*    If no error, extract ComponentName and return as object value.  */
/*    if error, when status code is:                                  */
/*        (0x0102) Component instrumentation not found,               */
/*                  return noError/noSuchInstance,                    */
/*        (else)   Return genErr.                                     */
/* 7) If attribute ID is 9, issue ListComponentDesc for the specified */
/*    component ID.  If no error, extract first 256 bytes             */
/*    of the component description and return as the object value.    */
/*    if error, when status code is:                                  */
/*        (0x0109) No description,                                    */
/*                  return noError/noSuchInstance,                    */
/*        (else)   Return genErr.                                     */
/* 8) Otherwise, (for attributes 1-6) issue GetAttribute using        */
/*    DpiGet, step 6.                                                 */
/**********************************************************************/
int dpiGetCompMib(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_get_packet *pack_p)
{
int i, rc=0, error=GET_COMP_MIB_getOk;
int dmistat;
ULONG dummy=0, componentId=0, groupId, attributeId=0L;
char               *temp;

DMI_GroupKeyData_t *keyblock = NULL;
void               *pvalue;
ULONG               type, length, keycount = 0UL, keyblocklen = 0UL;

DmiListComponentsOUT   *listcomp=NULL;
DmiGetAttributeOUT  *getattrout=NULL;     /* pointer to request buffer   */
DmiDataUnion_t       *value_ptr;
DmiString_t             *work;          /* pointer to component name   */
char                   *pcompdesc=NULL;      /* pointer to conf description buf*/
 /*------- Code Starts Here -------------------------------------------*/
if (pack_p->instance_p == DMISA_NULL ) {
     temp  = 0;                       /* set it to 0 so that we return noSuchObject*/
    }
   else {
      temp = strtok(pack_p->instance_p,DOT);   /* get first sub-id    */
} /* endif */
 /* parse the oid instance parameter to determine what is being requested*/

  for ( i=1;temp;++i)                 /* parse until nothing left     */
                                            /* i keeps track of the sub-id*/
                                            /* currently being processed*/
  {
  switch(i)           /* select action based on current sub-id        */
    {
    case 1:           /* dmiCompTable sub-id                          */
      if (*temp != '1')                  /* error if not 1            */
        {
        error=GET_COMP_MIB_noSuchObject;/* set error code             */
        }
      break;
    case 2:           /* dmiCompEntry sub-id                          */
      if (*temp != '1')                  /* error if not 1            */
        {
        error=GET_COMP_MIB_noSuchObject;/* set error code             */
        }
      break;
    case 3:           /* DmiCompEntry sub-id                          */
      if (*temp<1 || *temp>12)      /* error if not 1..9           */
        {
        error=GET_COMP_MIB_noSuchObject;/* set error code             */
        }
      else
        {
     attributeId = strtoul(temp, (char**)NULL, 10);
        }
      break;
    case 4:           /* dmiComponentID sub-id                        */
      componentId= strtoul(temp, (char**)NULL, 10);        /* save component ID*/
      break;
    default:          /* sub-id specified beyond dmiComponentID       */
      error=GET_COMP_MIB_noSuchInstance;/* set error code             */
      break;
    }                 /* end of switch statement                      */
  if (!error)                          /* no errors yet?              */
    {
    temp=strtok(0, DOT);               /* get next sub-id             */
    }
  else                                 /* error detected              */
    {
    temp=0;                            /* stop loop                   */
    }
  }                                    /* end of for loop             */

if (i <= 4)                               /* not enough  sub-id's found?*/
  {
  error=GET_COMP_MIB_noSuchObject;     /* set error code              */
  }

 /* Get the information requested                                     */
if (error==GET_COMP_MIB_getOk) {   /* check if we got an error with the instance*/
 switch(attributeId)   /* select action based on attribute ID         */
  {
  case 0:             /* do nothing                                   */
    break;
  case 1:             /* manufacturer                                 */
  case 2:             /* product                                      */
  case 3:             /* version                                      */
  case 4:             /* serial number                                */
  case 5:             /* installation                                 */
  case 6:             /* verify                                       */
  case 7:
  case 8:
  case 9:
  case 10:
  case 11:
  case 12:
    groupId = 1;   /*   Group ID is always 1                          */
    rc = issueGetAttribute(
          &getattrout,           /* pointer to request buffer          */
          componentId,          /* component ID                       */
          groupId,                /* group ID                         */
          attributeId,          /* attribute ID                       */
          keycount,
          keyblocklen,
          keyblock);

    switch(rc)
      {

      case ISSUE_GET_ATTRIBUTE_noError:         /* no error    */
             
             if (!getattrout) {
               error = GET_COMP_MIB_noSuchInstance;
               break;
             }
             value_ptr = getattrout->value;
             rc = xlateType(&type,value_ptr->type,TODPI, &length);  /* get the type*/
             if (rc != XLATE_TYPE_noError) {
                 rc = GET_ATTRIBUTE_genErr;           /* we got a xlate error*/
                 break;        /* we are out of here */
             }
             if (length != 0) {  /* attribute is a string             */
                 pvalue = (void *) &(value_ptr->DmiDataUnion_u.integer);
             } else {  /* treat as normal DMI string */
                 work = value_ptr->DmiDataUnion_u.str;
                 length = work->body.body_len;
                 pvalue = work->body.body_val;
             } /* end if */
            *ppvarbind = mkDPIset(     /* Make DPI set packet         */
                *ppvarbind,            /* ptr to varBind chain        */
                pack_p->group_p,       /* ptr to subtree              */
                pack_p->instance_p,    /* ptr to rest of OID          */
                (unsigned char)type,   /* value type                  */
                (int)length,           /* length of value             */
                pvalue);               /* ptr to value                */
            break;
    case ISSUE_GET_ATTRIBUTE_failed:
           error=GET_COMP_MIB_noSuchInstance;/* set error code        */
           break;
    default:        /* some other status code was returned            */
           error=GET_COMP_MIB_genErr;     /* set error code to genErr */
           break;
     }               /* end of switch statement on Get Attribute status*/
     if (getattrout) {
        free_getattrout(getattrout);
     }

     break;

#if 0
  case 7:             /* component ID                                 */
         rc=issueListComp(&listcomp,        /* request buffer         */
                dummy,            /*                                  */
                componentId,      /* component ID                     */
                DMISA_THIS,       /* this                             */
                1);

         switch(rc)
         {
         case ISSUE_LIST_COMP_noError:         /* no error     */
                   if (!listcomp) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }
                   if (!listcomp->reply) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }
                   if (!listcomp->reply->list.list_val) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }
                   *ppvarbind = mkDPIset(         /* Make DPI set packet*/
                   *ppvarbind,         /* ptr to varBind chain        */
                   pack_p->group_p,    /* ptr to subtree              */
                   pack_p->instance_p, /* ptr to rest of OID          */
                   SNMP_TYPE_Integer32,/* value type                  */
                   (DMI_UNSIGNED)sizeof(DmiId_t), /* length of value*/
                 &(listcomp->reply->list.list_val->id));/* ptr to value          */
                   break;


         case ISSUE_LIST_COMP_failed:   /*the rpc call failed/returned error */
                    error=GET_COMP_MIB_noSuchInstance;/* set error code*/
                    break;


         case ISSUE_LIST_COMP_outOfMemory:
         default:        /* some other status code was returned          */
                    error=GET_COMP_MIB_genErr;     /* set error code to genErr*/
                    break;
          }                                /* end of switch on List Comp Status*/
      if (listcomp) {
         free_listcompout(listcomp);
      }

    break;


  case 8:             /* component name                               */
         rc=issueListComp(&listcomp,        /* request buffer         */
                                 dummy,            /*                 */
                                 componentId,      /* component ID    */
                                 DMISA_THIS,       /* this component  */
                                 1);

         switch(rc)
         {

         case ISSUE_LIST_COMP_noError:         /* no error     */
                   if (!listcomp) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }
                   if (!listcomp->reply) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }
                   if (!listcomp->reply->list.list_val) {
                       error=GET_COMP_MIB_genErr;
                       break;
                   }

                 work= listcomp->reply->list.list_val->name;
                 *ppvarbind = mkDPIset(         /* Make DPI set packet*/
                             *ppvarbind,         /* ptr to varBind chain*/
                             pack_p->group_p,    /* ptr to subtree    */
                             pack_p->instance_p, /* ptr to rest of OID*/
                             SNMP_TYPE_DisplayString, /* value type   */
                            (DMI_UNSIGNED)(work->body.body_len),/* length of value*/
                             (work->body.body_val));     /* ptr to value      */
                  break;

         case ISSUE_LIST_COMP_failed:   /*the rpc call failed/returned error */
                 error=GET_COMP_MIB_noSuchInstance;/* set error code  */
                 break;

        case ISSUE_LIST_COMP_outOfMemory:
        default:        /* some other status code was returned        */
                 error=GET_COMP_MIB_genErr;     /* set error code to genErr*/
                 break;
        }                                /* end of switch  on status from List Comp*/
        if (listcomp) {
              free_listcompout(listcomp);
         }

        break;


  case 9:                              /* description                 */
         rc=issueListDesc(pcompdesc, /* request buffer               */
                               componentId,      /* component ID      */
                               groupId,             /* group id       */
                               attributeId,         /* attribute id   */
                               DMISA_COMPDESC); /* x0203               */


         switch(rc)
         {
         case ISSUE_LIST_DESCRIPTION_noError:         /* no error   */
                                    /* set pointer to confirm buffer  */
                *ppvarbind = mkDPIset(         /* Make DPI set packet */
                                  *ppvarbind,         /* ptr to varBind chain*/
                                   pack_p->group_p,    /* ptr to subtree*/
                                   pack_p->instance_p, /* ptr to rest of OID*/
                                   SNMP_TYPE_DisplayString, /* value type*/
                             (strlen(pcompdesc) > 256) ? 256 : 
                                        strlen(pcompdesc)     ,/* length of value*/
                                   pcompdesc);     /* ptr to value*/
         break;
         case ISSUE_LIST_DESCRIPTION_failed:
                error=GET_COMP_MIB_noSuchInstance;/* set error code   */
                break;
         case ISSUE_LIST_DESCRIPTION_outOfMemory:
         default:        /* some other status code was returned       */
                error=GET_COMP_MIB_genErr;     /* set error code to genErr*/
                break;
         }                                /* end of switch on status from List Desc*/
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
  default:            /* should not be possible to get here, but ...  */
         error=GET_COMP_MIB_noSuchObject;   /* set error code         */
         break;
  }
} /* endif */
return(error);                         /* return error code           */
}


/**********************************************************************/
/* Function: dpiSetCompMib()  - Set the specified dmiComp MIB object  */
/*                                                                    */
/* Input parameters:                                                  */
/*   OID prefix                                                       */
/*   OID instance                                                     */
/*                                                                    */
/* Output:                                                            */
/*    Return  error status - no CompMIBs are writeable                */
/* Description:                                                       */
/***********************************************************************/
int dpiSetCompMib(snmp_dpi_set_packet **ppvarbind,
           snmp_dpi_set_packet *ppack)
{
    return(SET_COMP_MIB_notWritable);
}

