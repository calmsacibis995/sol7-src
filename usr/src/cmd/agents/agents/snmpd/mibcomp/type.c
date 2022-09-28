/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)type.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)type.c	2.15 96/07/23 Sun Microsystems";
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
/* type.c 
 */

/* $Header: /projects/mibcomp/type.c,v 1.3 90/09/17 23:12:53 romkey Exp $ */

/* $Log:	type.c,v $
 * Revision 1.3  90/09/17  23:12:53  romkey
 * installed Stuart Vance's parser fix, with findwhite()
 * 
 * Revision 1.2  90/09/17  22:38:25  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/09/17 23:12:53 $
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

#include "mibcomp.h"

struct type types[] = {
	{ "displaystring", "VT_STRING", "int", "string", 0 },
	{ "integer",	"VT_NUMBER", "INT_32_T", "int", 0 },
	{ "octet string", "VT_STRING", "int", "string", 0 },
	{ "object identifier", "VT_OBJECT", "int", "object_identifier", 0 },
	{ "timeticks", "VT_TIMETICKS", "int", "ulong", 0 },
	{ "gauge", "VT_GAUGE", "int", "int", 0 },
	{ "counter", "VT_COUNTER", "int", "ulong",  0 },
	{ "networkaddress", "VT_IPADDRESS", "int", "ip_address", 0 },
	{ "ipaddress", "VT_IPADDRESS", "int", "ip_address", 0 },
	{ NULL, NULL }
	};

char *strchr();

/* this routine does somewhat of a special hack. If it can't find the name,
 * then this *might* be a reference to a sequence, so it creates a special
 * type structure and returns it, and later on the output_type() function
 * will check for certain.
 */
struct type *lookup_type(syntax)
	register char *syntax; {
	register struct type *t;
	char *u;
	int len;

	/* catch cases like INTEGER { foo(1), bar(2), frotz(3) } and
	 * INTEGER(0...32)
	 */
	if(strncmp(syntax, "INTEGER", 7) == 0)
		syntax = "integer";

	u = findwhite(syntax);
	if(u) {
		len = u - syntax;
		}
	else
		len = strlen(syntax);

	for(t = types; t->mib_name; t++)
		if(strncmpi(syntax, t->mib_name, len) == 0) {
			t->flags |= TFL_USED;
			return t;
			}

	t = (struct type *)malloc(sizeof(struct type));
	t->mib_name = strdup(syntax);
	t->epilogue_name = NULL;
	return t;
	}

char *resolve_type(n)
	struct node *n; {
	struct type *t = n->syntax;

	if(t->epilogue_name)
		return t->epilogue_name;
	else {
		fprintf(stderr, "Unknown type %s for node %s\n",
			t->mib_name, n->name);
		exit(1);
		}
	}

char *c_type(n)
	struct node *n; {
	struct type *t = n->syntax;

	if(t->c_name)
		return t->c_name;
	else {
		fprintf(stderr, "Unknown type %s for node %s\n",
			t->mib_name, n->name);
		exit(1);
		}
	}
