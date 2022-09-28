#ifndef _SNMPRELAY_STUB_H_
#define _SNMPRELAY_STUB_H_


typedef struct _AgentEntry_t {
	Integer agentID;
	Integer agentStatus;
	Integer agentTimeOut;
	Integer agentPortNumber;
	String agentPersonalFile;
	String agentConfigFile;
	String agentExecutable;
	String agentVersionNum;
	String agentProtocol;
	Integer agentProcessID;
	String agentName;
	Integer agentOperStatus;
} AgentEntry_t;

typedef struct _RegTreeEntry_t {
	Integer regTreeIndex;
	Integer regTreeAgentID;
	Oid regTreeOID;
	Integer regTreeStatus;
} RegTreeEntry_t;

#endif
