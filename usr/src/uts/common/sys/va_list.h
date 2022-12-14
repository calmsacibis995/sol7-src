/*
 * Copyright (c) 1996,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_VA_LIST_H
#define	_SYS_VA_LIST_H

#pragma ident	"@(#)va_list.h	1.11	97/11/22 SMI"

/*
 * This file is system implementation and generally should not be
 * included directly by applications.  It serves to resolve the
 * conflict in ANSI-C where the prototypes for v*printf are required
 * to be in <stdio.h> but only applications which reference these
 * routines are required to have previously included <stdarg.h>.
 * It also provides a clean way to allow either the ANSI <stdarg.h>
 * or the historical <varargs.h> to be used.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef __STDC__
typedef void *__va_list;
#else
typedef char *__va_list;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_VA_LIST_H */
