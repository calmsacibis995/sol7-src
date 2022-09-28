/*-
 * Copyright (c) 1990, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1994 - 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#pragma ident	"@(#)mail.local.c	1.16	98/01/30 SMI"

#ifndef lint
static char sccsid[] = "@(#)mail.local.c	8.6 (Berkeley) 4/8/94";
static char sccsi2[] = "@(#)mail.local.c	1.16 (Sun) 01/30/98";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h> 
#include <maillock.h>
#include <grp.h>

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include <syslog.h>
#include <pathnames.h>

int eval = EX_OK;			/* sysexits.h error value. */

#define _PATH_MAILDIR	"/var/mail"
#define _PATH_LOCTMP	"/tmp/local.XXXXXX"
#define _PATH_LOCHTMP	"/tmp/lochd.XXXXXX"
#define FALSE 0
#define TRUE  1
#define MAXLINE 2048

void            deliver (int, int, char *, int);
void            e_to_sys (int);
void            err ();
void            notifybiff (char *);
void            store (char *);
void            usage (void);
void            vwarn ();
void            warn ();
int             lock_inbox();
void            unlock_inbox();

char unix_from_line[MAXLINE];
int ulen;
int content_length;
int bfd, hfd; /* temp file */


int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct passwd *pw;
	uid_t src_uid, targ_uid;
	int ch, fd;
	uid_t uid;
	char *from;
	struct  group *grpptr;
	uid_t saved_uid;
	int  target_file_directly = FALSE;

	openlog("mail.local", 0, LOG_MAIL);

	from = NULL;
	pw = NULL;

	while ((ch = getopt(argc, argv, "dFf:r:")) != EOF)
		switch(ch) {
		case 'd':		/* Backward compatible. */
			break;
		case 'F':
			target_file_directly = TRUE;
			break;
		case 'f':
		case 'r':		/* Backward compatible. */
			if (from != NULL) {
				warn("multiple -f options");
				usage();
			}
			from = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (!*argv)
		usage();

	/*
	 * If from not specified, use the name from getlogin() if the
	 * uid matches, otherwise, use the name from the password file
	 * corresponding to the uid.
	 */
	uid = getuid();
	if (!from && (!(from = getlogin()) ||
	    !(pw = getpwnam(from)) || pw->pw_uid != uid))
		from = (pw = getpwuid(uid)) ? pw->pw_name : "???";
	src_uid = pw ? pw->pw_uid : uid;

	/*
	 * There is no way to distinguish the error status of one delivery
	 * from the rest of the deliveries.  So, if we failed hard on one
	 * or more deliveries, but had no failures on any of the others, we
	 * return a hard failure.  If we failed temporarily on one or more
	 * deliveries, we return a temporary failure regardless of the other
	 * failures.  This results in the delivery being reattempted later
	 * at the expense of repeated failures and multiple deliveries.
	 */

	/* We expect sendmail will invoke us with saved id 0 */
	/* we then do setgid and setuid defore delivery      */
	/* setgid to mail group */
	if ((grpptr = getgrnam("mail")) != NULL) {
		setgid(grpptr->gr_gid);
        }
	saved_uid = geteuid();
	if ((saved_uid != 0) && target_file_directly) {
		/*
		 * Require root privilege for -F option.  This prevents
		 * all sorts of potential nasty security attacks.
		 */
		warn("only super-user can use -F option");
		exit(EX_CANTCREAT);
	}
	for (store(from); *argv; ++argv) {
		if (target_file_directly)
			deliver(hfd, bfd, *argv, TRUE);
		else {
			/*
			 * Disallow delivery to unknown names -- special
			 * mailboxes can be handled in the sendmail
			 * aliases file.
			 */
			if (!(pw = getpwnam(*argv))) {
				warn("user: %s look up failed,"
					" name services outage ?", *argv);
				exit(EX_TEMPFAIL);
			}
			/* mailbox may be NFS mounted, seteuid to user */
			targ_uid = pw->pw_uid;
			seteuid(targ_uid);

			if ((saved_uid != 0) && (src_uid != targ_uid)) {
				/*
				 * If saved_uid == 0 (root), anything is OK;
				 * this is the way it should be.  But to prevent
				 * a random user from calling "mail.local foo"
				 * in an attempt to hijack foo's mail-box,
				 * make sure src_uid == targ_uid otherwise.
				 */
				warn("%s: wrong owner (is %d, should be %d)",
					*argv, src_uid, targ_uid);
				eval = EX_CANTCREAT;
			} else
				deliver(hfd, bfd, *argv, FALSE);
			seteuid(saved_uid); 
		}
	}
	exit(eval);
}

