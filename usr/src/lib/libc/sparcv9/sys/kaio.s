/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"@(#)kaio.s	1.1	96/12/04 SMI"	/* SVr4.0 1.9	*/

/* C library -- kaio						*/
/* int kaio (); */

	.file	"kaio.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(kaio,function)

#include "SYS.h"

	SYSCALL(kaio)
	RET

	SET_SIZE(kaio)
