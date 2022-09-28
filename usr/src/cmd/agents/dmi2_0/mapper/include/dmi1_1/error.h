/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)error.h	1.1 96/08/05 Sun Microsystems"

/*****************************************************************************************************************************************************

    Filename: error.h


    Description: Service Layer error codes

    Author(s): Alvin I.	PIvowar

    RCS	Revision: $Header: n:/sl/rcs/error.h 1.32 1994/05/04 13:43:06 shanraha Exp $

    Revision History:

	Date		Author	Description
	-------		---		-----------------------------------------------
	4/02/93		aip    Creation	date.
		12/09/93	sfh		Add DBERR_DIRECT_INTERFACE_NOT_REGISTERED.
							Add DBERR_DATABASE_CORRUPT.
		01/05/94	sfh		Remove DBERR_ILLEGAL_COMMAND, DBERR_ILLEGAL_KEY_TYPE, DBERR_ILLEGAL_REQUEST_COUNT.
		03/11/94	sfh		Update to 4.3 spec:
								Add DBERR_ATTRIBUTE_NOT_SUPPORTED.
							Add SLERR_SL_NOT_ACTIVE.
							Add SLERR_NULL_ACCESS_FUNCTION.
		04/06/94	sfh		Add SLERR_FILE_ERROR.
							Add SLERR_BAD_MIF_FILE.
							Add SLERR_EXEC_FAILURE.
		04/12/94	sfh		Add SLERR_SL_INACTIVE.
							Add SLERR_UNICODE_NOT_SUPPORTED.
		04/21/94	sfh		Change SLERR_SL_NOT_ACTIVE to SLERR_INSUFFICIENT_PRIVILEGES.
		05/04/94	sfh		Add CPERR_MODULE_REMOVED, CPERR_INVALID_OVERLAY, and CPERR_CANCEL_ERROR.
	05/18/94    par	    Updated the	OS/2 error codes

****************************************************************************************************************************************************/

#ifndef	ERROR_H_FILE
#define	ERROR_H_FILE


/****************************************************************** INCLUDES ***********************************************************************/

/***************************************************************************************************************************************************/


/******************************************************************* DEFINES ***********************************************************************/

#define	DMI_GENERAL_ERRORS					0x00000000

/* Non-error condition codes */
#define	SLERR_NO_ERROR						DMI_GENERAL_ERRORS + 0x00000000
#define	SLERR_NO_ERROR_MORE_DATA				DMI_GENERAL_ERRORS + 0x00000001


/* Database Errors */
#define	DB_ERRORS						DMI_GENERAL_ERRORS + 0x00000100

#define	DBERR_ATTRIBUTE_NOT_FOUND				DB_ERRORS + 0x00000000
#define	DBERR_VALUE_EXCEEDS_MAXSIZE				DB_ERRORS + 0x00000001
#define	DBERR_COMPONENT_NOT_FOUND				DB_ERRORS + 0x00000002
#define	DBERR_ENUM_ERROR					DB_ERRORS + 0x00000003
#define	DBERR_GROUP_NOT_FOUND					DB_ERRORS + 0x00000004
#define	DBERR_ILLEGAL_KEYS					DB_ERRORS + 0x00000005
#define	DBERR_ILLEGAL_TO_SET					DB_ERRORS + 0x00000006
#define	DBERR_OVERLAY_NAME_NOT_FOUND				DB_ERRORS + 0x00000007
#define	DBERR_ILLEGAL_TO_GET					DB_ERRORS + 0x00000008
#define	DBERR_NO_DESCRIPTION					DB_ERRORS + 0x00000009
#define	DBERR_ROW_NOT_FOUND					DB_ERRORS + 0x0000000a
#define	DBERR_DIRECT_INTERFACE_NOT_REGISTERED			DB_ERRORS + 0x0000000b
#define	DBERR_DATABASE_CORRUPT					DB_ERRORS + 0x0000000c
#define	DBERR_ATTRIBUTE_NOT_SUPPORTED				DB_ERRORS + 0x0000000d
#define	DBERR_NO_PRAGMA						DB_ERRORS + 0x0000000e
#define	DBERR_VALUE_UNKNOWN					DB_ERRORS + 0x0000000f
#define	DBERR_LIMITS_EXCEEDED					DB_ERRORS + 0x00000010


/* Service Layer Errors	*/
#define	SL_ERRORS						DMI_GENERAL_ERRORS + 0x00000200

