/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)getutx.c 1.27	97/08/29 SMI"	/* SVr4.0 1.7	*/

/*LINTLIBRARY*/

/*
******************************************************************

		PROPRIETARY NOTICE(Combined)

This source code is unpublished proprietary information
constituting, or derived under license from AT&T's UNIX(r) System V.
In addition, portions of such source code were derived from Berkeley
4.3 BSD under license from the Regents of the University of
California.



		Copyright Notice

Notice of copyright on this source code product does not indicate
publication.

	(c) 1986-1996  Sun Microsystems, Inc
	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
		All rights reserved.
******************************************************************* */

/*
 * Routines to read and write the /etc/utmpx file.
 */

#pragma weak getutxent = _getutxent
#pragma weak getutxid = _getutxid
#pragma weak getutxline = _getutxline
#pragma weak makeutx = _makeutx
#pragma weak modutx = _modutx
#pragma weak pututxline = _pututxline
#pragma weak setutxent = _setutxent
#pragma weak endutxent = _endutxent
#pragma weak utmpxname = _utmpxname
#pragma weak updutmp = _updutmp
#pragma weak updwtmpx = _updwtmpx

#include "synonyms.h"
#include <sys/types.h>
#include <stdio.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <utmpx.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>

#define	IDLEN		4	/* length of id field in utmp */
#define	SC_WILDC	0xff	/* wild char for utmp ids */
#define	MAXFILE		79	/* Maximum pathname length for "utmpx" file */

#define	MAXVAL		255		/* max value for an id `character' */
#define	IPIPE		"/etc/initpipe"	/* FIFO to send pids to init */
#define	UPIPE		"/etc/utmppipe"	/* FIFO to send pids to utmpd */

#define	VAR_UTMPX_FILE	"/var/adm/utmpx" /* for sanity check only */


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

static void	utmpx_frec2api(const struct futmpx *, struct utmpx *);
static void	utmpx_api2frec(const struct utmpx *, struct futmpx *);

static void	unlockutx(void);
static void	sendpid(int, pid_t);
static void	sendupid(int, pid_t);
static int	idcmp(const char *, const char *);
static int	allocid(char *, unsigned char *);
static int	lockutx(void);
static int	updutmp(struct utmpx *entry);
static int	updateutmp(struct utmpx *entry, off_t offset);

static void	getutmp_frec(const struct futmpx *, struct futmp *);

static int fd = -1;	/* File descriptor for the utmpx file. */
static int fd_u = -1;	/* File descriptor for the utmp file. */

static	FILE	*fp = NULL;	/* Buffered file descriptior for utmpx file */
static int changed_name = 0;	/* Flag set when not using utmpx file */
static char utmpxfile[MAXFILE+1] = UTMPX_FILE;	/* Name of the current */
static char utmpfile[MAXFILE+1] = UTMP_FILE;	/* "utmpx" and "utmp"  */

static struct futmpx fubuf;	/* Copy of last entry read in. */
static struct utmpx ubuf;	/* Last entry returned to client */

/*
 * In the 64-bit world, the utmpx data structure grows because of
 * the ut_time field (a struct timeval) grows in the middle of it.
 */
static void
utmpx_frec2api(const struct futmpx *src, struct utmpx *dst)
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
	dst->ut_tv.tv_sec = (time_t)src->ut_tv.tv_sec;
	dst->ut_tv.tv_usec = (suseconds_t)src->ut_tv.tv_usec;
	dst->ut_session = src->ut_session;
	bzero(dst->pad, sizeof (dst->pad));
	dst->ut_syslen = src->ut_syslen;
	(void) memcpy(dst->ut_host, src->ut_host, sizeof (dst->ut_host));
}

static void
utmpx_api2frec(const struct utmpx *src, struct futmpx *dst)
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
	dst->ut_tv.tv_sec = (time32_t)src->ut_tv.tv_sec;
	dst->ut_tv.tv_usec = (int32_t)src->ut_tv.tv_usec;
	dst->ut_session = src->ut_session;
	bzero(dst->pad, sizeof (dst->pad));
	dst->ut_syslen = src->ut_syslen;
	(void) memcpy(dst->ut_host, src->ut_host, sizeof (dst->ut_host));
}

