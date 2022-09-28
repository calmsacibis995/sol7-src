// Copyright 11/15/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dmispd.cc	1.11 96/11/15 Sun Microsystems"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include "trace.hh"
#include "server.h"
#include "initialization.hh"


static char default_config_dir[] = "/etc/dmi/conf";

void usage()
{
    fprintf(stderr, "Usage:\n");

    fprintf(stderr, "  dmispd\n");
	fprintf(stderr, "  [-c config-dir]\t(default = %s)\n", default_config_dir); 
	fprintf(stderr, "  [-d trace-level]\t(range 0..4, default 0)\n"); 
	fprintf(stderr, "  [-h]\n"); 
	exit(0); 
}


static int is_number (char *buf)
{
	int len, i;

	if (buf == NULL)
		return (-1);
	
	len = strlen(buf);
	for (i= 0; i < len; i++) 
		if (!isdigit(buf[i])) {
		fprintf(stderr, "\n%s is not a valid trace level number!\n\n", buf);
		return(-1);
		}
	int level = atoi(buf);
	
	if (level > 4 || level < 0) {
		fprintf(stderr, "\n%s is not a valid trace level number!\n\n", buf);
		return (1);
	}
	return (0);
}

main(int argc, char *argv[])
{

	char *config_dir = NULL;
	int debug_flag = 0; 
	
    extern char *optarg;
	extern int optind;
	int opt;

	optind = 1;

	/* get command-line options */
    while ((opt = getopt(argc, argv, "c:d:h")) != EOF) {
		switch (opt) {
			case 'c':
				config_dir = strdup(optarg); 
				break; 
			case 'd':
				if (is_number(optarg)) {
					usage(); 
				}
				trace_on();
				debug_flag = 1; 
				break;
			case 'h':
				usage();
				break; 
			case '?':		/* usage help */
				usage();
				break; 
			default:
				usage();
				break;
		}  /* switch */
	}/* while */

	if (optind != argc)
		usage();

	if (config_dir == NULL)
		config_dir = default_config_dir;

	openlog("dmispd", LOG_CONS, LOG_DAEMON );

	if (!debug_flag) {
		
		uid_t  euid = geteuid();
		if (euid != 0) {
			error("Not running with root permission. Current euid = %ld. Exit.", euid); 
			exit(1);
		}


		pid_t pid = fork();
		if (pid < 0) {
			perror("cannot fork");
			exit(1);
		}
		if (pid)
			exit(0);
		setsid();
	}

	InitDmiInfo(config_dir);
}


