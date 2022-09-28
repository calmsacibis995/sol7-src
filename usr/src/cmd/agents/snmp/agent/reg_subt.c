/* Copyright 10/08/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)reg_subtree.c	1.14 96/10/08 Sun Microsystems"

/* HISTORY
 * 7-2-96	Jerry Yeung	add system up time
 * 7-16-96	Jerry Yeung	change API func name
 * 8-28-96	Jerry Yeung	add Trap function
 * 8-30-96	Jerry Yeung	define extern var.
 */


#include <unistd.h>
#include <sys/types.h>
#include <sys/times.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <nlist.h>

#include "snmp_msg.h"
#include "impl.h"
#include "error.h"
#include "trace.h"
#include "snmp.h"
#include "pdu.h"
#include "request.h"
#include "pagent.h"
#include "subtree.h"
#include "table.h"

#include "node.h"

#include "oid_ar_p.h"

#define REG_AGENT_ID		1
#define REG_AGENT_STATUS	2
#define REG_AGENT_TIME_OUT	3
#define REG_AGENT_PORT_NUMBER	4
#define REG_AGENT_PERSONAL_FILE	5	
#define REG_AGENT_CONFIG_FILE	6	
#define REG_AGENT_EXECUTABLE	7
#define REG_AGENT_VERSION_NUM	8
#define REG_AGENT_PROCESS_ID	9
#define REG_AGENT_NAME		10
#define REG_AGENT_SYSTEM_UP_TIME 11

#define REG_TREE_INDEX		1
#define REG_TREE_AGENT_ID	2
#define REG_TREE_OID		3
#define REG_TREE_STATUS		4

#define REG_TBL_INDEX		1
#define REG_TBL_AGENT_ID	2
#define REG_TBL_OID		3
#define REG_TBL_SCOL	 	4
#define REG_TBL_ECOL		5
#define REG_TBL_SROW		6
#define REG_TBL_EROW		7
#define REG_TBL_STATUS		8

#define FAIL 0
#define SUCCESS 1

#define INVALID_HANDLER		0

/* global variable */
struct CallbackItem *callItem=NULL;
int numCallItem=0;
int *trapTableMap=NULL;
struct TrapHndlCxt *trapBucket=NULL;
int numTrapElem=0;


int sap_glb_agent_table_id()
{
  /* agent table id can be: from command line, global var. sap_agent_tbl_id,
     or from getpid() 
     The agent_id can be less than one.
   */
  return((int)getpid());
}

SNMP_variable *sap_append_integer_variable(SNMP_variable *list, Oid *oid, int num)
{
  SNMP_value value;

  value.v_integer = num;
  list = snmp_typed_variable_append(list,oid,INTEGER,&value,error_label);
  if(list == NULL){
	error("sap_append_integer_variable failed: oid: %s, value: %d\n",
		SSAOidString(oid),num);
  }
  return(list);
}

SNMP_variable *sap_append_variable(SNMP_variable *list, Oid *oid, int num, int type)
{
  SNMP_value value;

  value.v_integer = num;
  list = snmp_typed_variable_append(list,oid,type,&value,error_label);
  if(list == NULL){
	error("sap_append_variable failed: oid: %s, value: %d, type: %d\n",
		SSAOidString(oid),num,type);
  }
  return(list);
}

SNMP_variable *sap_append_string_variable(SNMP_variable *list, Oid *oid, char* str)
{
  SNMP_value value;

  if(str == NULL) return NULL;
  value.v_string.chars = (u_char *)str;
  value.v_string.len = strlen(str);
  list = snmp_typed_variable_append(list,oid,STRING,&value,error_label);
  if(list == NULL){
	error("sap_append_string_variable failed: oid: %s, value: %s\n"
		, SSAOidString(oid),str);
  }
  return(list);
}

SNMP_variable *sap_append_oid_variable(SNMP_variable *list, Oid *oid, Oid* name)
{
  SNMP_value value;

  if(oid == NULL || name == NULL) return NULL;
  value.v_oid.subids = name->subids;
  value.v_oid.len = name->len;	
  list = snmp_typed_variable_append(list,oid,OBJID,&value,error_label);
  if(list == NULL){
	error("sap_append_oid_varaible(%s,%s) failed\n",
		SSAOidString(oid),SSAOidString(name));
  }
  return(list);
}


