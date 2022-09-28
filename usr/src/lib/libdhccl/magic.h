/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _MAGIC_H
#define	_MAGIC_H

#pragma ident	"@(#)magic.h	1.2	96/11/21 SMI"

#include <string.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	MAJOR_VERSION	1
#define	MINOR_VERSION	0

/* vendor magic cookie prefix */
#define	VM_DHCP	"\300\365\213\000"

/*
 * The identifiers for particular file types. These are the VM_DHCP
 * cookie or'd with numerics 1, 2, ..n to produce identifiers for all
 * CA's special files.
 */

#define	CF_COOKIE	"\300\365\213\001"  /* a configuration file .dhc */

typedef char MAGIC_COOKIE[8];

#define	MAKE_COOKIE(cookie, type, majorRel, minorRel) \
	{ memcpy(cookie, type, 4); \
	cookie[4] = MAJOR_VERSION; \
	cookie[5] = MINOR_VERSION; \
	cookie[6] = cookie[7] = 0; \
	}

#define	COOKIE_ISNOT(cookie, type) \
    memcmp(cookie, type, 4)

#ifdef	__cplusplus
}
#endif

#endif /* _MAGIC_H */
