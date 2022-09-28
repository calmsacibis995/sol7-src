/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)sem_xlat.c 1.15	97/11/22  SMI"

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
static char rcsid[] = "@(#)$RCSfile: sem_xlat.c,v $ $Revision: 1.4.2.3 $"
	" (OSF) $Date: 1992/02/18 20:26:08 $";
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
 * 1.4  com/cmd/nls/sem_xlat.c, cmdnls, bos320, 9138320 9/11/91 16:35:09
 */

#include <sys/localedef.h>
#include <string.h>
#include "semstack.h"
#include "err.h"
#include "locdef.h"

extern void	compress_transtabs(_LC_ctype_t *, int);

/*
*  FUNCTION: add_upper
*
*  DESCRIPTION:
*  Build the 'upper' character translation tables from the symbols on the
*  semantic stack.
*/
void
add_upper(_LC_ctype_t *ctype)
{
	extern wchar_t max_wchar_enc;
	item_t *it;
	int i;

	/* check if upper array allocated yet - allocate if NULL */
	if (ctype->upper == NULL)
		ctype->upper = MALLOC(wchar_t, max_wchar_enc + 1);

	/* set up default translations - which is identity */
	for (i = 0; i <= max_wchar_enc; i++)
		ctype->upper[i] = i;

	/* for each range on stack - the min is the FROM pc, and the max is */
	/* the TO pc. */
	while ((it = sem_pop()) != NULL) {
		ctype->upper[it->value.range->min] = it->value.range->max;
	}
}


/*
*  FUNCTION: add_lower
*
*  DESCRIPTION:
*  Build the 'lower' character translation tables from the symbols on the
*  semantic stack.
*/
void
add_lower(_LC_ctype_t *ctype)
{
	extern wchar_t max_wchar_enc;
	item_t *it;
	int i;

	/* check if lower array allocated yet - allocate if NULL */
	if (ctype->lower == NULL)
		ctype->lower = MALLOC(wchar_t, max_wchar_enc + 1);

	/* set up default translations which is identity */
	for (i = 0; i <= max_wchar_enc; i++)
		ctype->lower[i] = i;

	/* for each range on stack - the min is the FROM pc, and the max is */
	/* the TO pc. */
	while ((it = sem_pop()) != NULL) {
		ctype->lower[it->value.range->min] = it->value.range->max;
	}
}

/*
*  FUNCTION: sem_push_xlat
*
*  DESCRIPTION:
*  Creates a character range item from two character reference items.
*  The routine pops two character reference items off the semantic stack.
*  These items represent the "to" and "from" pair for a character case
*  translation.  The implementation uses a character range structure to
*  represent the pair.
*/
void
sem_push_xlat(void)
{
	item_t   *it0, *it1;
	item_t   *it;
	it1 = sem_pop();	/* this is the TO member of the pair */
	it0 = sem_pop();	/* this is the FROM member of the pair */

	/* this creates the item and sets the min and max to wc_enc */

	if (it0->type == it1->type)	/* Same type is easy case */
		switch (it0->type) {
		case SK_CHR:
			it = create_item(SK_RNG, it0->value.chr->wc_enc,
				it1->value.chr->wc_enc);
			break;
		case SK_INT:
			it = create_item(SK_RNG, it0->value.int_no,
				it1->value.int_no);
			break;
		default:
			INTERNAL_ERROR;		/* NEVER RETURNS */
		}
	/*
	 * Not same types, we can coerce INT and CHR into a valid range
	 */
	else if (it0->type == SK_CHR && it1->type == SK_INT)
		it = create_item(SK_RNG, it0->value.chr->wc_enc,
			it1->value.int_no);
	else if (it0->type == SK_INT && it1->type == SK_CHR)
		it = create_item(SK_RNG, it0->value.int_no,
			it1->value.chr->wc_enc);
	else
		INTERNAL_ERROR;

	destroy_item(it1);
	destroy_item(it0);

	(void) sem_push(it);
}

/*
 * FUNCTION: add_transformation
 *
 * DESCRIPTION:
 * This function and compress_transtabs() in sem_comp.c are strongly
 * related to the implementaion of towctrans, towupper, and towlower.
 * localedef must guarantee that it generates the transformation
 * tables of toupper and tolower in the fixed position, that is,
 * it must keep the following:
 * toupper table is in the 1st entry.
 * tolower table is in the 2nd entry.
 */
