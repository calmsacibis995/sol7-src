/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _LLI_H
#define	_LLI_H

#pragma ident	"@(#)lli.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct LLinfo {
	int fd;
	char *macaddr;
	char *brdaddr;
	int lmacaddr;
	int bpmactype;
	int lbrdaddr;
	int ord;
} LLinfo;

struct in_addr;

int liOpen(const char *, LLinfo *);
void liClose(LLinfo *);
int liBroadcast(const LLinfo *, void *, int, const struct in_addr *);
int liRead(int, void *, int *, void *, void *);

#ifdef	__cplusplus
}
#endif

#endif /* _LLI_H */
