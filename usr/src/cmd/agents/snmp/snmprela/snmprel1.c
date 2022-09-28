#include <sys/types.h>
#include <netinet/in.h>

#include "impl.h"
#include "asn1.h"
#include "error.h"
#include "snmp.h"
#include "trap.h"
#include "pdu.h"

#include "snmprelay.stub.h"



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


/***** agentSystemUpTime    ********************************/

int set_agentSystemUpTime(int pass, Integer agentID, Integer *agentSystemUpTime)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentWatchDogTime    ********************************/

int set_agentWatchDogTime(int pass, Integer agentID, Integer *agentWatchDogTime)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** agentTableIndex      ********************************/

int get_agentTableIndex(Integer *agentTableIndex)
{
	return NOT_IMPLEMENTED;
}

int set_agentTableIndex(int pass, Integer *agentTableIndex)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreeEntry         ********************************/

RegTreeEntry_t *get_regTreeEntry(int search_type, int *snmp_error, Integer *regTreeAgentID, Integer *regTreeIndex)
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


/***** regTreeOID           ********************************/

int set_regTreeOID(int pass, Integer regTreeAgentID, Integer regTreeIndex, Oid *regTreeOID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreeView          ********************************/

int set_regTreeView(int pass, Integer regTreeAgentID, Integer regTreeIndex, String *regTreeView)
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

int set_regTreeStatus(int pass, Integer regTreeAgentID, Integer regTreeIndex, Integer *regTreeStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTreePriority      ********************************/

int set_regTreePriority(int pass, Integer regTreeAgentID, Integer regTreeIndex, Integer *regTreePriority)
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


int get_relayTrapPort(Integer *relayTrapPort)
{
	return NOT_IMPLEMENTED;
}

/***** relayCheckPoint      ********************************/

int get_relayCheckPoint(String *relayCheckPoint)
{
	return NOT_IMPLEMENTED;
}

int set_relayCheckPoint(int pass, String *relayCheckPoint)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


int get_relayNSession(Integer *relayNSession)
{
	return NOT_IMPLEMENTED;
}

int get_relayNSessionDiscards(Integer *relayNSessionDiscards)
{
	return NOT_IMPLEMENTED;
}

/***** regTblEntry          ********************************/

RegTblEntry_t *get_regTblEntry(int search_type, int *snmp_error, Integer *regTblAgentID, Integer *regTblIndex)
{
	static RegTblEntry_t regTblEntry_data;

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


/***** regTblOID            ********************************/

int set_regTblOID(int pass, Integer regTblAgentID, Integer regTblIndex, Oid *regTblOID)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblStartColumn    ********************************/

int set_regTblStartColumn(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStartColumn)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblEndColumn      ********************************/

int set_regTblEndColumn(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblEndColumn)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblStartRow       ********************************/

int set_regTblStartRow(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStartRow)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblEndRow         ********************************/

int set_regTblEndRow(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblEndRow)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblView           ********************************/

int set_regTblView(int pass, Integer regTblAgentID, Integer regTblIndex, String *regTblView)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


/***** regTblStatus         ********************************/

int set_regTblStatus(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStatus)
{
	switch(pass)
	{
		case FIRST_PASS:
			return NOT_IMPLEMENTED;

		case SECOND_PASS:
			return NOT_IMPLEMENTED;
	}
}


int get_regTblTableIndex(Integer *regTblTableIndex)
{
	return NOT_IMPLEMENTED;
}

int get_relayPollInterval(Integer *relayPollInterval)
{
	return NOT_IMPLEMENTED;
}

int get_relayMaxAgentTimeOut(Integer *relayMaxAgentTimeOut)
{
	return NOT_IMPLEMENTED;
}
void main(int argc, char** argv)
{
ssa_main(argc,argv);
}

