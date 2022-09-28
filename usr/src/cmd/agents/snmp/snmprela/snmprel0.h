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
	Integer agentSystemUpTime;
	Integer agentWatchDogTime;
} AgentEntry_t;

typedef struct _RegTreeEntry_t {
	Integer regTreeIndex;
	Integer regTreeAgentID;
	Oid regTreeOID;
	String regTreeView;
	Integer regTreeStatus;
	Integer regTreePriority;
} RegTreeEntry_t;

typedef struct _RegTblEntry_t {
	Integer regTblIndex;
	Integer regTblAgentID;
	Oid regTblOID;
	Integer regTblStartColumn;
	Integer regTblEndColumn;
	Integer regTblStartRow;
	Integer regTblEndRow;
	String regTblView;
	Integer regTblStatus;
} RegTblEntry_t;

extern int ssa_get_trap_port();
#endif
