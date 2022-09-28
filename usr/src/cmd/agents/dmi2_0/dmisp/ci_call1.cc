// Copyright 10/07/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)ci_callback_svc.cc	1.3 96/10/07 Sun Microsystems"

#include "ci_callback.h"
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */
#include <signal.h>
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <unistd.h> /* setsid */
#include <sys/types.h>
#include <memory.h>
#include <stropts.h>
#include <netconfig.h>
#include <sys/resource.h> /* rlimit */
#include <syslog.h>
#include <rw/rstream.h>
#include <syslog.h>
#define PORTMAP
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/svc_soc.h>
#include "ci_callback_svc.hh"

#define	_IDLE 0
#define	_SERVED 1

static int _rpcsvcstate = _IDLE;	/* Set when a request is serviced */
static int _rpcsvccount = 0;		/* Number of requests being serviced */

static
void _msgout(char* msg)
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, msg);
	else
		(void) fprintf(stderr, "%s\n", msg);
#else
	syslog(LOG_ERR, msg);
#endif
}

static void
dmi2_ci_callback_0x1(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		CiGetAttributeIN _cigetattribute_0x1_arg;
		CiGetNextAttributeIN _cigetnextattribute_0x1_arg;
		CiSetAttributeIN _cisetattribute_0x1_arg;
		CiReserveAttributeIN _cireserveattribute_0x1_arg;
		CiReleaseAttributeIN _cireleaseattribute_0x1_arg;
		CiAddRowIN _ciaddrow_0x1_arg;
		CiDeleteRowIN _cideleterow_0x1_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *);

	_rpcsvccount++;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp,
			(xdrproc_t) xdr_void, (char *)NULL);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;

	case _CiGetAttribute:
		xdr_argument = (xdrproc_t) xdr_CiGetAttributeIN;
		xdr_result = (xdrproc_t) xdr_CiGetAttributeOUT;
		local = (char *(*)(char *, struct svc_req *)) _cigetattribute_0x1_svc;
		break;

	case _CiGetNextAttribute:
		xdr_argument = (xdrproc_t) xdr_CiGetNextAttributeIN;
		xdr_result = (xdrproc_t) xdr_CiGetNextAttributeOUT;
		local = (char *(*)(char *, struct svc_req *)) _cigetnextattribute_0x1_svc;
		break;

	case _CiSetAttribute:
		xdr_argument = (xdrproc_t) xdr_CiSetAttributeIN;
		xdr_result = (xdrproc_t) xdr_DmiErrorStatus_t;
		local = (char *(*)(char *, struct svc_req *)) _cisetattribute_0x1_svc;
		break;

	case _CiReserveAttribute:
		xdr_argument = (xdrproc_t) xdr_CiReserveAttributeIN;
		xdr_result = (xdrproc_t) xdr_DmiErrorStatus_t;
		local = (char *(*)(char *, struct svc_req *)) _cireserveattribute_0x1_svc;
		break;

	case _CiReleaseAttribute:
		xdr_argument = (xdrproc_t) xdr_CiReleaseAttributeIN;
		xdr_result = (xdrproc_t) xdr_DmiErrorStatus_t;
		local = (char *(*)(char *, struct svc_req *)) _cireleaseattribute_0x1_svc;
		break;

	case _CiAddRow:
		xdr_argument = (xdrproc_t) xdr_CiAddRowIN;
		xdr_result = (xdrproc_t) xdr_DmiErrorStatus_t;
		local = (char *(*)(char *, struct svc_req *)) _ciaddrow_0x1_svc;
		break;

	case _CiDeleteRow:
		xdr_argument = (xdrproc_t) xdr_CiDeleteRowIN;
		xdr_result = (xdrproc_t) xdr_DmiErrorStatus_t;
		local = (char *(*)(char *, struct svc_req *)) _cideleterow_0x1_svc;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t) &argument)) {
		svcerr_decode(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		_msgout("unable to free arguments");
		exit(1);
	}
	_rpcsvccount--;
	_rpcsvcstate = _SERVED;
	return;
}

extern void CiInitialization(); 
u_long reg_ci_callback() 
{
	SVCXPRT *xprt;
	int sock = RPC_ANYSOCK;
	static u_long notify_rpc_number; 

	/* Create port and link to dispatcher */
	xprt = svcudp_create(sock);
	
	/* Assign transient number */
	notify_rpc_number = 0x40000000;		/* First transient number */
	while (notify_rpc_number++) 
		if (pmap_set(notify_rpc_number, DMI2_CI_CALLBACK_VERSION,
			IPPROTO_UDP, xprt->xp_port))
				break;
	svc_register(xprt, notify_rpc_number, DMI2_CI_CALLBACK_VERSION,
		dmi2_ci_callback_0x1, 0);

	CiInitialization(); 
	return (notify_rpc_number); 
}


void *start_svc_run_thread(void *arg)
{
	cout << "Start svc run thread and wait for rpc request coming " << endl;
	svc_run();
	return (0); 
}


