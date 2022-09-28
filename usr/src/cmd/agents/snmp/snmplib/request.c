/* Copyright 09/18/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)request.c	1.6 96/09/18 Sun Microsystems"

/*
 * HISTORY
 * 5-20-96	Jerry Yeung 	export request_create and send_blocking
 *				change the timeout to bigger value for
 *				testing, say 100 sec
 * 9-18-96	Jerry Yeung	Trap send to 162
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
#include "trace.h"
#include "snmp.h"
#include "pdu.h"
#include "request.h"



/***** GLOBAL VARIABLES *****/

static Subid sysUptime_subids[] = { 1, 3, 6, 1, 2, 1, 1, 3, 0 };

Oid sysUptime_name = { sysUptime_subids, 8 };
Oid sysUptime_instance = { sysUptime_subids, 9 };


/***** LOCAL VARIABLES *****/

static u_long request_id = 0;

static Subid snmpEnableAuthTraps_subids[] = { 1, 3, 6, 1, 2, 1, 11, 30, 0 };
static Oid snmpEnableAuthTraps_name = { snmpEnableAuthTraps_subids, 9 };


/***** LOCAL FUNCTIONS *****/

/* (5-20-96) no longer local funcs. 
static SNMP_pdu *request_create(char *community, int type, char *error_label);
static SNMP_pdu *request_send_blocking(IPAddress *ip_address, SNMP_pdu *request, char *error_label);
*/


/********************************************************************/

/* static */ SNMP_pdu *request_create(char *community, int type, char *error_label)
{
	SNMP_pdu *request;


	error_label[0] = '\0';

	switch(type)
	{
		case GET_REQ_MSG:
		case GETNEXT_REQ_MSG:
		case SET_REQ_MSG:
			break;

		default:
			sprintf(error_label, "BUG: request_create(): bad type (0x%x)",
				type);
			return NULL;
	}

	request = snmp_pdu_new(error_label);
	if(request == NULL)
	{
		return NULL;
	}

	request->version = SNMP_VERSION_1;
	request->community = strdup(community);
	if(request->community == NULL)
	{
		sprintf(error_label, ERR_MSG_ALLOC);
		snmp_pdu_free(request);
		return NULL;
	}
	request->type = type;
	request->request_id = request_id++;


	return request;
}


/********************************************************************/

SNMP_pdu *request_send_to_port_time_out_blocking(IPAddress *ip_address, int port,struct timeval *timeout,SNMP_pdu *request, char *error_label)
{
	int sd;
	Address address;
	SNMP_pdu *response;
	Address me;
	int numfds;
	fd_set fdset;
	int count;


	error_label[0] = '\0';

	if(request == NULL)
	{
		sprintf(error_label, "BUG: request_send_blocking(): request is NULL");
		return NULL;
	}

	switch(request->type)
	{
		case GET_REQ_MSG:
		case GETNEXT_REQ_MSG:
		case SET_REQ_MSG:
			break;

		default:
			sprintf(error_label, "BUG: request_send_blocking(): bad type (0x%x)",
				request->type);
			return NULL;
	}

	/* sd */
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0)
	{
		sprintf(error_label, ERR_MSG_SOCKET,
			errno_string());
		return NULL;
	}

	memset(&me, 0, sizeof(Address));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(0);
	if(bind(sd, (struct sockaddr *)&me, sizeof(me)) != 0)
	{
		sprintf(error_label, ERR_MSG_BIND,
			errno_string());
		close(sd);
		return NULL;
	}

	/* address */
	memset(&address, 0, sizeof(Address));
	address.sin_family = AF_INET;
	address.sin_port = port;
	address.sin_addr.s_addr = ip_address->s_addr;

	if(snmp_pdu_send(sd, &address, request, error_label))
	{
		close(sd);
		return NULL;
	}


	while(1)
	{
		numfds = 0;
		FD_ZERO(&fdset);

		numfds = sd + 1;
		FD_SET(sd, &fdset);

		count = select(numfds, &fdset, 0, 0, timeout);
		if(count > 0)
		{
			if(FD_ISSET(sd, &fdset))
			{
				response = snmp_pdu_receive(sd, &address, error_label);
				if(response == NULL)
				{
					close(sd);
					return NULL;
				}
				close(sd);

				return response;
			}
		}
		else
		{
			switch(count)
			{
				case 0:
					sprintf(error_label, ERR_MSG_TIMEOUT);
					close(sd);
					return NULL;

				case -1:
					if(errno == EINTR)
					{
						continue;
					}
					else
					{
						sprintf(error_label, ERR_MSG_SELECT,
							errno_string());
						close(sd);
						return NULL;
					}
			}
		}
	}
}


SNMP_pdu *request_send_to_port_blocking(IPAddress *ip_address, int port,SNMP_pdu *request, char *error_label)
{
	struct timeval timeout;
	
	timeout.tv_sec = 100;
	timeout.tv_usec = 0;
	return(request_send_to_port_time_out_blocking
		(ip_address,port,&timeout,request,error_label));
}


