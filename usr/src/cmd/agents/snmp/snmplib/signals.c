/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)signals.c	1.3 96/07/01 Sun Microsystems"

#include <sys/types.h>
#include <signal.h>

#include "snmp_msg.h"
#include "signals.h"


/*******************************************************************/

/*
 *	SIGQUIT: do not trap it to be able to generate a core
 */

int signals_init(void signals_sighup(), void signals_exit(), char *error_label)
{
	int siq;


	error_label[0] = '\0';

	for(siq = SIGHUP; siq <= SIGTHAW; siq++)
	{
		switch(siq)
		{
			case SIGHUP:
				if(sigset(siq, signals_sighup) == SIG_ERR)
				{
					sprintf(error_label, ERR_MSG_SIGSET,
						siq, "signals_sighup()", errno_string());
					return -1;
				}
				break;

			case SIGKILL:
			case SIGILL:
			case SIGSTOP:
			case SIGQUIT:
			case SIGSEGV:
			case SIGBUS:
			case SIGFPE:
				break;

			case SIGINT:
			case SIGTERM:	/* default signal sent by the kill command */
			case SIGUSR1:
			case SIGUSR2:
				if(sigset(siq, signals_exit) == SIG_ERR)
				{
					sprintf(error_label, ERR_MSG_SIGSET,
						siq, "signals_exit()", errno_string());
					return -1;
				}
				break;

			default:
				if(sigset(siq, SIG_IGN) == SIG_ERR)
				{
					sprintf(error_label, ERR_MSG_SIGSET,
						siq, "SIG_IGN", errno_string());
					return -1;
				}
				break;
		}
	}


	return 0;
}



