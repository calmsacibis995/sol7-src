/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__strxfrm_sb.c 1.9	97/04/09  SMI"

/*LINTLIBRARY*/

/*
#if !defined(lint) && !defined(_NOIDENT)
static char sccsid[] = "@(#)73	1.6.2.2  "
	"src/bos/usr/ccs/lib/libc/__strxfrm_sb.c, bos, bos410 1/12/93 11:11:06";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCSTR) Standard C Library String Handling Functions
 *
 * FUNCTIONS: __strxfrm_sb
 *
 * ORIGINS: 27
 *
 * IBM CONFIDENTIAL -- (IBM Confidential Restricted when
 * combined with the aggregated modules for this product)
 * OBJECT CODE ONLY SOURCE MATERIALS
 * (C) COPYRIGHT International Business Machines Corp. 1989,1992
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <sys/types.h>
#include <sys/localedef.h>
#include <string.h>
#include <stdlib.h>
#include <alloca.h>
#include "libc.h"

static size_t forward_xfrm_sb(_LC_collate_t *, const char *, char *,
				size_t, size_t, int);
static size_t forw_pos_xfrm_sb(_LC_collate_t *, const char *, char *,
				size_t, size_t, int);

size_t
__strxfrm_sb(_LC_collate_t *hdl, char *str_out, const char *str_in, size_t n)
{
	char *str_in_ptr;
	int cur_order;
	char *str_in_rep = NULL;
	size_t rev_start;
	int i;
	char save[5];
	size_t count = 0;
	int sort_mod;
	size_t copy_start;
	int xfrm_byte_count;
	size_t limit;

	/* Loop thru the orders. */
	for (cur_order = 0; cur_order <= (int)hdl->co_nord; cur_order++) {
		/* Get the sort modifier for this order. */
		sort_mod = hdl->co_sort[cur_order];

		/* If this order uses replacement strings, set them up. */
		if (hdl->co_nsubs && (sort_mod & _COLL_SUBS_MASK)) {
			if (! str_in_rep)
				str_in_rep = (char *)alloca(strlen(str_in) *
								2 + 20);
			str_in_ptr = do_replacement(hdl, str_in, cur_order,
							str_in_rep);
		} else
			str_in_ptr = (char *)str_in;

		/* Check for direction of the collation for this order. */
		if (sort_mod == 0)
			count = forward_xfrm_sb(hdl, str_in_ptr, str_out,
						count, n, cur_order);
		else if (sort_mod & _COLL_BACKWARD_MASK) {
			rev_start = count;
			if (sort_mod & _COLL_POSITION_MASK) {
				count = forw_pos_xfrm_sb(hdl, str_in_ptr,
						str_out, count, n, cur_order);
				xfrm_byte_count = 4;
			} else {
				count = forward_xfrm_sb(hdl, str_in_ptr,
						str_out, count, n, cur_order);
				xfrm_byte_count = 2;
			}
/*
 * Reverse the collation orders.
 *
 * Change:
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *	| a1|a2 | b1|b2 | c1|c2 | d1|d2 | e1|e2 | f1|f2 | g1|g2 |low|low|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * To:
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *	| g1|g2 | f1|f2 | e1|e2 | d1|d2 | c1|c2 | b1|b2 | a1|a2 |low|low|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 */
			limit = (count - rev_start - xfrm_byte_count) / (2 *
					xfrm_byte_count);
			copy_start = count - xfrm_byte_count;
			for (i = 0; i < limit && str_out; i++) {
				copy_start -= xfrm_byte_count;
				(void) strncpy(save, &str_out[rev_start],
					xfrm_byte_count);
				(void) strncpy(&str_out[rev_start],
					&str_out[copy_start], xfrm_byte_count);
				(void) strncpy(&str_out[copy_start], save,
					xfrm_byte_count);
				rev_start += xfrm_byte_count;
			}
		} else {
			if (sort_mod & _COLL_POSITION_MASK)
				count = forw_pos_xfrm_sb(hdl, str_in_ptr,
						str_out, count, n, cur_order);
			else
				count = forward_xfrm_sb(hdl, str_in_ptr,
						str_out, count, n, cur_order);
		}
	}

	/* Special case: forward_xfrm_sb does not handle n == 1. */
	if (n == 1 && str_out)
		*str_out = '\0';

	return (count);
}


