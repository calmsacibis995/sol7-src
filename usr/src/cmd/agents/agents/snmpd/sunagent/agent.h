/* Copyright 1988 - 08/30/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#pragma ident  "@(#)agent.h	2.18 96/08/30 Sun Microsystems"
#endif

/****************************************************************************
 *     Copyright (c) 1989, 1990  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header  $	*/

/*
 * $Log  $
 * 
*/

#if (!defined(agent_inc))
#define agent_inc

#define	AGENT_UDP_PORT	161

#define	CONFIG_FILE	"/etc/snmp/conf/snmpd.conf"
#define	MAX_CONFIG_FILE	128
extern char	config_file[];

#if 0
/* Say how many minutes the agent can go without a packet before */
/* killing itself.  Zero means forever.				 */
extern int	refresh_minutes;
#endif /* 0 */

extern int	trace_level;
extern int	read_only;	/* Set != 0 if writes are to be blocked */
extern int	sun_os_ver;	/* Set to 40, 41, ??? */

extern time_t	cache_now;	/* Time of each query.			*/

#define PERROR(M)	perror(M);
#define PRNTF0(M)	{printf(M);fflush(stdout);}
#define PRNTF1(M,A)	{printf(M,A);fflush(stdout);}
#define PRNTF2(M,A,B)	{printf(M,A,B);fflush(stdout);}
#define TRC_PRT0(L,M)	{if(trace_level>(L)){printf(M);fflush(stdout);}}
#define TRC_PRT1(L,M,A)	{if(trace_level>(L)){printf(M,A);fflush(stdout);}}
#define TRC_PRT2(L,M,A,B)	{if(trace_level>(L)){printf(M,A,B);fflush(stdout);}}
#define SYSLOG0(M)	syslog(LOG_ERR, M)
#define SYSLOG1(M,A)	syslog(LOG_ERR, M, A)
#define SYSLOG2(M,A,B)	syslog(LOG_ERR, M, A, B)

#if defined(__STDC__)
extern int	agent_init(int, char *);
extern void 	agent_body(int, char *);
#else	/* __STDC__ */
extern int	agent_init();
extern void	agent_body();
#endif	/* __STDC__ */

#endif /* defined(agent_inc) */
