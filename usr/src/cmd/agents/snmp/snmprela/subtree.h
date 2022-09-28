/* Copyright 09/26/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)subtree.h	1.6 96/09/26 Sun Microsystems"

/* HISTORY
 * 5-21-96	Jerry Yeung	support MIB
 * 6-4-96	Jerry Yeung	support table
 */

#ifndef _SUBTREE_H_
#define _SUBTREE_H_

#ifndef _SH_TABLE_H_
#include "sh_table.h"
#endif

#define TBL_TAG_TYPE_UNKNOWN 0
#define TBL_TAG_TYPE_COL 1
#define TBL_TAG_TYPE_LEAF 2

typedef struct _TblTag {
	int entry_index; /* lowest row index of the table */
	int type; /* col or leaf */
	Table *table;
} TblTag;

typedef struct _MirrorTag {
	Table *table;
} MirrorTag;

typedef struct _Subtree {
        Integer regTreeIndex;
        Integer regTreeAgentID;
	Oid		name;
/* rename regTreeOID to name, which has already used
        Oid regTreeOID;
*/
        Integer regTreeStatus;
	String regTreeView;
        Integer regTreePriority;
	struct _Subtree	*next_subtree;
	struct _Agent	*agent;
	struct _Subtree	*next_agent_subtree;
	struct _TblTag *tbl_tag;
	struct _MirrorTag *mirror_tag;

/* things to be addeded 
 * char view_selected;
 * char bulk_selected;
 * int priority;
 */

} Subtree;

extern Subtree *first_subtree;

extern int sap_reg_tree_index;

int subtree_add(Agent *agent, Subid *subids, int len, TblTag *tbl_tag);

/* if the the oid doesn't find, it will be created and inserted */
Subtree* subtree_find(Subid *subids, int len);

Subtree *subtree_match(u_char type, Oid *oid);

void subtree_list_delete();

void subtree_free(Subtree *sp); /* to be modified */

void subtree_detach(Subtree *sp);

void trace_subtrees();

void subtree_remove_from_agent_list(Subtree *subtree);

int subtree_is_valid(Subtree *subtree);

Subtree* subtree_next(Subtree *subtree);

void delete_all_subtree_from_agent(Agent* agent);

int subtree_purge(Subid *subids, int len);

int sync_subtrees_with_agent(Agent *agent);

#endif
