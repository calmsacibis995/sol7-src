/* Copyright 11/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)agent.c	1.7 96/11/01 Sun Microsystems"

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "impl.h"
#include "error.h"
#include "trace.h"
#include "pdu.h"

#include "agent.h"
#include "subtree.h"
#include "session.h"
#include "sh_table.h"


/***** STATIC VARIABLES *****/

int sap_agent_id = 1;

/* the agent list */
Agent *first_agent = NULL;


/****************************************************************/

void trace_agents()
{
	Agent *ap;


	trace("AGENTS:\n");
	for(ap = first_agent; ap; ap = ap->next_agent)
	{

                trace("\t%-30s %-30s %8d %8d %8d %8d\n",
                        ap->name?ap->name:"NO NAME",
                        address_string(&(ap->address)),
                        ap->timeout,ap->agentID,ap->agentStatus,
                        ap->agentProcessID);

	}
	trace("\n");
}


/****************************************************************/

/* We must invoke subtree_list_delete() before invoking	*/
/* this function because the first_agent_subtree member	*/
/* of the agent structures should be NULL		*/

void agent_list_delete()
{
	Agent *ap = first_agent;
	Agent *next;


	while(ap)
	{
		next = ap->next_agent;

		agent_free(ap);

		ap = next;
	}

	first_agent = NULL;

	return;
}


/****************************************************************/

static void free_string_content(String str)
{
  if(str.chars != NULL && str.len != 0){
	free(str.chars);
	str.chars = NULL;
	str.len = 0;
  }
}

/* The fisrt_agent_subtree member of the agent		*/
/* structure should be NULL				*/

void agent_free(Agent *ap)
{
	if(ap == NULL)
	{
		return;
	}

	if(ap->first_agent_subtree)
	{
		error("BUG: agent_free(): first_agent_subtree not NULL");
	}

	/* free the extra element */

	free_string_content(ap->agentPersonalFile);
	free_string_content(ap->agentConfigFile);
	free_string_content(ap->agentExecutable);
	free_string_content(ap->agentVersionNum);
	free_string_content(ap->agentProtocol);
	free_string_content(ap->agentName);
	if(ap->name) free(ap->name);
	free(ap);
	ap =NULL;
	return;
}

/****************************************************************/
Agent *agent_find_by_id(int id)
{
	Agent *ap;


	for(ap = first_agent; ap; ap = ap->next_agent)
	{
		if(ap->agentID == id)
		{
			return ap;
		}
	}

	return NULL;
}


Agent *agent_find_by_name(char* name)
{
	Agent *ap;


	for(ap = first_agent; ap; ap = ap->next_agent)
	{
		if(!strcmp(ap->name,name))
		{
			return ap;
		}
	}

	return NULL;
}

/* agent_find() is used to check if we have not		*/
/* two SNMP agents registered on the same UDP port	*/

Agent *agent_find(Address *address)
{
	Agent *ap;


	for(ap = first_agent; ap; ap = ap->next_agent)
	{
		if(ap->address.sin_port == address->sin_port)
		{
			return ap;
		}
	}

	return NULL;
}

void agent_update_subtree(Agent* agent)
{
  Subtree *sp;
  if(agent == NULL) return ;
  sp = agent->first_agent_subtree;
  for(;sp;sp=sp->next_agent_subtree){
	sp->regTreeStatus = agent->agentStatus;
  }
}

void agent_detach_from_list(Agent* agent)
{
	Agent *ap, *last=NULL;

        if(agent == NULL) return;
	for(ap = first_agent; ap ; ap = ap->next_agent)
	{
		if(ap == agent)
			break;
		last = ap;
	}
	if(ap==NULL) return;
	if(last == NULL){
		first_agent = ap->next_agent;
	}else{
		last->next_agent = ap->next_agent;
	}
	ap->next_agent = NULL;
}


void agent_destroy(Agent* agent)
{
  if(agent!=NULL){
	if(agent->agentID==sap_agent_id-1)
		sap_agent_id--;
  }
  agent_detach_from_list(agent);
  delete_all_table_from_agent(agent);
  delete_all_subtree_from_agent(agent);
  delete_agent_from_resource_list(agent); 
  agent_free(agent);
}

	
/****************************************************************/

/* destroy hanging agent when no outstanding session which
   relates to the agent */
void destroy_hanging_agent()
{
	Agent *ap;

	for(ap = first_agent; ap ; ap = ap->next_agent)
	{
		if(ap->numOfFailRequest > 5){
		  if(no_outstanding_session_for_the_agent(ap)){
			error("Delete agent %s from agent table",ap->name);
			agent_destroy(ap);
			return;
		  }
		}
	}
}
