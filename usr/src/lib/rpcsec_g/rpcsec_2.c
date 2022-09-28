/*
 * Copyright (c) 1986-1995,1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)rpcsec_gss_utils.c	1.28	97/12/12 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_ext.h>
#include <rpc/rpc.h>
#include <rpc/rpcsec_defs.h>
#include <rpc/trace.h>

#define	SVC_INTEGRITY	"integrity"
#define	SVC_PRIVACY	"privacy"
#define	SVC_NONE	"none"
#define	SVC_DEFAULT	"default"

#define	MCALL_MSG_SIZE 24
/*
 * Private data kept per client handle
 */
struct cu_data {
	int			cu_fd;		/* connections fd */
	bool_t			cu_closeit;	/* opened by library */
	struct netbuf		cu_raddr;	/* remote address */
	struct timeval		cu_wait;	/* retransmit interval */
	struct timeval		cu_total;	/* total time for the call */
	struct rpc_err		cu_error;
	struct t_unitdata	*cu_tr_data;
	XDR			cu_outxdrs;
	char			*cu_outbuf_start;
	char			cu_outbuf[MCALL_MSG_SIZE];
	u_int			cu_xdrpos;
	u_int			cu_sendsz;	/* send size */
	u_int			cu_recvsz;	/* recv size */
	struct pollfd		pfdp;
	char			cu_inbuf[1];
};

/*
 * Internal utility routines.
 */
bool_t
__rpc_gss_mech_to_oid(mech, oid)
	char	*mech;
	rpc_gss_OID	*oid;
{

	if (__gss_mech_to_oid(mech, (gss_OID*)oid) != GSS_S_COMPLETE)
		return (FALSE);

	return (TRUE);
}       

char *
__rpc_gss_oid_to_mech(oid)
	rpc_gss_OID	oid;
{

	return ((char *)__gss_oid_to_mech((const gss_OID)oid));
}


bool_t
__rpc_gss_qop_to_num(qop, mech, num)
	char		*qop;
	char		*mech;
	OM_uint32	*num;
{

	if (__gss_qop_to_num(qop, mech, num) != GSS_S_COMPLETE)
		return (FALSE);
	return (TRUE);
}

char *
__rpc_gss_num_to_qop(mech, num)
	char		*mech;
	OM_uint32	num;
{
	char *qop;

	if (__gss_num_to_qop(mech, num, &qop) != GSS_S_COMPLETE)
		return (NULL);
	return (qop);
}

bool_t
__rpc_gss_svc_to_num(svc, num)
	char			*svc;
	rpc_gss_service_t	*num;
{
	if (strcasecmp(svc, SVC_INTEGRITY) == 0)
		*num = rpc_gss_svc_integrity;
	else if (strcasecmp(svc, SVC_PRIVACY) == 0)
		*num = rpc_gss_svc_privacy;
	else if (strcasecmp(svc, SVC_NONE) == 0)
		*num = rpc_gss_svc_none;
	else if (strcasecmp(svc, SVC_DEFAULT) == 0)
		*num = rpc_gss_svc_default;
	else
		return (FALSE);
	return (TRUE);
}

char *
__rpc_gss_num_to_svc(num)
	rpc_gss_service_t	num;
{
	switch (num) {
	case rpc_gss_svc_integrity:
		return (strdup(SVC_INTEGRITY));
	case rpc_gss_svc_privacy:
		return (strdup(SVC_PRIVACY));
	case rpc_gss_svc_none:
		return (strdup(SVC_NONE));
	case rpc_gss_svc_default:
		return (strdup(SVC_DEFAULT));
	default:
		return (NULL);
	}
}

/*
 * Given the user name, node, and security domain, get the mechanism
 * specific principal name (for the user name) in exported form.
 */
