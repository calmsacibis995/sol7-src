/*
 * root.c: "Get a Portion of a Pathname".
 *
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident	"@(#)root.c	1.4	96/11/25 SMI"

#include <string.h>
#include "utils.h"
#define	CNULL (char *)0

static void usage()
{
	puts("usage: root <pathname>");
}

int
main(int argc, char *argv[])
{
	char buf[64];
	if (argc < 2) {
		usage();
		return (1);
	}
	printf("root = %s\n", root(argv[1], CNULL));
	printf("head = %s\n", head(argv[1], CNULL));
	printf("tail = %s\n", tail(argv[1]));
	printf("extn = %s\n", extn(argv[1]));
	printf("body = %s\n", body(argv[1], CNULL));
	root(argv[1], buf);
	printf("root(%s, buf) buf = %s\n", argv[1], buf);
	head(argv[1], buf);
	printf("head(%s, buf) buf = %s\n", argv[1], buf);
	body(argv[1], buf);
	printf("body(%s, buf) buf = %s\n", argv[1], buf);
	return (0);
}
