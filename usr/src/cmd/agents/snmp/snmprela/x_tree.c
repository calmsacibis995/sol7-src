#include <sys/types.h>

#include "impl.h"
#include "asn1.h"
#include "node.h"

#include "snmprelay.stub.h"


extern AgentEntry_t *get_agentEntry(int search_type, int *snmp_error, Integer *agentID);
extern int set_agentID(int pass, Integer agentID, Integer *agentID2);
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
extern int set_agentOperStatus(int pass, Integer agentID, Integer *agentOperStatus);
extern int get_agentTableIndex(Integer *agentTableIndex);
extern RegTreeEntry_t *get_regTreeEntry(int search_type, int *snmp_error, Integer *regTreeIndex);
extern int set_regTreeIndex(int pass, Integer index, Integer *regTreeIndex);
extern int set_regTreeAgentID(int pass, Integer regTreeIndex, Integer *regTreeAgentID);
extern int set_regTreeOID(int pass, Integer regTreeIndex, Oid *regTreeOID);
extern int set_regTreeStatus(int pass, Integer regTreeIndex, Integer *regTreeStatus);
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

Subid subid_table[] = {
/*      0 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 1,
/*     11 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 2,
/*     22 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 3,
/*     33 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 4,
/*     44 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 5,
/*     55 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 6,
/*     66 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 7,
/*     77 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 8,
/*     88 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 9,
/*     99 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 10,
/*    110 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 11,
/*    121 */ 1, 3, 6, 1, 4, 1, 42, 500, 1, 1, 12,
/*    132 */ 1, 3, 6, 1, 4, 1, 42, 500, 2,
/*    141 */ 1, 3, 6, 1, 4, 1, 42, 500, 3, 1, 1,
/*    152 */ 1, 3, 6, 1, 4, 1, 42, 500, 3, 1, 2,
/*    163 */ 1, 3, 6, 1, 4, 1, 42, 500, 3, 1, 3,
/*    174 */ 1, 3, 6, 1, 4, 1, 42, 500, 3, 1, 4,
/*    185 */ 1, 3, 6, 1, 4, 1, 42, 500, 4,
/*    194 */ 1, 3, 6, 1, 4, 1, 42, 500, 5,
/*    203 */ 1, 3, 6, 1, 4, 1, 42, 500, 6,
/*    212 */ 1, 3, 6, 1, 4, 1, 42, 500, 7,
/*    221 */ 1, 3, 6, 1, 4, 1, 42, 500, 8,
/*    230 */ 1, 3, 6, 1, 4, 1, 42, 500, 9,
0
};
int subid_table_size = 239;

Enum enum_table[] = {
/*      0 */ { &enum_table[   1], "active", 1 },
/*      1 */ {              NULL, "inactive", 2 },
/*      2 */ { &enum_table[   3], "active", 1 },
/*      3 */ {              NULL, "inactive", 2 },
/*      4 */ { &enum_table[   5], "init", 1 },
/*      5 */ { &enum_table[   6], "run", 2 },
/*      6 */ {              NULL, "stop", 3 },
{ NULL, NULL, 0 }
};
int enum_table_size = 7;

