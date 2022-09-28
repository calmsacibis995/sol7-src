/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)setup-svr4.c	1.11 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)setup-svr4.c	1.11 96/07/23 Sun Microsystems";
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
 * $Log:   E:/SNMPV2/AGENT/SUN/SETUP.C_V  $
 * 
 *    Rev 2.1   09 Apr 1990 14:22:40
 * Made the compilation of strnicmp() depend on the STRNICMP preprocessor
 * symbol.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:14
 * Initial revision.
 * 
*/

#include <stdio.h>
#include <sys/types.h>
#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>
#include "snmpvars.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <syslog.h>
#include <fcntl.h>
#include <utmp.h>

#include <netdb.h>
#include <ctype.h>
#include "agent.h"
#include "general.h"

#include <print.h>
#define then

extern int errno;

#if !defined(NO_PP)
static char *	to_l_case(char *);
#else	/* NO_PP */
static	void	set_trap_to();
static char *	to_l_case();
#endif	/* NO_PP */

static char nolocal[] ="Can not obain local IP address -- is \"%s\" in \"/etc/hosts\"?\n";


/**********************************************************************/
/*                                                                    */
/*  Initialize the relatively static parts of the MIB                 */
/*                                                                    */
/**********************************************************************/
int read_boottime(void)
{
struct utmp ute;
int fd;

static char utmp_file[] = UTMP_FILE;

if ((fd = open(utmp_file, O_RDONLY)) == -1)
   then {
        PERROR("Can not open utmp file");
	return -1;
        }

for(;;)
    {
    if (read(fd, (char *)&ute, sizeof(struct utmp)) != sizeof(struct utmp))
       then {
	    PRNTF1("Failed to find boot time in %s\n", utmp_file);
	    close(fd);
	    return -1;
	    }
    if (ute.ut_type != BOOT_TIME) then continue;
    boot_at.tv_sec = ute.ut_time;
    close(fd);
    return 0;
    }
/* NOTREACHED */
}

int
setup_mib()
{
struct hostent	*host;

TRC_PRT0(0, "Initializing MIB...\n");

/* Get the time the system booted */
if (read_boottime() == -1)
   then {
	PRNTF0("Error reading boot time\n");
	return -1;
	}

/* Get a local IP address */
(void)gethostname(snmp_sysName, MAX_SYSNAME);
if ((host = gethostbyname(snmp_sysName)) == (struct hostent *)0)
   then {
	PRNTF1(nolocal, snmp_sysName);
	SYSLOG1(nolocal, snmp_sysName);
	closelog();
	exit(30);
        }

 {
 u_long tlong;
 int    tmp;
 tlong = ntohl(*(u_long *)(host->h_addr_list[0]));
 (void)memcpy(snmp_local_ip_address, (char *)&tlong, 4);
#ifdef i386
	tmp = snmp_local_ip_address[0];
	snmp_local_ip_address[0] = snmp_local_ip_address[3];
	snmp_local_ip_address[3] = tmp;
	tmp = snmp_local_ip_address[1];
	snmp_local_ip_address[1] = snmp_local_ip_address[2];
	snmp_local_ip_address[2] = tmp;
#endif
 }

return 0;
}

int
name_to_ulong(tname, tlong)
	char *	  tname;
	u_long *  tlong;
{
struct hostent		*host;

if (isdigit(*tname))
   then {
	*tlong = (u_long)inet_addr(tname);
	}
   else {
	if ((host = gethostbyname(tname))
	     == (struct hostent *)0)
  	   then return -1;
	*tlong = *(u_long *)(host->h_addr);
	return 0;
	}
/*NOTREACHED*/
}

#if defined(STRNICMP)
/* The library seems not to have strnicmp()	*/
int
strnicmp(str1, str2, cnt)
	char *str1;
	char *str2;
	int  cnt;
{
u_char c1, c2;
for(;;cnt--, str1++, str2++)
   {
   if (cnt == 0) then return 0;	/* Strings were equal */

   if (isupper(*str1))
      then c1 = tolower(*str1);
      else c1 = *str1;   
   
   if (isupper(*str2))
      then c2 = tolower(*str2);
      else c2 = *str2;   
   
   if (c1 == c2)
      then {
	   if (c1 == '\0') then return 0;
	   }
      else {
	   if (c1 < c2)
	      then return -1;
	      else return 1;
	   }
   }
/*NOTREACHED*/
}
#endif /* STRNICMP */
