/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)unix_update_authtok.c	1.8	97/10/09 SMI"

#include <deflt.h>		/* for defopen, defread */
#include "unix_headers.h"

#ifdef PAM_SECURE_RPC
static bool_t	verify_rpc_passwd(char *, nis_result *, bool_t *, int);
extern bool_t	extract_sec_keyinfo(nis_object *, char **, char **, keylen_t *,
				    algtype_t *, int);
#endif

#ifdef PAM_NISPLUS
static int	get_nispluscred(pam_handle_t *, char *, char *, int,
				struct passwd *, nis_result **);
#endif

static int	verify_old_passwd(pam_handle_t *, int, int, char *,
			struct spwd *, char *, char *,
			int, void *, uid_t, int *, int *, int, int, bool_t *);
static int	turn_on_default_aging(pam_handle_t *, int, char *, int);
static int	get_newpasswd(pam_handle_t *, char *, int, char *,
			int, uid_t, int, int, int, int);
static int	circ(char *, char *);
static int	triviality_checks(pam_handle_t *, uid_t,
			char *, char *, int, int, int);

/*
 * __update_authtok():
 *	To change authentication token.
 *
 * 	This function calls ck_perm() to check the caller's
 *	permission.  If the check succeeds, it will then call
 *	verify_old_passwd() to validate the old password and password
 *	aging information.  If verify_old_passwd() succeeds, get_newpasswd()
 *	will then be called to get and check the user's new passwd.
 *	Last, update_authtok_<repository>() will be called to change the user's
 *	password to the new password.
 *
 *	All temporary password buffers allocate PAM_MAX_RESP_SIZE bytes.
 *	This is because the longest value (and hence password) that can be
 *	returned by a PAM conv function is PAM_MAX_RESP_SIZE bytes.
 *
 *	The "passwd" command bypasses PAM and calls this function directly
 *	if the -r (repository) or -D (domain) options are specified
 *	on the command line.
 */

int
__update_authtok(
	pam_handle_t		*pamh,
	int			flags,
	int			repository,
	char			*domain,
	int			argc,
	const char		**argv)
{

	int		retcode;
	int		i, j, c, done = 0;
	time_t 		salt;
	char 		saltc[2];	/* crypt() takes 2 char */
					/* string as a salt */
	char		*pw = NULL;	/* encrypted new password */
	struct passwd	*pwd = NULL;
	struct spwd	*shpwd = NULL;
	char 		*usrname = NULL;
	char 		*prognamep = NULL;
	int		re;		/* single repository */
	int		newpass = 0;	/* flag: user entered new passwd */
	int		found_user = 0;	/* flag: user found in a repository */
	int		privileged = 0;	/* privileged nis or nisplus user */
	uid_t		uid;		/* real uid of calling process */
	char	orpcpw[PAM_MAX_RESP_SIZE] = {'\0'};	/* old rpc passwd */
	char	opwbuf[PAM_MAX_RESP_SIZE] = {'\0'};	/* old passwd */
	char	pwbuf[PAM_MAX_RESP_SIZE] = {'\0'};	/* new passwd */
	char		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int		debug = 0;
	int		nowarn = 0;
	int		try_first_pass = 0;
	int		use_first_pass = 0;
	char		*old_aging = 0;		/* find SunOS aging info */
#ifdef PAM_NISPLUS
	nis_result	*passwd_res;
	nis_result	*cred_res = NULL;	/* cred entry nis_list result */
	bool_t		pk_creds_exist;		/* any pub key creds exist? */
#else
	void		*passwd_res = NULL, *cred_res = NULL;
#endif
	int		max = -1, min = -1, lastchg = -1;

	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "debug") == 0)
			debug = 1;
		else if (strcmp(argv[i], "nowarn") == 0)
			nowarn = 1;
		else if (strcmp(argv[i], "try_first_pass") == 0)
			try_first_pass = 1;
		else if (strcmp(argv[i], "use_first_pass") == 0)
			use_first_pass = 1;
		else
			syslog(LOG_ERR,
				"UNIX chauthtok(): illegal module option %s",
				argv[i]);
	}

	if (debug) {
		syslog(LOG_DEBUG, "unix_sm_chauthtok(): update passwords");
		syslog(LOG_DEBUG, "unix_sm_chauthtok: %s",
			repository_to_string(repository));
		syslog(LOG_DEBUG, "unix_sm_chauthtok: uid = %d, euid = %d",
			getuid(), geteuid());
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
						!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
						!= PAM_SUCCESS) {
		goto out;
	}

	/*
	 * get_ns() will consult nsswitch file and set the repository
	 * accordingly.
	 * If specified repository is not defined in switch file, a warning
	 * is printed, e.g. -r nis while "passwd: files, nis+" is defined
	 * in switch file.
	 */
	repository = get_ns(pamh, repository, debug, nowarn);
	if (repository == -1) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}
	if (debug)
		syslog(LOG_DEBUG,
		"unix pam_sm_chauthtok(): repository: %s after get_ns()",
			repository_to_string(repository));

	/* if we still get PAM_REP_DEFAULT after calling get_ns(), error! */
	if (repository == PAM_REP_DEFAULT) {
		retcode = PAM_AUTHTOK_ERR;
		goto out;
	}

#ifdef PAM_NISPLUS
	/* nispasswd or -r nisplus */
	if ((repository & PAM_REP_NISPLUS) && ((domain == NULL) ||
	    (*domain == '\0')))
		domain = nis_local_directory();
