/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)pwrite.s	1.4	97/02/12 SMI"

/*
 * C library -- pwrite
 * ssize_t pwrite(int fildes, const void *buf, size_t nbyte, off_t offset);
 */

	.file	"pwrite.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pwrite,function)

#include "SYS.h"

	SYSCALL_RESTART(pwrite)
	RET

	SET_SIZE(pwrite)
