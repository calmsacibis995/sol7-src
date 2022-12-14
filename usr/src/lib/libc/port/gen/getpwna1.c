/*
 *	Copyright (c) 1991-19996, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma	ident	"@(#)getpwnam_r.c	1.19	97/07/31 SMI"

/*LINTLIBRARY*/

#pragma weak endpwent	= _endpwent
#pragma weak setpwent	= _setpwent
#pragma weak getpwnam_r	= _getpwnam_r
#pragma weak getpwuid_r	= _getpwuid_r
#pragma weak getpwent_r	= _getpwent_r
#pragma weak fgetpwent_r = _fgetpwent_r

#include "synonyms.h"
#include <sys/types.h>
#include <pwd.h>
#include <nss_dbdefs.h>
#include <stdio.h>
#include <synch.h>
#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>

int str2passwd(const char *, int, void *,
	char *, int);

static DEFINE_NSS_DB_ROOT(db_root);
static DEFINE_NSS_GETENT(context);

void
_nss_initf_passwd(nss_db_params_t *p)
{
	p->name	= NSS_DBNAM_PASSWD;
	p->default_config = NSS_DEFCONF_PASSWD;
}

#ifdef PIC

#include <getxby_door.h>

struct passwd *
_uncached_getpwuid_r(uid_t uid, struct passwd *result, char *buffer,
	int buflen);

struct passwd *
_uncached_getpwnam_r(const char *name, struct passwd *result, char *buffer,
    int buflen);

static struct passwd *
process_getpw(struct passwd *result, char *buffer, int buflen,
	nsc_data_t *sptr, int ndata);

static struct passwd *
process_getpw(struct passwd *result, char *buffer, int buflen,
	nsc_data_t *sptr, int ndata)
{

	char *fixed;
#ifdef	_LP64
	struct passwd	pass64;
#endif

#ifdef	_LP64
	fixed = (char *)(((uintptr_t)buffer + 7) & ~7);
#else
	fixed = (char *)(((uintptr_t)buffer + 3) & ~3);
#endif
	buflen -= fixed - buffer;
	buffer = fixed;

	if (sptr->nsc_ret.nsc_return_code != SUCCESS)
		return (NULL);

#ifdef	_LP64
	if (sptr->nsc_ret.nsc_bufferbytesused - sizeof (passwd32_t)
	    > buflen) {
#else
	if (sptr->nsc_ret.nsc_bufferbytesused - sizeof (struct passwd)
	    > buflen) {
#endif
		errno = ERANGE;
		return (NULL);
	}

#ifdef	_LP64

	memcpy(buffer, (sptr->nsc_ret.nsc_u.buff + sizeof (passwd32_t)),
	    (sptr->nsc_ret.nsc_bufferbytesused - sizeof (passwd32_t)));

	pass64.pw_name = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_name +
				(uintptr_t) buffer);
	pass64.pw_passwd = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_passwd +
				(uintptr_t) buffer);
	pass64.pw_uid = sptr->nsc_ret.nsc_u.pwd.pw_uid;
	pass64.pw_gid = sptr->nsc_ret.nsc_u.pwd.pw_gid;
	pass64.pw_age = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_age +
				(uintptr_t) buffer);
	pass64.pw_comment = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_comment +
				(uintptr_t) buffer);
	pass64.pw_gecos = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_gecos +
				(uintptr_t) buffer);
	pass64.pw_dir = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_dir +
				(uintptr_t) buffer);
	pass64.pw_shell = (char *) (sptr->nsc_ret.nsc_u.pwd.pw_shell +
				(uintptr_t) buffer);

	*result = pass64;
#else
	sptr->nsc_ret.nsc_u.pwd.pw_name += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_passwd += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_age += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_comment += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_gecos += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_dir += (uintptr_t) buffer;
	sptr->nsc_ret.nsc_u.pwd.pw_shell += (uintptr_t) buffer;

	*result = sptr->nsc_ret.nsc_u.pwd;

	memcpy(buffer, (sptr->nsc_ret.nsc_u.buff + sizeof (struct passwd)),
	    (sptr->nsc_ret.nsc_bufferbytesused - sizeof (struct passwd)));
#endif

	return (result);
}

/*
 * POSIX.1c Draft-6 version of the function getpwnam_r.
 * It was implemented by Solaris 2.3.
 */
