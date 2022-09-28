/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)mibcomp.h	2.16 96/07/25 Sun Microsystems"
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
/* mibcomp.h
 */

/* $Header: /projects/mibcomp/mibcomp.h,v 1.3 90/10/02 15:32:01 romkey Exp $ */

/* $Log:	mibcomp.h,v $
 * Revision 1.3  90/10/02  15:32:01  romkey
 * added locator, view_mask and write_mask to default structure
 * added function declarations to get the default values for these items
 * 
 * Revision 1.2  90/09/17  23:13:02  romkey
 * added declaration of findwhite()
 * 
 * Revision 1.4  90/09/17  22:55:13  romkey
 * fixed usage line
 * 
 * Revision 1.3  90/09/17  22:38:18  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/10/02 15:32:01 $
 * $Revision: 1.3 $
 * $Author: romkey $
 */


/****************************************************************************
 *     Copyright (c) 1988  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************
 */

/* the node and leaf structures share a common section at the start
 */
struct node {
	char	*name;
	int	flags;
	int	number;
	struct node *next;

	struct node *parent;
	struct node *kids;
	struct node *lastkid;

	struct def	*defaults;

	struct type *syntax;
	char	*access;
	char	*status;
	};

/* valid type ("syntax") definitions
 */
struct type {
	char	*mib_name;
	char	*epilogue_name;
	char	*c_name;
	char	*c_mib;
	int	flags;
	};

/* default structure stores stuff...
 */
struct def {
	char *test_function;
	char *get_function;
	char *set_function;
	char *next_function;
	char *cookie;
	char *locator;
	char *view_mask;
	char *write_mask;
	};

/* node flags
 */
#define	FL_LEAF		0x0001
#define	FL_READABLE	0x0002
#define	FL_WRITEABLE	0x0004
#define	FL_GLOBAL	0x0008

/* type flags
 */
#define	TFL_USED	0x0001	/* this type was used */
#define	TFL_FORWARD	0x0002	/* forward declaration has been done */

/* common string handling functions
 */
void stripnl();
char *strdup();
int strcmpi();
char *afterwhite();
char *findwhite();

/* node manipulation functions
 */
struct node *mknode();
void freenode();
struct node *findnode();
void nodeaddleaf();

/* type functions
 */
struct type *lookup_type();
char *resolve_type();
char *default_testfunc();
char *default_getfunc();
char *default_setfunc();
char *default_nextfunc();
char *default_cookie();
char *default_locator();
char *default_view_mask();
char *default_write_mask();
struct def *mkdefaults();
int remember_include();
char *get_include();

extern int lineno;
extern struct node root;
extern struct type types[];
extern int use_leafm;
