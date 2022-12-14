/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)symtab.c 1.7	97/11/17  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: symtab.c,v $ $Revision: 1.3.2.3 $"
	" (OSF) $Date: 1992/02/18 20:26:18 $";
#endif
/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS:
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.2  com/cmd/nls/symtab.c, bos320 5/10/91 10:18:07
 */

#include "symtab.h"
#include "err.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>

static int	cmp_symbol(symbol_t *, char *, int);

/*
*  FUNCTION: cmp_symbol
*
*  DESCRIPTION:
*  Comparator used to determine equivalence of two symbols.
*/
static int
cmp_symbol(symbol_t *sym, char *id, int scope)
{
	int  c;

	c = strcmp(sym->sym_id, id);
	if (c == 0)
		return (sym->sym_scope - scope);
	else
		return (c);
}


/*
*  FUNCTION: create_symbol
*
*  DESCRIPTION:
*  Creates an instance of a symbol_t.  This routine malloc()s the space
*  necessary to contain the symbol_t but not for it's data.
*/
symbol_t *
create_symbol(char *id, int scope)
{
	symbol_t *sym;

	sym = MALLOC(symbol_t, 1);
	sym->sym_id = strdup(id);

	sym->sym_scope = scope;
	sym->data.str = NULL;
	sym->next = NULL;

	return (sym);
}


/*
*  FUNCTION: hash
*
*  DESCRIPTION:
*  The hash function used to determine in which hash bucket a symbol will
*  go.  This hash function is particularly well suited to symbols which use
*  patterns of numbers, e.g. <j2436>, due to POSIX's recommendation that
*  character names be so defined.  Symbols containing the same numbers in
*  different positions is handled by making characters positionally
*  significant.
*/
int
hash(char *id)
{
	int i = 0;
	int hashval = 0;

	while (*id != '\0') {
		i++;
		hashval += *id * i;
		id++;
	}

	hashval &= 0x7FFFFFFF;
	hashval %= HASH_TBL_SIZE;

	return (hashval);
}


/*
*  FUNCTION: add_symbol
*
*  DESCRIPTION:
*  Adds a symbol to the symbol table.  The symbol table is implemented
*  as a hash table pointing to linked lists of symbol entries.
*
*      +--- array of pointers to lists indexed by hash(symbol->sym_id)
*      |
*      |
*      V      /--linked list of symbols --\
*  +------+   +------+-+         +------+-+
*  |    --+-->|      |-+-- ...-->|      |^|
*  +------+   +------+_+         +------+-+
*  |    --+--> . . .
*  +------+
*     .
*     .
*     .
*  +------+   +------+-+         +------+-+
*  |    --+-->|      |-+-- ...-->|      |^|
*  +------+   +------+-+         +------+-+
*/
int
add_symbol(symtab_t *sym_tab, symbol_t *sym)
{
	symbol_t *p;
	symbol_t *t;
	int	c;
	int	hdx;

	hdx = hash(sym->sym_id);
	t = &(sym_tab->symbols[hdx]);

	c = -1;
	for (p = t;
		p->next != NULL &&
		(c = cmp_symbol(p->next, sym->sym_id, sym->sym_scope)) < 0;
		p = p->next)	/* LINTED */
		;

	if (c == 0)
		return (ST_DUP_SYMBOL);
	else {
		sym_tab->n_symbols++;
		sym->next = p->next;
		p->next = sym;
		return (ST_OK);
	}
}


/*
*  FUNCTION: loc_symbol
*
*  DESCRIPTION:
*  Locates a symbol with sym_id matching 'id' in the symbol table 'sym_tab'.
*  The functions hashes 'id' and searches the linked list indexed for
*  a matching symbol.  See comment for add_symbol for detail of symbol
*  table structure.
*/
symbol_t *
loc_symbol(symtab_t *sym_tab, char *id, int scope)
{
	symbol_t *p;

	int	c;
	int	hdx;

	hdx = hash(id);
	p = &sym_tab->symbols[hdx];

	for (p = p->next;		/* Top of hash isn't a real symbol_t */
		p != NULL;
		p = p->next) {

		/* Walk thru the hash chain looking for the matching symbol */

		c = cmp_symbol(p, id, scope);

		if (c == 0)		/* MATCH */
			return (p);
		else if (c > 0)		/* OVER-RUN, none possible */
			return (NULL);
	}

	return (NULL);
}


/* static implementing symbol stack */
static size_t	stack_top = 0;
static symbol_t	**stack = NULL;
static size_t	max_stack = 0;
#define	DEF_STACK_SIZE	4
#define	MAX_STACK_SIZE	(SIZE_MAX / (sizeof (symbol_t *)))
/*
*  FUNCTION: sym_push
*
*  DESCRIPTION:
*  Pushes a symbol on the symbol stack.
*/
int
sym_push(symbol_t *sym)
{
	if (stack_top < max_stack) {
		stack[stack_top++] = sym;
		return (ST_OK);
	} else {	/* stack overflow or not initialized */
		if (max_stack == 0) {	/* not initialized */
			stack = MALLOC(symbol_t *, DEF_STACK_SIZE);
			max_stack = DEF_STACK_SIZE;
#ifdef SDEBUG
			fprintf(stderr, "sym stack initialized: %d\n",
				max_stack);
#endif
		} else if (max_stack == MAX_STACK_SIZE) {
			/* stack cannot be expanded any more */
			INTERNAL_ERROR;
			/* NOTREACHED */
		} else {		/* stack overflow */
			size_t	new_size;
			symbol_t	**new_stack;

			if (MAX_STACK_SIZE - max_stack > max_stack) {
				new_size = max_stack * 2;
			} else {
				new_size = MAX_STACK_SIZE;
			}
			new_stack = (symbol_t **)realloc((void *)stack,
				sizeof (symbol_t *) * new_size);
			if (new_stack == NULL) {
				error(ERR_MEM_ALLOC_FAIL, __FILE__, __LINE__);
				/* NOTREACHED */
			}
#ifdef SDEBUG
			fprintf(stderr, "sym stack realloced: %d\n", new_size);
#endif
			stack = new_stack;
			max_stack = new_size;
		}
		stack[stack_top++] = sym;
		return (ST_OK);
	}
}


/*
*  FUNCTION: sym_pop
*
*  DESCRIPTION:
*  Pops a symbol off the symbol stack, returning it's address to the caller.
*/
symbol_t *
sym_pop(void)
{
	if (stack_top != 0)
		return (stack[--stack_top]);
	else
		return (NULL);
}