#endif

	/*
	 * XXX:
	 * For this module to work, real UID must be user's UID,
	 * effective UID must be 0.
	 *
	 * If the effective UID is not 0, then we can not open
	 * the shadow file (in ck_perm()).
	 * If the real UID is not the user's UID, then we have no
	 * idea who called passwd.  We need to know who invoked
	 * passwd because special priviliges are given to the root
	 * user (root can change anyone's password in FILES).
	 *
	 * If the service is login, rlogin, telnet, or dtlogin, then
	 * we query the password file for the uid of the user.
	 *
	 * Typically a service module should not use the service name
	 * in this fashion.  The service name should only be used when
	 * printing out error or text information.
	 */
	if ((strcmp(prognamep, "passwd") == 0)) {
		uid = getuid();
	} else {
		/*
		 * Service is login, rlogin, etc.
		 */
		struct passwd pd;
		char pwd_buf[1024];
		if (getpwnam_r(usrname, &pd, pwd_buf, sizeof (pwd_buf)) == NULL)
			return (PAM_USER_UNKNOWN);
		uid = pd.pw_uid;
	}

	/*
	 * We need to call ck_perm, verify_old_passwd, and get_newpasswd
	 * for each repository that the user may be in (nisplus, nis, files).
	 * The first time thru the loop handles NISPLUS.
	 * The second time thru the loop handles NIS.
	 * The third time thru handles FILES.
	 *
	 * We want to update remote repositories first.  If they fail
	 * we won't update local FILES - thus leaving a consistent
	 * state across the system.
	 */
	for (j = 1; j <= 3; j++) {
		switch (j) {
		case 1:
			/* first time thru loop, NISPLUS */
			re = PAM_REP_NISPLUS;
			break;
		case 2:
			/* 2nd time thru loop, NIS */
			re = PAM_REP_NIS;
#ifdef PAM_NISPLUS
			if (repository & PAM_REP_NISPLUS) {
				/*
				 * Nisplus case sets euid to user.
				 * Set it back to root now.
				 */
				seteuid(0);
				setreuid(uid, 0);
			}
#endif /* PAM_NISPLUS */
			break;
		case 3:
			/* 3rd time thru loop, FILES */
			re = PAM_REP_FILES;
			done = 1;
			break;
		}

		if ((repository & re) == 0) {
			/* the repository is not specified in nsswitch */

			if (j == 3 && !found_user) {
				if (!nowarn) {
					snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 60,
					"%s%s:  %s does not exist"),
					prognamep, UNIX_MSG, usrname);
					(void) __pam_display_msg(
						pamh,
						PAM_ERROR_MSG, 1,
						messages, NULL);
				}
				retcode = PAM_USER_UNKNOWN;
				goto out;
			}
			continue;
		}

		retcode = ck_perm(pamh, re,
			(char *)domain, &pwd, &shpwd, &privileged,
			(void **)&passwd_res, uid, debug, nowarn);
		switch (retcode) {
		case PAM_SUCCESS:
			found_user = 1;
			break;
		case PAM_USER_UNKNOWN:
			/*
			 * Although the repository was listed in
			 * nsswitch.conf, the user may not be in that
			 * repository -- check all repositories before
			 * quitting
			 */
			if (done) {
				if (found_user == 1) {
					retcode = PAM_SUCCESS;
					goto out;
				} else {
					if (!nowarn) {
						snprintf(messages[0],
							sizeof (messages[0]),
							PAM_MSG(pamh, 60,
						"%s%s: %s does not exist"),
							prognamep,
							UNIX_MSG,
							usrname);
						(void) __pam_display_msg(
							pamh,
							PAM_ERROR_MSG, 1,
							messages, NULL);
					}
					goto out;
				}
			}

			/* user not in this repository -- check the others */
			continue;
		default:
			/* all other errors */
			goto out;
		}

		/* check for old SunOS aging information */
		if (shpwd->sp_pwdp &&
		    (old_aging = (strchr(shpwd->sp_pwdp, ',')))) {

			int now = DAY_NOW;

			if ((retcode = decode_passwd_aging
					(old_aging,
					&max,
					&min,
					&lastchg)) != PAM_SUCCESS)
				goto out;

			/* only sysadmin can change passwd */
			if (min > max) {
				if (uid != 0 && privileged == 0) {
					retcode = PAM_PERM_DENIED;
					goto out;
				}
			}

			/* see if enough time elapsed since last change */
			if (min > 0 && now - lastchg < min) {
				retcode = PAM_PERM_DENIED;
				goto out;
			}

			/* remove aging field so we can verify the old passwd */
			*old_aging = '\0';
		}

#ifdef PAM_NISPLUS
		if (IS_NISPLUS(repository)) {

			char *p;	/* nispasswd */

			/*
			 * We must use an authenticated handle to get the cred
			 * table information for the user we want to modify the
			 * cred info for. If we can't even read that info, we
			 * definitely wouldn't have modify permission. Well..
			 */
			retcode = get_nispluscred(pamh, domain,
				prognamep, repository, pwd, &cred_res);
			if (retcode != PAM_SUCCESS)
				goto out;

			if ((cred_res == NULL) ||
			    (cred_res->status != NIS_SUCCESS)) {
				if (IS_OPWCMD(repository)) {
				    if (!nowarn) {
					(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 61,
					"%s%s: user must have NIS+ credential"),
						prognamep, UNIX_MSG);
					(void) __pam_display_msg(
						pamh,
						PAM_ERROR_MSG, 1,
						messages, NULL);
				    }
				    retcode = PAM_PERM_DENIED;
				    goto out;
				}
				/* continue even if there is no cred */
			}
		}
#endif /* PAM_NISPLUS */

		/* verify the old password */
		retcode = verify_old_passwd(pamh,
			re, repository, (char *)domain, shpwd,
			opwbuf, orpcpw,
			privileged, (void *)cred_res, uid,
			&try_first_pass, &use_first_pass,
			debug, nowarn, &pk_creds_exist);
		if (retcode != PAM_SUCCESS)
			goto out;

		/*
		 * verify_old_passwd() may have modified try_first_pass
		 * and use_first_pass.  This is to avoid get_newpasswd()
		 * from using the new password with use/try_first_pass
		 * when the old password failed.
		 *
		 * Once we get the new passwd for remote repository
		 * (nis or nis+), we don't need to prompt again for
		 * local repository (files).
		 * The default behavior is to keep passwds in sync.
		 * However, we check old passwds in both repositories.
		 */

		if (!newpass) {
			retcode = get_newpasswd(pamh, pwbuf, re,
				opwbuf, privileged, uid,
				try_first_pass, use_first_pass, debug, nowarn);
			if (retcode != PAM_SUCCESS)
				goto out;

			/* we have a new passwd from user */
			newpass = 1;
		}

		/* Construct salt, then encrypt the new password */
		(void) time((time_t *)&salt);
		salt += (long)getpid();

		saltc[0] = salt & 077;
		saltc[1] = (salt >> 6) & 077;
		for (i = 0; i < 2; i++) {
			c = saltc[i] + '.';
			if (c > '9') c += 7;
			if (c > 'Z') c += 6;
			saltc[i] = c;
		}
		pw = crypt(pwbuf, saltc);

		/*
		 * we do not have to replace the old SunOS aging
		 * information if present originally. in the NIS and NIS+
		 * cases, the server side adds the SunOS aging information
		 * for us.  in the FILES case, we're using the local
		 * /etc/shadow file and we're an SVR4 OS so there is
		 * no old SunOS aging information there.
		 */

		/* update remote repositories first */
		/* make sure user exists before we update the repository */
