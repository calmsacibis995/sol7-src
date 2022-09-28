/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)pread.s	1.3	97/02/12 SMI"

/*
 * C library -- pread
 * ssize_t pread(int fildes, void *buf, size_t nbyte, off_t offset);
 */

	.file	"pread.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(pread,function)

#include "SYS.h"

	SYSCALL_RESTART(pread)
	RET

	SET_SIZE(pread)
