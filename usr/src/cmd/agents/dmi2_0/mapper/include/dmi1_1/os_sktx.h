/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)os_sktX.h	1.1 96/08/05 Sun Microsystems"

/********************************************************************************************************

    Filename: os_sktX.h


    Description: Include Information for all the porting concerns
 
    This file contains changes for X socket communications

*********************************************************************************************************/

#ifndef SKT_X_H_FILE
#define SKT_X_H_FILE
 
#include <X11/Intrinsic.h>
#include "os_skt_common.h"
 
XtInputId clientSocketXid;
 
XtInputCallbackProc os_readXtSocketMessage(XtPointer client_data, int *Socket, XtInputId *Id);
 
#endif
