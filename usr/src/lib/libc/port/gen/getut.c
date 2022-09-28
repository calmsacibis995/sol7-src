/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getut.c	1.31	97/08/29	SMI"

/*LINTLIBRARY*/

/*
 * Routines to read and write the /etc/utmp file.
 */

#pragma weak endutent = _endutent
#pragma weak getutent = _getutent
#pragma weak getutid = _getutid
#pragma weak getutline = _getutline
#pragma weak getutmp = _getutmp
#pragma weak getutmpx = _getutmpx
#pragma weak makeut = _makeut
#pragma weak modut = _modut
#pragma weak pututline = _pututline
#pragma weak setutent = _setutent
#pragma weak updutfile = _updutfile
#pragma weak updutxfile = _updutxfile
#pragma weak updutmpx = _updutmpx
#pragma weak updwtmp = _updwtmp
#pragma weak utmpname = _utmpname

#include "synonyms.h"
#include "shlib.h"
#include <stdio.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utmpx.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <utime.h>
#include <sys/wait.h>

#define	IDLEN	4	/* length of id field in utmp */
#define	SC_WILDC	0xff	/* wild char for utmp ids */
#define	MAXFILE	79	/* Maximum pathname length for "utmp" file */
#define	MAXVAL	255	/* max value for an id 'character' */
#define	IPIPE	"/etc/initpipe"	/* FIFO to send pids to init */
#define	UPIPE	"/etc/utmppipe"	/* FIFO to send pids to utmpd */

/*
 * format of message sent to init
 */

typedef struct	pidrec {
	int	pd_type;	/* command type */
	pid_t	pd_pid;		/* pid */
} pidrec_t;

/*
 * pd_type's
 */

#define	ADDPID 1	/* add a pid to "godchild" list */
#define	REMPID 2	/* remove a pid to "godchild" list */

#ifdef ut_time
#undef ut_time
#endif

#ifdef	DEBUG
#undef	UTMP_FILE
#define	UTMP_FILE "utmp"
#undef	UTMPX_FILE
#define	UTMPX_FILE "utmpx"
#endif

#define	VAR_UTMP_FILE "/var/adm/utmp"

static void	utmp_frec2api(const struct futmp *, struct utmp *);
static void	utmp_api2frec(const struct utmp *, struct futmp *);

static void	unlockut(void);
static void	sendpid(int, pid_t);
static void	sendupid(int, pid_t);
static int	idcmp(const char *, const char *);
static int	allocid(char *, unsigned char *);
static int	lockut(void);
static int	updutmpx(const struct utmp *entry);

static void	getutmp_frec(const struct futmpx *, struct futmp *);
static void	getutmpx_frec(const struct futmp *, struct futmpx *);

static int fd = -1;	/* File descriptor for the utmp file. */
static int fd_u = -1;	/* File descriptor for the utmpx file. */
static const char *utmpfile = UTMP_FILE;	/* Name of the current */
static const char *utmpxfile = UTMPX_FILE;	/* "utmp" like file.   */

#ifdef ERRDEBUG
static long loc_utmp;	/* Where in "utmp" the current "ubuf" was found. */
#endif

static struct futmp fubuf;	/* Copy of last entry read in. */
static struct utmp ubuf;	/* Last entry returned to client */

static int changed_name = 0;	/* Flag set when the name is changed */

/*
 * In the 64-bit world, the utmp data structure grows because of
 * the ut_time field (a time_t) at the end of it.
 */
static void
utmp_frec2api(const struct futmp *src, struct utmp *dst)
{
	if (src == NULL)
		return;

	(void) strncpy(dst->ut_user, src->ut_user, sizeof (dst->ut_user));
	(void) strncpy(dst->ut_line, src->ut_line, sizeof (dst->ut_line));
	(void) memcpy(dst->ut_id, src->ut_id, sizeof (dst->ut_id));
	dst->ut_pid = src->ut_pid;
	dst->ut_type = src->ut_type;
	dst->ut_exit.e_termination = src->ut_exit.e_termination;
	dst->ut_exit.e_exit = src->ut_exit.e_exit;
	dst->ut_time = (time_t)src->ut_time;
}

static void
utmp_api2frec(const struct utmp *src, struct futmp *dst)
{
	if (src == NULL)
		return;

	(void) strncpy(dst->ut_user, src->ut_user, sizeof (dst->ut_user));
	(void) strncpy(dst->ut_line, src->ut_line, sizeof (dst->ut_line));
	(void) memcpy(dst->ut_id, src->ut_id, sizeof (dst->ut_id));
	dst->ut_pid = src->ut_pid;
	dst->ut_type = src->ut_type;
	dst->ut_exit.e_termination = src->ut_exit.e_termination;
	dst->ut_exit.e_exit = src->ut_exit.e_exit;
	dst->ut_time = (time32_t)src->ut_time;
}

/*
 * "getutent_frec" gets the raw version of the next entry in the utmp file.
 */
