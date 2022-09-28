/* Copyright 05/19/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)agent.c	1.8 97/05/19 Sun Microsystems"

/* HISTORY
 * 7-11-96	Jerry Yeung	change Entry to be input argument
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#include "impl.h"
#include "error.h"
#include "trace.h"
#include "snmp.h"
#include "pdu.h"
#include "request.h"
#include "trap.h"

#include "node.h"
#include "access.h"

static ssa_mem_free = 1; /* on */


/***** LOCAL VARIABLES *****/

/*
 *	The idea of these cache variables is to avoid to call
 *	the same *(entry->get) function for variables contained
 *	in a single SNMP PDU.
 *
 *	There is still a problem in this mechanism.
 *	When you query a whole table row by row and when
 *	you reached the end of the table, the *(entry->get)
 *	will be called several times:
 *	- for the first column, *(entry->get) will be called
 *	  once to find that the end of the table is reached
 *	  (==> the knowledge that we reached the last row is cached)
 *	  and then *(entry->get) will be called on the first row of
 *	  the same table (==> the first row is cached).
 *	- Same behaviour for all the remaining columns
 *
 *	Possible solutions:
 *	- two caches but this implies the copy of the cached
 *	  structures + all theis pointers
 *	- only one cache + caching the knowledge that we reached the last
 *	  row ???
 *
 */

static Entry *cache_input_entry = NULL;
static Subid cache_input_index1 = (Subid)-1;
static Subid cache_input_index2 = (Subid)-1;
static Subid cache_input_index3 = (Subid)-1;
static void *cache_output_pointer = NULL;
static int cache_output_snmp_error = -1;
static Subid cache_output_index1 = (Subid)-1;
static Subid cache_output_index2 = (Subid)-1;
static Subid cache_output_index3 = (Subid)-1;


/***** LOCAL FUNCTIONS *****/

static int agent_get_next(SNMP_pdu *pdu, char *error_label);
static int agent_get_next_loop(SNMP_variable *variable, Node *node, Oid *suffix);
static int agent_get(SNMP_pdu *pdu, char *error_label);
static int agent_set(int pass, SNMP_pdu *pdu, char *error_label);


/****************************************************************/

/* returns:						*/
/*	0 in case of success (the pdu should be sent	*/
/*	  back to its originator even if an SNMP error	*/
/*	  was detected)					*/
/*	-1 in case of failure (no pdu should be sent	*/
/*	  back)						*/

int agent_process(Address *address, SNMP_pdu *pdu)
{
	int snmpEnableAuthTraps = FALSE;
	Manager *mngr;


	if(pdu == NULL)
	{
		error("BUG: agent_process(): pdu is NULL");
		return -1;
	}


	/* check host */
	if(!is_valid_manager(address,&mngr))
	{
		error("agent_process(): unauthorized manager (%s)",
			ip_address_string(&(address->sin_addr)));

		snmpEnableAuthTraps = request_snmpEnableAuthTraps(error_label);
		switch(snmpEnableAuthTraps)
		{
			case TRUE:
				if(trap_send_to_all_destinators(NULL,
					SNMP_TRAP_AUTHFAIL, 0,
					NULL, error_label))
				{
					error("trap_send_to_all_destinators() failed: %s\n",
						error_label);
				}
				break;

			case FALSE:
			default:
				break;
		}

		return -1;
	}

	/* if mngr == NULL -> allow requests from any hosts */

	/* check pdu type */
	if(pdu->type != GETNEXT_REQ_MSG
		&& (pdu->type != GET_REQ_MSG)
		&& (pdu->type != SET_REQ_MSG) )
	{
		error("agent_process(): bad PDU type (0x%x)", pdu->type);
		return -1;
	}


	/* check host */
	if(!is_valid_community(pdu->community, pdu->type,mngr))
	{
		/*
		 * Earlier, the community name is displayed here
		 * in this error message. But since these error
		 * messages are readable by all users, it is not advisible
		 * to display community names in the error messages.
		 */
		error("agent_process() : bad community from %s",
			ip_address_string(&(address->sin_addr)));

		snmpEnableAuthTraps = request_snmpEnableAuthTraps(error_label);
		switch(snmpEnableAuthTraps)
		{
			case TRUE:
				if(trap_send_to_all_destinators(NULL,
					SNMP_TRAP_AUTHFAIL, 0,
					NULL, error_label))
				{
					error("trap_send_to_all_destinators() failed: %s\n",
						error_label);
				}
				break;

			case FALSE:
			default:
				break;
		}

		return -1;
	}


	if(cache_input_entry != NULL && cache_output_pointer != NULL)
		if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				(*(cache_input_entry->dealloc))(cache_output_pointer);
				cache_output_pointer = NULL;
		}


	cache_input_entry = NULL;


	switch(pdu->type)
	{
		case GETNEXT_REQ_MSG:
			if(agent_get_next(pdu, error_label))
			{
				error("agent_get_next() failed: %s", error_label);
				return -1;
			}
			return 0;

		case GET_REQ_MSG:
			if(agent_get(pdu, error_label))
			{
				error("agent_get() failed: %s", error_label);
				return -1;
			}
			return 0;

		case SET_REQ_MSG:
			switch(agent_set(FIRST_PASS, pdu, error_label))
			{
				case 0:
					switch(agent_set(SECOND_PASS, pdu, error_label))
					{
						case 0:
						case 1:
							return 0;

						case -1:
							error("agent_set(SECOND_PASS) failed: %s",
								error_label);
							return -1;
					}

					/* never reached */
					break;

				case 1:
					return 0;

				case -1:
					error("agent_set(FIRST_PASS) failed: %s",
						error_label);
					return -1;
			}
	}

	/* never reached */
	return -1;
}


