/*
 * seed.c: "Seed a random number generator".
 *
 * SYNOPSIS
 *    void seed(void)
 *
 * DESCRIPTION
 *    Initialise the random number generator. We aren't too concerned about the
 *    true randomness of the sequence generated, but only with having some
 *    reasonable expectation that different DHCP clients will be using
 *    different XIDs with a high probability. In SUN platforms we achieve
 *    this by multiplying the hostid by a number representing the current
 *    millisecond. This should give sufficient variance between clients
 *    and at different times of day.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)seed.c 1.4 96/11/26 SMI"

#include <stdio.h>
#include <stdlib.h>
#include "ca_time.h"
#include <sys/systeminfo.h>

void
seed(void)
{
	uint32_t hid;
	char buf[32];
	struct timeval tv;

	sysinfo(SI_HW_SERIAL, buf, sizeof (buf));
	sscanf(buf, "%u", &hid);
	gettimeofday(&tv, 0);
	if (tv.tv_usec <= 0)
		tv.tv_usec = 1;
	tv.tv_sec = tv.tv_sec % tv.tv_usec;
	hid *= tv.tv_sec;
	srand48(hid);
}