static struct futmp *
getutent_frec(void)
{
	int fdx;

	/*
	 * If the "utmp" file is not open, attempt to open it for
	 * reading.  If there is no file, attempt to create one.  If
	 * both attempts fail, return NULL.  If the file exists, but
	 * isn't readable and writeable, do not attempt to create.
	 */
	if (fd < 0) {
		if ((fd = open(utmpfile, O_RDWR|O_CREAT, 0644)) < 0) {

			/*
			 * If the open failed for permissions, try opening
			 * it only for reading.  All "pututline()" later
			 * will fail the writes.
			 */
			if ((fd = open(utmpfile, O_RDONLY)) < 0)
				return (NULL);
		}
		if (access(utmpxfile, F_OK) < 0) {
			if ((fdx = open(utmpxfile, O_CREAT|O_RDWR, 0644)) < 0)
				return (NULL);
			(void) close(fdx);
		}
	}

	/* Try to read in the next entry from the utmp file.  */

	if (read(fd, &fubuf, sizeof (fubuf)) != sizeof (fubuf)) {
		bzero(&fubuf, sizeof (fubuf));
		return (NULL);
	}

	/* Save the location in the file where this entry was found. */

	(void) lseek(fd, 0L, 1);
	return (&fubuf);
}

/*
 * "getutent" gets the next entry in the utmp file.
 */
struct utmp *
getutent(void)
{
	struct futmp *futp;

	futp = getutent_frec();
	utmp_frec2api(&fubuf, &ubuf);
	if (futp == NULL)
		return (NULL);
	return (&ubuf);
}

/*
 * "getutid" finds the specified entry in the utmp file.  If
 * it can't find it, it returns NULL.
 */
struct utmp *
getutid(const struct utmp *entry)
{
	short type;

	utmp_api2frec(&ubuf, &fubuf);

	/*
	 * Start looking for entry.  Look in our current buffer before
	 * reading in new entries.
	 */
	do {
		/*
		 * If there is no entry in "ubuf", skip to the read.
		 */
		if (fubuf.ut_type != EMPTY) {
			switch (entry->ut_type) {

			/*
			 * Do not look for an entry if the user sent
			 * us an EMPTY entry.
			 */
			case EMPTY:
				return (NULL);

			/*
			 * For RUN_LVL, BOOT_TIME, OLD_TIME, and NEW_TIME
			 * entries, only the types have to match.  If they
			 * do, return the address of internal buffer.
			 */
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == fubuf.ut_type) {
					utmp_frec2api(&fubuf, &ubuf);
					return (&ubuf);
				}
				break;

			/*
			 * For INIT_PROCESS, LOGIN_PROCESS, USER_PROCESS,
			 * and DEAD_PROCESS the type of the entry in "fubuf",
			 * must be one of the above and id's must match.
			 */
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				if (((type = fubuf.ut_type) == INIT_PROCESS ||
				    type == LOGIN_PROCESS ||
				    type == USER_PROCESS ||
				    type == DEAD_PROCESS) &&
				    fubuf.ut_id[0] == entry->ut_id[0] &&
				    fubuf.ut_id[1] == entry->ut_id[1] &&
				    fubuf.ut_id[2] == entry->ut_id[2] &&
				    fubuf.ut_id[3] == entry->ut_id[3]) {
					utmp_frec2api(&fubuf, &ubuf);
					return (&ubuf);
				}
				break;

			/* Do not search for illegal types of entry. */
			default:
				return (NULL);
			}
		}
	} while (getutent_frec() != NULL);

	/* the proper entry wasn't found. */

	utmp_frec2api(&fubuf, &ubuf);
	return (NULL);
}

/*
 * "getutline" searches the "utmp" file for a LOGIN_PROCESS or
 * USER_PROCESS with the same "line" as the specified "entry".
 */
struct utmp *
getutline(const struct utmp *entry)
{
	utmp_api2frec(&ubuf, &fubuf);

	do {
		/*
		 * If the current entry is the one we are interested in,
		 * return a pointer to it.
		 */
		if (fubuf.ut_type != EMPTY &&
		    (fubuf.ut_type == LOGIN_PROCESS ||
		    fubuf.ut_type == USER_PROCESS) &&
		    strncmp(&entry->ut_line[0], &fubuf.ut_line[0],
		    sizeof (fubuf.ut_line)) == 0) {
			utmp_frec2api(&fubuf, &ubuf);
			return (&ubuf);
		}
	} while (getutent_frec() != NULL);

	utmp_frec2api(&fubuf, &ubuf);
	return (NULL);
}

/*
 * invoke_utmp_update
 *
 * Invokes the utmp_update program which has the privilege to write
 * to the /etc/utmp file.
 */

#define	UTMP_UPDATE 	"/usr/lib/utmp_update"
#define	STRSZ	64	/* Size of char buffer for argument strings */

