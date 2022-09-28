/* Copyright 1988 - 11/15/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)agent.c	2.26 96/11/15 Sun Microsystems"
#else
static char sccsid[] = "@(#)agent.c	2.26 96/11/15 Sun Microsystems";
#endif
#endif

/****************************************************************************
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header	*/
/*
 * $Log:   E:/SNMPV2/AGENT/SUN/AGENT.C_V  $
 * 
 *    Rev 2.1   09 Apr 1990 14:23:08
 * Cast the address of inet_ntoa() so that it works on a Sun 4.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:16
 * Initial revision.
 * 
*/

#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
/* #include <time.h> */
#include <sys/time.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <sys/time.h>
#include <syslog.h>
#include <errno.h>
#ifdef BOGEY
#include <sys/stat.h>
#endif

#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>
#include "snmpvars.h"
#include "general.h"

#include "agent.h"

#include <print.h>
#define then

extern int errno;
extern int sys_nerr;
extern char *sys_errlist[];

#define TBSIZE 2048
unsigned char rcvbuff[TBSIZE];

#ifdef BOGEY
/* Hacks to recover from memory loss.
 */
static struct timeval start_time;
static char check_file[] = "/tmp/__snmpd_check__";
static int file_check_time = 15;
extern int cleanup_time;
extern int g_argc;
extern char **g_argv;
extern int already_detached;
 
int
check_file_present()
{
        struct stat buf;
        time_t cur_time;
 
        if (stat(check_file, &buf) < 0)
                return 0;
        if (!S_ISREG(buf.st_mode))
                return 0;
        unlink(check_file);
        cur_time = time((time_t *)0);
        if ((cur_time - buf.st_mtime) > file_check_time)
                return 0;
        return 1;
}
 
static void
check_time()
{
        struct timeval t;
        int i;
        char buff[500];
 
        (void) gettimeofday(&t, (struct timezone *)0);
        if (cleanup_time > 0 && (t.tv_sec - start_time.tv_sec) > cleanup_time) {      
                /*(void) gettimeofday(&t, (struct timezone *)0);*/
                i = open(check_file, O_CREAT | O_RDWR, 0666);
                for (i = 3; i < FD_SETSIZE; i++)
                        close(i);
                execvp(g_argv[0], g_argv);
                sprintf(buff, "argument %s %s",g_argv[0], g_argv[1]);
                syslog(LOG_ERR, buff);
                exit(1);
        }
}
#endif


