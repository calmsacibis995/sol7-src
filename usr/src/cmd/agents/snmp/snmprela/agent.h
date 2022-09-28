/* Copyright 10/30/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)agent.h	1.7 96/10/30 Sun Microsystems"

/* HISTORY
 * 5-20-96	Jerry Yeung	add mib-handling data structure
 * 9-20-96	Jerry Yeung	change agent structure
 */

#ifndef _AGENT_H_
#define _AGENT_H_

#include "snmpdx_stub.h"

#define SSA_OPER_STATUS_ACTIVE	1
#define SSA_OPER_STATUS_NOT_IN_SERVICE	2
#define SSA_OPER_STATUS_INIT 	3
#define SSA_OPER_STATUS_LOAD	4
#define SSA_OPER_STATUS_DESTROY	5

typedef struct _Agent {

	/* extra elements */
        Integer agentID;
        Integer agentStatus;
        Integer agentTimeOut;
        Integer agentPortNumber; /* same as address.sin_port */
        String agentPersonalFile;
        String agentConfigFile;
        String agentExecutable;
        String agentVersionNum;
        Integer agentProcessID;
        String agentName; /* it points to name */
	Integer agentSystemUpTime;
        Integer agentWatchDogTime;
        String agentProtocol;

	Integer	agentTreeIndex;
	Integer	agentTblIndex;

	struct _Agent	*next_agent;
	Address		address;
	char		*name;
	u_long		timeout;
	struct _Subtree	*first_agent_subtree;
	int		numOfFailRequest;
} Agent;

extern int sap_agent_id;

/* the agent list */
extern Agent *first_agent;

/* the address is a unique key for an agent */
extern Agent *agent_find(Address *address);
extern Agent *agent_find_by_name(char* name);
extern Agent *agent_find_by_id(int id);

/* We must invoke subtree_list_delete() before invoking */
/* this function because the first_agent_subtree member */
/* of the agent structures should be NULL               */
extern void agent_list_delete();

extern void agent_update_subtree(Agent* agent);

extern void agent_detach_from_list(Agent* agent);

extern void agent_destroy(Agent* agent);

extern void trace_agents();
extern void agent_free(Agent *ap);

#endif