void *
invoke_utmp_update(const struct utmp *entry, const struct utmpx *entryx)
{
	int status;
	pid_t childpid;
	int w, i;
	int use_utmpx = 0;
	char 	user[STRSZ], id[STRSZ], line[STRSZ], pid[STRSZ], type[STRSZ],
		term[STRSZ], exit[STRSZ], time[STRSZ], time_usec[STRSZ],
		session_id[STRSZ], pad[64], syslen[32], host[256];
	struct utmp *cur;
	struct utmpx *curx;

	if (entry == NULL)
		use_utmpx = 1;

	if (childpid = fork()) {
		if (childpid == -1)
			return (NULL);
		else {
			while ((w = waitpid(childpid, &status,
			    WEXITED)) != childpid && w != (pid_t)-1)
				;

			if (WIFEXITED(status) && WEXITSTATUS(status)) {
				/*
				 * The child encountered an error,
				 */
				return (NULL);
			}
			/*
			 * Normal termination, so return a pointer to the
			 * entry we just made.
			 */
			if (use_utmpx == 1) {
				setutxent();	/* Reset file pointer */

				while ((curx = getutxent()) != NULL) {
				    if ((curx->ut_type != EMPTY) &&
					((curx->ut_type == LOGIN_PROCESS) ||
					(curx->ut_type == USER_PROCESS) ||
					(curx->ut_type == DEAD_PROCESS)) &&
					(strncmp(&entryx->ut_line[0],
					&curx->ut_line[0],
					sizeof (curx->ut_line)) == 0))
					    return (curx);
				}
			} else {
				setutent();

				/*
				 * Find the entry that was just made -
				 * basically getutline, but allow DEAD_ type
				 */
				while ((cur = getutent()) != NULL) {
				    if (cur->ut_type != EMPTY &&
					((cur->ut_type == LOGIN_PROCESS) ||
					(cur->ut_type == USER_PROCESS) ||
					(cur->ut_type == DEAD_PROCESS))&&
					(strncmp(
					&entry->ut_line[0], &cur->ut_line[0],
					sizeof (cur->ut_line)) == 0))
					    return (cur);
				}
			}
			return (NULL);
		}
	} else {  /* We're the child, exec utmp_update */
		if (!use_utmpx) {
			/*
			 * Convert the utmp struct to strings for command line
			 * arguments.
			 */
			(void) strncpy(user, entry->ut_user,
			    sizeof (entry->ut_user));
			user[sizeof (entry->ut_user)] = '\0';
			(void) strncpy(id, entry->ut_id, sizeof (entry->ut_id));
			id[sizeof (entry->ut_id)] = '\0';
			(void) strncpy(line, entry->ut_line,
			    sizeof (entry->ut_line));
			line[sizeof (entry->ut_line)] = '\0';
			(void) sprintf(pid, "%d", entry->ut_pid);
			(void) sprintf(type, "%d", entry->ut_type);
			(void) sprintf(term, "%d",
			    entry->ut_exit.e_termination);
			(void) sprintf(exit, "%d", entry->ut_exit.e_exit);
			(void) sprintf(time, "%ld", entry->ut_time);

			(void) execl(UTMP_UPDATE, UTMP_UPDATE, "-u",
				user, id, line, pid, type, term, exit, time, 0);
		} else {
			char bin2hex[] = "0123456789ABCDEF";
			/*
			 * Convert the utmp struct to strings for command line
			 * arguments.
			 */
			(void) strncpy(user, entryx->ut_user,
			    sizeof (entryx->ut_user));
			user[sizeof (entryx->ut_user)] = '\0';
			(void) strncpy(id, entryx->ut_id,
			    sizeof (entryx->ut_id));
			id[sizeof (entryx->ut_id)] = '\0';
			(void) strncpy(line, entryx->ut_line,
			    sizeof (entryx->ut_line));
			line[sizeof (entryx->ut_line)] = '\0';
			(void) sprintf(pid, "%d", entryx->ut_pid);
			(void) sprintf(type, "%d", entryx->ut_type);
			(void) sprintf(term, "%d",
			    entryx->ut_exit.e_termination);
			(void) sprintf(exit, "%d", entryx->ut_exit.e_exit);
			(void) sprintf(time, "%ld", entryx->ut_tv.tv_sec);
			(void) sprintf(time_usec, "%ld", entryx->ut_tv.tv_usec);
			(void) sprintf(session_id, "%d", entryx->ut_session);
			for (i = 0; i < sizeof (entryx->pad); i += 2) {
				pad[i] = bin2hex[(entryx->pad[i] >> 4) & 0xF];
				pad[i+1] = bin2hex[(entryx->pad[i]) & 0xF];
			}
			pad[sizeof (entryx->pad)] = 0;
			(void) sprintf(syslen, "%d", entryx->ut_syslen);
			(void) sprintf(host, "%s", entryx->ut_host);

			(void) execl(UTMP_UPDATE, UTMP_UPDATE, "-x",
				user, id, line, pid, type, term, exit, time,
				time_usec, session_id, pad, syslen, host, 0);
		}

		/*
		 * only get here if the execl fails
		 */
		perror(UTMP_UPDATE);
		_exit(1);
		/*NOTREACHED*/
	}
}

/*
 * "pututline" writes the structure sent into the utmp file
 * If there is already an entry with the same id, then it is
 * overwritten, otherwise a new entry is made at the end of the
 * utmp file.
 */
