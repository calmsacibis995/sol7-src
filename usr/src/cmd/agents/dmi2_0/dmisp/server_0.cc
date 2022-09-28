// Copyright 09/11/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)server_server.cc	1.14 96/09/11 Sun Microsystems"
/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include <stdlib.h>
#include "server.h"
#include <rw/rstream.h>
#include <string.h>
#include "dmi.hh"
#include "operation.hh"
#include "database.hh"
#include "listing.hh"
#include "initialization.hh"
#include "regci.hh"

//RWTPtrSlist<ComponentItem_t> componentItems;

extern DmiId_t ComponentID;



DmiRegisterOUT *
_dmiregister_0x1_svc(DmiRegisterIN *argp, struct svc_req *rqstp)
{
	static DmiRegisterOUT result;

	/*
	 * insert server code here
	 */
	
	dmiregister(argp, &result); 	
	return (&result);
}

DmiUnregisterOUT *
_dmiunregister_0x1_svc(DmiUnregisterIN *argp, struct svc_req *rqstp)
{
	static DmiUnregisterOUT  result;

	/*
	 * insert server code here
	 */

	dmiunregister(argp, &result); 
	return (&result);
}

DmiGetVersionOUT *
_dmigetversion_0x1_svc(DmiGetVersionIN *argp, struct svc_req *rqstp)
{
	static DmiGetVersionOUT  result;

	/*
	 * insert server code here
	 */
	dmigetversion(argp, &result); 
	return (&result);
}

DmiGetConfigOUT *
_dmigetconfig_0x1_svc(DmiGetConfigIN *argp, struct svc_req *rqstp)
{
	static DmiGetConfigOUT  result;

	/*
	 * insert server code here
	 */

	dmigetconfig(argp, &result); 
	return (&result);
}

DmiSetConfigOUT *
_dmisetconfig_0x1_svc(DmiSetConfigIN *argp, struct svc_req *rqstp)
{
	static DmiSetConfigOUT  result;

	/*
	 * insert server code here
	 */

	dmisetconfig(argp, &result); 
	return (&result);
}

DmiListComponentsOUT *
_dmilistcomponents_0x1_svc(DmiListComponentsIN *argp, struct svc_req *rqstp)
{
	static DmiListComponentsOUT  result;

	/*
	 * insert server code here
	 */

	dmilistcomponents(argp, &result); 
	return (&result);
}

DmiListComponentsByClassOUT *
_dmilistcomponentsbyclass_0x1_svc(DmiListComponentsByClassIN *argp, struct svc_req *rqstp)
{
	static DmiListComponentsByClassOUT  result;

	/*
	 * insert server code here
	 */
	dmilistcomponentsbyclass(argp, &result); 
	return (&result);
}

DmiListLanguagesOUT *
_dmilistlanguages_0x1_svc(DmiListLanguagesIN *argp, struct svc_req *rqstp)
{
	static DmiListLanguagesOUT  result;

	/*
	 * insert server code here
	 */
	dmilistlanguages(argp, &result); 
	return (&result);
}

DmiListClassNamesOUT *
_dmilistclassnames_0x1_svc(DmiListClassNamesIN *argp, struct svc_req *rqstp)
{
	static DmiListClassNamesOUT  result;

	/*
	 * insert server code here
	 */
	dmilistclassnames(argp, &result); 
	return (&result);
}

DmiListGroupsOUT *
_dmilistgroups_0x1_svc(DmiListGroupsIN *argp, struct svc_req *rqstp)
{
	static DmiListGroupsOUT  result;

	/*
	 * insert server code here
	 */

	dmilistgroups(argp, &result); 
	return (&result);
}

DmiListAttributesOUT *
_dmilistattributes_0x1_svc(DmiListAttributesIN *argp, struct svc_req *rqstp)
{
	static DmiListAttributesOUT  result;

	/*
	 * insert server code here
	 */
	dmilistattributes(argp, &result); 
	return (&result);
}

DmiAddRowOUT *
_dmiaddrow_0x1_svc(DmiAddRowIN *argp, struct svc_req *rqstp)
{
	static DmiAddRowOUT  result;

	/*
	 * insert server code here
	 */

	dmiaddrow(argp, &result); 
	return (&result);
}

