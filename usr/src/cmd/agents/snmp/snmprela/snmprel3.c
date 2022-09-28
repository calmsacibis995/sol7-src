#include <sys/types.h>

#include "impl.h"
#include "asn1.h"
#include "node.h"

#include "snmprelay.stub.h"


extern AgentEntry_t *get_agentEntry(int search_type, int *snmp_error, Integer *agentID);
extern int set_agentStatus(int pass, Integer agentID, Integer *agentStatus);
extern int set_agentTimeOut(int pass, Integer agentID, Integer *agentTimeOut);
extern int set_agentPortNumber(int pass, Integer agentID, Integer *agentPortNumber);
extern int set_agentPersonalFile(int pass, Integer agentID, String *agentPersonalFile);
extern int set_agentConfigFile(int pass, Integer agentID, String *agentConfigFile);
extern int set_agentExecutable(int pass, Integer agentID, String *agentExecutable);
extern int set_agentVersionNum(int pass, Integer agentID, String *agentVersionNum);
extern int set_agentProtocol(int pass, Integer agentID, String *agentProtocol);
extern int set_agentProcessID(int pass, Integer agentID, Integer *agentProcessID);
extern int set_agentName(int pass, Integer agentID, String *agentName);
extern int set_agentSystemUpTime(int pass, Integer agentID, Integer *agentSystemUpTime);
extern int set_agentWatchDogTime(int pass, Integer agentID, Integer *agentWatchDogTime);
extern int get_agentTableIndex(Integer *agentTableIndex);
extern int set_agentTableIndex(int pass, Integer* agentTableIndex);
extern RegTreeEntry_t *get_regTreeEntry(int search_type, int *snmp_error, Integer *regTreeAgentID, Integer *regTreeIndex);
extern int set_regTreeOID(int pass, Integer regTreeAgentID, Integer regTreeIndex, Oid *regTreeOID);
extern int set_regTreeView(int pass, Integer regTreeAgentID, Integer regTreeIndex, String *regTreeView);
extern int set_regTreeStatus(int pass, Integer regTreeAgentID, Integer regTreeIndex, Integer *regTreeStatus);
extern int set_regTreePriority(int pass, Integer regTreeAgentID, Integer regTreeIndex, Integer *regTreePriority);
extern int get_regTreeTableIndex(Integer *regTreeTableIndex);
extern int get_relayProcessIDFile(String *relayProcessIDFile);
extern int set_relayProcessIDFile(int pass, String *relayProcessIDFile);
extern int get_relayResourceFile(String *relayResourceFile);
extern int set_relayResourceFile(int pass, String *relayResourceFile);
extern int get_relayPersonalFileDir(String *relayPersonalFileDir);
extern int set_relayPersonalFileDir(int pass, String *relayPersonalFileDir);
extern int get_relayLogFile(String *relayLogFile);
extern int set_relayLogFile(int pass, String *relayLogFile);
extern int get_relayOperationStatus(Integer *relayOperationStatus);
extern int set_relayOperationStatus(int pass, Integer* relayOperationStatus);
extern int get_relayTrapPort(Integer *relayTrapPort);
extern int get_relayCheckPoint(String *relayCheckPoint);
extern int set_relayCheckPoint(int pass, String *relayCheckPoint);
extern int get_relayNSession(Integer *relayNSession);
extern int get_relayNSessionDiscards(Integer *relayNSessionDiscards);
extern RegTblEntry_t *get_regTblEntry(int search_type, int *snmp_error, Integer *regTblAgentID, Integer *regTblIndex);
extern int set_regTblOID(int pass, Integer regTblAgentID, Integer regTblIndex, Oid *regTblOID);
extern int set_regTblStartColumn(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStartColumn);
extern int set_regTblEndColumn(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblEndColumn);
extern int set_regTblStartRow(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStartRow);
extern int set_regTblEndRow(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblEndRow);
extern int set_regTblView(int pass, Integer regTblAgentID, Integer regTblIndex, String *regTblView);
extern int set_regTblStatus(int pass, Integer regTblAgentID, Integer regTblIndex, Integer *regTblStatus);
extern int get_regTblTableIndex(Integer *regTblTableIndex);
extern int get_relayPollInterval(Integer *relayPollInterval);
extern int get_relayMaxAgentTimeOut(Integer *relayMaxAgentTimeOut);

