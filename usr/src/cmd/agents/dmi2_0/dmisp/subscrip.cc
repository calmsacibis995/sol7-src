// Copyright 10/16/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)subscription.cc	1.12 96/10/16 Sun Microsystems"

#include <subscription.hh>

char *filter_class = "DMTF|SPFilterInformation|001";
char *subs_class   = "DMTF|SP Indication Subscription|001";
cond_t subtbl_mod;
extern mutex_t  component_mutex_lock;
extern int      errno;

Table *filter_table, *subscription_table;
Component *compobj;

Exp_time::Exp_time(unsigned long s):RWTime(s) {
}


Exp_time::Exp_time(DmiTimestamp_t  tm) {
struct tm  tme;
char       buf[10];
unsigned long utcoff;

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.seconds, 2);
             tme.tm_sec = atoi(buf);

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.minutes, 2);
             tme.tm_min = atoi(buf);

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.hour, 2);
             tme.tm_hour = atoi(buf);

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.day, 2);
             tme.tm_mday = atoi(buf);

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.month, 2);
             tme.tm_mon = atoi(buf)-1;

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.year, 4);
             tme.tm_year = atoi(buf)-1900;

             memset(buf, 0, sizeof(buf));
             memcpy(buf, tm.utcOffset, 3);
             utcoff = atol(buf);

             tme.tm_wday = 0;
             tme.tm_yday = 0;
             tme.tm_isdst = 0;

             RWTime::RWTime(&tme, RWZone::utc()); 
 
             if (tm.plusOrMinus == '-')  
                  *this += (utcoff * 60);  /*Utc of is in minutes */
             else
                  *this -= (utcoff * 60);
}

Exp_time Exp_time::operator=(time_t s) {
struct tm *tme;

             tme = localtime(&s);   
             RWTime::RWTime( tme, RWZone::local());
             return *this;
}

RWBoolean Exp_time::operator<(time_t s) {
struct tm *tme;
RWTime    *temp;

             tme = localtime(&s);   
             temp = new RWTime( tme, RWZone::local());
             if (*this < *temp) {
               delete temp;
               return 1;
             }
             else {
               delete temp;
               return 0;
             }
}

void event_subscriber_init(void) {
DmiString_t  classname;
thread_t    thread_id;

/*Get the references to Filter Table/ Subscription Table */
     classname.body.body_len = strlen(filter_class);
     classname.body.body_val = filter_class;
      
     compobj = getComponent(SPCOMPID);
     if (!compobj) {
       printf("SPIndication:Unable to locate the Component Object of id=1\n"); 
       exit(1);
     }

     filter_table = getTableOnClassName(compobj , &classname);  

     if (!filter_table) {
       printf("SPIndication:Unable to locate the Filter Table\n"); 
       exit(1);
     }

     classname.body.body_len = strlen(subs_class);
     classname.body.body_val = subs_class;
      
     subscription_table = getTableOnClassName(compobj , &classname);  

     if (!subscription_table) {
       printf("SPIndication:Unable to locate the Subscription Table\n"); 
       exit(1);
     }
     
     if (cond_init( &subtbl_mod, USYNC_THREAD, NULL) ) {
       printf("SPIndication:Unable to cond_init errno = %d\n",errno);
       exit(1);
     }
/*Set up a house keeping thread - Detached / System Scope/ Sched OTHER */ 

    if (thr_create(NULL, NULL, (void *(*)(void *)) subscription_housekeep ,
                              NULL, THR_DETACHED | THR_NEW_LWP, &thread_id)) {
       printf("SPIndication:Error Creating a thread:HouseKeeping thread\n");
       exit(1);
    }

}

