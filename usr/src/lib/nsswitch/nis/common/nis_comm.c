/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 *
 * Common code and structures used by name-service-switch "nis" backends.
 */

#pragma ident	"@(#)nis_common.c	1.14	97/08/27 SMI"

#include "nis_common.h"
#include <string.h>
#include <synch.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yp_prot.h>
#include <thread.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>


struct nis_backend {
	nis_backend_op_t	*ops;
	nss_dbop_t		n_ops;
	const char		*domain;
	const char		*enum_map;
	char			*enum_key;
	int			enum_keylen;
};


#ifndef	MT_UNSAFE_YP		/* Is the libnsl YP client code MT-unsafe? */
#define	MT_UNSAFE_YP	0	/* No, not any longer */
#endif

#if	MT_UNSAFE_YP
static mutex_t	one_lane = DEFAULTMUTEX;
#endif

/* <rpcsvc/ypclnt.h> uses (char *) where it should use (const char *) */
typedef char *grrr;

/*
 * The YP client code thinks it's being helpful by appending '\n' and '\0'
 *   to the values returned by yp_match() et al.  In order to do this it
 *   ends up doing more malloc()ing and data copying than would otherwise
 *   be necessary.  If we're interested in performance we should provide
 *   alternative library interfaces that skip the helpfulness and instead
 *   let the XDR routines dump the value directly into the buffer where
 *   we really want it.  For now, though, we just use the vanilla interface.
 */

static nss_status_t
switch_err(ypstatus, ismatch)
	int			ypstatus;
	int			ismatch;
{
	switch (ypstatus) {
	    case 0:
		return (NSS_SUCCESS);

	    case YPERR_BADARGS:
	    case YPERR_KEY:
		return (NSS_NOTFOUND);

	    /*
	     *  When the YP server is running in DNS forwarding mode,
	     *  the forwarder will return YPERR_NOMORE to us if it
	     *  is unable to contact a server (i.e., it has timed out).
	     *  The NSS_TRYAGAIN is returned for timeout errors.
	     */
	    case YPERR_NOMORE:
		if (ismatch)
			return (NSS_TRYAGAIN);
		else
			return (NSS_NOTFOUND);

	    case YPERR_BUSY:
		return (NSS_TRYAGAIN); /* :-) */

	    default:
		return (NSS_UNAVAIL);
	}
}

/*ARGSUSED*/
nss_status_t
_nss_nis_setent(be, dummy)
	nis_backend_ptr_t	be;
	void			*dummy;
{
	if (be->enum_key != 0) {
		free(be->enum_key);
		be->enum_key = 0;
	}
	be->enum_keylen = 0;
	return (NSS_SUCCESS);
}

nss_status_t
_nss_nis_endent(be, dummy)
	nis_backend_ptr_t	be;
	void			*dummy;
{
	return (_nss_nis_setent(be, dummy));
	/* Nothing else we can clean up, is there? */
}

static void
massage_netdb(const char **valp, int *vallenp)
{
	const char		*first;
	const char		*last;
	const char		*val	= *valp;
	int			vallen	= *vallenp;

	if ((last = memchr(val, '#', vallen)) == 0) {
		last = val + vallen;
	}
	for (first = val;  first < last && isspace(*first);  first++) {
		;
	}
	for (/* cstyle */;  first < last && isspace(last[-1]);  last--) {
		;
	}
	/*
	 * Don't check for an empty line because it shouldn't ever
	 *   have made it into the YP map.
	 */
	*valp = first;
	*vallenp = (int) (last - first);
}

nss_status_t
_nss_nis_ypmatch(domain, map, key, valp, vallenp, ypstatusp)
	const char		*domain;
	const char		*map;
	const char		*key;
	char			**valp;
	int			*vallenp;
	int			*ypstatusp;
{
	int			ypstatus;

#if	MT_UNSAFE_YP
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);
#endif
	ypstatus = yp_match((grrr)domain, (grrr)map,
			    (grrr)key, (int) strlen(key), valp, vallenp);
