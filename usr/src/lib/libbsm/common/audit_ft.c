#ifndef lint
static char	sccsid[] = "@(#)audit_ftpd.c 1.15 97/12/05 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

#include <sys/types.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/sockio.h>
#include <netinet/in.h>

#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/audit_uevents.h>
#include <bsm/libbsm.h>

#include <locale.h>
#include <pwd.h>
#include <generic.h>

#define	BAD_PASSWD	(1)
#define	UNKNOWN_USER	(2)
#define	EXCLUDED_USER	(3)
#define	NO_ANONYMOUS	(4)
#define	MISC_FAILURE	(5)

extern int	errno;

static char		luser[16];
static struct in_addr	sav_addr;
static int		sav_port;

static void generate_record(char *, int, char *);
static selected(uid_t, char *, au_event_t, int);
static void setup_session(char *);

void
audit_ftpd_sav_data(struct sockaddr_in *sin, int port)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) memcpy(&sav_addr, &sin->sin_addr, sizeof (sav_addr));
	sav_port = port;
}


void
audit_ftpd_bad_pw(char *uname)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strncpy(luser, uname, 8);
	luser[8] = '\0';
	generate_record(luser, BAD_PASSWD, dgettext(bsm_dom,
		"bad password"));
}


void
audit_ftpd_unknown(char	*uname)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strncpy(luser, uname, 8);
	luser[8] = '\0';
	generate_record(luser, UNKNOWN_USER, dgettext(bsm_dom,
		"unknown user"));
}


void
audit_ftpd_excluded(char *uname)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strncpy(luser, uname, 8);
	luser[8] = '\0';
	generate_record(luser, EXCLUDED_USER, dgettext(bsm_dom,
		"excluded user"));
}


void
audit_ftpd_no_anon(void)
{
	if (cannot_audit(0)) {
		return;
	}
	generate_record("", NO_ANONYMOUS, dgettext(bsm_dom,
		"no anonymous"));
}

void
audit_ftpd_failure(char *uname)
{
	if (cannot_audit(0)) {
		return;
	}
	generate_record(uname, MISC_FAILURE, dgettext(bsm_dom,
		"misc failure"));
}

void
audit_ftpd_success(char	*uname)
{
	if (cannot_audit(0)) {
		return;
	}
	(void) strncpy(luser, uname, 8);
	luser[8] = '\0';
	generate_record(luser, 0, "");
	setup_session(luser);
}



static void
generate_record(
		char	*locuser,	/* username of local user */
		int	err,		/* error status (=0 success, >0 error code) */
		char	*msg)		/* error message */
{
	int	rd;		/* audit record descriptor */
	char	buf[256];	/* temporary buffer */
	uid_t	uid;
	gid_t	gid;
	pid_t	pid;
	au_tid_t	tid;
	struct passwd *pwd;
	uid_t	ceuid;		/* current effective uid */

	if (cannot_audit(0)) {
		return;
	}

	pwd = getpwnam(locuser);
	if (pwd == NULL) {
		uid = -1;
		gid = -1;
	} else {
		uid = pwd->pw_uid;
		gid = pwd->pw_gid;
	}

	ceuid = geteuid();	/* save current euid */
	(void) seteuid(0);	/* change to root so you can audit */

	/* determine if we're preselected */
	if (!selected(uid, locuser, AUE_ftpd, err)) {
		(void) seteuid(ceuid);
		return;
	}

	pid = getpid();
	tid.port	 = sav_port;
	tid.machine	 = sav_addr._S_un._S_addr;

	rd = au_open();

	/* add subject token */
	au_write(rd, au_to_subject(uid, uid, gid, uid, gid, pid, pid, &tid));

	/* add return token */
	errno = 0;
	if (err) {
		/* add reason for failure */
		if (err == UNKNOWN_USER)
			(void) sprintf(buf, "%s %s", msg, locuser);
			else
			(void) sprintf(buf, "%s", msg);
		au_write(rd, au_to_text(buf));
#ifdef _LP64
		au_write(rd, au_to_return64(err, (int64_t)-1));
#else
		au_write(rd, au_to_return32(err, (int32_t)-1));
#endif
	} else {
#ifdef _LP64
		au_write(rd, au_to_return64(0, (int64_t)0));
#else
		au_write(rd, au_to_return32(0, (int32_t)0));
#endif
	}

	/* write audit record */
	if (au_close(rd, 1, AUE_ftpd) < 0) {
		au_close(rd, 0, 0);
	}
	(void) seteuid(ceuid);
}


static
selected(
	 uid_t		uid,
	 char		*locuser,
	 au_event_t	event,
	 int	err)
{
	int	rc, sorf;
	char	naflags[512];
	struct au_mask mask;

	mask.am_success = mask.am_failure = 0;
	if (uid < 0) {
		rc = getacna(naflags, 256); /* get non-attrib flags */
		if (rc == 0)
			getauditflagsbin(naflags, &mask);
	} else {
		rc = au_user_mask(locuser, &mask);
	}

	if (err == 0)
		sorf = AU_PRS_SUCCESS;
	else if (err >= 1)
		sorf = AU_PRS_FAILURE;
	else
		sorf = AU_PRS_BOTH;
	rc = au_preselect(event, &mask, sorf, AU_PRS_REREAD);
	return (rc);
}


static void
setup_session(char *locuser)
{
	int	rc;
	struct auditinfo info;
	au_mask_t		mask;
	uid_t			uid;
	struct passwd *pwd;

	pwd = getpwnam(locuser);
	if (pwd == NULL)
		uid = -1;
		else
		uid = pwd->pw_uid;

	info.ai_auid = uid;
	info.ai_asid = getpid();

	mask.am_success = 0;
	mask.am_failure = 0;
	au_user_mask(locuser, &mask);

	info.ai_mask.am_success = mask.am_success;
	info.ai_mask.am_failure = mask.am_failure;

	info.ai_termid.port	 = sav_port;
	info.ai_termid.machine	 = sav_addr._S_un._S_addr;

	rc = setaudit(&info);
	if (rc < 0) {
		perror("setaudit");
	}
}
