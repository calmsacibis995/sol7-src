/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trace.c	1.3 96/07/01 Sun Microsystems"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <varargs.h>
#include <sys/types.h>


#include "snmp_msg.h"
#include "error.h"
#include "trace.h"


/***** GLOBAL VARIABLES *****/

int trace_level = 0;
u_long trace_flags = 0;


/***** STATIC VARIABLES *****/

static FILE *trace_stream = stdout;


/******************************************************************/

void trace(va_alist)
	va_dcl
{
	va_list ap;
	char *format;
	

	if(trace_stream == NULL)
	{
		return;
	}

	va_start(ap);
	format = va_arg(ap, char *);
	vfprintf(trace_stream, format, ap);
	va_end(ap);
}


/******************************************************************/

int trace_set(int level, char *error_label)
{
	error_label[0] = '\0';

	if(level < 0 || level > TRACE_LEVEL_MAX)
	{
		sprintf(error_label, ERR_MSG_BAD_TRACE_LEVEL,
			level, TRACE_LEVEL_MAX);
		return -1;
	}

	trace_level = level;

	if(trace_level > 0)
	{
		trace_flags = trace_flags | TRACE_TRAFFIC;
	}
	else
	{
		trace_flags = trace_flags & (~TRACE_TRAFFIC);
	}

	if(trace_level > 2)
	{
		trace_flags = trace_flags | TRACE_PDU;
	}
	else
	{
		trace_flags = trace_flags & (~TRACE_PDU);
	}

	if(trace_level > 3)
	{
		trace_flags = trace_flags | TRACE_PACKET;
	}
	else
	{
		trace_flags = trace_flags & (~TRACE_PACKET);
	}


	return 0;
}


/******************************************************************/

void trace_reset()
{
	trace_set(0, error_label);
}


/******************************************************************/

void trace_increment()
{
	if(trace_level < TRACE_LEVEL_MAX)
	{
		trace_set(trace_level + 1, error_label);
	}

}


/******************************************************************/

void trace_decrement()
{
	if(trace_level > 0)
	{
		trace_set(trace_level - 1, error_label);
	}
}


/******************************************************************/



