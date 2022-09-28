/* Copyright 10/18/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)snmpd.c	1.7 96/10/18 Sun Microsystems"


/* HISTORY
 * 5-20-96      Jerry Yeung     support security file and subtree reg.
 * 5-28-96  	Jerry Yeung	change main to sap_main()
 * 6-3-96       Jerry Yeung     a flag for not reading config file
 * 6-11-96	Jerry Yeung	max number of agent registration retry
 * 10-18-96	Jerry Yeung	waiting port message
 */

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <syslog.h>


#include "impl.h"
#include "error.h"
#include "trace.h"
#include "signals.h"
#include "snmp.h"
#include "pdu.h"


#include "agent_msg.h"
#include "agent.h"
#include "config.h"


/***** DEFINES *****/

#define DEFAULT_POLL_INTERVAL		30


/***** IMPORTED VARIABLES *****/

/* user defined data */

extern char default_config_file[];
extern char default_sec_config_file[];
extern char default_error_file[];


/***** IMPORTED FUNCTIONS *****/

/* user defined functions */
extern void agent_init();
extern void agent_end();
extern void agent_loop();
extern void agent_select_info(fd_set *fdset, int *numfds);
extern void agent_select_callback(fd_set *fdset);



/***** STATIC VARIABLES *****/

static int sighup = False;
char *config_file = NULL;
char *sec_config_file = NULL;
int agent_port_number = -1;

static int dont_read_config_file = FALSE;

static int poll_interval = DEFAULT_POLL_INTERVAL;
int max_agent_reg_retry = 10;



/***** LOCAL FUNCTIONS *****/

static void signals_sighup(int siq);
static void signals_exit(int siq);

static int snmpd_init(int port);
static void print_usage(char *command_name);
static void snmpd_loop(int sd);


/********************************************************************/

static void application_end()
{
	agent_end();
}


/********************************************************************/

static void signals_sighup(int siq)
{
	if(trace_level > 0)
	{
		trace("received signal SIGHUP(%d)\n\n", siq);
	}

	error(MSG_SIGHUP, siq);

	sighup = True;
}


/********************************************************************/

static void signals_exit(int siq)
{
	error_exit("received signal %d", siq);

	application_end();
}


/********************************************************************/

static int snmpd_init(int port)
{
	int sd;
	struct sockaddr_in me;


/* init the config_file pointer and then parse the configuration file */

     if(dont_read_config_file == FALSE){
	if(config_file == NULL)
	{
		config_file = default_config_file;
	}

	config_init(config_file);
      }

	if(sec_config_file == NULL)
	{
		sec_config_file = default_sec_config_file;
	}

	sec_config_init(sec_config_file);

/* successfully register the subagent, then set the operation status of
   subagent to run */

/* Set up connection */

	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sd < 0)
	{
		error_exit(ERR_MSG_SOCKET, errno_string());
	}

	/* evaluate the port to be used, the port priority is :
	  command port > config. file port > def. port */
	if(port == 0 && agent_port_number != 0){
		port = agent_port_number;
	}

	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(port);
	if(bind(sd, (struct sockaddr *)&me, sizeof(me)) != 0)
	{
		error_exit(ERR_MSG_BIND, port, errno_string());
	}

	if(trace_level > 0)
	{
		trace("Waiting for incoming SNMP requests on UDP port %d\n\n", port);
	}

	return sd;
}


/********************************************************************/

