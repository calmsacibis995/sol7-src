#include <sys/types.h>
#include <netinet/in.h>

#include "impl.h"
#include "asn1.h"
#include "error.h"
#include "snmp.h"
#include "trap.h"

#include "snmprelay.stub.h"


/***** GLOBAL VARIABLES *****/

char default_config_file[] = "/etc/snmprelayd.snmprelay";
char default_sec_config_file[] = "/etc/snmprelayd.conf";
char default_error_file[] = "/tmp/snmprelayd.log";


/***********************************************************/

void agent_init()
{
}


/***********************************************************/

void agent_end()
{
}


/***********************************************************/

void agent_loop()
{
}


/***********************************************************/

void agent_select_info(fd_set *fdset, int *numfds)
{
}


/***********************************************************/

void agent_select_callback(fd_set *fdset)
{
}




/***** agentEntry           ********************************/

AgentEntry_t *get_agentEntry(int search_type, int *snmp_error, Integer *agentID)
{
	static AgentEntry_t agentEntry_data;

	*snmp_error = NOT_IMPLEMENTED;
	return NULL;

	/* Implementation for an empty table */
	switch(search_type)
	{
		case FIRST_ENTRY:
		case NEXT_ENTRY:
			*snmp_error = END_OF_TABLE;
			return NULL;

		case EXACT_ENTRY:
			*snmp_error = SNMP_ERR_NOSUCHNAME;
			return NULL;
	}
}


/***** agentID              ********************************/

int set_agentID(int pass, Integer agentID, Integer *agentID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentStatus          ********************************/

int set_agentStatus(int pass, Integer agentID, Integer *agentStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentTimeOut         ********************************/

int set_agentTimeOut(int pass, Integer agentID, Integer *agentTimeOut)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentPortNumber      ********************************/

int set_agentPortNumber(int pass, Integer agentID, Integer *agentPortNumber)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentPersonalFile    ********************************/

int set_agentPersonalFile(int pass, Integer agentID, String *agentPersonalFile)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentConfigFile      ********************************/

int set_agentConfigFile(int pass, Integer agentID, String *agentConfigFile)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentExecutable      ********************************/

int set_agentExecutable(int pass, Integer agentID, String *agentExecutable)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentVersionNum      ********************************/

int set_agentVersionNum(int pass, Integer agentID, String *agentVersionNum)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentProtocol        ********************************/

int set_agentProtocol(int pass, Integer agentID, String *agentProtocol)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentProcessID       ********************************/

int set_agentProcessID(int pass, Integer agentID, Integer *agentProcessID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentName            ********************************/

int set_agentName(int pass, Integer agentID, String *agentName)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentOperStatus      ********************************/

int set_agentOperStatus(int pass, Integer agentID, Integer *agentOperStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


int get_agentTableIndex(Integer *agentTableIndex)
{
	return NOT_IMPLEMENTED;
}

/***** regTreeEntry         ********************************/

RegTreeEntry_t *get_regTreeEntry(int search_type, int *snmp_error, Integer *regTreeIndex)
{
	static RegTreeEntry_t regTreeEntry_data;

	*snmp_error = NOT_IMPLEMENTED;
	return NULL;

	/* Implementation for an empty table */
	switch(search_type)
	{
		case FIRST_ENTRY:
		case NEXT_ENTRY:
			*snmp_error = END_OF_TABLE;
			return NULL;

		case EXACT_ENTRY:
			*snmp_error = SNMP_ERR_NOSUCHNAME;
			return NULL;
	}
}


/***** regTreeIndex         ********************************/

int set_regTreeIndex(int pass, Integer regTreeIndex, Integer *regTreeIndex)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreeAgentID       ********************************/

int set_regTreeAgentID(int pass, Integer regTreeIndex, Integer *regTreeAgentID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreeOID           ********************************/

int set_regTreeOID(int pass, Integer regTreeIndex, Oid *regTreeOID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreeStatus        ********************************/

int set_regTreeStatus(int pass, Integer regTreeIndex, Integer *regTreeStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


int get_regTreeTableIndex(Integer *regTreeTableIndex)
{
	return NOT_IMPLEMENTED;
}

/***** relayProcessIDFile   ********************************/

int get_relayProcessIDFile(String *relayProcessIDFile)
{
	return NOT_IMPLEMENTED;
}

int set_relayProcessIDFile(int pass, String *relayProcessIDFile)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** relayResourceFile    ********************************/

int get_relayResourceFile(String *relayResourceFile)
{
	return NOT_IMPLEMENTED;
}

int set_relayResourceFile(int pass, String *relayResourceFile)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** relayPersonalFileDir ********************************/

int get_relayPersonalFileDir(String *relayPersonalFileDir)
{
	return NOT_IMPLEMENTED;
}

int set_relayPersonalFileDir(int pass, String *relayPersonalFileDir)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** relayLogFile         ********************************/

int get_relayLogFile(String *relayLogFile)
{
	return NOT_IMPLEMENTED;
}

int set_relayLogFile(int pass, String *relayLogFile)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** relayOperationStatus ********************************/

int get_relayOperationStatus(Integer *relayOperationStatus)
{
	return NOT_IMPLEMENTED;
}

int set_relayOperationStatus(int pass, Integer *relayOperationStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}

