/*
 * convsyms.c: "Set client and class IDs using symbolic substitutions".
 *
 * SYNOPSIS
 *	void convertClient(struct shared_bindata **newid , char *id,
 *	    const char *ifname, const HADDR *hw, u_char iford)
 *	void convertClass(char **newid, char *id)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *	Copyright 992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)convsyms.c 1.3 96/11/25 SMI"

#include <string.h>
#include "client.h"
#include "utils.h"
#include "hostdefs.h"
#include "camacros.h"
#include "haddr.h"
#include "catype.h"
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <ctype.h>

#define	ID_ARCH		0
#define	ID_CPU		1
#define	ID_OSREL	2
#define	ID_OSNAME	3
#define	ID_PLATFORM	4
#define	ID_IFNAME	5
#define	ID_IFADDRNUM	6
#define	ID_IFMACADDR	7
#define	ID_IFMACTYPE	8

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define	MAXSIZE	255

static int
findsym(const char *sym)
{
	static const char *symTable[] = {
		"ARCH", "CPU", "OSREL", "OSNAME", "PLATFORM",
		"IFNAME", "IFORD", "IFHADDR", "IFHTYPE"
	};
	register int i;

	for (i = 0; i < ARY_SIZE(symTable); i++) {
		if (strcmp(sym, symTable[i]) == 0)
			return (i);
	}
	return (-1);
}

void
convertClient(struct shared_bindata **newid, char *id, const char *ifname,
    const HADDR *hw, u_char iford)
{
	char buf[MAXSIZE];
	char *pbuf = buf;
	char *p, *q;
	char keep;
	int left, l;

	left = MAXSIZE; /* no terminating null */
	for (q = id; *q; ) {
		if (*q != '$') {
			*pbuf++ = *q++;
			left--;
		} else if (q[1] == '$') {
			*pbuf++ = '$';
			q += 2;
			left--;
		} else {
			for (p = q + 1; *p && isalnum(*p); p++)
				;
			keep = *p;
			*p = '\0';
			switch (findsym(q + 1)) {
			case ID_IFNAME:
				l = strlen(ifname);
				l = MIN(l, left);
				strncpy(pbuf, ifname, l);
				pbuf += l;
				left -= l;
				break;
			case ID_IFADDRNUM:
				*pbuf++ = iford;
				left--;
				break;
			case ID_IFMACADDR:
				l = MIN(hw->hlen, left);
				memcpy(pbuf, hw->chaddr, l);
				pbuf += l;
				left -= l;
				break;
			case ID_IFMACTYPE:
				*pbuf++ = hw->htype;
				left--;
				break;
			default:
				l = strlen(q);
				l = MIN(l, left);
				strncpy(pbuf, q, l);
				pbuf += l;
				left -= l;
			}
			*p = keep;
			q = p;
		}
	}
	l = sizeof (struct shared_bindata) + MAXSIZE - left - 1;
	*newid = (struct shared_bindata *)xmalloc(l);
	(*newid)->linkcount = 1;
	(*newid)->length = MAXSIZE - left;
	memcpy((*newid)->data, buf, MAXSIZE - left);
}

void
convertClass(char **newid, char *id)
{
	char buf[MAXSIZE];
	char *pbuf = buf;
	char *p, *q;
	char keep;
	int left, l;

	left = MAXSIZE - 1; /* leave space for a terminating null */
	for (q = id; *q; ) {
		if (*q != '$') {
			*pbuf++ = *q++;
			left--;
		} else if (q[1] == '$') {
			*pbuf++ = '$';
			q += 2;
			left--;
		} else {
			for (p = q + 1; *p && isalnum(*p); p++)
				;
			keep = *p;
			*p = '\0';
			switch (findsym(q + 1)) {
			case ID_ARCH:
				l = sysinfo(SI_MACHINE, pbuf, left);
				l = MIN(l - 1, left);
				pbuf += l;
				left -= l;
				break;
			case ID_CPU:
				l = sysinfo(SI_ARCHITECTURE, pbuf, left);
				l = MIN(l - 1, left);
				pbuf += l;
				left -= l;
				break;
			case ID_OSNAME:
				l = sysinfo(SI_SYSNAME, pbuf, left);
				l = MIN(l - 1, left);
				pbuf += l;
				left -= l;
				break;
			case ID_OSREL:
				l = sysinfo(SI_RELEASE, pbuf, left);
				l = MIN(l - 1, left);
				pbuf += l;
				left -= l;
				break;
			case ID_PLATFORM:
				l = sysinfo(SI_PLATFORM, pbuf, left);
				l = MIN(l - 1, left);
				pbuf += l;
				left -= l;
				break;
			default:
				l = strlen(q);
				l = MIN(l, left);
				strncpy(pbuf, q, l);
				pbuf += l;
				left -= l;
			}
			*p = keep;
			q = p;
		}
	}
	if ((MAXSIZE - 1 - left) == 0)
		*newid = 0;
	else {
		*pbuf = '\0';	/* terminating null */
		*newid = (char *)xmalloc(MAXSIZE - left);
		strcpy(*newid, buf);
	}
}
