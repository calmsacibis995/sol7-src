/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_skt_server.h	1.1 96/08/05 Sun Microsystems"

/********************************************************************************************************

    Filename: os_skt_server.h


    Description: Include Information for all the porting concerns

    This file contains changes for non-X socket communications

*********************************************************************************************************/

#ifndef SKT_SRVR_H_FILE
#define SKT_SRVR_H_FILE

#include "os_skt_common.h"

typedef struct {
   int     bytesRcvd;
   int     bytesLeft;
   DataPtr rcvData;
   } SKTMSG;

SKTMSG *socketMessage[MAXFDS + 1];

void os_setupSocket(int *Socket);
void *os_waitForSocketEventsLoop();
void os_removeSocket(int Socket);
void os_serverShutdown(void *junk);
int os_waitForSocketEvents();
int writeSocketMsg(int Socket, DataPtr packet);
int writeSocketQueue(int Socket);
int os_addSocket(int Socket);
int os_newHighSocket(void);
int os_socketActive(int Socket);

#endif
