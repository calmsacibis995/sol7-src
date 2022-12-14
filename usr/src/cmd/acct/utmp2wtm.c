/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)utmp2wtmp.c	1.8	97/10/14 SMI"	/* SVr4.0 1.2	*/
/*
 *	create entries for users who are still logged on when accounting
 *	is being run. Look at utmp, and update the time stamp. New info
 *	goes to wtmp. Call by runacct. 
 */

#include <stdio.h>
#include <sys/types.h>
#include <utmp.h>
#include <time.h>

main(argc, argv)
int argc;
char **argv;
{
	struct utmp *getutent(), *utmp;
	FILE *fp;

	fp = fopen(WTMP_FILE, "a+");
	if (fp == NULL) {
		fprintf(stderr, "%s: could not open %s: ", argv[0], WTMP_FILE);
		perror(NULL);
		exit(1);
	}

	while ((utmp=getutent()) != NULL) {
		if ((utmp->ut_type == USER_PROCESS) && !(nonuser(*utmp))) {
			time(&utmp->ut_time);
			fwrite(utmp, sizeof(*utmp), 1, fp);
		}
	}
	fclose(fp);
	exit(0);
}