#ifdef PAM_NIS
		if (IS_NIS(re) && (pwd != NULL)) {
			retcode = update_authtok_nis(pamh,
				"passwd", &pw, opwbuf, pwbuf,
				pwd, privileged, nowarn);
			if (retcode != PAM_SUCCESS) {
				free_passwd_structs(pwd, shpwd);
				goto out;
			}
		}
#endif /* PAM_NIS */

#ifdef PAM_NISPLUS
		/*
		 * nis+ update will choose different protocol depending
		 * on the opwcmd flag.
		 */
		if (IS_NISPLUS(re) && (pwd != NULL)) {
			/* nis+ needs clear versions of old/new passwds */
			if (orpcpw[0] == '\0') {
				if (strlen(opwbuf) + 1 <= PAM_MAX_RESP_SIZE) {
					(void) strncpy(orpcpw, opwbuf,
						strlen(opwbuf)+1);
				} else {
					retcode = PAM_BUF_ERR;
					free_passwd_structs(pwd, shpwd);
					goto out;
				}
			}
			retcode = update_authtok_nisplus(pamh,
				(char *)domain, "passwd", &pw,
				opwbuf, orpcpw, pwbuf,
				IS_OPWCMD(re) ? 1 : 0, pwd,
				privileged, passwd_res, cred_res,
				debug, nowarn, pk_creds_exist);
			if (retcode != PAM_SUCCESS) {
				free_passwd_structs(pwd, shpwd);
				goto out;
			}
		}
#endif /* PAM_NISPLUS */

		if (IS_FILES(re) && (pwd != NULL)) {
			retcode = update_authtok_file(pamh,
				"passwd", &pw, pwd,
				privileged, nowarn);
			if (retcode != PAM_SUCCESS) {
				free_passwd_structs(pwd, shpwd);
				goto out;
			}
		}
		free_passwd_structs(pwd, shpwd);
	} /* for */

	retcode = PAM_SUCCESS;
out:
	memset(pwbuf, 0, sizeof (pwbuf));
	memset(orpcpw, 0, PAM_MAX_RESP_SIZE);
	memset(opwbuf, 0, PAM_MAX_RESP_SIZE);
	if (pw) {
		/* paranoia */
		memset(pw, 0, strlen(pw));
	}
#ifdef PAM_NISPLUS
	if (cred_res)
		nis_freeresult(cred_res);
#endif

	/*
	 * If invoked from login, rlogin, etc,
	 * passwd module exits with:
	 *	uid = 0		euid = 0
	 *
	 * If invoked from passwd command,
	 * passwd module exits with:
	 *	uid = usrname UID	euid = 0
	 */
	if (strcmp("passwd", prognamep) != 0) {
		setuid(0);
	}

	/* audit_passwd_main10(retval); */
	return (retcode);
}


/*
 * verify_old_passwd():
 * 	To verify user old password. It also checks the
 * 	password aging information to verify that user is authorized
 * 	to change password.
 */

static int
verify_old_passwd(
	pam_handle_t *pamh,
	int repository,
	int real_rep,		/* repositories in nsswitch.conf */
	char *domain,		/* NIS+ domain */
	struct spwd *shpwd,
	char *opwbuf,		/* old password */
	char *orpcpw,		/* old RPC password */
	int privileged,		/* NIS+ privileged user? */
	void *cred_res,		/* cred entry nis_list result */
	uid_t uid,
	int *try_first_pass,
	int *use_first_pass,
	int debug,
	int nowarn,
	bool_t *pk_creds_exist)	/* pubkey creds exist at all? (out) */
{
	register int		now;
	int			done = 0;
	int			retcode;
	char			*prognamep;
	char 			messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char			*pswd = NULL;	/* user input clear passwd */
	char			*pw;		/* encrypted passwd */
	pid_t			pid;
	int			w;
	char			prompt[PAM_MAX_MSG_SIZE];
	/* Set if password already exists as pam handle item */
	int			passwd_already_set = 0;
	char			*saved_passwd = NULL;

	if (debug)
		syslog(LOG_DEBUG,
		"PAM: verify_old_passwd: start: uid = %d, pk_creds_exist = %d",
			uid, *pk_creds_exist);

	if (debug) {
		syslog(LOG_DEBUG,
			"verify_old_passwd(): repository is %s",
			repository_to_string(repository));
		syslog(LOG_DEBUG,
			"    try_first_pass = %d, use_first_pass = %d",
			*try_first_pass, *use_first_pass);
	}

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
							!= PAM_SUCCESS)
		return (retcode);

	/*
	 * Make sure repository is in range, and there is a shadow passwd.
	 * If the repository is FILES, only prompt for old password
	 * if user is not the root user.
	 */
	if (shpwd->sp_pwdp[0] &&
		((uid != 0 && IS_FILES(repository)) ||
		(IS_NIS(repository) && !privileged) ||
		IS_NISPLUS(repository))) {

		/*
		 * For nis+, prompt for passwd even if it's a privileged user.
		 * However to maintain the old nispasswd behavior, skip
		 * this part if it's a privileged user.
		 */
		if (IS_NISPLUS(repository) && IS_OPWCMD(real_rep) &&
		    privileged == 1) {
			opwbuf[0] = '\0';
			goto aging;
		}

		if (opwbuf && opwbuf[0] != '\0') {
			/* user already input passwd */
			pw = crypt(opwbuf, shpwd->sp_pwdp);
			if (strcmp(shpwd->sp_pwdp, pw) == 0) {
				/* same old passwd: no need to check again */
				goto aging;
			}
		}


		/*
		 * See if there's a password already saved in the
		 * pam handle.  If no, then if we prompt for a password,
		 * it will be saved in the handle by __pam_get_authtok().
		 * We will want to clear this password from the handle
		 * if it is incorrect before we return.
		 */
		pam_get_item(pamh, PAM_OLDAUTHTOK, (void **)&saved_passwd);
		if (saved_passwd != NULL)
			passwd_already_set = 1;

		/*
		 * The only time this loop will iterate more than once is
		 * if try_first_pass is set and the first password is not
		 * correct so we must prompt the user for a new one.
		 */
		while (!done) {
			if (*try_first_pass || *use_first_pass) {
				__pam_get_authtok(pamh, PAM_HANDLE,
					PAM_OLDAUTHTOK, PASSWORD_LEN, 0, &pswd);
			} else {
				memset(prompt, 0, PAM_MAX_MSG_SIZE);
				if (IS_NIS(repository))
					strcpy(prompt, PAM_MSG(pamh, 62,
						"Enter login(NIS) password: "));
				else if (IS_NISPLUS(repository))
					strcpy(prompt, PAM_MSG(pamh, 63,
					"Enter login(NIS+) password: "));
				else
					strcpy(prompt, PAM_MSG(pamh, 64,
						"Enter login password: "));

				retcode = __pam_get_authtok(pamh, PAM_PROMPT,
					PAM_OLDAUTHTOK, PASSWORD_LEN,
					prompt, &pswd);
				if (retcode != PAM_SUCCESS)
					return (retcode);
			}

			if (pswd == NULL || pswd[0] == '\0') {
				if (*try_first_pass) {
					/*
					 * This means that the module has
					 * try_first_pass set, but there was no
					 * previous module that ever prompted
					 * for a password.  Go back and prompt
					 * for a password.
					 */
					*try_first_pass = 0;
					continue;
				}

				(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 65, "%s%s: Sorry."),
					prognamep, UNIX_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);

				if (pswd)
					free(pswd);

				*use_first_pass = 0;
				return (PAM_AUTHTOK_ERR);
			} else {
				(void) strncpy(opwbuf, pswd, PAM_MAX_RESP_SIZE);
				pw = crypt(opwbuf, shpwd->sp_pwdp);

				memset(pswd, 0, strlen(pswd));
				free(pswd);
			}

			/*
			 * To really want to maintain the behavior of yppasswd,
			 * add a check of PAM_OPWCMD here for PAM_REP_NIS.
			 * Allow the user to change passwd to the master
			 * consecutively. It doesn't check the passwd
			 * at the server that it's bound to. (which may not be
			 * in sync with the master all the time).
			 */

			/*
			 * Privileged user may update someone else's passwd.
			 * It doesn't make sense to compare admin's passwd to
			 * the regular user's passwd.
			 */
			if ((privileged == 0) &&
			    strcmp(pw, shpwd->sp_pwdp) != 0) {
				if (*try_first_pass) {
					/*
					 * We just attempted the first
					 * password and it failed.  Go
					 * back and prompt for the old
					 * password.
					 */
					*try_first_pass = 0;
					continue;
				} else {
					(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 66,
						"%s%s: Sorry, wrong passwd"),
						prognamep, UNIX_MSG);
					(void) __pam_display_msg(pamh,
						PAM_ERROR_MSG, 1, messages,
						NULL);
					*use_first_pass = 0;

					/* clear bad passwd */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_OLDAUTHTOK,
							NULL);

					return (PAM_PERM_DENIED);
				}
			}

			/* At this point the old password has been verified. */
			done = 1;
		} /* while */

