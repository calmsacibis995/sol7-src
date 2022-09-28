/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)parse.c	2.16 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)parse.c	2.16 96/10/02 Sun Microsystems";
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
/* parse.c
 */

/* $Header: /projects/mibcomp/parse.c,v 1.4 90/10/02 15:32:45 romkey Exp $ */

/* $Log:	parse.c,v $
 * Revision 1.4  90/10/02  15:32:45  romkey
 * added code to parse DEFAULT locator, DEFAULT view_mask and DEFAULT write_mask
 * 
 * Revision 1.3  90/09/17  23:12:39  romkey
 * installed Stuart Vance's parser fix, with findwhite()
 * 
 * Revision 1.2  90/09/17  22:38:22  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/10/02 15:32:45 $
 * $Revision: 1.4 $
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

#include "mibcomp.h"

char *strchr();

#define	TOKEN_DEFAULT		"DEFAULT"
#define	TOKEN_FORCE_INC		"FORCE-INCLUDE"
#define	TOKEN_OBJECT_ID		"OBJECT IDENTIFIER ::="
#define	TOKEN_OBJECT_TYPE	"OBJECT-TYPE"
#define	TOKEN_SEQUENCE		"::= SEQUENCE {"
#define	TOKEN_DEFINITIONS	"DEFINITIONS ::="
#define	TOKEN_IMPORTS		"IMPORTS"
#define	TOKEN_SYNTAX		"SYNTAX"
#define	TOKEN_ACCESS		"ACCESS"
#define	TOKEN_STATUS		"STATUS"

char *skipblanklines();


/* parse the input file and read its contents into a series of lists of
 * structures that we maintain.
 */
scan_input(fin)
	FILE *fin; {
	char buffer[512];
	char *name;
	char *verb;
	char *s;

	/* The MIB file is assumed to have some fixed headers at the start -
	 *
	 *		RFCxxxxMIB { iso org(3) ... }
	 *		DEFINITIONS ::= BEGIN
	 *		IMPORTS ... ;
	 *
	 *	we have to scan over this stuff, tho the first line is VERY important
	 *	to us since it initializes the MIB tree.
	 */
	skipblanklines(fin, buffer);

	parse_initial_tree(buffer);

	s = skipblanklines(fin, buffer);

	while(strncmp(s, TOKEN_FORCE_INC, sizeof(TOKEN_FORCE_INC)-1) == 0) {
		s = findwhite(s);
		if(s == NULL)
			syntax_error("FORCE-INCLUDE requres a filename", buffer);
		if(remember_include(s) == -1)
			fprintf(stderr, "FORCE-INCLUDE memory exhausted, not remembering %s\n", s);
		s = skipblanklines(fin, buffer);
		}

	while(strncmp(s, TOKEN_DEFAULT, sizeof(TOKEN_DEFAULT)-1) == 0) {
		parse_default(&root, s + sizeof(TOKEN_DEFAULT)-1);
		s = skipblanklines(fin, buffer);
		}

	if(strncmp(s, TOKEN_DEFINITIONS, sizeof(TOKEN_DEFINITIONS)-1))
		syntax_error("expected 'DEFINITIONS ::= BEGIN'", buffer);

	s = skipblanklines(fin, buffer);
	if(strncmp(s, TOKEN_IMPORTS, sizeof(TOKEN_IMPORTS) - 1))
		syntax_error("expected IMPORTS section", buffer);

	/* having found the imports section, we read lines till we find one
	 * 	which (1) we don't ignore and (2) ends in ;
	 */
	while(!feof(fin)) {
		/* read the next line
		 */
		if(fgets(buffer, 512, fin) == NULL)
			break;

		/* increment the line number
		 */
		lineno++;

		/* strip the trailing newline
		 */
		stripnl(buffer);

		/* if it's a comment or an empty line, ignore it
		 */
		if(ignoreline(buffer))
			continue;

		if(buffer[strlen(buffer) - 1] == ';')
			break;
		}


	/* do this till we run out of file
	 */	
	while(!feof(fin)) {
		/* read the next line
		 */
		if(fgets(buffer, 512, fin) == NULL)
			break;

		/* increment the line number
		 */
		lineno++;

		/* strip the trailing newline
		 */
		stripnl(buffer);

		/* if it's a comment or an empty line, ignore it
		 */
		if(ignoreline(buffer))
			continue;

		/* get the name, verb and rest sections of the line
		 */
		name = afterwhite(buffer);

		/* check if it's the END token
		 */
		if(strcmpi(name, "END") == 0) {
#if	DEBUG
			printf("found END of MIB file\n");
#endif
			return 1;
			}

		/* find the verb
		 */
		verb = findwhite(name);
		if(verb) {
		  *verb++ = '\0';
		  verb = afterwhite(verb);
		  }
		else {
		  printf("error: no verb, line %d\n", lineno);
		  exit(1);
		}


		/* at this point in the game, we're looking for one of three things:
		 * 		- an OBJECT-TYPE definition
		 *		- an OBJECT IDENTIFIER definition
		 *	or
		 *		- a SEQUENCE definition
		 */
		if(strncmp(verb, TOKEN_OBJECT_ID, sizeof(TOKEN_OBJECT_ID) - 1) == 0)
			parse_object_id(name, verb + sizeof(TOKEN_OBJECT_ID));
		else
			if(strncmp(verb, TOKEN_OBJECT_TYPE, sizeof(TOKEN_OBJECT_TYPE))==0)
				parse_object_type(name, fin);
		else
			if(strncmp(verb, TOKEN_SEQUENCE, sizeof(TOKEN_SEQUENCE)) == 0)
				parse_sequence(name, fin);
		else
			fprintf(stderr, "line %d: syntax error, verb %s\n", 
				lineno, verb);
		}
	}


