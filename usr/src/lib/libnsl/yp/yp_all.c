#ident	"@(#)yp_all.c	1.12	97/08/01 SMI"

/*
 * Copyright (c) 1986-1995 by Sun Microsystems Inc.
 */

#define	NULL 0
#include <rpc/rpc.h>
#include <syslog.h>
#include "yp_b.h"
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <rpc/trace.h>
#include <netdir.h>
#include <string.h>
#include <stdlib.h>

static struct timeval tp_timout = { 120, 0};
static char nullstring[] = "\000";

/*
 * This does the "glommed enumeration" stuff.  callback->foreach is the name
 * of a function which gets called per decoded key-value pair:
 *
 * (*callback->foreach)(status, key, keylen, val, vallen, callback->data);
 *
 * If the server we get back from __yp_dobind speaks the old protocol, this
 * returns YPERR_VERS, and does not attempt to emulate the new functionality
 * by using the old protocol.
 */
int
yp_all(domain, map, callback)
	char *domain;
	char *map;
	struct ypall_callback *callback;
{
	size_t domlen;
	size_t maplen;
	struct ypreq_nokey req;
	int reason;
	struct dom_binding *pdomb;
	enum clnt_stat s;
	CLIENT *allc;
	char server_name[MAXHOSTNAMELEN];


	trace1(TR_yp_all, 0);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_all, 1);
		return (YPERR_BADARGS);
	}

	domlen = strlen(domain);
	maplen = strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP) ||
	    (callback == (struct ypall_callback *) NULL)) {
		trace1(TR_yp_all, 1);
		return (YPERR_BADARGS);
	}

	if (reason = __yp_dobind(domain, &pdomb)) {
		trace1(TR_yp_all, 1);
		return (reason);
	}

	if (pdomb->dom_binding->ypbind_hi_vers < YPVERS) {
		__yp_rel_binding(pdomb);
		trace1(TR_yp_all, 1);
		return (YPERR_VERS);
	}
	mutex_lock(&pdomb->server_name_lock);
	if (!pdomb->dom_binding->ypbind_servername) {
		mutex_unlock(&pdomb->server_name_lock);
		__yp_rel_binding(pdomb);
		syslog(LOG_ERR, "yp_all: failed to get server's name\n");
		trace1(TR_yp_all, 1);
		return (YPERR_RPC);
	}
	(void) strcpy(server_name, pdomb->dom_binding->ypbind_servername);
	mutex_unlock(&pdomb->server_name_lock);
	if (strcmp(server_name, nullstring) == 0) {
		/*
		 * This is the case where ypbind is running in broadcast mode,
		 * we have to do the jugglery to get the
		 * ypserv's address on COTS transport based
		 * on the CLTS address ypbind gave us !
		 */

		struct nd_hostservlist *nhs;

		if (netdir_getbyaddr(pdomb->dom_binding->ypbind_nconf,
			&nhs, pdomb->dom_binding->ypbind_svcaddr) != ND_OK) {
			syslog(LOG_ERR,
				"yp_all: failed to get server's name\n");
			__yp_rel_binding(pdomb);
			trace1(TR_yp_all, 1);
			return (YPERR_RPC);
		}
		/* check server name again, some other thread may have set it */
		mutex_lock(&pdomb->server_name_lock);
		if (strcmp(pdomb->dom_binding->ypbind_servername,
					nullstring) == 0) {
			pdomb->dom_binding->ypbind_servername =
				(char *) strdup(nhs->h_hostservs->h_host);
		}
		(void) strcpy(server_name,
		    pdomb->dom_binding->ypbind_servername);
		mutex_unlock(&pdomb->server_name_lock);
		netdir_free((char *)nhs, ND_HOSTSERVLIST);
	}
	__yp_rel_binding(pdomb);
	if ((allc = clnt_create(server_name, YPPROG,
		YPVERS, "circuit_n")) == (CLIENT *) NULL) {
		    syslog(LOG_ERR,
		    clnt_spcreateerror("yp_all \
- transport level create failure"));
			trace1(TR_yp_all, 1);
		    return (YPERR_RPC);
	}

	req.domain = domain;
	req.map = map;


	s = clnt_call(allc, YPPROC_ALL,
		(xdrproc_t)xdr_ypreq_nokey, (char *)&req,
	    (xdrproc_t)xdr_ypall, (char *)callback, tp_timout);

	if (s != RPC_SUCCESS) {
		syslog(LOG_ERR, clnt_sperror(allc,
		    "yp_all - RPC clnt_call (transport level) failure"));
	}

	clnt_destroy(allc);
	if (s == RPC_SUCCESS) {
		trace1(TR_yp_all, 1);
		return (0);
	} else {
		trace1(TR_yp_all, 1);
		return (YPERR_RPC);
	}
}

