#ifndef _DMI_ERROR_HH
#define _DMI_ERROR_HH

/* Copyright 09/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)dmi_error.hh	1.19 96/09/23 Sun Microsystems"


#define DMIERR_NO_ERROR                          0
#define DMIERR_ILLEGAL_HANDLE                    1
#define DMIERR_OUT_OF_MEMORY                     2
#define DMIERR_INSUFFICIENT_PRIVILEGES           3
#define DMIERR_SP_INACTIVE                       4
#define DMIERR_ATTRIBUTE_NOT_FOUND               5
#define DMIERR_COMPONENT_NOT_FOUND               6
#define DMIERR_GROUP_NOT_FOUND                   7
#define DMIERR_DATABASE_CORRUPT                  8
#define DMIERR_ILLEGAL_DMI_LEVEL                 9
#define DMIERR_ILLEGAL_RPC_HANDLE                10
#define DMIERR_CANT_UNINSTALL_COMPONENT_LANGUAGE 11
#define DMIERR_CANT_UNINSTALL_SP_COMPONENT       13
#define DMIERR_CANT_UNINSTALL_GROUP              14
#define DMIERR_UNKNOWN_CI_REGISTRY               15
#define DMIERR_ILLEGAL_KEYS                      16
#define DMIERR_UNABLE_TO_ADD_ROW                 17
#define DMIERR_UNABLE_TO_DELETE_ROW              18
#define DMIERR_ILLEGAL_TO_GET                    19
#define DMIERR_ILLEGAL_TO_SET                    20
#define DMIERR_VALUE_UNKNOWN                     21
#define DMIERR_DIRECT_INTERFACE_NOT_REGISTERED   22
#define DMIERR_FILE_ERROR                        23
#define DMIERR_ROW_NOT_FOUND                     24
#define DMIERR_ROW_EXIST                         25
#define DMIERR_ILLEGAL_PARAMETER                 26
#define DMIERR_BAD_SCHEMA_DESCRIPTION_FILE       27


/* error code for db */
#define DMIERR_DB_FIRST_ERROR			50
#define DMIERR_PARSING_ERROR			(DMIERR_DB_FIRST_ERROR+1)
#define DMIERR_DB_NOT_INITIALIZE		(DMIERR_DB_FIRST_ERROR+2)
#define DMIERR_DB_DIR_NOT_EXIST			(DMIERR_DB_FIRST_ERROR+3)

#ifdef __cplusplus
extern "C" {
#endif 	

extern void dmi_error(DmiErrorStatus_t error_status); 

#ifdef __cplusplus
}
#endif 	

#endif