SNMP_variable *reg_subagent_form_variables(Agent *agent,SNMP_variable *list)
{
  struct tms buffer;
  int system_up_time;

  if(agent == NULL){ return NULL; }

  /* set up the index */
  reg_subagent_oid.subids[reg_subagent_oid.len-1] = agent->agent_id;

  reg_subagent_oid.subids[reg_subagent_oid.len-2] = REG_AGENT_PROCESS_ID;
  list = sap_append_integer_variable(list,&reg_subagent_oid,agent->process_id);
  if(list == NULL) return list;

  if(agent->agent_status!=0){
    reg_subagent_oid.subids[reg_subagent_oid.len-2] = REG_AGENT_STATUS;
    list = sap_append_integer_variable(list,&reg_subagent_oid,agent->agent_status);
    if(list == NULL) return list;
    reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
		REG_AGENT_SYSTEM_UP_TIME;
    system_up_time = (Integer)times(&buffer);
    list = sap_append_variable(list,&reg_subagent_oid,system_up_time,TIMETICKS);
    if(list == NULL) return list;
  }
  
  reg_subagent_oid.subids[reg_subagent_oid.len-2] = REG_AGENT_TIME_OUT;
  list = sap_append_integer_variable(list,&reg_subagent_oid,agent->timeout);
  if(list == NULL) return list;

  reg_subagent_oid.subids[reg_subagent_oid.len-2] = REG_AGENT_PORT_NUMBER;
  list = sap_append_integer_variable(list,&reg_subagent_oid,agent->address.sin_port);
  if(list == NULL) return list;

  if(agent->personal_file != NULL){
  	reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
				REG_AGENT_PERSONAL_FILE;
  	list = sap_append_string_variable(list,&reg_subagent_oid,agent->personal_file);
  	if(list == NULL) return list;
  }

  if(agent->config_file != NULL) {
  	reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
				REG_AGENT_CONFIG_FILE;
  	list = sap_append_string_variable(list,&reg_subagent_oid,agent->config_file);
  	if(list == NULL) return list;
  }

  if(agent->executable != NULL) {
  	reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
				REG_AGENT_EXECUTABLE;
  	list = sap_append_string_variable(list,&reg_subagent_oid,agent->executable);
  	if(list == NULL) return list;
  }

  if(agent->version_string != NULL) {
  	reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
				REG_AGENT_VERSION_NUM;
  	list = sap_append_string_variable(list,&reg_subagent_oid,agent->version_string);
  	if(list == NULL) return list;
  }

  if(agent->name != NULL) {
  	reg_subagent_oid.subids[reg_subagent_oid.len-2] = 
				REG_AGENT_NAME;
  	list = sap_append_string_variable(list,&reg_subagent_oid,agent->name);
  	if(list == NULL) return list;
  }


  return(list);

}

/*
 * temporary using the process id as the agent id
 * set the following variables, 
 *	xtimeout 
 *	xportnumber
 *	xpersonal file
 *	xconfig file
 *	executable
 *	version number
 *	protocol
 *	xprocess id
 *	xagentname
 */
static SNMP_pdu *send_request_to_agent(SNMP_variable *list, int type,
		char* community, int port, struct timeval *timeout,
		IPAddress *agent_addr)
{
	SNMP_pdu *request, *response=NULL;

	error_label[0] = '\0';


	request = request_create( (community==NULL?"public":community), type, error_label);
	if(request == NULL)
	{
		return NULL;
	}

	if(list == NULL){
		snmp_pdu_free(request);
		error("SSARegSubagent failed\n");
		return NULL;
	}

	request->first_variable = list;

	/* later change the timeout to user-provided number */
	response =
	     request_send_to_port_time_out_blocking(agent_addr, port,timeout, request, error_label);
	snmp_pdu_free(request);
	return response;
}

SNMP_pdu *send_request_to_relay_agent(SNMP_variable *list, int type)
{
  struct timeval timeout;
  static int my_ip_address_initialized = False;
  static IPAddress my_ip_address;

  if(my_ip_address_initialized == False)
  {
	if(get_my_ip_address(&my_ip_address, error_label))
	{
		return NULL;
	}
	my_ip_address_initialized = True;
  }
  timeout.tv_sec = 100;
  timeout.tv_usec = 0;
  return(send_request_to_agent(list,type,"public",SNMP_PORT,&timeout,&my_ip_address));
}
 