/* parse the initial line of the form
 *	XXXXX { iso thing(1) thing(2) ... }
 * and stuff the things in the tree
 */
parse_initial_tree(buffer)
	char *buffer; {
	register char *s;
	struct node *last_node = &root;
	struct node *n;
	char *node_name;
	int node_number,permloop=1;

	/* strip out XXXX
	 */
	s = afterwhite(buffer);
	s = findwhite(s);
	if(s == NULL)
		syntax_error("", buffer);

	/* strip out {
	 */
	s = afterwhite(s);

	s = findwhite(s);
	if(s == NULL)
		syntax_error("", buffer);

	s = afterwhite(s);
	if(strncmp(s, "iso", 3) != 0)
		syntax_error("expected iso keyword", s);

	n = mknode("iso");
	n->number = 1;
	nodeaddleaf(last_node, n);
	last_node = n;

	/* go in a loop reading things
	 */
	while(permloop) {
		s = findwhite(s);
		if(s == NULL)
			syntax_error("", buffer);

		s = afterwhite(s);

		if(atoi(s))
			return 1;

		node_name = s;
		s = strchr(s, '(');
		if(s == NULL)
			syntax_error("expected form 'name(number)'", buffer);

		*s++ = '\0';
		node_number = atoi(s);

		n = mknode(node_name);
		n->number = node_number;

		nodeaddleaf(last_node, n);
		last_node = n;
		}
	}



/* parses an OBJECT IDENTIFIER statement. This causes a new node to be
 * added to the MIB tree.
 */
parse_object_id(name, rest)
	char *name;
	char *rest; {
	char parent_name[256];
	int number;
	struct node *n;
	struct node *parent;

#if	DEBUG
	printf("parse_object_id(%s, %s)\n", name, rest);
#endif

	sscanf(rest, "{%s %d}", parent_name, &number);

	n = mknode(name);
	n->number = number;

	/* make sure this node doesn't look like a leaf
	 */
	n->flags &= ~FL_LEAF;

	parent = findnode(parent_name, &root); 
	if(parent == NULL) {
		fprintf(stderr, "line %d: invalid parent node %s\n", 
			lineno, parent_name);
		dump_mib();
		exit(1);
		}

	nodeaddleaf(parent, n);
        return 1;
	}