/*
 * "getutxent_frec" gets the raw version of the next entry in the utmpx file.
 */
static struct futmpx *
getutxent_frec(void)
{
	int fd2;

	/*
	 * If the "utmpx" file is not open, attempt to open it for
	 * reading.  If there is no file, attempt to create one.  If
	 * both attempts fail, return NULL.  If the file exists, but
	 * isn't readable and writeable, do not attempt to create.
	 */
	if (fd < 0) {

		if ((fd = open(utmpxfile, O_RDWR|O_CREAT, 0644)) < 0) {

			/*
			 * If the open failed for permissions, try opening
			 * it only for reading.  All "pututxline()" later
			 * will fail the writes.
			 */

			if ((fd = open(utmpxfile, O_RDONLY)) < 0)
				return (NULL);

			if ((fp = fopen(utmpxfile, "r")) == NULL)
				return (NULL);
		} else {
			/*
			 * Get the stream pointer
			 */
			if ((fp = fopen(utmpxfile, "r+")) == NULL)
				return (NULL);
		}

		/*
		 * create the utmp file here
		 */
		if (access(utmpfile, F_OK) < 0) {
			if ((fd2 = open(utmpfile, O_RDWR|O_CREAT, 0644)) < 0)
				return (NULL);
			(void) close(fd2);
		}
	}

	/*
	 * Try to read in the next entry from the utmpx file.
	 */
	if (fread(&fubuf, sizeof (fubuf), 1, fp) != 1) {
		/*
		 * Make sure fubuf is zeroed.
		 */
		bzero(&fubuf, sizeof (fubuf));
		return (NULL);
	}

	return (&fubuf);
}

/*
 * "getutxent" gets the next entry in the utmpx file.
 */
struct utmpx *
getutxent(void)
{
	struct futmpx *futxp;

	futxp = getutxent_frec();
	utmpx_frec2api(&fubuf, &ubuf);
	if (futxp == NULL)
		return (NULL);
	return (&ubuf);
}

/*
 * "getutxid" finds the specified entry in the utmpx file.  If
 * it can't find it, it returns NULL.
 */
struct utmpx *
getutxid(const struct utmpx *entry)
{
	short type;

	/*
	 * From XPG5: "The getutxid() or getutxline() may cache data.
	 * For this reason, to use getutxline() to search for multiple
	 * occurrences, it is necessary to zero out the static data after
	 * each success, or getutxline() could just return a pointer to
	 * the same utmpx structure over and over again."
	 */
	utmpx_api2frec(&ubuf, &fubuf);

	/*
	 * Start looking for entry. Look in our current buffer before
	 * reading in new entries.
	 */
	do {
		/*
		 * If there is no entry in "fubuf", skip to the read.
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
			 * entries, only the types have to match.  If they do,
			 * return the address of internal buffer.
			 */
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == fubuf.ut_type) {
					utmpx_frec2api(&fubuf, &ubuf);
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
				    (fubuf.ut_id[0] == entry->ut_id[0]) &&
				    (fubuf.ut_id[1] == entry->ut_id[1]) &&
				    (fubuf.ut_id[2] == entry->ut_id[2]) &&
				    (fubuf.ut_id[3] == entry->ut_id[3])) {
					utmpx_frec2api(&fubuf, &ubuf);
					return (&ubuf);
				}
				break;

			/*
			 * Do not search for illegal types of entry.
			 */
			default:
				return (NULL);
			}
		}
	} while (getutxent_frec() != NULL);

	/*
	 * Return NULL since the proper entry wasn't found.
	 */
	utmpx_frec2api(&fubuf, &ubuf);
	return (NULL);
}

/*
 * "getutxline" searches the "utmpx" file for a LOGIN_PROCESS or
 * USER_PROCESS with the same "line" as the specified "entry".
 */
struct utmpx *
getutxline(const struct utmpx *entry)
{
	/*
	 * From XPG5: "The getutxid() or getutxline() may cache data.
	 * For this reason, to use getutxline() to search for multiple
	 * occurrences, it is necessary to zero out the static data after
	 * each success, or getutxline() could just return a pointer to
	 * the same utmpx structure over and over again."
	 */
	utmpx_api2frec(&ubuf, &fubuf);

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
			utmpx_frec2api(&fubuf, &ubuf);
			return (&ubuf);
		}
	} while (getutxent_frec() != NULL);

	/*
	 * Since entry wasn't found, return NULL.
	 */
	utmpx_frec2api(&fubuf, &ubuf);
	return (NULL);
}

