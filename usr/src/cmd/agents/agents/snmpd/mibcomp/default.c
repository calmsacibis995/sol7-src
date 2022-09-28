/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)default.c	2.17 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)default.c	2.17 96/10/02 Sun Microsystems";
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
/* default.c
 */

/* $Header: /projects/mibcomp/default.c,v 1.3 90/10/02 15:32:28 romkey Exp $ */

/* $Log:	default.c,v $
 * Revision 1.3  90/10/02  15:32:28  romkey
 * added functions to get default values for locator, view_mask and write_mask
 * 
 * Revision 1.2  90/09/17  22:38:12  romkey
 * added copyright, filename and RCS info comments
 * 
 *
 * $Date: 90/10/02 15:32:28 $
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

/* resolve(buf, "foo %t", "name", "type") -> buf is "foo type"
 * resolve(buf, "foo %n", "name", "type") -> buf is "foo name"
 * resolve(buf, "foo %%", "name", "type") -> buf is "foo %"
 * resolve(buf, "foo %", "name", "type") -> buf is "foo %"
 */
void resolve(buf, form, name, pname, dname, type)
	char *buf;
	char *form;
	char *name;
	char *pname;
	char *dname;
	char *type; {
int permloop=1;

	while(permloop) {
		if(*form != '%') {
			*buf++ = *form;
			if(*form++ == '\0')
				return;

			continue;
			}

		form++;
		switch(*form) {
		      case '%':
			*buf++ = '%';
			form++;
			continue;
		      case '\0':
			*buf++ = '%';
			*buf++ = '\0';
			form++;
			return;
		      case 't':
			if(type == NULL) {
				fprintf(stderr, "null type for %s\n", name);
				exit(1);
				}

			strcpy(buf, type);
			buf += strlen(type);
			form++;
			continue;
		      case 'n':
			strcpy(buf, name);
			buf += strlen(name);
			form++;
			continue;
		      case 'p':
			strcpy(buf, pname);
			buf += strlen(pname);
			form++;
			continue;
		      case 'd':
			strcpy(buf, dname);
			buf += strlen(dname);
			form++;
			continue;
		      default:
			*buf++ = *form++;
			continue;
			}
		}
	}

char *default_testfunc(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	n = target;

	/* start searching for a usable default
	 */

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->test_function == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->test_function, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->test_function, target->name,
				target->parent->name, n->name,
				(char *)NULL);


		fflush(stdout);

		return buf;
		}

	fprintf(stderr, "fatal: no get function for %s\n", target->name);
	exit(1);

	return NULL;
	}

char *default_getfunc(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	n = target;
	if(!(n->flags & FL_READABLE))
		return "null_get_proc";

	/* start searching for a usable default
	 */

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->get_function == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->get_function, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->get_function, target->name,
				target->parent->name, n->name,
				(char *)NULL);


		fflush(stdout);

		return buf;
		}

	fprintf(stderr, "fatal: no get function for %s\n", target->name);
	exit(1);

	return NULL;
	}

char *default_setfunc(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	n = target;
	if(!(n->flags & FL_WRITEABLE))
		return "null_set_proc";

	/* start searching for a usable default
	 */

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->set_function == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->set_function, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->set_function, target->name,
				target->parent->name, n->name,
				(char *)NULL);

		return buf;
		}

	fprintf(stderr, "fatal: no set function for %s\n", target->name);
	exit(1);

	return NULL;
	}

char *default_nextfunc(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	/* start searching for a usable default
	 */
	n = target;

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->next_function == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->next_function, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->next_function, target->name,
				target->parent->name, n->name,
				(char *)NULL);
			
		return buf;
		}

	fprintf(stderr, "fatal: no next function for %s\n", target->name);
	exit(1);

	return NULL;
	}

char *default_cookie(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	/* start searching for a usable default
	 */
	n = target;

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->cookie == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->cookie, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->cookie, target->name,
				target->parent->name, n->name,
				(char *)NULL);

		return buf;
		}

	fprintf(stderr, "fatal: no cookie for %s\n", target->name);
	exit(1);

	return NULL;
	}

/* get the default locator for a node. If none was set, use 0x0000.
 */
char *default_locator(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	/* start searching for a usable default
	 */
	n = target;

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->locator == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->locator, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->locator, target->name,
				target->parent->name, n->name,
				(char *)NULL);

		return buf;
		}

	return "0x0000";
	}

/* get the default view mask for a node. If none was set, use 0xFF.
 */
char *default_view_mask(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	/* start searching for a usable default
	 */
	n = target;

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->view_mask == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->view_mask, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->view_mask, target->name,
				target->parent->name, n->name,
				(char *)NULL);

		return buf;
		}

	return "0xFF";

	/*return NULL; warning: statement not reached */
	}

/* get the default write mask for a node. If none was set, use 0xFF
 */
char *default_write_mask(target)
	struct node *target; {
	struct node *n;
	static char buf[128];

	/* start searching for a usable default
	 */
	n = target;

	while(n) {
		/* if there exists no suitable default at this level, bop
		 * up one and inherit its defaults
		 */
		if((n->defaults == NULL)||(n->defaults->write_mask == NULL)){
			n = n->parent;
			continue;
			}

		if(target->syntax)
			resolve(buf, n->defaults->write_mask, target->name,
				target->parent->name, n->name,
				target->syntax->c_mib);
		else
			resolve(buf, n->defaults->write_mask, target->name,
				target->parent->name, n->name,
				(char *)NULL);

		return buf;
		}

	return "0xFF";
	}

struct def *mkdefaults() {
	struct def *d;

	d = (struct def *)malloc(sizeof(struct def));
	d->get_function = NULL;
	d->set_function = NULL;
	d->next_function = NULL;
	d->test_function = NULL;
	d->cookie = NULL;
	d->locator = NULL;
	d->view_mask = NULL;
	d->write_mask = NULL;

	return d;
	}

#define	ACC_MAX		1024

static char *accumulator[ACC_MAX];
static int accindex = 0;

void accumulate_add(s)
	char *s; {

	accumulator[accindex++] = strdup(s);
	if(accindex == ACC_MAX) {
		fprintf(stderr, "forward reference accumulator overflow\n");
		exit(1);
		}
	}

int accumulate_check(s)
	register char *s; {
	int i;

	for(i = 0; i < accindex; i++)
		if(strcmp(s, accumulator[i]) == 0)
		   return 1;

	return 0;
	}
