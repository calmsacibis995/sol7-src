/* Copyright 07/08/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)pagent.h	1.2 96/07/08 Sun Microsystems"


/* HISTORY
 * 5-17-96	Jerry Yeung	add file properties to agent
 *				change type of timeout from u_long to int
 */

#ifndef _AGENT_H_
#define _AGENT_H_

#define SSA_OPER_STATUS_ACTIVE    1
#define SSA_OPER_STATUS_NOT_IN_SERVICE    2
#define SSA_OPER_STATUS_NOT_READY 3
#define SSA_OPER_STATUS_CREATE_AND_WAIT   4
#define SSA_OPER_STATUS_DESTROY           5

/* this macro depends on the
   AgentStatus field in MIB object in
   relay agent
 */


typedef struct _Agent {
	int		timeout;
	int		agent_id;
	int		agent_status;
/* 
	int		port_number;
 */
	char		*personal_file;
	char		*config_file;
	char		*executable;
	char		*version_string;
	char		*protocol;
	int		process_id;
	char		*name;
	int		system_up_time;
	int		watch_dog_time;
	
	Address		address;
	struct _Agent	*next_agent;
	struct _Subtree	*first_agent_subtree;
		
	int 	tree_index;
	int	table_index;

/*
 *	max number of variables per packet
 	int 	max_var_binds;
 	char*	description;
	
 */
} Agent;


/* the agent list */
extern Agent *first_agent;

/* the address is a unique key for an agent */
extern Agent *agent_find(Address *address);

/* We must invoke subtree_list_delete() before invoking */
/* this function because the first_agent_subtree member */
/* of the agent structures should be NULL               */
extern void agent_list_delete();

extern void trace_agents();



#endif

