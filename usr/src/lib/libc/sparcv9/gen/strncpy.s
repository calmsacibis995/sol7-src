/*
 *	.seg	"data"
 *	.asciz	"@(#)strncpy.s 1.4 89/10/02"
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 */

.ident	"@(#)strncpy.s	1.13	97/02/09 SMI"

	.file	"strncpy.s"

/*
 * strncpy(s1, s2)
 *
 * Copy string s2 to s1, truncating or null-padding to always copy n bytes
 * return s1.
 *
 * Fast assembler language version of the following C-program for strncpy
 * which represents the `standard' for the C-library.
 *
 *	char *
 *	strncpy(s1, s2, n)
 *	register char *s1;
 *	register const char *s2;
 *	register size_t n;
 *	{
 *		register char *os1 = s1;
 *	
 *		n++;				
 *		while ((--n != 0) &&  ((*s1++ = *s2++) != '\0'))
 *			;
 *		if (n != 0)
 *			while (--n != 0)
 *				*s1++ = '\0';
 *		return (os1);
 *	}
 */

#include <sys/asm_linkage.h>
#include "synonyms.h"

#define	DEST	%i0
#define	SRC	%i1
#define CNT	%i2
#define DESTSV	%i5
#define ADDMSK	%l0
#define	ANDMSK	%l1
#define	MSKB0	%l2
#define	MSKB1	%l3
#define	MSKB2	%l4
#define SL	%o0
#define	SR	%o1
#define	MSKB3	0xff

	ENTRY(strncpy)
	save    %sp, -SA(WINDOWSIZE), %sp	! get a new window
	cmp	CNT, 8			! do a byte at a time for small n
	ble,pn	%xcc, .pad_trunc
	mov	DEST, DESTSV		! save return value
	andcc	SRC, 3, %i3		! is src word aligned
	bz,pn	%icc, .aldest
	cmp	%i3, 2			! is src half-word aligned
	be,pn	%icc, .s2algn
	cmp	%i3, 3			! src is byte aligned
	ldub	[SRC], %i3		! move 1 or 3 bytes to align it
	inc	SRC
	dec	CNT
	stb	%i3, [DEST]		! move a byte to align src
	be,pt	%icc, .s3algn
	tst	%i3
.s1algn:bnz,pt	%icc, .s2algn		! now go align dest
	inc	1, DEST
	b,a	.pad_null
.s2algn:lduh	[SRC], %i3		! know src is 2 byte alinged
	inc	2, SRC
	dec	2, CNT
	srl	%i3, 8, %i4
	tst	%i4
	stb	%i4, [DEST]
	bnz,a,pt %icc, 1f
	stb	%i3, [DEST + 1]
	inc	CNT			! back count up, only one transferred
	inc	DEST
	b,a	.pad_null
1:	andcc	%i3, MSKB3, %i3
	bnz,pt	%icc, .aldest
	inc	2, DEST
	b,a	.pad_null
.s3algn:bnz,pt	%icc, .aldest
	inc	1, DEST
	b,a	.pad_null

.aldest: 
	/*
	 * In all the destination alignment cases:
	 * CNT >= 4 upon entry. The idea is to copy a word at a time
	 * until we hit a NULL, or the CNT becomes less than 4 in which
	 * case we call pad_trunc in order to finish off the copying
	 * a byte at a time or padding the destination with 0's
	 */
	set     0x7efefeff, ADDMSK	! masks to test for terminating null
	set     0x81010100, ANDMSK
	sethi	%hi(0xff000000), MSKB0
	sethi	%hi(0x00ff0000), MSKB1

	/*
	 * source address is now aligned
	 * We are guarenteed that there are at least 4 bytes left in
	 * the count since we compare for n <= 8 at the entry
	 */
	andcc	DEST, 3, %i3		! is destination word aligned?
	bz,pn	%icc, .w4str
	srl	MSKB1, 8, MSKB2		! generate 0x0000ff00 mask
	cmp	%i3, 2			! is destination halfword aligned?
	be,pn	%icc, .w2str			
	cmp	%i3, 3			! worst case, dest is byte alinged
