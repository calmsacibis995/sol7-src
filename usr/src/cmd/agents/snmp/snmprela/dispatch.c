/* Copyright 07/16/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dispatcher.c	1.20 97/07/16 Sun Microsystems"



/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/

/* HISTORY
 * 5-14-96 	Jerry Yeung	add relay_agent_name
 * 6-26-96	Jerry Yeung	name oid pair
 * 7-03-96	Jerry Yeung	add relay_agent_max_agent_time_out &
 *				relay_agent_poll_interval
 * 8-21-96	Jerry Yeung	change the way of handling arguments
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>


#include "impl.h"
#include "error.h"
#include "trace.h"
#include "signals.h"
#include "snmp.h"
#include "pdu.h"

#include "snmprelay_msg.h"
#include "agent.h"
#include "subtree.h"
#include "session.h"
#include "config.h"
#include "dispatcher.h"
#include "res.h"


/***** LOCAL CONSTANTS ******/

/*
 *	select() will returned at least every
 *	POLL_INTERVAL seconds to test if some
 *	sessions have SNMP requests that have
 *	timeout (<==> the SNMP agnet has not
 *	responded)
 */

#define POLL_INTERVAL		240


/***** GLOBAL VARIABLES ******/

IPAddress my_ip_address;

int clients_sd = -1;	/* socket descriptor "connected" to SNMP application */
int agents_sd = -1;	/* socket descriptor "connected" to SNMP agent */
int trap_sd = -1; /* socket descriptor for receiving traps from agents */
int relay_agent_trap_port = 162;

/* (5-14-96) */
char *relay_agent_name = NULL;
char *sec_config_file ;
char *pid_file;
char *name_oid_file;

int relay_agent_max_agent_time_out= 987654321;
int relay_agent_poll_interval = POLL_INTERVAL;


/*
 *	the name of the directory that
 *	contains the configuration files
 */

char *config_dir = NULL;
char *resource_file = NULL;

/* There are two available modes to run the snmprelayd		*/
/*	1) MODE_SPLIT: we send a request for each variable	*/
/*	2) MODE_GROUP: we try to group the variables		*/
int mode = MODE_GROUP;
int recovery_on = FALSE;


/***** LOCAL VARIABLES *****/

static int sighup = False;	/* if True, we received the SIGHUP signal */


static char default_config_dir[] = "/etc/snmp/conf";
static char default_resource_file[] = "/etc/snmp/conf/snmpdx.rsrc";
static char default_error_file[] = "/var/snmp/snmpdx.log";

static char default_relay_agent_name[] = "relay-agent"; /*(5-14-96) */

static char default_pid_file[] = "/var/snmp/snmpdx.st";

static char default_name_oid_file[] = "/etc/snmp/conf/enterprises.oid";


/***** LOCAL FUNCTIONS *****/

static void application_end();

static void signals_sighup(int siq);
static void signals_exit(int siq);
static void signals_child(int siq);

static void dispatcher_init(int port);
static void dispatcher_loop();
static void print_usage();


/*******************************************************************/

/*
 *	this function is called by error_exit()
 */

static void application_end()
{
}


/*******************************************************************/

static void signals_sighup(int siq)
{
	if(trace_level > 0)
	{
		trace("received signal %d\n\n", siq);
	}

	error(MSG_SIGHUP, siq);

	sighup = True;
}

/*******************************************************************/

static void signals_exit(int siq)
{
	error_exit("received signal %d", siq);
}


static void init_relay_agent(int port)
{
  /* find agent matches the name, then modify
     the process id, operstatus */
  Agent *ap = agent_find_by_name(relay_agent_name);
  if(ap==NULL) return;
  if(ap){
	ap->agentProcessID = (int)getpid();
	ap->agentStatus = SSA_OPER_STATUS_ACTIVE;
	if(port!=SNMP_PORT){
		ap->address.sin_port = 
		ap->agentPortNumber = port;
	}
  }
  /* init the subtrees of relay agent */
  agent_update_subtree(ap);
}


/********************************************************************/

