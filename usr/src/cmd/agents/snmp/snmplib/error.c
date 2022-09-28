/* Copyright 12/06/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)error.c	1.7 96/12/06 Sun Microsystems"


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <varargs.h>
#include <sys/types.h>
#include <time.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <libgen.h>


#include "snmp_msg.h"
#include "impl.h"
#include "trace.h"
#include "error.h"


/***** GLOBAL VARIABLES *****/

int error_size = DEFAULT_ERROR_SIZE;

char error_label[1000] = "";


/***** LOCAL VARIABLES *****/

static int is_error_initialized = FALSE;
static int is_error_opened = FALSE;

static char *application_name = NULL;
static void (*application_end)() = NULL;

static char *error_file = NULL;

static FILE *file_stream = NULL;
static FILE *stderr_stream = stderr;

static char static_buffer[4096];


/***** LOCAL FUNCTIONS *****/

static void error_file_check();


/******************************************************************/

/*
 *	this function will exit on any error
 */

void error_init(char *name, void end())
{
	char *ptr;


	if(name == NULL)
	{
		fprintf(stderr, "BUG: error_init(): name is NULL");
		exit(1);
	}

	ptr = basename(name);
	if(ptr == NULL)
	{
		fprintf(stderr, "error_init(): bad application name: %s",
			name);
		exit(1);
	}

	application_name = strdup(ptr);
	if(application_name == NULL)
	{
		fprintf(stderr, ERR_MSG_ALLOC);
		exit(1);
	}

	if(end == NULL)
	{
		fprintf(stderr, "BUG: error_init(): end is NULL");
		exit(1);
	}

	application_end = end;

        openlog("snmpdx", LOG_CONS, LOG_DAEMON);
	is_error_initialized = TRUE;
}


/******************************************************************/

/*
 *	this function will exit on any error
 */

void error_open(char *filename)
{
/*
	FILE *stderr_stream_bak = stderr_stream;


	if(filename == NULL)
	{
		stderr_stream = stderr;
		error_exit("BUG: error_open(): filename is NULL");
	}

	error_file = strdup(filename);
	if(error_file == NULL)
	{
		stderr_stream = stderr;
		error_exit(ERR_MSG_ALLOC);
	}

	is_error_opened = TRUE;

	stderr_stream = NULL;
	error(LOG_MSG_STARTED);
	stderr_stream = stderr_stream_bak;
*/ 
}


/******************************************************************/

void error_close_stderr()
{
	stderr_stream = NULL;
}


/******************************************************************/

static void error_file_check()
{
	long offset = 0;
	int count;


	if(file_stream == NULL)
	{
		file_stream = fopen(error_file, "a");
		if(file_stream == NULL)
		{
			is_error_opened = FALSE;
			file_stream = NULL;
			stderr_stream = stderr;
			error_exit(ERR_MSG_FILE_OPEN,
				error_file, errno_string());
		}
	}

	if(ftell(file_stream) > error_size)
	{
		char tmps[1024];


		sprintf(tmps, "%s.old", error_file);

		if(trace_level > 0)
		{
			trace("Switching log file %s -> %s\n\n",
				error_file, tmps);
		}

		(void ) fclose(file_stream);
		rename(error_file, tmps);
		file_stream = fopen(error_file, "w");
		if(file_stream == NULL)
		{
			is_error_opened = FALSE;
			file_stream = NULL;
			stderr_stream = stderr;
			error_exit(ERR_MSG_FILE_OPEN,
				error_file, errno_string());
		}
	}
}


/******************************************************************/

void error(va_alist)
	va_dcl
{
	va_list ap;
	char *format;
	int len;
	

	va_start(ap);
	format = va_arg(ap, char *);

	/* remove '\n's at the end of format */
	len = strlen(format);
	while((len > 0) && (format[len - 1] == '\n'))
	{
		format[len - 1] = '\0';
		len--;
	}

	vsprintf(static_buffer, format, ap);
	va_end(ap);

	if(trace_level > 0){
		trace(static_buffer);
	}
	syslog(LOG_ERR,static_buffer);
}


/******************************************************************/

void error_exit(va_alist)
	va_dcl
{
	va_list ap;
	char *format;
	int len;
	
	va_start(ap);
	format = va_arg(ap, char *);

	/* remove '\n's at the end of format */
	len = strlen(format);
	while((len > 0) && (format[len - 1] == '\n'))
	{
		format[len - 1] = '\0';
		len--;
	}

	vsprintf(static_buffer, format, ap);
	va_end(ap);


	application_end();

	if(trace_level > 0){
		trace(static_buffer);
	}
	syslog(LOG_ERR,static_buffer);

	exit(1);
}


/******************************************************************/

char *errno_string()
{
	static char buffer[100];

	sprintf(buffer, "[errno: %s(%d)]",
		strerror(errno), errno);

	return buffer;
}


/******************************************************************/

char *h_errno_string()
{
	static char buffer[100];
	char *ptr = NULL;

	switch(h_errno)
	{
		case HOST_NOT_FOUND:
			ptr = "host not found";
			break;
		case TRY_AGAIN:
			ptr = "try again";
			break;
		case NO_RECOVERY:
			ptr = "no recovery";
			break;
		case NO_DATA:
			ptr = "no data";
			break;
		default:
			ptr = "???";
			break;
	}

	sprintf(buffer, "[h_errno: %s(%d)]",
		ptr, h_errno);

	return buffer;
}