struct passwd *
_getpwnam_r(const char *name, struct passwd *result, char *buffer, int buflen)
{
	/*
	 * allocate data on the stack for passwd information
	 */
	union {
		nsc_data_t 	s_d;
		char		s_b[1024];
	} space;
	nsc_data_t	*sptr;
	int		ndata;
	int		adata;
	struct passwd	*resptr = NULL;

	if ((name == (const char *)NULL) ||
	    (strlen(name) >= (sizeof (space) - sizeof (nsc_data_t)))) {
		errno = ERANGE;
		return ((struct passwd *)NULL);
	}
	ndata = sizeof (space);
	adata = strlen(name) + sizeof (nsc_call_t) + 1;
	space.s_d.nsc_call.nsc_callnumber = GETPWNAM;
	strcpy(space.s_d.nsc_call.nsc_u.name, name);
	sptr = &space.s_d;

	switch (_nsc_trydoorcall(&sptr, &ndata, &adata)) {
	case SUCCESS:	/* positive cache hit */
		break;
	case NOTFOUND:	/* negative cache hit */
		return (NULL);
	default:
		return ((struct passwd *)_uncached_getpwnam_r(name, result,
		    buffer, buflen));
	}
	resptr = process_getpw(result, buffer, buflen, sptr, ndata);

	/*
	 * check if doors reallocated the memory underneath us
	 * if they did munmap it or suffer a memory leak
	 */
	if (sptr != &space.s_d)
		munmap((void *)sptr, ndata);

	return (resptr);
}

/*
 * POSIX.1c Draft-6 version of the function getpwuid_r.
 * It was implemented by Solaris 2.3.
 */
struct passwd *
_getpwuid_r(uid_t uid, struct passwd *result, char *buffer, int buflen)
{
	union {
		nsc_data_t	s_d;
		char		s_b[1024];
	} space;
	nsc_data_t	*sptr;
	int		ndata;
	int		adata;
	struct passwd	*resptr = NULL;

	ndata = sizeof (space);
	adata = sizeof (nsc_call_t) + 1;
	space.s_d.nsc_call.nsc_callnumber = GETPWUID;
	space.s_d.nsc_call.nsc_u.uid = uid;
	sptr = &space.s_d;

	switch (_nsc_trydoorcall(&sptr, &ndata, &adata)) {
	case SUCCESS:	/* positive cache hit */
		break;
	case NOTFOUND:	/* negative cache hit */
		return (NULL);
	default:
		return ((struct passwd *)_uncached_getpwuid_r(uid, result,
		    buffer, buflen));
	}
	resptr = process_getpw(result, buffer, buflen, sptr, ndata);

	/*
	 * check if doors reallocated the memory underneath us
	 * if they did munmap it or suffer a memory leak
	 */
	if (sptr != &space.s_d)
		munmap((void *)sptr, ndata);

	return (resptr);
}


struct passwd *
_uncached_getpwuid_r(uid_t uid, struct passwd *result, char *buffer,
	int buflen)

#else

/*
 * POSIX.1c Draft-6 version of the function getpwuid_r.
 * It was implemented by Solaris 2.3.
 */
struct passwd *
_getpwuid_r(uid_t uid, struct passwd *result, char *buffer, int buflen)

#endif PIC

{
	nss_XbyY_args_t arg;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2passwd);
	arg.key.uid = uid;
	(void) nss_search(&db_root, _nss_initf_passwd, NSS_DBOP_PASSWD_BYUID,
				&arg);
	return ((struct passwd *) NSS_XbyY_FINI(&arg));
}


/*
 * POSIX.1c standard version of the function getpwuid_r.
 * User gets it via static getpwuid_r from the header file.
 */
int
__posix_getpwuid_r(uid_t uid, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result)
{
	int nerrno = 0;
	int oerrno = errno;

	errno = 0;
	if ((*result = _getpwuid_r(uid, pwd, buffer, (uintptr_t)bufsize))
		== NULL) {
		if (errno == 0)
			nerrno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}

#ifdef PIC

struct passwd *
_uncached_getpwnam_r(const char *name, struct passwd *result, char *buffer,
	int buflen)

#else

/*
 * POSIX.1c Draft-6 version of the function getpwnam_r.
 * It was implemented by Solaris 2.3.
 */
struct passwd *
_getpwnam_r(const char *name, struct passwd *result, char *buffer, int buflen)

#endif PIC

{
	nss_XbyY_args_t arg;

	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2passwd);
	arg.key.name = name;
	(void) nss_search(&db_root, _nss_initf_passwd, NSS_DBOP_PASSWD_BYNAME,
				&arg);
	return ((struct passwd *) NSS_XbyY_FINI(&arg));
}

/*
 * POSIX.1c standard version of the function getpwnam_r.
 * User gets it via static getpwnam_r from the header file.
 */
int
__posix_getpwnam_r(const char *name, struct passwd *pwd, char *buffer,
    size_t bufsize, struct passwd **result)
{
	int nerrno = 0;
	int oerrno = errno;

	errno = 0;
	if ((*result = _getpwnam_r(name, pwd, buffer, (uintptr_t)bufsize))
		== NULL) {
		if (errno == 0)
			nerrno = EINVAL;
		else
			nerrno = errno;
	}
	errno = oerrno;
	return (nerrno);
}

void
setpwent(void)
{
	nss_setent(&db_root, _nss_initf_passwd, &context);
}

void
endpwent(void)
{
	nss_endent(&db_root, _nss_initf_passwd, &context);
	nss_delete(&db_root);
}

