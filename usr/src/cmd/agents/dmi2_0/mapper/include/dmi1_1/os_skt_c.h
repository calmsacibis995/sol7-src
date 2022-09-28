/* Copyright 09/17/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_skt_common.h	1.2 96/09/17 Sun Microsystems"

/********************************************************************************************************

    Filename: os_skt_common.h


    Description: Include Information for all the porting concerns

    This file contains base socket communications stuff, common to all socket
    communications schemes (currently X and Other)

*********************************************************************************************************/

#ifndef SKT_COMMON_H_FILE
#define SKT_COMMON_H_FILE

#include "os_svc.h"     /* note this file pulls in: os_dmi.h, psl_mh.h, psl_om.h, psl_tm.h */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#define MAXFDS OPEN_MAX    /* was 256 */

#define miciSocket "/tmp/dmislSocket"

typedef enum SKTTYPE {
   SERVERSOCKET,
   CLIENTSOCKET
   } SocketType;

int socketActive;

int os_openSocket(int *Socket);
int os_createSocket(int *Socket);
int writeSocket(int socketFD, DataPtr packet);
int os_readSocketMessage(int Socket);
int os_closeSocket(int Socket);
void *newAddress(void *thingie);
int os_shutdownSocket(int Socket);
int os_startSocketThread();
void os_shutdownSocketThread();

#endif
