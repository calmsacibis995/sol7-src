/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CAMACROS_H
#define	_CAMACROS_H

#pragma ident	"@(#)camacros.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	ARY_SIZE(a)	(sizeof ((a)) / sizeof ((a[0])))
#define	MEMBER_OFFSET(s, m)	((int) &(((s *)0)->m))

#ifdef	__cplusplus
}
#endif

#endif /* _CAMACROS_H */
