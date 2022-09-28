/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)numbers.c	2.16 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)numbers.c	2.16 96/10/02 Sun Microsystems";
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
/* numbers.c
 */

/* $Header: /projects/mibcomp/numbers.c,v 1.3 90/11/03 13:07:06 romkey Exp $ */

/* $Log:	numbers.c,v $
 * Revision 1.3  90/11/03  13:07:06  romkey
 * got rid of sys/types.h include
 * 
 * Revision 1.2  90/09/17  22:38:19  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/11/03 13:07:06 $
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
#include <sys/types.h>
#include <time.h>

#include "mibcomp.h"
static void _write_numbers();

write_numbers(f, source_file)
	char *f;
	char *source_file; {
	FILE *fout;
	time_t now;

	fout = fopen(f, "w");
	if(fout == NULL) {
		perror(f);
		exit(1);
		}

	fprintf(fout, "; List of MIB nodes and leaves\n");
	fprintf(fout, "; generated automatically by Epilogue MIB Compiler\n");
	time(&now);
	fprintf(fout, "; from file %s, %s", source_file, ctime(&now));

	/* walk the tree. Start at the root and work down.
	 */
	if(root.kids == NULL)
		return 1;

	/* assume root has only one kid
	 */
	_write_numbers(fout, root.kids, "");
	}

static void _write_numbers(fout, tree, initial)
	FILE *fout;
	struct node *tree;
	char *initial; {
	struct node *kid;
	char objectid[128];

	sprintf(objectid, "%s%d", initial, tree->number);

	/* first, emit a node declaration if it's a node, or a leaf
	 * 	declaration if it's got no kids...
	 */
	if(tree->flags & FL_LEAF)
		fprintf(fout, "%-22s  %-26s  LEAF  %s\n", objectid, tree->name,
			tree->syntax->mib_name);
	else {
		fprintf(fout, "%-22s  %-26s  NODE\n", objectid, tree->name);
		strcat(objectid, ".");

		for(kid = tree->kids; kid; kid = kid->next)
			_write_numbers(fout, kid, objectid);
		}
	}