Subid subid_table[] = {
/*      0 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 1,
/*     12 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 2,
/*     24 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 3,
/*     36 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 4,
/*     48 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 5,
/*     60 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 6,
/*     72 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 7,
/*     84 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 8,
/*     96 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 9,
/*    108 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 10,
/*    120 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 11,
/*    132 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 12,
/*    144 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 1, 1, 13,
/*    156 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 2,
/*    166 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 1,
/*    178 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 2,
/*    190 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 3,
/*    202 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 4,
/*    214 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 5,
/*    226 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 3, 1, 6,
/*    238 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 4,
/*    248 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 5,
/*    258 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 6,
/*    268 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 7,
/*    278 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 8,
/*    288 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 9,
/*    298 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 10,
/*    308 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 11,
/*    318 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 12,
/*    328 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 13,
/*    338 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 1,
/*    350 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 2,
/*    362 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 3,
/*    374 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 4,
/*    386 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 5,
/*    398 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 6,
/*    410 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 7,
/*    422 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 8,
/*    434 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 14, 1, 9,
/*    446 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 15,
/*    456 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 16,
/*    466 */ 1, 3, 6, 1, 4, 1, 42, 2, 15, 17,
0
};
int subid_table_size = 476;

Enum enum_table[] = {
/*      0 */ { &enum_table[   1], "active", 1 },
/*      1 */ { &enum_table[   2], "inactive", 2 },
/*      2 */ { &enum_table[   3], "init", 3 },
/*      3 */ { &enum_table[   4], "load", 4 },
/*      4 */ {              NULL, "destroy", 5 },
/*      5 */ { &enum_table[   6], "active", 1 },
/*      6 */ {              NULL, "inactive", 2 },
/*      7 */ { &enum_table[   8], "active", 1 },
/*      8 */ {              NULL, "inactive", 2 },
/*      9 */ { &enum_table[  10], "active", 1 },
/*     10 */ {              NULL, "inactive", 2 },
{ NULL, NULL, 0 }
};
int enum_table_size = 11;