/****************************************************************/

/* returns:						*/
/*	0 in case of success (the pdu should be sent	*/
/*	  back to its originator even if an SNMP error	*/
/*	  was detected)					*/
/*	-1 in case of failure (no pdu should be sent	*/
/*	  back)						*/

static int agent_get_next(SNMP_pdu *pdu, char *error_label)
{
	SNMP_variable *variable;
	Node *node;
	Oid suffix;
	int index = 1;
	int snmp_error;


	error_label[0] = '\0';

	pdu->type = GET_RSP_MSG;

	for(variable = pdu->first_variable; variable; variable = variable->next_variable)
	{
		node = node_find(NEXT_ENTRY, &(variable->name), &suffix);
		if(node == NULL)
		{
			pdu->error_status = SNMP_ERR_NOSUCHNAME;
			pdu->error_index = index;
			return 0;
		}
		/* we should not forget to free suffix.subids */

		if(trace_level > 0)
		{
			trace("!! getnext(): processing the variable %s\n\n",
				node->label);
		}

		if(variable->type != NULLOBJ)
		{
			error("ASN.1 type (0x%x) is not NULL for node %s",
				variable->type, node->label);
			variable->type = NULLOBJ;
		}

		if(variable->val.string)
		{
			error("val is not NULL for node %s",
				node->label);
			free(variable->val.string);
			variable->val.string = NULL;
		}

		if(variable->val_len)
		{
			error("val_len is not 0 for node %s",
				node->label);
			variable->val_len = 0;
		}

		snmp_error = agent_get_next_loop(variable, node, &suffix);

		if(snmp_error != SNMP_ERR_NOERROR)
		{
			pdu->error_status = snmp_error;
			pdu->error_index = index;
			return 0;
		}

		index++;
	}

	return 0;
}


/****************************************************************/

/* This function will free suffix->subids	*/
/* It returns a positive snmp_error code.	*/

