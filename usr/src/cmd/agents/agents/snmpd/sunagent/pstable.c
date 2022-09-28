/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)pstable.c	2.15 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)pstable.c	2.15 96/07/23 Sun Microsystems";
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

#include <stdio.h>
#include <sys/types.h>
/* Now for a wonderfully stupid hack .. Sun OS 4.0 doesn't define pid_t, but */
/* Sun OS 4.1 does.							     */
#if !defined(SVR4)
#if !defined(__sys_stdtypes_h)
typedef	int		pid_t;		/* process id */
#endif	/* __sys_stdtypes_h */
#endif	/* (SVR4) */


#include  <sys/socket.h>
#include  <netinet/in.h>
#include  <net/if.h>
#include  <netinet/if_ether.h>

#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>
#include "snmpvars.h"

#include <string.h>
#include <memory.h>
#include <time.h>
#include <pwd.h>
#include <syslog.h>
#include <search.h>
#include <signal.h>

#if defined(SVR4)
#endif	/* (SVR4) */

#include "agent.h"
#include "general.h"

#define then

/**********************************************************************/
/*	PROCESS TABLE -- instance is a single component --	      */
/*			 the process ID.			      */
/*	This table is sorted by process ID.			      */
/**********************************************************************/

#define	USRNM_SZ	16
#define	WCHAN_SZ	16
#define	TTYNM_SZ	16
#define	STAT_SZ		4
#define	CMD_SZ		64

typedef	struct	ps_data_s
	{
	uid_t	uid;
	pid_t	pid;
	pid_t	ppid;
	int	sz;
	time_t	cpu;
	char	stat[STAT_SZ+1];
	char	wchan[WCHAN_SZ+1];
	char	tty[TTYNM_SZ+1];
	char	usrname[USRNM_SZ+1];
	char	cmd[CMD_SZ+1];
	} ps_data_t;


#define	PS_NULL		(ps_data_t *)0
ps_data_t *pstable = PS_NULL;
int	pstable_lines = 0;	/* # of items in memory block pointed */
				/* to by pstable.		      */

typedef struct ps_ldata_s
	{
	struct ps_ldata_s	*link;
	ps_data_t		pdata;
	} ps_ldata_t;

#define	PS_LNULL	(ps_ldata_t *)0

#define CACHE_LIFETIME 55
static time_t ps_cache_time = 0;

#if !defined(NO_PP)
static void	get_ps_data(void);
static void	clean_ps(ps_ldata_t *);
static char	*get_usr_name(uid_t);
static ps_data_t *find_ps_data(pid_t pid);
#if DEBUG
static void	pr_ps(void);
#endif	/* DEBUG */

#else	/* NO_PP */

static void	get_ps_data();
static void	clean_ps();
static char	*get_usr_name();
static ps_data_t *find_ps_data();
#if DEBUG
static void	pr_ps();
#endif	/* DEBUG */
#endif	/* NO_PP */


#if !defined(NO_PP)
static void
clean_ps(ps_ldata_t *head)
#else	/* NO_PP */
static void
clean_ps(head)
	ps_ldata_t *head;
#endif	/* NO_PP */
{
if (head != PS_LNULL)
   then {
        ps_ldata_t *pdp;
        ps_ldata_t *nxt;
	for (pdp = head; pdp != PS_LNULL; pdp = nxt)
	   {
	   nxt = pdp->link;
	   free(pdp);
	   }
	}
}

#if !defined(NO_PP)
static int pscomp(ps_data_t *i, ps_data_t *j)
#else	/* NO_PP */
static int pscomp(i, j)
	ps_data_t *i;
	ps_data_t *j;
#endif	/* NO_PP */
{
return (i->pid - j->pid);
}