DmiDeleteRowOUT *
_dmideleterow_0x1_svc(DmiDeleteRowIN *argp, struct svc_req *rqstp)
{
	static DmiDeleteRowOUT  result;

	/*
	 * insert server code here
	 */

	dmideleterow(argp, &result); 
	return (&result);
}

DmiGetMultipleOUT *
_dmigetmultiple_0x1_svc(DmiGetMultipleIN *argp, struct svc_req *rqstp)
{
	static DmiGetMultipleOUT  result;

	/*
	 * insert server code here
	 */

	dmigetmultiple(argp, &result); 
	return (&result);
}

DmiSetMultipleOUT *
_dmisetmultiple_0x1_svc(DmiSetMultipleIN *argp, struct svc_req *rqstp)
{
	static DmiSetMultipleOUT  result;

	/*
	 * insert server code here
	 */

	dmisetmultiple(argp, &result); 
	return (&result);
}

DmiAddComponentOUT *
_dmiaddcomponent_0x1_svc(DmiAddComponentIN *argp, struct svc_req *rqstp)
{
	static DmiAddComponentOUT  result;

	dmiaddcomponent(argp, &result);
	return (&result);
}

DmiAddLanguageOUT *
_dmiaddlanguage_0x1_svc(DmiAddLanguageIN *argp, struct svc_req *rqstp)
{
	static DmiAddLanguageOUT  result;

	/*
	 * insert server code here
	 */
	dmiaddlanguage(argp, &result); 
	return (&result);
}

DmiAddGroupOUT *
_dmiaddgroup_0x1_svc(DmiAddGroupIN *argp, struct svc_req *rqstp)
{
	static DmiAddGroupOUT  result;

	/*
	 * insert server code here
	 */

	dmiaddgroup(argp, &result); 
	return (&result);
}

DmiDeleteComponentOUT *
_dmideletecomponent_0x1_svc(DmiDeleteComponentIN *argp, struct svc_req *rqstp)
{
	static DmiDeleteComponentOUT  result;

	/*
	 * insert server code here
	 */

	dmideletecomponent(argp, &result); 
	return (&result);
}

DmiDeleteLanguageOUT *
_dmideletelanguage_0x1_svc(DmiDeleteLanguageIN *argp, struct svc_req *rqstp)
{
	static DmiDeleteLanguageOUT  result;

	/*
	 * insert server code here
	 */
	dmideletelanguage(argp, &result); 
	return (&result);
}

DmiDeleteGroupOUT *
_dmideletegroup_0x1_svc(DmiDeleteGroupIN *argp, struct svc_req *rqstp)
{
	static DmiDeleteGroupOUT  result;

	/*
	 * insert server code here
	 */

	dmideletegroup(argp, &result); 
	return (&result);
}

DmiGetAttributeOUT *
_dmigetattribute_0x1_svc(DmiGetAttributeIN *argp, struct svc_req *rqstp)
{
	static DmiGetAttributeOUT  result;

	/*
	 * insert server code here
	 */

	dmigetattribute(argp, &result); 
	return (&result);
}

DmiSetAttributeOUT *
_dmisetattribute_0x1_svc(DmiSetAttributeIN *argp, struct svc_req *rqstp)
{
	static DmiSetAttributeOUT  result;

	/*
	 * insert server code here
	 */

	dmisetattribute(argp, &result); 
	return (&result);
}

DmiRegisterCiOUT *
_dmiregisterci_0x1_svc(DmiRegisterCiIN *argp, struct svc_req *rqstp)
{
	static DmiRegisterCiOUT  result;

	dmiregisterci(argp, &result);
	return (&result);
}

DmiUnregisterCiOUT *
_dmiunregisterci_0x1_svc(DmiUnregisterCiIN *argp, struct svc_req *rqstp)
{
	static DmiUnregisterCiOUT  result;

	/*
	 * insert server code here
	 */
	
	dmiunregisterci(argp, &result); 
	return (&result);
}

DmiOriginateEventOUT *
_dmioriginateevent_0x1_svc(DmiOriginateEventIN *argp, struct svc_req *rqstp)
{
	static DmiOriginateEventOUT  result;

	/*
	 * insert server code here
	 */

	dmioriginateevent(argp, &result); 
	return (&result);
}