int  send_to_micallback(DmiString_t *receiver, int threshold, int event_type, void *argp) {
DmiErrorStatus_t *error; 
CLIENT           *clnt;
int               i;
char             *ptr;
char             *hostname=NULL;
unsigned long    prognum;
unsigned long    versnum;
char            *rec;

 if ((!receiver) || (!receiver->body.body_val) || (threshold==0) ||
     (receiver->body.body_len == 0))
       return 1;
 rec = (char *)calloc(1,receiver->body.body_len+1);
 memcpy(rec, receiver->body.body_val, receiver->body.body_len);
 ptr = strtok(rec, ":"); 
 if (ptr)
   hostname = strdup(ptr);
 if (!hostname)
     return 0; /*Hostname not found */

 for(i=0; (i < 2) && (ptr) ; i++) {
        ptr = strtok(NULL, ":");
        if (i == 0) {
          if (ptr)
           prognum = atol(ptr);
        }else {
          if (ptr)
           versnum = atol(ptr);
        }
 }
 free(rec);
 if (!ptr) {
     free(hostname);
     return 0; /*valid prognum/versnum were not found */
 }

   
 clnt = clnt_create(hostname, prognum , versnum, "netpath");
 if (!clnt) {
     free(hostname);
     return 0;        /*Unable to create RPC handle */
 }
 
 for(i= 0; i < threshold ; i++) {
    switch(event_type) {
      case COMPONENTADD:
         error =_dmicomponentadded_0x1((DmiComponentAddedIN *)argp, clnt);
         break;
      case COMPONENTDEL:
         error =_dmicomponentdeleted_0x1((DmiComponentDeletedIN *)argp, clnt);
         break;
      case GROUPADD:
         error =_dmigroupadded_0x1((DmiGroupAddedIN *)argp, clnt);
         break;
      case GROUPDEL: 
         error =_dmigroupdeleted_0x1((DmiGroupDeletedIN *)argp, clnt);
         break;
      case LANGADD:
         error =_dmilanguageadded_0x1((DmiLanguageAddedIN *)argp, clnt);
         break;
      case LANGDEL:
         error =_dmilanguagedeleted_0x1((DmiLanguageDeletedIN *)argp, clnt);
         break;
      case EVENT_OCCURENCE:
         error =_dmideliverevent_0x1((DmiDeliverEventIN *)argp, clnt);
         break;
      case SUBS_NOTICE:
         error =_dmisubscriptionnotice_0x1((DmiSubscriptionNoticeIN *)argp, clnt);
         break;
    }    

      if ((error) &&  (*error == DMIERR_NO_ERROR))
         break;
         
 }
 auth_destroy(clnt->cl_auth);
 clnt_destroy(clnt);

 free(hostname);
 if ((!error) || (*error != DMIERR_NO_ERROR)) {
      return 0; /*Failure to Xmit */
 }
return 1;   /*Success */
}

