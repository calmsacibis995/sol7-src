/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)include.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)include.c	2.15 96/07/23 Sun Microsystems";
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
/* include.c
 */

/* $Header: /projects/mibcomp/include.c,v 1.2 90/09/17 22:38:13 romkey Exp $ */

/* $Log:	include.c,v $
 * Revision 1.2  90/09/17  22:38:13  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/09/17 22:38:13 $
 * $Revision: 1.2 $
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

#define	MAX_INCLUDES	20

char *strdup();

static char *includes[MAX_INCLUDES];
static int next_include = 0;
static int next_read = 0;

int remember_include(s)
	char *s; {

	if(next_include == MAX_INCLUDES)
		return -1;

	includes[next_include++] = strdup(s);
	return 0;
	}

char *get_include(s)
	char *s; {

	if(next_read == next_include)
		return NULL;

	return includes[next_read++];
	}