void
store(from)
	char *from;
{
	FILE *fp;
	time_t tval;
	char *tn, line[MAXLINE];
	FILE *bfp, *hfp;
	char *btn, *htn;
	int in_header_section;
	uid_t uid;

	btn = strdup(_PATH_LOCTMP);
	if ((bfd = mkstemp(btn)) == -1 || (bfp = fdopen(bfd, "w+")) == NULL) {
		e_to_sys(errno);
		err("unable to open temporary file");
	}
	(void)unlink(btn);
	free(btn);

	htn = strdup(_PATH_LOCHTMP);
	if ((hfd = mkstemp(htn)) == -1 || (hfp = fdopen(hfd, "w+")) == NULL) {
		e_to_sys(errno);
		err("unable to open temporary file");
	}
	(void)unlink(htn);
	free(htn);

	in_header_section = TRUE;
	content_length = 0;
	fp = hfp;

	line[0] = '\0';
	for (; fgets(line, sizeof(line), stdin);) {
		if (line[0] == '\n') {
			if (in_header_section) {
				in_header_section = FALSE;
				if (fflush(fp) == EOF || ferror(fp)) {
					e_to_sys(errno);
					err("temporary file write error");
				}
				fp = bfp;
				continue;
			}
		} 
		if (in_header_section) {
			if (strncasecmp("Content-Length:", line, 15) == 0) {
				continue; /* skip this header */
			}
		} else 	content_length += strlen(line); 
		(void)fprintf(fp, "%s", line);
		if (ferror(fp)) {
			e_to_sys(errno);
			err("temporary file write error");
		}
	}

	/* If message not newline terminated, need an extra. */
	if (!strchr(line, '\n')) {
		(void)putc('\n', fp);
		content_length++;
	}
	/* Output a newline; note, empty messages are allowed. */
	(void)putc('\n', fp);

	if (fflush(fp) == EOF || ferror(fp)) {
		e_to_sys(errno);
		err("temporary file write error");
	}

	(void)time(&tval);
	(void)sprintf(unix_from_line, "From %s %s", from, ctime(&tval));
	ulen = strlen(unix_from_line);
}

