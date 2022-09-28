/*
 * dcuttxt.c: "Text of messages common to CA's DHCP client & server".
 *
 * USAGE
 *    const char *dccommon_txt(int index)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)dcuttxt.c 1.2 96/11/18 SMI"

#include <libintl.h>

static const char *msg[] = {
	"invalid tag# %s on line %d of %s -- ignored\n",
	"invalid type %s on line %d of %s -- ignored\n",
	"tag value %s on line %d of %s is out of range (%d-%d)\n",
	"syntax error on line %d of %s: wrong # of fields\n",
	"can't open DHCP tags file %s : %s\n",
	"overload option found twice! Ignoring all but first\n",
	"Missing 'end' option in overload section\n",
	"BOOT file name \"%s\" is too long to fit in BOOTP file field\n",
	"BOOT server name \"%s\" is too long to fit in BOOTP sname field\n",
	"Bad count in configuration file\n",
	"Bad configuration file\n",
	"Not enough space in bootp packet for %s option\n",
	"BOOT file name \"%s\" is too long to fit in BOOTP file field\n",
	"BOOT server name \"%s\" is too long to fit in BOOTP sname field\n",
	"Not enough space in bootp packet for data with tag %d\n",
	0
};

const char *
dccommon_txt(int index)
{
	static const char *notfound = "message text not found\n";

	if (index < 0 || index > (sizeof (msg) / sizeof (msg[0]) - 2))
		return (gettext(notfound));
	return (gettext(msg[index]));
}
