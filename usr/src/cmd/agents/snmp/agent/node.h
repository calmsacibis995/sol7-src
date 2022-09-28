/* Copyright 09/16/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)node.h	1.7 96/09/16 Sun Microsystems"


#ifndef _NODE_H_
#define _NODE_H_

#define COLUMN		1
#define OBJECT		2
#define NODE		3

#define READ_FLAG	0x1
#define WRITE_FLAG	0x2


typedef struct _Enum {
	struct _Enum *next_enum;
	char *label;
	Integer value;
} Enum;

typedef struct _Object {
	Oid name;
	u_char asn1_type;
	Enum *first_enum;
	int access;
	int (*get)();
	int (*set)();
	void (*dealloc)();
} Object;

typedef struct _Index {
	struct _Index *next_index;
	char *label;
	struct _Node *node;
} Index;

typedef struct _Entry {
	struct _Index *first_index;
	int n_indexs;
/*
	char *(*get)();
*/
	int (*get)();
	void (*dealloc)();
} Entry;


typedef struct _Column {
	Oid name;
	u_char asn1_type;
	Enum *first_enum;
	int access;
	int (*set)();
	Entry *entry;
	int offset;
} Column;


typedef struct _Node {
	struct _Node *parent;
	struct _Node *first_child;
	struct _Node *next_peer;
	struct _Node *next;

	char *label;
	Subid subid;

	int type;
	union {
		Object *object;
		Column *column;
	} data;
} Node;

struct CallbackItem {
        Object *ptr;
        int type,next;
};
struct TrapHndlCxt {
        char name[256];
        int is_sun_enterprise;
        int generic,specific;
};

extern Enum enum_table[];
extern int enum_table_size;

extern Object object_table[];
extern int object_table_size;

extern Index index_table[];
extern int index_table_size;

extern Entry entry_table[];
extern int entry_table_size;

extern Column column_table[];
extern int column_table_size;

extern Node node_table[];
extern int node_table_size;

extern struct CallbackItem *callItem;
extern int numCallItem;

extern int *trapTableMap;

extern struct TrapHndlCxt *trapBucket;
extern int numTrapElem;


extern Node *node_find(int search_type, Oid *name, Oid *suffix);

#endif