/* ARGSUSED1 */
void
add_transformation(_LC_ctype_t *ctype, struct lcbind_table *lcbind_table,
	char *ctype_symbol_name)
{
	extern wchar_t max_wchar_enc;
	item_t *it;
	int	slot;
	int i;
	int do_mask_check;
	uint from_mask;
	uint to_mask;

	/* check if array allocated yet - allocate if NULL */
	if (ctype->transname == (_LC_transnm_t *) NULL) {
		ctype->transname = MALLOC(_LC_transnm_t, 20);
		ctype->transtabs = MALLOC(_LC_transtabs_t, 20);
		/* toupper and tolower always exist in the locale. */
		/* So start from 2 */
		ctype->ntrans = 2;
	}

/*
 *	slot0 (index == 1) and slot1 (index == 2) need to be reserved
 *	for toupper and tolower
 */

	if (strcmp("toupper", ctype_symbol_name) == 0) {
		slot = 0;
	} else if (strcmp("tolower", ctype_symbol_name) == 0) {
		slot = 1;
	} else {
		slot = ctype->ntrans;
		ctype->ntrans++;
	}

/* craigm - if it's full reallocate it */
#if 0
	if (ctype->ntrans == 20) {
		realloc
	}
#endif

/* craigm */
/* lookup existing transname entry and add to it */

	/* allocate transtab vector */
	if (ctype->transtabs[slot].table == (wchar_t *) NULL) {

		ctype->transtabs[slot].tmax = max_wchar_enc;
		if (((strcmp("toupper", ctype_symbol_name) == 0) ||
			(strcmp("tolower", ctype_symbol_name) == 0)) &&
			(ctype->transtabs[slot].tmax < 255))
			ctype->transtabs[slot].tmax = 255;

		ctype->transtabs[slot].tmin = 0;
		ctype->transtabs[slot].table =
			MALLOC(wchar_t, ctype->transtabs[slot].tmax + 1);
		ctype->transname[slot].name = strdup(ctype_symbol_name);
		ctype->transname[slot].index = slot;
	}


	/* set up default translations which is identity */
	for (i = 0; i <= ctype->transtabs[slot].tmax; i++)
		ctype->transtabs[slot].table[i] = i;

	/*
	 * setup work for checking if the characters are in lower and
	 * upper if doing tolower or tolower transformations
	 */
	if (strcmp("toupper", ctype_symbol_name) == 0) {
		do_mask_check = TRUE;
		from_mask = _ISLOWER;
		to_mask   = _ISUPPER;
	} else if (strcmp("tolower", ctype_symbol_name) == 0) {
		do_mask_check = TRUE;
		from_mask = _ISUPPER;
		to_mask   = _ISLOWER;
	} else {
		do_mask_check = FALSE;
		from_mask = 0xffffffff;
		to_mask   = 0xffffffff;
	}

	/* for each range on stack - the min is the FROM pc, and the max is */
	/* the TO pc. */
	while ((it = sem_pop()) != NULL) {
	/*
	 * check if the characters are in lower and upper if
	 * doing tolower or toupper transformations.
	 */
		if ((do_mask_check == FALSE) ||
			((do_mask_check == TRUE) &&
			(ctype->mask[it->value.range->min] & from_mask) &&
			(ctype->mask[it->value.range->max] & to_mask)))
			ctype->transtabs[slot].table[it->value.range->min] =
				it->value.range->max;
		else
			diag_error(ERR_TOU_TOL_ILL_DEFINED);
	}

	/*
	 * Search the translation for the last character that is case sensitive
	 */
	for (i = ctype->transtabs[slot].tmax; i > 0; i--)
		if (i != ctype->transtabs[slot].table[i])
			break;

	ctype->transtabs[slot].tmax = i;

	/* Check to see if there is value greater than tmax */
	for (; i >= 0; i--)
		if (ctype->transtabs[slot].tmax <
			ctype->transtabs[slot].table[i])
			ctype->transtabs[slot].tmax =
				ctype->transtabs[slot].table[i];

	/*
	 * Search the translation for the first character that is case sensitive
	 */

	for (i = 0; i <= ctype->transtabs[slot].tmax; i++)
		if (i != ctype->transtabs[slot].table[i])
			break;

	ctype->transtabs[slot].tmin = i;

	/* Check to see if there is a value smaller than tmin */
	for (; i <= ctype->transtabs[slot].tmax; i++)
		if (ctype->transtabs[slot].table[i] <
			ctype->transtabs[slot].tmin)
			ctype->transtabs[slot].tmin =
				ctype->transtabs[slot].table[i];
	/*
	 * Do the same for the low end but NOT for "toupper" and "tolower"
	 */
	if (strcmp("toupper", ctype->transname[slot].name) == 0) {
		if (ctype->transtabs[slot].tmax < 255) {
			ctype->transtabs[slot].tmax = 255;
		}
		ctype->transtabs[slot].tmin = 0;
	} else if (strcmp("tolower", ctype->transname[slot].name) == 0) {
		if (ctype->transtabs[slot].tmax < 255) {
			ctype->transtabs[slot].tmax = 255;
		}
		ctype->transtabs[slot].tmin = 0;
	}

	compress_transtabs(ctype, slot);

	if (strcmp("toupper", ctype->transname[slot].name) == 0) {
		ctype->upper = ctype->transtabs[slot].table;
		ctype->max_upper = ctype->transname[slot].tmax;
	} else if (strcmp("tolower", ctype->transname[slot].name) == 0) {
		ctype->lower = ctype->transtabs[slot].table;
		ctype->max_lower = ctype->transname[slot].tmax;
	}


}
