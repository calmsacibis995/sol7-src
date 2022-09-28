/*
 *	gethostent.c
 *
 *	Copyright (c) 1988-1997 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma	ident	"@(#)gethostent.c	1.25	97/08/27 SMI"

/*
 * In order to avoid duplicating libresolv code here, and since libresolv.so.2
 * provides res_-equivalents of the getXbyY and {set,get}Xent, lets call
 * re_gethostbyaddr() and so on from this file. Among other things, this
 * should help us avoid problems like the one described in bug 1264386,
 * where the internal getanswer() acquired new functionality in BIND 4.9.3,
 * but the local copy of getanswer() in this file wasn't updated, so that new
 * functionality wasn't available to the name service switch.
 */

#define	gethostbyaddr	res_gethostbyaddr
#define	gethostbyname	res_gethostbyname
#define	sethostent	res_sethostent
#define	endhostent	res_endhostent

#include	<stdio.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include	<netdb.h>
#include	<strings.h>
#include	<thread.h>
#include	<arpa/nameser.h>
#include	<resolv.h>
#include	<syslog.h>
#include	<nsswitch.h>
#include	<nss_dbdefs.h>
#include 	<stdlib.h>
#include 	<signal.h>

/*
 * These should really be in some header.
 */
extern int _thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset);
extern int _mutex_lock(mutex_t *mp);
extern int _mutex_unlock(mutex_t *mp);

/*
 * This is declared here because we cannot include <arpa/inet.h>,
 * since our version of inet_addr must be static.
 */
extern char *inet_ntoa(struct in_addr in);

/*
 * If the DNS name service switch routines are used in a binary that depends
 * on an older libresolv (libresolv.so.1, say), then having nss_dns.so.1 or
 * libnss_dns.a depend on a newer libresolv (libresolv.so.2) will cause
 * relocation problems. In particular, copy relocation of the _res structure
 * (which changes in size from libresolv.so.1 to libresolv.so.2) could
 * cause corruption, and result in a number of strange problems, including
 * core dumps. Hence, we check if a libresolv is already loaded.
 */

#include	<dlfcn.h>

#pragma init	(_nss_dns_init)
static void	_nss_dns_init(void);
static in_addr_t inet_addr(const char *cp);
static struct hostent *_gethostbyname(int *h_errnop, const char *name);
static struct hostent *_gethostbyaddr(int *h_errnop, const char *addr,
    int len, int type);
int ent2result(struct hostent *he, nss_XbyY_args_t *argp);
int dns_netdb_aliases(char **from_list, char **to_list, char **aliaspp, 
						int type, int *count);
#pragma weak	res_gethostbyname
#pragma weak	res_gethostbyaddr
#pragma weak	res_sethostent
#pragma weak	res_endhostent

#define		RES_SET_NO_HOSTS_FALLBACK	"__res_set_no_hosts_fallback"
extern void	__res_set_no_hosts_fallback(void);
#pragma weak	__res_set_no_hosts_fallback

/* Usually set from the Makefile */
#ifndef	NSS_DNS_LIBRESOLV
#define	NSS_DNS_LIBRESOLV	"libresolv.so.2"
#endif

typedef	struct	dns_backend	*dns_backend_ptr_t;
typedef	nss_status_t	(*dns_backend_op_t)(dns_backend_ptr_t, void *);

struct dns_backend {
	dns_backend_op_t	*ops;
	nss_dbop_t		n_ops;
};

nss_backend_t *_nss_dns_constr(dns_backend_op_t ops[], int n_ops);

extern	nss_status_t _herrno2nss(int h_errno);

/* From libresolv */
extern	int	h_errno;

static mutex_t	one_lane = DEFAULTMUTEX;

typedef union {
	long al;
	char ac;
} align;

static struct in_addr host_addr;

void
_nss_dns_init(void)
{
	struct hostent	*(*f_hostent_ptr)();
	void		*reslib, (*f_void_ptr)();

	/* If no libresolv library, then load one */
	if ((f_hostent_ptr = res_gethostbyname) == 0) {
		if ((reslib =
		     dlopen(NSS_DNS_LIBRESOLV, RTLD_LAZY|RTLD_GLOBAL)) != 0) {
			/* Turn off /etc/hosts fall back in libresolv */
			if ((f_void_ptr = (void (*)(void))dlsym(reslib,
				RES_SET_NO_HOSTS_FALLBACK)) != 0) {
			(*f_void_ptr)();
			}
		}
	} else {
		/* Libresolv already loaded */
		if ((f_void_ptr = __res_set_no_hosts_fallback) != 0) {
			(*f_void_ptr)();
		}
	}
}

/*
 * Internet Name Domain Server (DNS) only implementation.
 */
static struct hostent *
_gethostbyaddr(int *h_errnop, const char *addr, int len, int type)
{
	struct hostent	*hp;

	hp = gethostbyaddr(addr, len, type);
	*h_errnop = h_errno;
	return(hp);
}

