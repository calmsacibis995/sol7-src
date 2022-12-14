#ifndef lint
static char	sccsid[] = "@(#)generic.c 1.12 97/12/17 SMI";
#endif

/*
 * Copyright (c) 1993-1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#include <netdb.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/errno.h>
#include <sys/mutex.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <bsm/audit.h>
#include <bsm/libbsm.h>
#include <bsm/audit_uevents.h>
#include <bsm/audit_record.h>

#define	AUC_NEVER	-2	/* audit module not loaded */

/* Private Functions */
static selected(au_event_t, au_mask_t *, int);

int aug_selected();
int aug_na_selected();

/* Global Variables */
static au_id_t		aug_auid;	/* auid of user writing audit record */
static uid_t		aug_uid;	/* uid of user writing audit record */
static uid_t		aug_euid;	/* euid of user writing audit record */
static gid_t		aug_gid;	/* gid of user writing audit record */
static gid_t		aug_egid;	/* euid of user writing audit record */
static pid_t		aug_pid;	/* pid of user writing audit record */
static au_tid_t		aug_tid;	/* tid of user writing audit record */
static int		aug_na;		/* 0 if event is attributable */
static au_mask_t	aug_namask;	/* not attributable flags */
static au_event_t	aug_event;	/* id of event being audited */
static int 		aug_sorf;	/* success or failure of aug_event */
static char *		aug_user;	/* text version of aug_uid */
static char *		aug_text;	/* misc text to be written to trail */
static au_asid_t	aug_asid;	/* asid of process writing record */
static int 		(*aug_afunc)();	/* write additional tokens if needed */
static char *		aug_path;	/* path token */
static int		aug_policy;	/* kernel audit policy */

/*
 * cannot_audit:
 *	Return 1 if audit module not loaded.
 *	Return 0 otherwise.
 *
 * The argument, force, should be set to 1 for long-lived processes
 * like some daemons.  Force should be set to 0 for most programs.
 */
int
cannot_audit(force)
	int force;
{
	static int auc = AUC_UNSET;
	int cond = 0;

	if (auc == AUC_UNSET || force) {
		if (auditon(A_GETCOND, (caddr_t)&cond, sizeof (cond))) {
			auc = AUC_NEVER;
		} else {
			auc = cond;
		}
	}
	return (auc == AUC_NEVER);
}

/*
 * aug_init():
 *	Initialize global variables.
 */
void
aug_init()
{
	aug_auid = -1;
	aug_uid = -1;
	aug_euid = -1;
	aug_gid = -1;
	aug_egid = -1;
	aug_pid = -1;
	aug_tid.port = -1;
	aug_tid.machine = -1;
	aug_namask.am_success = -1;
	aug_namask.am_failure = -1;
	aug_event = 0;
	aug_sorf = -2;
	aug_user = NULL;
	aug_text = NULL;
	aug_na = 0;
	aug_asid = -1;
	aug_afunc = NULL;
	aug_path = NULL;
}

/*
 * aug_get_port:
 *	Return the raw device number of the port to which the
 *	current process is attached (assumed to be attached
 *	through file descriptor 0) or 0 if can't stat the port.
 */
dev_t
aug_get_port()
{
	int	rc;
	char	*ttyn;
	struct stat64 sb;

	ttyn = (char *) ttyname(0);
	if (ttyn == 0 || *ttyn == '\0') {
		return (0);
	}

	rc = stat64(ttyn, &sb);
	if (rc < 0) {
		perror("stat");
		return (0);
	}

	return ((dev_t) sb.st_rdev);
}

/*
 * aug_get_machine:
 *	Return internet address of host hostname,
 *	or 0 if can't do lookup.
 */
in_addr_t
aug_get_machine(char *hostname)
{
	in_addr_t	mach;
	struct hostent *hostent;

	hostent = gethostbyname(hostname);
	if (hostent == 0) {
		perror("gethostbyname");
		return (0);
	}

	(void) memcpy(&mach, hostent->h_addr_list[0], sizeof (mach));

	return (mach);
}

void
aug_save_auid(au_id_t id)
{
	aug_auid = id;
}

void
aug_save_uid(uid_t id)
{
	aug_uid = id;
}

void
aug_save_euid(uid_t id)
{
	aug_euid = id;
}

void
aug_save_gid(gid_t id)
{
	aug_gid = id;
}

void
aug_save_egid(gid_t id)
{
	aug_egid = id;
}

void
aug_save_pid(pid_t id)
{
	aug_pid = id;
}

void
aug_save_asid(au_asid_t id)
{
	aug_asid = id;
}

void
aug_save_afunc(int (*afunc)())
{
	aug_afunc = afunc;
}

void
aug_save_tid(dev_t port, int machine)
{
	aug_tid.port = port;
	aug_tid.machine = machine;
}