.w1str:	lduw	[SRC], %i3		! read a word
	inc	4, SRC			! point to next one
	be,pn	%icc, .w3str
	/*
	 * Destination is byte aligned upon entry. We have to move 3 bytes
	 * in order to align the destination.
	 */
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	/* First byte was null */
	stb	%g0, [DEST]
	inc	DEST
	dec	CNT
	b	.pad_null		! finish up
	dec	3,SRC

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	/* Second byte was null, dest is byte aligned */
	dec	2, CNT
        srl     %i3, 24, %o3
        stb     %o3, [DEST]
	stb	%g0, [DEST + 1]
	inc	2, DEST
	b	.pad_null
	dec	2, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	/* Third byte was null, dest is byte aligned */
        srl     %i3, 8, %o3
        sth     %o3, [DEST + 1]		! store bytes 1, 2
	srl	%i3, 24, %o3
	stb	%o3, [DEST]		! store byte 0
	dec	3, CNT
	inc	3, DEST
	b	.pad_null
	dec	SRC

1:	bz,pt	%icc, 1f
	/* Fourth byte is NOT null, move 3 bytes to align dest */
        srl     %i3, 24, %o3            ! move three bytes to align dest
        stb     %o3, [DEST]		! store byte 0
        srl     %i3, 8, %o3
        sth     %o3, [DEST + 1]		! store bytes 1, 2
        inc     3, DEST                 ! destination now aligned
	dec	3, CNT
        b       7f
        mov     %i3, %o3
1:
	/* Fourth byte was null, dest is byte aligned */
	srl	%i3, 24, %o3
	stb	%o3, [DEST]		! store byte 0
	srl	%i3, 8, %o3
	sth	%o3, [DEST + 1]		! store byte 1, 2
	stb	%g0, [DEST + 3]		! store byte 3
	inc	4, DEST			! destination now aligned
	b	.pad_null
	dec	4, CNT			! Moved 4 bytes

	/* Top of loop */
2:	inc	4, DEST
7:	deccc	4, CNT			! check if CNT < 4
	bge,pt	%xcc, 1f
	sll	%o3, 24, %o2		! delay slot, save remaining byte
	/* Time to bail out */
	dec	SRC			! reset src ptr
	b	.pad_trunc
	inc	4, CNT			! delay slot, original value
1:
	andcc	%o2,MSKB0,%g0		! mhm check that remaining byte not NULL
	bnz,a,pt %icc, 3f		! mhm Remaining byte != NULL, branch
	andcc	%o2,MSKB0,%g0		! mhm Delay statement
	stb	%g0, [DEST]		! mhm Remaining  byte was zero
	inc	DEST			! mhm
	inc	3, CNT			! mhm Only moved 1 byte
	b	.pad_null		! mhm may have to do some padding
	dec	3, SRC			! mhm delay slot

3:	lduw	[SRC], %o3		! read a word
	inc	4, SRC			! point to next one
	srl	%o3, 8, %i3
	or	%i3, %o2, %i3

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero

	stb	%g0, [DEST]		! first byte was zero
	inc	DEST
	inc	3, CNT			! Only moved 1 byte
	b	.pad_null		! may have to do some padding
	dec	3, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero

	/* Second byte was 0, aligned dest */
	srl	%i3, 16, %o3
	sth	%o3, [DEST]		! store bytes 0, 1
	inc	2, DEST
	inc	2, CNT
	b	.pad_null
	dec	2, SRC

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero

	/* Third byte was 0, aligned dest */
	srl	%i3, 16, %o3
	sth	%o3, [DEST]		! store bytes 0, 1
	stb	%g0, [DEST + 2]		! store byte 3
	inc	3, DEST
	inc	1, CNT
	b	.pad_null
	dec	SRC

