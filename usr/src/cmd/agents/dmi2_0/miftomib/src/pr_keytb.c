/* Copyright 09/24/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)pr_keytb.c	1.2 96/09/24 Sun Microsystems"


/**********************************************************************
    Filename: pr_keytb.c
    
    Copyright (c) Intel, Inc. 1992,1993

    Description: Keyword table for MIF Installer.

    Author(s): Alvin I. Pivowar

    RCS Revision: $Header: j:/mif/parser/rcs/keytab.c 1.8 1994/03/22 16:29:57 apivowar Exp $

    Revision History:

        Date     Author Description
        -------  ---    -----------------------------------------------
        12/6/93  aip    Removed environment keyword.
                        Added OS keywords.
        3/22/94  aip    4.3
                        Added MACOS environment.
        8/24/94  aip    Name change from keytab.c

************************* INCLUDES ***********************************/

#include "pr_key.h"

/*********************************************************************/

/************************ GLOBALS ************************************/

PR_KeywordTable_t KeywordTable[] = {{"Access",       1},
                                    {"Attrib",       2},
                                    {"Attribute",    2},
                                    {"Class",        3},
                                    {"Comment",      4},
                                    {"Component",    5},
                                    {"Description",  6},
                                    {"End",          7},
                                    {"Enum",         8},
                                    {"Enumeration",  8},
                                    {"Env",          9},
                                    {"Group",       10},
                                    {"Id",          11},
                                    {"Key",         12},
                                    {"Language",    13},
                                    {"Name",        14},
                                    {"Paths",       15},
                                    {"Path",        15},
                                    {"Start",       16},
                                    {"Storage",     17},
                                    {"Table",       18},
                                    {"Type",        19},
                                    {"Value",       20},
                                    {"Pragma",      21},

/*
                                    OS Keywords
*/

                                    {"Dos",         PR_OS_KEYWORDS + 1},
                                    {"Macos",       PR_OS_KEYWORDS + 2},
                                    {"Os2",         PR_OS_KEYWORDS + 3},
                                    {"Unix",        PR_OS_KEYWORDS + 4},
                                    {"Win16",       PR_OS_KEYWORDS + 5},
                                    {"Win32",       PR_OS_KEYWORDS + 6},
                                    {"Win9x",       PR_OS_KEYWORDS + 7},
                                    {"Winnt",       PR_OS_KEYWORDS + 8},
                 
                                    {(char *) 0,     0}};

/*********************************************************************/
