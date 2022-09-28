/*
 * ==== hack-attack:  possibly MT-safe but definitely not MT-hot.
 * ==== turn this into a real switch frontend and backends
 *
 * Well, at least the API doesn't involve pointers-to-static.
 */

#ident	"@(#)publickey.c	1.25	98/01/23 SMI"  /*  SVr4 1.2 */

/*
 * Copyright (c) 1986-1992 by Sun Microsystems Inc.
 */

/*
 * publickey.c
 *
 *
 * Public and Private (secret) key lookup routines. These functions
 * are used by the secure RPC auth_des flavor to get the public and
 * private keys for secure RPC principals. Originally designed to
 * talk only to YP, AT&T modified them to talk to files, and now
 * they can also talk to NIS+. The policy for these lookups is now
 * defined in terms of the nameservice switch as follows :
 *	publickey: nis files
 *
 * Note :
 * 1.  NIS+ combines the netid.byname and publickey.byname maps
 *	into a single NIS+ table named cred.org_dir
 * 2.  To use NIS+, the publickey needs to be
 *	publickey: nisplus
 *	(or have nisplus as the first entry).
 *	The nsswitch.conf file should be updated after running nisinit
 *	to reflect this.
 */
#include "../rpc/rpc_mt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <assert.h>
#include <sys/types.h>
#include <rpc/trace.h>
#include <pwd.h>
#include "nsswitch.h"
#include <rpc/rpc.h>
#include <rpc/key_prot.h>
#include <rpcsvc/nis.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/nis_dhext.h>
#include <thread.h>

static const char *PKTABLE = "cred.org_dir";
static const char *PKMAP = "publickey.byname";
static const char *PKFILE = "/etc/publickey";
static const char dh_caps_str[] = "DH";
static const char des_caps_str[] = AUTH_DES_AUTH_TYPE;

#define	PKTABLE_LEN 12
#define	WORKBUFSIZE 1024

extern int xdecrypt();

/*
 * default publickey policy:
 *	publickey: nis [NOTFOUND = return] files
 */


/*	NSW_NOTSUCCESS  NSW_NOTFOUND   NSW_UNAVAIL    NSW_TRYAGAIN */
#define	DEF_ACTION {__NSW_RETURN, __NSW_RETURN, __NSW_CONTINUE, __NSW_CONTINUE}

static struct __nsw_lookup lookup_files = {"files", DEF_ACTION, NULL, NULL},
		lookup_nis = {"nis", DEF_ACTION, NULL, &lookup_files};
static struct __nsw_switchconfig publickey_default =
			{0, "publickey", 2, &lookup_nis};

#ifndef NUL
#define	NUL '\0'
#endif

extern mutex_t serialize_pkey;

static void pkey_cache_add();
static int pkey_cache_get();
static void pkey_cache_flush();

static int extract_secret();

/*
 * These functions are the "backends" for the switch for public keys. They
 * get both the public and private keys from each of the supported name
 * services (nis, nisplus, files). They are passed the appropriate parameters
 * and return 0 if unsuccessful with *errp set, or 1 when they got just the
 * public key and 3 when they got both the public and private keys.
 *
 *
 * getkey_nis()
 *
 * Internal implementation of getpublickey() using NIS (aka Yellow Pages,
 * aka YP).
 *
 * NOTE : *** this function returns nsswitch codes and _not_ the
 * value returned by getpublickey.
 */
static int
getkeys_nis(errp, netname, pkey, skey, passwd)
	int  *errp;
	char *netname;
	char *pkey;
	char *skey;
	char *passwd;
{
	char 	*domain;
	char	*keyval = NULL;
	int	keylen, err, r = 0;
	char	*p;

	trace1(TR_getkeys_nis, 0);

	p = strchr(netname, '@');
	if (! p) {
		*errp = __NSW_UNAVAIL;
		trace1(TR_getkeys_nis, 1);
		return (0);
	}

	domain = ++p;
	err = yp_match(domain, (char *)PKMAP, netname, strlen(netname),
			&keyval, &keylen);
	switch (err) {
	case YPERR_KEY :
		if (keyval)
			free(keyval);
		*errp = __NSW_NOTFOUND;
		trace1(TR_getkeys_nis, 1);
		return (0);
	default :
		if (keyval)
			free(keyval);
		*errp = __NSW_UNAVAIL;
		trace1(TR_getkeys_nis, 1);
		return (0);
	case 0:
		break;
	}

	p = strchr(keyval, ':');
	if (p == NULL) {
		free(keyval);
		*errp = __NSW_NOTFOUND;
		trace1(TR_getkeys_nis, 1);
		return (0);
	}
	*p = 0;
	if (pkey) {
		if (keylen > HEXKEYBYTES) {
			free(keyval);
			*errp = __NSW_NOTFOUND;
			trace1(TR_getkeys_nis, 1);
			return (0);
		}
		(void) strcpy(pkey, keyval);
	}
	r = 1;
	p++;
	if (skey && extract_secret(p, skey, passwd))
		r |= 2;
	free(keyval);
	*errp = __NSW_SUCCESS;
	trace1(TR_getkeys_nis, 1);
	return (r);
}