#ifdef PAM_NISPLUS
		/* nispasswd or -r nisplus */
		if ((privileged == 0) && IS_NISPLUS(repository) &&
		    ((nis_result *)cred_res != NULL) &&
		    ((nis_result *)cred_res)->status == NIS_SUCCESS) {
		/*
		 * At this point only check if the password matches the
		 * one in passwd table. If it doesn't, don't even bother
		 * about seeing if it can decrypt the secretkey in cred table.
		 * If it does, attempt to decrypt the old secretkey with it.
		 * This only makes sense when users have credentials.
		 */
			if (verify_rpc_passwd(opwbuf, cred_res,
						pk_creds_exist, debug)
								== FALSE) {
				/* failed to decrypt the secret key */
				/* ask for Old secure RPC passwd */
				char *tmpkeybuf = NULL;

				(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 67,
		    "This password differs from your secure RPC password."));
				(void) __pam_display_msg(pamh, PAM_TEXT_INFO,
					1, messages, NULL);

				if ((retcode = __pam_get_authtok(pamh,
					PAM_PROMPT, 0, PASSWORD_LEN,
	PAM_MSG(pamh, 68, "Please enter your old Secure RPC password: "),
					&tmpkeybuf)) != PAM_SUCCESS) {
					return (retcode);
				}

				if (verify_rpc_passwd(tmpkeybuf, cred_res,
							pk_creds_exist, debug)
								== FALSE) {
					(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 69,
"This password does not decrypt your secure RPC credentials. Try again:"));
					(void) __pam_display_msg(
						pamh, PAM_ERROR_MSG, 1,
						messages, NULL);

					memset(tmpkeybuf, 0, strlen(tmpkeybuf));
					free(tmpkeybuf);
					if ((retcode = __pam_get_authtok(pamh,
						PAM_PROMPT, 0, PASSWORD_LEN,
	PAM_MSG(pamh, 70, "Please enter your old Secure RPC password: "),
						&tmpkeybuf)) != PAM_SUCCESS) {
						return (retcode);
					}

					if (verify_rpc_passwd(tmpkeybuf,
						cred_res, pk_creds_exist, debug)
					    == FALSE) {
						(void) snprintf(messages[0],
							sizeof (messages[0]),
							PAM_MSG(pamh, 65,
							"%s%s: Sorry."),
							prognamep, UNIX_MSG);
						(void) __pam_display_msg(
						    pamh,
						    PAM_ERROR_MSG, 1,
						    messages, NULL);
					    memset(tmpkeybuf, 0,
						strlen(tmpkeybuf));
					    free(tmpkeybuf);
					    return (PAM_PERM_DENIED);
					}
				}
				(void) strcpy(orpcpw, tmpkeybuf);
				memset(tmpkeybuf, 0, strlen(tmpkeybuf));
				free(tmpkeybuf);
			}
		/* continue to construct a new passwd */
		}
#endif /* PAM_NISPLUS */
	} else {
		opwbuf[0] = '\0';
		orpcpw[0] = '\0';
	}
