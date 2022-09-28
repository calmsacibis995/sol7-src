/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_DICT_H
#define	_CA_DICT_H

#pragma ident	"@(#)ca_dict.h	1.2	96/11/21 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct ca_dict {
	char *desc;
	int tag;
} ca_dict;

extern int seekdict(const char *, const ca_dict *, int, int *);

#ifdef	__cplusplus
}
#endif

#endif /* _CA_DICT_H */