/*static*/ SNMP_pdu *request_send_blocking(IPAddress *ip_address, SNMP_pdu *request, char *error_label)
{
  return(request_send_to_port_blocking(ip_address,SNMP_PORT,request,error_label));
}

/********************************************************************/

/*
 *	if the request failed, this function returns 0
 *	otherwise it returns sysUpTime
 */

long request_sysUpTime(char *error_label)
{
	static int my_ip_address_initialized = False;
	static IPAddress my_ip_address;
	SNMP_pdu *request;
	SNMP_pdu *response;
	SNMP_variable *variable;
	static long sysUpTime = 0;
	static clock_t last = 0;
	clock_t now;
	struct tms buffer;


	error_label[0] = '\0';

	now = times(&buffer);
	if( (last == 0) || ((now - last) > 360000) )	/* 1 hour */
	{
		if(my_ip_address_initialized == False)
		{
			if(get_my_ip_address(&my_ip_address, error_label))
			{
				return 0;
			}

			my_ip_address_initialized = True;
		}

		request = request_create("public", GET_REQ_MSG, error_label);
		if(request == NULL)
		{
			return 0;
		}

		if(snmp_pdu_append_null_variable(request, &sysUptime_instance, error_label) == NULL)
		{
			snmp_pdu_free(request);
			return 0;
		}

		response = request_send_blocking(&my_ip_address, request, error_label);
		if(response == NULL)
		{
			snmp_pdu_free(request);
			return 0;
		}
		snmp_pdu_free(request);

		if(response->error_status)
		{
			sprintf(error_label, "%s",
				error_status_string(response->error_status));
			snmp_pdu_free(response);
			return 0;
		}

		variable = response->first_variable;
		if(variable->next_variable
			|| SSAOidCmp(&(variable->name), &sysUptime_instance)
			|| (variable->type != TIMETICKS)
			|| (variable->val.integer == NULL)
			|| (variable->val_len != sizeof(long)) )
		{
			sprintf(error_label, ERR_MSG_BAD_RESPONSE);
			snmp_pdu_free(response);
			return 0;
		}
		sysUpTime = *(variable->val.integer);
		last = now;
		snmp_pdu_free(response);

		if(trace_level > 0)
		{
			trace("sysUpTime: %d\n\n", sysUpTime);
		}

		return sysUpTime;
	}

	return (sysUpTime + (now - last));
}


/********************************************************************/

/*
 *	if the request failed, this function returns  -1
 *	otherwise it returns True or False accordind to the
 *	value of snmpEnableAuthTraps
 */

int request_snmpEnableAuthTraps(char *error_label)
{
	static int my_ip_address_initialized = False;
	static IPAddress my_ip_address;
	SNMP_pdu *request;
	SNMP_pdu *response;
	SNMP_variable *variable;
	int snmpEnableAuthTraps;
	struct timeval timeout;
	
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;


	error_label[0] = '\0';

	if(my_ip_address_initialized == False)
	{
		if(get_my_ip_address(&my_ip_address, error_label))
		{
			return -1;
		}

		my_ip_address_initialized = True;
	}

	request = request_create("public", GET_REQ_MSG, error_label);
	if(request == NULL)
	{
		return -1;
	}

	if(snmp_pdu_append_null_variable(request, &snmpEnableAuthTraps_name, error_label) == NULL)
	{
		snmp_pdu_free(request);
		return -1;
	}

	response = request_send_to_port_time_out_blocking(&my_ip_address,SNMP_TRAP_PORT,&timeout,request,error_label);
	if(response == NULL)
	{
		snmp_pdu_free(request);
		return -1;
	}
	snmp_pdu_free(request);

	if(response->error_status)
	{
		sprintf(error_label, "%s",
			error_status_string(response->error_status));
		snmp_pdu_free(response);
		return -1;
	}

	variable = response->first_variable;
	if(variable->next_variable
		|| SSAOidCmp(&(variable->name), &snmpEnableAuthTraps_name)
		|| (variable->type != INTEGER)
		|| (variable->val.integer == NULL)
		|| (variable->val_len != sizeof(long)) )
	{
		sprintf(error_label, ERR_MSG_BAD_RESPONSE);
		snmp_pdu_free(response);
		return -1;
	}
	snmpEnableAuthTraps = *(variable->val.integer);
	snmp_pdu_free(response);

	if(trace_level > 0)
	{
		trace("snmpAuthTraps: %s\n\n",
			(snmpEnableAuthTraps == 1)? "enabled(1)": "disabled(2)");
	}

	switch(snmpEnableAuthTraps)
	{
		case 1: /* enabled(1) */
			return TRUE;
		case 2: /* disable(2) */
			return FALSE;
		default:
			sprintf(error_label, ERR_MSG_BAD_VALUE);
			return -1;
	}
}


/********************************************************************/