Object object_table[] = {
/*      0 */ { { &subid_table[132], 9 }, INTEGER, NULL, READ_FLAG, get_agentTableIndex, NULL },
/*      1 */ { { &subid_table[185], 9 }, INTEGER, NULL, READ_FLAG, get_regTreeTableIndex, NULL },
/*      2 */ { { &subid_table[194], 9 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayProcessIDFile, set_relayProcessIDFile },
/*      3 */ { { &subid_table[203], 9 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayResourceFile, set_relayResourceFile },
/*      4 */ { { &subid_table[212], 9 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayPersonalFileDir, set_relayPersonalFileDir },
/*      5 */ { { &subid_table[221], 9 }, STRING, NULL, READ_FLAG | WRITE_FLAG, get_relayLogFile, set_relayLogFile },
/*      6 */ { { &subid_table[230], 9 }, INTEGER, &enum_table[4], READ_FLAG | WRITE_FLAG, get_relayOperationStatus, set_relayOperationStatus },
{ { NULL, 0}, 0, NULL, 0, NULL, NULL }
};
int object_table_size = 7;

Index index_table[] = {
/*      0 */ {              NULL, "agentID", &node_table[21] },
/*      1 */ {              NULL, "regTreeIndex", &node_table[36] },
{ NULL, NULL, NULL }
};
int index_table_size = 2;

Entry entry_table[] = {
/*      0 */ { &index_table[0], 1, (char *(*)()) get_agentEntry },
/*      1 */ { &index_table[1], 1, (char *(*)()) get_regTreeEntry },
{ NULL, 0, NULL }
};
int entry_table_size = 2;

Column column_table[] = {
/*      0 */ { { &subid_table[0], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentID, &entry_table[0], 0 },
/*      1 */ { { &subid_table[11], 11 }, INTEGER, &enum_table[0], READ_FLAG | WRITE_FLAG, set_agentStatus, &entry_table[0], 4 },
/*      2 */ { { &subid_table[22], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentTimeOut, &entry_table[0], 8 },
/*      3 */ { { &subid_table[33], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentPortNumber, &entry_table[0], 12 },
/*      4 */ { { &subid_table[44], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentPersonalFile, &entry_table[0], 16 },
/*      5 */ { { &subid_table[55], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentConfigFile, &entry_table[0], 24 },
/*      6 */ { { &subid_table[66], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentExecutable, &entry_table[0], 32 },
/*      7 */ { { &subid_table[77], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentVersionNum, &entry_table[0], 40 },
/*      8 */ { { &subid_table[88], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentProtocol, &entry_table[0], 48 },
/*      9 */ { { &subid_table[99], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentProcessID, &entry_table[0], 56 },
/*     10 */ { { &subid_table[110], 11 }, STRING, NULL, READ_FLAG | WRITE_FLAG, set_agentName, &entry_table[0], 60 },
/*     11 */ { { &subid_table[121], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_agentOperStatus, &entry_table[0], 68 },
/*     12 */ { { &subid_table[141], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTreeIndex, &entry_table[1], 0 },
/*     13 */ { { &subid_table[152], 11 }, INTEGER, NULL, READ_FLAG | WRITE_FLAG, set_regTreeAgentID, &entry_table[1], 4 },
/*     14 */ { { &subid_table[163], 11 }, OBJID, NULL, READ_FLAG | WRITE_FLAG, set_regTreeOID, &entry_table[1], 8 },
/*     15 */ { { &subid_table[174], 11 }, INTEGER, &enum_table[2], READ_FLAG | WRITE_FLAG, set_regTreeStatus, &entry_table[1], 16 },
{ { NULL, 0}, 0, NULL, 0, NULL, NULL , 0 }
};
int column_table_size = 16;

Node node_table[] = {
/*      0 */ {              NULL, &node_table[   1],              NULL, &node_table[  21], "iso", 1, NODE, NULL },
/*      1 */ { &node_table[   0], &node_table[   2],              NULL, &node_table[  21], "org", 3, NODE, NULL },
/*      2 */ { &node_table[   1], &node_table[   3],              NULL, &node_table[  21], "dod", 6, NODE, NULL },
/*      3 */ { &node_table[   2], &node_table[   4],              NULL, &node_table[  21], "internet", 1, NODE, NULL },
/*      4 */ { &node_table[   3],              NULL, &node_table[   5], &node_table[  21], "directory", 1, NODE, NULL },
/*      5 */ { &node_table[   3], &node_table[   6], &node_table[   7], &node_table[  21], "mgmt", 2, NODE, NULL },
/*      6 */ { &node_table[   5],              NULL,              NULL, &node_table[  21], "mib-2", 1, NODE, NULL },
/*      7 */ { &node_table[   3],              NULL, &node_table[   8], &node_table[  21], "experimental", 3, NODE, NULL },
/*      8 */ { &node_table[   3], &node_table[   9], &node_table[  47], &node_table[  21], "private", 4, NODE, NULL },
/*      9 */ { &node_table[   8], &node_table[  10],              NULL, &node_table[  21], "enterprises", 1, NODE, NULL },
/*     10 */ { &node_table[   9], &node_table[  11], &node_table[  46], &node_table[  21], "sun", 42, NODE, NULL },
/*     11 */ { &node_table[  10], &node_table[  12], &node_table[  18], &node_table[  21], "products", 2, NODE, NULL },
/*     12 */ { &node_table[  11], &node_table[  13],              NULL, &node_table[  21], "messaging", 8, NODE, NULL },
/*     13 */ { &node_table[  12], &node_table[  14], &node_table[  17], &node_table[  21], "agents", 1, NODE, NULL },
/*     14 */ { &node_table[  13],              NULL, &node_table[  15], &node_table[  21], "snmpx400d", 1, NODE, NULL },
/*     15 */ { &node_table[  13],              NULL, &node_table[  16], &node_table[  21], "snmpxapiad", 2, NODE, NULL },
/*     16 */ { &node_table[  13],              NULL,              NULL, &node_table[  21], "snmpx500d", 3, NODE, NULL },
/*     17 */ { &node_table[  12],              NULL,              NULL, &node_table[  21], "private-mibs", 2, NODE, NULL },
/*     18 */ { &node_table[  10], &node_table[  19],              NULL, &node_table[  21], "relay-agent", 500, NODE, NULL },
/*     19 */ { &node_table[  18], &node_table[  20], &node_table[  33], &node_table[  21], "agentTable", 1, NODE, NULL },
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
/*     32 */ { &node_table[  20],              NULL,              NULL, &node_table[  33], "agentOperStatus", 12, COLUMN, (void *) &column_table[11] },
/*     33 */ { &node_table[  18],              NULL, &node_table[  34], &node_table[  36], "agentTableIndex", 2, OBJECT, (void *) &object_table[0] },
/*     34 */ { &node_table[  18], &node_table[  35], &node_table[  40], &node_table[  36], "regTreeTable", 3, NODE, NULL },
/*     35 */ { &node_table[  34], &node_table[  36],              NULL, &node_table[  36], "regTreeEntry", 1, NODE, NULL },
/*     36 */ { &node_table[  35],              NULL, &node_table[  37], &node_table[  37], "regTreeIndex", 1, COLUMN, (void *) &column_table[12] },
/*     37 */ { &node_table[  35],              NULL, &node_table[  38], &node_table[  38], "regTreeAgentID", 2, COLUMN, (void *) &column_table[13] },
/*     38 */ { &node_table[  35],              NULL, &node_table[  39], &node_table[  39], "regTreeOID", 3, COLUMN, (void *) &column_table[14] },
/*     39 */ { &node_table[  35],              NULL,              NULL, &node_table[  40], "regTreeStatus", 4, COLUMN, (void *) &column_table[15] },
/*     40 */ { &node_table[  18],              NULL, &node_table[  41], &node_table[  41], "regTreeTableIndex", 4, OBJECT, (void *) &object_table[1] },
/*     41 */ { &node_table[  18],              NULL, &node_table[  42], &node_table[  42], "relayProcessIDFile", 5, OBJECT, (void *) &object_table[2] },
/*     42 */ { &node_table[  18],              NULL, &node_table[  43], &node_table[  43], "relayResourceFile", 6, OBJECT, (void *) &object_table[3] },
/*     43 */ { &node_table[  18],              NULL, &node_table[  44], &node_table[  44], "relayPersonalFileDir", 7, OBJECT, (void *) &object_table[4] },
/*     44 */ { &node_table[  18],              NULL, &node_table[  45], &node_table[  45], "relayLogFile", 8, OBJECT, (void *) &object_table[5] },
/*     45 */ { &node_table[  18],              NULL,              NULL,              NULL, "relayOperationStatus", 9, OBJECT, (void *) &object_table[6] },
/*     46 */ { &node_table[   9],              NULL,              NULL,              NULL, "sun", 42, NODE, NULL },
/*     47 */ { &node_table[   3],              NULL, &node_table[  48],              NULL, "security", 5, NODE, NULL },
/*     48 */ { &node_table[   3], &node_table[  49],              NULL,              NULL, "snmpV2", 6, NODE, NULL },
/*     49 */ { &node_table[  48],              NULL, &node_table[  50],              NULL, "snmpDomains", 1, NODE, NULL },
/*     50 */ { &node_table[  48],              NULL, &node_table[  51],              NULL, "snmpProxys", 2, NODE, NULL },
/*     51 */ { &node_table[  48],              NULL,              NULL,              NULL, "snmpModules", 3, NODE, NULL },
{ NULL, NULL, NULL, NULL, NULL, 0, 0, NULL }
};
int node_table_size = 52;