/*
 * This function is identical to 'yp_all' with the exception that it
 * attempts to use reserve ports.
 */
int
__yp_all_rsvdport(domain, map, callback)
	char *domain;
	char *map;
	struct ypall_callback *callback;
{
	size_t domlen;
	size_t maplen;
	struct ypreq_nokey req;
	int reason;
	struct dom_binding *pdomb;
	enum clnt_stat s;
	CLIENT *allc;
	char server_name[MAXHOSTNAMELEN];

	trace1(TR_yp_all, 0);
	if ((map == NULL) || (domain == NULL)) {
		trace1(TR_yp_all, 1);
		return (YPERR_BADARGS);
	}

	domlen =  strlen(domain);
	maplen =  strlen(map);

	if ((domlen == 0) || (domlen > YPMAXDOMAIN) ||
	    (maplen == 0) || (maplen > YPMAXMAP) ||
	    (callback == (struct ypall_callback *) NULL)) {
		trace1(TR_yp_all, 1);
		return (YPERR_BADARGS);
	}

	if (reason = __yp_dobind_rsvdport(domain, &pdomb)) {
		trace1(TR_yp_all, 1);
		return (reason);
	}

	if (pdomb->dom_binding->ypbind_hi_vers < YPVERS) {
		/*
		 * Have to free the binding since the reserved
		 * port bindings are not cached.
		 */
		__yp_rel_binding(pdomb);
		free_dom_binding(pdomb);
		trace1(TR_yp_all, 1);
		return (YPERR_VERS);
	}
	mutex_lock(&pdomb->server_name_lock);
	if (!pdomb->dom_binding->ypbind_servername) {
		mutex_unlock(&pdomb->server_name_lock);
		syslog(LOG_ERR, "yp_all: failed to get server's name\n");
		__yp_rel_binding(pdomb);
		free_dom_binding(pdomb);
		trace1(TR_yp_all, 1);
		return (YPERR_RPC);
	}
	(void) strcpy(server_name, pdomb->dom_binding->ypbind_servername);
	mutex_unlock(&pdomb->server_name_lock);
	if (strcmp(server_name, nullstring) == 0) {
		/*
		 * This is the case where ypbind is running in broadcast mode,
		 * we have to do the jugglery to get the
		 * ypserv's address on COTS transport based
		 * on the CLTS address ypbind gave us !
		 */

		struct nd_hostservlist *nhs;

		if (netdir_getbyaddr(pdomb->dom_binding->ypbind_nconf,
			&nhs, pdomb->dom_binding->ypbind_svcaddr) != ND_OK) {
			syslog(LOG_ERR,
				"yp_all: failed to get server's name\n");
			__yp_rel_binding(pdomb);
			free_dom_binding(pdomb);
			trace1(TR_yp_all, 1);
			return (YPERR_RPC);
		}
		/* check server name again, some other thread may have set it */
		mutex_lock(&pdomb->server_name_lock);
		if (strcmp(pdomb->dom_binding->ypbind_servername,
					nullstring) == 0) {
			pdomb->dom_binding->ypbind_servername =
			(char *) strdup(nhs->h_hostservs->h_host);
		}
		(void) strcpy(server_name,
		    pdomb->dom_binding->ypbind_servername);
		mutex_unlock(&pdomb->server_name_lock);
		netdir_free((char *)nhs, ND_HOSTSERVLIST);

	}
	__yp_rel_binding(pdomb);
	if ((allc = __yp_clnt_create_rsvdport(server_name, YPPROG, YPVERS,
	    "tcp", 0, 0)) == (CLIENT *) NULL) {
		syslog(LOG_ERR, clnt_spcreateerror("yp_all - transport "
		    "level create failure"));
		free_dom_binding(pdomb);
		trace1(TR_yp_all, 1);
		return (YPERR_RPC);
	}

	req.domain = domain;
	req.map = map;

	s = clnt_call(allc, YPPROC_ALL,
		(xdrproc_t)xdr_ypreq_nokey, (char *)&req,
	    (xdrproc_t)xdr_ypall, (char *)callback, tp_timout);

	if (s != RPC_SUCCESS) {
		syslog(LOG_ERR, clnt_sperror(allc,
		    "yp_all - RPC clnt_call (transport level) failure"));
	}

	clnt_destroy(allc);
	if (s == RPC_SUCCESS) {
		trace1(TR_yp_all, 1);
		free_dom_binding(pdomb);
		return (0);
	} else {
		trace1(TR_yp_all, 1);
		free_dom_binding(pdomb);
		return (YPERR_RPC);
	}
}
