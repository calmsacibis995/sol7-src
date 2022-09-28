/*
 * haddrtoa.c: "Convert a hardware address to an ASCII string.".
 *
 * SYNOPSIS
 *    char *haddrtoa     (unsigned char *haddr, int htype, int sep)
 *    int   haddrlength  (int htype)
 *    int   haddrtok     (const char **src, unsigned char *sink, int hlen)
 *
 * DESCRIPTION
 * haddrtok:
 * "src" points to a pointer to a hexadecimal ASCII
 * string.  This string is interpreted as a hardware address and returned
 * as a pointer to the actual hardware address, represented as an array of
 * bytes.
 *
 * The input string will be consumed until either "hlen" octets have
 * been read, or the input string contains an invalid character.
 * One or two-digit sequences (bytes) may be separated with periods (.)
 * or colons (:) and/or prefixed with '0x' for readability, but this is
 * not required. Each sequence is interpreted as a hex value, regardless
 * of whether or not a leading "0x" prefix is present.
 *
 * RETURNS
 *    haddrtok:
 * The length of the resulting datum.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)haddrtoa.c 1.4 96/11/26 SMI"

#include "catype.h"
#include "utils.h"
#include "haddr.h"

/*
 * Hardware address lengths (in bytes) based on hardware type code.
 * List in order specified by Assigned Numbers RFC; Array index is
 * hardware type code.  Entries marked as zero are unknown to the
 * author at this time.
 */
static unsigned maphaddrlen[] = {
	MAXHADDRLEN,	/* Type 0: used for client IDs */
	6,		/* Type 1: 10Mb Ethernet (48 bits) */
	1,		/* Type 2:  3Mb Ethernet (8 bits) */
	0,		/* Type 3: Amateur Radio AX.25 */
	1,		/* Type 4: Proteon ProNET Token Ring */
	0,		/* Type 5: Chaos */
	6,		/* Type 6: IEEE 802 Networks */
	0		/* Type 7: ARCNET */
};

#ifdef	__CODE_UNUSED
char *
haddrtoa(const unsigned char *haddr, int htype, int sep)
{
	static char haddrbuf[3 * MAXHADDRLEN + 1];
	register char *bufptr;
	register count;

	sep &= 0xFF;

	bufptr = haddrbuf;
	for (count = maphaddrlen[htype]; count > 0; count--) {
		sprintf(bufptr, "%02X", *haddr++ & 0xFF);
		bufptr += 2;
		if (count > 1 && sep)
			*bufptr++ = (char)sep;
	}
	return (haddrbuf);
}
#endif	/* __CODE_UNUSED */

int
haddrlength(int htype)
{
	if (htype < 0)
		return (0);
	if (htype >= (sizeof (maphaddrlen) / sizeof (maphaddrlen[0])))
		return (0);
	return (maphaddrlen[htype]);
}

int
haddrtok(const char **src, unsigned char *sink, int hlen)
{
	unsigned char *hptr;

	hptr = sink;
	while (hptr < sink + hlen) {
		if (**src == '.' || **src == ':' || **src == '-')
			(*src)++;
		if (bytetok(src, hptr) < 0)
			break;
		hptr++;
	}
	hlen = hptr - sink;
	while (hptr < sink + MAXHADDRLEN)
		*hptr++ = '\0';
	return (hlen);
}
