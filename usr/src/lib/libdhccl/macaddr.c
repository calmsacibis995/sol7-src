/*
 * macaddr.c: "Find MAC address of interface".
 *
 * SYNOPSIS
 *    int myMacAddr(struct haddr *, const char *ifname)
 *
 * DESCRIPTION
 *    Get our MAC address somehow ... There doesn't seem to be any
 *    good way to do this if you're not root
 *
 * RETURNS
 *    0 for success, 1 for failure.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)macaddr.c 1.2 96/11/21 SMI"

#include "unixgen.h"
#include "haddr.h"
#include "ca_time.h"
#include "utils.h"
#include "utiltxt.h"
#include "ca_dlpi.h"
#include <fcntl.h>
#include <string.h>
#include <stropts.h>

int
myMacAddr(HADDR *mac, const char *ifname)
{
	char *device = NULL;
	int ord, ppa;
	int fd;
	int rt;
	Jodlpiu buf;
	dl_info_ack_t *d = &buf.d.info_ack;

	iftoppa(ifname, &device, &ppa, &ord);
	if (ppa < 0) {
		loge(UTILMSG17, ifname);
		return (-1);
	}

	fd = open(device, O_RDWR);
	if (fd < 0) {
		loge(UTILMSG00, device, SYSMSG);
		rt = -1;
		goto error;
	}

	rt = dlattach(fd, ppa);
	if (rt < 0)
		goto error;

	rt = dlinfo(fd, &buf);
	if (rt < 0) {
		loge(UTILMSG18, ifname, SYSMSG);
		goto error;
	}

	if (d->dl_mac_type != DL_ETHER) { /* ##BUG## allow other HW */
		loge(UTILMSG11, ifname);
		goto error;
	}

	rt = dlgetphysaddr(fd, mac->chaddr, sizeof (mac->chaddr));
	mac->hlen = 6; /* XXXX ##BUG## Get correct HW type somehow */
	mac->htype = HTYPE_ETHERNET;

error:
	close(fd);
	if (device)
		free(device);
	return (rt);
}