1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pt	%icc, 7b		! if not zero, go read another word
	inc	4, DEST			! else finished
	b,a	.pad_null		! 4th byte was 0
/*
 * Destination is byte aligned upon entry. We have to move 1 byte
 * in order to align the destination.
 */
.w3str:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	/* First byte was null */
	stb	%g0, [DEST]
	inc	DEST
	dec	CNT
	b	.pad_null		! finish up
	dec	3,SRC

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero

	/* 2nd byte was null, dest is byte aligned */
	dec	2, CNT
        srl     %i3, 24, %o3
        stb     %o3, [DEST]
	stb	%g0, [DEST + 1]
	inc	2, DEST
	b	.pad_null
	dec	2, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero

	/* Third byte was null, dest is byte aligned */
        srl     %i3, 8, %o3
        sth     %o3, [DEST + 1]		! store bytes 1, 2
	srl	%i3, 24, %o3
	stb	%o3, [DEST]		! store byte 0
	dec	3, CNT
	inc	3, DEST
	b	.pad_null
	dec	SRC

1:	bz,pt	%icc, 1f
        srl     %i3, 24, %o3            ! move three bytes to align dest
	/* Fourth byte is NOT null, move 1 byte to align dest */
        stb     %o3, [DEST]		! store byte 0
        inc     DEST                    ! destination now aligned
	dec	CNT
        b       8f
        mov     %i3, %o3
1:
	/* Fourth byte was null, dest is byte aligned */
	stb	%o3, [DEST]		! store byte 0
	srl	%i3, 8, %o3
	sth	%o3, [DEST + 1]		! store byte 1, 2
	stb	%g0, [DEST + 3]		! store byte 3
	inc	4, DEST	
	b	.pad_null
	dec	4, CNT			! Moved 4 bytes

	/* Top of loop */
2:	inc	4, DEST
8:	deccc	4, CNT			! check if CNT < 4
	bge,pt	%xcc, 1f
	sll	%o3, 8, %o2		! delay slot, save remaining byte
	/* Time to bail out */
	dec	3, SRC			! reset src ptr
	b	.pad_trunc
	inc	4, CNT			! delay slot, original value
1:
	andcc	%o2, MSKB0, %g0		! mhm check if first byte was zero
	bnz,pt	%icc, 1f		! mhm branch if first byte != zero
	andcc	%o2, MSKB1, %g0		! mhm check if second byte was zero
	/* First byte was zero */
	stb	%g0, [DEST]		! mhm write out zero byte
	inc	DEST			! mhm
	inc	3, CNT			! mhm Only moved 1 byte
	b	.pad_null		! mhm may have to do some padding
	dec	3, SRC			! mhm delay slot

1:	bnz,pt	%icc, 1f		! mhm branch if second byte != zero
	andcc	%o2, MSKB2, %g0		! mhm check if third byte was zero

	/* Second byte was 0, aligned dest */
	srl	%o2, 16, %o3		! mhm
	sth	%o3, [DEST]		! mhm store bytes 0, 1
	inc	2, DEST			! mhm
	inc	2, CNT			! mhm
	b	.pad_null		! mhm
	dec	2, SRC			! mhm
 
1:	bnz,a,pt %icc, 3f		! mhm branch if 3rd byte != zero
	andcc	%o2, MSKB2, %g0		! mhm delay statement

	/* Third byte was 0, aligned dest */
	srl	%o2, 16, %o3		! mhm
	sth	%o3, [DEST]		! mhm store bytes 0, 1
	stb	%g0, [DEST + 2]		! mhm store byte 3
	inc	3, DEST			! mhm
	inc	1, CNT			! mhm
	b	.pad_null		! mhm
	dec	SRC			! mhm