bool_t
__rpc_gss_get_principal_name(principal, mech, user, node, secdomain)
	rpc_gss_principal_t	*principal;
	char			*mech;
	char			*user;
	char			*node;
	char			*secdomain;
{
	gss_name_t		gss_name, gss_canon_name;
	gss_buffer_desc		name_buf = GSS_C_EMPTY_BUFFER;
	char			user_name[256], *s;
	gss_OID			mech_oid;
	int			nlen = 0, slen = 0, plen;
	OM_uint32		major, minor;

	*principal = NULL;
	if (user == NULL || strlen(user) == 0)
		return (FALSE);

	if (!__rpc_gss_mech_to_oid(mech, (rpc_gss_OID *) &mech_oid)) {
		syslog(LOG_ERR, "rpc_gss_get_principal_name: can't get"
			"mech oid");
		return (FALSE);
	}

	if (secdomain != NULL)
		slen = strlen(secdomain);

	if (node != NULL)
		nlen = strlen(node);

	strcpy(user_name, user);
	if (nlen > 0) {
		strcat(user_name, "/");
		strcat(user_name, node);
	}

	if (slen > 0) {
		strcat(user_name, "@");
		strcat(user_name, secdomain);
	}

	name_buf.value = user_name;
	name_buf.length = strlen(user_name);

	/*
	 *  Convert a text string to a GSSAPI Internal name.
	 */
	if ((major = gss_import_name(&minor, &name_buf,
		(gss_OID) GSS_C_NT_USER_NAME, &gss_name)) != GSS_S_COMPLETE) {
		syslog(LOG_ERR, "rpc_gss_get_principal_name: import name"
			"failed 0x%x", major);
		return (FALSE);
	}

	/*
	 *  Convert the GSSAPI Internal name to a MN - Mechanism Name
	 */
	if ((major = gss_canonicalize_name(&minor, gss_name, mech_oid,
		&gss_canon_name)) != GSS_S_COMPLETE) {
		syslog(LOG_ERR, "rpc_gss_get_principal_name: canonicalize name"
			"failed 0x%x", major);
		gss_release_name(&minor, &gss_name);
		return (FALSE);
	}
	gss_release_name(&minor, &gss_name);

	/*
	 *  Convert the MN Internal name to an exported flat name, so
	 *  it is suitable for binary comparison.
	 */
	if ((major = gss_export_name(&minor, gss_canon_name, &name_buf)) !=
		GSS_S_COMPLETE) {
		syslog(LOG_ERR, "rpc_gss_get_principal_name: export name"
			"failed %x", major);
		gss_release_name(&minor, &gss_canon_name);
		return (FALSE);
	}
	gss_release_name(&minor, &gss_canon_name);

	/*
	 *  Put the exported name into rpc_gss_principal_t structure.
	 */
	plen = RNDUP(name_buf.length) + sizeof (int);
	(*principal) = (rpc_gss_principal_t) malloc(plen);
	if ((*principal) == NULL) {
		gss_release_buffer(&minor, &name_buf);
		return (FALSE);
	}
	bzero((caddr_t) (*principal), plen);
	(*principal)->len = RNDUP(name_buf.length);
	s = (*principal)->name;
	memcpy(s, name_buf.value, name_buf.length);
	gss_release_buffer(&minor, &name_buf);

	return (TRUE);
}

/*
 * Return supported mechanisms.
 */
char **
__rpc_gss_get_mechanisms()
{
	static char	*mech_list[MAX_MECH_OID_PAIRS+1];

	*mech_list = NULL;
	__gss_get_mechanisms(mech_list, MAX_MECH_OID_PAIRS+1);
	return (mech_list);
}

/*
 * For a given mechanism, return information about it.
 */
/*
static char			*krb5_qop_list[] = {Q_DEFAULT, NULL};
*/

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/* Don't know how to get the service type for a given mech.	*/
/* "service" should NOT be there!				*/
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*!!!!!!!!!!! */

char **
__rpc_gss_get_mech_info(mech, service)
	char			*mech;
	rpc_gss_service_t	*service;
{
	char **l;

	l = calloc(MAX_QOPS_PER_MECH + 1, sizeof (char *));
	if (l == NULL)
		return (NULL);

	if (__gss_get_mech_info(mech, l) != GSS_S_COMPLETE) {
		free(l);
		return (NULL);
	}
					/* !!!!!!!!!!!!!!!! */
	*service = rpc_gss_svc_privacy; /* What service type? */
					/* !!!!!!!!!!!!!!!! */
	return (l);
}

/*
 * Returns highest and lowest versions of RPCSEC_GSS flavor supported.
 */
bool_t
__rpc_gss_get_versions(vers_hi, vers_lo)
	u_int	*vers_hi;
	u_int	*vers_lo;
{
	*vers_hi = RPCSEC_GSS_VERSION;
	*vers_lo = RPCSEC_GSS_VERSION;
	return (TRUE);
}

/*
 * Check if a mechanism is installed.
 */
bool_t
__rpc_gss_is_installed(mech)
	char	*mech;
{
	char **l;

	if (mech == NULL)
		return (FALSE);

	if ((l = __rpc_gss_get_mechanisms()) == NULL)
		return (FALSE);

	while (*l != NULL) {
		if (strcmp(*l, mech) == 0)
			return (TRUE);
		l++;
	}
	return (FALSE);
}