/* This function assumes a very rigid declaration format
 *
 *	name OBJECT-TYPE
 *		SYNTAX syntax-type
 *		ACCESS access-type
 *		STATUS status-type
 *		::= { parent number }
 *
 * and will fail with anything not following this format.
 */
parse_object_type(name, fin)
	char *name;
	FILE *fin; {
	char buffer[512];
	char *s;
	register struct node *lf;
	char parent_name[128];
	struct node *parent;
	int i;

#if	DEBUG
	printf("parse_object_type(%s, ...)\n", name);
#endif

	lf = mknode(name);
	
	s = skipblanklines(fin, buffer);
	if(strncmp(s, TOKEN_SYNTAX, sizeof(TOKEN_SYNTAX) - 1))
		syntax_error("expecting SYNTAX keyword", buffer);

	s = findwhite(s);
	lf->syntax = lookup_type(afterwhite(s));

	/* SYNTAX may end with {, which means there's some kind of list
	 * involved, so make sure we skip over that.
	 */
	if(s[strlen(s) - 1] == '{') {
		s[strlen(s) - 1] = '\0';

		while(!feof(fin)) {
			/* read the next line
			 */
			if(fgets(buffer, 512, fin) == NULL)
				break;

			/* increment the line number
			 */
			lineno++;

			/* strip the trailing newline
			 */
			stripnl(buffer);

			/* if it's a comment or an empty line, ignore it
			 */
			if(ignoreline(buffer))
				continue;

			s = afterwhite(buffer);
			if(strncmp(s, "}", 1) == 0)
				break;
			}
		}


	s = skipblanklines(fin, buffer);
	if(strncmp(s, TOKEN_ACCESS, sizeof(TOKEN_ACCESS) - 1))
		syntax_error("expecting ACCESS keyword", buffer);

	s = findwhite(s);
	s = afterwhite(s);
	lf->access = strdup(s);
	lf->flags |= FL_READABLE|FL_WRITEABLE;
	if(strncmp(lf->access, "read-only", 9) == 0) {
		lf->flags &= ~FL_WRITEABLE;

		strcpy(lf->access, "RO");

		for(i = 3; i < 9; i++)
			lf->access[i] = ' ';

		if(strlen(s) > 9)
			lf->access[2] = '|';
		}
	else {
		strcpy(lf->access, "RW");
		for(i = 3; i < 10; i++)
			lf->access[i] = ' ';

		if(strlen(s) > 10)
			lf->access[2] = '|';
		}

	s = skipblanklines(fin, buffer);
	if(strncmp(s, TOKEN_STATUS, sizeof(TOKEN_STATUS) - 1))
		syntax_error("expecting STATUS keyword", buffer);

	s = findwhite(s);
	lf->status = strdup(s);

	s = skipblanklines(fin, buffer);

	while(strncmp(s, TOKEN_DEFAULT, sizeof(TOKEN_DEFAULT)-1) == 0) {
		parse_default(lf, s + sizeof(TOKEN_DEFAULT)-1);
		s = skipblanklines(fin, buffer);
		}

	if(strncmp(s, "::=", 3))
		syntax_error("expecting ::= keyword", buffer);

	sscanf(s, "::= { %s %d }", parent_name, &lf->number);

	parent = findnode(parent_name, &root);
	if(parent == NULL) {
		fprintf(stderr, "invalid parent node %s\n", parent_name);
		dump_mib();
		exit(1);
		}

	/* make sure this node does look like a leaf
	 */
	lf->flags |= FL_LEAF;

	nodeaddleaf(parent, lf);
        return 1;
	}