void *subscription_housekeep(void *param) {

unsigned int  num_table_entries=0;
unsigned int  count,loop;
int      errcode;
char     errbuff[100];
RWTime   *current_time;
int      threshold_val;
Exp_time warn_time, exp_time;
Exp_time last_update_time =(unsigned long) 0L;
Exp_time next_exp_time = (unsigned long)0L; 
time_t   applnstart_time;
unsigned long timecorrection;
struct timespec abs_time;
RWTPtrSlist<DmiAttributeValues_t>  *subs_rows,*filt_rows;
DmiAttributeValues_t               *subs_ent,*filt_ent;
DmiDataUnion_t                     *attr_data;
DmiSubscriptionNoticeIN            *notice=NULL;
DmiNodeAddress_t                   sender;
DmiString_t                        *receiver;
DmiString_t                        address,rpc,transport;
char                               hostname[MAXHOSTNAMELEN+1];
TableInfo                          *tableinfo;

  sysinfo(SI_HOSTNAME, hostname, MAXHOSTNAMELEN+1); /*Sender hostname */
  address.body.body_len = strlen(hostname);
  address.body.body_val = hostname;
  
  rpc.body.body_len = 3;
  rpc.body.body_val = "ONC";

  transport.body.body_len = 3; 
  transport.body.body_val = "udp";

  sender.address = &address;
  sender.rpc     = &rpc;
  sender.transport = &transport;

  time(&applnstart_time);
  current_time = new RWTime;
  timecorrection = current_time->seconds()-applnstart_time;
  delete current_time;
  
  acquire_component_lock(); /*This has to be acquired for this access */
  while(1) {
       current_time = new RWTime; /*Get the current time */

       if ((*current_time > next_exp_time) ||
           (num_table_entries !=  subscription_table->GetNumOfRows()) ||
           (last_update_time < subscription_table->GetTimestamp()))  {

          num_table_entries = subscription_table->GetNumOfRows();
          last_update_time  = subscription_table->GetTimestamp();  
          subs_rows  = subscription_table->GetRows();
          filt_rows  = filter_table->GetRows();
   
          count = subs_rows->entries();
          while(count-- > 0) {
             subs_ent = subs_rows->get();
             if (!subs_ent)  {
               printf("SPIndication:Subscription table has NULL table entry\n");
               continue;
             }
                
             attr_data = getAttrData(subs_ent, WARNING_TIME_ATTRID);
             if (!attr_data) {
               printf("SPIndication:NULL DataUnion ptr. for Warning Time AttrID\n");
               continue;
             }

            if (attr_data->type != MIF_DATE) {
               printf("SPIndication:Illegal type for Warning Time AttrID\n");
               continue;
            }

            if ((!attr_data->DmiDataUnion_u.date) ||
                (attr_data->DmiDataUnion_u.date->year[0] == 0)){
                 subs_rows->append(subs_ent);
                 continue;
            }
            warn_time = *(attr_data->DmiDataUnion_u.date);

             attr_data = getAttrData(subs_ent, EXP_TIME_ATTRID);
             if (!attr_data) {
               printf("SPIndication:NULL DataUnion ptr. for Expiry Time AttrID\n");
               continue;
             }

            if (attr_data->type != MIF_DATE) {
               printf("SPIndication:Illegal type for Expiry Time AttrID\n");
               continue;
            }

            exp_time = *(attr_data->DmiDataUnion_u.date);
            
                
             attr_data = getAttrData(subs_ent, THRESH_ATTRID);            
             if (!attr_data) {
               printf("SPIndication:NULL DataUnion ptr. for Threshold AttrID\n");
               continue;            
             }

            if (attr_data->type != MIF_INTEGER) {
               printf("SPIndication:Illegal type for Threshold AttrID\n");
               continue;     
            }

            threshold_val = attr_data->DmiDataUnion_u.integer;

            if (*current_time > exp_time) {
                   //generate_exp  message
                    notice = (DmiSubscriptionNoticeIN *)
                                  malloc(sizeof(DmiSubscriptionNoticeIN));
                    
                   if (!notice) 
                      printf("SPIndication:Failure to alloc Subscription Notice\n");
                   else {
                         attr_data = getAttrData(subs_ent, RECEIVER_ADDRESSID);
                         if (!attr_data) {
               printf("SPIndication:NULL DataUnion ptr. for Addressing AttrID\n");
                                       free(notice);
                         }else if ((attr_data->type != MIF_DISPLAYSTRING) 
                                   &&(attr_data->type != MIF_OCTETSTRING)){
                         printf("SPIndication:Illegal type of Addressing AttrID\n");
                         free(notice);
                        } else  {
                             receiver = attr_data->DmiDataUnion_u.str;

                         notice->handle = 0;
                         notice->sender = &sender;
                         notice->expired = 1;
                         notice->rowData.compId = SPCOMPID;
                         tableinfo = subscription_table->GetTableInfo();
                         notice->rowData.groupId = tableinfo->id;                                            notice->rowData.className = tableinfo->className; 
                         notice->rowData.keyList = NULL;
                         notice->rowData.values = subs_ent;

                         send_to_micallback(receiver, threshold_val ,
                                            SUBS_NOTICE, (void *)notice);
                         free(notice);
                       }
                   } 

                   //The entry is already removed . Do not append it again
                   //subs_rows->removeAt(count); //clean subscription table   


                   //clean the filter table 

                   subs_ent->list.list_len = 4;   /*Key for subscription table*/
                   loop = filt_rows->entries();
                   while(loop-- > 0) {
                         filt_ent = filt_rows->get();
                         if (IsKeyMatch(filt_ent, subs_ent) == TRUE) {
                            free_attrvalues(filt_ent);
                            //filt_rows->removeAt(loop);
                            continue;
                         }
                         filt_rows->append(filt_ent);
                   }
                   subs_ent->list.list_len = 7;

                   free_attrvalues( subs_ent ); // free the subs table buffer
                    
                   
                    // update the database - TBD
                   WriteComponentToDB(errcode, compobj, DB_UPDATE_ALL);
                   if (errcode != DMIERR_NO_ERROR) {
                    sprintf(errbuff, "Error in WriteComponentToDB =%d",errcode);
                    syslog(LOG_ERR, errbuff);
		    exit(1);
                   }
                   continue;
            }

            if ((*current_time > warn_time) &&
                (*current_time < (warn_time + SLEEPTIME))) {
                 // generate_warn message
                    notice = (DmiSubscriptionNoticeIN *)
                                  malloc(sizeof(DmiSubscriptionNoticeIN));

                   if (!notice)
                      printf("SPIndication:Failure to alloc Subscription Notice\n");
                   else {
                         attr_data = getAttrData(subs_ent, RECEIVER_ADDRESSID);
                         if (!attr_data) {
               printf("SPIndication:NULL DataUnion ptr. for Addressing AttrID\n");
                                       free(notice);
                         }else if ((attr_data->type != MIF_DISPLAYSTRING) &&
                                   (attr_data->type != MIF_OCTETSTRING)) {
                         printf("SPIndication:Illegal type of Addressing AttrID\n");
                         free(notice);
                        } else  {
                             receiver = attr_data->DmiDataUnion_u.str;

                         notice->handle = 0;
                         notice->sender = &sender;
                         notice->expired = 0;
                         notice->rowData.compId = SPCOMPID;
                         tableinfo = subscription_table->GetTableInfo();
                         notice->rowData.groupId = tableinfo->id;
                         notice->rowData.className = tableinfo->className;
                         notice->rowData.keyList = NULL;
                         notice->rowData.values = subs_ent;

                         if (!send_to_micallback(receiver, threshold_val ,
                                            SUBS_NOTICE, (void *)notice)) {
                         //  subs_rows->removeAt(count); //clean subscription table


                           //clean the filter table

                            subs_ent->list.list_len = 4; 
                                            /*Key for subscription table*/
                            loop = filt_rows->entries();
                            while(loop-- > 0) {
                                filt_ent = filt_rows->get();
                                if (IsKeyMatch(filt_ent, subs_ent) == TRUE) {
                                     free_attrvalues(filt_ent);
                                     //filt_rows->removeAt(loop);
                                     continue;
                                }
                                filt_rows->append(filt_ent);
                            }
                            subs_ent->list.list_len = 7;

                            free_attrvalues( subs_ent ); // free the subs table buffer
                             // update the database - TBD
                            WriteComponentToDB(errcode, compobj, DB_UPDATE_ALL);
                            if (errcode != DMIERR_NO_ERROR) {
                                sprintf(errbuff, "Error in WriteComponentToDB =%d",errcode);
                                syslog(LOG_ERR, errbuff);
                                exit(1);
                            }
                            free(notice);
                            continue; 

                         }
                         free(notice);
                       }
                   } /*Valid notice buffer */

                 } /*Issue Warning Notice */
                 subs_rows->append(subs_ent);     
            } /*loop through the Subscription Table */

            if ((*current_time < warn_time) &&
                ((next_exp_time == (unsigned long) 0L) ||
                 (next_exp_time > warn_time))) 
                      next_exp_time = warn_time;


            if ((*current_time < exp_time) && 
                ((next_exp_time == (unsigned long) 0L) ||
                 (next_exp_time > exp_time)))
                      next_exp_time = exp_time;
            

          }  /*End of for loop-subs table scan */

        if ((next_exp_time.seconds() == 0) ||
           ((next_exp_time.seconds()-timecorrection) >=
            (applnstart_time + 50000000L))) {
          abs_time.tv_sec = applnstart_time + 45000000L; 
        }else {
        abs_time.tv_sec = next_exp_time.seconds() - timecorrection;
        }
        abs_time.tv_nsec = 0;

       //  release_component_lock(); 
       delete current_time ;
       cond_timedwait( &subtbl_mod, &component_mutex_lock, &abs_time);
       
      // sleep(SLEEPTIME);
  } /*End of while */
  return ((void *)NULL);
}

