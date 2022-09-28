/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CATYPE_H
#define	_CATYPE_H

#pragma ident	"@(#)catype.h	1.3	96/11/25 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef FALSE
#define	FALSE (0)
#endif

#ifndef TRUE
#define	TRUE (1)
#endif

typedef int ca_boolean_t;

#ifdef	__cplusplus
}
#endif

#endif /* _CATYPE_H */