struct utmp *
pututline(const struct utmp *entry)
{
	int fc;
	struct utmp *answer;
	struct utmp tmpbuf;
	struct futmp ftmpbuf;
	struct futmp savbuf;

	/*
	 * Copy the user supplied entry into our temporary buffer to
	 * avoid the possibility that the user is actually passing us
	 * the address of "ubuf".
	 */
	tmpbuf = *entry;
	utmp_api2frec(entry, &ftmpbuf);

	(void) getutent_frec();
	if (fd < 0) {
#ifdef	ERRDEBUG
		gdebug("pututline: Unable to create utmp file.\n");
#endif
		return (NULL);
	}

	/*
	 * If we are not the superuser than we can't write to /etc/utmp,
	 * so invoke update_utmp(8) to write the entry for us.
	 */
	if (changed_name == 0 && geteuid())
		return (invoke_utmp_update(entry, 0));

	/* Make sure file is writable */

	if ((fc = fcntl(fd, F_GETFL, NULL)) == -1 || (fc & O_RDWR) != O_RDWR)
		return (NULL);

	/*
	 * Find the proper entry in the utmp file.  Start at the current
	 * location.  If it isn't found from here to the end of the
	 * file, then reset to the beginning of the file and try again.
	 * If it still isn't found, then write a new entry at the end of
	 * the file.  (Making sure the location is an integral number of
	 * utmp structures into the file incase the file is scribbled.)
	 */

	if (getutid(&tmpbuf) == NULL) {
#ifdef	ERRDEBUG
		gdebug("1st getutid() failed. fd: %d", fd);
#endif
		setutent();
		if (getutid(&tmpbuf) == NULL) {
#ifdef	ERRDEBUG
			loc_utmp = lseek(fd, 0L, 1);
			gdebug("2nd getutid() failed. fd: %d loc_utmp: %ld\n",
			    fd, loc_utmp);
#endif
			(void) fcntl(fd, F_SETFL, fc | O_APPEND);
		} else
			(void) lseek(fd, -(long)sizeof (struct futmp), 1);
	} else
		(void) lseek(fd, -(long)sizeof (struct futmp), 1);

	/*
	 * Write out the user supplied structure.  If the write fails,
	 * then the user probably doesn't have permission to write the
	 * utmp file.
	 */
	if (write(fd, &ftmpbuf, sizeof (ftmpbuf)) != sizeof (ftmpbuf)) {
#ifdef	ERRDEBUG
		gdebug("pututline failed: write-%d\n", errno);
#endif
		answer = NULL;
	} else {
		/*
		 * Save the user structure that was overwritten.
		 * Copy the new user structure into ubuf so that it will
		 * be up to date in the future.
		 */
		savbuf = fubuf;
		fubuf = ftmpbuf;
		utmp_frec2api(&fubuf, &ubuf);
		answer = &ubuf;

#ifdef	ERRDEBUG
		gdebug("id: %c%c loc: %ld\n", fubuf.ut_id[0],
		    fubuf.ut_id[1], fubuf.ut_id[2], fubuf.ut_id[3],
		    loc_utmp);
#endif
	}

	/* update the parallel utmpx file */

	if (updutmpx(entry)) {
		(void) lseek(fd, -(long)sizeof (struct futmp), 1);
		(void) write(fd, &savbuf, sizeof (savbuf));
		answer = NULL;
	}

	(void) fcntl(fd, F_SETFL, fc);

	if (answer != NULL && (tmpbuf.ut_type == USER_PROCESS ||
	    tmpbuf.ut_type == DEAD_PROCESS))
		sendupid(tmpbuf.ut_type == USER_PROCESS ? ADDPID : REMPID,
		    (pid_t)tmpbuf.ut_pid);
	return (answer);
}

/*
 * "setutent" just resets the utmp file back to the beginning.
 */
void
setutent(void)
{
	if (fd != -1)
		(void) lseek(fd, 0L, 0);

	/*
	 * Zero the stored copy of the last entry read, since we are
	 * resetting to the beginning of the file.
	 */
	bzero(&ubuf, sizeof (ubuf));
	bzero(&fubuf, sizeof (fubuf));
}

/*
 * "endutent" closes the utmp file.
 */
void
endutent(void)
{
	if (fd != -1)
		(void) close(fd);
	fd = -1;
	bzero(&ubuf, sizeof (ubuf));
	bzero(&fubuf, sizeof (fubuf));
}

/*
 * "utmpname" allows the user to read a file other than the
 * normal "utmp" file.
 */
int
utmpname(const char *newfile)
{
	static char *saveptr;
	static size_t savelen = 0;
	size_t len;

	/* Determine if the new filename will fit.  If not, return 0. */

	if ((len = strlen(newfile)) >= MAXFILE)
		return (0);

	/* malloc enough space for utmp, utmpx, and null bytes */

	if (len > savelen) {
		if (saveptr)
			free(saveptr);
		if ((saveptr = malloc(2 * len + 3)) == 0)
			return (0);
		savelen = len;
	}

	/* copy in the new file name. */
	utmpfile = (const char *)saveptr;
	(void) strcpy(saveptr, newfile);
	utmpxfile = (const char *)saveptr + len + 2;
	(void) strcpy(saveptr + len + 2, newfile);
	(void) strcat(saveptr + len + 2, "x");

	/* Make sure everything is reset to the beginning state. */

	endutent();

	/*
	 * If the file is being changed to /etc/utmp or /var/adm/utmp then
	 * we clear the flag so pututline invokes utmp_update.  Otherwise
	 * we set the flag indicating that they changed to another name.
	 */
	if (strcmp(utmpfile, UTMP_FILE) == 0 ||
	    strcmp(utmpfile, VAR_UTMP_FILE) == 0)
		changed_name = 0;
	else
		changed_name = 1;

	return (1);
}

