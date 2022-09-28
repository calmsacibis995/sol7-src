/*
 * strioctl.c: "Formulate a STREAMS message to ioctl".
 *
 * SYNOPSIS
 *	int strioctl(int fd, int cmd, int timout, int len, void *dp)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)strioctl.c 1.2 96/11/21 SMI"

#include "catype.h"
#include <stropts.h>
#include "caunistd.h"

int
strioctl(int fd, int cmd, int timout, int len, void *dp)
{
	struct strioctl sioc;
	int rc;

	sioc.ic_cmd = cmd;
	sioc.ic_timout = timout;
	sioc.ic_len = len;
	sioc.ic_dp = (char *)dp;
	rc = ioctl(fd, I_STR, &sioc);

	return (rc < 0 ? rc : sioc.ic_len);
}