int
aug_save_me(void)
{
	auditinfo_t ai;

	if (getaudit(&ai))
		return (-1);

	aug_save_auid(ai.ai_auid);
	aug_save_euid(geteuid());
	aug_save_egid(getegid());
	aug_save_uid(getuid());
	aug_save_gid(getgid());
	aug_save_pid(getpid());
	aug_save_asid(ai.ai_asid);
	aug_save_tid(ai.ai_termid.port, ai.ai_termid.machine);

	return (0);
}

/*
 * aug_save_namask():
 *	Save the namask using the naflags entry in the audit_control file.
 *	Return 0 if successful.
 *	Return -1, and don't change the namask, if failed.
 *	Side Effect: Sets aug_na to -1 if error, 1 if successful.
 */
int
aug_save_namask()
{
	char naflags[256];	/* audit flags from na entry of audit_control */
	au_mask_t mask;

	aug_na = -1;

	/*
	 * get non-attributable system event mask from kernel.
	 */
	if (auditon(A_GETKMASK, (caddr_t)&mask, sizeof (mask)) != 0) {
		return (-1);
	}

	aug_namask.am_success = mask.am_success;
	aug_namask.am_failure = mask.am_failure;
	aug_na = 1;
	return (0);
}

void
aug_save_event(au_event_t id)
{
	aug_event = id;
}

void
aug_save_sorf(int sorf)
{
	aug_sorf = sorf;
}

void
aug_save_text(char *s)
{
	if (aug_text != NULL)
		free(aug_text);
	if (s == NULL)
		aug_text = NULL;
	else
		aug_text = strdup(s);
}

void
aug_save_na(int flag)
{
	aug_na = flag;
}

void
aug_save_user(char *s)
{
	if (aug_user != NULL)
		free(aug_user);
	if (s == NULL)
		aug_user = NULL;
	aug_user = strdup(s);
}

void
aug_save_path(char *s)
{
	if (aug_path != NULL)
		free(aug_path);
	if (s == NULL)
		aug_path = NULL;
	aug_path = strdup(s);
}

int
aug_save_policy()
{
	int policy;

	if (auditon(A_GETPOLICY, (caddr_t)&policy, sizeof (policy))) {
		return (-1);
	}
	aug_policy = policy;
	return (0);
}

/*
 * aug_audit:
 *	Cut and audit record if it is selected.
 *	Return 0, if successfully written.
 *	Return 0, if not written, and not expected to write.
 *	Return -1, if not written because of unexpected error.
 */
int
aug_audit(void)
{
	int ad;

	if (cannot_audit(0)) {
		return (0);
	}

	if (aug_na) {
		if (!aug_na_selected()) {
			return (0);
		}
	} else if (!aug_selected()) {
		return (0);
	}

	if ((ad = au_open()) == -1) {
		return (-1);
	}

	au_write(ad, au_to_subject(aug_auid, aug_euid, aug_egid,
		aug_uid, aug_gid, aug_pid, aug_asid, &aug_tid));
	if (aug_policy & AUDIT_GROUP) {

		int ng;
		gid_t grplst[NGROUPS_MAX];

		memset(grplst, 0, sizeof (grplst));
		if ((ng = getgroups(NGROUPS_UMAX, grplst))) {
			au_write(ad, au_to_newgroups(ng, grplst));
		}
	}
	if (aug_text != NULL) {
		au_write(ad, au_to_text(aug_text));
	}
	if (aug_path != NULL) {
		au_write(ad, au_to_path(aug_path));
	}
	if (aug_afunc != NULL) {
		(*aug_afunc)(ad);
	}
#ifdef _LP64
	au_write(ad, au_to_return64(aug_sorf,
					(int64_t)((aug_sorf == 0) ? 0 : -1)));
#else
	au_write(ad, au_to_return32(aug_sorf,
					(int32_t)((aug_sorf == 0) ? 0 : -1)));
#endif
	if (au_close(ad, 1, aug_event) < 0) {
		au_close(ad, 0, 0);
		return (-1);
	}

	return (0);
}

int
aug_na_selected()
{
	if (aug_na == -1) {
		return (-1);
	}

	return (selected(aug_event, &aug_namask, aug_sorf));
}

int
aug_selected()
{
	struct passwd *pw;
	char *user;
	auditinfo_t mask;

	if (aug_uid < 0) {
		aug_save_namask();
		return (aug_na_selected());
	}

	if (aug_user != NULL) {
		user = aug_user;
	} else {
		if ((pw = getpwuid(aug_uid)) == NULL) {
			return (-1);
		}
		user = pw->pw_name;
	}

	if (getaudit(&mask)) {
		return (-1);
	}

	return (selected(aug_event, &mask.ai_mask, aug_sorf));
}

static
selected(au_event_t e, au_mask_t *m, int sorf)
{
	int prs_sorf;

	if (sorf == 0) {
		prs_sorf = AU_PRS_SUCCESS;
	} else if (sorf == -1) {
		prs_sorf = AU_PRS_FAILURE;
	} else {
		prs_sorf = AU_PRS_BOTH;
	}

	return (au_preselect(e, m, prs_sorf, AU_PRS_REREAD));
}
