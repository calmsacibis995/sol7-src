/* Copyright 10/15/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)subscription.hh	1.8 96/10/15 Sun Microsystems"




#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/systeminfo.h>
#include <netdb.h>
#include <thread.h>
#include <string.h>
#include <rpc/rpc.h>
#include <syslog.h>
#include <errno.h>

#include <rw/rwtime.h>

//#include <common.h>
#include <server.h>
#include <dmi.hh>
#include <mi_indicate.h>
#include <dmi_error.hh>
#include <search_util.hh>
#include <dbapi.hh>


#define  SPCOMPID     1
#define  RECEIVER_ADDRESSID   3
#define  WARNING_TIME_ATTRID  5
#define  EXP_TIME_ATTRID      6
#define  THRESH_ATTRID        7
#define  COMPID_ATTRID        5
#define  CLASS_ATTRID         6
#define  SEVERITY_ATTRID      7

#define  EVENT_ASSOCGROUP     5
#define  EVENT_SEVERITY       2

#define  SLEEPTIME            600  


/*Indication Types */
#define COMPONENTADD       1 
#define COMPONENTDEL       2 
#define GROUPADD           3 
#define GROUPDEL           4 
#define LANGADD            5 
#define LANGDEL            6 
#define EVENT_OCCURENCE    7 
#define SUBS_NOTICE        8 

extern char *filter_class;
extern char *subs_class;

class Exp_time: public RWTime {
public:
      Exp_time():RWTime() {};
      ~Exp_time() {};

      Exp_time(unsigned long s);
      Exp_time(DmiTimestamp_t tm);     
Exp_time operator=(time_t s);
RWBoolean  operator<(time_t s);
};


extern int  send_to_micallback(DmiString_t *receiver, int threshold, 
                    int event_type, void *argp) ;
extern void dispatch_dmimessage(int event_type, void *argp);
extern void *subscription_housekeep(void *param) ;
extern void free_attrvalues(DmiAttributeValues_t *keylist); 
extern int compMatch(unsigned long compId, DmiMultiRowData_t *rowdata); 
extern int classMatch(DmiString_t *groupclass, DmiMultiRowData_t *rowdata); 
extern int severityMatch(unsigned long severity, DmiMultiRowData_t *rowdata); 
extern int mgt_dmideliverevent(DmiDeliverEventIN *argp); 
extern int mgt_dmicomponentadded(DmiComponentAddedIN *argp); 
extern int mgt_dmicomponentdeleted(DmiComponentDeletedIN  *argp); 
extern int mgt_dmilanguageadded(DmiLanguageAddedIN *argp); 
extern int mgt_dmilanguagedeleted(DmiLanguageDeletedIN *argp); 
extern int mgt_dmigroupadded(DmiGroupAddedIN *argp); 
extern int mgt_dmigroupdeleted(DmiGroupDeletedIN *argp); 
extern void event_subscriber_init(void );
extern void acquire_component_lock(void );
extern void release_component_lock(void);