aging:

	/* YP doesn't support aging. */
	if (IS_NIS(repository))
		return (PAM_SUCCESS);

	/* password age checking applies */
	if (shpwd->sp_max != -1 && shpwd->sp_lstchg != 0) {
		/* If password aging is turned on and the password last */
		/* change date is set */
		now  =  DAY_NOW;
		if (shpwd->sp_lstchg <= now) {
			if (((uid != 0 && IS_FILES(repository)) ||
			    (privileged == 0 && IS_NISPLUS(repository))) &&
			    (now < shpwd->sp_lstchg  + shpwd->sp_min)) {
				if (!nowarn) {
					(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 71,
		"%s%s: Sorry: less than %ld days since the last change."),
					prognamep, UNIX_MSG, shpwd->sp_min);
					(void) __pam_display_msg(
						pamh,
						PAM_ERROR_MSG, 1,
						messages, NULL);
				}
				return (PAM_PERM_DENIED);
			}
			if (shpwd->sp_min > shpwd->sp_max) {
				if ((IS_FILES(repository) && uid != 0) ||
				    (IS_NISPLUS(repository) &&
				    (privileged == 0))) {
					if (!nowarn) {
						(void) snprintf(messages[0],
							sizeof (messages[0]),
							PAM_MSG(pamh, 72,
				"%s%s: You may not change this password."),
						prognamep, UNIX_MSG);
						(void) __pam_display_msg(
							pamh,
							PAM_ERROR_MSG, 1,
							messages, NULL);
					}
					return (PAM_PERM_DENIED);
				}
			}
		}
	} else {
		if (shpwd->sp_lstchg == 0 &&
		    (shpwd->sp_max > 0 || shpwd->sp_min > 0)) {
			/* If password aging is turned on */
			return (PAM_SUCCESS);
		} else {
			/* aging not turned on */
			/* so turn on passwd for user with default values */
			/*
			 * Because we need to run the current process as
			 * regular user for nis+ request, we don't want
			 * to become root at this point. However, in order
			 * to turn on default aging for files, we have
			 * to have root privilege. Thus, we let the child
			 * have root privilege while the current process
			 * remains as regular user process. Refer to ck_perm()
			 * for related uid manipulation.
			 */
			switch (pid = fork()) {
			case -1:
				if (!nowarn) {
					(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 73,
				    "%s%s: can't create process"),
						prognamep, UNIX_MSG);
					(void) __pam_display_msg(
						pamh, PAM_ERROR_MSG, 1,
						messages, NULL);
				}
				return (PAM_SYSTEM_ERR);
			case 0: /* child */
				(void) seteuid(0);
				retcode = turn_on_default_aging(pamh,
				    repository, domain, debug);
				exit(retcode);
			default:
				/* wait for child */
				while ((w = (int)waitpid(pid, &retcode, 0))
				    != pid && w != -1)
					;
				return ((w == -1) ? w : retcode);
			}
		}
	}
	return (PAM_SUCCESS);
}


/*
 * turn_on_default_aging():
 * 	Turn on the default password aging
 */

static int
turn_on_default_aging(pamh, repository, domain, debug)
	pam_handle_t *pamh;
	int repository;
	char *domain;
	int debug;
{
	char			value[PAM_MAX_ATTR_SIZE];
	char			*char_p;
	char			*set_attribute[PAM_MAX_NUM_ATTR];
	int			retcode;
	int			k;
	int 			mindate;	/* password aging information */
	int 			maxdate;	/* password aging information */
	int 			warndate;	/* password aging information */

	/* can't set network policy locally */
	if (IS_NISPLUS(repository))
		return (PAM_SUCCESS);

	/* We only process local files. Skip anything else. */
	if (!IS_FILES(repository))
		return (PAM_PERM_DENIED);

	k = 0;

	/*
	 * Open "/etc/default/passwd" file,
	 * if password administration file can't be opened
	 * use built in defaults.
	 */
	if ((defopen(PWADMIN)) != 0) { /* M005  start */
		mindate = MINWEEKS * 7;
		maxdate = MAXWEEKS * 7;
		warndate = WARNWEEKS * 7;
	} else {
		/* get minimum date before password can be changed */
		if ((char_p = defread("MINWEEKS=")) == NULL)
			mindate = 7 * MINWEEKS;
		else {
			mindate = 7 * atoi(char_p);
			if (mindate < 0)
				mindate = 7 * MINWEEKS;

			if (mindate == 0)
				mindate = -1;
		}

		/* get warn date before password is expired */
		if ((char_p = defread("WARNWEEKS=")) == NULL)
			warndate = 7 * WARNWEEKS;
		else {
			warndate = 7 * atoi(char_p);
			if (warndate < 0)
				warndate = 7 * WARNWEEKS;
		}

		/* get max date that password is valid */
		if ((char_p = defread("MAXWEEKS=")) == NULL)
			maxdate = 7 * MAXWEEKS;
		else if ((maxdate = atoi(char_p)) == -1) {
			mindate = -1;
			warndate = -1;
		} else if (maxdate < -1 || maxdate == 0)
				maxdate = 7 * MAXWEEKS;
			else
				maxdate *= 7;

		/* close defaults file */
		defopen(NULL);
	}

	if (debug)
		syslog(LOG_DEBUG,
		"turn: maxdate == %d, mindate == %d\n", maxdate, mindate);

	/* set up the attribute/value pairs, then call pam_set_authtokattr() */
	/* to change the attribute values				    */

	(void) snprintf(value, sizeof (value), "%d", maxdate);
	setup_attr(set_attribute, k++, "AUTHTOK_MAXAGE=", value);
	(void) snprintf(value, sizeof (value), "%d", mindate);
	setup_attr(set_attribute, k++, "AUTHTOK_MINAGE=", value);
	(void) snprintf(value, sizeof (value), "%d", warndate);
	setup_attr(set_attribute, k++, "AUTHTOK_WARNDATE=", value);
	setup_attr(set_attribute, k, NULL, NULL);

	retcode = __set_authtoken_attr(pamh, (const char **) &set_attribute[0],
	    repository, domain, 0, NULL);
	free_setattr(set_attribute);
	return (retcode);
}


/*
 * get_newpasswd():
 * 	Get user's new password. It also does the syntax check for
 * 	the new password.
 */

