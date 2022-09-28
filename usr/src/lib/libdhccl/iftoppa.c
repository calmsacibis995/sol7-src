/*
 * iftoppa.c: "Convert an interface name to device, PPA and logical IP number"
 *
 * SYNOPSIS
 *	void iftoppa(const char *interface, char **device, int *ppa, int *ord)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)iftoppa.c	1.2	96/11/20 SMI"

#include <ctype.h>
#include <string.h>
#include "utils.h"

void
iftoppa(const char *interface, char **device, int *ppa, int *ord)
{
	static char	devdir[] = "/dev/";
	int i = 0;

	while (isalpha(interface[i]))
		i++;

	if (i > 0 && device) {
		if (*device == 0)
			*device = (char *) xmalloc(i + sizeof (devdir));
		strcpy(*device, devdir);
		strncat(*device, interface, i);
		(*device)[i + sizeof (devdir) - 1] = '\0';
	}

	if (!isdigit(interface[i])) {
		*ord = *ppa = -1;
		return;
	}

	*ppa = interface[i] - '0';
	while (isdigit(interface[++i]))
		*ppa = 10 * (*ppa) + interface[i] - '0';

	*ord = 0;
	if (interface[i] != ':')
		return;
	while (isdigit(interface[++i]))
		*ord = 10 * (*ord) + interface[i] - '0';
}
