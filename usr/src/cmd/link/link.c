/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)link.c	1.7	98/01/18 SMI"	/* SVr4.0 1.4	*/
#include <locale.h>

main(argc, argv) char *argv[]; {
	char *use;

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	use = gettext("Usage: /usr/sbin/link from to\n");

	if (argc != 3) {
		write(2, use, strlen(use));
		exit(1);
	}
	exit((link(argv[1], argv[2]) == 0)? 0 : 2);
}
