/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)cryptio.c	1.13	97/07/07 SMI"
/*LINTLIBRARY*/

#pragma weak run_setkey = _run_setkey
#pragma weak run_crypt = _run_crypt
#pragma weak crypt_close = _crypt_close
#pragma weak makekey = _makekey

#include "synonyms.h"
#include "mtlib.h"
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <thread.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>
#include <crypt.h> 
#include "des_soft.h" 


#define	READER	0
#define	WRITER	1
#define	KSIZE 	8

/*  Global Variables  */
static pid_t	popen_pid[256];
static char key[KSIZE+1];
static struct header {
	long offset;
	unsigned int count;
};

#ifdef _REENTRANT
static mutex_t lock = DEFAULTMUTEX;
#endif _REENTRANT

static int cryptopen();
static int writekey();
static int _cp2close(), _cp2open();

void	_exit();

int
run_setkey(int p[2], const char *keyparam)
{
#ifdef _REENTRANT
	(void) mutex_lock(&lock);
#endif _REENTRANT
	if (cryptopen(p) == -1) {
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
	(void)  strncpy(key, keyparam, KSIZE);
	if (*key == 0) {
		(void) crypt_close_nolock(p);
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (0);
	}
	if (writekey(p, key) == -1) {
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
#ifdef _REENTRANT
	(void) mutex_unlock(&lock);
#endif _REENTRANT
	return (1);
}

static char cmd[] = "exec /usr/bin/crypt -p 2>/dev/null";
static int
cryptopen(int p[2])
{
	char c;

	if (_cp2open(cmd, p) < 0)
		return (-1);
	if (read(p[WRITER], &c, 1) != 1) { /* check that crypt is working on */
					    /* other end */
		(void)  crypt_close(p); /* remove defunct process */
		return (-1);
	}
	return (1);
}

static int
writekey(int p[2], char *keyarg)
{
	void (*pstat) ();
	pstat = signal(SIGPIPE, SIG_IGN); /* don't want pipe errors to cause */
					    /*  death */
	if (write(p[READER], keyarg, KSIZE) != KSIZE) {
		(void)  crypt_close(p); /* remove defunct process */
		(void)  signal(SIGPIPE, pstat);
		return (-1);
	}
	(void)  signal(SIGPIPE, pstat);
	return (1);
}


int
run_crypt(long offset, char *buffer, unsigned int count, int p[2])
{
	struct header header;
	void (*pstat) ();

#ifdef _REENTRANT
	(void) mutex_lock(&lock);
#endif _REENTRANT
	header.count = count;
	header.offset = offset;
	pstat = signal(SIGPIPE, SIG_IGN);
	if (write(p[READER], (char *)&header, sizeof (header))
		!= sizeof (header)) {
		(void) crypt_close_nolock(p);
		(void) signal(SIGPIPE, pstat);
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
	if (write(p[READER], buffer, count) < count) {
		(void) crypt_close_nolock(p);
		(void) signal(SIGPIPE, pstat);
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
	if (read(p[WRITER], buffer,  count) < count) {
		(void) crypt_close_nolock(p);
		(void) signal(SIGPIPE, pstat);
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
	(void) signal(SIGPIPE, pstat);
#ifdef _REENTRANT
	(void) mutex_unlock(&lock);
#endif _REENTRANT
	return (0);
}

makekey(int b[2])
{
	int i;
	long gorp;
	char tempbuf[KSIZE], *a, *temp;

#ifdef _REENTRANT
	(void) mutex_lock(&lock);
#endif _REENTRANT
	a = key;
	temp = tempbuf;
	for (i = 0; i < KSIZE; i++)
		temp[i] = *a++;
	gorp = getuid() + getgid();

	for (i = 0; i < 4; i++)
		temp[i] ^= (char)((gorp>>(8*i))&0377);

	if (cryptopen(b) == -1) {
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
	if (writekey(b, temp) == -1) {
#ifdef _REENTRANT
		(void) mutex_unlock(&lock);
#endif _REENTRANT
		return (-1);
	}
#ifdef _REENTRANT
	(void) mutex_unlock(&lock);
#endif _REENTRANT
	return (0);
}

int
crypt_close_nolock(int p[2])
{
	pid_t pid;
	int ret;

	if (p[0] == 0 && p[1] == 0 || p[0] < 0 || p[1] < 0) {
		ret = -1;
		goto done;
	}
	pid = popen_pid[p[0]];
	if (pid != popen_pid[p[1]]) {
		ret = -1;
		goto done;
	}
	if (!pid) {
		ret = -1;
		goto done;
	}
	(void)  kill(pid, 9);
	ret = _cp2close(p);

	done:
	return (ret);
}

int
crypt_close(int p[2])
{
#ifdef _REENTRANT
	(void) mutex_lock(&lock);
#endif _REENTRANT
	(void) crypt_close_nolock(p);
#ifdef _REENTRANT
	(void) mutex_unlock(&lock);
#endif _REENTRANT
	return (0);
}

/*
	Similar to popen(3S) but with pipe to cmd's stdin and from stdout.
*/

static int
_cp2open(char *cmd, int p[2])
/* p[2] - file descriptor array to cmd stdin and stdout */
{
	int	tocmd[2];
	int	fromcmd[2];
	pid_t	pid;

	if ((pipe(tocmd) < 0) || (pipe(fromcmd) < 0))
		return (-1);
	/* be consistent with stdio */
	if (tocmd[1] >= 256 || fromcmd[0] >= 256) {
		(void)  close(tocmd[0]);
		(void)  close(tocmd[1]);
		(void)  close(fromcmd[0]);
		(void)  close(fromcmd[1]);
		return (-1);
	}
	if ((pid = fork()) == 0) {
		(void) close(tocmd[1]);
		(void) close(0);
		(void) fcntl(tocmd[0], F_DUPFD, 0);
		(void) close(tocmd[0]);
		(void) close(fromcmd[0]);
		(void) close(1);
		(void) fcntl(fromcmd[1], F_DUPFD, 1);
		(void) close(fromcmd[1]);
		(void)  execl("/sbin/sh", "sh", "-c", cmd, 0);
		_exit(1);
	}
	if (pid == -1)
		return (-1);
	popen_pid[ tocmd[1] ] = pid;
	popen_pid[ fromcmd[0] ] = pid;
	(void) close(tocmd[0]);
	(void) close(fromcmd[1]);
	p[0] = tocmd[1];
	p[1] = fromcmd[0];
	return (0);
}

static int
_cp2close(int p[2])
{
	pid_t	r;
	int	status;
	pid_t	waitpid();
	void	(*hstat)(), (*istat)(), (*qstat)();

	pid_t pid;

	if (p[0] < 0 || p[0] >= 256 || p[1] < 0 || p[1] >= 256)
		return (-1);
	pid = popen_pid[p[0]];
	if (pid != popen_pid[p[1]])
		return (-1);
	(void) close(p[0]);
	(void) close(p[1]);
	istat = signal(SIGINT, SIG_IGN);
	qstat = signal(SIGQUIT, SIG_IGN);
	hstat = signal(SIGHUP, SIG_IGN);
	while ((r = waitpid(pid, &status, 0)) == (pid_t)-1 && errno == EINTR)
		;
	if (r == (pid_t)-1)
		status = -1;
	(void)  signal(SIGINT, istat);
	(void)  signal(SIGQUIT, qstat);
	(void)  signal(SIGHUP, hstat);
	return (status);
}
