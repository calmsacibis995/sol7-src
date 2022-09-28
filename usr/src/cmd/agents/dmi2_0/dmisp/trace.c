/* Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trace.c	1.3 96/10/07 Sun Microsystems"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <varargs.h>
#include <sys/types.h>
#include <syslog.h>

static FILE *trace_stream = NULL;

void trace_on()
{
	trace_stream = stdout;
}

void trace_off()
{
	trace_stream = NULL;
}

void trace(char *format, ...)
{
	va_list ap;

	if (trace_stream == NULL) return;
	va_start(ap);
	vfprintf(trace_stream, format, ap);
	va_end(ap);
}


void error(char *format, ...)
{
	va_list ap;
	char err_str[2000]; 

	va_start(ap);
	
	if (trace_stream != NULL) {
		fprintf(stderr, "dmispd error: "); 
		vfprintf(stderr, format, ap);
	}
	else {
		vsprintf(err_str, format, ap);
		syslog(LOG_ERR, err_str);
	}
	va_end(ap);
}