/*
 * getkey_files()
 *
 * The files version of getpublickey. This function attempts to
 * get the publickey from the file PKFILE .
 *
 * This function defines the format of the /etc/publickey file to
 * be :
 *	netname <whitespace> publickey:privatekey
 *
 * NOTE : *** this function returns nsswitch codes and _not_ the
 * value returned by getpublickey.
 */

static int
getkeys_files(errp, netname, pkey, skey, passwd)
	int	*errp;
	char	*netname;
	char	*pkey;
	char	*skey;
	char	*passwd;
{
	register char *mkey;
	register char *mval;
	char buf[WORKBUFSIZE];
	int	r = 0;
	char *res;
	FILE *fd;
	char *p;

	trace1(TR_getkeys_files, 0);

	fd = fopen(PKFILE, "r");
	if (fd == (FILE *) 0) {
		*errp = __NSW_UNAVAIL;
		trace1(TR_getkeys_files, 1);
		return (0);
	}

	/* Search through the file linearly :-( */
	while ((res = fgets(buf, WORKBUFSIZE, fd)) != NULL) {

		if ((res[0] == '#') || (res[0] == '\n'))
			continue;
		else {
			mkey = strtok(buf, "\t ");
			if (mkey == NULL) {
				syslog(LOG_INFO,
				"getpublickey: Bad record in %s for %s",
							PKFILE, netname);
				continue;
			}
			mval = strtok((char *)NULL, " \t#\n");
			if (mval == NULL) {
				syslog(LOG_INFO,
				"getpublickey: Bad record in %s for %s",
							PKFILE, netname);
				continue;
			}
			/* NOTE : Case insensitive compare. */
			if (strcasecmp(mkey, netname) == 0) {
				p = strchr(mval, ':');
				if (p == NULL) {
					syslog(LOG_INFO,
				"getpublickey: Bad record in %s for %s",
							PKFILE, netname);
					continue;
				}

				*p = 0;
				if (pkey) {
					int len = strlen(mval);

					if (len > HEXKEYBYTES) {
						syslog(LOG_INFO,
				"getpublickey: Bad record in %s for %s",
							PKFILE, netname);
						continue;
					}
					(void) strcpy(pkey, mval);
				}
				r = 1;
				p++;
				if (skey && extract_secret(p, skey, passwd))
					r |= 2;
				(void) fclose(fd);
				*errp = __NSW_SUCCESS;
				trace1(TR_getkeys_files, 1);
				return (r);
			}
		}
	}

	(void) fclose(fd);
	*errp = __NSW_NOTFOUND;
	trace1(TR_getkeys_files, 1);
	return (0);
}

/*
 * getpublickey(netname, key)
 *
 * This is the actual exported interface for this function.
 */

int
__getpublickey_cached(netname, pkey, from_cache)
	char	*netname;
	char	*pkey;
	int	*from_cache;
{
	return (__getpublickey_cached_g(netname, KEYSIZE, 0, pkey,
					HEXKEYBYTES+1, from_cache));
}

int
getpublickey(netname, pkey)
	const char	*netname;
	char		*pkey;
{
	return (__getpublickey_cached((char *)netname, pkey, (int *)0));
}

void
__getpublickey_flush(const char *netname)
{
	pkey_cache_flush(netname);
}

int
getsecretkey(netname, skey, passwd)
	const char	*netname;
	char		*skey;
	const char	*passwd;
{
	return (getsecretkey_g(netname, KEYSIZE, 0, skey, HEXKEYBYTES+1,
				passwd));
}

/*
 *  Routines to cache publickeys.
 */

static NIS_HASH_TABLE pkey_tbl;
struct pkey_item {
	NIS_HASH_ITEM item;
	char *pkey;
};