/*
 * "updutmpx" updates the utmpx file. Uses the same
 * search algorithm as pututline to make sure records
 * end up in the same place.
 */
static int
updutmpx(const struct utmp *entry)
{
	int fc, type;
	struct futmpx futx, *futxp = NULL;
	struct futmp fut;

	if (fd_u < 0) {
		if ((fd_u = open(utmpxfile, O_RDWR|O_CREAT, 0644)) < 0) {
#ifdef ERRDEBUG
			gdebug("Could not open utmpxfile\n");
#endif
			return (1);
		}
	}

	if ((fc = fcntl(fd_u, F_GETFL, NULL)) == -1) {
		(void) close(fd_u);
		fd_u = -1;
		return (1);
	}

	while (read(fd_u, &futx, sizeof (futx)) == sizeof (futx)) {
		if (futx.ut_type != EMPTY) {
			switch (entry->ut_type) {
			case EMPTY:
				goto done;
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == futx.ut_type) {
					futxp = &futx;
					goto done;
				}
				/*FALLTHROUGH*/
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				type = futx.ut_type;
				if ((type == INIT_PROCESS ||
				    type == LOGIN_PROCESS ||
				    type == USER_PROCESS ||
				    type == DEAD_PROCESS) &&
				    futx.ut_id[0] == entry->ut_id[0] &&
				    futx.ut_id[1] == entry->ut_id[1] &&
				    futx.ut_id[2] == entry->ut_id[2] &&
				    futx.ut_id[3] == entry->ut_id[3]) {
					futxp = &futx;
					goto done;
				}
			}
		}
	}

done:
	if (futxp)
		(void) lseek(fd_u, -(long)sizeof (futx), 1);
	else
		(void) fcntl(fd_u, F_SETFL, fc|O_APPEND);

	utmp_api2frec(entry, &fut);
	getutmpx_frec(&fut, &futx);

	if (write(fd_u, &futx, sizeof (futx)) != sizeof (futx)) {
#ifdef ERRDEBUG
		gdebug("updutmpx failed: write-%d\n", errno);
#endif
		(void) close(fd_u);
		fd_u = -1;
		return (1);
	}

	(void) fcntl(fd_u, F_SETFL, fc);

	(void) close(fd_u);
	fd_u = -1;

	return (0);
}

/*
 * If one of wtmp and wtmpx files exist, create the other, and the record.
 * If they both exist add the record.
 */
void
updwtmp(const char *file, struct utmp *ut)
{
	char filex[256];
	struct futmpx futx;
	struct futmp fut;
	int fd, fdx;

	(void) strcpy(filex, file);
	(void) strcat(filex, "x");

	fd = open(file, O_WRONLY | O_APPEND);
	fdx = open(filex, O_WRONLY | O_APPEND);

	if (fd < 0) {
		if (fdx < 0)
			return;
		if ((fd = open(file, O_WRONLY|O_CREAT)) < 0) {
			(void) close(fdx);
			return;
		}
	} else if ((fdx < 0) && ((fdx = open(filex, O_WRONLY|O_CREAT)) < 0)) {
		(void) close(fd);
		return;
	}

	(void) lseek(fd, 0, 2);
	(void) lseek(fdx, 0, 2);

	utmp_api2frec(ut, &fut);
	getutmpx_frec(&fut, &futx);
	(void) write(fd, &fut, sizeof (fut));
	(void) write(fdx, &futx, sizeof (futx));

/*
done:
	(void) close(fd);
	(void) close(fdx);
 */
}


/*
 * "getutmp" - convert a utmpx record to a utmp record.
 * "getutmp_frec" - private version of same routine for file data.
 */
void
getutmp(const struct utmpx *utx, struct utmp *ut)
{
	(void) strncpy(ut->ut_user, utx->ut_user, sizeof (ut->ut_user));
	(void) strncpy(ut->ut_line, utx->ut_line, sizeof (ut->ut_line));
	(void) memcpy(ut->ut_id, utx->ut_id, sizeof (utx->ut_id));
	ut->ut_pid = utx->ut_pid;
	ut->ut_type = utx->ut_type;
	ut->ut_exit = utx->ut_exit;
	ut->ut_time = utx->ut_tv.tv_sec;
}

