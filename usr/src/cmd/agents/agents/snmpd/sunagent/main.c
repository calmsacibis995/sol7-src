/* Copyright 1988 - 02/07/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)main.c	2.31 97/02/07 Sun Microsystems"
#else
static char sccsid[] = "@(#)main.c	2.31 97/02/07 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1988, 1989, 1990  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/**********************************************************************/
/*                                                                    */
/*  Simple SNMP agent                                                 */
/*                                                                    */
/**********************************************************************/

/* $Header:   E:/snmpv2/agent/agent.c_v   2.0   31 Mar 1990 15:19:50  $	*/
/*
 * $Log:   E:/snmpv2/agent/agent.c_v  $
 * 
 *    Rev 2.0   31 Mar 1990 15:19:50
 * Initial revision.
*/

/* In the makefile:							*/
/*	For 4.2bsd (Sun OS 3) systems set BSD_RELEASE to "42"			*/
/*	For 4.3bsd (Sun OS 4) systems set BSD_RELEASE to "43"			*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

#include "agent.h"
#include "snmpvars.h"
#include "patchlevel.h"

#define then

extern int errno;


static char usage[] =
"Usage: mibiisa [-a] [-r]\n             [-p port (default 161)]\n             [-c config-dir (default /etc/snmp/conf)]\n             [-d trace-level (range 0..3)]\n";
static char not_init[] = "Can not initialize";
static char no_ssid[] = "Can not set session ID";

/* Stuff for getopt()	*/
extern char *	optarg;
extern int	optint, opterr;

#if defined(__STDC__)
/* extern char	getopt(int, char **, char *); */
extern int	getopt(int, char *const*,const char *);
#else	/* __STDC__ */
extern char	getopt();
#endif	/* __STDC__ */

#ifdef BOGEY
/* Hacks for memory loss problem.
 */
int g_argc;
char **g_argv;
int already_detached;
int cleanup_time = 10800;
extern int check_file_present();
#endif

/* Let SIGUSR1 increment the trace level and SIGUSR0 reset it to zero */
void sigusr_handler(sig) /* , code, scp, addr) */
	int	sig;
/*	int	code;	       */
/*	struct sigcontext *scp;*/
/*	char	addr;		*/
{
switch (sig)
   {
   case SIGUSR1:
   	trace_level++;
	break;

   case SIGUSR2:
   	trace_level = 0;
	break;

   default:
	break;
   }
}

main(argc, argv)
	int argc;
	char **argv;
{
int			portnum;
char			c;

#ifdef BOGEY
g_argc = argc;
g_argv = argv;
#endif

openlog("mibiisa", LOG_CONS, LOG_DAEMON);

portnum = AGENT_UDP_PORT;
trace_level = 0;

opterr = 0;	/* Keep getopt quiet */
#ifdef BOGEY
while ((c = getopt(argc, argv, "ap:c:rd:C:")) != (char)-1)
#else
while ((c = getopt(argc, argv, "ap:c:rd:")) != (char)-1)
#endif
   {
   switch(c)
      {
      case 'a':	/* disable authentication traps */
      		snmp_auth_traps = 0;
		break;

      case 'c':	/* configuration dir name */
		strncpy(config_file, optarg, MAX_CONFIG_FILE - 1);
                strcat(config_file, "/snmpd.conf");
		break;

      case 'p':	/* port number */
	        portnum = atoi(optarg);
		break;

      case 'r':	/* Read-only lock */
	        read_only = 1;
		break;

#if 0
      case 't':	/* Time can live without input, zero is forever	*/
		/* Unit is in minutes.				*/
	        refresh_minutes = atoi(optarg);
		break;
#endif /* 0 */

      case 'd':	/* Tracing level */
	        trace_level = atoi(optarg);
		break;

#ifdef BOGEY
      case 'C': /* Cleanup time */
                cleanup_time = atoi(optarg);
                if (cleanup_time < 60)
                    cleanup_time = 0;
                break;
#endif
      case '?':	/* Unknown */
      default:
		PRNTF0(usage);
		SYSLOG0("bad command line usage");
		closelog();
		exit(1);
      }
   }

#ifdef BOGEY
already_detached = check_file_present();
#endif

/* Spawn a background process */
/* Don't fork if we are tracing */

if ( trace_level == 0 ) {
#if 0
#ifdef BOGEY
	if (!already_detached)
#endif
		if (fork() != 0) then exit(0);
#endif

	fclose(stdin); 
	fclose(stdout);
	fclose(stderr);
 
}

/* Give the agent-specific code a chance to initialize	*/
if (agent_init(portnum, argv[0]) == -1)
   then {
        /*PRNTF1("%s\n", not_init);
        SYSLOG0(not_init);*/
	closelog();
	exit(2);
    	}

if ( trace_level == 0 ) {
	if (!already_detached && setsid() == (int)-1)
	   then {
		PERROR(no_ssid);
		SYSLOG0(no_ssid);
		closelog();
		exit(3);
        }

   /* Ignore many maskable signals */
   /* Allow illegal instruction and bus error to make it through */
   {
   int signum;
   for (signum = 1; signum <= SIGUSR2; signum++)
      {
      if (signum == SIGILL) then continue;
      if (signum == SIGBUS) then continue;
      (void) signal(signum, SIG_IGN);
      }
   }
}

/* Let the two user signals control the trace level */
signal(SIGUSR1, sigusr_handler);
signal(SIGUSR2, sigusr_handler);

/* call into the agent-specific body
 */
(void) agent_body(portnum, argv[0]);
}