static int agent_get_next_loop(SNMP_variable *variable, Node *node, Oid *suffix)
{
	Object *object;
	Column *column;
	Entry *entry;
	Integer integer = 0;
	Integer *integer_ptr;
	String string = { NULL, 0 };
	String *string_ptr;
	Oid oid = { NULL, 0 };
	Oid *oid_ptr;
	char *pointer;
	int snmp_error;
	Subid index1;
	Subid index2;
	Subid index3;

	/* create index struct */
	IndexType index_obj;
	int index_buffer[256];

	index_obj.len = 0;
	index_obj.value = index_buffer; 


	if(node == NULL)
	{
		if(trace_level > 0)
		{
			trace("!! End of MIB\n\n");
		}

		SSAOidZero(suffix);
		return SNMP_ERR_NOSUCHNAME;
	}

	if(trace_level > 0)
	{
		trace("!! Trying %s with suffix %s\n\n",
			node->label, SSAOidString(suffix));
	}

	switch(node->type)
	{
		case OBJECT:
			object = node->data.object;

			switch(suffix->len)
			{
				case 0:
					if( !(object->access & READ_FLAG) )
					{
						return SNMP_ERR_NOSUCHNAME;
					}
						
					switch(object->asn1_type)
					{
						case INTEGER:
						case COUNTER:
						case GAUGE:
						case TIMETICKS:
							snmp_error = (*(object->get))(&integer);
							break;

						case OBJID:
							snmp_error = (*(object->get))(&oid);
							if(snmp_error != SNMP_ERR_NOERROR  &&
							   ssa_mem_free != 0){
								if(object->dealloc != NULL)
								  (*(object->dealloc))(&oid);
							}
						break;

						case STRING:
						case IPADDRESS:
						case OPAQUE:
							snmp_error = (*(object->get))(&string);
							if(snmp_error != SNMP_ERR_NOERROR  &&
							   ssa_mem_free != 0){
								if(object->dealloc != NULL)
								  (*(object->dealloc))(&string);
							}
							break;
					}

					if(snmp_error != SNMP_ERR_NOERROR)
					{
						if(snmp_error < 0)
						{
							error("the get() method of %s returned %d",
								node->label,
								snmp_error);
							snmp_error = SNMP_ERR_GENERR;
						}
						return snmp_error;
					}

					/* variable->name */
					SSAOidZero(&(variable->name));
					variable->name.subids = (Subid *) malloc((object->name.len + 1) * sizeof(Subid));
					memcpy(variable->name.subids, object->name.subids, object->name.len * sizeof(Subid));
					variable->name.subids[object->name.len] = 0;
					variable->name.len = object->name.len + 1;

					/* variable->type */
					variable->type = object->asn1_type;

					/* variable->val, variable->val_len */
					switch(object->asn1_type)
					{
						case INTEGER:
						case COUNTER:
						case GAUGE:
						case TIMETICKS:
							variable->val.integer = (Integer *) malloc(sizeof(Integer));
							*(variable->val.integer) = integer;
							variable->val_len = sizeof(Integer);
							break;

						case OBJID:
							variable->val.objid = (Subid *) malloc(oid.len * sizeof(Subid));
							memcpy(variable->val.objid, oid.subids, oid.len * sizeof(Subid));
							variable->val_len = oid.len * sizeof(Subid);
							if(ssa_mem_free !=0 && object->dealloc != NULL)
								  (*(object->dealloc))(&oid);
							break;

						case STRING:
						case IPADDRESS:
						case OPAQUE:
							variable->val.string = (u_char *) malloc(string.len);
							memcpy(variable->val.string, string.chars, string.len);
							variable->val_len = string.len;
							if(ssa_mem_free != 0 && object->dealloc != NULL)
								  (*(object->dealloc))(&string);
							break;

					}

					return SNMP_ERR_NOERROR;


				case 1:
					if(suffix->subids[0] != 0)
					{
						SSAOidZero(suffix);
						return SNMP_ERR_NOSUCHNAME;
					}
					SSAOidZero(suffix);

					return agent_get_next_loop(variable, node->next, suffix);

				default:
					SSAOidZero(suffix);
					return SNMP_ERR_NOSUCHNAME;
			}


		case COLUMN:
			column = node->data.column;
			entry = column->entry;

			if( !(column->access & READ_FLAG) )
			{
				SSAOidZero(suffix);
				return SNMP_ERR_NOSUCHNAME;
			}

			switch(entry->n_indexs)
			{
				case 1:
					switch(suffix->len)
					{
						case 0:
							index1 = 0;
							
							index_obj.len=1;
							index_obj.value[0]=0;

							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
							}
							else
							{
	                                                 if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                             if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
                                        				(*(cache_input_entry->dealloc))(cache_output_pointer);
				                                        cache_output_pointer = NULL;
		                                             }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								snmp_error =
								(*(entry->get))(FIRST_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
							}
							break;

						case 1:
							index1 = suffix->subids[0];

							index_obj.len = 1;
							index_obj.value[0] = index1;
					
							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
							}
							else
							{
	                                                  if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                              if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                      (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                      cache_output_pointer = NULL;
		                                              }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
							}
							SSAOidZero(suffix);
							break;

						default:
							SSAOidZero(suffix);
							return SNMP_ERR_NOSUCHNAME;
					}
					break;

				case 2:
					switch(suffix->len)
					{
						case 0:
							index1 = 0;
							index2 = 0;

							index_obj.len = 2;
							index_obj.value[0] = 0;
							index_obj.value[1] = 0;

							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
							}
							else
							{
	                                                   if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                              if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                    (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                    cache_output_pointer = NULL;
		                                              }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								snmp_error =
								(*(entry->get))(FIRST_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
							}
							break;

						case 1:
							index1 = suffix->subids[0];
							index2 = 0;
							index_obj.len = 2;
							index_obj.value[0] = index1;
							index_obj.value[1] = index2;
							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
							}
							else
							{
	                                                     if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                                if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                       (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                       cache_output_pointer = NULL;
		                                                }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
							}
							SSAOidZero(suffix);
							break;

						case 2:
							index1 = suffix->subids[0];
							index2 = suffix->subids[1];
					
							index_obj.len = 2;
							index_obj.value[0] = suffix->subids[0];
							index_obj.value[1] = suffix->subids[1];

							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
							}
							else
							{
	                                                   if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                                 if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                       (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                       cache_output_pointer = NULL;
		                                                 }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
							}
							SSAOidZero(suffix);
							break;

						default:
							SSAOidZero(suffix);
							return SNMP_ERR_NOSUCHNAME;
					}
					break;

				case 3:
					switch(suffix->len)
					{
						case 0:
							index1 = 0;
							index2 = 0;
							index3 = 0;

							index_obj.len = 3;
							index_obj.value[0] = 0;
							index_obj.value[1] = 0;
							index_obj.value[2] = 0;
					
							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2)
							 && (cache_input_index3 == index3) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
								index3 = cache_output_index3;
							}
							else
							{
	                                                   if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                                if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                     (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                     cache_output_pointer = NULL;
		                                                }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								cache_input_index3 = index3;
								snmp_error =
								(*(entry->get))(FIRST_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								index3 = index_obj.value[2];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
								cache_output_index3 = index3;
							}
							break;

						case 1:
							index1 = suffix->subids[0];
							index2 = 0;
							index3 = 0;

							index_obj.len = 3;
							index_obj.value[0] = suffix->subids[0];
							index_obj.value[1] = 0;
							index_obj.value[2] = 0;
							
							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2)
							 && (cache_input_index3 == index3) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
								index3 = cache_output_index3;
							}
							else
							{
	                                                  if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                             if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                  (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                  cache_output_pointer = NULL;
		                                             }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								cache_input_index3 = index3;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								index3 = index_obj.value[2];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
								cache_output_index3 = index3;
							}
							SSAOidZero(suffix);
							break;

						case 2:
							index1 = suffix->subids[0];
							index2 = suffix->subids[1];
							index3 = 0;

							index_obj.len = 3;
							index_obj.value[0] = suffix->subids[0];
							index_obj.value[1] = suffix->subids[1];
							index_obj.value[2] = 0;

							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2)
							 && (cache_input_index3 == index3) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
								index3 = cache_output_index3;
							}
							else
							{
	                                                   if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                                if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                     (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                     cache_output_pointer = NULL;
		                                                }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								cache_input_index3 = index3;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								index3 = index_obj.value[2];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
								cache_output_index3 = index3;
							}
							SSAOidZero(suffix);
							break;

						case 3:
							index1 = suffix->subids[0];
							index2 = suffix->subids[1];
							index3 = suffix->subids[2];

							index_obj.len = 3;
							index_obj.value[0] = suffix->subids[0];
							index_obj.value[1] = suffix->subids[1];
							index_obj.value[2] = suffix->subids[2];

							if( (cache_input_entry == entry)
							 && (cache_input_index1 == index1)
							 && (cache_input_index2 == index2)
							 && (cache_input_index3 == index3) )
							{
								pointer = cache_output_pointer;
								snmp_error = cache_output_snmp_error;
								index1 = cache_output_index1;
								index2 = cache_output_index2;
								index3 = cache_output_index3;
							}
							else
							{
	                                                  if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                            if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                                  (*(cache_input_entry->dealloc))(cache_output_pointer);
				                                  cache_output_pointer = NULL;
		                                            }
								cache_input_entry = entry;
								cache_input_index1 = index1;
								cache_input_index2 = index2;
								cache_input_index3 = index3;
								snmp_error =
								(*(entry->get))(NEXT_ENTRY,&pointer,
									&index_obj);
								index1 = index_obj.value[0];
								index2 = index_obj.value[1];
								index3 = index_obj.value[2];
								cache_output_pointer = pointer;
								cache_output_snmp_error = snmp_error;
								cache_output_index1 = index1;
								cache_output_index2 = index2;
								cache_output_index3 = index3;
							}
							SSAOidZero(suffix);
							break;

						default:
							SSAOidZero(suffix);
							return SNMP_ERR_NOSUCHNAME;
					}
					break;
			}

			if(pointer == NULL)
			{
				if(snmp_error == END_OF_TABLE)
				{
					if(trace_level > 0)
					{
						trace("!! End of table %s\n\n",
							node->parent->parent->label);
					}

					return agent_get_next_loop(variable, node->next, suffix);
				}

				if(snmp_error < 0)
				{
					error("the get() method of %s returned %d",
						node->parent->label,
						snmp_error);
					snmp_error = SNMP_ERR_GENERR;
				}
				return snmp_error;
			}

			/* variable->name */
			SSAOidZero(&(variable->name));
			switch(entry->n_indexs)
			{
				case 1:
					variable->name.subids = (Subid *) malloc((column->name.len + 1) * sizeof(Subid));
					memcpy(variable->name.subids, column->name.subids, column->name.len * sizeof(Subid));
					variable->name.subids[column->name.len] = index1;
					variable->name.len = column->name.len + 1;
					break;

				case 2:
					variable->name.subids = (Subid *) malloc((column->name.len + 2) * sizeof(Subid));
					memcpy(variable->name.subids, column->name.subids, column->name.len * sizeof(Subid));
					variable->name.subids[column->name.len] = index1;
					variable->name.subids[column->name.len + 1] = index2;
					variable->name.len = column->name.len + 2;
					break;

				case 3:
					variable->name.subids = (Subid *) malloc((column->name.len + 3) * sizeof(Subid));
					memcpy(variable->name.subids, column->name.subids, column->name.len * sizeof(Subid));
					variable->name.subids[column->name.len] = index1;
					variable->name.subids[column->name.len + 1] = index2;
					variable->name.subids[column->name.len + 2] = index3;
					variable->name.len = column->name.len + 3;
					break;
			}

			/* variable->type */
			variable->type = column->asn1_type;

			/* variable->val, variable->val_len */
			switch(column->asn1_type)
			{
				case INTEGER:
				case COUNTER:
				case GAUGE:
				case TIMETICKS:
					integer_ptr = (Integer *) (pointer + column->offset);
					variable->val.integer = (Integer *) malloc(sizeof(Integer));
					*(variable->val.integer) = *integer_ptr;
					variable->val_len = sizeof(Integer);
					break;

				case OBJID:
					oid_ptr = (Oid *) (pointer + column->offset);
					/* fix the null subid */
					if(oid_ptr->subids == NULL){
					  variable->val.objid = NULL;
					}else{
					variable->val.objid = (Subid *) malloc(oid_ptr->len * sizeof(Subid));
					memcpy(variable->val.objid, oid_ptr->subids, oid_ptr->len * sizeof(Subid));
					}
					variable->val_len = oid_ptr->len * sizeof(Subid);
					break;

				case STRING:
				case IPADDRESS:
				case OPAQUE:
					string_ptr = (String *) (pointer + column->offset);
					if(string_ptr->chars == NULL){
					  variable->val.string =(u_char*)NULL;
					}else{
					variable->val.string = (u_char *) malloc(string_ptr->len);
					memcpy(variable->val.string, string_ptr->chars, string_ptr->len);
					}
					variable->val_len = string_ptr->len;
					break;
			}


			return SNMP_ERR_NOERROR;


		case NODE:
			return agent_get_next_loop(variable, node->next, suffix);
	}

	/* never reached */
	return -1;
}

	
/****************************************************************/

