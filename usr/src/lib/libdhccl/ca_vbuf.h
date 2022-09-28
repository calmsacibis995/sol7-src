/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_VBUF_H
#define	_CA_VBUF_H

#pragma ident	"@(#)ca_vbuf.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

struct Vbuf {
	int len;
	char *dbuf;
};

int xgets(FILE *, struct Vbuf *);

#ifdef	__cplusplus
}
#endif

#endif /* _CA_VBUF_H */
