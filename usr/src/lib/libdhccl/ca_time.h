/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */


#ifndef _CA_TIME_H
#define	_CA_TIME_H

#pragma ident	"@(#)ca_time.h	1.3	96/11/26 SMI"

#include <catype.h>
#include <sys/time.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__CODE_UNUSED
time_t GetCurrentMilli(void);
int SetTimeOffset(int32_t);
#endif	/* __CODE_UNUSED */

time_t GetCurrentSecond(void);

#ifdef	__cplusplus
}
#endif

#endif /* _CA_TIME_H */
