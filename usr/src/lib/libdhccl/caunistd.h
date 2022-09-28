/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CAUNISTD_H
#define	_CAUNISTD_H

#pragma ident	"@(#)caunistd.h	1.2	96/11/21 SMI"

#include <unistd.h>

#ifdef	__cplusplus
extern "C" {
#endif

int getdomainname(char *, int);
int setdomainname(const char *, int);
int gethostname(char *, int);
int sethostname(const char *, int);
int ioctl(int, int, ...);

#ifdef	__cplusplus
}
#endif

#endif /* _CAUNISTD_H */