static void
pkey_cache_add(netname, pkey)
	const char *netname;
	char *pkey;
{
	struct pkey_item *item;

	(void) mutex_lock(&serialize_pkey);
	if (! netname || ! pkey) {
		(void) mutex_unlock(&serialize_pkey);
		return;
	}

	item = (struct pkey_item *)calloc(1, sizeof (struct pkey_item));
	if (item == NULL) {
		(void) mutex_unlock(&serialize_pkey);
		return;
	}
	item->item.name = strdup(netname);
	if (item->item.name == NULL) {
		free((void *)item);
		(void) mutex_unlock(&serialize_pkey);
		return;
	}
	item->pkey = strdup(pkey);
	if (item->pkey == 0) {
		free(item->item.name);
		free(item);
		(void) mutex_unlock(&serialize_pkey);
		return;
	}

	if (!nis_insert_item((NIS_HASH_ITEM *)item, &pkey_tbl)) {
		free(item->item.name);
		free(item->pkey);
		free((void *)item);
		(void) mutex_unlock(&serialize_pkey);
		return;
	}
	(void) mutex_unlock(&serialize_pkey);
}

static int
pkey_cache_get(netname, pkey)
	const char	*netname;
	char		*pkey;
{
	struct pkey_item *item;

	(void) mutex_lock(&serialize_pkey);
	if (! netname || ! pkey) {
		(void) mutex_unlock(&serialize_pkey);
		return (0);
	}

	item = (struct pkey_item *)nis_find_item((char *)netname, &pkey_tbl);
	if (item) {
		(void) strcpy(pkey, item->pkey);
		(void) mutex_unlock(&serialize_pkey);
		return (1);
	}

	(void) mutex_unlock(&serialize_pkey);
	return (0);
}

static void
pkey_cache_flush(netname)
	const char	*netname;
{
	struct pkey_item *item;

	(void) mutex_lock(&serialize_pkey);

	item = (struct pkey_item *)nis_remove_item((char *)netname, &pkey_tbl);
	if (item) {
		free(item->item.name);
		free(item->pkey);
		free((void *)item);
	}
	(void) mutex_unlock(&serialize_pkey);
}

/*
 * Generic DH (any size keys) version of extract_secret.
 */
static int
extract_secret_g(
	char		*raw,		/* in  */
	char		*private,	/* out */
	int		prilen,		/* in  */
	char		*passwd,	/* in  */
	char		*netname,	/* in  */
	keylen_t	keylen,		/* in  */
	algtype_t	algtype)	/* in  */

{
	char	*buf = malloc(strlen(raw) + 1); /* private tmp buf */
	char	*p;

	trace1(TR_extract_secret_g, 0);
	if (! buf || ! passwd || ! raw || ! private || ! prilen ||
		! VALID_KEYALG(keylen, algtype)) {
		if (private)
			*private = NUL;
		if (buf)
			free(buf);
		trace1(TR_extract_secret_g, 1);
		return (0);
	}

	(void) strcpy(buf, raw);

	/* strip off pesky colon if it exists */
	p = strchr(buf, ':');
	if (p) {
		*p = 0;
	}

	/* raw buf has chksum appended, so let's verify it too */
	if (! xdecrypt_g(buf, keylen, algtype, passwd, netname, TRUE)) {
		private[0] = 0;
		free(buf);
		trace1(TR_extract_secret_g, 1);
		return (1); /* yes, return 1 even if xdecrypt fails */
	}

	if (strlen(buf) >= prilen) {
		private[0] = 0;
		free(buf);
		trace1(TR_extract_secret_g, 1);
		return (0);
	}

	(void) strcpy(private, buf);
	free(buf);
	trace1(TR_extract_secret_g, 1);
	return (1);
}

/*
 * extract_secret()
 *
 * This generic function will extract the private key
 * from a string using the given password. Note that
 * it uses the DES based function xdecrypt()
 */
static int
extract_secret(raw, private, passwd)
	char	*raw;
	char	*private;
	char	*passwd;
{
	return (extract_secret_g(raw, private, HEXKEYBYTES+1, passwd,
					NULL, KEYSIZE, 0));
}

/*
 * getkeys_nisplus_g()
 *
 * Fetches the key pair from NIS+.  This version handles any size
 * DH keys.
 */
