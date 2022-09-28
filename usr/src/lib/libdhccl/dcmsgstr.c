/*
 * dcmsgstr.c: "Convert DHCP message number to human readable string".
 *
 * SYNOPSIS
 *	const char *DHCPmessageTypeString(int DHCPmessage)
 *
 * DESCRIPTION
 *	Convert a DHCP message type into a string for human readability.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)dcmsgstr.c 1.2 96/11/18 SMI"

#include "dhcp.h"


static int messageType[] = {
	DHCPDISCOVER,	DHCPOFFER,	DHCPREQUEST,	DHCPDECLINE,
	DHCPACK,	DHCPNAK,	DHCPRELEASE,	DHCPINFORM,
	DHCPREVALIDATE
};

static const char *typeAsString[] = {
	"DHCPDISCOVER",	"DHCPOFFER",	"DHCPREQUEST",	"DHCPDECLINE",
	"DHCPACK",	"DHCPNAK",	"DHCPRELEASE",	"DHCPINFORM",
	"DHCPREVALIDATE"
};

static const char *unknownType = "unknown DHCP message type";

const char *
DHCPmessageTypeString(int DHCPmessage)
{
	register const char *retp = unknownType;
	register int i;

	for (i = 0; i < (sizeof (messageType) / sizeof (messageType[0])); i++) {
		if (DHCPmessage == messageType[i]) {
			retp = typeAsString[i];
			break;
		}
	}
	return (retp);
}
