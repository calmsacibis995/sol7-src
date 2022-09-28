/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)parse.h	1.3 96/07/01 Sun Microsystems"

#ifndef _PARSE_H_
#define _PARSE_H_

/***********************************************************
	Copyright 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/*
 * parse.h
 */

/*
 * HISTORY
 * Jerry Yeung	6-6-96	support trap(trap-support)
 */

#define MAXLABEL	64	/* maximum characters in a label */
#define MAXTOKEN	64	/* maximum characters in a token */
#define MAXQUOTESTR	512	/* maximum characters in a quoted string */

/*
-- Olivier Reisacher 95/2/14
-- access flags
*/

#define READ_FLAG	0x1
#define WRITE_FLAG	0x2
#define CREATE_FLAG	0x4


/*
-- Olivier Reisacher 95/2/14
*/
struct index_list {
    struct index_list *next;
    struct tree *tp;
    char *label;
    int mark;
};



/*
 * A linked list of tag-value pairs for enumerated integers.
 */
struct enum_list {
    struct enum_list *next;
    int	value;
    char *label;
};

/*
 * Jerry Yeung 6/5/96
 */
struct trap_item {
	struct trap_item *next;
	char label[MAXLABEL];
	char enterprise_label[MAXLABEL];
	u_long enterprise_subids[32];
	struct index_list *var_list;
	int n_variables;
        char *description;
	int value;	/* trap-value */
}; 

extern struct trap_item *trap_list;


/*
 * A linked list of nodes.
 */
struct node {
    struct node *next;
    char label[MAXLABEL]; /* This node's (unique) textual name */
    u_long  subid;  /* This node's integer subidentifier */
    char parent[MAXLABEL];/* The parent's textual name */
    int type;	    /* The type of object this represents */
    struct enum_list *enums;	/* (optional) list of enumerated integers
(otherwise NULL) */
    char *description;	/* description (a quoted string) */
/*
-- Olivier Reisacher 95/2/14
*/
	int access;
	struct index_list *indexs;
	int n_indexs;
};

/*
 * A tree in the format of the tree structure of the MIB.
 */
struct tree {
    struct tree *child_list;	/* list of children of this node */
    struct tree *next_peer;	/* Next node in list of peers */
    struct tree *parent;
    char label[MAXLABEL];		/* This node's textual name */
    u_long subid;		/* This node's integer subidentifier */
    int type;			/* This node's object type */
    struct enum_list *enums;	/* (optional) list of enumerated integers
(otherwise NULL) */
    void (*printer)();     /* Value printing function */
    char *description;	/* description (a quoted string) */
/*
 -- Olivier Reisacher 95/2/14
*/
	int access;
	struct index_list *indexs;
	int n_indexs;
	struct tree *next;
	int node_index;
	int node_type;
	int object_index;
	int column_index;
	int entry_index;
};

/* non-aggregate types for tree end nodes */
#define TYPE_OTHER	    0
#define TYPE_OBJID	    1
#define TYPE_OCTETSTR	    2
#define TYPE_INTEGER	    3
#define TYPE_NETADDR	    4
#define	TYPE_IPADDR	    5
#define TYPE_COUNTER	    6
#define TYPE_GAUGE	    7
#define TYPE_TIMETICKS	    8
#define TYPE_OPAQUE	    9
#define TYPE_NULL	    10
#define TYPE_COUNTER64      11
#define TYPE_BITSTRING      12
#define TYPE_NSAPADDRESS    13
#define TYPE_UINTEGER	    14
/*
-- Olivier Reisacher 95/2/14
*/
#define TYPE_TABLE	20
#define TYPE_ENTRY	21

struct tree *read_mib();

/*
-- Olivier Reisacher 95/2/14
*/
void parse_init();
struct node *parse(FILE *fp);
struct tree *build_tree(struct node *nodes);
void print_subtree(struct tree *subtree, int count);

#endif
