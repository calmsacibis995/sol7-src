/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)getmsg.s	1.1	96/12/04 SMI"	/* SVr4.0 1.5	*/

/* C library -- getmsg						*/
/* int getmsg (int fd, struct strbuf *ctlptr,			*/
/*	       struct strbuf *dataptr, int *flags)		*/

	.file	"getmsg.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(getmsg,function)

#include "SYS.h"

	SYSCALL_RESTART(getmsg)
	RET

	SET_SIZE(getmsg)