#if !defined(NO_PP)
static void
get_ps_data(void)
#else	/* NO_PP */
static void
get_ps_data()
#endif	/* NO_PP */
{
FILE *ps_infile;
char inbuf[192];
char *cp;
int  tmins, tsecs;
ps_ldata_t *ps_last = PS_LNULL;
ps_ldata_t *ps_head = PS_LNULL;
ps_ldata_t *psp;
static char cantps[] = "Can not run 'ps'";

if (pstable != PS_NULL) /* Don't run ps unless we need to */
   then {
        if ((cache_now - ps_cache_time) <= CACHE_LIFETIME)
	   then return;
	free(pstable);
	}

pstable_lines = 0;
ps_cache_time = cache_now;

if ((ps_infile = popen("ps -alxw", "r")) == (FILE *)0)
   then {
	PRNTF0(cantps);
	SYSLOG0(cantps);
	return;
        }

if (fgets(inbuf, sizeof(inbuf), ps_infile) == (char *)0)
   then {
        pclose(ps_infile);
	return;
	}

for(ps_last = PS_LNULL ;;)
   {
   int scanned;

   if (fgets(inbuf, sizeof(inbuf), ps_infile) == (char *)0)
      then break;

   if ((psp = (ps_ldata_t *)malloc(sizeof(ps_ldata_t))) == PS_LNULL)
      then break;
   memset((char *)psp, 0, sizeof(ps_ldata_t));

#if defined(SVR4)
#else	/* (SVR4) */
/* This code is based on the output of Sun OS 4.x "ps -alxw" */

/*
         1         2         3         4         5         6         7
1234567890123456789012345678901234567890123456789012345678901234567890
       F UID   PID  PPID CP PRI NI  SZ  RSS WCHAN        STAT TT  TIME COMMAND
   80003   0     0     0  0 -25  0   0    0 runout       D    ?   4:58 swapper
20088000   0     1     0  0   5  0  52    0 child        IW   ?   0:00 /sbin/init -
   80003   0     2     0  0 -24  0   0    0 child        D    ?   0:00 pagedaemon
   88000   0    52     1  0   1  0  68    0 select       IW   ?   0:03 portmap
*/

   scanned = sscanf(inbuf,
/* Oh, for an ANSI C compiler!! */
#if 0
		    "%*8c"	/* Skip Flags		*/
		    "%5hd"	/* User id		*/
		    "%6u"	/* Process id		*/
		    "%6u"	/* Parent Process id	*/
		    "%*3d"	/* Skip CP		*/
		    "%*4d"	/* Skip PRI		*/
		    "%*3d"	/* Skip NI		*/
		    "%4d"	/* SZ			*/
		    "%*5d"	/* Skip RSS		*/
		    "%*1c"	/* Skip a blank		*/
		    "%8c"	/* WCHAN	        */
		    "%*5c"	/* Skip 5 blanks	*/
		    "%4c"	/* STAT		        */
		    "%*1c"	/* Skip a blank		*/
		    "%2c"	/* TTY		        */
		    "%d"	/* Time, minutes        */
		    "%*1c"	/* Skip colon		*/
		    "%d"	/* Time, seconds        */
		    "%64c",	/* Command		*/
#endif /* 0 */
		    sun_os_ver == 40 ?
		    "%*7c%5hd%6u%6u%*3d%*4d%*3d%4d%*5d%*1c%8c%*5c%4c%*1c%2c%d%*1c%d%64c" :
		    "%*8c%5hd%6u%6u%*3d%*4d%*3d%4d%*5d%*1c%8c%*5c%4c%*1c%2c%d%*1c%d%64c",
		    &(psp->pdata.uid),
		    &(psp->pdata.pid),
		    &(psp->pdata.ppid),
		    &(psp->pdata.sz),
		    psp->pdata.wchan,
		    psp->pdata.stat,
		    psp->pdata.tty,
		    &tmins, &tsecs,
		    psp->pdata.cmd
		    );

   if (scanned != 10) then break;
#endif	/* (SVR4) */

   pstable_lines++;
   if (ps_last == PS_LNULL)
      then ps_head = psp;
      else ps_last->link = psp;
   ps_last = psp;

   if ((cp = strchr(psp->pdata.wchan, ' ')) != (char *)0) *cp = '\0';
   if ((cp = strchr(psp->pdata.tty, ' ')) != (char *)0) *cp = '\0';
   psp->pdata.cmd[CMD_SZ] = '\0';
   psp->pdata.cpu = 60*tmins + tsecs;

   strncpy(psp->pdata.usrname, get_usr_name(psp->pdata.uid), USRNM_SZ);
   psp->pdata.usrname[USRNM_SZ] = '\0';
   }

pclose(ps_infile);
   {
   ps_data_t *pstp;

   if ((pstable = (ps_data_t *)malloc(pstable_lines * sizeof(ps_data_t)))
       == PS_NULL)
      then {
           clean_ps(ps_head);
           return;
	   }
   for (pstp = pstable, psp = ps_head; psp != PS_LNULL; pstp++, psp = psp->link)
      {
      memcpy((char *)pstp, (char *)&(psp->pdata), sizeof(ps_data_t));
      }
   clean_ps(ps_head);
   qsort(pstable, pstable_lines, sizeof(ps_data_t), pscomp);
   }

return;
}

