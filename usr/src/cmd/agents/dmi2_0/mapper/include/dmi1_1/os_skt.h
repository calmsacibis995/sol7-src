/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_skt.h	1.1 96/08/05 Sun Microsystems"

/********************************************************************************************************

    Filename: os_skt.h


    Description: Include Information for all the porting concerns

    This file contains changes for non-X socket communications

*********************************************************************************************************/

#ifndef SKT_H_FILE
#define SKT_H_FILE

#include "os_skt_common.h"

void *os_waitForSocketMessagesLoop();

void os_clientShutdown();

#endif
