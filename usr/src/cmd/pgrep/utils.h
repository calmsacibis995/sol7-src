/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_UTILS_H
#define	_UTILS_H

#pragma ident	"@(#)utils.h	1.1	97/12/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern void warn(const char *, ...);
extern void die(const char *, ...);

extern const char *getpname(const char *);

#ifdef	__cplusplus
}
#endif

#endif	/* _UTILS_H */
