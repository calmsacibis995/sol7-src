/* Copyright 1988 - 07/25/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)scan.h	2.16 96/07/25 Sun Microsystems"
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
/* scan.h
 */

/* tokens are created by the lexical scanner and presented to the parser.
 */

struct token {
	int type;
	char *text;
	};

#define	TK_OPENCURLY
#define	TK_CLOSECURLY
#define	TK_OPENPAREN
#define	TK_CLOSEPAREN
#define	TK_DEFAULT
#define	TK_INCLUDE
#define	TK_DEFINITIONS
#define	TK_COLONCOLONEQ
#define	TK_BEGIN
#define	TK_IMPORTS
#define	TK_OBJECT_ID
#define	TK_OBJECT_TYPE
#define	TK_SYNTAX
#define	TK_ACCESS
#define	TK_STATUS
#define	TK_SEQUENCE
#define	TK_OF
#define	TK_COMMA
#define	TK_END
#define	TK_string
#define	TK_number

struct token *tk_make(type, string);
void tk_free(struct token *);
