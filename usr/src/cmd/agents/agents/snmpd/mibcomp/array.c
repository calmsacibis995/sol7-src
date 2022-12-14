/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)array.c	2.16 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)array.c	2.16 96/10/02 Sun Microsystems";
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
/* array.c
 */

/* $Header: /projects/mibcomp/array.c,v 1.5 90/11/03 13:07:40 romkey Exp Locker: romkey $ */

/* $Log:	array.c,v $
 * Revision 1.5  90/11/03  13:07:40  romkey
 * got rid of sys/types.h include
 * 
 * Revision 1.4  90/09/18  15:48:31  romkey
 * fixed problem where spaces could be embedded in object types
 * 
 * Revision 1.3  90/09/17  22:37:59  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/11/03 13:07:40 $
 * $Revision: 1.5 $
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

/* This file contains the code which writes a file of stub functions for 
 * a manager; eg: fetch_sysDescr, store_sysName, and also a set of arrays
 * providing the manager with an easy way to access the object id's.
 */

static void _write_array();

static char *fixspace();

write_array(f, source_file)
	char *f;
	char *source_file; {
	FILE *fout;
	time_t now;
	char *s;
	int i;

	fout = fopen(f, "w");
	if(fout == NULL) {
		perror(f);
		exit(1);
		}

	fprintf(fout, "/*****************************************************************************\n");
	fprintf(fout, " *****************************************************************************\n");
	fprintf(fout, " ****\t\t\tWARNING\n");
	fprintf(fout, " ****\t\n");
	fprintf(fout, " ****\tThis file is automatically generated by the Epilogue Technology\n");
	fprintf(fout, " ****\t MIB compiler.\n");
	fprintf(fout, " ****\t\n");
	fprintf(fout, " ****\tThis file contains functions to allow a manager to fetch and store\n");
	fprintf(fout, " ****\tMIB variables, and their object ID's.\n");
	fprintf(fout, " ****\t\n");
	fprintf(fout, " ****\tDO NOT MODIFY THIS FILE BY HAND\n");
	fprintf(fout, " ****\n");

	time(&now);
	fprintf(fout, " **** Last build date: %s", ctime(&now));
	fprintf(fout, " ****\tfrom file %s\n", source_file);
	fprintf(fout, " *****************************************************************************\n");
	fprintf(fout, " *****************************************************************************\n");

	fprintf(fout, " */\n\n\n");

	fprintf(fout, "#include <asn1.h>\n");
	fprintf(fout, "#include <snmp.h>\n");
	fprintf(fout, "#include <mib.h>\n");
	fprintf(fout, "#include <libfuncs.h>\n\n");

	fprintf(fout, "#include \"ids.h\"\t/* generated with -man.h*/\n");

	if(root.kids == NULL) {
		fprintf(stderr, "warning: empty MIB\n");
		return 1;
		}

	/* generate all the object ID's
	 */
	fprintf(fout, "struct oidtab {\n");
	fprintf(fout, "\tOIDC_T *oid;\n");
	fprintf(fout, "\tchar *name;\n");
	fprintf(fout, "\tint len;\n");
	fprintf(fout, "\tint type;\n");
	fprintf(fout, "\t};\n\n");

	for(i = 0; ; i++) {
		if(types[i].mib_name == NULL)
			break;

		fprintf(fout, "#define\tTYPE_%s\t%d\n",
			fixspace(types[i].mib_name), i);
		}

	fprintf(fout, "\n");

	fprintf(fout, "#ifndef NULL\n");
	fprintf(fout, "#define NULL (char *)0\n");
	fprintf(fout, "#endif /* NULL */\n");

	fprintf(fout, "\n");

	fprintf(fout, "#ifdef	C_CODE\n");
	fprintf(fout, "struct oidtab oids[] = {\n");
	_write_array(fout, root.kids, "", 0);
	fprintf(fout, "\t{ NULL, NULL, 0, 0 }\n");
	fprintf(fout, "\t};\n");
	fprintf(fout, "#else\n");
	fprintf(fout, "extern struct oidtab oids[];\n");
	fprintf(fout, "#endif\n");
	
	fprintf(fout, "\n\n");
#if	0
	fprintf(fout, "unsigned int req_id = 0;\n\n");

	/* walk the tree. Start at the root and work down.
	 */
	_write_mib(fout, &root);
#endif
	}

static void _write_array(fout, tree, initial, idlen)
	FILE *fout;
	struct node *tree;
	char *initial;
	unsigned idlen; {
	struct node *kid;
	char objectid[128];

	idlen++;

	/* first, emit a node declaration if it's a node, or a leaf
	 * 	declaration if it's got no kids...
	 */
	if(tree->flags & FL_LEAF) {
		fprintf(fout, "{ ID_%s, \"%s\", %d, ",
			fixspace(tree->name), tree->name, idlen);
		fprintf(fout,"TYPE_%s },\n", fixspace(tree->syntax->mib_name));
		}
	else {
		for(kid = tree->kids; kid; kid = kid->next)
			_write_array(fout, kid, objectid, idlen);
		}
	}

static char *fixspace(str)
	char *str; {
	static char holder[64];

	strcpy(holder, str);
	for(str = holder; *str; str++)
		if((*str == ' ') || (*str == '-'))
			*str = '_';

	return holder;
	}
