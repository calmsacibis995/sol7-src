/*
 * fswok.c: "Check whether file systems writable".
 *
 * SYNOPSIS
 *    int fswok(const char *)
 *
 * DESCRIPTION
 *    Returns true (1) if the file system containing the
 *    pathname argument is writable, or false (0) if not
 *    writable.
 *
 *    DHCP is used to configure UNIX clients at a time
 *    when the root file system may be mounted read-only.
 *    When this is so, the DHCP client cannot write its
 *    configuration (.dhc) files. Instead it must defer
 *    this operation until the file system becomes
 *    writable. Of course, the DHCP client might choose
 *    not to store its configuration files on the root
 *    partition, but in that case it would be unusable
 *    on a dataless client since then its configuration
 *    and policy (.pcy) files would not even be readable.
 *
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)fswok.c 1.2 96/11/20 SMI"

#include "catype.h"
#include <sys/statvfs.h>

int
fswok(const char *path)
{
	struct statvfs v;
	int rc;

	rc = statvfs(path, &v);
	if (rc)
		return (0);

	return ((v.f_flag & ST_RDONLY) == 0);
}