/***************************************************************************/
/*******Recursively frees all the dynamically allocated memory in the    **/
/*****  DmiAttributeValues_t structure                               ******/
void free_attrvalues(DmiAttributeValues_t *keylist) {

DmiAttributeData_t *attribptr=NULL;
int     count;

    if (keylist) {
    attribptr = keylist->list.list_val;
    for(count=1; count<=keylist->list.list_len;count++) {
           switch(attribptr->data.type) {
           case MIF_COUNTER:
                          break;
           case MIF_COUNTER64:
                          break;
           case MIF_GAUGE:
                          break;
           case MIF_INTEGER:
                          break;
           case MIF_INTEGER64:
                          break;
           case MIF_OCTETSTRING:
            if ((attribptr) &&
             (attribptr->data.DmiDataUnion_u.octetstring) &&
             (attribptr->data.DmiDataUnion_u.octetstring->body.body_val)) {
               free(attribptr->data.DmiDataUnion_u.octetstring->body.body_val);
               free(attribptr->data.DmiDataUnion_u.octetstring);
            }
                          break;
           case MIF_DISPLAYSTRING:
            if ((attribptr) &&
                (attribptr->data.DmiDataUnion_u.str) &&
                (attribptr->data.DmiDataUnion_u.str->body.body_val)) {
               free(attribptr->data.DmiDataUnion_u.str->body.body_val);
               free(attribptr->data.DmiDataUnion_u.str);
            }
                          break;
           case MIF_DATE:
            if ((attribptr) && (attribptr->data.DmiDataUnion_u.date))
                 free(attribptr->data.DmiDataUnion_u.date);
                 break;
           }
      attribptr++;
    }
      if (keylist->list.list_val)
           free(keylist->list.list_val);
      free(keylist);
    }
}