static int
getkeys_nisplus_g(
	int		*err,		/* in  */
	char		*netname,	/* in  */
	char		*pkey,		/* out */
	int		pkeylen,	/* in  */
	char		*skey,		/* out */
	int		skeylen,	/* in  */
	char		*passwd,	/* in  */
	keylen_t	keylen,		/* in  */
	algtype_t	algtype)	/* in  */
{
	nis_result	*res;
	int		r = 0;
	char		*domain, *p;
	char		buf[NIS_MAXNAMELEN+1];
	char		keytypename[NIS_MAXNAMELEN+1];
	int		len;
	const bool_t	classic_des = AUTH_DES_KEY(keylen, algtype);

	trace1(TR_getkeys_nisplus_g, 0);

	domain = strchr(netname, '@');
	if (! domain) {
		*err = __NSW_UNAVAIL;
		trace1(TR_getkeys_nisplus_g, 1);
		return (0);
	}
	domain++;

	if ((strlen(netname)+PKTABLE_LEN+strlen(domain)+32) >
		(size_t) NIS_MAXNAMELEN) {
		*err = __NSW_UNAVAIL;
		trace1(TR_getkeys_nisplus_g, 1);
		return (0);
	}

	/*
	 * Cred table has following format for PK crypto entries:
	 * cname   auth_type auth_name public  private
	 * ----------------------------------------------------------
	 * nisname	AT	netname	pubkey	prikey
	 *
	 * where AT can be "DES" for classic AUTH_DES, or something like
	 * "DH640-0" for a longer Diffie-Hellman key pair.
	 */
	if (classic_des)
		(void) strcpy(keytypename, des_caps_str);
	else
		(void) sprintf(keytypename, "%s%d-%d",
			dh_caps_str, keylen, algtype);
	(void) sprintf(buf, "[auth_name=\"%s\",auth_type=%s],%s.%s",
		netname, keytypename, PKTABLE, domain);
	if (buf[strlen(buf)-1] != '.')
	(void) strcat(buf, ".");

	/*
	 * Because of some bootstrapping issues (keylogin, etc) the
	 * keys lookup needs to be done without auth.  This is
	 * less-then-ideal from a security perspective and hopefully
	 * will be revisited soon...
	 */
	res = nis_list(buf, USE_DGRAM+NO_AUTHINFO+FOLLOW_LINKS+FOLLOW_PATH,
			NULL, NULL);
	switch (res->status) {
	case NIS_SUCCESS:
	case NIS_S_SUCCESS:
		break;
	case NIS_NOTFOUND:
	case NIS_PARTIAL:
	case NIS_NOSUCHNAME:
	case NIS_NOSUCHTABLE:
		nis_freeresult(res);
		*err = __NSW_NOTFOUND;
		trace1(TR_getkeys_nisplus_g, 1);
		return (0);
	case NIS_S_NOTFOUND:
	case NIS_TRYAGAIN:
		syslog(LOG_ERR, "getkeys: (nis+ key lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		*err = __NSW_TRYAGAIN;
		trace1(TR_getkeys_nisplus_g, 1);
		return (0);
	default:
		*err = __NSW_UNAVAIL;
		syslog(LOG_ERR,
			"getkeys: (nis+ key lookup): %s\n",
			nis_sperrno(res->status));
		nis_freeresult(res);
		trace1(TR_getkeys_nisplus_g, 1);
		return (0);
	}

	if (pkey) {
		len = ENTRY_LEN(res->objects.objects_val, 3);
		if (len > pkeylen) {
			syslog(LOG_ERR,
		"getkeys(nis+): pub key for '%s' (keytype = '%s') too long",
				netname, keytypename);
			nis_freeresult(res);
			return (0);
		}
		(void) strncpy(pkey, ENTRY_VAL(res->objects.objects_val, 3),
				len);
		/*
		 * len has included the terminating null.
		 *
		 * XXX
		 * This is only for backward compatibility with the old cred
		 * table format.  The new one does not have a ':'.
		 */
		p = strchr(pkey, ':');
		if (p)
			*p = NUL;
	}
	r = 1; /* At least public key was found; always true at this point */

	if (skey && extract_secret_g(ENTRY_VAL(res->objects.objects_val, 4),
				skey, skeylen, passwd, netname, keylen,
				algtype))
		r |= 2;

	nis_freeresult(res);
	*err = __NSW_SUCCESS;
	trace1(TR_getkeys_nisplus_g, 1);
	return (r);
}

/*
 * Convert a netname to a name we will hash on.  For classic_des,
 * just copy netname as is.  But for new and improved ("now in
 * new longer sizes!") DHEXT, add a ":keylen-algtype" suffix to hash on.
 *
 * Returns the hashname string on success or NULL on failure.
 */
static char *
netname2hashname(
	const char *netname,
	char *hashname,
	int bufsiz,
	keylen_t keylen,
	algtype_t algtype)
{
	const bool_t classic_des = AUTH_DES_KEY(keylen, algtype);

	trace3(TR_netname2hashname, 0, keylen, algtype);
	if (! netname || ! hashname || ! bufsiz) {
		trace1(TR_netname2hashname, 1);
		return (NULL);
	}

	if (classic_des) {
		if (bufsiz > strlen(netname))
			(void) strcpy(hashname, netname);
		else {
			trace1(TR_netname2hashname, 1);
			return (NULL);
		}
	} else {
		char tmp[128];
		(void) sprintf(tmp, ":%d-%d", keylen, algtype);
		if (bufsiz > (strlen(netname) + strlen(tmp)))
			(void) sprintf(hashname, "%s%s", netname, tmp);
		else {
			trace1(TR_netname2hashname, 1);
			return (NULL);
		}
	}

	trace1(TR_netname2hashname, 1);
	return (hashname);
}

/*
 * Generic DH (any size keys) version of __getpublickey_cached.
 */
int
__getpublickey_cached_g(const char netname[],	/* in  */
			keylen_t keylen,	/* in  */
			algtype_t algtype,	/* in  */
			char *pkey,		/* out */
			size_t pkeylen,		/* in  */
			int *from_cache)	/* in/out  */
{
	int	needfree = 1, res, err;
	struct __nsw_switchconfig *conf;
	struct __nsw_lookup *look;
	enum __nsw_parse_err perr;
	const bool_t classic_des = AUTH_DES_KEY(keylen, algtype);

	trace1(TR_getpublickey_cached_g, 0);
	if (! netname || ! pkey) {
		trace1(TR_getpublickey_cached_g, 1);
		return (0);
	}


	if (from_cache) {
		char hashname[MAXNETNAMELEN];
		if (pkey_cache_get(netname2hashname(netname, hashname,
						    MAXNETNAMELEN, keylen,
						    algtype), pkey)) {
			*from_cache = 1;
			trace1(TR_getpublickey_cached_g, 1);
			return (1);
		}
		*from_cache = 0;
	}

	conf = __nsw_getconfig("publickey", &perr);
	if (! conf) {
		conf = &publickey_default;
		needfree = 0;
	}
	for (look = conf->lookups; look; look = look->next) {
		if (strcmp(look->service_name, "nisplus") == 0) {
			res = getkeys_nisplus_g(&err, (char *) netname,
						pkey, pkeylen,
						(char *) NULL, 0,
						(char *) NULL,
						keylen, algtype);
		/* long DH keys will not be in nis or files */
		} else if (classic_des &&
				strcmp(look->service_name, "nis") == 0)
			res = getkeys_nis(&err, (char *) netname, pkey,
					(char *) NULL, (char *) NULL);
		else if (classic_des &&
				strcmp(look->service_name, "files") == 0)
			res = getkeys_files(&err, (char *) netname, pkey,
					(char *) NULL, (char *) NULL);
		else {
			syslog(LOG_INFO, "Unknown publickey nameservice '%s'",
						look->service_name);
			err = __NSW_UNAVAIL;
		}

		/*
		 *  If we found the publickey, save it in the cache.
		 */
		if (err == __NSW_SUCCESS) {
			char hashname[MAXNETNAMELEN];
			pkey_cache_add(netname2hashname(netname, hashname,
							MAXNETNAMELEN, keylen,
							algtype), pkey);
		}

		switch (look->actions[err]) {
		case __NSW_CONTINUE :
			continue;
		case __NSW_RETURN :
			if (needfree)
				__nsw_freeconfig(conf);
			trace1(TR_getpublickey_cached_g, 1);
			return ((res & 1) != 0);
		default :
			syslog(LOG_INFO, "Unknown action for nameservice %s",
					look->service_name);
		}
	}

	if (needfree)
		__nsw_freeconfig(conf);
	trace1(TR_getpublickey_cached_g, 1);
	return (0);
}


/*
 * The public key cache (used by nisd in this case) must be filled with
 * the data in the NIS_COLD_START file in order for extended Diffie-Hellman
 * operations to work.
 */
void
prime_pkey_cache(directory_obj *dobj)
{
	int scount;

	for (scount = 0; scount < dobj->do_servers.do_servers_len; scount++) {
		nis_server *srv = &(dobj->do_servers.do_servers_val[scount]);
		extdhkey_t	*keylens;
		char		*pkey = NULL, hashname[MAXNETNAMELEN];
		char		netname[MAXNETNAMELEN];
		int		kcount, nkeys = 0;

		(void) host2netname(netname, srv->name, NULL);

		/* determine the number of keys to process */
		if (!(nkeys = __nis_dhext_extract_keyinfo(srv, &keylens)))
			continue;

		/* store them */
		if (srv->key_type == NIS_PK_DHEXT) {
			for (kcount = 0; kcount < nkeys; kcount++) {
				if (!netname2hashname(netname, hashname,
						MAXNETNAMELEN,
						keylens[kcount].keylen,
						keylens[kcount].algtype))
					continue;

				if (!(pkey = __nis_dhext_extract_pkey(
					&srv->pkey, keylens[kcount].keylen,
					keylens[kcount].algtype)))
					continue;

				if (!pkey_cache_get(hashname, pkey))
					pkey_cache_add(hashname, pkey);
			}
		} else {
			if (srv->key_type == NIS_PK_DH) {
				pkey = srv->pkey.n_bytes;

				if (!netname2hashname(netname, hashname,
							MAXNETNAMELEN,
							KEYSIZE, 0))
					continue;

				if (!pkey_cache_get(hashname, pkey))
					pkey_cache_add(hashname, pkey);
			}
		}
	}
}

/*
 * Generic (all sizes) DH version of getpublickey.
 */
int
getpublickey_g(
	const char *netname,	/* in  */
	int keylen,		/* in  */
	int algtype,		/* in  */
	char *pkey,		/* out  */
	size_t pkeylen)		/* in  */
{
	trace1(TR_getpublickey_g, 0);
	return (__getpublickey_cached_g(netname, keylen, algtype, pkey,
					pkeylen, (int *)0));
}

/*
 * Generic (all sizes) DH version of getsecretkey_g.
 */
int
getsecretkey_g(
	const char	*netname,	/* in  */
	keylen_t	keylen,		/* in  */
	algtype_t	algtype,	/* in  */
	char		*skey,		/* out */
	size_t		skeylen,	/* in  */
	const char	*passwd)	/* in  */
{
	int	needfree = 1, res, err;
	struct __nsw_switchconfig *conf;
	struct __nsw_lookup *look;
	enum __nsw_parse_err perr;
	const bool_t classic_des = AUTH_DES_KEY(keylen, algtype);

	trace1(TR_getsecretkey_g, 0);
	if (! netname || !skey || ! skeylen) {
		trace1(TR_getsecretkey_g, 1);
		return (0);
	}

	conf = __nsw_getconfig("publickey", &perr);

	if (! conf) {
		conf = &publickey_default;
		needfree = 0;
	}

	for (look = conf->lookups; look; look = look->next) {
		if (strcmp(look->service_name, "nisplus") == 0)
			res = getkeys_nisplus_g(&err, (char *) netname,
					(char *) NULL, 0, skey, skeylen,
					(char *) passwd, keylen, algtype);
		/* long DH keys will not be in nis or files */
		else if (classic_des && strcmp(look->service_name, "nis") == 0)
			res = getkeys_nis(&err, (char *) netname,
					(char *) NULL, skey, (char *) passwd);
		else if (classic_des &&
				strcmp(look->service_name, "files") == 0)
			res = getkeys_files(&err, (char *) netname,
					(char *) NULL, skey, (char *) passwd);
		else {
			syslog(LOG_INFO, "Unknown publickey nameservice '%s'",
						look->service_name);
			err = __NSW_UNAVAIL;
		}
		switch (look->actions[err]) {
		case __NSW_CONTINUE :
			continue;
		case __NSW_RETURN :
			if (needfree)
				__nsw_freeconfig(conf);
			trace1(TR_getsecretkey_g, 1);
			return ((res & 2) != 0);
		default :
			syslog(LOG_INFO, "Unknown action for nameservice %s",
					look->service_name);
		}
	}
	if (needfree)
		__nsw_freeconfig(conf);
	trace1(TR_getsecretkey_g, 1);
	return (0);
}
