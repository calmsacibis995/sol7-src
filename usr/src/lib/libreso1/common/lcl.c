/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1996 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#pragma ident   "@(#)lcl.c 1.1     97/12/03 SMI"

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: lcl.c,v 1.9 1996/11/18 09:09:33 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

/* Forward. */

static void		lcl_close(struct irs_acc *);

/* Public */

struct irs_acc *
irs_lcl_acc(const char *options) {
	struct irs_acc *acc;
	struct lcl_p *lcl;

	if (!(acc = malloc(sizeof *acc))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(acc, 0x5e, sizeof *acc);
	if (!(lcl = malloc(sizeof *lcl))) {
		errno = ENOMEM;
		free(acc);
		return (NULL);
	}
	memset(lcl, 0x5e, sizeof *lcl);
	acc->private = lcl;
	acc->gr_map = irs_lcl_gr;
#ifdef WANT_IRS_PW
	acc->pw_map = irs_lcl_pw;
#else
	acc->pw_map = NULL;
#endif
	acc->sv_map = irs_lcl_sv;
	acc->pr_map = irs_lcl_pr;
	acc->ho_map = irs_lcl_ho;
	acc->nw_map = irs_lcl_nw;
	acc->ng_map = irs_lcl_ng;
	acc->close = lcl_close;
	return (acc);
}

/* Methods */

static void
lcl_close(struct irs_acc *this) {
	struct lcl_p *lcl = (struct lcl_p *)this->private;

	if (lcl)
		free(lcl);
	free(this);
}
