/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_class.h	1.1 96/08/05 Sun Microsystems"

/**********************************************************************
    Filename: pr_class.h
    

    Description: MIF Parser Character Class Header

    Author(s): Alvin I. Pivowar

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------

**********************************************************************/

#ifndef PR_CLASS_H_FILE
#define PR_CLASS_H_FILE

/************************ INCLUDES ***********************************/

#include "mif_db.h"
#include "pr_lex.h"

/*********************************************************************/

/************************ DEFINES ************************************/

#define PR_ASCII_BEL 0x07
#define PR_ASCII_BS  0x08
#define PR_ASCII_HT  0x09
#define PR_ASCII_LF  0x0a
#define PR_ASCII_VT  0x0b
#define PR_ASCII_FF  0x0c
#define PR_ASCII_CR  0x0d
#define PR_ASCII_MAX 0xff

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef struct {
            PR_LexicalState_t state;
            char              *set;
        } PR_ClassTable_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

MIF_Bool_t PR_isClassMember(PR_LexicalState_t state, int c);

/*********************************************************************/

/************************ PRIVATE ************************************/
/*********************************************************************/

#endif