static SNMP_variable *create_variable(Oid *name)
{
	SNMP_variable *new;
	if((new = snmp_variable_new(error_label)) == NULL)
		return NULL;
	if(SSAOidCpy(&(new->name),name,error_label))
	{
		snmp_variable_free(new);
		return NULL;
	}
	new->type = NULLOBJ;
 	return new;
}

int sap_avail_index(Oid *name, int type)
{
  	SNMP_pdu *response;
	SNMP_variable *new, *variable;
	int idx = 0;

	if( (new = create_variable(name)) == NULL )
		return idx;
	response = send_request_to_relay_agent(new,type);
	if(response == NULL)
	{
		return idx;
	}

	if(response->error_status)
	{
		sprintf(error_label, "%s",
			error_status_string(response->error_status));
		snmp_pdu_free(response);
		return idx;
	}

  /* need checking the response */
	variable = response->first_variable;
	if(SSAOidCmp(&(variable->name), name)
		|| (variable->type != INTEGER)
		|| (variable->val.integer == NULL)
		|| (variable->val_len != sizeof(long)) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return idx;
	}
	idx = *(variable->val.integer);
	snmp_pdu_free(response);
	/*snmp_variable_free(new); */
	return(idx);
}

/* 
 * send a request to an agent, return TRUE, if agent response
 * otherwise, FALSE
 */
static int probe_agent(Oid *name, int type, char *community, int port, 
		struct timeval *timeout,IPAddress *agent_addr)
{
  	SNMP_pdu *response;
	SNMP_variable *new, *variable;
 	int res;
  	static int my_ip_address_initialized = False;
  	static IPAddress my_ip_address;

  	if(my_ip_address_initialized == False)
  	{
		if(get_my_ip_address(&my_ip_address, error_label))
		{
			return NULL;
		}
		my_ip_address_initialized = True;
  	}

	if( (new = create_variable(name)) == NULL )
		return -1;
	response = send_request_to_agent(new,type,community,port,timeout,
				agent_addr!=NULL?agent_addr:&my_ip_address);
	if(response == NULL)
	{
		return FALSE;
	}


  /* need checking the response */
	variable = response->first_variable;
	if(SSAOidCmp(&(variable->name), name) )
	{
		snmp_pdu_free(response);
		return FALSE;
	}
	snmp_pdu_free(response);
	snmp_variable_free(new);
	return(TRUE);
}

/* 
 * agent_addr==NULL => use the local host address
 * community==NULL => public
 */
int SSAAgentIsAlive(IPAddress *agent_addr,int port,char* community,
	struct timeval *timeout)
{
  static Subid system_service_subids[] = {1,3,6,1,2,1,7};
  static Oid system_service_oid = { system_service_subids, 7};

  return(probe_agent(&system_service_oid,GET_REQ_MSG,
			community!=NULL?community:"public", 
			port,timeout,agent_addr));
}

int get_available_index_from_relay_agent()
{
  return(sap_avail_index(&agent_tbl_index_oid,GET_REQ_MSG));
}

int bump_index_of_relay_agent(int num)
{
  SNMP_variable *variable=NULL;
  SNMP_pdu *response;
  int idx = 0;

  variable = sap_append_integer_variable(variable,&agent_tbl_index_oid,num++);
  if(variable == NULL) return  idx;
  response = send_request_to_relay_agent(variable,SET_REQ_MSG);
  if(response == NULL)
  {
	return idx;
  }

  if(response->error_status)
  {
	sprintf(error_label, "%s",
		error_status_string(response->error_status));
	snmp_pdu_free(response);
	return idx;
  }

  /* need checking the response */
	variable = response->first_variable;
	if(SSAOidCmp(&(variable->name), &agent_tbl_index_oid)
		|| (variable->type != INTEGER)
		|| (variable->val.integer == NULL)
		|| (variable->val_len != sizeof(long)) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return idx;
	}
  idx = *(variable->val.integer);
  snmp_pdu_free(response);
  /*snmp_variable_free(variable);  This memory is already freed above*/
  return(idx);
}