static struct hostent *
_gethostbyname(int *h_errnop, const char *name)
{
	struct hostent *hp;

	hp = gethostbyname(name);
	*h_errnop = h_errno;
	return(hp);
}

void
_sethostent(errp, stayopen)
	nss_status_t	*errp;
	int		stayopen;
{
	int	ret;

	ret = sethostent(stayopen);
	if (ret == 0)
		*errp = NSS_SUCCESS;
	else
		*errp = NSS_UNAVAIL;
}

void
_endhostent(errp)
	nss_status_t	*errp;
{
	int	ret;

	ret = endhostent();
	if (ret == 0)
		*errp = NSS_SUCCESS;
	else
		*errp = NSS_UNAVAIL;
}

/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 *
 * Duplicated here to avoid dependecy on libnsl.
 */
static in_addr_t
inet_addr(const char *cp)
{
	uint32_t val;
	int base, n;
	char	c;
	u_int parts[4];
	u_int *pp = parts;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0') {
		if (*++cp == 'x' || *cp == 'X')
			base = 16, cp++;
		else
			base = 8;
	}
	while (c = *cp) {
		if (isdigit(c)) {
			if ((c - '0') >= base)
				break;
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 3)
			return (-1);
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp))
		return (-1);
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = (int32_t) (pp - parts);
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
				((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		return (-1);
	}
	val = htonl(val);
	return (val);
}


/*
 * Section below is added to serialize the DNS backend.
 */


/*ARGSUSED*/
static nss_status_t
getbyname(be, a)
	dns_backend_ptr_t	be;
	void			*a;
{
	struct hostent	*he;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *) a;
	int		ret;

	sigset_t	oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);

	he = _gethostbyname(&argp->h_errno, argp->key.name);
	if (he != NULL) {
		ret = ent2result(he, a);
		if (ret == NSS_STR_PARSE_SUCCESS) {
			argp->returnval = argp->buf.result;
		} else {
			argp->h_errno = HOST_NOT_FOUND;
			if (ret == NSS_STR_PARSE_ERANGE) {
				argp->erange = 1;
			}
		}
	}
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

	return (_herrno2nss(argp->h_errno));
}


/*ARGSUSED*/
static nss_status_t
getbyaddr(be, a)
	dns_backend_ptr_t	be;
	void			*a;
{
	size_t	n;
	struct hostent	*he, *he2;
	nss_XbyY_args_t	*argp = (nss_XbyY_args_t *) a;
	int		ret, save_h_errno;
	char		**ans, hbuf[MAXHOSTNAMELEN];

	sigset_t	oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);

	he = _gethostbyaddr(&argp->h_errno, argp->key.hostaddr.addr,
		argp->key.hostaddr.len, argp->key.hostaddr.type);
	if (he != NULL) {

		if (strlen(he->h_name) >= MAXHOSTNAMELEN)
			ret = NSS_STR_PARSE_ERANGE;
		else {
			/* save a copy of the (alleged) hostname */
			(void) strcpy(hbuf, he->h_name);
			n = strlen(hbuf);
			if (n < MAXHOSTNAMELEN-1 && hbuf[n-1] != '.') {
				strcat(hbuf, ".");
			}
			ret = ent2result(he, a);
			save_h_errno = argp->h_errno;
		}
		if (ret == NSS_STR_PARSE_SUCCESS) {
			/*
			 * check to make sure by doing a forward query
			 * We use _gethostbyname() to avoid the stack, and
			 * then we throw the result from argp->h_errno away,
			 * becase we don't care.  And besides you want the
			 * return code from _gethostbyaddr() anyway.
			 */
			he2 = _gethostbyname(&argp->h_errno, hbuf);

			if (he2 != (struct hostent *)NULL) {
				/* until we prove name and addr match */
				argp->h_errno = HOST_NOT_FOUND;
				for (ans = he2->h_addr_list; *ans; ans++)
					if (memcmp(*ans,
						argp->key.hostaddr.addr,
						he2->h_length) == 0) {
					argp->h_errno = save_h_errno;
					argp->returnval = argp->buf.result;
					break;
				}
			} else {
				/*
				 * What to do if _gethostbyname() fails ???
				 * We assume they are doing something stupid
				 * like registering addresses but not names
				 * (some people actually think that provides
				 * some "security", through obscurity).  So for
				 * these poor lost souls, because we can't
				 * PROVE spoofing and because we did try (and
				 * we don't want a bug filed on this), we let
				 * this go.  And return the name from byaddr.
				 */
				argp->h_errno = save_h_errno;
				argp->returnval = argp->buf.result;
			}
			/* we've been spoofed, make sure to log it. */
			if (argp->h_errno == HOST_NOT_FOUND)
				syslog(LOG_NOTICE, "gethostbyaddr: %s != %s",
		hbuf, inet_ntoa(*(struct in_addr *)argp->key.hostaddr.addr));
		} else {
			argp->h_errno = HOST_NOT_FOUND;
			if (ret == NSS_STR_PARSE_ERANGE) {
				argp->erange = 1;
			}
		}
	}
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

	return (_herrno2nss(argp->h_errno));
}