static size_t
forward_xfrm_sb(_LC_collate_t *hdl, const char *str_in, char *str_out,
		    size_t count, size_t n, int order)
{
	char *str_in_ptr;
	char *str_out_ptr;
	wchar_t wc;
	int fill_flag;
	size_t nn = n-1;
	wchar_t str_in_colval;
	str_in_ptr = (char *)str_in;
	str_out_ptr = ((str_out && n) ? &str_out[count] : NULL);

	if (count < nn)
		fill_flag = 1;
	else
		fill_flag = 0;

	if (! str_out_ptr)
		fill_flag = 0;

	/* Go through all of the characters until a null is hit. */
	while (*str_in_ptr != '\0') {
		/* Get the collating value for each character. */
		wc = (wchar_t)(unsigned char)*str_in_ptr++;
		str_in_ptr += _getcolval(hdl, &str_in_colval, wc, str_in_ptr,
					order);

		/*
		 * If this character has collation, put it in the output
		 * string.
		 */
		if (str_in_colval != 0) {
			if (fill_flag) {
				if (count < nn)
					*str_out_ptr++ = (char)(str_in_colval
							>> 8) & 0x00ff;
				count++;

				if (count < nn)
					*str_out_ptr++ = (char)str_in_colval &
							0x00ff;
				else {
					*str_out_ptr = '\0';
					fill_flag = 0;
				}
				count++;
			} else
				count += 2;
		}
	}

	/* Add the low weight. */
	if (fill_flag) {
		if (count < nn)
			*str_out_ptr++ = (char)(hdl->co_col_min >> 8) & 0x00ff;
		count++;

		if (count < nn)
			*str_out_ptr++ = (char)hdl->co_col_min & 0x00ff;
		count++;

		/*
		 * Always add a null to the end.
		 * If this was not the last order, it will be overwritten on
		 * the next pass.
		 */
		*str_out_ptr = '\0';
	} else
		count += 2;

	return (count);
}


static size_t
forw_pos_xfrm_sb(_LC_collate_t *hdl, const char *str_in, char *str_out,
		    size_t count, size_t n, int order)
{
	char *str_in_ptr;
	char *str_out_ptr;
	wchar_t wc;
	int fill_flag;
	size_t nn = n - 1;
	int str_pos;
	wchar_t str_in_colval;
	str_in_ptr = (char *)str_in;
	str_out_ptr = ((str_out && n) ? &str_out[count] : NULL);

	if (count < nn)
		fill_flag = 1;
	else
		fill_flag = 0;

	if (! str_out_ptr)
		fill_flag = 0;

	str_pos = hdl->co_col_min;
	/* Go thru all of the characters until a null is hit. */
	while (*str_in_ptr != '\0') {
		/* Get the collating value for each character. */
		wc = (wchar_t)(unsigned char)*str_in_ptr++;
		str_in_ptr += _getcolval(hdl, &str_in_colval, wc, str_in_ptr,
					order);

		/*
		 * If this is a multiple of 256, add 1 more (this will make
		 * sure that none of the bytes are 0x00.
		 */
		str_pos++;
		if (! (str_pos % 256))
			str_pos++;

		/*
		 * If this character has collation, put its position and
		 * its weight in the output string.
		 */
		if (str_in_colval != 0) {
			if (fill_flag) {
				if (count < nn)
					*str_out_ptr++ = (char)(str_pos >> 8) &
								0x00ff;
				count++;
				if (count < nn)
					*str_out_ptr++ = (char)str_pos & 0x00ff;
				count++;
				if (count < nn)
					*str_out_ptr++ = (char)(str_in_colval
								>> 8) & 0x00ff;
				count++;
				if (count < nn)
					*str_out_ptr++ = (char)str_in_colval &
								0x00ff;
				else {
					*str_out_ptr = '\0';
					fill_flag = 0;
				}
				count++;
			} else
				count += 4;
		}
	}

	/* Add the low weight. */
	if (fill_flag) {
		if (count < nn)
			*str_out_ptr++ = (char)(hdl->co_col_min >> 8) & 0x00ff;
		count++;
		if (count < nn)
			*str_out_ptr++ = (char)hdl->co_col_min & 0x00ff;
		count++;
		if (count < nn)
			*str_out_ptr++ = (char)(hdl->co_col_min >> 8) & 0x00ff;
		count++;
		if (count < nn)
			*str_out_ptr++ = (char)hdl->co_col_min & 0x00ff;
		count++;

		/*
		 * Always add a null to the end.
		 * If this was not the last order, it will be overwritten on
		 * the next pass.
		 */
		*str_out_ptr = '\0';
	} else
		count += 4;

	return (count);
}
