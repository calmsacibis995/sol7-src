/*
 * enabsig.c: "Enable SIGPOLL on file descriptor".
 *
 * SYNOPSIS
 *    int enableSigpoll(int fd)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)enabsig.c 1.3 96/11/20 SMI"

#include <string.h>
#include "client.h"
#include "msgindex.h"
#include "unixgen.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/sockio.h>

int
enableSigpoll(int fd)
{
	int on = 1;

	pid_t pid = getpid();
	if (ioctl(fd, SIOCSPGRP, &pid) < 0) {
		loge(DHCPCMSG52, SYSMSG);
		return (-1);
	}

	if (ioctl(fd, FIOASYNC, &on) < 0) {
		loge(DHCPCMSG53, SYSMSG);
		return (-1);
	}
#if 0
	ioctl(fd, FIONBIO, &on);
#endif
	return (0);
}
