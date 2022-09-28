/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)usage.c	1.2 96/07/01 Sun Microsystems"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void
usage()
{
    fprintf(stderr, "Usage:\n");

    fprintf(stderr, "  trapsend\n");
    fprintf(stderr, "  [-h host]\t\t(default = localhost)\n");
	fprintf(stderr, "  [-c community]\t(default = public)\n"); 
	fprintf(stderr, "  [-e enterprise | -E enterprise_str]\t(default = 1.3.6.1.4.1.42)\n");
    fprintf(stderr, "  [-g generic#]\t\t(range 0..6, default = 6)\n");
    fprintf(stderr, "  [-s specific#]\t(default = 1)\n");
	fprintf(stderr, "  [-i ipaddr]\t\t(default = localhost)\n");
	fprintf(stderr, "  [-p trap_port]\t(default = 162)\n");
	fprintf(stderr, "  [-t timestamp]\t(a time in unix-time format, default is uptime)\n");
    fprintf(stderr, "  -a \"object-id object-type ( object-value )\"\n");
	fprintf(stderr, "  [-T trace-level]\t(range 0..4, default = 0)\n\n"); 
    fprintf(stderr, "  Note: Valid object types are:\n");
    fprintf(stderr, "           STRING\n");
    fprintf(stderr, "           INTEGER\n");
    fprintf(stderr, "           COUNTER\n");
    fprintf(stderr, "           GAUGE\n");
    fprintf(stderr, "           TIMETICKS\n");
    fprintf(stderr, "           OBJECTID\n");
    fprintf(stderr, "           IPADDRESS\n");
    fprintf(stderr, "           OPAQUE\n");

    exit(1);
}  /* usage */


