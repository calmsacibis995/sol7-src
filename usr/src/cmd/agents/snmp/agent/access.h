/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)access.h	1.3 96/07/01 Sun Microsystems"

/*
 * HISTORY
 * 6-21-96	Jerry Yeung	ACL
 * 6-25-96	Jerry Yeung	TRAP
 */

#include "impl.h"
#include "pdu.h"

#ifndef _ACCESS_H_
#define _ACCESS_H_

#define READ_ONLY	1
#define READ_WRITE	2

typedef struct _Community {
	struct _Community *next_community;
	char *name;
	int type;
} Community;

typedef struct _AccessPolicy {
	Community *first_community;
	int access_type; /* one access type for all the communities */
	int count;
} AccessPolicy;

typedef struct _AccessServer {
	struct _AccessServer *next_acc_server;
	AccessPolicy *first_acc_policy;
	int attached;
} AccessServer;

typedef struct _Manager {
	struct _Manager *next_manager;
	char *name;
	IPAddress ip_address;
	AccessServer *first_acc_server;	
} Manager;

/****************************************************

--------------
| Manager     |-->
| xxxxxxx     |
| :name       |
| :ip_address |
--------------
   |
   V                
----------     --------------     -------------
| Manager |-->| AccessServer |-->| AccessServer|-->
----------     --------------     -------------
   |		    0
		    0 (link to shared object)
   V		    V
		-------------     ---------     ---------
		|AccessPolicy|-->|Community|-->|Community|-->
		|xxxxxxxxxxxx|   |xxxxxxxxx|    ---------
		|:access_type|   |:name	   |
		-------------     ---------    
1
******************************************************/
typedef struct _SubMember {
	Manager *first_manager;
	char *community_string;
	int count;
} SubMember;

typedef struct _SubGroup {
	struct _SubGroup *next_sub_group;
	SubMember *first_sub_member;
} SubGroup;

typedef struct _TrapSlot {
	struct _TrapSlot *next_trap_slot;
	SubGroup *first_sub_group;
	int num;
} TrapSlot;

typedef struct _EFilter {
	struct _EFilter *next_efilter;
	TrapSlot *first_trap_slot;
	Oid  *enterprise;
	char *name;
	int type; /* generic or specific */
} EFilter;

/****************************************************

--------------
| EFilter     |-->
| xxxxxxx     |
| :enterprise |
--------------
   | next_efilter
   V                
---------- first_trap_slot -----------  next_trap_slot   ---------
|EFilter |--------------->| TrapSlot  |---------------->| TrapSlot|-->
----------     		  | xxxxxxxxx |  		 ---------
   |		          | : num     |	
   V		           -----------
				| first_sub_group
				V
			------------------                ------------   
			| SubGroup        |next_sub_group | SubGroup |
			| xxxxxxxxxxxxxxxx|--------------> ----------
			------------------  
				0 first_sub_member
				0
				V
			    -----------------
			   | SubMember 	     |first_manager ---------
			   | xxxxxxxxxxxxxxx |------------>| Manager |
			   |:community_string|		    --------- 
			    -----------------

******************************************************/
typedef struct _NameOidPair {
  struct _NameOidPair *next;
  char *name;
  Oid *oid;
} NameOidPair;

/* routines for trap */
extern EFilter* efilter_add(char* name, char *error_label);
extern TrapSlot* trap_slot_add(int num,EFilter *efilter,char *error_label);
extern void sub_group_add_tail(TrapSlot *slot,SubGroup *group);
extern void sub_member_free(SubMember *mem);
extern void sub_group_list_free(SubGroup *group);
extern void trap_slot_list_free(TrapSlot *slot);
extern void mem_filter_join(int low,int high,SubMember *mem,EFilter *filter);
extern void trace_filter();

extern Manager* manager_add(char *name, char *error_label);
extern int is_valid_manager(Address *address,Manager **mngr);
extern void delete_manager_list();
extern void manager_list_free(Manager *mngr);
extern void trace_managers();

extern int community_add(char *name, int type, char *error_label);
extern int is_valid_community(char *name, int type, Manager *mngr);
extern void delete_community_list();
extern void trace_communities(Community *c);
extern void trace_access_server(AccessServer *as);
extern void trace_access_policy(AccessPolicy *ap);

extern void community_attach(AccessPolicy *ap, Community *comm);
extern void access_server_add_tail(Manager* mngr, AccessServer *acc_server);
extern void access_server_free(AccessServer *as);
extern void access_policy_list_free(AccessPolicy *ap);
extern int get_access_type(Manager *mngr,char *name);

/* name oid pair loading */
extern void trace_name_oid_pair();
extern Oid *enterprise_name_to_oid(char *name);
extern void load_enterprise_oid(char* filename);

extern void trap_filter_action(Oid *oid,int generic,int specific,
	u_long time_stamp,SNMP_variable *variables);

#endif