/* returns:						*/
/*	0 in case of success (the pdu should be sent	*/
/*	  back to its originator even if an SNMP error	*/
/*	  was detected)					*/
/*	-1 in case of failure (no pdu should be sent	*/
/*	  back)						*/

static int agent_get(SNMP_pdu *pdu, char *error_label)
{
	SNMP_variable *variable;
	Node *node;
	Object *object;
	Column *column;
	Entry *entry;
	Oid suffix;
	int index = 1;
	Integer integer;
	Oid oid;
	String string;
	Integer *integer_ptr;
	Oid *oid_ptr;
	String *string_ptr;
	int snmp_error;
	char *pointer;
	Subid index1;
	Subid index2;
	Subid index3;

	/* create index struct */
	IndexType index_obj;
	int index_buffer[256];

	index_obj.len = 0;
	index_obj.value = index_buffer; 


	error_label[0] = '\0';

	pdu->type = GET_RSP_MSG;

	for(variable = pdu->first_variable; variable; variable = variable->next_variable)
	{
		node = node_find(EXACT_ENTRY, &(variable->name), &suffix);
		if(node == NULL)
		{
			pdu->error_status = SNMP_ERR_NOSUCHNAME;
			pdu->error_index = index;
			return 0;
		}
		/* we should not forget to free suffix.subids */

		if(trace_level > 0)
		{
			trace("!! get(): processing the variable %s\n\n",
				node->label);
		}

		if(variable->type != NULLOBJ)
		{
			error("agent_get(): ASN.1 type (0x%x) is not NULL for node %s",
					variable->type,
					node->label);
			variable->type = NULLOBJ;
		}

		if(variable->val.string)
		{
			error("agent_get(): val is not NULL for node %s", node->label);
			free(variable->val.string);
			variable->val.string = NULL;
		}

		if(variable->val_len)
		{
			error("agent_get(): val_len is not 0 for node %s", node->label);
			variable->val_len = 0;
		}

		switch(node->type)
		{
			case OBJECT:
				object = node->data.object;

				if( (suffix.len != 1)
					|| (suffix.subids[0] != 0) )
				{
					pdu->error_status = SNMP_ERR_NOSUCHNAME;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 0;
				}

				if( !(object->access & READ_FLAG) )
				{
					pdu->error_status = SNMP_ERR_NOSUCHNAME;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 0;
				}

				switch(object->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
						snmp_error = (*(object->get))(&integer);
						break;

					case OBJID:
						snmp_error = (*(object->get))(&oid);
                                                if(snmp_error != SNMP_ERR_NOERROR  &&
                                                   ssa_mem_free != 0){
                                                       if(object->dealloc != NULL)
                                                         (*(object->dealloc))(&oid);
						}
						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						snmp_error = (*(object->get))(&string);
                                                if(snmp_error != SNMP_ERR_NOERROR  &&
                                                   ssa_mem_free != 0){
                                                       if(object->dealloc != NULL)
                                                         (*(object->dealloc))(&string);
						}
						break;
				}

				if(snmp_error != SNMP_ERR_NOERROR)
				{
					if(snmp_error < 0)
					{
						error("the get() method of %s returned %d",
							node->label,
							snmp_error);
						snmp_error = SNMP_ERR_GENERR;
					}
					pdu->error_status = snmp_error;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 0;
				}

				/* variable->name */

				/* variable->type */
				variable->type = object->asn1_type;

				/* variable->val, variable->val_len */
				switch(object->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
						variable->val.integer = (Integer *) malloc(sizeof(Integer));
						*(variable->val.integer) = integer;
						variable->val_len = sizeof(Integer);
						break;

					case OBJID:
						variable->val.objid = (Subid *) malloc(oid.len * sizeof(Subid));
						memcpy(variable->val.objid, oid.subids, oid.len * sizeof(Subid));
						variable->val_len = oid.len * sizeof(Subid);
                                                /*if(snmp_error != SNMP_ERR_NOERROR  &&*/
                                                  if (ssa_mem_free != 0){
                                                       if(object->dealloc != NULL)
                                                         (*(object->dealloc))(&oid);
						}
						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						variable->val.string = (u_char *) malloc(string.len);
						memcpy(variable->val.string, string.chars, string.len);
						variable->val_len = string.len;
                                                /*if(snmp_error != SNMP_ERR_NOERROR  &&*/
                                                  if (ssa_mem_free != 0){
                                                       if(object->dealloc != NULL)
                                                         (*(object->dealloc))(&string);
						}
						break;
				}


				break;


			case COLUMN:
				column = node->data.column;
				entry = column->entry;

				if( !(column->access & READ_FLAG) )
				{
					pdu->error_status = SNMP_ERR_NOSUCHNAME;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 0;
				}

				switch(entry->n_indexs)
				{
					case 1:
						if(suffix.len != 1)
						{
							pdu->error_status = SNMP_ERR_NOSUCHNAME;
							pdu->error_index = index;
							SSAOidZero(&suffix);
							return 0;
						}

						index1 = suffix.subids[0];

						index_obj.len = 1;
						index_obj.value[0] = suffix.subids[0];

						if( (cache_input_entry == entry)
						 && (cache_input_index1 == index1) )
						{
							pointer = cache_output_pointer;
							snmp_error = cache_output_snmp_error;
						}
						else
						{
	                                          if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                      if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                          (*(cache_input_entry->dealloc))(cache_output_pointer);
				                          cache_output_pointer = NULL;
		                                      }
							cache_input_entry = entry;
							cache_input_index1 = index1;
							snmp_error =
							(*(entry->get))(EXACT_ENTRY,&pointer,&index_obj);
							cache_output_pointer = pointer;
							cache_output_snmp_error = snmp_error;
						}
						break;

					case 2:
						if(suffix.len != 2)
						{
							pdu->error_status = SNMP_ERR_NOSUCHNAME;
							pdu->error_index = index;
							SSAOidZero(&suffix);
							return 0;
						}

						index1 = suffix.subids[0];
						index2 = suffix.subids[1];

						index_obj.len = 2;
						index_obj.value[0] = suffix.subids[0];
						index_obj.value[1] = suffix.subids[1];

						if( (cache_input_entry == entry)
						 && (cache_input_index1 == index1)
						 && (cache_input_index2 == index2) )
						{
							pointer = cache_output_pointer;
							snmp_error = cache_output_snmp_error;
						}
						else
						{
	                                          if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                        if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                             (*(cache_input_entry->dealloc))(cache_output_pointer);
				                             cache_output_pointer = NULL;
		                                        }
							cache_input_entry = entry;
							cache_input_index1 = index1;
							cache_input_index2 = index2;
							snmp_error =
							(*(entry->get))(EXACT_ENTRY,&pointer,&index_obj);
							cache_output_pointer = pointer;
							cache_output_snmp_error = snmp_error;
						}
						break;

					case 3:
						if(suffix.len != 3)
						{
							pdu->error_status = SNMP_ERR_NOSUCHNAME;
							pdu->error_index = index;
							SSAOidZero(&suffix);
							return 0;
						}

						index1 = suffix.subids[0];
						index2 = suffix.subids[1];
						index3 = suffix.subids[2];

						index_obj.len = 3;
						index_obj.value[0] = suffix.subids[0];
						index_obj.value[1] = suffix.subids[1];
						index_obj.value[2] = suffix.subids[2];

						if( (cache_input_entry == entry)
						 && (cache_input_index1 == index1)
						 && (cache_input_index2 == index2)
						 && (cache_input_index3 == index3) )
						{
							pointer = cache_output_pointer;
							snmp_error = cache_output_snmp_error;
						}
						else
						{
	                                            if(cache_input_entry != NULL && cache_output_pointer != NULL)
		                                        if(ssa_mem_free != 0 && cache_input_entry->dealloc != NULL){
				                             (*(cache_input_entry->dealloc))(cache_output_pointer);
				                             cache_output_pointer = NULL;
		                                        }
							cache_input_entry = entry;
							cache_input_index1 = index1;
							cache_input_index2 = index2;
							cache_input_index3 = index3;
							snmp_error =
							(*(entry->get))(EXACT_ENTRY,&pointer,&index_obj);
							cache_output_pointer = pointer;
							cache_output_snmp_error = snmp_error;
						}
						break;
				}

				if(pointer == NULL)
				{
					if(snmp_error < 0)
					{
						error("the get() method of %s returned %d",
							node->parent->label,
							snmp_error);
						snmp_error = SNMP_ERR_GENERR;
					}
					
					pdu->error_status = snmp_error;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 0;
				}

				/* variable->name */

				/* variable->type */
				variable->type = column->asn1_type;

				/* variable->val, variable->val_len */
				switch(column->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
						integer_ptr = (Integer *) (pointer + column->offset);
						variable->val.integer = (Integer *) malloc(sizeof(Integer));
						*(variable->val.integer) = *integer_ptr;
						variable->val_len = sizeof(Integer);
						break;

					case OBJID:
						oid_ptr = (Oid *) (pointer + column->offset);
						variable->val.objid = (Subid *) malloc(oid_ptr->len * sizeof(Subid));
						memcpy(variable->val.objid, oid_ptr->subids, oid_ptr->len * sizeof(Subid));
						variable->val_len = oid_ptr->len * sizeof(Subid);
						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						string_ptr = (String *) (pointer + column->offset);
						variable->val.string = (u_char *) malloc(string_ptr->len);
						memcpy(variable->val.string, string_ptr->chars, string_ptr->len);
						variable->val_len = string_ptr->len;
						break;
				}
				if(ssa_mem_free != 0 && entry->dealloc != NULL){
					/* remember to turn off the caching */
					(*(entry->dealloc))(pointer);
					pointer = NULL;
				}

				break;


			case NODE:
				pdu->error_status = SNMP_ERR_NOSUCHNAME;
				pdu->error_index = index;
				SSAOidZero(&suffix);
				return 0;
		}

		SSAOidZero(&suffix);

		index++;
	}

	return 0;
}