/* The following code is borrowed from ps.c */
#define NUID	64

static struct ncache {
	uid_t	uid;
	char	name[USRNM_SZ+1];
} nc[NUID];

/*
 * This function assumes that the password file is hashed
 * (or some such) to allow fast access based on a uid key.
 */
#if !defined(NO_PP)
static char *
get_usr_name(uid_t uid)
#else	/* NO_PP */
static char *
get_usr_name(uid)
	uid_t uid;
#endif	/* NO_PP */
{
struct passwd *pw;
int cp;

#if	(((NUID) & ((NUID) - 1)) != 0)
cp = uid % (NUID);
#else
cp = uid & ((NUID) - 1);
#endif
if (uid >= 0 && nc[cp].uid == uid && nc[cp].name[0])
   return (nc[cp].name);
pw = getpwuid(uid);
if (!pw) return ((char *)0);
nc[cp].uid = uid;
strncpy(nc[cp].name, pw->pw_name, USRNM_SZ);
return (nc[cp].name);
}

#if DEBUG
#if !defined(NO_PP)
static void
pr_ps(void)
#else	/* NO_PP */
static void
pr_ps()
#endif	/* NO_PP */
{
ps_data_t *psp;
int lines;
printf("%d entries\n", pstable_lines);
for (psp = pstable, lines = 0; lines < pstable_lines; psp++, lines++)
   {
   printf("UID %d  PID %u  PPID %u  SZ %d  USR %s  WCHAN %s$ TTY %s$ CPU %d CMD %s$\n",
	  psp->uid,
	  psp->pid,
	  psp->ppid,
	  psp->sz,
	  psp->usrname,
	  psp->wchan,
	  psp->tty,
	  psp->cpu,
	  psp->cmd
	  );
   }
}
#endif /* DEBUG */

/**********************************************************************/
/*  Locate a particular PID.					      */
/*  Return a pointer to the entry or NULL if not found.		      */
/**********************************************************************/
#if !defined(NO_PP)
static ps_data_t *
find_ps_data(pid_t pid)
#else	/* NO_PP */
static ps_data_t *
find_ps_data(pid)
	pid_t pid;
#endif	/* NO_PP */
{
ps_data_t *psp;
ps_data_t key;

key.pid = pid;

/**** Should add a cache here ***/

psp = (ps_data_t *)bsearch((char *)&key, (char *)pstable,
			   pstable_lines, sizeof(ps_data_t),
			   pscomp);
return psp;
}

/****************************************************************************
NAME:  pstable_test

PURPOSE:  Test whether a given object exists in the proces table

RETURNS:  int		0 if the attribute is accessable.
			-1 if the attribute is not accessable.
****************************************************************************/
/*ARGSUSED*/
int
pstable_test(form, last_match, compc, compl, cookie)
	int	form;	/* See TEST_SET & TEST_GET in mib.h	*/
	OIDC_T	last_match; /* Last component matched */
	int	compc;
	OIDC_T	*compl;
	char	*cookie;
{

/* There must be exactly 1 unused component */
if (compc != 1) return -1;

get_ps_data();

if (find_ps_data((pid_t)compl[0]) != PS_NULL)
   then return 0;
   else return -1;
}

/****************************************************************************
NAME:  pstable_next

PURPOSE:  Locate the "next" object in the process table

RETURNS:  int		>0 indicates how many object identifiers have
			been placed in the result list.
			0 if there is no "next" in the table
****************************************************************************/
/*ARGSUSED*/
int
pstable_next(last_match, tcount, tlist, rlist, cookie)
	OIDC_T	last_match; /* Last component matched */
	int	tcount;
	OIDC_T	*tlist;
	OIDC_T	*rlist;
	char	*cookie;
{
ps_data_t *psp;
OIDC_T maxpid;

get_ps_data();

if (tcount == 0)
  {
  if (pstable_lines != 0)
     then {
          rlist[0] = (OIDC_T)pstable[0].pid;
	  return 1;
	  }
  return 0;
  }

maxpid = (OIDC_T)pstable[pstable_lines-1].pid;

if (tlist[0] < maxpid)
   {
  if ((psp = find_ps_data((pid_t)tlist[0])) != PS_NULL)
     then {
	  psp++;
          rlist[0] = (OIDC_T)(psp->pid);
	  return 1;
	  }
     else {
          int lines;
	  /* We *know* that tlist[0] is < maxpid, i.e. pstable[last entry] */
          for (lines = 0, psp = pstable; lines < pstable_lines; lines++, psp++)
	     {
	     if (tlist[0] < (OIDC_T)(psp->pid))
	        then { /* Hit first pid greater than target */
		     rlist[0] = (OIDC_T)(psp->pid);
		     return 1;
		     }
	     }
	  }
   }
/* Pass on it */
return 0;
}

