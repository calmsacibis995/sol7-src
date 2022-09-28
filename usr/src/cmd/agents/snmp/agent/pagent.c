/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)pagent.c	1.1 96/07/01 Sun Microsystems"


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

#include "pagent.h"
#include "subtree.h"


/***** STATIC VARIABLES *****/

/* the agent list */
Agent *first_agent = NULL;


/***** STATIC FUNCTIONS *****/

static void agent_free(Agent *ap);


/****************************************************************/

void trace_agents()
{
	Agent *ap;


	trace("AGENTS:\n");
	for(ap = first_agent; ap; ap = ap->next_agent)
	{
		trace("\t%-30s %-30s %8d\n",
			ap->name,
			address_string(&(ap->address)),
			ap->timeout);
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

/* The fisrt_agent_subtree member of the agent		*/
/* structure should be NULL				*/

static void agent_free(Agent *ap)
{
	if(ap == NULL)
	{
		return;
	}

	if(ap->first_agent_subtree)
	{
		error("BUG: agent_free(): first_agent_subtree not NULL");
	}

	free(ap->name);
	free(ap);

	return;
}

/****************************************************************/

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

	
/****************************************************************/