int check_dup_agent_name(char *agent_name)
{
  SNMP_variable *variable=NULL;
  SNMP_pdu *response;
  int res=TRUE;

  variable = sap_append_string_variable(variable,&ra_check_point_oid,agent_name);
  if(variable == NULL) return  res;
  response = send_request_to_relay_agent(variable,SET_REQ_MSG);
  if(response == NULL)
  {
	return res;
  }

  if(response->error_status)
  {
	sprintf(error_label, "%s",
		error_status_string(response->error_status));
	snmp_pdu_free(response);
	return res;
  }

  /* need checking the response */
	variable = response->first_variable;
	if(SSAOidCmp(&(variable->name), &ra_check_point_oid)
		|| (variable->type != STRING)
		|| (variable->val.string == NULL)
		|| (variable->val_len == 0) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return res;
	}
  res = FALSE;
  snmp_pdu_free(response);
  /*snmp_variable_free(variable);this mem. is freed above*/
  return(res);
}
  

/*
 * mechanism for getting agent id
 * first, set relayCheckPoint to agent name
 * second, if first successful(no agent name dup.), get the availabe id
 */
int SSASubagentOpen(int num_of_retry,char* agent_name)
{
  int i, index;
 
  if(check_dup_agent_name(agent_name)==TRUE) return INVALID_HANDLER;
  for(i=0;i<num_of_retry;i++){
  	if( (index=get_available_index_from_relay_agent()) == 0)
		return INVALID_HANDLER;
	index++;
	if( (bump_index_of_relay_agent(index)) == index )
		return index-1;
	else if( i > num_of_retry) return INVALID_HANDLER;
  }
  return INVALID_HANDLER;
}


int SSARegSubagent(Agent* agent)
{
  SNMP_variable *variable, *list=NULL;
  SNMP_pdu *response;

  /* form variable list */
 	list = reg_subagent_form_variables(agent,list);

	if(list == NULL){
		error("SSARegSubagent failed\n");
		return 0;
  	}

	response = send_request_to_relay_agent(list,SET_REQ_MSG);
	if(response == NULL)
	{
		return 0;
	}

	if(response->error_status)
	{
		sprintf(error_label, "%s",
			error_status_string(response->error_status));
		snmp_pdu_free(response);
		return 0;
	}

  /* need checking the response
	variable = response->first_variable;
	if(SSAOidCmp(&(variable->name), tree_oid)
		|| (variable->type != OBJID)
		|| (variable->val.objid == NULL)
		|| (variable->val_len != 0) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return 0;
	}
*/
	snmp_pdu_free(response);

	return 1;
}


/* 
 * register the subtree, index is AgentID.RegTreeIndex
 * if the registration is not accepted by the relay agent,
 * the error_status is non-zero
 */
int SSARegSubtree(SSA_Subtree *subtree)
{
  Oid *tree_oid = &(subtree->name);
  int agent_id = subtree->regTreeAgentID;
  SNMP_variable *new, *list=NULL, *variable;
  SNMP_pdu *response;
  int idx = subtree->regTreeIndex;

	/* form variable list */
	reg_tree_ra_oid.subids[reg_tree_ra_oid.len-1] = idx;
	reg_tree_ra_oid.subids[reg_tree_ra_oid.len-2] = agent_id;

        if (subtree->regTreeStatus != SSA_OPER_STATUS_NOT_IN_SERVICE) {
	reg_tree_ra_oid.subids[reg_tree_ra_oid.len-3] = REG_TREE_OID;
  	list = sap_append_oid_variable(list,&reg_tree_ra_oid,tree_oid);
	if(list == NULL) return 0;
        }

	reg_tree_ra_oid.subids[reg_tree_ra_oid.len-3] = REG_TREE_STATUS;
	list = sap_append_integer_variable(list,&reg_tree_ra_oid,subtree->regTreeStatus);
	if(list == NULL) return 0;

	response = send_request_to_relay_agent(list,SET_REQ_MSG);
	if(response == NULL)
	{
		return 0;
	}

	if(response->error_status)
	{
		sprintf(error_label, "%s",
			error_status_string(response->error_status));
		snmp_pdu_free(response);
		return 0;
	}

/* needs checking
	variable = response->first_variable;
	if(variable->next_variable
		|| SSAOidCmp(&(variable->name), &reg_tree_ra_oid)
		|| (variable->type != OBJID)
		|| (variable->val_len/sizeof(Subid) != tree_oid->len) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return 0;
	}
*/
	snmp_pdu_free(response);

	return idx;

}