/* Initialize the agent			*/
/* Return 0 on sucess, -1 on failure	*/
/*ARGSUSED*/
int
agent_init(port, syms)
	int	port;
	char	*syms;
{
struct sockaddr_in	srvr;

if (read_config() == -1)
   then {
   	SYSLOG1("Can not read configuration file `%s'\n",
	       config_file);
	return -1;
	}

TRC_PRT1(0,"Using kernel file `%s'\n", kernel_file);

if (setup_mib() == -1)
   then {
	SYSLOG0("Can not set up management information base (MIB)\n");
	return -1;
	}

if (trace_level > 0)
   then {
	int	i;
	OIDC_T *oidp;
	printf("\n\n****HOST INFORMATION****\n");
	printf("sysDescr is \"%s\"\n", snmp_sysDescr);
	printf("sysContact is \"%s\"\n", snmp_sysContact);
	printf("sysLocation is \"%s\"\n", snmp_sysLocation);
	printf("System Boot time was %24.24s\n",
	       (char *)ctime((time_t *)&(boot_at.tv_sec)));
	printf("System object identifier has %d components:\n\t",
       		snmp_sysObjectID.num_components);
	 for (i = 0, oidp = snmp_sysObjectID.component_list;
	      i < snmp_sysObjectID.num_components; i++, oidp++)
	    printf("%u.", *oidp);
	printf("\n");

	printf("Local IP address: %d.%d.%d.%d\n", snmp_local_ip_address[0],
	       snmp_local_ip_address[1], snmp_local_ip_address[2],
	       snmp_local_ip_address[3]);

	printf("\n\n****SNMP AGENT INFORMATION****\n");
	printf("Community names:\n");
	printf("\tSystem group read:  \"%s\"\n", snmp_sysgrp_read_community);
	printf("\tSystem group write: \"%s\"\n", snmp_sysgrp_write_community);
	printf("\tFull MIB read:      \"%s\"\n", snmp_fullmib_read_community);
	printf("\tFull MIB write:     \"%s\"\n", snmp_fullmib_write_community);

	if (read_only != 0)
	   then printf("Agent is operating in read-only mode\n");

	if (trap_2_cnt > 0)
	   then {
		struct in_addr ia;
		printf("Traps will be sent to:\n");
		for(i = 0; i < trap_2_cnt; i++)
		   {
		   ia.s_addr = traplist[i];	   
		   printf("\t%s\n", inet_ntoa(ia));
		   }
		}
	   else printf("No traps receivers defined\n");

	if (mgr_cnt > 0)
	   then {
		struct in_addr ia;
		printf("Managers:\n");
		for(i = 0; i < mgr_cnt; i++)
		   {
		   ia.s_addr = mgr_list[i];	   
		   printf("\t%s\n", inet_ntoa(ia));
		   }
		}
	   else printf("Queries may be received from any manager\n");

	printf("SNMP service will be provided on port %d\n", port);

#if 0
	if (refresh_minutes == 0)
	   then printf("Agent will persist indefinitely without input packets\n");
	   else printf("Agent will persist for %d minutes without input packets\n",
		       refresh_minutes);
#endif /* 0 */

	printf("\n\n");
	fflush(stdout);
	}

if ((snmp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
   then {
        SYSLOG0("Can't get socket");
	return -1;
	}

srvr.sin_family = AF_INET;
srvr.sin_port = htons(port);
#if (BSD_RELEASE < 43)
srvr.sin_addr.S_un.S_addr = 0L;
#else	/* (BSD_RELEASE < 43) */
srvr.sin_addr.s_addr = 0L;
#endif	/* (BSD_RELEASE < 43) */

if (bind(snmp_socket, (struct sockaddr *)&srvr, sizeof(srvr)) == -1)
   then {
        SYSLOG1("Can't bind server socket for port # = %d, try another port in /etc/snmp/conf/mibiisa.reg",port);
	return -1;
	}

(void)time(&cache_now);
if (snmpvars_init() == -1) {SYSLOG0("snmpvars_init failed");return -1; }
if (libfuncs_init() == -1) {SYSLOG0("libfuncs_init failed");return -1; }
if (sys_init() == -1) {SYSLOG0("sys_init failed"); return -1;}   /* Must preceed if_init() */
if (if_init() == -1) {SYSLOG0("if_init failed"); return -1;}    /* Must come somewhere after sys_init */
if (arp_init() == -1) {SYSLOG0("arp_init failed"); return -1; }
if (icmp_init() == -1) {SYSLOG0("icmp_init failed"); return -1; }
if (ip_init() == -1) { SYSLOG0("ip_init failed"); return -1; }
if (icmp_init() == -1) { SYSLOG0("icmp_init failed"); return -1; }
if (iprte_init() == -1) { SYSLOG0("iprte_init failed"); return -1; }
if (tcp_init() == -1) { SYSLOG0("tcp_init failed"); return -1; }
if (udp_init() == -1) { SYSLOG0("udp_init failed");return -1; }

return 0;
}

/*ARGSUSED*/
void
agent_body(port, syms)
	int	port;
	char	*syms;
{
EBUFFER_T		ebuff;
struct sockaddr_in	from;
struct sockaddr_in	dest;
socklen_t		szfrom;
unsigned int		pktnum;

#ifdef BOGEY
/* note starting time */
(void) gettimeofday(&start_time, NULL);
#endif

(void)memset(&dest, 0, sizeof(dest));

#ifdef BOGEY
if (!already_detached)
#endif
   send_traps(snmp_socket, COLD_START, 0);

EBufferInitialize(&ebuff);
EBufferClean(&ebuff);

TRC_PRT0(2, "Waiting for input...\n");

for(pktnum = 0;;pktnum++)
   {
   int got;
   struct timeval rcvd_time;

#if 0
   if (refresh_minutes != 0)
      then {
           int sel_rcode;
	   fd_set readfds, writefds, exceptfds;
	   struct timeval refresh;

	   FD_ZERO(&readfds);
	   FD_ZERO(&writefds);
	   FD_ZERO(&exceptfds);
	   FD_SET(snmp_socket, &readfds);

           refresh.tv_sec = refresh_minutes * 60;
	   refresh.tv_usec = 0;
	   sel_rcode = select(FD_SETSIZE, &readfds, &writefds, &exceptfds,
			      &refresh);
	   switch (sel_rcode)
	      {
	      case -1:		/* Select failed       */
	           PERROR("Can't select");
		   SYSLOG1("Cant select: %s", sys_errlist[errno]);
		   closelog();
		   exit(10);
	  
	      case 0:		/* Nothing arrived and the select timed out */
		   		/* We only do this if running under inetd   */
		   if (!inetd) then break;
		   /* printf("Terminating due to lack of interest\n"); */
		   closelog();
		   exit(0);
	  
	      default:	/* Something has arrived */
		   break;
	      }
	   }
#endif /* 0 */

   /* Receive the query into rcvbuff */
   szfrom = (socklen_t)sizeof(from);
   if ((got = recvfrom(snmp_socket, (char *)rcvbuff, sizeof(rcvbuff),
		       0, (struct sockaddr *)&from, &szfrom))
	== -1)
      then {
	   PERROR("Recvfrom failed");
	   continue;
	   }

   (void) gettimeofday(&rcvd_time, (struct timezone *)0);
   TRC_PRT2(1, "Received packet from %s at %s",
	    inet_ntoa(from.sin_addr),
	    ctime((time_t *)&rcvd_time.tv_sec));

   (void)time(&cache_now);
   if (Process_Received_SNMP_Packet(rcvbuff, got, (SNMPADDR_T *)&from, 
				    (SNMPADDR_T *)&dest, &ebuff) == -1)
      then {
	   TRC_PRT0(0, "Error occured while processing query\n");
	   continue;
	   }

   TRC_PRT0(1, "Sending reply\n");

   /* Send the response packet */
   (void) sendto(snmp_socket, (char *)ebuff.start_bp, EBufferUsed(&ebuff),
		 0, (struct sockaddr *)&from, szfrom);

   EBufferClean(&ebuff);
   fflush(stdout);

#if 0
/*PLEASE DO NOT KILL this process*/
#ifdef BOGEY
   check_time();
#endif
#endif
   }
}
