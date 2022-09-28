/* Copyright 09/27/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)trapsend.c	1.10 96/09/27 Sun Microsystems"


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <nlist.h>

#include "snmp_msg.h"
#include "impl.h"
#include "trace.h"
#include "snmp.h"
#include "pdu.h"
#include "trap.h"
#include "error.h"
#include "oid.h"
#include "usage.h"


static int is_number (char *buf); 

main(argc, argv)
int argc;
char *argv[];
{
    extern char *optarg;
	extern int optind;
	int opt;

	char hostname[MAXHOSTNAMELEN];
	IPAddress ip_address;
	IPAddress my_ip_addr;	
	Oid *enterprise;
	int generic, specific, level; 
	SNMP_variable *variables;
	struct hostent *hp;
	int trap_port = -1;
	u_long time_stamp = (u_long)-1;
	int enterprise_flag= 0, a_flag = 0, i_flag = 0;
	
	optind = 1;
	
	/* the default host name is local host */
	gethostname(hostname, sizeof(hostname)); 

	/* default Oid for enterprise is sun */
	enterprise = &sun_oid; 

	/* generic, specific */
	generic = 6; 
	specific = 1;

	/* get command-line options */
    while ((opt = getopt(argc, argv, "h:c:e:E:g:s:i:t:a:T:p:")) != EOF) {
		switch (opt) {
			case 'T':
				level = atoi(optarg);
				if(trace_set(level, error_label)){
					fprintf(stderr, " %d is not a valid trace level!\n",
							level); 
					usage();
				}
				break; 
			case 'h':         /* host to send trap to */
				strcpy(hostname, optarg);
		
				break;
			case 'c':
				trap_community = optarg; 
				break; 
			case 'e':
				if (enterprise_flag) {
					usage();
				}				  
				enterprise = SSAOidStrToOid(optarg,error_label);
				if (!enterprise){ /* error */
					fprintf(stderr,
							"%s: not a valid enterprise oid string!\n",
							optarg);
					usage();
				}
				enterprise_flag = 1; 
				break; 
			case 'E':
				if (enterprise_flag) {
					usage();
				}
				enterprise = get_oid(optarg);
				if (!enterprise) {
					usage();
				}
				enterprise_flag = 1; 
				break; 
			case 'g':         /* generic trap type */
				if (is_number(optarg))
					usage(); 
				generic = atoi(optarg);
				if ((generic > 6 ) || (generic < 0))
					usage(); 
				break;
			case 's':         /* specific trap type */
				if (is_number(optarg))
					usage(); 
				specific = atoi(optarg);
				break;
			case 'i':
				if (name_to_ip_address(optarg,
									   &my_ip_addr,
									   error_label)) {
					usage();
				}
				i_flag = 1; 
				break; 
			case 't': /* timestamp */
				time_stamp = atol(optarg);
				break;
			case 'p':
				if (is_number(optarg))
					usage(); 
				trap_port = atoi(optarg);
				break; 
			case 'a':         /* attribute information */
				if ((variables = get_variable(optarg))== NULL){
					fprintf(stderr,
							"%s: not a valid variable!\n", optarg); 
					usage();
				}
				a_flag = 1; 
				break;
			case '?':		/* usage help */
				usage();
				break; 
			default:
				usage();
				break;
		}  /* switch */
	}/* while */

	if ((optind != argc) || (!a_flag))
		usage();
   
	if ((ip_address.s_addr = inet_addr(hostname)) == -1 ) {
		if ((hp = gethostbyname(hostname)) == NULL) {
			fprintf(stderr, "\n%s is not a valid hostname!\n\n", hostname);
			usage(); 
		}
		memcpy(&(ip_address.s_addr), hp->h_addr, hp->h_length);
	}



	/* some trace message */
	
	if (trap_send_with_more_para(&ip_address,
								 my_ip_addr, i_flag,
								 enterprise,
								 generic,
								 specific,
								 trap_port,
								 time_stamp,
								 variables,
								 error_label)) {
		fprintf(stderr, "trap_send not success!\n\n");
	} 

/*	if (trap_send(&ip_address,
				  enterprise,
				  generic,
				  specific,
				  variables,
				  error_label)) {
		fprintf(stderr, "trap_send not success!\n\n");
	}	 */
}



static int is_number (buf)
char *buf;
{
	int len, i;

	if (buf == NULL)
		return (-1);
	
	len = strlen(buf);
	for (i= 0; i < len; i++) 
		if (!isdigit(buf[i])) {
		fprintf(stderr, "\n%s is not a valid generic or specific trap type number!\n\n", buf);
		return(-1);
		}
	return (0);
}
	
