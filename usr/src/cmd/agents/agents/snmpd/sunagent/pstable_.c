/* Copyright 1988 - 05/29/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)pstable-svr4.c	1.12 97/05/29 Sun Microsystems"
#else
static char sccsid[] = "@(#)pstable-svr4.c	1.12 97/05/29 Sun Microsystems";
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

#include "agent.h"
#include "general.h"

/* header files needed for ps code */
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ftw.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/mntent.h>
#include <sys/mnttab.h>
#include <dirent.h>
#include <sys/signal.h>
#include <sys/fault.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/procfs.h>
#include <locale.h>
#include <wctype.h>
#include <stdarg.h>
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
void wrdata();
void write_tmp_file();
static int isprocdir();

#if !defined(NO_PP)
void	get_ps_data(void);
static void	clean_ps(ps_ldata_t *);
static char	*get_usr_name(uid_t);
static ps_data_t *find_ps_data(pid_t pid);
void	pr_ps(void);

#else	/* NO_PP */

void	get_ps_data();
static void	clean_ps();
static char	*get_usr_name();
static ps_data_t *find_ps_data();
void	pr_ps();
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

#if !defined(NO_PP)
void
pr_ps(void)
#else	/* NO_PP */
void
pr_ps()
#endif	/* NO_PP */
{
ps_data_t *psp;
int lines;
printf("%d entries\n", pstable_lines);
printf("UID   PID   PPID   SZ   USR   WCHAN  TTY  CPU  CMD \n\n");
for (psp = pstable, lines = 0; lines < pstable_lines; psp++, lines++)
   {
   printf("%d     %u     %u        %d    %s     %s    %s    %d   %s\n",
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
			   (int (*)())pscomp);
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

/********************************/
/*	ps code added here	*/
/********************************/




#define	TRUE	1
#define	FALSE	0

#define NTTYS	20	/* max ttys that can be specified with the -t option  */
#define SIZ	30	/* max processes that can be specified with -p and -g */
#define ARGSIZ	30	/* size of buffer holding args for -t, -p, -u options */

#define FSTYPE_MAX	8

#ifndef MAXLOGIN
#define MAXLOGIN 8	/* max number of chars in login that will be printed */
#endif

#define UDQ	50


struct prpsinfo info;	/* process information structure from /proc */

char	*ttyname();
char	argbuf[ARGSIZ];
char	*parg;
char	*p1;			/* points to successive option arguments */
static char	stdbuf[BUFSIZ];

int	ndev;			/* number of devices */
int	maxdev;			/* number of devl structures allocated */

#define DNSIZE	14
struct devl {			/* device list	 */
	char	dname[DNSIZE];	/* device name	 */
	dev_t	dev;		/* device number */
} *devl;

char	*procdir = "/proc";	/* standard /proc directory */
int	rd_only = 0;		/* flag for remote filesystem read-only */
void	usage();		/* print usage message and quit */

extern int	errno;

#if !defined(NO_PP)
void
get_ps_data(void)
#else	/* NO_PP */
void
get_ps_data()
#endif	/* NO_PP */
{
	ps_ldata_t *ps_last = PS_LNULL;
	ps_ldata_t *ps_head = PS_LNULL;
	ps_ldata_t *psp;
	ps_data_t  *pstp;
	int  tmins, tsecs;
	char	*name;
	char	*p;
	int	c;
	int	i=0, found;
	void	getdev();
	DIR *dirp;
	struct dirent *dentp;
	char	pname[100];
	int	pdlen;
	char *gettty();

	if (pstable != PS_NULL) /* Don't run ps unless we need to */
   	{
		if ((cache_now - ps_cache_time) <= CACHE_LIFETIME)
		    return;
		free(pstable);
        }

	pstable_lines = 0;
	ps_cache_time = cache_now;
	/*
	 * Determine root path for remote machine.
	 */
	if (!readata()) {	/* get data from psfile */
		getdev();
		wrdata();
	}

	/*
	 * Determine which processes to print info about by searching
	 * the /proc directory and looking at each process.
	 */
	if ((dirp = opendir(procdir)) == NULL) {
		(void) SYSLOG0("Cannot open PROC directory\n");
		return;
	}

	(void) strcpy(pname, procdir);
	pdlen = strlen(pname);
	pname[pdlen++] = '/';

	/* for each active process --- */
	while (dentp = readdir(dirp)) {
		int	procfd;	

		if (dentp->d_name[0] == '.')		/* skip . and .. */
			continue;
		(void) strcpy(pname + pdlen, dentp->d_name);
retry:
		if ((procfd = open(pname, O_RDONLY)) == -1)
			continue;

		/*
		 * Get the info structure for the process and close quickly.
		 */
		if (ioctl(procfd, PIOCPSINFO, (char *) &info) == -1) {
			int	saverr = errno;

			(void) close(procfd);
			if (saverr == EAGAIN)
				goto retry;
			if (saverr != ENOENT)
				(void)SYSLOG2("PIOCPSINFO on %s: %s\n",
				  pname, strerror(saverr));
			continue;
		}
		close(procfd);
		if ((psp = (ps_ldata_t *)malloc(sizeof(ps_ldata_t))) == PS_LNULL)
			break;
		memset((char *)psp, 0, sizeof(ps_ldata_t));
		psp->pdata.uid = info.pr_uid;
		psp->pdata.pid = info.pr_pid;
		psp->pdata.ppid = info.pr_ppid;
		psp->pdata.sz = info.pr_size;
		if (info.pr_wchan) 
			sprintf(psp->pdata.wchan,"%9x", info.pr_wchan);
		else
			strcpy(psp->pdata.wchan, "         ");
		memset(&psp->pdata.stat[0], 0, STAT_SZ+1);
		if (info.pr_sname)
			psp->pdata.stat[0] = info.pr_sname;
		i = 0;
		strcpy(psp->pdata.tty, (char *)gettty(&i));
		psp->pdata.cpu = info.pr_time.tv_sec;		
		strcpy(psp->pdata.cmd, info.pr_fname);
		strncpy(psp->pdata.usrname, get_usr_name(psp->pdata.uid), USRNM_SZ);
		psp->pdata.usrname[USRNM_SZ] = '\0';
		pstable_lines++;
		if (ps_last == PS_LNULL)
			ps_head = psp; 
		else 
			ps_last->link = psp;  
		ps_last = psp;
	}

	(void) closedir(dirp);
	if ((pstable = (ps_data_t *)malloc(pstable_lines * sizeof(ps_data_t))) == PS_NULL) {
       
		clean_ps(ps_head);
		return;
	}
	for (pstp = pstable, psp = ps_head; psp != PS_LNULL; 
						pstp++, psp = psp->link) {
		memcpy((char *)pstp, (char *)&(psp->pdata), 
						sizeof(ps_data_t));
	}
	clean_ps(ps_head);
	qsort(pstable, pstable_lines, sizeof(ps_data_t), (int (*)())pscomp);
	return;
}

char	*psfile = "/tmp/mibiisa_ps_data";

int
readata()
{
	struct stat sbuf1, sbuf2;
	int fd;

	if ((fd = open(psfile, O_RDONLY)) == -1)
		return 0;

	if (fstat(fd, &sbuf1) < 0
	  || sbuf1.st_size == 0
	  || stat("/dev", &sbuf2) == -1
	  || sbuf1.st_mtime <= sbuf2.st_mtime
	  || sbuf1.st_mtime <= sbuf2.st_ctime
	  ) {
		if (!rd_only) {		/* if read-only, believe old data */
			(void) close(fd);
			return 0;
		}
	}

	/* Read /dev data from psfile. */
	if (read_tmp_file(fd, (char *) &ndev, sizeof(ndev)) == 0)  {
		(void) close(fd);
		return 0;
	}

	if ((devl = (struct devl *)malloc(ndev * sizeof(*devl))) == NULL) {
		SYSLOG1("malloc() for device table failed, %s\n",
		  strerror(errno));
		exit(1);
	}
	if (read_tmp_file(fd, (char *)devl, ndev * sizeof(*devl)) == 0)  {
		(void) close(fd);
		return 0;
	}

	(void) close(fd);
	return 1;
}

/*
 * getdev() uses ftw() to pass pathnames under /dev to gdev()
 * along with a status buffer.
 */
void
getdev()
{
	int	gdev();
	int	rcode;

	ndev = 0;
	rcode = ftw("/dev", gdev, 17);

	switch (rcode) {
	case 0:
		return;		/* successful return, devl populated */
	case 1:
		SYSLOG0( " ftw() encountered problem\n");
		break;
	case -1:
		SYSLOG1( " ftw() failed, %s\n", strerror(errno));
		break;
	default:
		SYSLOG1( " ftw() unexpected return, rcode=%d\n",
		 rcode);
		break;
	}
	exit(1);
}

/*
 * gdev() puts device names and ID into the devl structure for character
 * special files in /dev.  The "/dev/" string is stripped from the name
 * and if the resulting pathname exceeds DNSIZE in length then the highest
 * level directory names are stripped until the pathname is DNSIZE or less.
 */
int
gdev(objptr, statp, numb)
	char	*objptr;
	struct stat *statp;
	int	numb;
{
	register int	i;
	int	leng, start;
	static struct devl ldevl[2];
	static int	lndev, consflg;

	switch (numb) {

	case FTW_F:	
		if ((statp->st_mode & S_IFMT) == S_IFCHR) {
			/* Get more and be ready for syscon & systty. */
			while (ndev + lndev >= maxdev) {
				maxdev += UDQ;
				devl = (struct devl *) ((devl == NULL) ? 
				  malloc(sizeof(struct devl ) * maxdev) : 
				  realloc(devl, sizeof(struct devl ) * maxdev));
				if (devl == NULL) {
					SYSLOG1( " not enough memory for %d devices\n",
					    maxdev);
					exit(1);
				}
			}
			/*
			 * Save systty & syscon entries if the console
			 * entry hasn't been seen.
			 */
			if (!consflg
			  && (strcmp("/dev/systty", objptr) == 0
			    || strcmp("/dev/syscon", objptr) == 0)) {
				(void) strncpy(ldevl[lndev].dname,
				  &objptr[5], DNSIZE);
				ldevl[lndev].dev = statp->st_rdev;
				lndev++;
				return 0;
			}

			leng = strlen(objptr);
			/* Strip off /dev/ */
			if (leng < DNSIZE + 4)
				(void) strcpy(devl[ndev].dname, &objptr[5]);
			else {
				start = leng - DNSIZE - 1;

				for (i = start; i < leng && (objptr[i] != '/');
				  i++)
					;
				if (i == leng )
					(void) strncpy(devl[ndev].dname,
					  &objptr[start], DNSIZE);
				else
					(void) strncpy(devl[ndev].dname,
					  &objptr[i+1], DNSIZE);
			}
			devl[ndev].dev = statp->st_rdev;
			ndev++;
			/*
			 * Put systty & syscon entries in devl when console
			 * is found.
			 */
			if (strcmp("/dev/console", objptr) == 0) {
				consflg++;
				for (i = 0; i < lndev; i++) {
					(void) strncpy(devl[ndev].dname,
					  ldevl[i].dname, DNSIZE);
					devl[ndev].dev = ldevl[i].dev;
					ndev++;
				}
				lndev = 0;
			}
		}
		return 0;

	case FTW_D:
	case FTW_DNR:
	case FTW_NS:
		return 0;

	default:
		SYSLOG1( " gdev() error, %d, encountered\n", numb);
		return 1;
	}
}


void wrdata()
{
	char	tmpname[16];
	char *	tfname;
	int	fd;

	(void) umask(02);
	(void) strcpy(tmpname, "/tmp/mibiisa_ps.XXXXXX");
	if ((tfname = mktemp(tmpname)) == NULL || *tfname == '\0') {
		SYSLOG1( " mktemp(\"/tmp/mibiisa_ps.XXXXXX\") failed, %s\n",
		    strerror(errno));
		return;
	}

	if ((fd = open(tfname, O_WRONLY|O_CREAT|O_EXCL, 0664)) < 0) {
		SYSLOG2( " open(\"%s\") for write failed, %s\n",
		    tfname, strerror(errno));
		return;
	}

	/*
	 * Make owner root, group sys.
	 */
	(void) chown(tfname, (uid_t)0, (gid_t)3);

	/* write /dev data */
	write_tmp_file(fd, (char *) &ndev, sizeof(ndev));
	write_tmp_file(fd, (char *)devl, ndev * sizeof(*devl));

	(void) close(fd);

	if (rename(tfname, psfile) != 0) {
		SYSLOG2( " rename(\"%s\",\"%s\") failed\n",
		    tfname, psfile);
		return;
	}
}

/*
 * getarg() finds the next argument in list and copies arg into argbuf.
 * p1 first pts to arg passed back from getopt routine.  p1 is then
 * bumped to next character that is not a comma or blank -- p1 NULL
 * indicates end of list.
 */

void getarg()
{
	char	*parga;

	parga = argbuf;
	while (*p1 && *p1 != ',' && *p1 != ' ')
		*parga++ = *p1++;
	*parga = '\0';

	while (*p1 && (*p1 == ',' || *p1 == ' '))
		p1++;
}

/*
 * gettty returns the user's tty number or ? if none.
 */
char *
gettty(ip)
	register int	*ip;	/* where the search left off last time */
{
	register int	i;

	if (info.pr_ttydev != PRNODEV && *ip >= 0) {
		for (i = *ip; i < ndev; i++) {
			if (devl[i].dev == info.pr_ttydev) {
				*ip = i + 1;
				return devl[i].dname;
			}
		}
	}
	*ip = -1;
	return "?";
}



/*
 * Special read; unlinks psfile on read error.
 */
int
read_tmp_file(fd, bp, bs)
	int fd;
	char *bp;
	unsigned int bs;
{
	int rbs;

	if ((rbs = read(fd, bp, bs)) != bs) {
		SYSLOG2("read_tmp_file() error on read, rbs=%d, bs=%d\n",
		  rbs, bs);
		(void) unlink(psfile);
		return 0;
	}
	return 1;
}

/*
 * Special write; unlinks psfile on write error.
 */
void write_tmp_file(fd, bp, bs)
int	fd;
char	*bp;
unsigned	bs;
{
	int	wbs;

	if ((wbs = write(fd, bp, bs)) != bs) {
		SYSLOG2("write_tmp_file() error on write, wbs=%d, bs=%d\n",
		  wbs, bs);
		(void) unlink(psfile);
	}
}

/*
 * Returns true iff string is all numeric.
 */
int
num(s)
	register char	*s;
{
	register int c;

	if (s == NULL)
		return 0;
	c = *s;
	do {
		if (!isdigit(c))
			return 0;
	} while (c = *++s);
	return 1;
}

static char *
mntopt(p)
        char **p;
{
        char *cp = *p;
        char *retstr;

        while (*cp && isspace(*cp))
                cp++;
        retstr = cp;
        while (*cp && *cp != ',')
                cp++;
        if (*cp) {
                *cp = '\0';
                cp++;
        }
        *p = cp;
        return (retstr);
}

char *
hasmntopt(mnt, opt)
        register struct mnttab *mnt;
        register char *opt;
{
        char tmpopts[256];
        char *f, *opts=tmpopts;

        strcpy(opts, mnt->mnt_mntopts);
        f = mntopt(&opts);
        for (; *f; f = mntopt(&opts)) {
                if (strncmp(opt, f, strlen(opt)) == 0)
                        return (f - tmpopts + mnt->mnt_mntopts);
        }
        return (NULL);
}


/*
 * Return true iff dir is a /proc directory.
 *
 * This works because of the fact that "/proc/0" and "/proc/00" are the
 * same file, namely process 0, and are not linked to each other.  Ugly.
 */
static int	
isprocdir(dir)		/* return TRUE iff dir is a PROC directory */
	char	*dir;
{
	struct stat stat1;	/* dir/0  */
	struct stat stat2;	/* dir/00 */
	char	path[200];
	register char	*p;

	/*
	 * Make a copy of the directory name without trailing '/'s
	 */
	if (dir == NULL)
		(void) strcpy(path, ".");
	else {
		(void) strncpy(path, dir, (int)sizeof(path) - 4);
		path[sizeof(path)-4] = '\0';
		p = path + strlen(path);
		while (p > path && *--p == '/')
			*p = '\0';
		if (*path == '\0')
			(void) strcpy(path, ".");
	}

	/*
	 * Append "/0" to the directory path and stat() the file.
	 */
	p = path + strlen(path);
	*p++ = '/';
	*p++ = '0';
	*p = '\0';
	if (stat(path, &stat1) != 0)
		return FALSE;

	/*
	 * Append "/00" to the directory path and stat() the file.
	 */
	*p++ = '0';
	*p = '\0';
	if (stat(path, &stat2) != 0)
		return FALSE;

	/*
	 * See if we ended up with the same file.
	 */
	if (stat1.st_dev != stat2.st_dev
	  || stat1.st_ino   != stat2.st_ino
	  || stat1.st_mode  != stat2.st_mode
	  || stat1.st_nlink != stat2.st_nlink
	  || stat1.st_uid   != stat2.st_uid
	  || stat1.st_gid   != stat2.st_gid
	  || stat1.st_size  != stat2.st_size)
		return FALSE;

	/*
	 * Return TRUE iff we have a regular file with a single link.
	 */
	return (stat1.st_mode & S_IFMT) == S_IFREG && stat1.st_nlink == 1;
}




