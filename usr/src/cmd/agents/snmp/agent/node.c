/* Copyright 07/22/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)node.c	1.4 96/07/22 Sun Microsystems"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "impl.h"
#include "error.h"
#include "trace.h"

#include "agent_msg.h"
#include "node.h"


/***** STATIC VARIABLES ******/

static Node *root_node = &node_table[0];


/*****************************************************************/

Node *node_find(int search_type, Oid *name, Oid *suffix)
{
	int i;
	Node *parent;
	Node *previous;
	Node *node;


	if( (name == NULL)
		|| (name->len < 1)
		|| (name->subids[0] != root_node->subid) )
	{
		suffix->subids = NULL;
		suffix->len = 0;

		if(trace_level > 0)
		{
			trace("node_find() returned NULL\n\n");
		}

		return NULL;
	}

	parent = root_node;
	for(i = 1; i < name->len; i++)
	{
		previous = NULL;

		for(node = parent->first_child; node; node = node->next_peer)
		{
			if(node->subid > name->subids[i])
			{
				switch(search_type)
				{
					case NEXT_ENTRY:
						suffix->len = 0;
						suffix->subids = NULL;

						if(trace_level > 0)
						{
							trace("node_find() returned %s with no suffix\n\n",
								node->label);
						}

						return node;

					case EXACT_ENTRY:
						node = NULL;
						break;
				}

				break;
			}

			if(node->subid == name->subids[i])
			{
				parent = node;
				break;
			}

			previous = node;
		}

		if(node == NULL)
		{
			switch(search_type)
			{
				case NEXT_ENTRY:
					suffix->subids = NULL;
					suffix->len = 0;

					if(previous)
					{
						if(trace_level > 0)
						{
							if(previous->next)
							{
								trace("node_find() returned %s with no suffix\n\n",
									previous->next->label);
							}
							else
							{
								trace("node_find() returned NULL\n\n");
							}
						}

						return previous->next;
					}
					else
					{
						if(trace_level > 0)
						{
							if(parent->next)
							{
								trace("node_find() returned %s with no suffix\n\n",
									parent->next->label);
							}
							else
							{
								trace("node_find() returned NULL\n\n");
							}
						}

						return parent->next;
					}

				case EXACT_ENTRY:
					suffix->subids = NULL;
					suffix->len = 0;

					if(trace_level > 0)
					{
						trace("node_find() returned NULL\n\n");
					}

					return NULL;
			}
		}

		if( (node->type == COLUMN)
			|| (node->type == OBJECT) )
		{
			suffix->len = name->len - (i + 1);
			if(suffix->len)
			{
				suffix->subids = (Subid *) malloc(suffix->len * sizeof(Subid));
				if(suffix->subids == NULL)
				{
					error(ERR_MSG_ALLOC);
					return NULL;
				}

				memcpy(suffix->subids, &(name->subids[i + 1]), suffix->len * sizeof(Subid));
			}
			else
			{
				suffix->subids = NULL;
			}

			if(trace_level > 0)
			{
				trace("node_find() returned %s with suffix %s\n\n",
					parent->label, SSAOidString(suffix));
			}

			return node;
		}
	}

	suffix->len = 0;
	suffix->subids = NULL;

	if(trace_level > 0)
	{
		trace("node_find() returned %s with no suffix\n\n",
			node->label);
	}

	return node;
}


/*****************************************************************/