/*
 * "pututxline" writes the structure sent into the utmpx file.
 * If there is already an entry with the same id, then it is
 * overwritten, otherwise a new entry is made at the end of the
 * utmpx file.
 */

extern	struct utmpx *
    invoke_utmp_update(const struct utmp *, const struct utmpx *);

struct utmpx *
pututxline(const struct utmpx *entry)
{
	struct utmpx *answer;
	off_t offset, lock = 0;
	struct utmpx tmpxbuf;
	struct futmpx ftmpxbuf;
	struct futmpx savbuf;

	/*
	 * Copy the user supplied entry into our temporary buffer to
	 * avoid the possibility that the user is actually passing us
	 * the address of "ubuf".
	 */
	if (entry == NULL)
		return (NULL);

	tmpxbuf = *entry;
	utmpx_api2frec(entry, &ftmpxbuf);

	if (fd < 0) {
		(void) getutxent_frec();
		if (fd < 0)
			return ((struct utmpx *)NULL);
	}

	/*
	 * If we are not the superuser than we can't write to /etc/utmp,
	 * so invoke update_utmp(8) to write the entry for us.
	 */
	if (changed_name == 0 && geteuid() != 0)
		return (invoke_utmp_update((struct utmp *)0, entry));

	/*
	 * Find the proper entry in the utmpx file.  Start at the current
	 * location.  If it isn't found from here to the end of the
	 * file, then reset to the beginning of the file and try again.
	 * If it still isn't found, then write a new entry at the end of
	 * the file.  (Making sure the location is an integral number of
	 * utmp structures into the file incase the file is scribbled.)
	 */

	if (getutxid(&tmpxbuf) == NULL) {

		setutxent();

		/*
		 * Lock the the entire file from here onwards.
		 */
		if (getutxid(&tmpxbuf) == NULL) {
			lock++;
			if (lockf(fd, F_LOCK, 0) < NULL)
				return (NULL);
			(void) fseek(fp, 0, SEEK_END);
		} else
			(void) fseek(fp, -(long)sizeof (struct futmpx),
			    SEEK_CUR);
	} else
		(void) fseek(fp, -(long)sizeof (struct futmpx), SEEK_CUR);

	/*
	 * Write out the user supplied structure.  If the write fails,
	 * then the user probably doesn't have permission to write the
	 * utmpx file.
	 */
	offset = ftell(fp);
	if (fwrite(&ftmpxbuf, sizeof (ftmpxbuf), 1, fp) != 1) {
		answer = (struct utmpx *)NULL;
	} else {
		/*
		 * Save the user structure that was overwritten. Copy
		 * the new user structure into ubuf and fubuf so that
		 * it will be up to date in the future.
		 */
		(void) fflush(fp);
		savbuf = fubuf;
		fubuf = ftmpxbuf;
		utmpx_frec2api(&fubuf, &ubuf);
		answer = &ubuf;
	}

	if (updateutmp((struct utmpx *)entry, offset)) {
		(void) fseek(fp, -(long)sizeof (struct futmpx), SEEK_CUR);
		(void) fwrite(&savbuf, sizeof (savbuf), 1, fp);
		answer = NULL;
	}

	if (lock)
		(void) lockf(fd, F_ULOCK, 0);

	if (answer != NULL && (tmpxbuf.ut_type == USER_PROCESS ||
	    tmpxbuf.ut_type == DEAD_PROCESS))
		sendupid(tmpxbuf.ut_type == USER_PROCESS ? ADDPID : REMPID,
		    (pid_t)tmpxbuf.ut_pid);
	return (answer);
}

/*
 * "setutxent" just resets the utmpx file back to the beginning.
 */
void
setutxent(void)
{
	if (fd != -1)
		(void) lseek(fd, 0L, SEEK_SET);

	if (fp != NULL)
		(void) fseek(fp, 0L, SEEK_SET);

	/*
	 * Zero the stored copy of the last entry read, since we are
	 * resetting to the beginning of the file.
	 */
	bzero(&ubuf, sizeof (ubuf));
	bzero(&fubuf, sizeof (fubuf));
}


