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
#pragma ident   "@(#)gen_sv.c 1.1     97/12/03 SMI"

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: gen_sv.c,v 1.7 1996/11/18 09:09:25 vixie Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "port_after.h"

#include "irs_p.h"
#include "gen_p.h"

/* Types */

struct pvt {
	struct irs_rule *	rules;
	struct irs_rule *	rule;
};

/* Forward */

static void			sv_close(struct irs_sv*);
static struct servent *		sv_next(struct irs_sv *);
static struct servent *		sv_byname(struct irs_sv *, const char *,
					  const char *);
static struct servent *		sv_byport(struct irs_sv *, int, const char *);
static void			sv_rewind(struct irs_sv *);
static void			sv_minimize(struct irs_sv *);

/* Public */

struct irs_sv *
irs_gen_sv(struct irs_acc *this) {
	struct gen_p *accpvt = (struct gen_p *)this->private;
	struct irs_sv *sv;
	struct pvt *pvt;

	if (!(sv = (struct irs_sv *)malloc(sizeof *sv))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(sv, 0x5e, sizeof *sv);
	if (!(pvt = (struct pvt *)malloc(sizeof *pvt))) {
		free(sv);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	pvt->rules = accpvt->map_rules[irs_sv];
	pvt->rule = pvt->rules;
	sv->private = pvt;
	sv->close = sv_close;
	sv->next = sv_next;
	sv->byname = sv_byname;
	sv->byport = sv_byport;
	sv->rewind = sv_rewind;
	sv->minimize = sv_minimize;
	return (sv);
}

/* Methods */

static void
sv_close(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	free(pvt);
	free(this);
}

static struct servent *
sv_next(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct servent *rval;
	struct irs_sv *sv;
	
	while (pvt->rule) {
		sv = pvt->rule->inst->sv;
		rval = (*sv->next)(sv);
		if (rval)
			return (rval);
		if (!(pvt->rule->flags & IRS_CONTINUE))
			break;
		pvt->rule = pvt->rule->next;
		if (pvt->rule) {
			sv = pvt->rule->inst->sv;
			(*sv->rewind)(sv);
		}
	}
	return (NULL);
}

static struct servent *
sv_byname(struct irs_sv *this, const char *name, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct servent *rval;
	struct irs_sv *sv;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		sv = rule->inst->sv;
		rval = (*sv->byname)(sv, name, proto);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static struct servent *
sv_byport(struct irs_sv *this, int port, const char *proto) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;
	struct servent *rval;
	struct irs_sv *sv;
	
	rval = NULL;
	for (rule = pvt->rules; rule; rule = rule->next) {
		sv = rule->inst->sv;
		rval = (*sv->byport)(sv, port, proto);
		if (rval || !(rule->flags & IRS_CONTINUE))
			break;
	}
	return (rval);
}

static void
sv_rewind(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_sv *sv;

	pvt->rule = pvt->rules;
	if (pvt->rule) {
		sv = pvt->rule->inst->sv;
		(*sv->rewind)(sv);
	}
}

static void
sv_minimize(struct irs_sv *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct irs_rule *rule;

	for (rule = pvt->rules; rule != NULL; rule = rule->next) {
		struct irs_sv *sv = rule->inst->sv;

		(*sv->minimize)(sv);
	}
}
