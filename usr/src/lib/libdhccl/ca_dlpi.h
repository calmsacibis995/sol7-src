/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_DLPI_H
#define	_CA_DLPI_H

#pragma ident	"@(#)ca_dlpi.h	1.4	96/11/26 SMI"

#include <catype.h>
#include <sys/dlpi.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DLPIBUFSZ	256

/*
 * This union is defined to extend the length of the union of DLPI primitives
 * (so that, e.g., the DL_INFO_REQ has room for the link level hardware,
 * service access point, and broadcast addresses, and to provide a suitable
 * alignment for a pointer to any of the DLPI primitives
 */

typedef union Jodlpiu {
	char b[DLPIBUFSZ];
	union DL_primitives d;
} Jodlpiu;

#ifdef	DEBUG
void DLPIshow(const union DL_primitives *, int);
#endif	/* DEBUG */
int  dlinfo(int, Jodlpiu *);
int  dlattach(int, int);
int  dlgetphysaddr(int, void *, int);
int  dlbind(int, int, int);
int dlunitdatareq(int, const void *, int, void *, int);

#ifdef	__cplusplus
}
#endif

#endif /* _CA_DLPI_H */