Object object_table[] = {
/*      0 */ { { &subid_table[156], 10 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, get_agentTableIndex, set_agentTableIndex },
/*      1 */ { { &subid_table[238], 10 }, INTEGER, NULL, READ_FLAG, get_regTreeTableIndex, NULL },
/*      2 */ { { &subid_table[248], 10 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayProcessIDFile, set_relayProcessIDFile },
/*      3 */ { { &subid_table[258], 10 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayResourceFile, set_relayResourceFile },
/*      4 */ { { &subid_table[268], 10 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayPersonalFileDir, set_relayPersonalFileDir },
/*      5 */ { { &subid_table[278], 10 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayLogFile, set_relayLogFile },
/*      6 */ { { &subid_table[288], 10 }, INTEGER, &enum_table[7], READ_FLAG | WRITE_FLAG, get_relayOperationStatus, set_relayOperationStatus },
/*      7 */ { { &subid_table[298], 10 }, INTEGER, NULL, READ_FLAG, get_relayTrapPort, NULL },
/*      8 */ { { &subid_table[308], 10 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayCheckPoint, set_relayCheckPoint },
/*      9 */ { { &subid_table[318], 10 }, INTEGER, NULL, READ_FLAG, get_relayNSession, NULL },
/*     10 */ { { &subid_table[328], 10 }, INTEGER, NULL, READ_FLAG, get_relayNSessionDiscards, NULL },
/*     11 */ { { &subid_table[446], 10 }, INTEGER, NULL, READ_FLAG, get_regTblTableIndex, NULL },
/*     12 */ { { &subid_table[456], 10 }, INTEGER, NULL, READ_FLAG, get_relayPollInterval, NULL },
/*     13 */ { { &subid_table[466], 10 }, INTEGER, NULL, READ_FLAG, get_relayMaxAgentTimeOut, NULL },
{ { NULL, 0}, 0, NULL, 0, NULL, NULL }
};
int object_table_size = 14;

Index index_table[] = {
/*      0 */ {              NULL, "agentID", &node_table[21] },
/*      1 */ { &index_table[   2], "regTreeAgentID", &node_table[38] },
/*      2 */ {              NULL, "regTreeIndex", &node_table[37] },
/*      3 */ { &index_table[   4], "regTblAgentID", &node_table[56] },
/*      4 */ {              NULL, "regTblIndex", &node_table[55] },
{ NULL, NULL, NULL }
};
int index_table_size = 5;

Entry entry_table[] = {
/*      0 */ { &index_table[0], 1, (char *(*)()) get_agentEntry },
/*      1 */ { &index_table[1], 2, (char *(*)()) get_regTreeEntry },
/*      2 */ { &index_table[3], 2, (char *(*)()) get_regTblEntry },
{ NULL, 0, NULL }
};
int entry_table_size = 3;

Column column_table[] = {
/*      0 */ { { &subid_table[0], 12 }, INTEGER, NULL, READ_FLAG, NULL, &entry_table[0], 0 },
/*      1 */ { { &subid_table[12], 12 }, INTEGER, &enum_table[0], READ_FLAG | WRITE_FLAG, set_agentStatus, &entry_table[0], 4 },
/*      2 */ { { &subid_table[24], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentTimeOut, &entry_table[0], 8 },
/*      3 */ { { &subid_table[36], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentPortNumber, &entry_table[0], 12 },
/*      4 */ { { &subid_table[48], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentPersonalFile, &entry_table[0], 16 },
/*      5 */ { { &subid_table[60], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentConfigFile, &entry_table[0], 24 },
/*      6 */ { { &subid_table[72], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentExecutable, &entry_table[0], 32 },
/*      7 */ { { &subid_table[84], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentVersionNum, &entry_table[0], 40 },
/*      8 */ { { &subid_table[96], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentProtocol, &entry_table[0], 48 },
/*      9 */ { { &subid_table[108], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentProcessID, &entry_table[0], 56 },
/*     10 */ { { &subid_table[120], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentName, &entry_table[0], 60 },
/*     11 */ { { &subid_table[132], 12 }, TIMETICKS, NULL, READ_FLAG | WRITE_FLAG, set_agentSystemUpTime, &entry_table[0], 68 },
/*     12 */ { { &subid_table[144], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentWatchDogTime, &entry_table[0], 72 },
/*     13 */ { { &subid_table[166], 12 }, INTEGER, NULL, READ_FLAG, NULL, &entry_table[1], 0 },
/*     14 */ { { &subid_table[178], 12 }, INTEGER, NULL, READ_FLAG, NULL, &entry_table[1], 4 },
/*     15 */ { { &subid_table[190], 12 }, OBJID, NULL, READ_FLAG | WRITE_FLAG, set_regTreeOID, &entry_table[1], 8 },
/*     16 */ { { &subid_table[202], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_regTreeView, &entry_table[1], 16 },
/*     17 */ { { &subid_table[214], 12 }, INTEGER, &enum_table[5], READ_FLAG | WRITE_FLAG, set_regTreeStatus, &entry_table[1], 24 },
/*     18 */ { { &subid_table[226], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTreePriority, &entry_table[1], 28 },
/*     19 */ { { &subid_table[338], 12 }, INTEGER, NULL, READ_FLAG, NULL, &entry_table[2], 0 },
/*     20 */ { { &subid_table[350], 12 }, INTEGER, NULL, READ_FLAG, NULL, &entry_table[2], 4 },
/*     21 */ { { &subid_table[362], 12 }, OBJID, NULL, READ_FLAG | WRITE_FLAG, set_regTblOID, &entry_table[2], 8 },
/*     22 */ { { &subid_table[374], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTblStartColumn, &entry_table[2], 16 },
/*     23 */ { { &subid_table[386], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTblEndColumn, &entry_table[2], 20 },
/*     24 */ { { &subid_table[398], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTblStartRow, &entry_table[2], 24 },
/*     25 */ { { &subid_table[410], 12 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTblEndRow, &entry_table[2], 28 },
/*     26 */ { { &subid_table[422], 12 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_regTblView, &entry_table[2], 32 },
/*     27 */ { { &subid_table[434], 12 }, INTEGER, &enum_table[9], READ_FLAG | WRITE_FLAG, set_regTblStatus, &entry_table[2], 40 },
{ { NULL, 0}, 0, NULL, 0, NULL, NULL , 0 }
};
int column_table_size = 28;

Node node_table[] = {
/*      0 */ {              NULL, &node_table[   1],              NULL, &node_table[  21], "iso", 1, NODE, NULL },
/*      1 */ { &node_table[   0], &node_table[   2],              NULL, &node_table[  21], "org", 3, NODE, NULL },
/*      2 */ { &node_table[   1], &node_table[   3],              NULL, &node_table[  21], "dod", 6, NODE, NULL },
/*      3 */ { &node_table[   2], &node_table[   4],              NULL, &node_table[  21], "internet", 1, NODE, NULL },
/*      4 */ { &node_table[   3],              NULL, &node_table[   5], &node_table[  21], "directory", 1, NODE, NULL },
/*      5 */ { &node_table[   3], &node_table[   6], &node_table[   7], &node_table[  21], "mgmt", 2, NODE, NULL },
/*      6 */ { &node_table[   5],              NULL,              NULL, &node_table[  21], "mib-2", 1, NODE, NULL },
/*      7 */ { &node_table[   3],              NULL, &node_table[   8], &node_table[  21], "experimental", 3, NODE, NULL },
/*      8 */ { &node_table[   3], &node_table[   9], &node_table[  69], &node_table[  21], "private", 4, NODE, NULL },
/*      9 */ { &node_table[   8], &node_table[  10],              NULL, &node_table[  21], "enterprises", 1, NODE, NULL },
/*     10 */ { &node_table[   9], &node_table[  11], &node_table[  68], &node_table[  21], "sun", 42, NODE, NULL },
/*     11 */ { &node_table[  10], &node_table[  12], &node_table[  67], &node_table[  21], "products", 2, NODE, NULL },
/*     12 */ { &node_table[  11], &node_table[  13], &node_table[  18], &node_table[  21], "messaging", 8, NODE, NULL },
/*     13 */ { &node_table[  12], &node_table[  14], &node_table[  17], &node_table[  21], "agents", 1, NODE, NULL },
/*     14 */ { &node_table[  13],              NULL, &node_table[  15], &node_table[  21], "snmpx400d", 1, NODE, NULL },
/*     15 */ { &node_table[  13],              NULL, &node_table[  16], &node_table[  21], "snmpxapiad", 2, NODE, NULL },
/*     16 */ { &node_table[  13],              NULL,              NULL, &node_table[  21], "snmpx500d", 3, NODE, NULL },
/*     17 */ { &node_table[  12],              NULL,              NULL, &node_table[  21], "private-mibs", 2, NODE, NULL },
/*     18 */ { &node_table[  11], &node_table[  19],              NULL, &node_table[  21], "relay-agent", 15, NODE, NULL },
/*     19 */ { &node_table[  18], &node_table[  20], &node_table[  34], &node_table[  21], "agentTable", 1, NODE, NULL },
/*     20 */ { &node_table[  19], &node_table[  21],              NULL, &node_table[  21], "agentEntry", 1, NODE, NULL },
/*     21 */ { &node_table[  20],              NULL, &node_table[  22], &node_table[  22], "agentID", 1, COLUMN, (void *) &column_table[0] },
/*     22 */ { &node_table[  20],              NULL, &node_table[  23], &node_table[  23], "agentStatus", 2, COLUMN, (void *) &column_table[1] },
/*     23 */ { &node_table[  20],              NULL, &node_table[  24], &node_table[  24], "agentTimeOut", 3, COLUMN, (void *) &column_table[2] },
/*     24 */ { &node_table[  20],              NULL, &node_table[  25], &node_table[  25], "agentPortNumber", 4, COLUMN, (void *) &column_table[3] },
/*     25 */ { &node_table[  20],              NULL, &node_table[  26], &node_table[  26], "agentPersonalFile", 5, COLUMN, (void *) &column_table[4] },
/*     26 */ { &node_table[  20],              NULL, &node_table[  27], &node_table[  27], "agentConfigFile", 6, COLUMN, (void *) &column_table[5] },
/*     27 */ { &node_table[  20],              NULL, &node_table[  28], &node_table[  28], "agentExecutable", 7, COLUMN, (void *) &column_table[6] },
/*     28 */ { &node_table[  20],              NULL, &node_table[  29], &node_table[  29], "agentVersionNum", 8, COLUMN, (void *) &column_table[7] },
/*     29 */ { &node_table[  20],              NULL, &node_table[  30], &node_table[  30], "agentProtocol", 9, COLUMN, (void *) &column_table[8] },
/*     30 */ { &node_table[  20],              NULL, &node_table[  31], &node_table[  31], "agentProcessID", 10, COLUMN, (void *) &column_table[9] },
/*     31 */ { &node_table[  20],              NULL, &node_table[  32], &node_table[  32], "agentName", 11, COLUMN, (void *) &column_table[10] },
/*     32 */ { &node_table[  20],              NULL, &node_table[  33], &node_table[  33], "agentSystemUpTime", 12, COLUMN, (void *) &column_table[11] },
/*     33 */ { &node_table[  20],              NULL,              NULL, &node_table[  34], "agentWatchDogTime", 13, COLUMN, (void *) &column_table[12] },
/*     34 */ { &node_table[  18],              NULL, &node_table[  35], &node_table[  37], "agentTableIndex", 2, OBJECT, (void *) &object_table[0] },
/*     35 */ { &node_table[  18], &node_table[  36], &node_table[  43], &node_table[  37], "regTreeTable", 3, NODE, NULL },
/*     36 */ { &node_table[  35], &node_table[  37],              NULL, &node_table[  37], "regTreeEntry", 1, NODE, NULL },
/*     37 */ { &node_table[  36],              NULL, &node_table[  38], &node_table[  38], "regTreeIndex", 1, COLUMN, (void *) &column_table[13] },
/*     38 */ { &node_table[  36],              NULL, &node_table[  39], &node_table[  39], "regTreeAgentID", 2, COLUMN, (void *) &column_table[14] },
/*     39 */ { &node_table[  36],              NULL, &node_table[  40], &node_table[  40], "regTreeOID", 3, COLUMN, (void *) &column_table[15] },
/*     40 */ { &node_table[  36],              NULL, &node_table[  41], &node_table[  41], "regTreeView", 4, COLUMN, (void *) &column_table[16] },
/*     41 */ { &node_table[  36],              NULL, &node_table[  42], &node_table[  42], "regTreeStatus", 5, COLUMN, (void *) &column_table[17] },
/*     42 */ { &node_table[  36],              NULL,              NULL, &node_table[  43], "regTreePriority", 6, COLUMN, (void *) &column_table[18] },
/*     43 */ { &node_table[  18],              NULL, &node_table[  44], &node_table[  44], "regTreeTableIndex", 4, OBJECT, (void *) &object_table[1] },
/*     44 */ { &node_table[  18],              NULL, &node_table[  45], &node_table[  45], "relayProcessIDFile", 5, OBJECT, (void *) &object_table[2] },
/*     45 */ { &node_table[  18],              NULL, &node_table[  46], &node_table[  46], "relayResourceFile", 6, OBJECT, (void *) &object_table[3] },
/*     46 */ { &node_table[  18],              NULL, &node_table[  47], &node_table[  47], "relayPersonalFileDir", 7, OBJECT, (void *) &object_table[4] },
/*     47 */ { &node_table[  18],              NULL, &node_table[  48], &node_table[  48], "relayLogFile", 8, OBJECT, (void *) &object_table[5] },
/*     48 */ { &node_table[  18],              NULL, &node_table[  49], &node_table[  49], "relayOperationStatus", 9, OBJECT, (void *) &object_table[6] },
/*     49 */ { &node_table[  18],              NULL, &node_table[  50], &node_table[  50], "relayTrapPort", 10, OBJECT, (void *) &object_table[7] },
/*     50 */ { &node_table[  18],              NULL, &node_table[  51], &node_table[  51], "relayCheckPoint", 11, OBJECT, (void *) &object_table[8] },
/*     51 */ { &node_table[  18],              NULL, &node_table[  52], &node_table[  52], "relayNSession", 12, OBJECT, (void *) &object_table[9] },
/*     52 */ { &node_table[  18],              NULL, &node_table[  53], &node_table[  55], "relayNSessionDiscards", 13, OBJECT, (void *) &object_table[10] },
/*     53 */ { &node_table[  18], &node_table[  54], &node_table[  64], &node_table[  55], "regTblTable", 14, NODE, NULL },
/*     54 */ { &node_table[  53], &node_table[  55],              NULL, &node_table[  55], "regTblEntry", 1, NODE, NULL },
/*     55 */ { &node_table[  54],              NULL, &node_table[  56], &node_table[  56], "regTblIndex", 1, COLUMN, (void *) &column_table[19] },
/*     56 */ { &node_table[  54],              NULL, &node_table[  57], &node_table[  57], "regTblAgentID", 2, COLUMN, (void *) &column_table[20] },
/*     57 */ { &node_table[  54],              NULL, &node_table[  58], &node_table[  58], "regTblOID", 3, COLUMN, (void *) &column_table[21] },
/*     58 */ { &node_table[  54],              NULL, &node_table[  59], &node_table[  59], "regTblStartColumn", 4, COLUMN, (void *) &column_table[22] },
/*     59 */ { &node_table[  54],              NULL, &node_table[  60], &node_table[  60], "regTblEndColumn", 5, COLUMN, (void *) &column_table[23] },
/*     60 */ { &node_table[  54],              NULL, &node_table[  61], &node_table[  61], "regTblStartRow", 6, COLUMN, (void *) &column_table[24] },
/*     61 */ { &node_table[  54],              NULL, &node_table[  62], &node_table[  62], "regTblEndRow", 7, COLUMN, (void *) &column_table[25] },
/*     62 */ { &node_table[  54],              NULL, &node_table[  63], &node_table[  63], "regTblView", 8, COLUMN, (void *) &column_table[26] },
/*     63 */ { &node_table[  54],              NULL,              NULL, &node_table[  64], "regTblStatus", 9, COLUMN, (void *) &column_table[27] },
/*     64 */ { &node_table[  18],              NULL, &node_table[  65], &node_table[  65], "regTblTableIndex", 15, OBJECT, (void *) &object_table[11] },
/*     65 */ { &node_table[  18],              NULL, &node_table[  66], &node_table[  66], "relayPollInterval", 16, OBJECT, (void *) &object_table[12] },
/*     66 */ { &node_table[  18],              NULL,              NULL,              NULL, "relayMaxAgentTimeOut", 17, OBJECT, (void *) &object_table[13] },
/*     67 */ { &node_table[  10],              NULL,              NULL,              NULL, "products", 2, NODE, NULL },
/*     68 */ { &node_table[   9],              NULL,              NULL,              NULL, "sun", 42, NODE, NULL },
/*     69 */ { &node_table[   3],              NULL, &node_table[  70],              NULL, "security", 5, NODE, NULL },
/*     70 */ { &node_table[   3], &node_table[  71],              NULL,              NULL, "snmpV2", 6, NODE, NULL },
/*     71 */ { &node_table[  70],              NULL, &node_table[  72],              NULL, "snmpDomains", 1, NODE, NULL },
/*     72 */ { &node_table[  70],              NULL, &node_table[  73],              NULL, "snmpProxys", 2, NODE, NULL },
/*     73 */ { &node_table[  70],              NULL,              NULL,              NULL, "snmpModules", 3, NODE, NULL },
{ NULL, NULL, NULL, NULL, NULL, 0, 0, NULL }
};
int node_table_size = 74;

