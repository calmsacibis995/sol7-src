// Copyright 07/19/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)ci_callback_server.cc	1.3 96/07/19 Sun Microsystems"


/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "ci_callback.h"
#include "ci_callbacks.hh"

CiGetAttributeOUT *
_cigetattribute_0x1_svc(CiGetAttributeIN *argp, struct svc_req *rqstp)
{
    static CiGetAttributeOUT  result;
	
	/*
	 * insert server code here
	 */
	CiGetAttribute(argp, &result); 
	return (&result);
}

CiGetNextAttributeOUT *
_cigetnextattribute_0x1_svc(CiGetNextAttributeIN *argp, struct svc_req *rqstp)
{
	static CiGetNextAttributeOUT  result;

	/*
	 * insert server code here
	 */

	CiGetNextAttribute(argp, &result); 
	return (&result);
}

DmiErrorStatus_t *
_cisetattribute_0x1_svc(CiSetAttributeIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */

	CiSetAttribute(argp, &result); 
	return (&result);
}

DmiErrorStatus_t *
_cireserveattribute_0x1_svc(CiReserveAttributeIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */

	CiReserveAttribute(argp, &result); 
	return (&result);
}

DmiErrorStatus_t *
_cireleaseattribute_0x1_svc(CiReleaseAttributeIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */

	CiReleaseAttribute(argp, &result); 
	return (&result);
}

DmiErrorStatus_t *
_ciaddrow_0x1_svc(CiAddRowIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */

	CiAddRow(argp, &result); 
	return (&result);
}

DmiErrorStatus_t *
_cideleterow_0x1_svc(CiDeleteRowIN *argp, struct svc_req *rqstp)
{
	static DmiErrorStatus_t  result;

	/*
	 * insert server code here
	 */

	CiDeleteRow(argp, &result); 
	return (&result);
}