int SSARegSubtable(SSA_Table *table)
{
  Oid *table_oid = &(table->regTblOID);
  int agent_id = table->regTblAgentID;
  SNMP_variable *new, *list=NULL, *variable;
  SNMP_pdu *response;
  int idx = table->regTblIndex;

	/* form variable list */
	reg_shared_table_oid.subids[reg_shared_table_oid.len-1] = idx;
	reg_shared_table_oid.subids[reg_shared_table_oid.len-2] = agent_id;

	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_OID;
  	list = sap_append_oid_variable(list,&reg_shared_table_oid,table_oid);
	if(list == NULL) return 0;

	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_SCOL;
  	list = sap_append_integer_variable(list,&reg_shared_table_oid,table->regTblStartColumn);
	if(list == NULL) return 0;

	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_ECOL;
  	list = sap_append_integer_variable(list,&reg_shared_table_oid,table->regTblEndColumn);
	if(list == NULL) return 0;

	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_SROW;
  	list = sap_append_integer_variable(list,&reg_shared_table_oid,table->regTblStartRow);
	if(list == NULL) return 0;

	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_EROW;
  	list = sap_append_integer_variable(list,&reg_shared_table_oid,table->regTblEndRow);
	if(list == NULL) return 0;


	reg_shared_table_oid.subids[reg_shared_table_oid.len-3] = REG_TBL_STATUS;
  	list = sap_append_integer_variable(list,&reg_shared_table_oid,table->regTblStatus);
	if(list == NULL) return 0;

	response = send_request_to_relay_agent(list,SET_REQ_MSG);
	if(response == NULL)
	{
		return 0;
	}

	if(response->error_status)
	{
		sprintf(error_label, "%s",
			error_status_string(response->error_status));
		snmp_pdu_free(response);
		return 0;
	}

/* needs checking
	variable = response->first_variable;
	if(variable->next_variable
		|| SSAOidCmp(&(variable->name), &reg_shared_table_oid)
		|| (variable->type != OBJID)
		|| (variable->val_len/sizeof(Subid) != table_oid->len) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return 0;
	}
*/
	snmp_pdu_free(response);

	return idx;

}


int SSAGetTrapPort()
{
  /* not agent_tbl_index_oid, but trap_port_number */
  return(sap_avail_index(&ra_trap_port_oid,GET_REQ_MSG));
}

static int search_trap_num(char* name)
{
 int idx;
 if(!name) return -1;
 for(idx=0;idx<numTrapElem;idx++)
 	if(!strcmp(name,trapBucket[idx].name)) return idx;
 return -1;
}

int _SSASendTrap(char *name)
{
	IPAddress dest_ip_address, my_ip_address;
	int generic;
	int specific;
	u_long time_stamp;
	int trap_port = SSAGetTrapPort();
	SNMP_variable *variables=NULL;
	Integer val_integer;
	String val_str;
	Oid val_oid;
	int num;
	Object *ptr;
	int trapNum;

	if(!trapBucket) return -1;

	if((trapNum=search_trap_num(name))==-1) return -1;

	if(get_my_ip_address(&dest_ip_address,error_label)== -1) return -1;
	if(get_my_ip_address(&my_ip_address,error_label)== -1) return -1;

	generic = trapBucket[trapNum].generic;
	specific = trapBucket[trapNum].specific;

	num = trapTableMap[trapNum];
	ptr=callItem[num].ptr;
	while(num != -1 && ptr){
		if(ptr->asn1_type == INTEGER){
			ptr->get(&val_integer);
			variables = ssa_append_integer_variable(variables,&(ptr->name),val_integer,error_label);
		}else if(ptr->asn1_type == STRING){
			ptr->get(&val_str);
			variables = ssa_append_string_variable(variables,&(ptr->name),val_str,error_label);

		}else if(ptr->asn1_type == OBJID){
			ptr->get(&val_oid);
			variables = ssa_append_oid_variable(variables,&(ptr->name),val_oid,error_label);
		}
		num = callItem[num].next;
		if(num <0 ) ptr=NULL;
		else ptr = callItem[num].ptr;
	}


	if(trapBucket[trapNum].is_sun_enterprise){
	  if(trap_send_with_more_para(&dest_ip_address,my_ip_address,1,&sun_oid,generic,specific,trap_port,time_stamp,variables,error_label)){
		printf("trap_send fails!\n");
	  }
	}else{
	  if(trap_send_with_more_para(&dest_ip_address,my_ip_address,1,&snmp_oid,generic,specific,trap_port,time_stamp,variables,error_label)){
		printf("trap_send fails!\n");
	  }
	}
return 0;
}

