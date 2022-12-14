/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)syssignal.c	1.1	96/11/01 SMI"

#include <stdio.h>
#include <sys/types.h>
#include <signal.h>

#include "ntp_stdlib.h"

#if defined(NTP_POSIX_SOURCE) && !defined(SYS_WINNT) && !defined(VMS)
#include <errno.h>

extern int errno;

void
signal_no_reset(sig, func)
int sig;
void (*func)();
{
    int n;
    struct sigaction vec;

    vec.sa_handler = func;
    sigemptyset(&vec.sa_mask);
    vec.sa_flags = 0;

    while (1) {
        n = sigaction(sig, &vec, NULL);
	if (n == -1 && errno == EINTR) continue;
	break;
    }
    if (n == -1) {
	perror("sigaction");
        exit(1);
    }
}

#else
RETSIGTYPE
signal_no_reset(sig, func)
int sig;
RETSIGTYPE (*func)();
{
    signal(sig, func);

}
#endif