#if	MT_UNSAFE_YP
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
#endif

	if (ypstatusp != 0) {
		*ypstatusp = ypstatus;
	}
	return (switch_err(ypstatus, 1));
}

/*
 * XXX special version of _nss_nis_ypmatch() for handling C2 (passwd.adjunct)
 * lookups when we need a reserved port.
 */
nss_status_t
_nss_nis_ypmatch_rsvdport(domain, map, key, valp, vallenp, ypstatusp)
	const char		*domain;
	const char		*map;
	const char		*key;
	char			**valp;
	int			*vallenp;
	int			*ypstatusp;
{
	int			ypstatus;
	extern int yp_match_rsvdport();

#if	MT_UNSAFE_YP
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);
#endif
	ypstatus = yp_match_rsvdport((grrr)domain, (grrr)map,
			    (grrr)key, strlen(key), valp, vallenp);
#if	MT_UNSAFE_YP
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
#endif

	if (ypstatusp != 0) {
		*ypstatusp = ypstatus;
	}
	return (switch_err(ypstatus, 1));
}

nss_status_t
_nss_nis_lookup(be, args, netdb, map, key, ypstatusp)
	nis_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	int			netdb;
	const char		*map;
	const char		*key;
	int			*ypstatusp;
{
	nss_status_t		res;
	int			vallen;
	char			*val;
	char			*free_ptr;
	int			parsestat;

	if ((res = _nss_nis_ypmatch(be->domain, map, key, &val, &vallen,
				    ypstatusp)) != NSS_SUCCESS) {
		return (res);
	}

	free_ptr = val;

	if (netdb) {
		massage_netdb((const char **)&val, &vallen);
	}

	parsestat = (*args->str2ent)(val, vallen,
			args->buf.result, args->buf.buffer, args->buf.buflen);
	if (parsestat == NSS_STR_PARSE_SUCCESS) {
		args->returnval = args->buf.result;
		res = NSS_SUCCESS;
	} else if (parsestat == NSS_STR_PARSE_ERANGE) {
		args->erange = 1;
		/* We won't find this otherwise, anyway */
		res = NSS_NOTFOUND;
	} /* else if (parsestat == NSS_STR_PARSE_PARSE) won't happen ! */

	free(free_ptr);

	return (res);
}

nss_status_t
_nss_nis_lookup_rsvdport(be, args, netdb, map, key, ypstatusp)
	nis_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	int			netdb;
	const char		*map;
	const char		*key;
	int			*ypstatusp;
{
	nss_status_t		res;
	int			vallen;
	char			*val;
	char			*free_ptr;
	int			parsestat;

	if ((res = _nss_nis_ypmatch_rsvdport(be->domain, map, key, &val,
				    &vallen, ypstatusp)) != NSS_SUCCESS) {
		return (res);
	}

	free_ptr = val;

	if (netdb) {
		massage_netdb((const char **)&val, &vallen);
	}

	parsestat = (*args->str2ent)(val, vallen,
			args->buf.result, args->buf.buffer, args->buf.buflen);
	if (parsestat == NSS_STR_PARSE_SUCCESS) {
		args->returnval = args->buf.result;
		res = NSS_SUCCESS;
	} else if (parsestat == NSS_STR_PARSE_ERANGE) {
		args->erange = 1;
		/* We won't find this otherwise, anyway */
		res = NSS_NOTFOUND;
	} /* else if (parsestat == NSS_STR_PARSE_PARSE) won't happen ! */

	free(free_ptr);

	return (res);
}

static nss_status_t
do_getent(be, args, netdb)
	nis_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	int			netdb;
{
	nss_status_t		res;
	int			ypstatus;
	int			outkeylen, outvallen;
	char			*outkey, *outval;
	char			*free_ptr;
	int			parsestat;

#if	MT_UNSAFE_YP
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);
#endif
	if (be->enum_key == 0) {
		ypstatus = yp_first((grrr)be->domain, (grrr)be->enum_map,
				    &outkey, &outkeylen,
				    &outval, &outvallen);
	} else {
		ypstatus = yp_next ((grrr)be->domain, (grrr)be->enum_map,
				    be->enum_key, be->enum_keylen,
				    &outkey, &outkeylen,
				    &outval, &outvallen);
	}