3:	lduw	[SRC], %o3		! read a word
	inc	4, SRC			! point to next one
	srl	%o3, 24, %i3
	or	%i3, %o2, %i3

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	/* First byte was zero */
	stb	%g0, [DEST]
	inc	DEST
	inc	3, CNT			! Only moved 1 byte
	b	.pad_null		! may have to do some padding
	dec	3, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero

	/* Second byte was 0, aligned dest */
	srl	%i3, 16, %o3
	sth	%o3, [DEST]		! store bytes 0, 1
	inc	2, DEST
	inc	2, CNT
	b	.pad_null
	dec	2, SRC

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero

	/* Third byte was 0, aligned dest */
	srl	%i3, 16, %o3
	sth	%o3, [DEST]		! store bytes 0, 1
	stb	%g0, [DEST + 2]		! store byte 3
	inc	3, DEST
	inc	1, CNT
	b	.pad_null
	dec	SRC

1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pn	%icc, 8b		! if not zero, go read another word
	inc	4, DEST			! else finished
	b,a	.pad_null
/*
 * 
 * Destination is halfword aligned
 */
.w2str:	lduw	[SRC], %i3		! read a word
	inc	4, SRC			! point to next one
	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	stb	%g0, [DEST]
	inc	DEST
	dec	CNT
	b	.pad_null		! may have to do some padding
	dec	3, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero

	srl	%i3, 16, %i4		! second byte of word was 0
	sth	%i4, [DEST]		! store 2 bytes
	inc	2, DEST			! new DEST ptr
	dec	2, CNT			! 
	b	.pad_null		! may have to do some padding
	dec	2, SRC			! delay slot

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero

        srl     %i3, 16, %i4            ! third byte of word was the term 0
        sth     %i4, [DEST]
        stb     %g0, [DEST + 2]
        inc     3, DEST
        dec     3, CNT                  ! only really did 3 bytes
        b       .pad_null		! may have to do some padding
        dec     3, SRC
1:	bnz,pt	%icc, 1f
	srl	%i3, 16, %o3

	sth	%o3, [DEST]		! Last byte was 0
	sth	%i3, [DEST + 2]
	inc	4, DEST
	b	.pad_null
	dec	4, CNT
1:
	sth	%o3, [DEST]		
	inc	2, DEST			! DEST is now word aligned
	dec	2, CNT
	b	9f
	mov	%i3, %o3

	/* Top of loop */
.loop2:	inc	4, DEST
9:	deccc	4, CNT			! check if CNT < 4
	bge,a,pt %xcc, 1f
	sll	%o3, 16, %o2		! delay slot, save rest
	dec	2, SRC			! reset src ptr
	b	.pad_trunc
	inc	4, CNT			! delay slot, original value
1:
	andcc	%o2, MSKB0, %g0		! mhm check if first byte was zero
	bnz,pt	%icc, 3f		! mhm if first byte not zero,branch
	andcc	%o2, MSKB1, %g0		! mhm check if second byte was zero
	stb	%g0, [DEST]		! mhm 1st byte was zero
	inc	DEST			! mhm
	inc	3, CNT			! mhm wrote 1 byte
	b	.pad_null		! mhm may have to do some padding
	dec	3, SRC			! mhm delay slot

3:	bnz,a,pt %icc, 1f		! mhm if second byte not zero, branch
	andcc	%o2, MSKB1, %g0		! mhm delay statement
	srl	%o2, 16, %i4		! mhm second byte of word was 0
	sth	%i4, [DEST]		! mhm store 2 bytes
	inc	2, DEST			! mhm new DEST ptr
	inc	2, CNT			! mhm only wrote 2 bytes
	b	.pad_null		! mhm may have to do some padding
	dec	2, SRC			! mhm delay slot