/**********************************************************************/
/*	ACCESS FUNCTIONS					      */
/**********************************************************************/
#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_psProcessID(OIDC_T lastmatch,
	      int compc,
	      OIDC_T *compl,
	      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_psProcessID(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
return (INT_32_T)(compl[0]);
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_psParentProcessID(OIDC_T lastmatch,
	      int compc,
	      OIDC_T *compl,
	      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_psParentProcessID(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        return (INT_32_T)(psp->ppid);
        }
return (INT_32_T)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_psProcessUserID(OIDC_T lastmatch,
	      int compc,
	      OIDC_T *compl,
	      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_psProcessUserID(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        return (INT_32_T)(psp->uid);
        }
return (INT_32_T)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_psProcessSize(OIDC_T lastmatch,
	      int compc,
	      OIDC_T *compl,
	      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_psProcessSize(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        return (INT_32_T)(psp->sz);
        }
return (INT_32_T)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
INT_32_T
get_psProcessCpuTime(OIDC_T lastmatch,
	      int compc,
	      OIDC_T *compl,
	      char *cookie)
#else   /* NO_PP */
/*ARGSUSED*/
INT_32_T
  get_psProcessCpuTime(lastmatch, compc, compl, cookie)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        return (INT_32_T)(psp->cpu);
        }
return (INT_32_T)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_psProcessState(OIDC_T lastmatch,
		int compc,
		OIDC_T *compl,
		char *cookie,
		int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_psProcessState(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        *lengthp = strlen(psp->stat);
	return (unsigned char *)(psp->stat);
        }
*lengthp = 0;
return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_psProcessWaitChannel(OIDC_T lastmatch,
		int compc,
		OIDC_T *compl,
		char *cookie,
		int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_psProcessWaitChannel(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        *lengthp = strlen(psp->wchan);
	return (unsigned char *)(psp->wchan);
        }
*lengthp = 0;
return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_psProcessTTY(OIDC_T lastmatch,
		int compc,
		OIDC_T *compl,
		char *cookie,
		int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_psProcessTTY(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        *lengthp = strlen(psp->tty);
	return (unsigned char *)(psp->tty);
        }
*lengthp = 0;
return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_psProcessUserName(OIDC_T lastmatch,
		int compc,
		OIDC_T *compl,
		char *cookie,
		int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_psProcessUserName(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        *lengthp = strlen(psp->usrname);
	return (unsigned char *)(psp->usrname);
        }
*lengthp = 0;
return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
unsigned char *
get_psProcessName(OIDC_T lastmatch,
		int compc,
		OIDC_T *compl,
		char *cookie,
		int *lengthp)
#else   /* NO_PP */
/*ARGSUSED*/
unsigned char *
  get_psProcessName(lastmatch, compc, compl, cookie, lengthp)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	int *lengthp;
#endif  /* NO_PP */
{
ps_data_t *psp;

if ((psp = find_ps_data((pid_t)compl[0])) != PS_NULL)
   then {
        *lengthp = strlen(psp->cmd);
	return (unsigned char *)(psp->cmd);
        }
*lengthp = 0;
return (unsigned char *)0;
}

#if !defined(NO_PP)
/*ARGSUSED*/
void
set_psProcessStatus(OIDC_T lastmatch, int compc,
		    OIDC_T *compl,
		    char *cookie, INT_32_T value)
#else   /* NO_PP */
/*ARGSUSED*/
void
set_psProcessStatus(lastmatch, compc, compl, cookie, value)
	OIDC_T lastmatch;
	int compc;
	OIDC_T *compl;
	char *cookie;
	INT_32_T value;
#endif  /* NO_PP */
{
if (value != 0)
   then {
        (void) kill((pid_t)compl[0], (int)value);
	}
}
