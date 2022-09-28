/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)open.s	1.4	97/04/22 SMI"

	.file	"open.s"

#include "SYS.h"

/*
 * C library -- open
 * int open(const char *path, int oflag, [ mode_t mode ] )
 */

	.weak	__open;
	.type	__open, #function
	__open = open

	SYSCALL(open)
	RET

	SET_SIZE(open)