static void
getutmp_frec(const struct futmpx *utx, struct futmp *ut)
{
	(void) strncpy(ut->ut_user, utx->ut_user, sizeof (ut->ut_user));
	(void) strncpy(ut->ut_line, utx->ut_line, sizeof (ut->ut_line));
	(void) memcpy(ut->ut_id, utx->ut_id, sizeof (utx->ut_id));
	ut->ut_pid = utx->ut_pid;
	ut->ut_type = utx->ut_type;
	ut->ut_exit.e_termination = utx->ut_exit.e_termination;
	ut->ut_exit.e_exit = utx->ut_exit.e_exit;
	ut->ut_time = (time32_t)utx->ut_tv.tv_sec;
}

/*
 * "getutmpx" - convert a utmp record to a utmpx record.
 * "getutmpx_frec" - private version of same routine for file data.
 */
void
getutmpx(const struct utmp *ut, struct utmpx *utx)
{
	(void) strncpy(utx->ut_user, ut->ut_user, sizeof (ut->ut_user));
	(void) bzero(&utx->ut_user[sizeof (ut->ut_user)],
	    sizeof (utx->ut_user) - sizeof (ut->ut_user));
	(void) strncpy(utx->ut_line, ut->ut_line, sizeof (ut->ut_line));
	(void) bzero(&utx->ut_line[sizeof (ut->ut_line)],
	    sizeof (utx->ut_line) - sizeof (ut->ut_line));
	(void) memcpy(utx->ut_id, ut->ut_id, sizeof (ut->ut_id));
	utx->ut_pid = ut->ut_pid;
	utx->ut_type = ut->ut_type;
	utx->ut_exit = ut->ut_exit;
	utx->ut_tv.tv_sec = ut->ut_time;
	utx->ut_tv.tv_usec = 0;
	utx->ut_session = 0;
	bzero(utx->pad, sizeof (utx->pad));
	bzero(utx->ut_host, sizeof (utx->ut_host));
	utx->ut_syslen = 0;
}

static void
getutmpx_frec(const struct futmp *ut, struct futmpx *utx)
{
	(void) strncpy(utx->ut_user, ut->ut_user, sizeof (ut->ut_user));
	(void) bzero(&utx->ut_user[sizeof (ut->ut_user)],
	    sizeof (utx->ut_user) - sizeof (ut->ut_user));
	(void) strncpy(utx->ut_line, ut->ut_line, sizeof (ut->ut_line));
	(void) bzero(&utx->ut_line[sizeof (ut->ut_line)],
	    sizeof (utx->ut_line) - sizeof (ut->ut_line));
	(void) memcpy(utx->ut_id, ut->ut_id, sizeof (ut->ut_id));
	utx->ut_pid = ut->ut_pid;
	utx->ut_type = ut->ut_type;
	utx->ut_exit.e_termination = ut->ut_exit.e_termination;
	utx->ut_exit.e_exit = ut->ut_exit.e_exit;
	utx->ut_tv.tv_sec = ut->ut_time;
	utx->ut_tv.tv_usec = 0;
	utx->ut_session = 0;
	bzero(utx->pad, sizeof (utx->pad));
	bzero(utx->ut_host, sizeof (utx->ut_host));
	utx->ut_syslen = 0;
}

/*
 * "updutfile" updates the utmp file using the contents of the
 * utmpx file.
 */
int
updutfile(char *utf, char *utxf)
{
	struct futmpx utx;
	struct futmp ut;
	int fd1, fd2;

	if ((fd1 = open(utf, O_RDWR|O_TRUNC)) < 0)
		return (1);

	if ((fd2 = open(utxf, O_RDONLY)) < 0) {
		(void) close(fd1);
		return (1);
	}

	while (read(fd2, &utx, sizeof (utx)) == sizeof (utx)) {
		getutmp_frec(&utx, &ut);
		if (write(fd1, &ut, sizeof (ut)) != sizeof (ut)) {
			(void) close(fd1);
			(void) close(fd2);
			return (1);
		}
	}

	(void) close(fd1);
	(void) close(fd2);
	(void) utime(utxf, NULL);
	return (0);
}


/*
 * "updutxfile" updates the utmpx file using the contents of the
 * utmp file. Tries to preserve the host information as much
 * as possible.
 */
int
updutxfile(char *utf, char *utxf)
{
	struct futmp fut;
	struct futmpx futx;
	int fd1, fd2;
	ssize_t n1, cnt = 0;

	if ((fd1 = open(utf, O_RDONLY)) < 0)
		return (1);

	if ((fd2 = open(utxf, O_RDWR|O_TRUNC)) < 0) {
		(void) close(fd1);
		return (1);
	}

	/*
	 * As long as the entries match, copy the records from the
	 * utmpx file to keep the host information.
	 */
	while ((n1 = read(fd1, &fut, sizeof (fut))) == sizeof (fut)) {
		if (read(fd2, &futx, sizeof (futx)) != sizeof (futx))
			break;
		if (fut.ut_pid != futx.ut_pid ||
		    fut.ut_type != futx.ut_type ||
		    !memcmp(fut.ut_id, futx.ut_id, sizeof (fut.ut_id)) ||
		    !memcmp(fut.ut_line, futx.ut_line, sizeof (fut.ut_line))) {
			getutmpx_frec(&fut, &futx);
			(void) lseek(fd2,
			    -(long)sizeof (struct futmpx), 1);
			if (write(fd2, &futx,
			    sizeof (futx)) != sizeof (futx)) {
				(void) close(fd1);
				(void) close(fd2);
				return (1);
			}
			cnt += sizeof (struct futmpx);
		}
	}

	/*
	 * out of date file is shorter, copy from the up to date file
	 * to the new file.
	 */
	if (n1 > 0) {
		do {
			getutmpx_frec(&fut, &futx);
			if (write(fd2, &futx,
			    sizeof (futx)) != sizeof (futx)) {
				(void) close(fd1);
				(void) close(fd2);
				return (1);
			}
		} while ((n1 = read(fd1, &fut,
		    sizeof (fut))) == sizeof (fut));
	} else {
		/* out of date file was longer, truncate it */
		(void) truncate(utxf, cnt);
	}

	(void) close(fd1);
	(void) close(fd2);
	(void) utime(utf, NULL);
	return (0);
}