/*
 * "endutxent" closes the utmpx file.
 */
void
endutxent(void)
{
	if (fd != -1)
		(void) close(fd);
	fd = -1;

	if (fp != NULL)
		(void) fclose(fp);
	fp = NULL;

	bzero(&ubuf, sizeof (ubuf));
	bzero(&fubuf, sizeof (fubuf));
}

/*
 * "utmpxname" allows the user to read a file other than the
 * normal "utmpx" file.
 */
int
utmpxname(const char *newfile)
{
	char utmpxfile[MAXFILE+1];
	char utmpfile[MAXFILE+1];
	size_t len;

	/*
	 * Determine if the new filename will fit.  If not, return 0.
	 */
	if ((len = strlen(newfile)) > MAXFILE-1)
		return (0);

	/*
	 * The name of the utmpx file has to end with 'x'
	 */
	if (newfile[len-1] != 'x')
		return (0);

	/*
	 * Otherwise copy in the new file name.
	 */
	else {
		(void) strcpy(&utmpxfile[0], newfile);
		(void) strcpy(&utmpfile[0], newfile);
		/*
		 * strip the 'x'
		 */
		utmpfile[len-1] = '\0';
	}
	/*
	 * Make sure everything is reset to the beginning state.
	 */
	endutxent();

	/*
	 * If the file is being changed to /etc/utmpx or /var/adm/utmpx then
	 * we clear the flag so pututxline invokes utmp_update.  Otherwise
	 * we set the flag indicating that they changed to another name.
	 */
	if (strcmp(utmpxfile, UTMPX_FILE) == 0 ||
	    strcmp(utmpxfile, VAR_UTMPX_FILE) == 0)
		changed_name = 0;
	else
		changed_name = 1;

	return (1);
}

/*
 * "updutmp" updates the utmp file, uses same algorithm as
 * pututxline so that the records end up in the same spot.
 */