#define	SLERR_BUFFER_FULL					SL_ERRORS + 0x00000000
#define	SLERR_ILL_FORMED_COMMAND				SL_ERRORS + 0x00000001
#define	SLERR_ILLEGAL_COMMAND					SL_ERRORS + 0x00000002
#define	SLERR_ILLEGAL_HANDLE					SL_ERRORS + 0x00000003
#define	SLERR_OUT_OF_MEMORY					SL_ERRORS + 0x00000004
#define	SLERR_NULL_COMPLETION_FUNCTION				SL_ERRORS + 0x00000005
#define	SLERR_NULL_RESPONSE_BUFFER				SL_ERRORS + 0x00000006
#define	SLERR_CMD_HANDLE_IN_USE					SL_ERRORS + 0x00000007
#define	SLERR_ILLEGAL_DMI_LEVEL					SL_ERRORS + 0x00000008
#define	SLERR_UNKNOWN_CI_REGISTRY				SL_ERRORS + 0x00000009
#define	SLERR_COMMAND_CANCELED					SL_ERRORS + 0x0000000a
#define	SLERR_INSUFFICIENT_PRIVILEGES				SL_ERRORS + 0x0000000b
#define	SLERR_NULL_ACCESS_FUNCTION				SL_ERRORS + 0x0000000c
#define	SLERR_FILE_ERROR					SL_ERRORS + 0x0000000d
#define	SLERR_EXEC_FAILURE					SL_ERRORS + 0x0000000e
#define	SLERR_BAD_MIF_FILE					SL_ERRORS + 0x0000000f
#define	SLERR_INVALID_FILE_TYPE					SL_ERRORS + 0x00000010
#define	SLERR_SL_INACTIVE					SL_ERRORS + 0x00000011
#define	SLERR_UNICODE_NOT_SUPPORTED				SL_ERRORS + 0x00000012
#define	SLERR_CANT_UNINSTALL_SL_COMPONENT			SL_ERRORS + 0x00000013
#define	SLERR_NULL_CANCEL_FUNCTION				SL_ERRORS + 0x00000014

/* DMI DOS ERRORS							     */
/* --------------							     */
#define	DMI_DOS_ERRORS						0x00001000

/* Overlay Manager Errors */
#define	DOS_OM_ERRORS						DMI_DOS_ERRORS + 0x00000000

#define	OMERR_OUT_OF_PARTITION_MEMORY				DOS_OM_ERRORS +	0x00000000
#define	OMERR_OVERLAY_NOT_FOUND					DOS_OM_ERRORS +	0x00000001
#define	OMERR_READING_FILE					DOS_OM_ERRORS +	0x00000002
#define	OMERR_SECURITY_VIOLATION				DOS_OM_ERRORS +	0x00000003

#define	DMI_DOS_SYSTEM_ERRORS					DMI_DOS_ERRORS + 0x00000100

#define	DOSERR_SL_BUSY						DMI_DOS_SYSTEM_ERRORS +	0x00000001

/* DMI WINDOWS ERRORS							     */
/* ------------------							     */
#define	DMI_WINDOWS_ERRORS					0x00002000

#define	WINERR_OUT_OF_DPMI_MEMORY				DMI_WINDOWS_ERRORS + 0x00000000
#define	WINERR_OUT_OF_DPMI_CALLBACKS			DMI_WINDOWS_ERRORS + 0x00000001
#define	WINERR_UNABLE_TO_FIND_TASK_HANDLE		DMI_WINDOWS_ERRORS + 0x00000002

/* DMI OS/2 ERRORS OR NT SPECIFIC ERRORS				     */
/* ----------------							     */
#if defined(_WIN32)
#define	DMI_OS2_ERRORS 0x00006000
#else
#define	DMI_OS2_ERRORS 0x00003000
#endif

#define	SLERR_NOT_INITIALIZED			DMI_OS2_ERRORS + 0x00000000
#define	SLERR_IPC_CREATE_ERROR			DMI_OS2_ERRORS + 0x00000001
#define	SLERR_THREAD_CREATE_ERROR		DMI_OS2_ERRORS + 0x00000002
#define	SLERR_QUEUE_CREATE_ERROR		DMI_OS2_ERRORS + 0x00000003
#define	SLERR_SL_TERMINATED			DMI_OS2_ERRORS + 0x00000004
#define	SLERR_CMD_EXCEPTION			DMI_OS2_ERRORS + 0x00000005
#define	SLERR_SYNC_SETUP_ERROR			DMI_OS2_ERRORS + 0x00000006
#define	SLERR_SL_DLL_MISMATCH			DMI_OS2_ERRORS + 0x00000007
#define	SLERR_IPC_ERROR				DMI_OS2_ERRORS + 0x00000008


