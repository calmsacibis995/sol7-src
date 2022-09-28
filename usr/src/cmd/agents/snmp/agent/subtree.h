/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)subtree.h	1.1 96/07/01 Sun Microsystems"


#ifndef _SUBTREE_H_
#define _SUBTREE_H_

typedef struct _Subtree {
	int regTreeIndex;
	int regTreeAgentID;
	Oid		name;
	int regTreeStatus;
	struct _Subtree	*next_subtree;
	struct _Agent	*agent;
	struct _Subtree	*next_agent_subtree;
} Subtree;

typedef Subtree SSA_Subtree;


int subtree_add(int agent_id, Agent *agent, Subid *subids, int len);

Subtree *subtree_match(u_char type, Oid *oid);

void subtree_list_delete();


void trace_subtrees();


#endif