static int
get_newpasswd(pamh, pwbuf, re, opwbuf, privileged, uid,
	try_first_pass, use_first_pass, debug, nowarn)
	pam_handle_t *pamh;
	char pwbuf[PAM_MAX_RESP_SIZE];	/* new password */
	int re;			/* current repository trying to update passwd */
	char *opwbuf;		/* old password */
	int privileged;		/* NIS+ privileged user */
	uid_t uid;		/* UID of user that invoked passwd cmd */
	int try_first_pass;
	int use_first_pass;
	int debug;
	int nowarn;
{

	int		insist = 0; /* # times new passwd fails checks */
	int		count = 0;  /* # times old/new passwds do not match */
	int		done = 0;	/* continue to prompt until done */
	char		*usrname;
	char		*prognamep;
	int		retcode;
	char 		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char		*pswd;

	/* Set if password already exists as pam handle item */
	int		passwd_already_set = 0;
	char		*saved_passwd = NULL;

	if (debug)
		syslog(LOG_DEBUG,
		"get_newpasswd(), try_first_pass = %d, use_first_pass = %d",
		try_first_pass, use_first_pass);

	if ((retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
						!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
						!= PAM_SUCCESS)
		return (retcode);

	pam_get_item(pamh, PAM_AUTHTOK, (void **)&saved_passwd);
	if (saved_passwd != NULL)
		passwd_already_set = 1;

	while (insist < MAX_CHANCES && count < MAX_CHANCES && !done) {

		/* clear a bad "new password" if it was saved */
		if (passwd_already_set == 0)
			pam_set_item(pamh, PAM_AUTHTOK, NULL);

		/*
		 * If use_first_pass failed, then return.
		 * If try_first_pass failed, note it so that we
		 * do not try it again (when retyping in the new
		 * password).
		 */
		if (use_first_pass && (insist != 0 || count != 0))
			return (PAM_AUTHTOK_ERR);
		if (try_first_pass && (insist != 0 || count != 0))
			try_first_pass = 0;

		if ((try_first_pass || use_first_pass) &&
			insist == 0 && count == 0) {
			/*
			 * Try the new password entered to the first password
			 * module in the stack (try/use_first_pass option).
			 * If it already failed (insist > 0 || count > 0) then
			 * prompt if option is try_first_pass.
			 */
			if ((retcode = __pam_get_authtok(pamh, PAM_HANDLE,
				PAM_AUTHTOK, PASSWORD_LEN, 0,
				&pswd)) != PAM_SUCCESS) {
				return (retcode);
			}
		} else {
			if ((retcode = __pam_get_authtok(pamh, PAM_PROMPT,
				PAM_AUTHTOK, PASSWORD_LEN,
				PAM_MSG(pamh, 74, "New password: "),
				&pswd)) != PAM_SUCCESS) {
					return (retcode);
			}
		}

		if (pswd == NULL) {
			if (try_first_pass) {
				/*
				 * This means that the module has
				 * try_first_pass set, but there was no
				 * previous module that ever prompted for
				 * a password.  Go back and prompt for
				 * a password.
				 */
				count++;
				continue;
			}

			(void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 65, "%s%s: Sorry."),
				prognamep, UNIX_MSG);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);

			if (pswd)
				free(pswd);

			/* clear a bad "new password" if it was saved */
			if (passwd_already_set == 0)
				pam_set_item(pamh, PAM_AUTHTOK, NULL);
			return (PAM_AUTHTOK_ERR);
		} else {
			memset(pwbuf, 0, PAM_MAX_RESP_SIZE);
			(void) strncpy(pwbuf, pswd, PAM_MAX_RESP_SIZE);

			memset(pswd, 0, PASSWORD_LEN);
			free(pswd);
		}

		if ((retcode = triviality_checks(pamh, uid, opwbuf,
			pwbuf, re, privileged, nowarn)) != PAM_SUCCESS) {
			if (retcode == PAM_AUTHTOK_ERR) {
				insist++;
				continue;
			} else {
				/* clear bad "new password" if saved */
				if (passwd_already_set == 0)
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
				return (retcode);
			}
		}

		/*
		 * New password passed triviality check.  Reset
		 * insist to 0 in case user does not retype password
		 * correctly and we have to loop all over.
		 */
		insist = 0;

		/* Now make sure user re-types in passwd correctly. */
		if (try_first_pass || use_first_pass) {
			/*
			 * Try the new password entered to the first password
			 * module in the stack (try/use_first_pass option).
			 */
			if ((retcode = __pam_get_authtok(pamh, PAM_HANDLE,
				PAM_AUTHTOK, PASSWORD_LEN, NULL,
				&pswd)) != PAM_SUCCESS) {
					/* clear bad "new password" if saved */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_AUTHTOK, NULL);
					return (retcode);
			}
		} else {
			if ((retcode = __pam_get_authtok(pamh,
				PAM_PROMPT, 0, PASSWORD_LEN,
				PAM_MSG(pamh, 75, "Re-enter new password: "),
				&pswd)) != PAM_SUCCESS) {
					/* clear bad "new password" if saved */
					if (passwd_already_set == 0)
						pam_set_item(pamh,
							PAM_AUTHTOK, NULL);
					return (retcode);
			}
		}

		if ((strlen(pswd) != strlen(pwbuf)) ||
		    (strncmp(pswd, pwbuf, strlen(pwbuf)))) {
			memset(pswd, 0, strlen(pswd));
			free(pswd);
			if (++count >= MAX_CHANCES) {
				(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 76,
				    "%s%s: Too many tries; try again later"),
					prognamep, UNIX_MSG);
				(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
					1, messages, NULL);
				/* clear bad "new password" if saved */
				if (passwd_already_set == 0)
					pam_set_item(pamh, PAM_AUTHTOK, NULL);
				return (PAM_PERM_DENIED);
			} else {
				(void) snprintf(messages[0],
						sizeof (messages[0]),
						PAM_MSG(pamh, 77,
				"%s%s: They don't match; try again."),
						prognamep, UNIX_MSG);
				(void) __pam_display_msg(
						pamh, PAM_ERROR_MSG, 1,
						messages, NULL);
			}
			/* audit_passwd_main9(); */
			continue;
		}

		/* password matched - exit loop and return PAM_SUCCESS */
		done = 1;
	} /* while loop */

	/*
	 * If we exit the while loop with too
	 * many attempts return error.
	 */
	if (insist >= MAX_CHANCES) {
		if (debug)
			syslog(LOG_DEBUG,
			"get_newpasswd: failed trivial check");
		(void) snprintf(messages[0],
				sizeof (messages[0]),
				PAM_MSG(pamh, 78,
				"%s%s: Too many failures - try later."),
				prognamep, UNIX_MSG);
		(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
			1, messages, NULL);
		/* clear bad "new password" if saved */
		if (passwd_already_set == 0)
			pam_set_item(pamh, PAM_AUTHTOK, NULL);
		return (PAM_PERM_DENIED);
	}

	/* new password passed triviality check and re-type check */
	return (PAM_SUCCESS);
}

