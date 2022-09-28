/* Copyright 09/16/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)reg_subtree.h	1.5 96/09/16 Sun Microsystems"



#ifndef _REG_SUBTREE_H_
#define _REG_SUBTREE_H_

extern int SSARegSubagent(Agent* agent);

extern int SSARegSubtree(SSA_Subtree *subtree);

extern int SSARegSubtable(SSA_Table *table);

extern int SSAGetTrapPort();

/*
 * it will request a resource handler(an agent id) from the relay agent,
 * it returns the agent id if successful, otherwise, return 0
 * if fails, it will retry "num_of_retry".
 */
extern int SSASubagentOpen(int num_of_retry,char *agent_name);

/* 
 * agent_addr==NULL => use the local host address 
 * community==NULL => public 
 * return TRUE if agent is alive, otherwise FALSE 
 */ 
extern int SSAAgentIsAlive(IPAddress *agent_addr,int port,char* community, 
        struct timeval *timeout); 
 

/* if flag = 1, turn on the auto mode */
extern void SSAAutoMemFree(int flag);

extern void _SSASendTrap(char* name);

#endif
