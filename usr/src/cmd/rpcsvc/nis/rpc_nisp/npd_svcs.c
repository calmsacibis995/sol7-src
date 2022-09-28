/*
 *	npd_svcsubr.c
 *	Contains the sub-routines required by the server.
 *
 *	Copyright (c) 1994 Sun Microsystems, Inc.
 *	All Rights Reserved.
 */

#pragma ident	"@(#)npd_svcsubr.c	1.9	97/11/19 SMI"

#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <memory.h>
#include <shadow.h>
#include <crypt.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpc/key_prot.h>
#include <rpc/des_crypt.h>
#include <mp.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/nispasswd.h>
#include <rpcsvc/nis_dhext.h>

extern int verbose;
extern bool_t debug;
extern bool_t generatekeys;

bool_t __npd_prin2netname(char *, char []);

/*
 * get the password information for this user
 * the result should be freed using nis_freeresult()
 */
struct nis_result *
nis_getpwdent(user, domain)
char		*user;
char		*domain;
{
	char	buf[NIS_MAXNAMELEN];

	if ((user == NULL || *user == '\0') ||
		(domain == NULL || *domain == '\0'))
		return (NULL);

	/* strlen("[name=],passwd.org_dir.") + null + "." = 25 */
	if ((25 + strlen(user) + strlen(domain)) >
					(size_t) NIS_MAXNAMELEN)
		return (NULL);

	(void) sprintf(buf, "[name=%s],passwd.org_dir.%s", user, domain);
	if (buf[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	return (nis_list(buf, USE_DGRAM+MASTER_ONLY, NULL, NULL));
}

/*
 * get the credential information for this user
 * the result should be freed using nis_freeresult()
 */
static struct nis_result *
nis_getcredent(principal, domain, auth_type)
char	*principal;
char	*domain;
char	*auth_type;
{
	char	buf[NIS_MAXNAMELEN];

	if ((principal == NULL || *principal == '\0') ||
		(domain == NULL || *domain == '\0') || auth_type == NULL)
		return (NULL);

	/* strlen("[cname=,auth_type=],cred.org_dir.") + null + "." = 35 */
	if ((35 + strlen(principal) + strlen(domain)) >
				(size_t) NIS_MAXNAMELEN)
		return (NULL);

	(void) sprintf(buf, "[cname=%s,auth_type=%s],cred.org_dir.%s",
			principal, auth_type, domain);

	if (buf[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	return (nis_list(buf, USE_DGRAM+MASTER_ONLY, NULL, NULL));
}

static bool_t
__npd_fill_spwd(obj, sp)
struct nis_object *obj;
struct	spwd	*sp;
{
	long	val;
	char	*p = NULL, *ageinfo = NULL;
	char	*ep, *end;

	/* defaults */
	sp->sp_lstchg = -1;
	sp->sp_min = -1;
	sp->sp_max = -1;
	sp->sp_warn = -1;
	sp->sp_inact = -1;
	sp->sp_expire = -1;
	sp->sp_flag = 0;

	/* shadow information is in column 7 */
	if ((p = ENTRY_VAL(obj, 7)) == NULL)
		return (FALSE);
	if ((ageinfo = strdup(p)) == NULL)
		return (FALSE);

	p = ageinfo;
	end = ageinfo + ENTRY_LEN(obj, 7);

	/* format is: lstchg:min:max:warn:inact:expire:flag */

	val = strtol(p, &ep, 10);	/* base = 10 */
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_lstchg = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_min = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_max = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_warn = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_inact = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_expire = val;
	p = ep + 1;

	val = strtol(p, &ep, 10);
	if (*ep != ':' || ep >= end) {
		(void) free(ageinfo);
		return (FALSE);
	}
	if (ep != p)
		sp->sp_flag = val;

	(void) free(ageinfo);
	return (TRUE);
}
/*
 * checks if the password has aged sufficiently
 */
bool_t
__npd_has_aged(obj, res)
struct nis_object *obj;
int	*res;
{
	struct	spwd	sp;
	long	now;

	if (__npd_fill_spwd(obj, &sp) == TRUE) {

		now = DAY_NOW;
		if (sp.sp_lstchg != 0 && sp.sp_lstchg <= now) {
			/* password changed before or just now */
			if (now < (sp.sp_lstchg + sp.sp_min)) {
				*res = NPD_NOTAGED;
				return (FALSE);
			} else
				return (TRUE);
		}
	}
	*res = NPD_NOSHDWINFO;
	return (FALSE);
}

/*
 * Authenticate the admin by decrypting any of their secret keys with
 * the passwd they sent across.  If any of the secret keys, of mech types
 * listed in the NIS+ security cf, are decrypted, then return TRUE.
 */
bool_t
__authenticate_admin(char *prin, char *pass)
{
	char	*d;
	struct	nis_result	*cres;
	char    netname[MAXNETNAMELEN+1];
	mechanism_t **mechs;

	if ((prin == NULL || *prin == '\0') ||
		(pass == NULL || *pass == '\0'))
		return (FALSE);

	if (!__npd_prin2netname(prin, netname)) {
		syslog(LOG_ERR,
	"__authenticate_admin: cannot convert principal '%s' to netname", prin);
		return (FALSE);
	}

	d = strchr(prin, '.');
	if (d == NULL)
		d = nis_local_directory();
	else
		d++;

	if (mechs = __nis_get_mechanisms(FALSE)) {
		mechanism_t **mpp;
		char auth_type[MECH_MAXATNAME+1];

		for (mpp = mechs; *mpp; mpp++) {
			mechanism_t *mp = *mpp;

			if (verbose)
				syslog(LOG_INFO,
		"__authenticate_admin: trying mech '%s' for user '%s'",
				mp->alias ? mp->alias : "NULL", prin);

			if (AUTH_DES_COMPAT_CHK(mp)) {
				__nis_release_mechanisms(mechs);
				goto try_auth_des;
			}
			if (! VALID_MECH_ENTRY(mp))
				continue;

			if (!__nis_mechalias2authtype(mp->alias, auth_type,
							sizeof (auth_type)))
				continue;

			cres = nis_getcredent(prin, d, auth_type);
			if (cres != NULL && cres->status == NIS_SUCCESS) {
				char *sp;

				if (debug)
					syslog(LOG_DEBUG,
				"__authenticate_admin: got cred entry");

				sp = ENTRY_VAL(NIS_RES_OBJECT(cres), 4);

				if (sp != NULL) {
					if (xdecrypt_g(sp, mp->keylen,
							mp->algtype, pass,
							netname, TRUE)) {
						__nis_release_mechanisms(
							mechs);

						if (debug)
							syslog(LOG_DEBUG,
				"__authenticate_admin: xdecrypt success");

						(void) nis_freeresult(cres);
						return (TRUE);
					}
				}
			}
			if (cres != NULL)
				(void) nis_freeresult(cres);

		}
		__nis_release_mechanisms(mechs);
	} else {
		/*
		 * No valid mechs in the NIS+ security cf file,
		 * so let's try AUTH_DES.
		 */
	try_auth_des:
		cres = nis_getcredent(prin, d, AUTH_DES_AUTH_TYPE);
		if (cres != NULL && cres->status == NIS_SUCCESS) {
			char *sp;
			sp = ENTRY_VAL(NIS_RES_OBJECT(cres), 4);
			if (sp != NULL) {
				if (xdecrypt_g(sp, AUTH_DES_KEYLEN,
						AUTH_DES_ALGTYPE, pass,
						NULL, TRUE)) {
					(void) nis_freeresult(cres);
					return (TRUE);
				}
			}
		}
		if (cres != NULL)
			(void) nis_freeresult(cres);
	}

	return (FALSE);
}

/*
 * build a netname given a nis+ principal name
 */
bool_t
__npd_prin2netname(prin, netname)
char	*prin;
char	netname[MAXNETNAMELEN];
{
	nis_result	*pass_res;
	nis_object	*pobj;
	char		name[NIS_MAXNAMELEN];
	char		*domain;
	uid_t		uid;


	if (prin == NULL || *prin == '\0')
		return (FALSE);
	if (strlen(prin) > (size_t) NIS_MAXNAMELEN)
		return (FALSE);
	(void) sprintf(name, "%s", prin);

	/* get the domain name */
	if (name[strlen(name) - 1] == '.')
		name[strlen(name) - 1] = '\0';
	else
		return (FALSE);
	domain = strchr(name, '.');
	if (domain == NULL)
		return (FALSE);
	else
		*domain++ = '\0';

	/* nis_getpwdent() will fully qualify the domain */
	pass_res = nis_getpwdent(name, domain);

	if (pass_res == NULL)
		return (FALSE);

	switch (pass_res->status) {
	case NIS_SUCCESS:
		pobj = NIS_RES_OBJECT(pass_res);
		uid = atol(ENTRY_VAL(pobj, 2));
		(void) nis_freeresult(pass_res);
		/* we cannot simply call user2netname() ! */
		if ((strlen(domain) + 7 + 11) > (size_t) MAXNETNAMELEN)
			return (FALSE);
		(void) sprintf(netname, "unix.%d@%s", uid, domain);
		return (TRUE);

	case NIS_NOTFOUND:
		/*
		 * assumption: hosts DO NOT have their passwords
		 * stored in NIS+ ==> not a user but a host
		 */
		if ((strlen(domain) + 7 + strlen(name)) >
						(size_t) MAXNETNAMELEN)
			return (FALSE);
		(void) sprintf(netname, "unix.%s@%s", name, domain);
		(void) nis_freeresult(pass_res);
		return (TRUE);

	default:
		syslog(LOG_ERR, "no passwd entry found for %s", prin);
		(void) nis_freeresult(pass_res);
		return (FALSE);
	}
}

/*
 * make a new cred entry and add it
 */
bool_t
__npd_addcredent(prin, domain, auth_type, newpass)
char	*prin;		/* principal name */
char	*domain;	/* domain name */
char	*auth_type;	/* cred table auth type name */
char	*newpass;	/* new passwd used to encrypt secret key */
{
	char	*public = NULL;
	char	*secret = NULL;
	char	*encrypted_secret = NULL;
	nis_object	obj;
	entry_col	ecol[5];
	struct nis_result *mod_res;
	char	buf[NIS_MAXNAMELEN];
	char	nisdomain[NIS_MAXNAMELEN];
	char	netname[MAXNETNAMELEN];
	int	status;
	char	mech_alias[MECH_MAXALIASNAME+1];
	keylen_t keylen;
	algtype_t algtype;
	bool_t	ret;

	/* check args */
	if ((prin == NULL || *prin == '\0') ||
	    (domain == NULL || *domain == '\0') || auth_type == NULL) {
		ret = FALSE;
		goto out;
	}

	if (strlen(domain) > (size_t) NIS_MAXNAMELEN) {
		ret = FALSE;
		goto out;
	}
	(void) sprintf(nisdomain, "%s", domain);
	if (nisdomain[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	/* build netname from principal name */
	if (__npd_prin2netname(prin, netname) == FALSE) {
		ret = FALSE;
		goto out;
	}

	/* initialize object */
	memset((char *)&obj, 0, sizeof (obj));
	memset((char *)ecol, 0, sizeof (ecol));

	obj.zo_name = "cred";
	obj.zo_ttl = 43200;
	obj.zo_data.zo_type = NIS_ENTRY_OBJ;
	obj.zo_owner = prin;
	obj.zo_group = nis_local_group();
	obj.zo_domain = nisdomain;
			/* owner: r; group: rmcd */
	obj.zo_access = (NIS_READ_ACC<<16)|
			((NIS_READ_ACC|NIS_MODIFY_ACC|
			NIS_CREATE_ACC|NIS_DESTROY_ACC)<<8);

	if (strcasecmp(auth_type, "LOCAL") != 0) {


		/* we need the mech alias name to get keylen/algtype */
		if (! __nis_authtype2mechalias(auth_type, mech_alias,
						sizeof (mech_alias))) {
			syslog(LOG_ERR,
			"could not convert auth type '%s' to mech alias",
				auth_type);
			ret = FALSE;
			goto out;
		}

		/* get the key length and algorithm type */
		if (__nis_translate_mechanism(mech_alias, &keylen, &algtype)
		    < 0) {
			syslog(LOG_ERR,
				"could not convert mech alias '%s' to keyalg",
				mech_alias);
			ret = FALSE;
			goto out;
		}

		if ((public = malloc(BITS2NIBBLES(keylen) + 1)) == NULL) {
			syslog(LOG_ALERT, "malloc failed");
			ret = FALSE;
			goto out;
		}
		if ((secret = malloc(BITS2NIBBLES(keylen) + 1)) == NULL) {
			syslog(LOG_ALERT, "malloc failed");
			ret = FALSE;
			goto out;
		}

		/* generate key-pair */
		if (! __gen_dhkeys_g(public, secret, keylen, algtype,
					newpass)) {
			syslog(LOG_ERR,
				"could not generate DH key pair for %d-%d",
				keylen, algtype);
			ret = FALSE;
			goto out;
		}
		if (! xencrypt_g(secret, keylen, algtype, newpass, netname,
					&encrypted_secret, TRUE)) {
			syslog(LOG_ERR,
				"could not encrypt secret key for %d-%d",
				keylen, algtype);
			ret = FALSE;
			goto out;
		}

		/* build cred entry */
		ecol[0].ec_value.ec_value_val = prin;
		ecol[0].ec_value.ec_value_len = strlen(prin) + 1;

		ecol[1].ec_value.ec_value_val = auth_type;
		ecol[1].ec_value.ec_value_len = strlen(auth_type) + 1;

		ecol[2].ec_value.ec_value_val = netname;
		ecol[2].ec_value.ec_value_len = strlen(netname) + 1;

		ecol[3].ec_value.ec_value_val = public;
		ecol[3].ec_value.ec_value_len = strlen(public) + 1;

		ecol[4].ec_value.ec_value_val = encrypted_secret;
		ecol[4].ec_value.ec_value_len = strlen(encrypted_secret) + 1;
		ecol[4].ec_flags |= EN_CRYPT;
	}
	obj.EN_data.en_type = "cred_tbl";
	obj.EN_data.en_cols.en_cols_val = ecol;
	obj.EN_data.en_cols.en_cols_len = 5;

	/* strlen("cred.org_dir.") + null = 14 */
	if ((strlen(nisdomain) + 14) > (size_t) NIS_MAXNAMELEN)
		return (FALSE);
	(void) sprintf(buf, "cred.org_dir.%s", (char *)&nisdomain[0]);

	if (debug == TRUE)
		(void) nis_print_object(&obj);

	mod_res = nis_add_entry(buf, &obj, 0);
	status = mod_res->status;
	(void) nis_freeresult(mod_res);
	switch (status) {
	case NIS_SUCCESS:
		ret = TRUE;
		goto out;

	case NIS_PERMISSION:
		if (verbose == TRUE)
			syslog(LOG_ERR,
			"permission denied to add a %s cred entry for %s",
				auth_type, prin);
		ret = FALSE;
		goto out;

	default:
		if (verbose == TRUE)
			syslog(LOG_ERR,
				"error creating %s cred for %s, NIS+ error: %s",
				auth_type, prin, nis_sperrno(status));
		ret = FALSE;
		goto out;
	}

out:
	if (secret)
		free(secret);
	if (public)
		free(public);
	if (encrypted_secret)
		free(encrypted_secret);

	return (ret);
}

static nis_result *
nis_get_all_creds(char *principal, char *domain)
{
	char	buf[NIS_MAXNAMELEN];

	if ((principal == NULL || *principal == '\0') ||
		(domain == NULL || *domain == '\0'))
		return (NULL);

	/* strlen("[cname=,auth_type=],cred.org_dir.") + null + "." = 35 */
	if ((35 + strlen(principal) + strlen(domain)) >
				(size_t) NIS_MAXNAMELEN)
		return (NULL);

	(void) sprintf(buf, "[cname=%s],cred.org_dir.%s",
			principal, domain);

	if (buf[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	return (nis_list(buf, USE_DGRAM+MASTER_ONLY, NULL, NULL));
}

/*
 * Given a cred table entry, return the secret key, auth type, key length
 * of secret key, and algorithm type of secret key.
 *
 * The type of key must be listed in the NIS+ security cf.
 *
 * Return TRUE on success and FALSE on failure.
 */
static bool_t
extract_sec_keyinfo(nis_object *cred_entry,	/* in */
		    char **seckey,		/* out */
		    char **authtype,		/* out */
		    keylen_t *keylen,		/* out */
		    algtype_t *algtype,		/* out */
		    int debug)			/* in */
{
	char mechalias[MECH_MAXALIASNAME+1];

	*authtype = ENTRY_VAL(cred_entry, 1);
	if (!*authtype) {
		syslog(LOG_ERR, "auth type field is empty for cred entry");
		return (FALSE);
	}

	/* "local" unix system cred is not a PK crypto cred */
	if (strncmp(*authtype, "LOCAL", sizeof ("LOCAL")) == 0) {
		return (FALSE);
	}

	if (!__nis_authtype2mechalias(*authtype, mechalias,
						sizeof (mechalias))) {
		syslog(LOG_ERR,
			"can't convert authtype '%s' to mechanism alias",
			*authtype);
		return (FALSE);
	}

	/* make sure the mech is in the NIS+ security cf */
	if (__nis_translate_mechanism(mechalias, keylen, algtype) < 0) {
		syslog(LOG_WARNING,
		"can't convert mechanism alias '%s' to keylen and algtype",
			mechalias);
		return (FALSE);
	}

	*seckey = ENTRY_VAL(cred_entry, 4);
	if (!*seckey) {
		return (FALSE);
	}

	if (debug)
		syslog(LOG_DEBUG, "extract_sec_keyinfo: returning SUCCESS");

	return (TRUE);
}

/*
 * Loop thru all of the user's valid PK cred entries and return TRUE if
 * we can decrypt all of them, else return FALSE.
 */
static bool_t
decrypt_all_sec_keys(nis_result *cred_res,	/* in */
			char *netname,		/* in */
			char *oldpass)		/* in */
{
	int i;

	if (!cred_res || !netname || !oldpass)
		return (FALSE);

	for (i = 0; i < cred_res->objects.objects_len; i++) {
		nis_object *cred_entry = &(cred_res->objects.objects_val[i]);
		char *oldcryptsecret;
		char *secret;
		char *authtype;
		keylen_t keylen;
		algtype_t algtype;

		if (!extract_sec_keyinfo(cred_entry, &oldcryptsecret,
					&authtype, &keylen, &algtype, verbose))
			continue;

		/* use tmp secret buf because xdecrypt_g is destructive */
		if ((secret = malloc(ENTRY_LEN(cred_entry, 4))) == NULL) {
			syslog(LOG_ALERT,
				"__decrypt_all_sec_keys: no memory!");
			return (FALSE);
		}

		memcpy(secret, ENTRY_VAL(cred_entry, 4),
			ENTRY_LEN(cred_entry, 4));

		if (!xdecrypt_g(secret, keylen, algtype, oldpass, netname,
				TRUE)) {
			if (debug)
				syslog(LOG_DEBUG,
			"decrypt_all_sec_keys: xdecrypt_g failed for '%s'",
					netname);
			free(secret);
			return (FALSE);
		}
		free(secret);
	}

	return (TRUE);
}

/*
 * Reencrypt a cred tbl secret key and return a ptr to it's newly
 * allocated memory on successful return.  Also, if the global generatekeys
 * is set, return a ptr of newly allocated memory containing a new public
 * (and secret) key too.
 *
 * Return NULL on any type of failure.
 *
 * Caller must free secret and pubilc key memory on successful return.
 */
static char *
reencrypt_secret(char *oldcryptsecret,	/* in */
		char *oldpass,		/* in */
		char *newpass,		/* in */
		keylen_t keylen,	/* in */
		algtype_t algtype,	/* in */
		char **public,		/* out */
		char *netname)		/* in */
{
	char *secret;
	char *reencrypted_secret;

	if (!oldcryptsecret || !oldpass || !newpass || !public)
		return (NULL);

	if ((secret = malloc(strlen(oldcryptsecret) + 1)) == NULL) {
		syslog(LOG_ALERT,
			"reencrypte_secret: no memory!");
		return (NULL);
	}

	(void) strcpy(secret, oldcryptsecret);

	if (xdecrypt_g(secret, keylen, algtype, oldpass, netname, TRUE) == 0) {
		if (debug)
			syslog(LOG_DEBUG,
				"reencrypt_secret: xdecrypt_g failed for '%s'",
				netname);
		if (generatekeys == TRUE) {
			if ((*public = malloc(strlen(oldcryptsecret) + 1))
			    == NULL) {
				syslog(LOG_ALERT, "no memory!");
				free(secret);
				return (NULL);
			}
			if (!__gen_dhkeys_g(*public, secret, keylen,
					    algtype, newpass)) {
				syslog(LOG_ERR,
		"could not generate new Diffie-Hellman key pair for type %d-%d",
					keylen, algtype);
				free(secret);
				free(*public);
				return (NULL);
			}
		}
	}
	if (xencrypt_g(secret, keylen, algtype, newpass, netname,
			&reencrypted_secret, TRUE) == 0) {
		syslog(LOG_ERR, "could not encrypt keytype %d-%d",
			keylen, algtype);
		free(secret);
		if (*public)
			free(public);
		return (FALSE);
	}

	free(secret);
	if (*public)
		free(*public);

	return (reencrypted_secret);
}

/*
 * Loop thru the user's PK creds and try to update all possible
 * secret keys.  Return NIS_SUCCESS and set err to NIS_SUCCESS if
 * all secret keys were updated. If all secret keys are not updated, then
 * set err to NPD_KEYNOTREENC.
 *
 * Inorder to handle multiple PK cred entries without changing the protocol
 * (we would need a new partial-cred-update err condition), we first try to
 * decrypt all secret keys avail.  If we can decrypt them all, we assume we
 * will be successful encrypting and modifing the tbl.  If we cannot decrypt
 * all the secret keys avail, we punt right away with an err condition that
 * will trigger the client side to try the cred(s) update.  The downside of
 * doing the full decrypt first is that we will have to redo the decrypt
 * for the reencryption phase (but such is life...).
 */
int
__npd_upd_all_pk_creds(char *user,	/* in */
			char *domain,	/* in */
			char *oldpass,	/* in */
			char *newpass,	/* in */
			int  *err)	/* out */
{
	char	prin[NIS_MAXNAMELEN+1];
	char	netname[MAXNETNAMELEN+1];
	nis_result *cred_res;
	int status;
	int i;

	if ((user == NULL || *user == '\0') ||
		(domain == NULL || *domain == '\0')) {
		*err = NPD_KEYNOTREENC;
		return (NIS_SYSTEMERROR);
	}
	/* "." + "." + null = 3 */
	if ((strlen(user) + strlen(domain) + 3) > (size_t) NIS_MAXNAMELEN) {
		*err = NPD_KEYNOTREENC;
		return (NIS_SYSTEMERROR);
	}
	(void) sprintf(prin, "%s.%s", user, domain);
	if (prin[strlen(prin) - 1] != '.')
		(void) strcat(prin, ".");

	/* build netname from principal name */
	if (__npd_prin2netname(prin, netname) == FALSE) {
		*err = NPD_KEYNOTREENC;
		return (NIS_SYSTEMERROR);
	}

	if (debug)
		syslog(LOG_DEBUG, "__npd_upd_all_pk_creds: netname = %s",
			netname);

	/* get all cred entries */
	cred_res = nis_get_all_creds(prin, domain);
	if (!cred_res || cred_res->status != NIS_SUCCESS) {
		*err = NPD_KEYNOTREENC;
		return (NIS_SYSTEMERROR);
	}

	/* if we can't decrypt all secret keys, we punt to clnt side */
	if (!generatekeys && !decrypt_all_sec_keys(cred_res, netname,
							oldpass)) {
		*err = NPD_KEYNOTREENC;
		return (NIS_SYSTEMERROR);
	}

	/* reencrypt all the secret keys */
	for (i = 0; i < cred_res->objects.objects_len; i++) {
		nis_object *cred_entry = &(cred_res->objects.objects_val[i]);
		char *oldcryptsecret;
		char *newcryptsecret;
		char *public = NULL;
		char *authtype;
		keylen_t keylen;
		algtype_t algtype;
		nis_object *eobj;
		entry_col ecol[5];
		char mname[NIS_MAXNAMELEN+1] = {0};
		nis_result *mres;

		if (!extract_sec_keyinfo(cred_entry, &oldcryptsecret,
					&authtype, &keylen, &algtype, verbose))
			continue;

		if (!(newcryptsecret = reencrypt_secret(oldcryptsecret,
							oldpass, newpass,
							keylen, algtype,
							&public, netname))) {
			/* this should not happen */
			syslog(LOG_ERR,
	"reencrypt of secret key failed for keytype %d-%d for user '%s'",
				keylen, algtype, prin);
			*err = NPD_KEYNOTREENC;
			return (NIS_SYSTEMERROR);
		}

		/* update cred at server */
		(void) memset((char *)ecol, 0, sizeof (ecol));

		if (generatekeys == TRUE) {
			/* public key changed too */
			ecol[3].ec_value.ec_value_val = public;
			ecol[3].ec_value.ec_value_len = strlen(public) + 1;
			ecol[3].ec_flags = EN_MODIFIED;
		}

		ecol[4].ec_value.ec_value_val = newcryptsecret;
		ecol[4].ec_value.ec_value_len = strlen(newcryptsecret) + 1;
		ecol[4].ec_flags = EN_CRYPT|EN_MODIFIED;
		eobj = nis_clone_object(cred_entry, NULL);
		if (eobj == NULL) {
			free(newcryptsecret);
			if (public)
				free(public);
			*err = NPD_KEYNOTREENC;
			return (NIS_SYSTEMERROR);
		}
		eobj->EN_data.en_cols.en_cols_val = ecol;
		eobj->EN_data.en_cols.en_cols_len = 5;

		/* strlen("[cname=,auth_type=],cred.") + null + "." = 27 */
		if ((strlen(prin) + strlen(authtype) +
			strlen(cred_entry->zo_domain) + 27) > sizeof (mname)) {
			syslog(LOG_ERR,
				"upd_all_pk_creds: not enough buffer space");
			if (public)
				free(public);
			free(newcryptsecret);
			*err = NPD_KEYNOTREENC;
			return (NIS_SYSTEMERROR);
		}
		(void) snprintf(mname, sizeof (mname),
				"[cname=%s,auth_type=%s],cred.%s",
				prin, authtype,	cred_entry->zo_domain);
		if (mname[strlen(mname) - 1] != '.')
			(void) strcat(mname, ".");
		mres = nis_modify_entry(mname, eobj, 0);
		status = mres->status;

		free(newcryptsecret);
		if (public)
			free(public);
		/* set column stuff to NULL so that we can free eobj */
		eobj->EN_data.en_cols.en_cols_val = NULL;
		eobj->EN_data.en_cols.en_cols_len = 0;
		(void) nis_destroy_object(eobj);
		(void) nis_freeresult(mres);

		if (status != NIS_SUCCESS) {
			syslog(LOG_ERR,
		"NIS+ error on updating secret key type %d-%d for user '%s'",
				keylen, algtype, prin);

			*err = NPD_KEYNOTREENC;
			return (NIS_SYSTEMERROR);
		}
	}

	/* if we get here, all PK creds have been successfully updated */
	*err = NIS_SUCCESS;

	return (NIS_SUCCESS);
}

/*
 * encrypt new passwd
 */
char *
__npd_encryptpass(pass)
char	*pass;
{
	char	saltc[2];
	long	salt;
	int	i, c;

	if (pass == NULL || *pass == '\0')
		return (NULL);

	(void) time((time_t *)&salt);
	salt += (long) getpid();

	saltc[0] = salt & 077;
	saltc[1] = (salt >> 6) & 077;
	for (i = 0; i < 2; i++) {
		c = saltc[i] + '.';
		if (c > '9') c += 7;
		if (c > 'Z') c += 6;
		saltc[i] = c;
	}
	return (crypt(pass, saltc));
}


/*
 * find the passwd object for this user from the list
 * of dirs given. If object is found in more than one
 * place return an error, otherwise clone the object.
 * Note, the object should be freed using nis_destroy_object().
 */
bool_t
__npd_find_obj(user, dirlist, obj)
char	*user;
char	*dirlist;
nis_object **obj;	/* returned */
{
	char	*tmplist;
	char	*curdir, *end = NULL;
	char	buf[NIS_MAXNAMELEN];
	nis_result	*tmpres;
	nis_object	*tmpobj = NULL;

	if ((user == NULL || *user == '\0') ||
		(dirlist == NULL || *dirlist == '\0'))
		return (FALSE);

	*obj = NULL;
	if ((tmplist = strdup(dirlist)) == NULL) {
		syslog(LOG_CRIT, "out of memory");
		return (FALSE);
	}
	for (curdir = tmplist; curdir != NULL; curdir = end) {
		end = strchr(curdir, ' ');
		if (end != NULL)
			*end++ = NULL;
		if (strncasecmp(curdir, "org_dir", 7) != 0)
			continue;	/* not an org_dir */

		/* strlen("[name=],passwd." + null + "." = 17 */
		if ((strlen(user) + 17 + strlen(curdir)) >
				(size_t) NIS_MAXNAMELEN) {
			(void) free(tmplist);
			return (FALSE);
		}

		(void) sprintf(buf, "[name=%s],passwd.%s", user, curdir);
		if (buf[strlen(buf) - 1] != '.')
			(void) strcat(buf, ".");
		tmpres = nis_list(buf, USE_DGRAM+MASTER_ONLY, NULL, NULL);
		switch (tmpres->status) {
		case NIS_NOTFOUND:	/* skip */
			(void) nis_freeresult(tmpres);
			continue;

		case NIS_SUCCESS:
			if (NIS_RES_NUMOBJ(tmpres) != 1) {
				/* should only have one entry */
				(void) nis_freeresult(tmpres);
				(void) free(tmplist);
				if (tmpobj != NULL)
					(void) nis_destroy_object(tmpobj);
				return (FALSE);
			}
			if (tmpobj != NULL) {
				/* found in more than one dir */
				(void) nis_destroy_object(tmpobj);
				(void) nis_freeresult(tmpres);
				(void) free(tmplist);
				*obj = NULL;
				return (FALSE);
			}
			tmpobj = nis_clone_object(NIS_RES_OBJECT(tmpres), NULL);
			if (tmpobj == NULL) {
				syslog(LOG_CRIT, "out of memory");
				(void) nis_freeresult(tmpres);
				(void) free(tmplist);
				return (FALSE);
			}
			(void) nis_freeresult(tmpres);
			continue;

		default:
			/* some NIS+ error - quit processing */
			curdir = NULL;
			break;
		}
	}
	(void) nis_freeresult(tmpres);
	(void) free(tmplist);
	if (tmpobj == NULL)	/* no object found */
		return (FALSE);
	*obj = tmpobj;
	return (TRUE);
}

/*
 * go thru' the list of dirs and see if the host is a
 * master server for any 'org_dir'.
 */
bool_t
__npd_am_master(host, dirlist)
char	*host;
char	*dirlist;
{
	char	*tmplist;
	char	*curdir, *end = NULL;
	nis_server	**srvs;

	if ((host == NULL || *host == '\0') ||
		(dirlist == NULL || *dirlist == '\0'))
		return (FALSE);

	if ((tmplist = strdup(dirlist)) == NULL) {
		syslog(LOG_CRIT, "out of memory");
		return (FALSE);
	}
	for (curdir = tmplist; curdir != NULL; curdir = end) {
		end = strchr(curdir, ' ');
		if (end != NULL)
			*end++ = NULL;
		if (strncasecmp(curdir, "org_dir", 7) != 0)
			continue;	/* not an org_dir */

		srvs = nis_getservlist(curdir);
		if (srvs == NULL) {
			syslog(LOG_ERR,
		"cannot get a list of servers that serve '%s'", curdir);
			(void) free(tmplist);
			return (FALSE);
		}
		if (strcasecmp(host, srvs[0]->name) == 0) {
			(void) free(tmplist);
			(void) nis_freeservlist(srvs);
			return (TRUE);
		}
		(void) nis_freeservlist(srvs);
	}
	(void) free(tmplist);
	return (FALSE);
}


/*
 * check whether this principal has permission to
 * add/delete/modify an entry object
 */
bool_t
__npd_can_do(right, obj, prin, column)
unsigned long	right;		/* access right seeked */
nis_object *obj;		/* entry object */
char	*prin;			/* principal seeking access */
int	column;			/* column being modified */
{
	nis_result	*res;
	nis_object	*tobj;
	table_col	*tc;
	char		buf[NIS_MAXNAMELEN];
	int		mod_ok;

	/* strlen("passwd." + null + "." = 9 */
	if ((9 + strlen(obj->zo_domain)) > (size_t) NIS_MAXNAMELEN) {
		return (FALSE);
	}
	(void) sprintf(buf, "passwd.%s", obj->zo_domain);
	if (buf[strlen(buf) - 1] != '.')
		(void) strcat(buf, ".");

	res = nis_lookup(buf, MASTER_ONLY);

	if (res->status == NIS_SUCCESS) {
		tobj = NIS_RES_OBJECT(res);
		if (__type_of(tobj) != NIS_TABLE_OBJ) {
			(void) nis_freeresult(res);
			return (FALSE);
		}
	} else {
		(void) nis_freeresult(res);
		return (FALSE);
	}

	/* check the permission on the table */
	mod_ok = __nis_ck_perms(right, tobj->zo_access, tobj, prin, 1);
	if (mod_ok == TRUE) {
		(void) nis_freeresult(res);
		return (TRUE);
	}

	/* check the permission on the obj */
	mod_ok = __nis_ck_perms(right, obj->zo_access, obj, prin, 1);
	if (mod_ok == TRUE) {
		(void) nis_freeresult(res);
		return (TRUE);
	}

	if (column > tobj->TA_data.ta_maxcol) {
		/* invalid column */
		(void) nis_freeresult(res);
		return (FALSE);
	}
	tc = tobj->TA_data.ta_cols.ta_cols_val;	/* table columns */

	/* check the permission on column being modified */
	mod_ok = __nis_ck_perms(right, tc[column].tc_rights, obj, prin, 1);

	(void) nis_freeresult(res);
	return (mod_ok);
}

void
__npd_gen_rval(randval)
unsigned long *randval;
{
	int		i, shift;
	struct timeval	tv;
	unsigned int	seed = 0;

	for (i = 0; i < 1024; i++) {
		(void) gettimeofday(&tv, NULL);
		shift = i % 8 * sizeof (int);
		seed ^= (tv.tv_usec << shift) | (tv.tv_usec >> (32 - shift));
	}
	(void) srandom(seed);
	*randval = (u_long) random();
}
