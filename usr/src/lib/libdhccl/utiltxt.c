/*
 * utiltxt.c: "Text of messages in util directory".
 *
 * USAGE
 *    const char *util_txt(int index)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)utiltxt.c 1.2 96/11/21 SMI"

static const char *msg[] = {
	/* 00 */ "can't open %s : %s\n",
	/* 01 */ "can't bind nit to interface '%s' : %s\n",
	/* 02 */ "can't send message : %s\n",
	/* 03 */ "can't set up ARP filter : %s\n",
	/* 04 */ "",
	/* 05 */ "can't set mode to message discard : %s\n",
	/* 06 */ "can't set interface to raw mode : %s\n",
	/* 07 */ "error sending packet : %s\n",
	/* 08 */ "can't open packet filter for interface %s: %s\n",
	/* 09 */ "can't push packet filter for interface %s: %s\n",
	/* 10 */ "can't get MAC address of interface %s: %s\n",
	/* 11 */ "interface %s not Ethernet\n",
	/* 12 */ "",
	/* 13 */ "putmsg failed : %s\n",
	/* 14 */ "getmsg failed : %s\n",
	/* 15 */ "DLPI returned bad primitive\n",
	/* 16 */ "address has wrong size ( = %lu octets)\n",
	/* 17 */ "can't determine protocol attach point from interface %s\n",
	/* 18 */ "can't get DLPI info on interface %s : %s\n",
	/* 19 */ "",
	/* 20 */ "can't get data on interface %s (EIOCDEVP) : %s\n",
	/* 21 */ "can't get MAC address: socket open failed : %s\n",
	/* 22 */ "Unable to open %s. Please execute \"chmod 777 %s\"\n",
	/* 23 */ "select failed on raw socket : %s\n",
	/* 24 */ "can't send packet through raw socket : %s\n",
	/* 25 */ "can't recv on raw socket : %s\n",
	/* 26 */ "can't create raw socket : %s\n",
	/* 27 */ "ioctl(SIOCSARP): %s\n",
	/* 28 */ "ioctl(SIOCDARP): %s\n",
	/* 29 */ "can't add route to %s through %s : %s\n",
	/* 30 */ "can't delete route to %s through %s : %s\n",
	/* 31 */ "heap memory exhausted (need %d bytes)",
	/* 32 */ "",
	/* 33 */ "",
	/* 34 */ "rename : %s\n",
	/* 35 */ "Modified file left in %s\n",
	0
};

const char *
util_txt(int index)
{
	static const char *retval = "message text not found\n";

	if (index >= 0 && index <= (sizeof (msg) / sizeof (msg[0]) - 2))
		retval = msg[index];
	return (retval);
}