static void dispatcher_init(int port)
{
	struct sockaddr_in me;
	socklen_t len;

/* init my_ip_address (we need it before parsing the config file) */

	if(get_my_ip_address(&my_ip_address, error_label))
	{
		error_exit(ERR_MSG_MY_IP_ADDRESS,
			error_label);
	}

	if(trace_level > 0)
	{
		trace("Local IP Addresss : %s\n\n",
			inet_ntoa(my_ip_address));
	}


/* init the config_dir pointer and then parse the configuration files */

	if(config_dir == NULL)
	{
		config_dir = default_config_dir;
	}

	config_init(config_dir);

/* (6-26-96) read enterprise name-oid file */
	if(name_oid_file == NULL)
	{
		name_oid_file = default_name_oid_file;
	}
	load_enterprise_oid(name_oid_file);

/* (5-14-96) set up the relay agent name */
	if(sec_config_file == NULL)
	{
		sec_config_file = default_sec_config_file;
	}
	sec_config_init(sec_config_file);
	if(relay_agent_name == NULL)
	{
		relay_agent_name = default_relay_agent_name;
	}
	init_relay_agent(port);

/* (5-28-96) read the resource file */
	if(resource_file == NULL)
	{
		resource_file = default_resource_file;
	}
	if(pid_file == NULL)
 	{
		pid_file = default_pid_file;
	}
	res_config_init(config_dir);
	write_pid_file(pid_file);

/* init clients_sd and agents_sd */

	clients_sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(clients_sd < 0)
	{
		error_exit(ERR_MSG_SOCKET, errno_string());
	}

	memset(&me, 0, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(port);

	if(trace_level > 0)
	{
		trace("Waiting for incoming SNMP requests on UDP port %d\n\n", port);
	}
	if(bind(clients_sd, (struct sockaddr *)&me, sizeof(me)) != 0)
	{
		error_exit(ERR_MSG_BIND, port, errno_string());
	}


	agents_sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(agents_sd < 0)
	{
		error_exit(ERR_MSG_SOCKET, errno_string());
	}
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(0);
	if(bind(agents_sd, (struct sockaddr *)&me, sizeof(me)) != 0)
	{
		error_exit(ERR_MSG_BIND, 0, errno_string());

	}

	trap_sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(trap_sd < 0)
	{
		error_exit(ERR_MSG_SOCKET, errno_string());
	}
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(0);

	if(bind(trap_sd, (struct sockaddr *)&me, sizeof(me)) != 0)
	{
		error_exit(ERR_MSG_BIND, 0, errno_string());

	}

	len = (socklen_t)sizeof(me);
	if (getsockname(trap_sd, (struct sockaddr *)&me, &len) == -1) {
		error_exit(ERR_MSG_BIND, 0, errno_string());
	}
	relay_agent_trap_port = me.sin_port;
}


/********************************************************************/

static void dispatcher_loop()
{
	int numfds;
	fd_set fdset;
	int count;
	struct timeval timeout;


	timeout.tv_usec = 0;

	while(1)
	{
		if(sighup)
		{
			resource_update(config_dir);
			sighup = False;
		}

		if(trace_level > 1)
		{
			trace_sessions();
		}

		numfds = 0;
		FD_ZERO(&fdset);

		numfds = MAX(clients_sd, agents_sd);
		numfds = MAX(numfds,trap_sd);
		numfds++;
		FD_SET(clients_sd, &fdset);
		FD_SET(agents_sd, &fdset);
		FD_SET(trap_sd, &fdset);

		timeout.tv_sec = relay_agent_poll_interval;

		/* we compute the timeout according to the	*/
		/* timeout of the pending requests		*/
		session_select_info(&timeout);

		count = select(numfds, &fdset, 0, 0, &timeout);
		if(count > 0)
		{
			if(FD_ISSET(agents_sd, &fdset))
			{
				/* we read the responses of the agents */
				session_read();
				continue;
				
			}

			if(FD_ISSET(trap_sd, &fdset))
			{
				/* working on the trap */
				trap_processing();
				continue;
				
			}

			if(FD_ISSET(clients_sd, &fdset))
			{
				/* we dispatch the requests of the application */
				session_dispatch();

			        session_timeout();
				continue;
			}

		}
		else
		{
			switch(count)
			{
				case 0:
					/* we check if some requests have timeout */
					session_timeout();
					  watch_dog_in_action();
					break;

				case -1:
					if(errno == EINTR)
					{
						break;
					}
					else
					{
						error_exit(ERR_MSG_SELECT, errno_string());
					}
			}
		}
	}
}


/********************************************************************/

/*
 *	\t[-m GROUP | SPLIT (default GROUP)]\n\
 */

static void print_usage()
{
	fprintf(stderr, "Usage: snmpdx [-h]\n\
\t[-h]\n\
\t[-n relay_agent_name (default %s)]\n\
\t[-p port (default %d)]\n\
\t[-r resource-file (default %s)]\n\
\t[-a access-control-file (default %s)]\n\
\t[-c config-dir (default %s)]\n\
\t[-i pid-file (default %s)]\n\
\t[-o enterprise-oid-file (default %s)]\n\
\t[-y]\n\
\t[-m GROUP | SPLIT (default GROUP)]\n\
\t[-d trace-level (range 0..%d, default %d)]\n\n",
		default_relay_agent_name,
		SNMP_PORT,
		default_resource_file,
		default_sec_config_file,
		default_config_dir,
		default_pid_file,
		default_name_oid_file,
		TRACE_LEVEL_MAX,
		trace_level);
	exit(1);
}


/********************************************************************/

main(argc, argv)
	int argc;
	char *argv[];
{
	int arg;
	int port = SNMP_PORT;
	char *str;
	int level;
	char *error_file = NULL;

 	extern char *optarg;
	extern int optind;
	int opt;

	error_init(argv[0], application_end);

	optind = 1;

	/* parse arguments */
	while((opt = getopt(argc,argv,"c:i:hr:m:o:p:a:d:y:n:?"))!=EOF){
		switch(opt){
				case 'h':
				case '?':
					print_usage();
					break;

				case 'y':
					recovery_on=TRUE;
					break;

				case 'p':
					port = strtol(optarg, &str, 10);
					if(optarg == str)
					{
						fprintf(stderr, "Not a valid integer following the -p option: %s\n", optarg);
						print_usage();
					}

					break;

				case 'n':
					relay_agent_name = strdup(optarg);
					if(relay_agent_name == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

				case 'o':
					if(optind > argc)
					{
						fprintf(stderr, "must have the enterprise name-oid file\n");
						print_usage();
					}

					name_oid_file= strdup(optarg);
					if(name_oid_file == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

				case 'c':
					if(optind > argc)
					{
						fprintf(stderr, "Must have a configuration directory name following the -c option\n");
						print_usage();
					}

					config_dir = strdup(optarg);
					if(config_dir == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

                                case 'a':
                                        if(optind > argc)
                                        {
                                                fprintf(stderr, "Must have a access control filename following the -a option\n");
                                                print_usage();
                                        }

                                        sec_config_file = strdup(optarg);
                                        if(sec_config_file == NULL)
                                        {
                                                fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
                                                exit(1);
                                        }

                                        break;

				case 'r':
					if(optind > argc)
					{
						fprintf(stderr, "Must have a resource file name following the -r option\n");
						print_usage();
					}

					resource_file = strdup(optarg);
					if(resource_file == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

				case 'i':
					if(optind > argc)
					{
						fprintf(stderr, "Must have a pid file name following the -i option\n");
						print_usage();
					}

					pid_file = strdup(optarg);
					if(pid_file == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;


				case 'd':
					if(optind> argc)
					{
						fprintf(stderr, "Must have a trace-level following the -d option\n");
						print_usage();
					}

					level = strtol(optarg, &str, 10);
					if(optarg == str)
					{
						fprintf(stderr, "Not a valid integer following the -d option: %s\n", optarg);
						print_usage();
					}

					if(trace_set(level, error_label))
					{
						print_usage();
					}

					break;

				case 'm':
					if(optind > argc)
					{
						fprintf(stderr, "Must have GROUP or SPLIT following the -m option\n");
						print_usage();
					}

					if(strcmp(optarg, "GROUP") == 0)
					{
						mode = MODE_GROUP;
					}
					else
					if(strcmp(optarg, "SPLIT") == 0)
					{
						mode = MODE_SPLIT;
					}
					else
					{
						fprintf(stderr, "Invalid mode: %s\n", optarg);
						print_usage();
					}

					break;

				default:
					fprintf(stderr, "Invalid Option: -%c\n", optarg);
					print_usage();
					break;
			}
		}


/*
	if(error_file == NULL)
	{
		error_file = default_error_file;
	}
	error_open(error_file);
*/

	if(trace_level == 0)
	{
		/* run the daemon in backgound */

		int pid;

		pid = fork();
		switch(pid)
		{
			case -1:
				error_exit(ERR_MSG_FORK, errno_string());
				break;

			case 0: /* child process */
				break;

			default: /* parent process */
				exit(0);
				break;
		}
	}

	if(fclose(stdin) == EOF)
	{
		error(ERR_MSG_FCLOSE, "stdin", errno_string());
	}

	dispatcher_init(port);

	if(signals_init(signals_sighup, signals_exit, error_label))
	{
		error_exit("signals_init() failed: %s", error_label);
	}
	
	if(trace_level == 0)
	{
		if(fclose(stdout) == EOF)
		{
			error(ERR_MSG_FCLOSE, "stdout", errno_string());
		}
	}

	if(trace_level == 0)
	{
		/* background */

		if(chdir("/") == -1)
		{
			error(ERR_MSG_CHDIR, "/", errno_string());
		}

		/* set process group ID */
		setpgrp();

		error_close_stderr();
	}

	dispatcher_loop();
}


