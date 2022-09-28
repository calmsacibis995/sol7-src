/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)ntp_malloc.h	1.1	96/11/01 SMI"

/*
 * Define malloc and friends.
 */
#ifndef  _ntp_malloc_h

#define  _ntp_malloc_h
#ifdef NTP_POSIX_SOURCE
#include <stdlib.h>
#else /* NTP_POSIX_SOURCE */
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#endif /* NTP_POSIX_SOURCE */

#endif /* _ntp_malloc_h */