1:	lduw	[SRC], %o3		! read a word
	inc	4, SRC			! point to next one
	srl	%o3, 16, %i3
	or	%i3, %o2, %i3

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, .loop2		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero

	/* First byte zero */
	stb	%g0, [DEST]		! store byte
	inc	DEST
	inc	3, CNT			! only really did 1 byte
	b	.pad_null		! may have to do some padding
	dec	3, SRC

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	srl	%i3, 16, %i4		! second byte of word was 0
	sth	%i4, [DEST]		! store 2 bytes
	inc	2, DEST
	inc	2, CNT			! only really did 2 bytes
	b	.pad_null
	dec	2, SRC			! delay slot, reset src ptr

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	srl	%i3, 16, %i4		! third byte of word was 0
	sth	%i4, [DEST]
	stb	%g0, [DEST + 2]
	inc	3, DEST
	inc	CNT			! only really did 3 bytes
	b	.pad_null
	dec	1, SRC			! delay slot, reset src ptr

1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pn	%icc, 9b		! if not zero, go read another word
	inc	4, DEST			! else fall through
	b,a	.pad_null

/*
 * Both src and destination fields are word aligned.
 * CNT >= 4 upon entry. The idea is to copy a word at a time
 * until we hit a NULL, or the CNT becomes less than 4 in which
 * case we call pad_trunc in order to finish off the copying
 * a byte at a time or padding the destination with 0's
 */
2:	inc	4, DEST
.w4str:	deccc	4, CNT
	ble,a,pn %xcc, .pad_trunc	! if less than 4 bytes left go 1 by 1
	inc	4, CNT			! delay slot, original value
	
	lduw	[SRC], %i3		! read a word
	inc	4, SRC			! point to next one

	add	%i3, ADDMSK, %l5	! check for a zero byte 
	xor	%l5, %i3, %l5
	and	%l5, ANDMSK, %l5
	cmp	%l5, ANDMSK
	be,a,pt	%icc, 2b		! if no zero byte in word, loop
	st	%i3, [DEST]

	andcc	%i3, MSKB0, %g0		! check if first byte was zero
	bnz,pt	%icc, 1f
	andcc	%i3, MSKB1, %g0		! check if second byte was zero
	stb	%g0, [DEST]		! store byte
	inc	DEST
	inc	3, CNT			! only really did 1 byte
	b	.pad_null		! may have to do some padding
	dec	3, SRC

1:	bnz,pt	%icc, 1f
	andcc 	%i3, MSKB2, %g0		! check if third byte was zero
	srl	%i3, 16, %i4		! second byte of word was the term 0
	sth	%i4, [DEST]
	inc	2, DEST
	inc	2, CNT			! only really did 2 bytes
	b	.pad_null
	dec	2, SRC

1:	bnz,pt	%icc, 1f
	andcc	%i3, MSKB3, %g0		! check if last byte is zero
	srl	%i3, 16, %i4		! third byte of word was the term 0
	sth	%i4, [DEST]
	stb	%g0, [DEST + 2]
	inc	3, DEST
	inc	CNT			! only really did 3 bytes
	b	.pad_null
	dec	SRC

1:	st	%i3, [DEST]		! it is safe to write the word now
	bnz,pn	%icc, .w4str		! if not zero, go read another word
	inc	4, DEST			! else pad the rest
	b,a	.pad_null

.done:	ret			! last byte of word was the terminating zero 
	restore	DESTSV, %g0, %o0

/*
 * Copy a byte at a time until we:
 *	1) Exhaust the CNT (may cause truncation)
 *	2) Hit a null in the src, at which point we pad the destination
 *	   with nulls until the CNT is done.
 */
.pad_trunc:
	deccc	CNT
	bneg,pn	%xcc, 2f	! all done now. May have truncated
	nop
	
	ldub	[SRC], %i3	! get byte
	inc	SRC
	stb	%i3, [DEST]	! store result
	tst	%i3		! check for zero
	bnz,pt	%icc, .pad_trunc
	inc	DEST		! delay slot

	/*  null pad the rest */
.pad_null:
	deccc	CNT
	bneg,pn	%xcc, 2f	! all done now.
	nop

	stb	%g0, [DEST]
	b	.pad_null
	inc	DEST		! delay slot
2:
	ret
	restore	DESTSV, %g0, %o0

	SET_SIZE(strncpy)
