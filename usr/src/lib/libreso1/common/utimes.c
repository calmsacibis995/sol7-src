/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Copyright (c) 1997 by Internet Software Consortium.
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


#pragma ident   "@(#)utimes.c 1.1     97/12/03 SMI"

#include "port_before.h"

#include <sys/types.h>
#include <sys/time.h>
#include <utime.h>

#include "port_after.h"

#ifndef NEED_UTIMES
int __bind_utimes_unneeded;
#else

int
__utimes(char *filename, struct timeval *tvp) {
	struct utimbuf utb;

	utb.actime = (time_t)tvp[0].tv_sec;
	utb.modtime = (time_t)tvp[1].tv_sec;
	return (utime(filename, &utb));
}

#endif /* NEED_UTIMES */
