/*
 * polldbg.c:
 * "Print debug information on the type of event available in poll()".
 *
 * SYNOPSIS
 *	void polltype(const struct pollfd *pfd)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)polldbg.c 1.2 96/11/21 SMI"

#include <poll.h>
#include "client.h"
#include "catype.h"
#include "unixgen.h"

extern const char *prog;

void
polltype(const struct pollfd *pfd)
{
	static int eventTypes[] = {
		POLLIN, POLLPRI, POLLOUT, POLLRDNORM, POLLWRNORM,
		POLLRDBAND, POLLWRBAND, POLLERR, POLLHUP, POLLNVAL
	};

	static const char *eventStrings[] = {
		"POLLIN", "POLLPRI", "POLLOUT", "POLLRDNORM", "POLLWRNORM",
		"POLLRDBAND", "POLLWRBAND", "POLLERR", "POLLHUP", "POLLNVAL"
	};
	int i;
	ca_boolean_t set = FALSE;

	for (i = 0; i < sizeof (eventTypes) / sizeof (eventTypes[0]); i++) {
		if (pfd->revents & eventTypes[i]) {
			if (set)
				putc('|', stdbug);
			else {
				set = TRUE;
				logb("%s: fd %d has event(s) ", prog, pfd->fd);
			}
			logb("%s", eventStrings[i]);
		}
	}
	if (set)
		putc('\n', stdbug);
}