static int
updutmp(struct utmpx *entry)
{
	int fc, type;
	struct futmp fut, *futp = NULL;
	struct futmpx futx;

	if (fd_u < 0) {
		if ((fd_u = open(utmpfile, O_RDWR|O_CREAT, 0644)) < 0) {
			return (1);
		}
	}

	if ((fc = fcntl(fd_u, F_GETFL, NULL)) == -1) {
		(void) close(fd_u);
		fd_u = -1;
		return (1);
	}

	while (read(fd_u, &fut, sizeof (fut)) == sizeof (fut)) {
		if (fut.ut_type != EMPTY) {
			switch (entry->ut_type) {
			case EMPTY:
				goto done;
			case RUN_LVL:
			case BOOT_TIME:
			case OLD_TIME:
			case NEW_TIME:
				if (entry->ut_type == fut.ut_type) {
					futp = &fut;
					goto done;
				}
				/*FALLTHROUGH*/
			case INIT_PROCESS:
			case LOGIN_PROCESS:
			case USER_PROCESS:
			case DEAD_PROCESS:
				type = fut.ut_type;
				if ((type == INIT_PROCESS ||
				    type == LOGIN_PROCESS ||
				    type == USER_PROCESS ||
				    type == DEAD_PROCESS) &&
				    fut.ut_id[0] == entry->ut_id[0] &&
				    fut.ut_id[1] == entry->ut_id[1] &&
				    fut.ut_id[2] == entry->ut_id[2] &&
				    fut.ut_id[3] == entry->ut_id[3]) {
					futp = &fut;
					goto done;
				}
			}
		}
	}

done:
	if (futp)
		(void) lseek(fd_u, -(long)sizeof (fut), 1);
	else
		(void) fcntl(fd_u, F_SETFL, fc|O_APPEND);

	utmpx_api2frec(entry, &futx);
	getutmp_frec(&futx, &fut);

	if (write(fd_u, &fut, sizeof (fut)) != sizeof (fut)) {
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
updwtmpx(const char *filex, struct utmpx *utx)
{
	char file[MAXFILE+1];
	struct futmp fut;
	struct futmpx futx;
	int wfd, wfdx;

	(void) strcpy(file, filex);
	file[strlen(filex) - 1] = '\0';

	wfd = open(file, O_WRONLY | O_APPEND);
	wfdx = open(filex, O_WRONLY | O_APPEND);

	if (wfd < 0) {
		if (wfdx < 0)
			return;
		if ((wfd = open(file, O_WRONLY|O_CREAT)) < 0) {
			(void) close(wfdx);
			return;
		}
	} else if ((wfdx < 0) && ((wfdx = open(filex, O_WRONLY|O_CREAT)) < 0)) {
		(void) close(wfd);
		return;
	}

	(void) lseek(wfd, 0, SEEK_END);
	(void) lseek(wfdx, 0, SEEK_END);

	utmpx_api2frec(utx, &futx);
	getutmp_frec(&futx, &fut);
	(void) write(wfd, &fut, sizeof (fut));
	(void) write(wfdx, &futx, sizeof (futx));

done:
	(void) close(wfd);
	(void) close(wfdx);
}

/*
 * Make a utmp file record from a utmpx file record
 */
static void
getutmp_frec(const struct futmpx *src, struct futmp *dst)
{
	(void) strncpy(dst->ut_user, src->ut_user, sizeof (dst->ut_user));
	(void) strncpy(dst->ut_line, src->ut_line, sizeof (dst->ut_line));
	(void) memcpy(dst->ut_id, src->ut_id, sizeof (dst->ut_id));
	dst->ut_pid = src->ut_pid;
	dst->ut_type = src->ut_type;
	dst->ut_exit.e_termination = src->ut_exit.e_termination;
	dst->ut_exit.e_exit = src->ut_exit.e_exit;
	dst->ut_time = (time32_t)src->ut_tv.tv_sec;
}

/*
 * modutx - modify a utmpx entry.  Also notify init about new pids or
 *	old pids that it no longer needs to care about
 *
 *	args:	utp- point to utmpx structure to be created
 */
struct utmpx *
modutx(const struct utmpx *utp)
{
	int i;
	struct utmpx utmp;		/* holding area */
	struct utmpx *ucp = &utmp;	/* and a pointer to it */
	struct utmpx *up;		/* "current" utmpx entry */
	struct futmpx *fup;		/* being examined */

	for (i = 0; i < IDLEN; ++i) {
		if ((unsigned char)utp->ut_id[i] == SC_WILDC)
			return (NULL);
	}

	/*
	 * copy the supplied utmpx structure someplace safe
	 */
	utmp = *utp;
	setutxent();
	while (fup = getutxent_frec()) {
		if (idcmp(ucp->ut_id, fup->ut_id))
			continue;

		/*
		 * only get here if ids are the same, i.e. found right entry
		 */
		if (ucp->ut_pid != fup->ut_pid) {
			sendpid(REMPID, (pid_t)fup->ut_pid);
			sendpid(ADDPID, (pid_t)ucp->ut_pid);
		}
		break;
	}
	up = pututxline(ucp);
	if (ucp->ut_type == DEAD_PROCESS)
		sendpid(REMPID, (pid_t)ucp->ut_pid);
	if (up)
		updwtmpx(WTMPX_FILE, up);
	endutxent();
	return (up);
}


/*
 * idcmp - compare two id strings, return  0 if same, non-zero if not *
 *	args:	s1 - first id string
 *		s2 - second id string
 */
static int
idcmp(const char *s1, const char *s2)
{
	int i;

	for (i = 0; i < IDLEN; ++i)
		if ((unsigned char) *s1 != SC_WILDC && (*s1++ != *s2++))
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
	int changed;		/* flag to indicate that a new id has */
				/* been generated */
	char copyid[IDLEN];	/* work area */

	(void) memcpy(copyid, srcid, IDLEN);
	changed = 0;
	for (i = 0; i < IDLEN; ++i) {

		/*
		 * if this character isn't wild, it'll be part of the
		 * generated id
		 */
		if ((unsigned char) copyid[i] != SC_WILDC)
			continue;

		/*
		 * it's a wild character, retrieve the character from the
		 * saved id
		 */
		copyid[i] = saveid[i];

		/*
		 * if we haven't changed anything yet, try to find a new char
		 * to use
		 */
		if (!changed && (saveid[i] < MAXVAL)) {

		/*
		 * Note: this algorithm is taking the "last matched" id
		 * and trying to make a 1 character change to it to create
		 * a new one.  Rather than special-case the first time
		 * (when no perturbation is really necessary), just don't
		 * allocate the first valid id.
		 */

			while (++saveid[i] < MAXVAL) {
				/*
				 * make sure new char is alphanumeric
				 */
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
	/*
	 * changed is true if we were successful in allocating an id
	 */
	if (changed) {
		(void) memcpy(srcid, copyid, IDLEN);
		return (0);
	} else {
		return (-1);
	}
}


/*
 * lockutx - lock utmpx and utmp files
 */
static int
lockutx(void)
{
	if ((fd = open(UTMPX_FILE, O_RDWR|O_CREAT, 0644)) < 0)
		return (-1);

	if ((fd_u = open(UTMP_FILE, O_RDWR|O_CREAT, 0644)) < 0) {
		(void) close(fd);
		fd = -1;
		return (-1);
	}

	if ((lockf(fd, F_LOCK, 0) < 0) || (lockf(fd_u, F_LOCK, 0) < 0)) {
		(void) close(fd);
		(void) close(fd_u);
		fd = -1; fd_u = -1;
		return (-1);
	}

	return (0);
}


/*
 * unlockutx - unlock utmp and utmpx files
 */
static void
unlockutx(void)
{
	(void) lockf(fd, F_ULOCK, 0);
	(void) lockf(fd_u, F_ULOCK, 0);
	(void) close(fd);
	(void) close(fd_u);
	fd = fd_u = -1;
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



static	struct	futmp *getoneut(off_t *);
static	void	putoneutx(const struct utmpx *, off_t);

/*
 * makeutx - create a utmpx entry, recycling an id if a wild card is
 *	specified.  Also notify init about the new pid
 *
 *	args:	utmpx - point to utmpx structure to be created
 */

struct utmpx *
makeutx(const struct utmpx *utmp)
{
	struct utmpx *utp;		/* "current" utmpx being examined */
	struct futmp *ut;		/* "current" utmp being examined */
	int wild;			/* flag, true iff wild card char seen */
	unsigned char saveid[IDLEN];	/* the last id we matched that was */
					/* NOT a dead proc */
	int falphanum = 0x30;		/* first alpha num char */
	off_t offset;

	/*
	 * Are any wild card char's present in the idlen string?
	 */
	wild = (int)memchr(utmp->ut_id, SC_WILDC, IDLEN);

	if (wild) {
		/*
		 * try to lock the utmpx and utmp files, only needed if
		 * we're doing wildcard matching
		 */
		if (lockutx())
			return (NULL);

		/*
		 * used in allocid
		 */
		(void) memset(saveid, falphanum, IDLEN);

		while (ut = getoneut(&offset))
			if (idcmp(utmp->ut_id, ut->ut_id)) {
				continue;
			} else {
				/*
				 * Found a match. We are done if this is
				 * a free slot. Else record this id. We
				 * will need it to generate the next new id.
				 */
				if (ut->ut_type == DEAD_PROCESS)
					break;
				else
					(void) memcpy(saveid, ut->ut_id, IDLEN);
			}

		if (ut) {

			/*
			 * Unused entry, reuse it. We know the offset. So
			 * just go to that offset  utmpx and write it out.
			 * Also update the utmp, wtmp and wtmpx file.
			 */
			(void) memcpy((caddr_t)utmp->ut_id, ut->ut_id, IDLEN);

			/*
			 * convert offset into utmpx file.
			 */
			offset = (offset / sizeof (struct futmp)) *
			    sizeof (struct futmpx);

			putoneutx(utmp, offset);
			updwtmpx(WTMPX_FILE, (struct utmpx *)utmp);
			unlockutx();
			sendpid(ADDPID, (pid_t)utmp->ut_pid);
			return ((struct utmpx *)utmp);
		} else {
			/*
			 * nothing available, allocate an id and
			 * write it out at the end.
			 */

			if (allocid((char *)utmp->ut_id, saveid)) {
				unlockutx();
				return (NULL);
			} else {
				/*
				 * Seek to end and write out the entry
				 * and also update the utmpx file.
				 */
				(void) lseek(fd, 0L, SEEK_END);
				offset = lseek(fd, 0L, SEEK_CUR);

				putoneutx(utmp, offset);
				updwtmpx(WTMPX_FILE, (struct utmpx *)utmp);
				unlockutx();
				sendpid(ADDPID, (pid_t)utmp->ut_pid);
				return ((struct utmpx *)utmp);
			}
		}
	} else {
		utp = pututxline(utmp);
		if (utp)
			updwtmpx(WTMPX_FILE, utp);
		endutxent();
		sendpid(ADDPID, (pid_t)utmp->ut_pid);
		return (utp);
	}
}


#define	UTMPNBUF	200	/* Approx 8k (FS Block) size */
static struct futmp	*utmpbuf = NULL;

/*
 * Buffered read routine to get one entry from utmp file
 */
static struct futmp *
getoneut(off_t *off)
{
	static	size_t idx = 0;	/* Current index in the utmpbuf */
	static	size_t nidx = 0;	/* Max entries in this utmpbuf */
	static	nbuf = 0;	/* number of utmpbufs read from disk */
	ssize_t	nbytes, bufsz = sizeof (struct futmp) * UTMPNBUF;

	if (utmpbuf == NULL)
		if ((utmpbuf = malloc(bufsz)) == NULL) {
			perror("malloc");
			return (NULL);
		}

	if (idx == nidx) {
		/*
		 *	We have read all entries in the utmpbuf. Read
		 *	the buffer from the disk.
		 */
		if ((nbytes = read(fd_u, utmpbuf, bufsz)) < bufsz) {
			/*
			 *	Partial read only. keep count of the
			 *	number of valid entries in the buffer
			 */
			nidx = nbytes / sizeof (struct futmp);
		} else {
			/*
			 *	We read in the full UTMPNBUF entries
			 *	Great !
			 */
			nidx = UTMPNBUF;
		}
		nbuf++;		/* Number of buf we have read in. */
		idx = 0;	/* reset index within utmpbuf */
	}

	/*
	 *	Current offset of this buffer in the file
	 */
	*off = (((nbuf - 1) * UTMPNBUF) + idx) * sizeof (struct futmp);

	if (idx < nidx) {
		/*
		 *	We still have at least one valid buffer in
		 *	utmpbuf to be passed to the caller.
		 */
		return (&utmpbuf[idx++]);
	}

	/*
	 *	Reached EOF. Return NULL. Offset is set correctly
	 *	to append at the end of the file
	 */

	return (NULL);
}

static void
putoneutx(const struct utmpx *utpx, off_t off)
{
	struct	futmp fut;		/* temporary to update utmp file */
	struct	futmpx futx;
	off_t	nstruct;		/* structure number we are writing */

	utmpx_api2frec(utpx, &futx);
	(void) lseek(fd, off, SEEK_SET);	/* seek in the utmpx file */
	(void) write(fd, &futx, sizeof (futx));

	nstruct	= off / sizeof (struct futmpx);
	off = nstruct * sizeof (struct futmp);
	getutmp_frec(&futx, &fut);

	(void) lseek(fd_u, off, SEEK_SET);
	(void) write(fd_u, &fut, sizeof (fut));	/* write utmp file */
}


static int
updateutmp(struct utmpx *entry, off_t offset)
{
	struct	futmp	fut;
	struct	futmpx	futx;
	off_t	nstruct;
	int	utpopen = 0;

	if (fd_u < 0) {
		if ((fd_u = open(utmpfile, O_RDWR|O_CREAT, 0644)) < 0)
			return (1);
		utpopen++;
	}

	nstruct = offset / sizeof (struct futmpx);
	offset = nstruct * sizeof (struct futmp);

	utmpx_api2frec(entry, &futx);
	getutmp_frec(&futx, &fut);

	(void) lseek(fd_u, offset, SEEK_SET);
	(void) write(fd_u, &fut, sizeof (fut));

	if (utpopen)  {
		(void) close(fd_u);
		fd_u = -1;
	}
	return (0);
}

/*
 * sendupid - send message to utmpd to add or remove a pid from the
 *	list of procs to watch
 *
 *	args:	cmd - ADDPID or REMPID
 *		pid - process ID of process to watch
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