static int
triviality_checks(pam_handle_t *pamh, uid_t uid,
		char *opwbuf, char *pwbuf, int re,
		int privileged, int nowarn)
{

	int	retcode = 0;
	int	tmpflag = 0;
	int	flags = 0;
	char	*p, *o;
	int	c;
	int 	i, j, k;
	int	bare_minima = MINLENGTH;
	char	*char_p;
	char	*usrname, *prognamep;
	char 	messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	int pwlen = strlen(pwbuf);

	if ((retcode = pam_get_item(pamh, PAM_USER, (void **)&usrname))
							!= PAM_SUCCESS ||
	    (retcode = pam_get_item(pamh, PAM_SERVICE, (void **)&prognamep))
							!= PAM_SUCCESS)
		return (retcode);

	/*
	 * Make sure new password is long enough if not privileged user
	 */

	/*
	 * Open "/etc/default/passwd" file,
	 * if password administration file can't be opened
	 * use built in defaults.
	 */
	if ((defopen(PWADMIN)) == 0) {
		/* get minimum length of password */
		if ((char_p = defread("PASSLENGTH=")) != NULL)
			bare_minima = atoi(char_p);

		/* close defaults file */
		defopen(NULL);
	}
	if (bare_minima < MINLENGTH)
		bare_minima = MINLENGTH;
	else if (bare_minima > MAXLENGTH)
		bare_minima = MAXLENGTH;

	if (!((uid == 0 && IS_FILES(re)) ||
	    (privileged && IS_NISPLUS(re)) ||
	    (privileged && IS_NIS(re))) &&
	    (pwlen < bare_minima)) {
		if (!nowarn) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 79,
		"%s%s: Password too short - must be at least %d characters."),
					prognamep, UNIX_MSG, bare_minima);
			retcode = __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			if (retcode != PAM_SUCCESS) {
				return (retcode);
			}
		}
		return (PAM_AUTHTOK_ERR);
	}

	/* Check the circular shift of the logonid */

	if (!((uid == 0 && IS_FILES(re)) ||
		(privileged && IS_NISPLUS(re)) ||
		(privileged && IS_NIS(re))) &&
	    circ(usrname, pwbuf)) {
		if (!nowarn) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 80,
		"%s%s: Password cannot be circular shift of logonid."),
				prognamep, UNIX_MSG);
			retcode = __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			if (retcode != PAM_SUCCESS) {
				return (retcode);
			}
		}
		return (PAM_AUTHTOK_ERR);
	}

	/*
	 * Ensure passwords contain at least two alpha characters
	 * and one numeric or special character
	 */

	flags = 0;
	tmpflag = 0;
	p = pwbuf;
	if (!((uid == 0 && IS_FILES(re)) ||
	    (privileged && IS_NISPLUS(re)) ||
	    (privileged && IS_NIS(re)))) {
		c = *p++;
		while (c != '\0') {
			if (isalpha(c) && tmpflag)
				flags |= 1;
			else if (isalpha(c) && !tmpflag) {
				flags |= 2;
				tmpflag = 1;
			} else if (isdigit(c))
				flags |= 4;
			else
				flags |= 8;
			c = *p++;
		}

		/*
		 * 7 = lca, lca, num
		 * 7 = lca, uca, num
		 * 7 = uca, uca, num
		 * 11 = lca, lca, spec
		 * 11 = lca, uca, spec
		 * 11 = uca, uca, spec
		 * 15 = spec, num, alpha, alpha
		 */

		if (flags != 7 && flags != 11 && flags != 15) {
			if (!nowarn) {
			    (void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 81,
	"%s%s: The first %d characters of the password"),
					prognamep, UNIX_MSG, bare_minima);
			    (void) snprintf(messages[1],
					sizeof (messages[1]),
					PAM_MSG(pamh, 82,
	"must contain at least two alphabetic characters and at least"));
			    (void) snprintf(messages[2],
					sizeof (messages[2]),
					PAM_MSG(pamh, 83,
	"one numeric or special character."));
			    retcode = __pam_display_msg(pamh,
				PAM_ERROR_MSG, 3, messages, NULL);
			    if (retcode != PAM_SUCCESS) {
				return (retcode);
			    }
			}
			return (PAM_AUTHTOK_ERR);
		}
	}

	if (!((uid == 0 && IS_FILES(re)) ||
		(privileged && IS_NISPLUS(re)) ||
		(privileged && IS_NIS(re)))) {
		p = pwbuf;
		o = opwbuf;
		if (pwlen >= strlen(opwbuf)) {
			i = pwlen;
			k = pwlen - strlen(opwbuf);
		} else {
			i = strlen(opwbuf);
			k = strlen(opwbuf) - pwlen;
		}
		for (j = 1; j <= i; j++)
			if (*p++ != *o++)
				k++;
		if (k  <  3) {
			if (!nowarn) {
			    (void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 84,
			"%s%s: Passwords must differ by at least 3 positions"),
					prognamep, UNIX_MSG);
			    retcode = __pam_display_msg(pamh,
				PAM_ERROR_MSG, 1, messages, NULL);
			    if (retcode != PAM_SUCCESS) {
				return (retcode);
			    }
			}
			return (PAM_AUTHTOK_ERR);
		}
	}

	return (PAM_SUCCESS);
}

/*
 * circ():
 * 	This function return 1 if string "t" is a circular shift of
 *	string "s", else it returns 0.
 */

static int
circ(s, t)
	char *s, *t;
{
	char c, *p, *o, *r, buff[25], ubuff[25], pubuff[25];
	int i, j, k, l, m;

	m = 2;
	i = strlen(s);
	o = &ubuff[0];
	for (p = s; c = *p++; *o++ = c)
		if (islower(c))
			c = toupper(c);
	*o = '\0';
	o = &pubuff[0];
	for (p = t; c = *p++; *o++ = c)
		if (islower(c))
			c = toupper(c);

	*o = '\0';

	p = &ubuff[0];
	while (m--) {
		for (k = 0; k  <=  i; k++) {
			c = *p++;
			o = p;
			l = i;
			r = &buff[0];
			while (--l)
				*r++ = *o++;
			*r++ = c;
			*r = '\0';
			p = &buff[0];
			if (strcmp(p, pubuff) == 0)
				return (1);
		}
		p = p + i;
		r = &ubuff[0];
		j = i;
		while (j--)
			*--p = *r++;
	}
	return (0);
}

#ifdef PAM_SECURE_RPC

/*
 * Loop thru each cred table entry for the user, skip the local
 * cred entry, skip any authtypes that are not in the NIS+ security cf,
 * then try to decrypt the rest.
 *
 * Also, set pk_creds_exist to TRUE if any public key cred table
 * entries are found (regardless of the (non)success of the decrypting).
 *
 * Return TRUE if any are successfully decrypted, else return FALSE.
 */
