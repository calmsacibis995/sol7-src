/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)util.c	2.17 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)util.c	2.17 96/10/02 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/* util.c
 */

/* $Header: /projects/mibcomp/util.c,v 1.3 90/09/17 23:12:55 romkey Exp $ */

/* $Log:	util.c,v $
 * Revision 1.3  90/09/17  23:12:55  romkey
 * installed Stuart Vance's parser fix, with findwhite()
 * 
 * Revision 1.2  90/09/17  22:38:27  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/09/17 23:12:55 $
 * $Revision: 1.3 $
 * $Author: romkey $
 */

/****************************************************************************
 *     Copyright (c) 1988-1990  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>

#include "mibcomp.h"

char *strdup();
int strcmpi();
void _dump_mib();

void syntax_error(reason, line)
	char *reason;
	char *line; {

	fprintf(stderr, "line %d: syntax error %s: %s\n",
		lineno, reason, line);
	exit(1);
	}

/* PORTABLE STRING HANDLING FUNCTIONS
 */
void stripnl(s)
	char *s; {
	int len;

	len = strlen(s);
	if(len && (s[len - 1] == '\n'))
		s[len - 1] = '\0';
	}

char *strdup(s)
	register char *s; {
	register char *t;
	extern char *malloc();

	t = malloc(strlen(s) + 1);
	if(t)
		strcpy(t, s);

	return t;
	}

int strcmpi(s, t)
	register char *s;
	register char *t; {
#ifdef SUN
	char a, b;
#endif /* SUN */

	while(*s && *t) {
#ifdef SUN
	        a = *s; b = *t;
		if(isupper(a))
		  a = tolower(a);
		if(isupper(b))
		  b = tolower(b);
		if(a != b)
			return -1;
#else
		if(tolower(*s) != tolower(*t))
			return -1;
#endif /* SUN */
		s++;
		t++;
		}

	if(*s != *t)
		return -1;
	else
		return 0;
	}

int strncmpi(s, t,len)
	register char *s;
	register char *t;
	int len; {
#ifdef SUN
	char a, b;
#endif /* SUN */

	while(*s && *t && len--) {
#ifdef SUN
	        a = *s; b = *t;
		if(isupper(a))
		  a = tolower(a);
		if(isupper(b))
		  b = tolower(b);
		if(a != b)
			return -1;
#else
		if(tolower(*s) != tolower(*t))
			return -1;
#endif /* SUN */
		s++;
		t++;
		}

	/* if we're here because len is zero, don't do the final compare,
	 * which verifies that both strings are null terminated, cause
	 * chances are, they're not.
	 */
	if(len == 0)
		return 0;
	
	if(*s != *t) {
		return -1;
		}
	else {
		return 0;
		}
	}

char *afterwhite(s)
	register char *s; {

	while(((*s == '\t') || (*s == ' ')) && (*s != '\0'))
		s++;

	return s;
	}

char *findwhite(s)
	register char *s; {

	while(((*s != '\t') && (*s != ' ')) && (*s != '\0'))
		s++;

	return s;
	}


/* MIB TREE HANDLING FUNCTIONS
 *
 *	The MIB tree is represented as a tree of nodes. Each node may have
 *	some children. A child may be either another node -or- an object
 *	(essentially a leaf in the tree).
 *
 *	Below we define functions for manipulating nodes and leaves on the
 *	tree.
 */
struct node *mknode(name)
        char *name;  {
	struct node *n;
        extern char *malloc();

	n = (struct node *)malloc(sizeof(struct node));
	if(n == NULL)
		return n;

	n->name = strdup(name);
	n->flags = 0;
	n->number = 0;
	n->kids = NULL;
	n->lastkid = NULL;
	n->defaults = NULL;
	n->parent = NULL;
	n->syntax = NULL;
	n->access = NULL;
	n->status  = NULL;
	return n;
	}

/* free a node entry, first freeing the name, later, other stuff...does not
 * really take into account default entries and stuff
 */
void freenode(n)
	struct node *n; {

	free(n->name);
	free((char *)n);
	}

/* perform a depth-first search looking for a particular node.
 *	this function maintains a one element cache because often succesive
 *	lookups will be for the same node.
 */
struct node *findnode(name, n)
	char *name;
	struct node *n; {
	struct node *kid;
	struct node *result;
	static struct node *cached = NULL;

	if(cached && strcmp(cached->name, name) == 0)
		return cached;

	for(kid = n->kids; kid; kid = kid->next) {
		if(strcmp(kid->name, name) == 0) {
			cached = (struct node *)kid;
			return (struct node *)kid;
			}

		if(kid->kids) {
			result = findnode(name, kid);
			if(result != NULL) {
				cached = result;
				return result;
				}
			}
		}

	return NULL;
	}

/* add a leaf to a node
 */
void nodeaddleaf(n, lf)
	struct node *n;
	struct node *lf; {

	lf->parent = n;
	n->flags &= ~FL_LEAF;

	if(n->kids) {
		n->lastkid->next = lf;
		n->lastkid = lf;
		}
	else {
		n->kids = lf;
		n->lastkid = lf;
		}

	lf->next = NULL;
	}

void dump_mib() {
	_dump_mib(&root);
	}

void _dump_mib(n)
	register struct node *n; {
	register struct node *kid;
	static int levels = 0;
	int i;
	
	for(i = 0; i < levels; i++)
		printf("  ");

	if(n->kids == NULL) {
		printf("leaf %s (number %d)\n", n->name, n->number);
		return;
		}

	printf("node %s (number %d)\n", n->name, n->number);

	for(kid = n->kids; kid; kid = kid->next) {
		levels++;
		_dump_mib(kid);
		levels--;
		}
	}
