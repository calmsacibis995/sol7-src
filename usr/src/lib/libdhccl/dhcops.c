/*
 * dhcpops.c: "Manage client configuration files".
 *
 * SYNOPSIS
 * int read_dhc(const char *configdir, const char *ifname, struct Host **hp)
 * void unlink_dhc(const char *configdir, const char *ifname)
 *	int write_dhc(const char *configdir, const char *ifname,
 *	    const struct Host *hp)
 *
 * DESCRIPTION
 *    These routines manage reading, writing and unlinking the
 *    .dhc configuration files.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)dhcops.c 1.4 96/11/25 SMI"

#include "client.h"
#include "utils.h"
#include "dccommon.h"
#include "hostdefs.h"
#include "magic.h"
#include "error.h"
#include "msgindex.h"
#include <alloca.h>
#include "unixgen.h"
#include <string.h>

#define	LSUFF	6  /* 1 for nul, 1 for '/', +4 for ".dhc" */

int
read_dhc(const char *configdir, const char *ifname, struct Host **hp)
{
	FILE *f;
	MAGIC_COOKIE cookie;
	int err = 0;
	int items;
	char *configfile = (char *)alloca(LSUFF + strlen(configdir) +
	    strlen(ifname));
	sprintf(configfile, "%s/%s.dhc", configdir, ifname);

	f = fopen(configfile, "r");
	if (!f)
		return (ERR_NO_CONFIG);

	items = fread(cookie, sizeof (cookie), 1, f);
	if (items != 1 || COOKIE_ISNOT(cookie, CF_COOKIE)) {
		loge(DHCPCMSG50, configfile);
		err = ERR_BAD_CONFIG;
		goto cleanup;
	}

	*hp = (struct Host *)xmalloc(sizeof (struct Host));
	if (hp)
		FileToHost(f, *hp);
	else
		err = ERR_OUT_OF_MEMORY;

cleanup:
	fclose(f);
	return (err);
}

void
unlink_dhc(const char *configdir, const char *ifname)
{
	char *configfile = (char *)alloca(LSUFF + strlen(configdir) +
	    strlen(ifname));

	sprintf(configfile, "%s/%s.dhc", configdir, ifname);

	unlink(configfile);
}

int
write_dhc(const char *configdir, const char *ifname, const struct Host *hp)
{
	FILE *f;
	char *configfile = (char *)alloca(LSUFF + strlen(configdir) +
	    strlen(ifname));

	sprintf(configfile, "%s/%s.dhc", configdir, ifname);

	f = fopen(configfile, "w");
	if (!f) {
		loge(DHCPCMSG41, configfile, SYSMSG);
		return (ERR_WRITE_CONFIG);
	}
	HostToFile(hp, f);
	fclose(f);
	return (0);
}
