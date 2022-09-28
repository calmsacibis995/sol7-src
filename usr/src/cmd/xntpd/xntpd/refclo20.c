/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)refclock_ptbacts.c	1.1	96/11/01 SMI"

/*
 * crude hack to avoid hard links in distribution
 * and keep only one ACTS type source for different
 * ACTS refclocks
 */
#ifdef PTBACTS
#define KEEPPTBACTS
#undef ACTS
#include "refclock_acts.c"
#endif
