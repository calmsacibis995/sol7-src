/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)table.h	1.1 96/07/01 Sun Microsystems"


#ifndef _TABLE_H_
#define _TABLE_H_

typedef struct Table {
	int regTblIndex;
	int regTblAgentID;
	Oid regTblOID;
	int regTblStartColumn;
	int regTblEndColumn;	
	int	regTblStartRow;
	int	regTblEndRow;
	char 	*regTblView;
	int regTblStatus;
	struct Table* next_table;
	Agent *agent;
	
} Table, SSA_Table;


#endif