/*ARGSUSED*/
nss_status_t
_nss_dns_getent(be, args)
	dns_backend_ptr_t	be;
	void			*args;
{
	return (NSS_UNAVAIL);
}


/*ARGSUSED*/
nss_status_t
_nss_dns_setent(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	nss_status_t	errp;

	sigset_t	oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);

	_sethostent(&errp, 1);

	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

	return (errp);
}


/*ARGSUSED*/
nss_status_t
_nss_dns_endent(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	nss_status_t	errp;

	sigset_t	oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);

	_endhostent(&errp);

	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

	return (errp);
}


/*ARGSUSED*/
nss_status_t
_nss_dns_destr(be, dummy)
	dns_backend_ptr_t	be;
	void			*dummy;
{
	nss_status_t	errp;

	if (be != 0) {
		/* === Should change to invoke ops[ENDENT] ? */
		sigset_t	oldmask, newmask;

		sigfillset(&newmask);
		_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
		_mutex_lock(&one_lane);

		_endhostent(&errp);

		_mutex_unlock(&one_lane);
		_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

		free(be);
	}
	return (NSS_SUCCESS);   /* In case anyone is dumb enough to check */
}



nss_backend_t *
_nss_dns_constr(dns_backend_op_t ops[], int n_ops)
{
	dns_backend_ptr_t	be;

	if ((be = (dns_backend_ptr_t) malloc(sizeof (*be))) == 0)
		return (0);

	be->ops = ops;
	be->n_ops = n_ops;
	return ((nss_backend_t *) be);
}


static dns_backend_op_t host_ops[] = {
	_nss_dns_destr,
	_nss_dns_endent,
	_nss_dns_setent,
	_nss_dns_getent,
	getbyname,
	getbyaddr
};

/*ARGSUSED*/
nss_backend_t *
_nss_dns_hosts_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	return (_nss_dns_constr(host_ops,
		sizeof (host_ops) / sizeof (host_ops[0])));
}


#define	DNS_ALIASES	0
#define	DNS_ADDRLIST	1

int
ent2result(he, argp)
	struct hostent		*he;
	nss_XbyY_args_t		*argp;
{
	char		*buffer, *limit;
	int		buflen = argp->buf.buflen;
	int		ret, count;
	size_t len;
	struct hostent 	*host;
	struct in_addr	*addrp;

	limit = argp->buf.buffer + buflen;
	host = (struct hostent *) argp->buf.result;
	buffer = argp->buf.buffer;

	/* h_addrtype and h_length */
	host->h_addrtype = AF_INET;
	host->h_length = sizeof (u_long);

	/* h_name */
	len = strlen(he->h_name) + 1;
	host->h_name = buffer;
	if (host->h_name + len >= limit)
		return (NSS_STR_PARSE_ERANGE);
	memcpy(host->h_name, he->h_name, len);
	buffer += len;

	/* h_addr_list */
	addrp = (struct in_addr *) ROUND_DOWN(limit, sizeof (*addrp));
	host->h_addr_list = (char **) ROUND_UP(buffer, sizeof (char **));
	ret = dns_netdb_aliases(he->h_addr_list, host->h_addr_list,
		(char **)&addrp, DNS_ADDRLIST, &count);
	if (ret != NSS_STR_PARSE_SUCCESS)
		return (ret);

	/* h_aliases */
	host->h_aliases = host->h_addr_list + count + 1;
	ret = dns_netdb_aliases(he->h_aliases, host->h_aliases,
		(char **)&addrp, DNS_ALIASES, &count);
	if (ret == NSS_STR_PARSE_PARSE)
		ret = NSS_STR_PARSE_SUCCESS;

	return (ret);

}



int
dns_netdb_aliases(from_list, to_list, aliaspp, type, count)
	char	**from_list, **to_list, **aliaspp;
	int	type, *count;
{
	char	*fstr;
	int	cnt = 0;
	size_t len;

	*count = 0;
	if ((char *)to_list >= *aliaspp)
		return (NSS_STR_PARSE_ERANGE);

	for (fstr = from_list[cnt]; fstr != NULL; fstr = from_list[cnt]) {
		if (type == DNS_ALIASES)
			len = strlen(fstr) + 1;
		else
			len = sizeof (u_long);
		*aliaspp -= len;
		to_list[cnt] = *aliaspp;
		if (*aliaspp <= (char *)&to_list[cnt+1])
			return (NSS_STR_PARSE_ERANGE);
		memcpy (*aliaspp, fstr, len);
		++cnt;
	}
	to_list[cnt] = NULL;

	*count = cnt;
	if (cnt == 0)
		return (NSS_STR_PARSE_PARSE);

	return (NSS_STR_PARSE_SUCCESS);
}
