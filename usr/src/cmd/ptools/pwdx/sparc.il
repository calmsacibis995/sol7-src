/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sparc.il	1.2	97/03/29	SMI"

	.inline	syscall, 0
	mov	%o0, %g1
	mov	%o1, %o0
	mov	%o2, %o1
	mov	%o3, %o2
	mov	%o4, %o3
	mov	%o5, %o4
	mov	0, %o5
	ta	8
	bcs,a	.+8
	mov	-1, %o0
	.end