/*
 * makeut - create a utmp entry, recycling an id if a wild card is
 *	specified.  Also notify init about the new pid
 *
 *	args:	utmp - point to utmp structure to be created
 */
struct utmp *
makeut(struct utmp *utmp)
{
	int i;
	struct utmp *utp;	/* "current" utmp entry being examined */
	int wild;		/* flag, true iff wild card char seen */

	/* the last id we matched that was NOT a dead proc */
	unsigned char saveid[IDLEN];

	wild = 0;
	for (i = 0; i < IDLEN; i++)
		if ((unsigned char)utmp->ut_id[i] == SC_WILDC) {
			wild = 1;
			break;
		}

	if (wild) {

		/*
		 * try to lock the utmp file, only needed if we're
		 * doing wildcard matching
		 */

		if (lockut())
			return (0);
		setutent();

		/* find the first alphanumeric character */
		for (i = 0; i < MAXVAL; ++i)
			if (isalnum(i))
				break;

		(void) memset(saveid, i, IDLEN);

		while ((utp = getutent()) != 0) {
			if (idcmp(utmp->ut_id, utp->ut_id))
				continue;
			if (utp->ut_type == DEAD_PROCESS)
				break;
			(void) memcpy(saveid, utp->ut_id, IDLEN);
		}

		if (utp) {
			/*
			 * found an unused entry, reuse it
			 */
			(void) memcpy(utmp->ut_id, utp->ut_id, IDLEN);
			utp = pututline(utmp);
			if (utp)
				updwtmp(WTMP_FILE, utp);
			endutent();
			unlockut();
			sendpid(ADDPID, (pid_t)utmp->ut_pid);
			return (utp);

		} else {
			/*
			 * nothing available, try to allocate an id
			 */
			if (allocid(utmp->ut_id, saveid)) {
				endutent();
				unlockut();
				return (NULL);
			} else {
				utp = pututline(utmp);
				if (utp)
					updwtmp(WTMP_FILE, utp);
				endutent();
				unlockut();
				sendpid(ADDPID, (pid_t)utmp->ut_pid);
				return (utp);
			}
		}
	} else {
		utp = pututline(utmp);
		if (utp)
			updwtmp(WTMP_FILE, utp);
		endutent();
		sendpid(ADDPID, (pid_t)utmp->ut_pid);
		return (utp);
	}
}


/*
 * modut - modify a utmp entry.	 Also notify init about new pids or
 *	old pids that it no longer needs to care about
 *
 *	args:	utmp - point to utmp structure to be created
 */
struct utmp *
modut(struct utmp *utp)
{
	int i;					/* scratch variable */
	struct utmp utmp;			/* holding area */
	struct utmp *ucp = &utmp;		/* and a pointer to it */
	struct utmp *up;	/* "current" utmp entry being examined */
	struct futmp *fup;

	for (i = 0; i < IDLEN; ++i)
		if ((unsigned char)utp->ut_id[i] == SC_WILDC)
			return (0);

	/* copy the supplied utmp structure someplace safe */
	utmp = *utp;
	setutent();
	while (fup = getutent_frec()) {
		if (idcmp(ucp->ut_id, fup->ut_id))
			continue;
		/* only get here if ids are the same, i.e. found right entry */
		if (ucp->ut_pid != fup->ut_pid) {
			sendpid(REMPID, (pid_t)fup->ut_pid);
			sendpid(ADDPID, (pid_t)ucp->ut_pid);
		}
		break;
	}
	up = pututline(ucp);
	if (ucp->ut_type == DEAD_PROCESS)
		sendpid(REMPID, (pid_t)ucp->ut_pid);
	if (up)
		updwtmp(WTMP_FILE, up);
	endutent();
	return (up);
}



/*
 * idcmp - compare two id strings, return 0 if same, non-zero if not *
 *	args:	s1 - first id string
 *		s2 - second id string
 */
static int
idcmp(const char *s1, const char *s2)
{
	int i;

	for (i = 0; i < IDLEN; ++i)
		if ((unsigned char)*s1 != SC_WILDC && (*s1++ != *s2++))
			return (-1);
	return (0);
}


