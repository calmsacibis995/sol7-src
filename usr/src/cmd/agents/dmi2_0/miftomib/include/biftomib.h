/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)biftomib.h	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: biftomib.h

    Copyright (c) Intel, Inc. 1992,1993,1994

    Description: Header for BIF to MIB translator

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/bif/rcs/biftomib.h 1.2 1994/11/14 05:45:47 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        8/10/94  aip    Creation date.
        11/14/94 aip    Revised.

**********************************************************************/

#ifndef BIFTOMIB_H_FILE
#define BIFTOMIB_H_FILE

/************************ INCLUDES ***********************************/

/*********************************************************************/

/************************ DEFINES ************************************/

#define BIF_TO_MIB_VERSION "1.0a"

/*********************************************************************/

/************************ TYPEDEFS ***********************************/

typedef enum {BifToMibFailure, BifToMibSuccess} BifToMibStatus_t;

/*********************************************************************/

/************************ PUBLIC *************************************/

BifToMibStatus_t bifToMib(char *mibPathname,
                          char *enterpriseName,
                          char *enterpriseValue); /* $MED */

/*********************************************************************/

#endif

