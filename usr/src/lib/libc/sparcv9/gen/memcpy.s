/*
 * Copyright (c) 1997, Sun Microsystems, Inc.
 * All rights reserved.
 */

.ident	"@(#)memcpy.s 1.14     97/03/25 SMI"

	.file	"memcpy.s"

/*
 * memcpy(s1, s2, len)
 *
 * Copy s2 to s1, always copy n bytes.
 * Note: this does not work for overlapped copies, bcopy() does
 *
 * Added entry __align_cpy_1 is generally for use of the compilers.
 *
 *
 * Fast assembler language version of the following C-program for memcpy
 * which represents the `standard' for the C-library.
 *
 *	void *
 *	memcpy(s, s0, n)
 *	void *s;
 *	const void *s0;
 *	register size_t n;
 *	{
 *		if (n != 0) {
 *			register char *s1 = s;
 *			register const char *s2 = s0;
 *			do {
 *				*s1++ = *s2++;
 *			} while (--n != 0);
 *		}
 *		return ( s );
 *	}
 */

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(memcpy,function)

#include "synonyms.h"

	ENTRY(memcpy)
	ENTRY(__align_cpy_1)
	mov	%o0, %g5		! save des address for return val
	cmp	%o2, 17			! for small counts copy bytes
	ble,pn	%xcc, .dbytecp
	andcc	%o1, 3, %o5		! is src word aligned
	bz,pn	%icc, .aldst
	cmp	%o5, 2			! is src half-word aligned
	be,pt	%xcc, .s2algn
	cmp	%o5, 3			! src is byte aligned
.s1algn:ldub	[%o1], %o3		! move 1 or 3 bytes to align it
	inc	1, %o1
	stb	%o3, [%g5]		! move a byte to align src
	inc	1, %g5
	bne,pt	%icc, .s2algn
	dec	%o2
	b	.ald			! now go align dest
	andcc	%g5, 3, %o5

.s2algn:lduh	[%o1], %o3		! know src is 2 byte alinged
	inc	2, %o1
	srl	%o3, 8, %o4
	stb	%o4, [%g5]		! have to do bytes,
	stb	%o3, [%g5 + 1]		! don't know dst alingment
	inc	2, %g5
	dec	2, %o2

.aldst:	andcc	%g5, 3, %o5		! align the destination address
.ald:	bz,pn	%icc, .w4cp
	cmp	%o5, 2
	bz,pn	%icc, .w2cp
	cmp	%o5, 3
.w3cp:	lduw	[%o1], %o4
	inc	4, %o1
	srl	%o4, 24, %o5
	stb	%o5, [%g5]
	bne,pt	%icc, .w1cp
	inc	%g5
	dec	1, %o2
	andn	%o2, 3, %o3		! o3 is aligned word count
	dec	4, %o3			! avoid reading beyond tail of src
	sub	%o1, %g5, %o1		! o1 gets the difference

1:	sll	%o4, 8, %g1		! save residual bytes
	lduw	[%o1+%g5], %o4
	deccc	4, %o3
	srl	%o4, 24, %o5		! merge with residual
	or	%o5, %g1, %g1
	st	%g1, [%g5]
	bnz,pt	%xcc, 1b
	inc	4, %g5
	sub	%o1, 3, %o1		! used one byte of last word read
	and	%o2, 3, %o2
	b	7f
	inc	4, %o2

.w1cp:	srl	%o4, 8, %o5
	sth	%o5, [%g5]
	inc	2, %g5
	dec	3, %o2
	andn	%o2, 3, %o3		! o3 is aligned word count
	dec	4, %o3			! avoid reading beyond tail of src
	sub	%o1, %g5, %o1		! o1 gets the difference

2:	sll	%o4, 24, %g1		! save residual bytes
	lduw	[%o1+%g5], %o4
	deccc	4, %o3
	srl	%o4, 8, %o5		! merge with residual
	or	%o5, %g1, %g1
	st	%g1, [%g5]
	bnz,pt	%xcc, 2b
	inc	4, %g5
	sub	%o1, 1, %o1		! used three bytes of last word read
	and	%o2, 3, %o2
	b	7f
	inc	4, %o2

.w2cp:	lduw	[%o1], %o4
	inc	4, %o1
	srl	%o4, 16, %o5
	sth	%o5, [%g5]
	inc	2, %g5
	dec	2, %o2
	andn	%o2, 3, %o3		! o3 is aligned word count
	dec	4, %o3			! avoid reading beyond tail of src
	sub	%o1, %g5, %o1		! o1 gets the difference
	
3:	sll	%o4, 16, %g1		! save residual bytes
	lduw	[%o1+%g5], %o4
	deccc	4, %o3
	srl	%o4, 16, %o5		! merge with residual
	or	%o5, %g1, %g1
	st	%g1, [%g5]
	bnz,pt	%xcc, 3b
	inc	4, %g5
	sub	%o1, 2, %o1		! used two bytes of last word read
	and	%o2, 3, %o2
	b	7f
	inc	4, %o2

.w4cp:	andn	%o2, 3, %o3		! o3 is aligned word count
	sub	%o1, %g5, %o1		! o1 gets the difference

1:	lduw	[%o1+%g5], %o4		! read from address
	deccc	4, %o3			! decrement count
	st	%o4, [%g5]		! write at destination address
	bg,pt	%xcc, 1b
	inc	4, %g5			! increment to address
	b	7f
	and	%o2, 3, %o2		! number of leftover bytes, if any

	!
	! differenced byte copy, works with any alignment
	!
.dbytecp:
	b	7f
	sub	%o1, %g5, %o1		! o1 gets the difference

4:	stb	%o4, [%g5]		! write to address
	inc	%g5			! inc to address
7:	deccc	%o2			! decrement count
	bge,a,pt %xcc,4b		! loop till done
	ldub	[%o1+%g5], %o4		! read from address
	retl
	nop

	SET_SIZE(memcpy)
	SET_SIZE(__align_cpy_1)