void
deliver(hfd, bfd, name, name_directly)
	int bfd;
	int hfd;
	char *name;
	int name_directly;
{
	struct stat fsb, sb;
	int mbfd, nr, nw, off;
	char biffmsg[100], buf[8*1024], path[MAXPATHLEN];
	off_t curoff;
	int len;

	path[0] = '\0';
	if (name_directly) {
		(void)strcpy(path, name);
		/*
		 * For special case /dev/null we can skip everything.
		 */
		if (strcmp(path, "/dev/null") == 0)
			return;
	} else
		(void)sprintf(path, "%s/%s", _PATH_MAILDIR, name);

	/*
	 * If the mailbox is linked or a symlink, fail.  There's an obvious
	 * race here, that the file was replaced with a symbolic link after
	 * the lstat returned, but before the open.  We attempt to detect
	 * this by comparing the original stat information and information
	 * returned by an fstat of the file descriptor returned by the open.
	 *
	 * NB: this is a symptom of a larger problem, that the mail spooling
	 * directory is writeable by the wrong users.  If that directory is
	 * writeable, system security is compromised for other reasons, and
	 * it cannot be fixed here.
	 *
	 * If we created the mailbox, set the owner/group.  If that fails,
	 * just return.  Another process may have already opened it, so we
	 * can't unlink it.  Historically, binmail set the owner/group at
	 * each mail delivery.  We no longer do this, assuming that if the
	 * ownership or permissions were changed there was a reason.
	 *
	 * XXX
	 * open(2) should support flock'ing the file.
	 */
tryagain:
	/* bug fix for 1203379 */
	/* should check lock status, but... maillock return no value */
	if (name_directly)
		lock_inbox(path, 10);
	else
		maillock(name, 10);

	if (lstat(path, &sb)) {
		mbfd = open(path,
		    O_APPEND|O_CREAT|O_EXCL|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		if (mbfd != -1)
			fchmod(mbfd, 0660);


		if (mbfd == -1) {
			if (errno == EEXIST) {
				if (name_directly)
					unlock_inbox(path);
				else
					mailunlock();
				goto tryagain;
			}
		} 
	} else if (sb.st_nlink != 1 || S_ISLNK(sb.st_mode)) {
		e_to_sys(errno);
		warn("%s: linked file", path);
		if (name_directly)
			unlock_inbox(path);
		else
			mailunlock();
		return;
	} else {
		mbfd = open(path, O_APPEND|O_WRONLY, 0);
		if (mbfd != -1 &&
		    (fstat(mbfd, &fsb) || fsb.st_nlink != 1 ||
		    S_ISLNK(fsb.st_mode) || sb.st_dev != fsb.st_dev ||
		    sb.st_ino != fsb.st_ino)) {
			warn("%s: file changed after open", path);
			goto err1;
		}
	}

	if (mbfd == -1) {
		e_to_sys(errno);
		warn("%s: %s", path, strerror(errno));
		if (name_directly)
			unlock_inbox(path);
		else
			mailunlock();
		return;
	}

	/* Get the starting offset of the new message for biff. */
	curoff = lseek(mbfd, (off_t)0, SEEK_END);
	(void)sprintf(biffmsg, "%s@%ld\n", name, curoff);

	/* Copy the message into the file. */
	if (lseek(hfd, (off_t)0, SEEK_SET) == (off_t)-1) {
		e_to_sys(errno);
		warn("temporary file: %s", strerror(errno));
		goto err1;
	}
	/* Copy the message into the file. */
	if (lseek(bfd, (off_t)0, SEEK_SET) == (off_t)-1) {
		e_to_sys(errno);
		warn("temporary file: %s", strerror(errno));
		goto err1;
	}
	if ((write(mbfd, unix_from_line, ulen)) != ulen) {
		e_to_sys(errno);
		warn("%s: %s", path, strerror(errno));
		goto err2;;
	}
	while ((nr = read(hfd, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(mbfd, buf + off, nr)) < 0) {
				e_to_sys(errno);
				warn("%s: %s", path, strerror(errno));
				goto err2;;
			}
	if (nr < 0) {
		e_to_sys(errno);
		warn("temporary file: %s", strerror(errno));
		goto err2;;
	}
	sprintf(buf, "Content-Length: %d\n\n", content_length);
	len = strlen(buf);
	if (write(mbfd, buf, len) != len) {
		e_to_sys(errno);
		warn("%s: %s", path, strerror(errno));
		goto err2;;
	}
	while ((nr = read(bfd, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr; nr -= nw, off += nw)
			if ((nw = write(mbfd, buf + off, nr)) < 0) {
				e_to_sys(errno);
				warn("%s: %s", path, strerror(errno));
				goto err2;;
			}
	if (nr < 0) {
		e_to_sys(errno);
		warn("temporary file: %s", strerror(errno));
		goto err2;;
	}

	/* Flush to disk, don't wait for update. */
	if (fsync(mbfd)) {
		e_to_sys(errno);
		warn("%s: %s", path, strerror(errno));
err2:		(void)ftruncate(mbfd, curoff);
err1:		(void)close(mbfd);
		if (name_directly)
			unlock_inbox(path);
		else
			mailunlock();
		return;
	}
		
	/* Close and check -- NFS doesn't write until the close. */
	if (close(mbfd)) {
		e_to_sys(errno);
		warn("%s: %s", path, strerror(errno));
		mailunlock();
		return;
	}

	notifybiff(biffmsg);
	if (name_directly)
		unlock_inbox(path);
	else
		mailunlock();
}

void
notifybiff(msg)
	char *msg;
{
	static struct sockaddr_in addr;
	static int f = -1;
	struct hostent *hp;
	struct servent *sp;
	int len;

	if (!addr.sin_family) {
		/* Be silent if biff service not available. */
		if (!(sp = getservbyname("biff", "udp")))
			return;
		if (!(hp = gethostbyname("localhost"))) {
			warn("localhost: %s", strerror(errno));
			return;
		}
		addr.sin_family = hp->h_addrtype;
		memmove(&addr.sin_addr, hp->h_addr, hp->h_length);
		addr.sin_port = sp->s_port;
	}
	if (f < 0 && (f = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		warn("socket: %s", strerror(errno));
		return;
	}
	len = strlen(msg) + 1;
	if (sendto(f, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr))
	    != len)
		warn("sendto biff: %s", strerror(errno));
}

void
usage()
{
	eval = EX_USAGE;
	err("usage: mail.local [-f from] user ...");
}

#if __STDC__
/*VARARGS1*/
void
err(const char *fmt, ...)
#else
void
err(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarn(fmt, ap);
	va_end(ap);

	exit(eval);
}

void
#if __STDC__
warn(const char *fmt, ...)
#else
warn(fmt, va_alist)
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vwarn(fmt, ap);
	va_end(ap);
}

void
vwarn(fmt, ap)
	const char *fmt;
	va_list ap;
{
	/*
	 * Log the message to stderr.
	 *
	 * Don't use LOG_PERROR as an openlog() flag to do this,
	 * it's not portable enough.
	 */
	if (eval != EX_USAGE)
		(void)fprintf(stderr, "mail.local: ");
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");

	/* Log the message to syslog. */
	vsyslog(LOG_ERR, fmt, ap);
}

/*
 * e_to_sys --
 *	Guess which errno's are temporary.  Gag me.
 */
void
e_to_sys(num)
	int num;
{
	/* Temporary failures override hard errors. */
	if (eval == EX_TEMPFAIL)
		return;

	switch(num) {		/* Hopefully temporary errors. */
#ifdef EAGAIN
	case EAGAIN:		/* Resource temporarily unavailable */
#endif
#ifdef EDQUOT
	case EDQUOT:		/* Disc quota exceeded */
#endif
#ifdef EBUSY
	case EBUSY:		/* Device busy */
#endif
#ifdef EPROCLIM
	case EPROCLIM:		/* Too many processes */
#endif
#ifdef EUSERS
	case EUSERS:		/* Too many users */
#endif
#ifdef ECONNABORTED
	case ECONNABORTED:	/* Software caused connection abort */
#endif
#ifdef ECONNREFUSED
	case ECONNREFUSED:	/* Connection refused */
#endif
#ifdef ECONNRESET
	case ECONNRESET:	/* Connection reset by peer */
#endif
#ifdef EDEADLK
	case EDEADLK:		/* Resource deadlock avoided */
#endif
#ifdef EFBIG
	case EFBIG:		/* File too large */
#endif
#ifdef EHOSTDOWN
	case EHOSTDOWN:		/* Host is down */
#endif
#ifdef EHOSTUNREACH
	case EHOSTUNREACH:	/* No route to host */
#endif
#ifdef EMFILE
	case EMFILE:		/* Too many open files */
#endif
#ifdef ENETDOWN
	case ENETDOWN:		/* Network is down */
#endif
#ifdef ENETRESET
	case ENETRESET:		/* Network dropped connection on reset */
#endif
#ifdef ENETUNREACH
	case ENETUNREACH:	/* Network is unreachable */
#endif
#ifdef ENFILE
	case ENFILE:		/* Too many open files in system */
#endif
#ifdef ENOBUFS
	case ENOBUFS:		/* No buffer space available */
#endif
#ifdef ENOMEM
	case ENOMEM:		/* Cannot allocate memory */
#endif
#ifdef ENOSPC
	case ENOSPC:		/* No space left on device */
#endif
#ifdef EROFS
	case EROFS:		/* Read-only file system */
#endif
#ifdef ESTALE
	case ESTALE:		/* Stale NFS file handle */
#endif
#ifdef ETIMEDOUT
	case ETIMEDOUT:		/* Connection timed out */
#endif
#if defined(EWOULDBLOCK) && (EWOULDBLOCK != EAGAIN)
	case EWOULDBLOCK:	/* Operation would block. */
#endif
		eval = EX_TEMPFAIL;
		break;
	default:
		eval = EX_UNAVAILABLE;
		break;
	}
}


#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/file.h>

static	char	*lockext = ".lock";	/* Lock suffix for mailname */
static	char	curlock[PATHSIZE];	/* Last used name of lock */
static	int	locked;			/* To note that we locked it */
static	time_t	locktime;		/* time lock file was touched */
static int lock1();

/*
 * Lock the specified mail file by setting the file mailfile.lock.
 * We must, of course, be careful to remove the lock file by a call
 * to unlock before we stop.  The algorithm used here is to see if
 * the lock exists, and if it does, to check its modify time.  If it
 * is older than 5 minutes, we assume error and set our own file.
 * Otherwise, we wait for 5 seconds and try again.
 */

/*ARGSUSED*/
int
lock_inbox(path, retrycnt)
char *path;
int retrycnt;
{
	register time_t t;
	struct stat sbuf;
	int statfailed;
	char locktmp[PATHSIZE];	/* Usable lock temporary */
	char file[PATHSIZE];

	if (locked)
		return (0);
	strcpy(file, path);
	strcpy(curlock, file);
	strcat(curlock, lockext);
	strcpy(locktmp, file);
	strcat(locktmp, "XXXXXX");
	mktemp(locktmp);
	remove(locktmp);
	statfailed = 0;
	for (;;) {
		t = lock1(locktmp, curlock);
		if (t == 0) {
			locked = 1;
			locktime = time(0);
			return (0);
		}
		if (stat(curlock, &sbuf) < 0) {
			if (statfailed++ > 5)
				return (-1);
			sleep(5);
			continue;
		}
		statfailed = 0;

		/*
		 * Compare the time of the temp file with the time
		 * of the lock file, rather than with the current
		 * time of day, since the files may reside on
		 * another machine whose time of day differs from
		 * ours.  If the lock file is less than 5 minutes
		 * old, keep trying.
		 */
		if (t < sbuf.st_ctime + 300) {
			sleep(5);
			continue;
		}
		remove(curlock);
	}
}

/*
 * Remove the mail lock, and note that we no longer
 * have it locked.
 */
void
unlock_inbox()
{
	remove(curlock);
	locked = 0;
}

/*
 * Attempt to set the lock by creating the temporary file,
 * then doing a link/unlink.  If it succeeds, return 0,
 * else return a guess of the current time on the machine
 * holding the file.
 */
static int
lock1(tempfile, name)
	char tempfile[], name[];
{
	register int fd;
	struct stat sbuf;

	fd = open(tempfile, O_RDWR|O_CREAT|O_EXCL, 0600);

	if (fd < 0)
		return (time(0));
	fstat(fd, &sbuf);
	/*
	 * Write the string "0" into the lock file to give us some
	 * interoperability with SVR4 mailers.  SVR4 mailers expect
	 * a process ID to be written into the lock file and then
	 * use kill() to see if the process is alive or not.  We write
	 * 0 into it so that SVR4 mailers will always think our lock file
	 * is valid.
	 */
	write(fd, "0", 2);
	close(fd);
	if (link(tempfile, name) < 0) {
		remove(tempfile);
		return (sbuf.st_ctime);
	}
	remove(tempfile);
	return (0);
}