/* DMI UNIX ERRORS							     */
/* ----------------							     */
#define	DMI_UNIX_ERRORS						0x00004000

#define	SLERR_SOCKET_EAFNOSUPPORT				DMI_UNIX_ERRORS	+ 0x00000001
#define	SLERR_SOCKET_ESOCKTNOSUPPORT				DMI_UNIX_ERRORS	+ 0x00000002
#define	SLERR_SOCKET_EMFILE					DMI_UNIX_ERRORS	+ 0x00000003
#define	SLERR_SOCKET_ENOBUFS					DMI_UNIX_ERRORS	+ 0x00000004
#define	SLERR_SOCKET_UNKNOWN_ERROR				DMI_UNIX_ERRORS	+ 0x00000005
#define	SLERR_BIND_EBADF					DMI_UNIX_ERRORS	+ 0x00000006
#define	SLERR_BIND_ENOTSOCK					DMI_UNIX_ERRORS	+ 0x00000007
#define	SLERR_BIND_EADDRNOTAVAIL				DMI_UNIX_ERRORS	+ 0x00000008
#define	SLERR_BIND_EADDRINUSE					DMI_UNIX_ERRORS	+ 0x00000009
#define	SLERR_BIND_EINVAL					DMI_UNIX_ERRORS	+ 0x0000000a
#define	SLERR_BIND_EACCES					DMI_UNIX_ERRORS	+ 0x0000000b
#define	SLERR_BIND_EFAULT					DMI_UNIX_ERRORS	+ 0x0000000c
#define	SLERR_BIND_UNKNOWN_ERROR				DMI_UNIX_ERRORS	+ 0x0000000d
#define	SLERR_LISTEN_EBADF					DMI_UNIX_ERRORS	+ 0x0000000e
#define	SLERR_LISTEN_ECONNREFUSED				DMI_UNIX_ERRORS	+ 0x0000000f
#define	SLERR_LISTEN_ENOTSOCK					DMI_UNIX_ERRORS	+ 0x00000010
#define	SLERR_LISTEN_EOPNOTSUPP					DMI_UNIX_ERRORS	+ 0x00000011
#define	SLERR_LISTEN_UNKNOWN_ERROR				DMI_UNIX_ERRORS	+ 0x00000012
#define	SLERR_QUEUE_EACCES					DMI_UNIX_ERRORS	+ 0x00000013
#define	SLERR_QUEUE_ENOENT					DMI_UNIX_ERRORS	+ 0x00000014
#define	SLERR_QUEUE_ENOSPC					DMI_UNIX_ERRORS	+ 0x00000015
#define	SLERR_QUEUE_EEXIST					DMI_UNIX_ERRORS	+ 0x00000016
#define	SLERR_QUEUE_UNKNOWN_ERROR				DMI_UNIX_ERRORS	+ 0x00000017

/* DMI COMPONENT ERRORS							     */
/* --------------------							     */
#define	DMI_COMPONENT_ERRORS					0x00010000

#define	CPERR_MODULE_REMOVED					DMI_COMPONENT_ERRORS + 0x00000000
#define	CPERR_CANCEL_ERROR					DMI_COMPONENT_ERRORS + 0x00000001		/* Who knows what this means; it's in the spec.	*/
#define	CPERR_INVALID_OVERLAY					DMI_COMPONENT_ERRORS + 0x00000002
#define	CPERR_CI_TERMINATED					DMI_COMPONENT_ERRORS + 0x00000003
#define	CPERR_GET_ERROR			DMI_COMPONENT_ERRORS + 0x00000004
#define	CPERR_KEY_ERROR			DMI_COMPONENT_ERRORS + 0x00000005
#define	CPERR_RELEASE_ERROR		DMI_COMPONENT_ERRORS + 0x00000006
#define	CPERR_RESERVE_ERROR		DMI_COMPONENT_ERRORS + 0x00000007
#define	CPERR_ROW_ERROR			DMI_COMPONENT_ERRORS + 0x00000008
#define	CPERR_SET_ERROR			DMI_COMPONENT_ERRORS + 0x00000009

/***************************************************************************************************************************************************/


/******************************************************************** TYPEDEFS *********************************************************************/

typedef	unsigned long SL_ErrorCode_t;

/***************************************************************************************************************************************************/


/**********************************************************************	DATA ***********************************************************************/

/***************************************************************************************************************************************************/

/**************************************************************** FUNCTION PROTOTYPES **************************************************************/

/***************************************************************************************************************************************************/


#endif