static void snmpd_loop(int sd)
{
	int numfds;
	fd_set fdset;
	int count;
	struct timeval expire;
	struct timeval timeout;
	struct timeval now;

	expire.tv_sec = 0;
	expire.tv_usec = 0;


	while(1)
	{
		if(sighup)
		{
			error(MSG_READING_CONFIG,
				config_file);

			config_init(config_file);

			error(MSG_READING_CONFIG,sec_config_file);

			sec_config_init(sec_config_file);

			error(MSG_CONFIG_READED);

			sighup = False;
		}

		numfds = 0;
		FD_ZERO(&fdset);

		numfds = sd + 1;
		FD_SET(sd, &fdset);

		agent_select_info(&fdset, &numfds);
		
		gettimeofday(&now, (struct timezone *) 0);

/*
trace("now:       %s\n", timeval_string(&now));
trace("expire:    %s\n", timeval_string(&expire));
*/
		if( (now.tv_sec > expire.tv_sec)
			|| ( (now.tv_sec == expire.tv_sec)
				&& (now.tv_usec > expire.tv_usec) ) )
		{
			/* now > timeval + poll_interval */
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
		}
		else
		{
			if(expire.tv_usec - now.tv_usec > 0)
			{
				timeout.tv_sec = expire.tv_sec - now.tv_sec;
				timeout.tv_usec = expire.tv_usec - now.tv_usec;
			}
			else
			{
				timeout.tv_sec = expire.tv_sec - now.tv_sec - 1;
				timeout.tv_usec = expire.tv_usec - now.tv_usec + 1000000L;
			}
		}
/*
trace("timeout:   %s\n\n", timeval_string(&timeout));
*/


		count = select(numfds, &fdset, 0, 0, &timeout);
		if(count > 0)
		{
			if(FD_ISSET(sd, &fdset))
			{
				Address address;
				SNMP_pdu *pdu;


				if((pdu = snmp_pdu_receive(sd, &address, error_label)) == NULL)
				{
					error(ERR_MSG_PDU_RECEIVED,
						address_string(&address),
						error_label);
					continue;
				}

				if(agent_process(&address, pdu) == -1)
				{
					error(ERR_MSG_PDU_PROCESS,
						address_string(&address));
					snmp_pdu_free(pdu);
					continue;
				}

				if( (pdu->error_status != SNMP_ERR_NOERROR)
					&& (pdu->error_status != SNMP_ERR_NOSUCHNAME) )
				{
					error(ERR_MSG_SNMP_ERROR,
						error_status_string(pdu->error_status),
						pdu->error_index,
						address_string(&address));
				}

				if(snmp_pdu_send(sd, &address, pdu, error_label) == -1)
				{
					error(ERR_MSG_PDU_SEND,
						address_string(&address),
						error_label);
					snmp_pdu_free(pdu);
					continue;
				}

				snmp_pdu_free(pdu);
			}

			agent_select_callback(&fdset);
		}
		else
		{
			switch(count)
			{
				case 0:
					gettimeofday(&expire, (struct timezone *) 0);
/*
trace("agent_loop() invoked at at %s\n\n", timeval_string(&expire));
*/
					expire.tv_sec = expire.tv_sec + poll_interval;
					agent_loop();
					break;

				case -1:
					if(errno == EINTR)
					{
						continue;
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

static void print_usage(char *command_name)
{
	fprintf(stderr, "Usage: %s [-h]\n\
\t[-k (don't read config file)]\n\
\t[-p port ]\n\
\t[-c config-file (default %s)]\n\
\t[-a sec-config-file (default %s)]\n\
\t[-i poll-interval (default %d seconds)]\n\
\t[-d trace-level (range 0..%d, default %d)]\n\n",
		command_name,
		default_config_file,
		default_sec_config_file,
		DEFAULT_POLL_INTERVAL,
		TRACE_LEVEL_MAX,
		trace_level);
	exit(1);
}


/********************************************************************/

sap_main(argc, argv)
	int argc;
	char *argv[];
{
	int arg;
	int port = 0;
	int sd;
	char *str;
	int level;
	char *error_file = NULL;



	error_init(argv[0], application_end);

	/* parse arguments */

	for(arg = 1; arg < argc; arg++)
	{
		if(argv[arg][0] == '-')
		{
			switch(argv[arg][1])
			{
                                case 'k':
                                        dont_read_config_file = TRUE;
                                        break;
				case 'h':
				case '?':
					print_usage(argv[0]);

					/* never reached */
					return 1;

				case 'p':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have another argument following the -p option\n");
						print_usage(argv[0]);
					}

					port = strtol(argv[arg], &str, 10);
					if(argv[arg] == str)
					{
						fprintf(stderr, "Not a valid integer following the -p option: %s\n", argv[arg]);
						print_usage(argv[0]);
					}

					break;

				case 'c':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have a configuration file name following the -c option\n");
						print_usage(argv[0]);
					}

					config_file = (char *) strdup(argv[arg]);
					if(config_file == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

				case 'a':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have a security configuration file name following the -a option\n");
						print_usage(argv[0]);
					}

					sec_config_file = (char *) strdup(argv[arg]);
					if(sec_config_file == NULL)
					{
						fprintf(stderr, "%s\n", ERR_MSG_ALLOC);
						exit(1);
					}

					break;

/*
				case 'l':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have a log file name following the -l option\n");
						print_usage(argv[0]);
					}

					error_file = argv[arg];

					break;


				case 's':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have another argument following the -s option\n");
						print_usage(argv[0]);
					}

					error_size = strtol(argv[arg], &str, 10);
					if(argv[arg] == str)
					{
						fprintf(stderr, "Not a valid integer following the -s option: %s\n", argv[arg]);
						print_usage(argv[0]);
					}

					break;
*/

				case 'i':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have another argument following the -i option\n");
						print_usage(argv[0]);
					}

					poll_interval = strtol(argv[arg], &str, 10);
					if(argv[arg] == str)
					{
						fprintf(stderr, "Not a valid integer following the -i option: %s\n", argv[arg]);
						print_usage(argv[0]);
					}
					if(poll_interval <= 0)
					{
						fprintf(stderr, "The poll-interval must be greater than 0: %d\n", poll_interval);
						print_usage(argv[0]);
					}

					break;

				case 'd':
					arg++;
					if(arg >= argc)
					{
						fprintf(stderr, "Must have another argument following the -d option\n");
						print_usage(argv[0]);
					}

					level = strtol(argv[arg], &str, 10);
					if(argv[arg] == str)
					{
						fprintf(stderr, "Not a valid integer following the -d option: %s\n", argv[arg]);
						print_usage(argv[0]);
					}
					if(trace_set(level, error_label))
					{
						print_usage(argv[0]);
					}

					break;

				default:
					fprintf(stderr, "Invalid option: -%c\n", argv[arg][1]);
					print_usage(argv[0]);
			}
			continue;
		}
	}


	if(error_file == NULL)
	{
		error_file = default_error_file;
	}
	error_open(error_file);

	if(trace_level == 0)
	{
		/* run the daemon in backgound */

		int pid; 


		pid = fork();
		switch(pid)
		{
			case -1:
				error_exit(ERR_MSG_FORK, errno_string());

				/* never reached */
				return 1;

			case 0: /* child process */
				break;

			default: /* parent process */
				exit(0);
		}
	}

	if(fclose(stdin) == EOF) 
	{
		error(ERR_MSG_FCLOSE, "stdin", errno_string());
	}

	sd = snmpd_init(port);

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
		/* backgound */

		if(chdir("/") == -1)
		{
			error(ERR_MSG_CHDIR, "/", errno_string());
		}

		/* set process group ID */
		setpgrp();

		error_close_stderr();
	}

	/* have to be called after error_open() and error_close_stderr() */
	agent_init();

	snmpd_loop(sd);

	/* never reached */
}

void SSAMain(int argc, char** argv)
{
  sap_main(argc,argv);
}
