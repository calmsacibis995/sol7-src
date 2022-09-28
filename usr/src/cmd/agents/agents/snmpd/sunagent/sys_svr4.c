/* Copyright 1988 - 11/15/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)sys-svr4.c	1.13 96/11/15 Sun Microsystems"
#else
static char sccsid[] = "@(#)sys-svr4.c	1.13 96/11/15 Sun Microsystems";
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
 * $Log:   D:/snmpv2/agent/sun/sys.c_v  $
 * 
 *    Rev 2.2   19 Jun 1990 17:02:06
 * Removed redundant min() functions.
 * 
 *    Rev 2.1   23 May 1990 10:52:56
 * Removed the routine to set sysLocation -- it is a read-only variable.
 * 
 *    Rev 2.0   31 Mar 1990 15:34:24
 * Initial revision.
 * 
*/

#include <stdio.h>
/* #include <stdlib.h> */
#include <memory.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>

#include <sys/utsname.h>
#include <sys/systeminfo.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include "snmpvars.h"
#include "general.h"

/*ARGSUSED*/
UINT_32_T
get_sysUpTime(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie; {
	struct timeval now_is;

	(void) gettimeofday(&now_is, NULL);

	return (UINT_32_T)(((now_is.tv_sec - boot_at.tv_sec) * 100) +
			   (now_is.tv_usec / 10000));
	}

/*ARGSUSED*/
unsigned char *
get_sysName(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T	lastmatch;
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
	int	*lengthp;
{
static int nm_length = 0;
/* The address returned *MUST* reference a static location */
if (nm_length == 0)
  {
  (void)gethostname(snmp_sysName, MAX_SYSNAME);
  nm_length = strlen(snmp_sysName);
  }
*lengthp = nm_length;
return (unsigned char *)snmp_sysName;
}

/*ARGSUSED*/
void
set_sysLocation(lastmatch, compc, compl, cookie, cp, length)
	OIDC_T	lastmatch;
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
	char	*cp;
	int	length;
{
int lngth;
if (length != 0)
  {
  lngth = min(MAX_SYSLOCATION-1, length);
  (void) memcpy(cookie, cp, lngth);
  cookie[lngth] = '\0';
  }
}

/*ARGSUSED*/
void
set_sysContact(lastmatch, compc, compl, cookie, cp, length)
	OIDC_T	lastmatch;
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
	char	*cp;
	int	length;
{
int lngth;
if (length != 0)
  {
  lngth = min(MAX_SYSCONTACT-1, length);
  (void) memcpy(cookie, cp, lngth);
  cookie[lngth] = '\0';
  }
}

/*ARGSUSED*/
unsigned char *
get_hostID(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T	lastmatch;
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
	int	*lengthp;
{
static unsigned long int my_host_id;

char sibuf[16];
(void) sysinfo(SI_HW_SERIAL, sibuf, (long) sizeof(sibuf));
my_host_id = atol(sibuf);

*lengthp = sizeof(unsigned long int);
return (unsigned char *)&my_host_id;
}

/* Get the first line of /etc/motd */
/*ARGSUSED*/
unsigned char *
get_motd(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T	lastmatch;
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
	int	*lengthp;
{
static char motd[256];
FILE *fd;

motd[0] = '\0';
fd = fopen("/etc/motd", "r");
fgets(motd, sizeof(motd), fd);
fclose(fd);

*lengthp = strlen(motd);
return (unsigned char *)motd;
}

/*ARGSUSED*/
UINT_32_T
get_unixTime(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
{
struct timeval now_is;
(void) gettimeofday(&now_is, NULL);
return (UINT_32_T)now_is.tv_sec;
}

/**********************************************************************/
/*  Initialize this module...					      */
/**********************************************************************/
int
sys_init()
{
return 0;
}

