/*
 * SYNOPSIS
 *	int configHostname(const struct Host *hp)
 *
 * DESCRIPTION
 *	Configure the host's official network name.
 *
 * COPYRIGHT
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)cfhost.c 1.2 96/11/17 SMI"

#include "client.h"
#include "hostdefs.h"
#include <string.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>

int
configHostname(const struct Host *hp)
{
	int rc;
	int len;
	const char *unknown = "unknown";
	const char *hostname;

	if (inhost(hp->flags, TAG_HOSTNAME))
		hostname = hp->stags[TAG_HOSTNAME].dhccp;
	else
		hostname = unknown;

	if (hostname[0] == '\0')
		return (1);

	len = strlen(hostname);
	if (len <= SYS_NMLN)
		rc = sysinfo(SI_SET_HOSTNAME, (char *)hostname, len);
	else
		rc = -1;

	return (rc < 0);
}