/****************************************************************/

/* returns							*/
/*	0	in case of success. If we are in the		*/
/*		FIRST_PASS, we should go to the second pass.	*/
/*	-1	in case of failure. If we are in the		*/
/*		FIRST_PASS, we should not go to the		*/
/*		second pass and no pdu should be sent back.	*/
/*	1	If we are in the FIRST_PASS, we should not go	*/
/*		to the second pass but a pdu			*/
/*		with an SNMP error should be sent back to its	*/
/*		originator					*/

static int agent_set(int pass, SNMP_pdu *pdu, char *error_label)
{
	SNMP_variable *variable;
	Node *node;
	Object *object;
	Column *column;
	Entry *entry;
	Oid suffix;
	int index = 1;
	Integer integer = 0;
	Oid oid = { NULL, 0 };
	String string = { NULL, 0 };
	int snmp_error;
	Subid index1;
	Subid index2;
	Subid index3;


	/* create index struct */
	IndexType index_obj;
	int index_buffer[256];

	index_obj.len = 0;
	index_obj.value = index_buffer; 

	error_label[0] = '\0';

	pdu->type = GET_RSP_MSG;

	for(variable = pdu->first_variable; variable; variable = variable->next_variable)
	{
		node = node_find(EXACT_ENTRY, &(variable->name), &suffix);
		if(node == NULL)
		{
			pdu->error_status = SNMP_ERR_NOSUCHNAME;
			pdu->error_index = index;
			return 1;
		}
		/* we should not forget to free suffix.subids */

		if(trace_level > 0)
		{
			trace("!! set(%s): processing the variable %s\n\n",
				(pass == FIRST_PASS)? "FIRST_PASS": "SECOND_PASS",
				node->label);
		}

/*
		if(variable->val.string == NULL)
		{
			sprintf(error_label, "val.string is NULL for node %s",
				node->label);
			SSAOidZero(&suffix);
			return -1;
		}

		if(variable->val_len == 0)
		{
			sprintf(error_label, "val_len is 0 for node %s",
				node->label);
			SSAOidZero(&suffix);
			return -1;
		}
*/

		switch(node->type)
		{
			case OBJECT:
				object = node->data.object;

				/* check the ASN.1 type */
				if(variable->type != object->asn1_type)
				{
					sprintf(error_label, "wrong ASN.1 type (0x%x) for node %s",
						variable->type, node->label);
					SSAOidZero(&suffix);
					return -1;
				}

				/* check the suffix */
				if( (suffix.len != 1)
					|| (suffix.subids[0] != 0) )
				{
					pdu->error_status = SNMP_ERR_NOSUCHNAME;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				/* check the access */
				if( !(object->access & WRITE_FLAG) )
				{
					pdu->error_status = SNMP_ERR_READONLY;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				/* check the value length */
				switch(object->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
					case IPADDRESS:
						if(variable->val_len != 4)
						{
							sprintf(error_label, "val_len is not 4 (%d) for node %s",
								variable->val_len, node->label);
							SSAOidZero(&suffix);
							return -1;
						}

						if(variable->val.integer == NULL)
						{
							sprintf(error_label, "val.integer is NULL for node %s",
								node->label);
							SSAOidZero(&suffix);
							return -1;
						}

						break;
				}

				/* in case of enumerated integer, check the value */
				if( (object->asn1_type == INTEGER)
					&& (object->first_enum != NULL) )
				{
					Enum *enums;


					integer = *(variable->val.integer);

					for(enums = object->first_enum; enums; enums = enums->next_enum)
					{
						if(enums->value == integer)
						{
							break;
						}
					}

					if(enums == NULL)
					{
						pdu->error_status = SNMP_ERR_BADVALUE;
						pdu->error_index = index;
						SSAOidZero(&suffix);
						return 1;
					}
				}

				switch(object->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
						integer = *(variable->val.integer);

						snmp_error = (*(object->set))(pass, &integer);
						break;

					case OBJID:
						if(SSAOidInit(&oid, variable->val.objid, variable->val_len / sizeof(Subid), error_label))
						{
							SSAOidZero(&suffix);
							return -1;
						}

						snmp_error = (*(object->set))(pass, &oid);
						SSAOidZero(&oid);
						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						if(SSAStringInit(&string, variable->val.string, variable->val_len, error_label))
						{
							SSAOidZero(&suffix);
							return -1;
						}

						snmp_error = (*(object->set))(pass, &string);
						SSAStringZero(&string);
						break;
				}

				if(snmp_error != SNMP_ERR_NOERROR)
				{
					if(snmp_error < 0)
					{
						error("the set(%s) method of %s returned %d",
							(pass == FIRST_PASS)? "FIRST_PASS": "SECOND_PASS",
							node->label, snmp_error);
						snmp_error = SNMP_ERR_GENERR;
					}
					pdu->error_status = snmp_error;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				break;


			case COLUMN:
				column = node->data.column;
				entry = column->entry;

				/* check the ASN.1 type */
				if(variable->type != column->asn1_type)
				{
					sprintf(error_label, "wrong ASN.1 type (0x%x) for node %s",
						variable->type, node->label);
					SSAOidZero(&suffix);
					return -1;
				}

				/* check the suffix */
				if(suffix.len != entry->n_indexs)
				{
					pdu->error_status = SNMP_ERR_NOSUCHNAME;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				/* check the access */
				if( !(column->access & WRITE_FLAG) )
				{
					pdu->error_status = SNMP_ERR_READONLY;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				/* check the value length */
				switch(column->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
					case IPADDRESS:
						if(variable->val_len != 4)
						{
							sprintf(error_label, "val_len is not 4 (%d) for node %s",
								variable->val_len, node->label);
							SSAOidZero(&suffix);
							return -1;
						}

						if(variable->val.integer == NULL)
						{
							sprintf(error_label, "val.integer is NULL for node %s",
								node->label);
							SSAOidZero(&suffix);
							return -1;
						}

						break;
				}

				/* in case of enumerated integer, check the value */
				if( (column->asn1_type == INTEGER)
					&& (column->first_enum != NULL) )
				{
					Enum *enums;


					integer = *(variable->val.integer);

					for(enums = column->first_enum; enums; enums = enums->next_enum)
					{
						if(enums->value == integer)
						{
							break;
						}
					}

					if(enums == NULL)
					{
						pdu->error_status = SNMP_ERR_BADVALUE;
						pdu->error_index = index;
						SSAOidZero(&suffix);
						return 1;
					}
				}

				switch(column->asn1_type)
				{
					case INTEGER:
					case COUNTER:
					case GAUGE:
					case TIMETICKS:
						integer = *(variable->val.integer);

						break;

					case OBJID:
						if(SSAOidInit(&oid, variable->val.objid, variable->val_len / sizeof(Subid), error_label))
						{
							SSAOidZero(&suffix);
							return -1;
						}

						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						if(SSAStringInit(&string, variable->val.string, variable->val_len, error_label))
						{
							SSAOidZero(&suffix);
							return -1;
						}

						break;
				}

				switch(entry->n_indexs)
				{
					case 1:
						index1 = suffix.subids[0];

						index_obj.len = 1;
						index_obj.value[0] = suffix.subids[0];

						switch(column->asn1_type)
						{
							case INTEGER:
							case COUNTER:
							case GAUGE:
							case TIMETICKS:
								snmp_error = (*(column->set))(pass, index_obj, &integer);
								break;

							case OBJID:
								snmp_error = (*(column->set))(pass, index_obj, &oid);
								break;

							case STRING:
							case IPADDRESS:
							case OPAQUE:
								snmp_error = (*(column->set))(pass, index_obj, &string);
								break;
						}

						break;

					case 2:
						index1 = suffix.subids[0];
						index2 = suffix.subids[1];

						index_obj.len = 2;
						index_obj.value[0] = suffix.subids[0];
						index_obj.value[1] = suffix.subids[1];

						switch(column->asn1_type)
						{
							case INTEGER:
							case COUNTER:
							case GAUGE:
							case TIMETICKS:
								snmp_error = (*(column->set))(pass, index_obj, &integer);
								break;

							case OBJID:
								snmp_error = (*(column->set))(pass, index_obj, &oid);
								break;

							case STRING:
							case IPADDRESS:
							case OPAQUE:
								snmp_error = (*(column->set))(pass, index_obj, &string);
								break;
						}

						break;

					case 3:
						index1 = suffix.subids[0];
						index2 = suffix.subids[1];
						index3 = suffix.subids[2];

						index_obj.len = 3;
						index_obj.value[0] = suffix.subids[0];
						index_obj.value[1] = suffix.subids[1];
						index_obj.value[2] = suffix.subids[2];
						
						switch(column->asn1_type)
						{
							case INTEGER:
							case COUNTER:
							case GAUGE:
							case TIMETICKS:
								snmp_error = (*(column->set))(pass, index_obj, &integer);
								break;

							case OBJID:
								snmp_error = (*(column->set))(pass, index_obj, &oid);
								break;

							case STRING:
							case IPADDRESS:
							case OPAQUE:
								snmp_error = (*(column->set))(pass, index_obj, &string);
								break;
						}

						break;
				}

				switch(column->asn1_type)
				{
					case OBJID:
						SSAOidZero(&oid);
						break;

					case STRING:
					case IPADDRESS:
					case OPAQUE:
						SSAStringZero(&string);
						break;
				}

				if(snmp_error != SNMP_ERR_NOERROR)
				{
					if(snmp_error < 0)
					{
						error("the set(%s) method of %s returned %d",
							(pass == FIRST_PASS)? "FIRST_PASS": "SECOND_PASS",
							node->parent->label,
							snmp_error);
							snmp_error = SNMP_ERR_GENERR;
					}
					
					pdu->error_status = snmp_error;
					pdu->error_index = index;
					SSAOidZero(&suffix);
					return 1;
				}

				break;


			case NODE:
				pdu->error_status = SNMP_ERR_NOSUCHNAME;
				pdu->error_index = index;
				SSAOidZero(&suffix);
				return 1;
		}

		SSAOidZero(&suffix);

		index++;
	}

	return 0;
}


/****************************************************************/

/* flag == 0 means turn off auto mem free */
void SSAAutoMemFree(int flag)
{
  ssa_mem_free = flag;
}


