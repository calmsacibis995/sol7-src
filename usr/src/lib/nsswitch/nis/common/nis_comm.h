/*
 * Copyright (c) 1992, by Sun Microsystems, Inc.
 *
 * Common code and structures used by name-service-switch "nis" backends.
 */

#ifndef _NIS_COMMON_H
#define	_NIS_COMMON_H

#pragma	ident	"@(#)nis_common.h 1.7 97/08/27 SMI@(#)"

#include <nss_dbdefs.h>
#include <signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct nis_backend *nis_backend_ptr_t;
typedef nss_status_t	(*nis_backend_op_t)(nis_backend_ptr_t, void *);

/*
 * Iterator function for _nss_nis_do_all(), which probably calls yp_all().
 *   NSS_NOTFOUND means "keep enumerating", NSS_SUCCESS means"return now",
 *   other values don't make much sense.  In other words we're abusing
 *   (overloading) the meaning of nss_status_t, but hey...
 * _nss_nis_XY_all() is a wrapper around _nss_nis_do_all() that does the
 *   generic work for nss_XbyY_args_t backends (calls cstr2ent etc).
 */
typedef nss_status_t	(*nis_do_all_func_t)(const char *, int, void *priv);
typedef int		(*nis_XY_check_func)(nss_XbyY_args_t *);

extern nss_backend_t	*_nss_nis_constr(nis_backend_op_t	*ops,
					int			n_ops,
					const char		*map);
extern nss_status_t	_nss_nis_destr (nis_backend_ptr_t, void *dummy);
extern nss_status_t	_nss_nis_setent(nis_backend_ptr_t, void *dummy);
extern nss_status_t  	_nss_nis_endent(nis_backend_ptr_t, void *dummy);
extern nss_status_t  	_nss_nis_getent_rigid(nis_backend_ptr_t, void *);
extern nss_status_t  	_nss_nis_getent_netdb(nis_backend_ptr_t, void *);
extern nss_status_t 	_nss_nis_do_all(nis_backend_ptr_t,
					void			*func_priv,
					const char		*filter,
					nis_do_all_func_t	func);
extern nss_status_t 	_nss_nis_XY_all(nis_backend_ptr_t,
					nss_XbyY_args_t		*check_args,
					int			netdb,
					const char		*filter,
					nis_XY_check_func	check);
extern nss_status_t	_nss_nis_lookup(nis_backend_ptr_t,
					nss_XbyY_args_t		*args,
					int			netdb,
					const char		*map,
					const char		*key,
					int			*yp_statusp);
extern nss_status_t _nss_nis_lookup_rsvdport(nis_backend_ptr_t   be,
					nss_XbyY_args_t     *args,
					int netdb,
					const char      *map,
					const char      *key,
					int         *ypstatusp);

/* Lower-level interface */
extern nss_status_t	_nss_nis_ypmatch(const char		*domain,
					const char		*map,
					const char		*key,
					char			**valp,
					int			*vallenp,
					int			*yp_statusp);
extern const char	*_nss_nis_domain();
extern int __nss2herrno(nss_status_t nsstat);
extern int _thr_sigsetmask(int how, const sigset_t *set, sigset_t *oset);
extern int _mutex_lock(mutex_t *mp);
extern int _mutex_unlock(mutex_t *mp);

#endif /* _NIS_COMMON_H */