/******************************************************************************/
int compMatch(unsigned long compId, DmiMultiRowData_t *rowdata) {
 
DmiRowData_t *rowData;      

          rowData= rowdata->list.list_val;
          if (!rowData)
             return 0;
          if (compId == 0xFFFFFFFF ) 
                return 1; /*Match any compId */

          if (compId == rowData->compId) 
               return 1;
return 0;
}
/******************************************************************************/
int classMatch(DmiString_t *groupclass, DmiMultiRowData_t *rowdata) {
             
char *defbody,*specname, *version;
char *ev_defbody,*ev_specname, *ev_version;
DmiRowData_t  *rowData;
int           count,prevcount;
DmiDataUnion_t         *attr_data;
DmiString_t            *assocgroup;

   defbody = specname = version = NULL;
   ev_defbody = ev_specname = ev_version = NULL;
   rowData = rowdata->list.list_val;

   if (!groupclass)
        return 0;
   if (!strncmp(groupclass->body.body_val, "||", 2)) 
        return 1;

   prevcount = count =0;
   while((count < groupclass->body.body_len) &&
         (groupclass->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
       defbody = (char *)calloc(1,(count-prevcount)+1);    
       memcpy(defbody, &(groupclass->body.body_val[prevcount]), (count-prevcount));
   }

   prevcount = ++count;    /* Bump beyond '|' char */
   while((count < groupclass->body.body_len) &&
         (groupclass->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
       specname = (char *)calloc(1,(count-prevcount)+1);    
       memcpy(specname, &(groupclass->body.body_val[prevcount]), (count-prevcount));
   }
 
   prevcount = ++count;    /* Bump beyond '|' char */
   while((count < groupclass->body.body_len) &&
         (groupclass->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
       version = (char *)calloc(1,(count-prevcount)+1);    
       memcpy(version, &(groupclass->body.body_val[prevcount]), (count-prevcount));
   }


   attr_data = getAttrData(rowData->values, EVENT_ASSOCGROUP);
   if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for GroupClass AttrID\n");
         return 0;
   }

   if ((attr_data->type != MIF_DISPLAYSTRING)
       && (attr_data->type != MIF_OCTETSTRING)) {
         printf("SPIndication:Illegal type for GroupClass AttrID\n");
         return 0;
   }
   assocgroup = (attr_data->DmiDataUnion_u.str);
   prevcount = count =0;
   while((count < assocgroup->body.body_len) &&
         (assocgroup->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
     ev_defbody = (char *)calloc(1,(count-prevcount)+1);
     memcpy(ev_defbody, &(assocgroup->body.body_val[prevcount]), (count-prevcount));
   }

   prevcount = ++count;    /* Bump beyond '|' char */
   while((count < assocgroup->body.body_len) &&
         (assocgroup->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
    ev_specname = (char *)calloc(1,(count-prevcount)+1);
    memcpy(ev_specname, &(assocgroup->body.body_val[prevcount]), (count-prevcount));
   }

   prevcount = ++count;    /* Bump beyond '|' char */
   while((count < assocgroup->body.body_len) &&
         (assocgroup->body.body_val[count] != '|'))
                     count++;
   if (count - prevcount) {
     ev_version = (char *)calloc(1,(count-prevcount)+1);
     memcpy(ev_version, &(assocgroup->body.body_val[prevcount]), (count-prevcount));
   }

   if (defbody) {
     if (strcmp(defbody,ev_defbody))
          return 0;
   }
   if (specname) {
     if (strcmp(specname, ev_specname))
           return 0;
   }
   if (version) {
     if (strcmp(version, ev_version))
           return 0;
   }
   
  return  1;
   
}
/******************************************************************************/
int severityMatch(unsigned long severity, DmiMultiRowData_t *rowdata) {
unsigned long ev_severity;
DmiDataUnion_t  *attr_data;
DmiRowData_t    *rowData;

  rowData = rowdata->list.list_val;
  if (!rowData)
     return 0;

  attr_data = getAttrData(rowData->values, EVENT_SEVERITY);            
  if (!attr_data) {
      printf("SPIndication:NULL DataUnion ptr. for Event Severity AttrID\n");
      return 0;            
  }

  if (attr_data->type != MIF_INTEGER) {
      printf("SPIndication:Illegal type for Event Severity AttrID\n");
      return 0;     
  }
  ev_severity = attr_data->DmiDataUnion_u.integer;

  if (severity | ev_severity)
        return 1;
  else
        return 0;

}
/******************************************************************************/
int mgt_dmideliverevent(DmiDeliverEventIN *argp) {

unsigned int  count,loop;
int 	errcode;
char     errbuff[100];
int      threshold_val;
RWTPtrSlist<DmiAttributeValues_t>  *subs_rows,*filt_rows;
DmiAttributeValues_t               *subs_ent,*filt_ent;
DmiDataUnion_t                     *attr_data;
DmiString_t                        *receiver;
unsigned long                      compId, severity;
DmiString_t                        *groupclass;

  acquire_component_lock();
  subs_rows  = subscription_table->GetRows();
  filt_rows  = filter_table->GetRows();
 
  /*loop thru' the filter table */
  count = filt_rows->entries();
  while(count-- > 0)  {
      filt_ent = filt_rows->get();
      if (!filt_ent) {
         printf("SPIndication:Filter table has NULL entry\n");
         continue;
      }
      
      attr_data = getAttrData(filt_ent, COMPID_ATTRID);
      if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for CompID AttrID\n");
         continue;
      }

      if (attr_data->type != MIF_INTEGER) {
         printf("SPIndication:Illegal type for CompId AttrID\n");
         continue;
      }
      compId = attr_data->DmiDataUnion_u.integer;

      attr_data = getAttrData(filt_ent, CLASS_ATTRID);
      if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for GroupClass AttrID\n");
         continue;
      }

      if ((attr_data->type != MIF_DISPLAYSTRING)
           && (attr_data->type != MIF_OCTETSTRING)) {
         printf("SPIndication:Illegal type for GroupClass AttrID\n");
         continue;
      }
      groupclass = (attr_data->DmiDataUnion_u.str);

      attr_data = getAttrData(filt_ent, SEVERITY_ATTRID);
      if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for Severity AttrID\n");
         continue;
      }

      if (attr_data->type != MIF_INTEGER) {
         printf("SPIndication:Illegal type for Severity AttrID\n");
         continue;
      }
      severity = attr_data->DmiDataUnion_u.integer;


      if ((compMatch(compId , argp->rowData)) &&
          (classMatch(groupclass, argp->rowData)) &&
          (severityMatch(severity , argp->rowData))) {

/*Find Receiver Addr/ Threshold Val. first */
        attr_data = getAttrData(filt_ent, RECEIVER_ADDRESSID);
        if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for Receiver_ADDR AttrID\n");
         continue;
        }

        if ((attr_data->type != MIF_DISPLAYSTRING)
           && (attr_data->type != MIF_OCTETSTRING)) {
            printf("SPIndication:Illegal type for Receiver_ADDR AttrID\n");
            continue;
        }
        receiver = (attr_data->DmiDataUnion_u.str);

        filt_ent->list.list_len = 4;   /*Key for subscription table*/
        for(loop=0; loop < subs_rows->entries(); loop++) {
           subs_ent = subs_rows->at(loop);
           if (IsKeyMatch(subs_ent, filt_ent) == TRUE) {
                 break;
           }
        }
        filt_ent->list.list_len = 7;

        if (loop == subs_rows->entries()) {
           printf("SPIndication:No matching Subscription entry for Filter Ent.\n");
           continue;
        }

        attr_data = getAttrData(subs_ent, THRESH_ATTRID);
        if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for Threshold AttrID\n");
         continue;
        }

        if (attr_data->type != MIF_INTEGER) {
          printf("SPIndication:Illegal type for Threshold AttrID\n");
          continue;
        }
        threshold_val = attr_data->DmiDataUnion_u.integer;

        if (!send_to_micallback(receiver, threshold_val, EVENT_OCCURENCE,
                                                     (void *)argp))  {
            subs_ent->list.list_len = 4;
                   /*Key for subscription table*/
            int lp= filt_rows->entries();
            while(lp-- > 0) {
                 filt_ent = filt_rows->get();
                 if (IsKeyMatch(filt_ent, subs_ent) == TRUE) {
                        free_attrvalues(filt_ent);
                        //filt_rows->removeAt(lp);
                        continue;
                 }
                 filt_rows->append(filt_ent);
            }
            subs_ent->list.list_len = 7;

            subs_rows->removeAt(loop);
            free_attrvalues(subs_ent);

            /*update the database - TBD */ 
            WriteComponentToDB(errcode, compobj, DB_UPDATE_ALL);
            if (errcode != DMIERR_NO_ERROR) {
                sprintf(errbuff, "Error in WriteComponentToDB =%d",errcode);
                syslog(LOG_ERR, errbuff);
                exit(1);
            }
            continue;
        }
      }
      filt_rows->append(filt_ent);        
  } /*Loop thru' the filter table */
  release_component_lock();         
  return 1;
}
/******************************************************************************/
void dispatch_dmimessage(int event_type, void *argp) {
unsigned int  count,loop;
int	errcode;
char     errbuff[100];
int      threshold_val;
RWTPtrSlist<DmiAttributeValues_t>  *subs_rows,*filt_rows;
DmiAttributeValues_t               *subs_ent,*filt_ent;
DmiDataUnion_t                     *attr_data;
DmiString_t                        *receiver;

  acquire_component_lock();
  subs_rows  = subscription_table->GetRows();
  filt_rows  = filter_table->GetRows();

  /*loop thru' the subscription table */
  count = subs_rows->entries();
  while(count-- > 0) {
     subs_ent = subs_rows->get();
     if (!subs_ent)  {
       printf("SPIndication:Subscription table has NULL table entry\n");
       continue;
     }

     attr_data = getAttrData(subs_ent, THRESH_ATTRID);
     if (!attr_data) {
        printf("SPIndication:NULL DataUnion ptr. for Threshold AttrID\n");
        continue;
     }

     if (attr_data->type != MIF_INTEGER) {
        printf("SPIndication:Illegal type for Threshold AttrID\n");
        continue;
     }

     threshold_val = attr_data->DmiDataUnion_u.integer;

     attr_data = getAttrData(subs_ent, RECEIVER_ADDRESSID);
     if (!attr_data) {
         printf("SPIndication:NULL DataUnion ptr. for Receiver_ADDR AttrID\n");
         continue;
     }

     if ((attr_data->type != MIF_DISPLAYSTRING)
         && (attr_data->type != MIF_OCTETSTRING)) {
         printf("SPIndication:Illegal type for Receiver_ADDR AttrID\n");
         continue;
     }
     receiver = (attr_data->DmiDataUnion_u.str);

     if (!send_to_micallback(receiver,threshold_val,event_type, argp)) {
             //subs_rows->removeAt(count); //clean subscription table

             //clean the filter table
             subs_ent->list.list_len = 4;
                     /*Key for subscription table*/
             loop = filt_rows->entries();
             while(loop-- > 0) {
                     filt_ent = filt_rows->get();
                     if (IsKeyMatch(filt_ent, subs_ent) == TRUE) {
                         free_attrvalues(filt_ent);
                        // filt_rows->removeAt(loop);
                         continue;
                     }
                     filt_rows->append(filt_ent);
             }
             subs_ent->list.list_len = 7;

             free_attrvalues( subs_ent ); // free the subs table buffer
             // update the database - TBD
             WriteComponentToDB(errcode, compobj, DB_UPDATE_ALL);
             if (errcode != DMIERR_NO_ERROR) {
                sprintf(errbuff, "Error in WriteComponentToDB =%d",errcode);
                syslog(LOG_ERR, errbuff);
                exit(1);
             }
             continue;
     }
     subs_rows->append(subs_ent);
  }
  release_component_lock();
}
/******************************************************************************/
int mgt_dmicomponentadded(DmiComponentAddedIN *argp) {
    dispatch_dmimessage(COMPONENTADD, (void *)argp);
    return 1;
}
/******************************************************************************/
int mgt_dmicomponentdeleted(DmiComponentDeletedIN  *argp) {
    dispatch_dmimessage(COMPONENTDEL, (void *)argp);
    return 1;
}
/******************************************************************************/
int mgt_dmilanguageadded(DmiLanguageAddedIN *argp) {
    dispatch_dmimessage(LANGADD, (void *)argp);
    return 1;
}
/******************************************************************************/
int mgt_dmilanguagedeleted(DmiLanguageDeletedIN *argp) {
    dispatch_dmimessage(LANGDEL, (void *)argp);
    return 1;
}
/******************************************************************************/
int mgt_dmigroupadded(DmiGroupAddedIN *argp) {
    dispatch_dmimessage(GROUPADD, (void *)argp);
    return 1;
}
/******************************************************************************/
int mgt_dmigroupdeleted(DmiGroupDeletedIN *argp) {
    dispatch_dmimessage(GROUPDEL, (void *)argp);
    return 1;
}

