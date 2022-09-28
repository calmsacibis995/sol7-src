/*
 * getctime.c: "UCT utilities".
 *
 * SYNOPSIS
 *	time_t GetCurrentSecond ()
 *	time_t GetCurrentMilli  ()
 *    int    SetTimeOffset    (int32_t secsWestGMT)
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)getctime.c 1.3 96/11/26 SMI"

#include "ca_time.h"

time_t
GetCurrentSecond(void)
{
	struct timeval tp;
	gettimeofday(&tp, (struct timezone *)NULL);
	return ((time_t)tp.tv_sec);
}

#ifdef	__CODE_UNUSED
time_t
GetCurrentMilli(void)
{
	struct timeval tp;
	gettimeofday(&tp, (struct timezone *)NULL);
	return ((time_t)tp.tv_usec / 1000);
}

int
SetTimeOffset(int32_t secsWestGMT)
{
	struct timezone tzp;
	tzp.tz_dsttime = DST_USA;
	tzp.tz_minuteswest = secsWestGMT / 60;
	return (settimeofday((struct timeval *)NULL, &tzp));
}
#endif	/* __CODE_UNUSED */
