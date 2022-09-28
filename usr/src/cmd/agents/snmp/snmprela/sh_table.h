/* Copyright 09/26/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)sh_table.h	1.4 96/09/26 Sun Microsystems"

#ifndef _SH_TABLE_H_
#define _SH_TABLE_H_

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>

#include "impl.h"
#include "error.h"
#include "trace.h"
#include "pdu.h"

#include "snmprelay_msg.h"
#include "agent.h"
/*
#ifndef _SUBTREE_H_
#include "subtree.h"
#endif
*/

#define TABLE_TO_OID_TRY 0
#define TABLE_TO_OID_GO  1

typedef struct _Table {
	Integer regTblIndex;
	Integer	regTblAgentID;
        Oid name;
        Subid first_column_subid; /* Subid may convert to Integer */
        Subid last_column_subid;
        Subid first_index_subid;
        Subid last_index_subid;
	Integer	regTblStatus;
	String	regTblView;
        Agent *agent;
        struct _Table *next_table;
	int mirror_flag;
} Table;

extern int is_first_entry(Table *table);
extern void table_free(Table *tp);
extern void table_list_delete();
extern void trace_tables();
extern void delete_all_tables_for_agent(Agent *agent);
extern void table_detach(Table *tp);
extern int activate_table(Table *tp);
extern int delete_table(Table *tp);
extern int activate_table_for_agent(Agent* agent);

extern Table *first_table;
extern Table *last_table;

extern int single_table_to_subtrees(int pass,Table *tp, char* error_label);

extern void delete_all_table_from_agent(Agent *agent);

extern void create_mirror_table_from_subtree();

#endif
