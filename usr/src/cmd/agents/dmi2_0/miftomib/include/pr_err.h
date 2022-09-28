/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_err.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_err.h
    
    Copyright (c) Intel, Inc. 1992,1993,1994
    Copyright (C) International Business Machines, Corp. 1995

    Description: MIF Parser Error Logging Header

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/pr_err.h 1.11 1994/08/08 13:30:17 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        7/23/93  aip    Added EXPECTING_IDENTIFICATION error.
                        Changed line and column to unsigned long.
        12/3/93  aip    Added MISSING_CLASS error.
                        Added MISSING_KEY error.
        12/6/93  aip    Added EXPECTING_OS error.
        3/22/94  aip    4.3
        4/7/94   aip    4.4
        5/31/94  aip    Changed parameter named errno to errorNumber in
                        PR_errorLogAdd()
        8/1/94   aip    Added initialization routine.
                        Moved private prototypes from .h to .c
        10/10/95 par    Modified to remove dead code

**********************************************************************/

#ifndef PR_ERR_H_FILE
#define PR_ERR_H_FILE

/************************ INCLUDES ***********************************/

#include <setjmp.h>
#include "mif_db.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_ERROR_MAX_PER_LINE 9
#define PR_UNKNOWN_ERROR      0

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef unsigned short PR_ErrorNumber_t;

typedef enum {PR_INFO = 1000, PR_WARN = 2000, PR_ERROR = 3000,
              PR_FATAL = 4000} PR_ErrorClass_t;

typedef enum {PR_COMMENT_ERROR = 100, PR_ESCAPE_CHAR_ERROR = 200,
              PR_LITERAL_ERROR = 300, PR_SOURCE_ERROR = 400,
              PR_ATTRIBUTE_ERROR = 500, PR_ENUM_ERROR = 550,
              PR_COMPONENT_ERROR = 600, PR_TABLE_ERROR = 650,
              PR_GROUP_ERROR = 700, PR_KEY_ERROR = 750, PR_PATH_ERROR = 800,
              PR_TEMPLATE_ERROR = 850, PR_FILE_ERROR = 900} PR_ErrorSubject_t;

typedef enum {PR_ILLEGAL_CHARACTER = 1, PR_ILLEGAL_DIGIT, PR_ILLEGAL_VALUE,
              PR_MISSING_DIGIT, PR_UNEXPECTED_END_OF_COMMENT, PR_UNEXPECTED_EOF,
              PR_UNEXPECTED_NEW_LINE, PR_EXPECTING_START, PR_EXPECTING_NAME,
              PR_EXPECTING_ID, PR_EXPECTING_IDENTIFICATION, PR_EXPECTING_EQUALS,
              PR_EXPECTING_LITERAL, PR_EXPECTING_INTEGER,
              PR_EXPECTING_STATEMENT, PR_EXPECTING_BLOCK, PR_EXPECTING_END,
              PR_EXPECTING_OVERLAY, PR_EXPECTING_VALUE, PR_ILLEGAL_SIZE,
              PR_MISSING_BODY, PR_MISSING_ACCESS, PR_MISSING_TYPE,
              PR_MISSING_VALUE, PR_MISSING_SIZE, PR_NO_GROUPS, PR_NO_ATTRIBUTES,
              PR_CONFLICTING, PR_DUPLICATE_STATEMENT, PR_NOT_IMPLEMENTED,
              PR_DATABASE_FAULT, PR_OUT_OF_MEMORY, PR_EXPECTING_RIGHT_PAREN,
              PR_EXPECTING_TEMPLATE_CLASS, PR_TOO_MANY_VALUES,
              PR_EXPECTING_SEPARATOR, PR_WRITE_ONLY, PR_MISSING_CLASS,
              PR_MISSING_KEY, PR_EXPECTING_OS, PR_CID_ERROR,
              PR_NO_KEY, PR_UNUSED_TEMPLATES, PR_MIXED_ROW,
              PR_ROW_NOT_UNIQUE, PR_NO_DEFAULT_VALUE} PR_ErrorType_t;

typedef struct {
            unsigned long    line;
            unsigned long    col;
            PR_ErrorNumber_t errorNumber;
        } PR_Error_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

void            PR_errorInit(void);
unsigned short  PR_errorClass(PR_ErrorNumber_t errorNumber);
void            PR_errorFatalExceptionTrapSet(jmp_buf *fatalException);
PR_ErrorClass_t PR_errorLargestErrorClassEncounteredGet(void);
void            PR_errorLargestErrorClassEncounteredSet(PR_ErrorClass_t
                    errorClass);
MIF_Status_t    PR_errorLogAdd(unsigned long line, unsigned long col,
                               PR_ErrorNumber_t errorNumber);

/*********************************************************************/

#endif