struct passwd *
getpwent_r(struct passwd *result, char *buffer, int buflen)
{
	nss_XbyY_args_t arg;
	char		*nam;

	/* In getXXent_r(), protect the unsuspecting caller from +/- entries */

	do {
		NSS_XbyY_INIT(&arg, result, buffer, buflen, str2passwd);
		/* No key to fill in */
		(void) nss_getent(&db_root, _nss_initf_passwd, &context, &arg);
	} while (arg.returnval != 0 &&
	    (nam = ((struct passwd *)arg.returnval)->pw_name) != 0 &&
		(*nam == '+' || *nam == '-'));

	return ((struct passwd *) NSS_XbyY_FINI(&arg));
}

struct passwd *
fgetpwent_r(FILE *f, struct passwd *result, char *buffer, int buflen)
{
	extern void	_nss_XbyY_fgets(FILE *, nss_XbyY_args_t *);
	nss_XbyY_args_t	arg;

	/* ... but in fgetXXent_r, the caller deserves any +/- entry he gets */

	/* No key to fill in */
	NSS_XbyY_INIT(&arg, result, buffer, buflen, str2passwd);
	_nss_XbyY_fgets(f, &arg);
	return ((struct passwd *) NSS_XbyY_FINI(&arg));
}

static char *
gettok(char **nextpp)
{
	char	*p = *nextpp;
	char	*q = p;
	char	c;

	if (p == 0)
		return (0);

	while ((c = *q) != '\0' && c != ':')
		q++;

	if (c == '\0')
		*nextpp = 0;
	else {
		*q++ = '\0';
		*nextpp = q;
	}
	return (p);
}

/*
 * Return values: 0 = success, 1 = parse error, 2 = erange ...
 * The structure pointer passed in is a structure in the caller's space
 * wherein the field pointers would be set to areas in the buffer if
 * need be. instring and buffer should be separate areas.
 */
int
str2passwd(const char *instr, int lenstr, void *ent, char *buffer, int buflen)
{
	struct passwd	*passwd	= (struct passwd *)ent;
	char		*p, *next;
	int		black_magic;	/* "+" or "-" entry */

	if (lenstr + 1 > buflen)
		return (NSS_STR_PARSE_ERANGE);

	/*
	 * We copy the input string into the output buffer and
	 * operate on it in place.
	 */
	(void) memcpy(buffer, instr, lenstr);
	buffer[lenstr] = '\0';

	next = buffer;

	passwd->pw_name = p = gettok(&next);		/* username */
	if (*p == '\0') {
		/* Empty username;  not allowed */
		return (NSS_STR_PARSE_PARSE);
	}
	black_magic = (*p == '+' || *p == '-');
	if (black_magic) {
		passwd->pw_uid = UID_NOBODY;
		passwd->pw_gid = GID_NOBODY;
		/*
		 *  pwconv tests pw_passwd and pw_age == NULL
		 */
		passwd->pw_passwd  = "";
		passwd->pw_age	= "";
		/*
		 * the rest of the passwd entry is "optional"
		 */
		passwd->pw_comment = "";
		passwd->pw_gecos = "";
		passwd->pw_dir	= "";
		passwd->pw_shell = "";
	}

	passwd->pw_passwd = p = gettok(&next);		/* password */
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	for (; *p != '\0';  p++) {			/* age */
		if (*p == ',') {
			*p++ = '\0';
			break;
		}
	}
	passwd->pw_age = p;

	p = next;					/* uid */
	if (p == 0 || *p == '\0') {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	if (!black_magic) {
		passwd->pw_uid = (uid_t)strtol(p, &next, 10);
		if (next == p) {
			/* uid field should be nonempty */
			return (NSS_STR_PARSE_PARSE);
		}
		/*
		 * The old code (in 2.0 through 2.5) would check
		 * for the uid being negative, or being greater
		 * than 60001 (the rfs limit).  If it met either of
		 * these conditions, the uid was translated to 60001.
		 *
		 * Now we just check for negative uids; anything else
		 * is administrative policy
		 */
		if (passwd->pw_uid < 0)
			passwd->pw_uid = UID_NOBODY;
	}
	if (*next++ != ':') {
		if (black_magic)
			(void) gettok(&next);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	p = next;					/* gid */
	if (p == 0 || *p == '\0') {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}
	if (!black_magic) {
		passwd->pw_gid = (gid_t)strtol(p, &next, 10);
		if (next == p) {
			/* gid field should be nonempty */
			return (NSS_STR_PARSE_PARSE);
		}
		/*
		 * gid should be non-negative; anything else
		 * is administrative policy.
		 */
		if (passwd->pw_gid < 0)
			passwd->pw_gid = GID_NOBODY;
	}
	if (*next++ != ':') {
		if (black_magic)
			(void) gettok(&next);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_gecos = passwd->pw_comment = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_dir = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	passwd->pw_shell = p = gettok(&next);
	if (p == 0) {
		if (black_magic)
			return (NSS_STR_PARSE_SUCCESS);
		else
			return (NSS_STR_PARSE_PARSE);
	}

	/* Better not be any more fields... */
	if (next == 0) {
		/* Successfully parsed and stored */
		return (NSS_STR_PARSE_SUCCESS);
	}
	return (NSS_STR_PARSE_PARSE);
}
