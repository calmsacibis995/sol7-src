/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _DAEMON_H
#define	_DAEMON_H

#pragma ident	"@(#)daemon.h	1.4	96/12/19 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* Return codes from daemon() */
#define	DAEMON_OK		0
#define	DAEMON_FORK_FAILED	(-1)
#define	DAEMON_SETPGRP_FAILED	(-2)
#define	DAEMON_NOTTY_FAILED	(-3)
#define	DAEMON_CANNOT_CDROOT	(-4)

/* Agent name - for logging purposes */
#define	DAEMON_NAME		"dhcpagent"

int daemon(void);
#ifdef	__CODE_UNUSED
int isdaemon(void);
#endif	/* __CODE_UNUSED */

#ifdef	__cplusplus
}
#endif

#endif /* _DAEMON_H */
