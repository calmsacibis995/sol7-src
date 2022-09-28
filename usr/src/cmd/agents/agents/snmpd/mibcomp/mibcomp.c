/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)mibcomp.c	2.16 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)mibcomp.c	2.16 96/10/02 Sun Microsystems";
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
/* mibcomp.c
 */

/* $Header: /projects/mibcomp/mibcomp.c,v 1.4 90/09/17 22:55:13 romkey Exp Locker: romkey $ */

/* $Log:	mibcomp.c,v $
 * Revision 1.4  90/09/17  22:55:13  romkey
 * fixed usage line
 * 
 * Revision 1.3  90/09/17  22:38:18  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/09/17 22:55:13 $
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

/* usage: mibcomp MIB-definitions-file
 *	creates mibout.c, defining the MIB tree
 */

enum functions { MIB_C, SKELETON, NUMBERS, MANAGER, MANAGER_H, ARRAY };

struct node root;
enum functions function = MIB_C;

int lineno = 0;

void usage() {
	fprintf(stderr, "usage: mibcomp [-o output_file] [-skel] [-man] [-man.h] [-numbers] [-array] MIB-file\n");
	exit(1);
	}

void main(argc, argv)
	int argc;
	char *argv[]; {
	char *outfile = "OUTPUT";
	FILE *fin;

	printf("Epilogue Technology SNMP MIB Compiler version 1.1\n");
	printf("Copyright (c) 1989,1990 by Epilogue Technology Corporation.\n");

#ifdef	THINK_C
	agrc = ccommand(&argv);
#endif /* THINK_C */

	if(argc == 1)
		usage();

	/* make sure we've got an argument
	 */
	while(argc > 1) {
		argc--; argv++;

		if(strcmp(argv[0], "-o") == 0) {
			if(argc == 0)
				usage();

			outfile = argv[1];
			argc--; argv++;
			continue;
			}
		if(strcmp(argv[0], "-skel") == 0) {
			function = SKELETON;
			continue;
			}
		if(strcmp(argv[0], "-numbers") == 0) {
			function = NUMBERS;
			continue;
			}
		if(strcmp(argv[0], "-man") == 0) {
			function = MANAGER;
			continue;
			}

		if(strcmp(argv[0], "-man.h") == 0) {
			function = MANAGER_H;
			continue;
			}
		if(strcmp(argv[0], "-array") == 0) {
			function = ARRAY;
			continue;
			}
		break;
		}

	if(argc != 1)
		usage();

	root.name = strdup("mib_root");
	root.number = 0;
	root.flags = FL_GLOBAL;
	root.kids = NULL;
	root.lastkid = NULL;
	root.defaults = NULL;
	root.parent = NULL;

	fin = fopen(argv[0], "r");
	if(fin == NULL) {
		perror(argv[0]);
		exit(1);
		}


	scan_input(fin);
	switch(function) {
	      case SKELETON:
		write_skel(outfile, argv[0]);
		break;
	      case MANAGER:
		write_man(outfile, argv[0]);
		break;
	      case MANAGER_H:
		write_manh(outfile, argv[0]);
		break;
	      case MIB_C:
		write_mib(outfile, argv[0]);
		break;
	      case NUMBERS:
		write_numbers(outfile, argv[0]);
		break;
	      case ARRAY:
		write_array(outfile, argv[0]);
		break;
	}

#if	  0
	dump_mib();	
#endif /* 0 */

	exit(0);
	}
