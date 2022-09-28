/* Copyright 10/30/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)res.h	1.4 96/10/30 Sun Microsystems"

#ifndef _RES_H_
#define _RES_H_

#define RES_NOT_VISIT 0
#define RES_VISIT 1 

#define TYPE_LEGACY_SUB_AGENT   "legacy"
#define POLICY_SPAWN            "spawn"
#define POLICY_LOAD     "load"

/* resouce support */
typedef struct _SapResource{
        struct _SapResource* next_res;
        char *res_name;
        char *dir_file;
        char *personal_file;
        time_t personal_file_time;
        char *sec_file;
        time_t sec_file_time;
        int invoke_mode; /* invoke it and keep it alive */
        char* policy;
	char* type;
	char* user; 
        char *start_cmd;
        Agent* agent;
	int mark; /* flag for visit */
        time_t rsrc_file_time;
} SapResource;

extern SapResource *first_res;
extern SapResource *reconfig_first_res;

extern void resource_list_delete();
extern void trace_resources();
extern SapResource *resource_find_by_agent(Agent* agent);
extern void mark_all_resources_not_visit();
extern SapResource *resource_find_by_name(char* name);
extern void get_file_modify_time(char* filename,time_t *file_time);
extern void resource_free(SapResource *ap);
extern void merging_resource_list();
extern void resource_handling(SapResource *rp);
extern void delete_pid_rec_list();
extern void delete_agent_from_resource_list(Agent *agent); 
extern void write_pid_file(char* filename);
extern void watch_dog_in_action();
extern void kill_all_pid_rec_list();
extern void kill_part_pid_rec_list();

#endif

