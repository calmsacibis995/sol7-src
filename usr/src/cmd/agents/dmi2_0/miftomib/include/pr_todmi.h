/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_todmi.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: PR_todmi.h
    
    Copyright (C) International Business Machines, Corp. 1995

    Description: This module provides the functions to take the output of the
                 parser, and add it to the Service Layer database.

    Author(s): Paul A. Ruocchio

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        10/05/95 par    Creation date.

**********************************************************************/

#ifndef PR_TODMI_H_FILE
#define PR_TODMI_H_FILE

/************************ INCLUDES ***********************************/
/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {BIF_DOS = 1, BIF_MACOS, BIF_OS2, BIF_UNIX, BIF_WIN16,
              BIF_WINNT,BIF_WIN95,BIF_WIN32} BIF_OsEnvironment_t;

typedef enum {BIF_UNKNOWN_ACCESS_TYPE, BIF_READ_ONLY, BIF_READ_WRITE,
              BIF_WRITE_ONLY, BIF_UNSUPPORTED,BIF_UNKNOWN} BIF_AttributeAccess_t;

typedef enum {BIF_UNKNOWN_STORAGE_TYPE = -1, BIF_SPECIFIC_STORAGE,
              BIF_COMMON_STORAGE} BIF_AttributeStorage_t;

typedef enum {BIF_UNKNOWN_DATA_TYPE, BIF_COUNTER, BIF_COUNTER64, BIF_GAUGE,
              BIF_INT = 5, BIF_INT64, BIF_OCTETSTRING, BIF_DISPLAYSTRING,
              BIF_DATE = 11} BIF_AttributeDataType_t;

typedef enum {BIF_VALUE_TYPE, BIF_OVERLAY_TYPE} BIF_AttributeValueType_t;

#define BIF_FIRST_OS BIF_DOS
#define BIF_LAST_OS  BIF_WIN32

typedef enum {BifToDmiFailure, BifToDmiSuccess} BifToDmiStatus_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

BifToDmiStatus_t PR_ToDmi(void);
BIF_AttributeDataType_t BIF_stringToType(DMI_STRING *string);
BIF_OsEnvironment_t     BIF_stringToEnvironmentId(DMI_STRING *environment);

/*********************************************************************/

#endif