/*
 * allocid - allocate an unused id for utmp, either by recycling a
 *	DEAD_PROCESS entry or creating a new one.  This routine only
 *	gets called if a wild card character was specified.
 *
 *	args:	srcid - pattern for new id
 *		saveid - last id matching pattern for a non-dead process
 */
static int
allocid(char *srcid, unsigned char *saveid)
{
	int i;		/* scratch variable */
	int changed;	/* flag to indicate that a new id has been generated */
	char copyid[IDLEN];	/* work area */

	(void) memcpy(copyid, srcid, IDLEN);
	changed = 0;
	for (i = 0; i < IDLEN; ++i) {
		/*
		 * if this character isn't wild, it'll
		 * be part of the generated id
		 */
		if ((unsigned char) copyid[i] != SC_WILDC)
			continue;
		/*
		 * it's a wild character, retrieve the
		 * character from the saved id
		 */
		copyid[i] = saveid[i];
		/*
		 * if we haven't changed anything yet,
		 * try to find a new char to use
		 */
		if (!changed && (saveid[i] < MAXVAL)) {

/*
 * Note: this algorithm is taking the "last matched" id and trying to make
 * a 1 character change to it to create a new one.  Rather than special-case
 * the first time (when no perturbation is really necessary), just don't
 * allocate the first valid id.
 */

			while (++saveid[i] < MAXVAL) {
				/* make sure new char is alphanumeric */
				if (isalnum(saveid[i])) {
					copyid[i] = saveid[i];
					changed = 1;
					break;
				}
			}

			if (!changed) {
				/*
				 * Then 'reset' the current count at
				 * this position to it's lowest valid
				 * value, and propagate the carry to
				 * the next wild-card slot
				 *
				 * See 1113208.
				 */
				saveid[i] = 0;
				while (!isalnum(saveid[i]))
					saveid[i]++;
				copyid[i] = ++saveid[i];
			}
		}
	}
	/* changed is true if we were successful in allocating an id */
	if (changed) {
		(void) memcpy(srcid, copyid, IDLEN);
		return (0);
	} else
		return (-1);
}


/*
 * lockut - lock utmp and utmpx files
 */
static int
lockut(void)
{
	if ((fd = open(UTMP_FILE, O_RDWR|O_CREAT, 0644)) < 0)
		return (-1);

	if ((fd_u = open(UTMPX_FILE, O_RDWR|O_CREAT, 0644)) < 0) {
		(void) close(fd);
		fd = -1;
		return (-1);
	}

	if (lockf(fd, F_LOCK, 0) < 0 || lockf(fd_u, F_LOCK, 0) < 0) {
		(void) close(fd);
		(void) close(fd_u);
		fd = fd_u = -1;
		return (-1);
	}
	return (0);
}


/*
 * unlockut - unlock utmp and utmpx files
 */
static void
unlockut(void)
{
	(void) lockf(fd, F_ULOCK, 0);
	(void) lockf(fd_u, F_ULOCK, 0);
	(void) close(fd);
	(void) close(fd_u);
	fd = -1;
	fd_u = -1;
}


/*
 * sendpid - send message to init to add or remove a pid from the
 *	"godchild" list
 *
 *	args:	cmd - ADDPID or REMPID
 *		pid - pid of "godchild"
 */
static void
sendpid(int cmd, pid_t pid)
{
	int pfd;		/* file desc. for init pipe */
	pidrec_t prec;		/* place for message to be built */

	/*
	 * if for some reason init didn't open initpipe, open it read/write
	 * here to avoid sending SIGPIPE to the calling process
	 */

	pfd = open(IPIPE, O_RDWR);
	if (pfd < 0)
		return;
	prec.pd_pid = pid;
	prec.pd_type = cmd;
	(void) write(pfd, &prec, sizeof (pidrec_t));
	(void) close(pfd);
}


#ifdef  ERRDEBUG

#include <stdarg.h>
#include <stdio.h>

static void
gdebug(const char *fmt, ...)
{
	FILE *fp;
	int errnum;
	va_list ap;

	if ((fp = fopen("/etc/dbg.getut", "a+")) == NULL)
		return;
	va_start(ap, fmt);
	(void) vfprintf(fp, fmt, ap);
	va_end(ap);
	(void) fclose(fp);
}
#endif

/*
 * sendupid - send message to utmp daemon to add or remove a pid from the
 *	list to be watched.
 *
 *	args:	cmd - ADDPID or REMPID
 *		pid - pid of "godchild"
 */
static void
sendupid(int cmd, pid_t pid)
{
	int pfd;		/* file desc. for utmp pipe */
	pidrec_t prec;		/* place for message to be built */

	/*
	 * if for some reason utmp didn't open utmppipe, open it read/write
	 * here to avoid sending SIGPIPE to the calling process
	 */
	pfd = open(UPIPE, O_RDWR | O_NONBLOCK | O_NDELAY);
	if (pfd < 0)
		return;
	prec.pd_pid = pid;
	prec.pd_type = cmd;
	(void) write(pfd, &prec, sizeof (pidrec_t));
	(void) close(pfd);
}