#if	MT_UNSAFE_YP
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
#endif

	if ((res = switch_err(ypstatus, 0)) != NSS_SUCCESS) {
		return (res);
	}

	free_ptr = outval;

	if (netdb) {
		massage_netdb((const char **)&outval, &outvallen);
	}

	parsestat = (*args->str2ent)(outval, outvallen,
			args->buf.result, args->buf.buffer, args->buf.buflen);
	if (parsestat == NSS_STR_PARSE_SUCCESS) {
		args->returnval = args->buf.result;
		res = NSS_SUCCESS;
	} else if (parsestat == NSS_STR_PARSE_ERANGE) {
		args->erange = 1;
		/* We won't find this otherwise, anyway */
		res = NSS_NOTFOUND;
	} /* else if (parsestat == NSS_STR_PARSE_PARSE) won't happen ! */

	free(free_ptr);

	if (be->enum_key != 0) {
		free(be->enum_key);
	}
	be->enum_key = outkey;
	be->enum_keylen = outkeylen;

	return (res);
}

nss_status_t
_nss_nis_getent_rigid(be, args)
	nis_backend_ptr_t	be;
	void			*args;
{
	return (do_getent(be, (nss_XbyY_args_t *) args, 0));
}

nss_status_t
_nss_nis_getent_netdb(be, args)
	nis_backend_ptr_t	be;
	void			*args;
{
	return (do_getent(be, (nss_XbyY_args_t *) args, 1));
}


struct cb_data {
	void			*args;
	const char		*filter;
	nis_do_all_func_t	func;
	nss_status_t		result;
};

enum { ITER_NEXT = 0, ITER_STOP = 1 };	/* Should be in <rpcsvc/ypclnt.h> */

/*ARGSUSED*/
static int
do_cback(instatus, inkey, inkeylen, inval, invallen, indata)
	int			instatus;
	const char		*inkey;
	int			inkeylen;
	const char		*inval;
	int			invallen;
	struct cb_data		*indata;
{
	nss_status_t		res;

	if (instatus != YP_TRUE) {
		return (ITER_NEXT);	/* yp_all may decide otherwise... */
	}

	if (indata->filter != 0 && strstr(inval, indata->filter) == 0) {
		/*
		 * Optimization:  if the entry doesn't contain the filter
		 *   string then it can't be the entry we want, so don't
		 *   bother looking more closely at it.
		 */
		return (ITER_NEXT);
	}

	res = (*indata->func)(inval, invallen, indata->args);

	if (res == NSS_NOTFOUND) {
		return (ITER_NEXT);
	} else {
		indata->result = res;
		return (ITER_STOP);
	}
}

nss_status_t
_nss_nis_do_all(be, args, filter, func)
	nis_backend_ptr_t	be;
	void			*args;
	const char		*filter;
	nis_do_all_func_t	func;
{
	int			ypall_status;
	struct cb_data		data;
	struct ypall_callback	cback;

	data.args	= args;
	data.filter	= filter;
	data.func	= func;
	data.result	= NSS_NOTFOUND;

	cback.foreach	= do_cback;
	cback.data	= (char *) &data;

#if	MT_UNSAFE_YP
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	_thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	_mutex_lock(&one_lane);
#endif
	ypall_status = yp_all((grrr)be->domain, (grrr) be->enum_map, &cback);
#if	MT_UNSAFE_YP
	_mutex_unlock(&one_lane);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);
#endif

	switch (ypall_status) {
	    case 0:
		return (data.result);
	    case YPERR_BUSY:		/* Probably never get this, but... */
		return (NSS_TRYAGAIN);
	    default:
		return (NSS_UNAVAIL);
	}
}

struct XbyY_data {
	nss_XbyY_args_t		*args;
	nis_XY_check_func	func;
	int			netdb;
};