static bool_t
verify_rpc_passwd(
	char *oldpass,		/* in */
	nis_result *cred_res,	/* in */
	bool_t *pk_creds_exist,	/* out */
	int debug)		/* in */
{
	int i, success_cnt = 0, mech_cnt = 0;

	if (!oldpass || !cred_res) {
		*pk_creds_exist = cred_res ? TRUE : FALSE;
		return (FALSE);
	}

	if (debug)
		syslog(LOG_DEBUG,
			"PAM: verify_rpc_passwd: got %d cred entries",
			cred_res->objects.objects_len);

	for (i = 0; i < cred_res->objects.objects_len; i++) {
		nis_object *cred_entry = &(cred_res->objects.objects_val[i]);
		char *auth_type;
		char *sec_key;
		keylen_t keylen;
		algtype_t algtype;
		char netname[MAXNETNAMELEN+1];
		char *tmpsec_key;

		if (!extract_sec_keyinfo(cred_entry, &sec_key, &auth_type,
						&keylen, &algtype, debug))
			continue;
		mech_cnt++;

		if (!getnetname(netname)) {
			syslog(LOG_ERR,
				"PAM: verify_rpc_passwd: can't get netname");
			continue;
		}

		if (debug)
			syslog(LOG_DEBUG,
"PAM: verify_rpc_passwd: call xdecrypt_g: keylen=%d, algtype=%d, netname=%s\n",
				keylen, algtype, netname);

		if (!(tmpsec_key = malloc(strlen(sec_key)+1))) {
			syslog(LOG_ALERT, "out of memory!");
			return (FALSE);
		}

		/* xdcrypt_g is destructive to sec key, so let's use clone */
		(void) strcpy(tmpsec_key, sec_key);
		if (xdecrypt_g(tmpsec_key, keylen, algtype,
				oldpass, netname, TRUE))
			success_cnt++;
		else
			syslog(LOG_INFO,
	"PAM: verify_rpc_passwd: could not decrypt secret key for %s/%d-%d",
				netname, keylen, algtype);

		free(tmpsec_key);
	}

	*pk_creds_exist = mech_cnt ? TRUE : FALSE;

	if (debug) {
		if (*pk_creds_exist)
			syslog(LOG_DEBUG,
				"PAM: verify_rpc_passwd: PK creds exist");
		else
			syslog(LOG_DEBUG,
			"PAM: verify_rpc_passwd: PK creds do NOT exist");

		syslog(LOG_DEBUG,
			"PAM: verify_rpc_passwd: %d of %d key(s) decrypted",
			success_cnt, mech_cnt);
	}

	return (success_cnt ? TRUE : FALSE);
}

#endif /* PAM_SECURE_RPC */

#ifdef PAM_NISPLUS
/*
 * Sets the global variable cred_res
 *
 * First, find the LOCAL credentials
 *	use the user's uid from the passwd entry pointed to by
 *	nisplus_pwd as the key to search on.
 * Next, obtain the user's home domain
 *	this is gotten from the cname of the LOCAL credentials
 * Finally, get all the user's credentials so we can later loop thru
 * the public key ones
 *	use the cname of the LOCAL credentials for the search key
 *	and perform the search in the user's home domain
 *
 * Note:	PAM_SUCCESS does not mean credentials were found,
 *		only that this routine did what it could without
 *		internal errors.
 *		Check cred_res for actual results.
 */
static int
get_nispluscred(pamh, nisdomain, prognamep, repository, nisplus_pwd, cred_res)
	pam_handle_t	*pamh;
	char		*nisdomain;
	char 		*prognamep;
	int		repository;
	struct passwd	*nisplus_pwd;
	nis_result	**cred_res;
{
	char		messages[PAM_MAX_NUM_MSG][PAM_MAX_MSG_SIZE];
	char		buf[NIS_MAXNAMELEN+1];
	struct nis_result *local_res = NULL;	/* cred local nis_list result */
	nis_name	cred_domain;
	char		*local_cname;

	if (nisplus_pwd == NULL) {
		/* No NIS+ passwd entry, can't resolve credentials. */
		return (PAM_SUCCESS);
	}

	/*
	 * The credentials may not be in the local domain.
	 * Find the the LOCAL entry first to get the correct
	 * cname to search for DES credentials.
	 */

	/*
	 * strlen("[auth_name=nnnnnnnnnnnnnnnn,auth_type=LOCAL],.") + null
	 *	+ "." = 50  (allow for long UID in future release)
	 */
	if ((50 + strlen(nisdomain) + PKTABLELEN) >
	    (size_t) NIS_MAXNAMELEN) {
		syslog(LOG_ERR, "%s: Name too long", prognamep);
		*cred_res = NULL;
		return (PAM_AUTHTOK_ERR);
	}

	(void) snprintf(buf,
			sizeof (buf),
			"[auth_name=%d,auth_type=LOCAL],%s.%s",
			nisplus_pwd->pw_uid, PKTABLE, nisdomain);
	if (buf[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	local_res = nis_list(buf,
		USE_DGRAM+FOLLOW_LINKS+FOLLOW_PATH+MASTER_ONLY,
		NULL, NULL);
	if ((local_res == NULL) || (local_res->status != NIS_SUCCESS)) {
		nis_freeresult(local_res);
		if (IS_OPWCMD(repository)) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 85,
				"nispasswd: user must have LOCAL credential"));
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			*cred_res = NULL;
			return (PAM_CRED_INSUFFICIENT);
		}
		*cred_res = NULL;
		return (PAM_SUCCESS);
	}

	local_cname = ENTRY_VAL(NIS_RES_OBJECT(local_res), 0);
	if (local_cname == NULL) {
		nis_freeresult(local_res);
		if (IS_OPWCMD(repository)) {
			(void) snprintf(messages[0],
					sizeof (messages[0]),
					PAM_MSG(pamh, 85,
			    "%s: invalid LOCAL credential"), prognamep);
			(void) __pam_display_msg(pamh, PAM_ERROR_MSG,
				1, messages, NULL);
			*cred_res = NULL;
			return (PAM_CRED_ERR);
		}
		*cred_res = NULL;
		return (PAM_SUCCESS);
	}

	cred_domain = nis_domain_of(local_cname);

	/*
	 * strlen("[cname=],.") + null = 11
	 */
	if ((11 + strlen(local_cname) + strlen(cred_domain)
	    + PKTABLELEN) > (size_t) NIS_MAXNAMELEN) {
		syslog(LOG_ERR, "%s: Name too long", prognamep);
		nis_freeresult(local_res);
		*cred_res = NULL;
		return (PAM_AUTHTOK_ERR);
	}

	(void) snprintf(buf,
			sizeof (buf),
			"[cname=%s],%s.%s", /* get all entries for user */
			local_cname, PKTABLE, cred_domain);

	nis_freeresult(local_res);

	*cred_res = nis_list(buf,
		USE_DGRAM+FOLLOW_LINKS+FOLLOW_PATH+MASTER_ONLY,
		NULL, NULL);

	return (PAM_SUCCESS);
}
#endif /* PAM_NISPLUS */