parse_sequence(name, fin)
	char *name;
	FILE *fin; {
	char buffer[512];
	char *s;

#if	DEBUG
	printf("parse_sequence(%s, ...)\n", name);
#endif

	/* do this till we run out of file
	 */	
	while(!feof(fin)) {
		/* read the next line
		 */
		if(fgets(buffer, 512, fin) == NULL)
			break;

		/* increment the line number
		 */
		lineno++;

		/* strip the trailing newline
		 */
		stripnl(buffer);

		/* if it's a comment or an empty line, ignore it
		 */
		if(ignoreline(buffer))
			continue;

		s = afterwhite(buffer);
		if(strncmp(s, "}", 1) == 0)
			return 1;
		}

	fprintf(stderr, "unexpected EOF\n");
	return 1;
	}

/* return !0 if this line should be silently ignored - for blank lines
 * and comments.
 */
ignoreline(s)
	char *s; {

	s = afterwhite(s);

	if(strlen(s) == 0) {
#if	DEBUG > 5
		printf("blank line %d\n", lineno);
#endif
		return !0;
		}

	if(strncmp(s, "--", 2) == 0) {
#if	DEBUG	> 5
		printf("comment %d\n", lineno);
#endif
		return !0;
		}

	return 0;
	}

char *skipblanklines(fin, buffer)
	FILE *fin;
	char *buffer; {

	while(!feof(fin)) {
		if(fgets(buffer, 512, fin) == 0) {
			fprintf(stderr, "unexpected EOF\n");
			exit(1);
			}

		lineno++;

		stripnl(buffer);

		if(ignoreline(buffer))
			continue;
		else {
			return afterwhite(buffer);
			}
		}
	}

/* parse the DEFAULT statement
 */
int use_leafm = 0;	/* indicates that we should use LEAFM() instead of LEAF() */

parse_default(n, s)
	struct node *n;
	char *s; {

	if(n->defaults == NULL)
		n->defaults = mkdefaults();

	s = afterwhite(s);
	if(strncmp(s, "set-function", sizeof("set-function") - 1) == 0) {
		s += sizeof("set-function") - 1;
		s = afterwhite(s);
		n->defaults->set_function = strdup(s);
		return 1;
		}

	if(strncmp(s, "test-function", sizeof("test-function") - 1) == 0) {
		s += sizeof("test-function") - 1;
		s = afterwhite(s);
		n->defaults->test_function = strdup(s);
		return 1;
		}

	if(strncmp(s, "get-function", sizeof("get-function") - 1) == 0) {
		s += sizeof("get-function") - 1;
		s = afterwhite(s);
		n->defaults->get_function = strdup(s);
		return 1;
		}

	if(strncmp(s, "next-function", sizeof("next-function") - 1) == 0) {
		s += sizeof("next-function") - 1;
		s = afterwhite(s);
		n->defaults->next_function = strdup(s);
		return 1;
		}

	if(strncmp(s, "cookie", sizeof("cookie") - 1) == 0) {
		s += sizeof("cookie") - 1;
		s = afterwhite(s);
		n->defaults->cookie = strdup(s);
		return 1;
		}

	if(strncmp(s, "locator", sizeof("locator") - 1) == 0) {
		s += sizeof("locator") - 1;
		s = afterwhite(s);
		n->defaults->locator = strdup(s);
		use_leafm = 1;
		return 1;
		}

	if(strncmp(s, "view-mask", sizeof("view-mask") - 1) == 0) {
		s += sizeof("view-mask") - 1;
		s = afterwhite(s);
		n->defaults->view_mask = strdup(s);
		use_leafm = 1;
		return 1;
		}

	if(strncmp(s, "write-mask", sizeof("write-mask") - 1) == 0) {
		s += sizeof("write-mask") - 1;
		s = afterwhite(s);
		n->defaults->write_mask = strdup(s);
		use_leafm = 1;
		return 1;
		}

	syntax_error("bad DEFAULT form", s);
	}