static nss_status_t
XbyY_iterator(instr, instr_len, a)
	const char		*instr;
	int			instr_len;
	void			*a;
{
	struct XbyY_data	*xydata	= (struct XbyY_data *) a;
	nss_XbyY_args_t		*args	= xydata->args;
	nss_status_t		res;
	int			parsestat;

	if (xydata->netdb) {
		massage_netdb(&instr, &instr_len);
	}

	parsestat = (*args->str2ent)(instr, instr_len,
			args->buf.result, args->buf.buffer, args->buf.buflen);
	if (parsestat == NSS_STR_PARSE_SUCCESS) {
		args->returnval = args->buf.result;
		if ((*xydata->func)(args)) {
			res = NSS_SUCCESS;
		} else {
			res = NSS_NOTFOUND;
			args->returnval = 0;
		}
	} else if (parsestat == NSS_STR_PARSE_ERANGE) {
		/*
		 * If we got here because (*str2ent)() found that the buffer
		 * wasn't big enough, maybe we should quit and return erange.
		 * Instead we'll keep looking and eventually return "not
		 * found" -- it's a bug, but not an earth-shattering one.
		 */
		args->erange = 1;	/* <== Is this a good idea? */
		res = NSS_NOTFOUND;
	} /* else if (parsestat == NSS_STR_PARSE_PARSE) won't happen ! */

	return (res);
}

nss_status_t
_nss_nis_XY_all(be, args, netdb, filter, func)
	nis_backend_ptr_t	be;
	nss_XbyY_args_t		*args;
	int			netdb;
	const char		*filter;
	nis_XY_check_func	func;
{
	struct XbyY_data	data;

	data.args = args;
	data.func = func;
	data.netdb = netdb;

	return (_nss_nis_do_all(be, &data, filter, XbyY_iterator));
	/* Now how many levels of callbacks was that? */
}


/*ARGSUSED*/
nss_status_t
_nss_nis_destr(be, dummy)
	nis_backend_ptr_t	be;
	void			*dummy;
{
	if (be != 0) {
		/* === Should change to invoke ops[ENDENT] ? */
		_nss_nis_endent(be, 0);
		free(be);
	}
	return (NSS_SUCCESS);	/* In case anyone is dumb enough to check */
}

/* We want to lock this even if the YP routines are MT-safe */
static mutex_t	yp_domain_lock = DEFAULTMUTEX;
static char	*yp_domain;

const char *
_nss_nis_domain()
{
	char			*domain;

	/*
	 * This much locking is probably more "by the book" than necessary...
	 */
	sigset_t		oldmask, newmask;

	sigfillset(&newmask);
	(void) _thr_sigsetmask(SIG_SETMASK, &newmask, &oldmask);
	(void) _mutex_lock(&yp_domain_lock);

	if ((domain = yp_domain) == 0) {
#if	MT_UNSAFE_YP
		_mutex_lock(&one_lane);
#endif
		if (yp_get_default_domain(&yp_domain) == 0) {
			domain = yp_domain;
		}
#if	MT_UNSAFE_YP
		_mutex_unlock(&one_lane);
#endif
	}

	_mutex_unlock(&yp_domain_lock);
	_thr_sigsetmask(SIG_SETMASK, &oldmask, NULL);

	return (domain);
}

nss_backend_t *
_nss_nis_constr(ops, n_ops, enum_map)
	nis_backend_op_t	ops[];
	int			n_ops;
	const char		*enum_map;
{
	const char		*domain;
	nis_backend_ptr_t	be;

	if ((domain = _nss_nis_domain()) == 0 ||
	    (be = (nis_backend_ptr_t) malloc(sizeof (*be))) == 0) {
		return (0);
	}
	be->ops		= ops;
	be->n_ops	= n_ops;
	be->domain	= domain;
	be->enum_map	= enum_map;   /* Don't strdup, assume valid forever */
	be->enum_key	= 0;
	be->enum_keylen	= 0;

	return ((nss_backend_t *) be);
}
