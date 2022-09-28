// Copyright 09/23/96 Sun Microsystems, Inc. All Rights Reserved.
//
#pragma ident  "@(#)dmi_error.cc	1.16 96/09/23 Sun Microsystems"

#include <stream.h>
#include "common.h"
#include "dmi_error.hh"

void dmi_error(DmiErrorStatus_t error_status)
{
	switch (error_status) {
		case DMIERR_NO_ERROR:
			cout << "DMIERR_NO_ERROR" ; 
			break;
		case DMIERR_ILLEGAL_HANDLE:
			cout << "DMIERR_ILLEGAL_HANDLE" ; 
			break;
		case DMIERR_OUT_OF_MEMORY:
			cout << "DMIERR_OUT_OF_MEMORY" ; 
			break;
		case DMIERR_INSUFFICIENT_PRIVILEGES:
			cout << "DMIERR_INSUFFICIENT_PRIVILEGES" ; 
			break;
		case DMIERR_SP_INACTIVE: 
			cout << "DMIERR_SP_INACTIVE" ; 
			break;
		case DMIERR_ATTRIBUTE_NOT_FOUND: 
			cout << "DMIERR_ATTRIBUTE_NOT_FOUND" ; 
			break;
		case DMIERR_COMPONENT_NOT_FOUND:
			cout << "DMIERR_COMPONENT_NOT_FOUND"; 
			break;
		case DMIERR_GROUP_NOT_FOUND: 
			cout << "DMIERR_GROUP_NOT_FOUND" ; 
			break;
		case DMIERR_DATABASE_CORRUPT:
			cout << "DMIERR_DATABASE_CORRUPT" ; 
			break;
		case DMIERR_ILLEGAL_DMI_LEVEL: 
			cout << "DMIERR_ILLEGAL_DMI_LEVEL" ; 
			break;
		case DMIERR_ILLEGAL_RPC_HANDLE:
			cout << "DMIERR_ILLEGAL_RPC_HANDLE" ;
			break; 
		case DMIERR_CANT_UNINSTALL_COMPONENT_LANGUAGE:
			cout << "DMIERR_CANT_UNINSTALL_COMPONENT_LANGUAGE" ; 
			break;
		case DMIERR_CANT_UNINSTALL_SP_COMPONENT:
			cout << "DMIERR_CANT_UNINSTALL_SP_COMPONENT" ;
			break;
		case DMIERR_CANT_UNINSTALL_GROUP:
			cout << "DMIERR_CANT_UNINSTALL_GROUP" ; 
			break;
		case DMIERR_UNKNOWN_CI_REGISTRY:
			cout << "DMIERR_UNKNOWN_CI_REGISTRY" ;
			break;
		case DMIERR_ILLEGAL_KEYS:
			cout << "DMIERR_ILLEGAL_KEYS" ;
			break;
		case DMIERR_UNABLE_TO_ADD_ROW:
			cout << "DMIERR_UNABLE_TO_ADD_ROW";
			break;
		case DMIERR_UNABLE_TO_DELETE_ROW:
			cout << "DMIERR_UNABLE_TO_DELETE_ROW";
			break;
		case DMIERR_ILLEGAL_TO_GET:
			cout << "DMIERR_ILLEGAL_TO_GET";
			break;
		case DMIERR_ILLEGAL_TO_SET:
			cout << "DMIERR_ILLEGAL_TO_SET";
			break;
		case DMIERR_VALUE_UNKNOWN:
			cout << "DMIERR_VALUE_UNKNOWN" ;
			break; 
		case DMIERR_DIRECT_INTERFACE_NOT_REGISTERED:
			cout << "DMIERR_DIRECT_INTERFACE_NOT_REGISTERED" ;
			break;
		case DMIERR_FILE_ERROR: 
			cout << "DMIERR_FILE_ERROR" ;
			break;
		case DMIERR_ROW_NOT_FOUND:
			cout << "DMIERR_ROW_NOT_FOUND" ;
			break;
		case DMIERR_ROW_EXIST:
			cout << "DMIERR_ROW_EXIST" ;
			break;
		case DMIERR_ILLEGAL_PARAMETER:
			cout << "DMIERR_ILLEGAL_PARAMETER" ;
			break;
		case DMIERR_PARSING_ERROR:
			cout << "DMIERR_PARSING_ERROR" ;
			break;
		case DMIERR_DB_NOT_INITIALIZE:
			cout << "DMIERR_DB_NOT_INITIALIZE" ;
			break;
		case DMIERR_DB_DIR_NOT_EXIST: 
			cout << "DMIERR_DB_DIR_NOT_EXIST" ;
			break;
		case DMIERR_BAD_SCHEMA_DESCRIPTION_FILE:
			cout << "DMIERR_BAD_SCHEMA_DESCRIPTION_FILE";
			break;
		default:
			cout << "unknown DMI errors"; 
			break;
	}
	cout << endl; 
	fflush(0); 
}

