/*       Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*       Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T		*/
/*         All Rights Reserved						*/

/*       THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF			*/
/*       UNIX System Laboratories, Inc.					*/
/*       The copyright notice above does not evidence any		*/
/*       actual or intended publication of such source code.		*/
/*									*/
/*       .ident  "@(#)kern-ml:misc.s     1.4.4.5"			*/
/*									*/
/*       Copyright (c) 1987, 1988 Microsoft Corporation			*/
/*         All Rights Reserved						*/

/*       This Module contains Proprietary Information of Microsoft	*/
/*       Corporation and should be treated as Confidential.		*/

/*
 * Copyright (c) 1987, 1995, 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)copy.s	1.27	97/06/29 SMI"

#include <sys/errno.h>
#include <sys/asm_linkage.h>

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/systm.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

/* Stack offsets for [bk]copy. */

#define FROMADR	8	/* source address */
#define TOADR	12	/* destination address */
#define BCOUNT	16	/* count of bytes to copy */

/*
 * Copy a block of storage, returning an error code if `from' or
 * `to' takes a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
kcopy(const void *from, void *to, size_t count)
{ return (0); }

#else	/* lint */

	.text
	.align	4

	ENTRY(kcopy)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jb	kcopy_panic

	cmpl	$KERNELBASE, 8(%esp)
	jb	kcopy_panic
#endif DEBUG

	lea	copyerr, %eax		/ copyerr is lofault value

do_copy_fault:
	pushl	%ebp
	movl	%esp, %ebp		/ setup stack frame
	pushl	%esi
	pushl	%edi			/ save registers
	/ save the old lofault value and install the new one
	movl	%gs:CPU_THREAD, %edx	
	movl	T_LOFAULT(%edx), %edi
	pushl	%edi			/ save the current lofault
	movl	%eax, T_LOFAULT(%edx)	/ new lofault

	movl	BCOUNT(%ebp), %eax	/ get count
	orl	%eax, %eax
	jz	.bcpfdone		/ count ?= 0
	movl	FROMADR(%ebp), %esi	/ get source address
	movl	TOADR(%ebp), %edi	/ get destination address
	cmpl	$NBPW, %eax		/ are there less bytes to copy
					/ than in a word ?
	jl	.bcpftail		/ treat it like the tail at end
					/ of a non-aligned bcopy
	testl	$NBPW-1, %esi		/ is source address word aligned?
	jz	.bcpfalign		/ do copy from word-aligned source
	/
	/ copy bytes until source address is word aligned 
	/
	movl	%esi, %ecx		/ get source address
	andl	$NBPW-1, %ecx		/ src_add modulo NBPW

	neg	%ecx
	addl	$NBPW, %ecx		/ %ecx now is # bytes to next word

	subl	%ecx, %eax		/ decrement count of bytes to copy
	repz
	smovb				/ move bytes from %ds:si -> %es:di
	/ if no of bytes to move now is less than in a word
	/ jump to tail copy
	cmpl	$NBPW, %eax
	jl	.bcpftail 

	/ %esi= word aligned source address
	/ %edi = destination address
	/ %eax = count of bytes to copy
.bcpfalign:
	movl	%eax, %ecx		/ get count of bytes to copy
	andl	$NBPW-1, %eax		/ %eax gets remainder bytes to copy
	shrl	$2, %ecx		/ convert to count of words to copy
	repz
	smovl				/ copy words

	/ %esi=source address
	/ %edi = dest address
	/ %eax = byte count to copy
.bcpftail:
	movl	%eax, %ecx
	repz
	smovb
.bcpfdone:
	xorl	%eax,%eax
.bcpfault:
	/ restore the original lofault
	popl	%ecx
	popl	%edi
	movl	%ecx, T_LOFAULT(%edx)	/ original lofault
	popl	%esi
	popl	%ebp
	ret

#ifdef DEBUG
/* We got here because our arguments are less than kernebase */

kcopy_panic:

	pushl	$.kcopy_panic_msg
	call	panic

#endif DEBUG

/*
 * We got here because of a fault during kcopy.
 * Errno value is in %eax.
 */
copyerr:
	jmp	.bcpfault
	SET_SIZE(kcopy)

#ifdef DEBUG
	.data
.kcopy_panic_msg:
	.string "kcopy: arguments below kernelbase"
	.text
#endif DEBUG


#endif	/* lint */

/*
 *
 *	This is the block copy routine.
 *
 *		bcopy(from, to, bytes)
 *		caddr_t from, to;
 *		{
 *			while( bytes-- > 0 )
 *				*to++ = *from++;
 *		}
 *
 *
 * This code assumes no faults(except page) will occur while executing this code
 * Hence this can not be used for copying data to or from user memory
 * Use copyin() and copyout() instead
 *
 * Several Cases We have to take care of:
 *	1) Source is word aligned.
 *		Copy as many whole words as possible.
 *		Go to 3 
 *	2) Source is not word aligned
 *		Copy bytes until source is word aligned.
 *		Go to 1)
 *	3) Count of bytes is less than NBPW
 *		copy bytes
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
bcopy(const void *from, void *to, size_t count)
{}

#else	/* lint */

	ENTRY(bcopy)
#ifdef DEBUG
	movl	12(%esp), %eax
	orl	%eax, %eax
	jz	.bcopy_real		/ if count is zero,	
					/ dont bother checking
	cmpl	$KERNELBASE, 4(%esp)
	jb	bcopy_panic

	cmpl	$KERNELBASE, 8(%esp)
	jb	bcopy_panic

.bcopy_real:
#endif DEBUG
        / need this entry point to avoid the above DEBUG code
        ENTRY(bcopy_nodebug)

do_copy:
	pushl	%ebp
	movl	%esp, %ebp		/ setup stack frame
	pushl	%esi
	pushl	%edi			/ save registers

	movl	BCOUNT(%ebp), %eax	/ get count
	orl	%eax, %eax
	jz	.bcpdone		/ count ?= 0
	movl	FROMADR(%ebp), %esi	/ get source address
	movl	TOADR(%ebp), %edi	/ get destination address
	cmpl	$NBPW, %eax		/ are there less bytes to copy
					/ than in a word ?
	jl	.bcptail		/ treat it like the tail at end
					/ of a non-aligned bcopy
	testl	$NBPW-1, %esi		/ is source address word aligned?
	jz	.bcpalign		/ do copy from word-aligned source
	/
	/ copy bytes until source address is word aligned 
	/
	movl	%esi, %ecx		/ get source address
	andl	$NBPW-1, %ecx		/ src_add modulo NBPW

	neg	%ecx
	addl	$NBPW, %ecx		/ %ecx now is # bytes to next word

	subl	%ecx, %eax		/ decrement count of bytes to copy
	repz
	smovb				/ move bytes from %ds:si -> %es:di
	/ if no of bytes to move now is less than in a word
	/ jump to tail copy
	cmpl	$NBPW, %eax
	jl	.bcptail 

	/ %esi= word aligned source address
	/ %edi = destination address
	/ %eax = count of bytes to copy
.bcpalign:
	movl	%eax, %ecx		/ get count of bytes to copy
	shrl	$2, %ecx		/ convert to count of words to copy
	andl	$NBPW-1, %eax		/ %eax gets remainder bytes to copy
	repz
	smovl				/ copy words

	/ %esi=source address
	/ %edi = dest address
	/ %eax = byte count to copy
.bcptail:
	movl	%eax, %ecx
	repz
	smovb
.bcpdone:
	popl	%edi
	xorl	%eax,%eax		/ No one should use return value
					/ but, just in case
	popl	%esi
	popl	%ebp
	ret

#ifdef DEBUG
/* We got here because our arguments are less than kernebase */

bcopy_panic:

	pushl	$.bcopy_panic_msg
	call	panic

	.data
.bcopy_panic_msg:
	.string "bcopy: arguments below kernelbase"
	.text

#endif DEBUG

#endif	/* lint */


/*
 * Zero a block of storage, returning an error code if we
 * take a kernel pagefault which cannot be resolved.
 * Returns errno value on pagefault error, 0 if all ok
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
kzero(void *addr, size_t count)
{ return (0); }

#else	/* lint */

/* stack offsets for kzero */
#define	KZEROADDR	8
#define	KZEROCOUNT	12

	ENTRY(kzero)
#ifdef DEBUG
        cmpl    $KERNELBASE, 4(%esp)
        jb      kzero_panic
#endif DEBUG_UAC

	lea	zeroerr, %eax		/ zeroerr is lofault value

do_zero_fault:
	pushl	%ebp			/ save stack base
	movl	%esp, %ebp		/ set new stack base
	pushl	%edi			/ save %edi
	/ save the old lofault value and install the new one
	mov	%gs:CPU_THREAD, %edx	
	movl	T_LOFAULT(%edx), %edi
	pushl	%edi			/ save the current lofault
	movl	%eax, T_LOFAULT(%edx)	/ new lofault

	movl	KZEROADDR(%ebp), %edi	/ %edi <- address of bytes to clear
	xorl	%eax, %eax		/ sstol val
	movl	KZEROCOUNT(%ebp), %ecx	/ get size in bytes
	cmpl	%eax, %ecx		/ short circut if len = 0
	jz	.bzfdone
	shrl	$2, %ecx		/ Count of double words to zero
	repz
	sstol				/ %ecx contains words to clear(%eax=0)
	movl	KZEROCOUNT(%ebp), %ecx	/ get size in bytes
	andl	$3, %ecx		/ do mod 4
	repz
	sstob				/ %ecx contains residual bytes to clear
.bzfdone:
	/ restore the original lofault
	popl	%edi
	movl	%edi, T_LOFAULT(%edx)	/ original lofault

	popl	%edi
	popl	%ebp
	ret

#ifdef DEBUG
/* We got here because our arguments are less than kernebase */
 
kzero_panic:
 
        pushl   $.kzero_panic_msg
        call    panic
 
#endif DEBUG

/*
 * We got here because of a fault during kzero.
 * Errno value is in %eax.
 */
zeroerr:
	jmp	.bzfdone
	SET_SIZE(kzero)

#ifdef DEBUG
        .data
.kzero_panic_msg:
        .string "kzero: arguments below kernelbase"
        .text
#endif DEBUG

#endif	/* lint */

/*
 * Zero a block of storage. (also known as blkclr)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
bzero(void *addr, size_t count)
{}

/* ARGSUSED */
void
blkclr(void *addr, size_t count)
{}

#else	/* lint */

/* stack offsets for bzero */
#define	ZEROADDR	8
#define	ZEROCOUNT	12

	ENTRY2(bzero,blkclr)

#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jb	bzero_panic
#endif

do_zero:
	pushl	%ebp			/ save stack base
	movl	%esp, %ebp		/ set new stack base
	pushl	%edi			/ save %edi

	movl	ZEROADDR(%ebp), %edi	/ %edi <- address of bytes to clear
	xorl	%eax, %eax		/ sstol val
	movl	ZEROCOUNT(%ebp), %ecx	/ get size in bytes
	cmpl	%eax, %ecx		/ short circut if len = 0
	jz	.bzdone
	shrl	$2, %ecx		/ Count of double words to zero
	repz
	sstol				/ %ecx contains words to clear(%eax=0)
	movl	ZEROCOUNT(%ebp), %ecx	/ get size in bytes
	andl	$3, %ecx		/ do mod 4
	repz
	sstob				/ %ecx contains residual bytes to clear
.bzdone:

	popl	%edi
	popl	%ebp
	ret

#ifdef DEBUG
bzero_panic:
	pushl	$.bzero_panic_msg
	call	panic

	.data
.bzero_panic_msg:
	.string "bzero: arguments below kernelbase"
	.text

#endif DEBUG

#endif	/* lint */

/*
 * Transfer data to and from user space -
 * Note that these routines can cause faults
 * It is assumed that the kernel has nothing at
 * less than KERNELBASE in the virtual address space.
 *
 * Note that copyin(9F) and copyout(9F) are part of the
 * DDI/DKI which specifies that they return '-1' on "errors."
 *
 * Sigh.
 *
 * So there's two extremely similar routines - xcopyin() and xcopyout()
 * which return the errno that we've faithfully computed.  This
 * allows other callers (e.g. uiomove(9F)) to work correctly.
 * Given that these are used pretty heavily, we expand the calling
 * sequences inline for all flavours (rather than making wrappers).
 */

/*
 * Copy user data to kernel space.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_copyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyin)
#ifdef DEBUG
	cmpl	$KERNELBASE, 8(%esp)
	jb	copyin_panic
#endif
	movl	$copyioerr, %eax
	cmpl	$KERNELBASE, 4(%esp)	/ test uaddr < KERNELBASE
	jb	do_copy_fault
	movl	$-1, %eax		/ return failure
	ret

#ifdef DEBUG
copyin_panic:
	pushl	$.copyin_panic_msg
	call	panic

	.data
.copyin_panic_msg:
	.string "copyin: To argument below kernelbase"
	.text
#endif

/*
 * We got here because of a fault during copy{in,out}.
 * Errno value is in %eax, but DDI/DKI says return -1 (sigh).
 */
copyioerr:
	movl	$-1, %eax
	jmp	.bcpfault
	SET_SIZE(default_copyin)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_xcopyin(const void *uaddr, void *kaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyin)
	movl	$xcopyioerr, %eax
	cmpl	$KERNELBASE, 4(%esp)	/ test uaddr < KERNELBASE
	jb	do_copy_fault
	movl	$EFAULT, %eax		/ return failure
	ret

/*
 * We got here because of a fault during copy{in,out}.
 * Errno value is in %eax.
 */
xcopyioerr:
	jmp	.bcpfault
	SET_SIZE(default_xcopyin)

#endif	/* lint */

/*
 * Copy kernel data to user space.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_copyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyout)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jb	copyout_panic
#endif
	movl	$copyioerr, %eax
	cmpl	$KERNELBASE, 8(%esp)	/ test uaddr < KERNELBASE
	jb	do_copy_fault
	movl	$-1, %eax		/ return failure
	ret

#ifdef DEBUG
copyout_panic:
	pushl	$.copyout_panic_msg
	call	panic

	.data
.copyout_panic_msg:
	.string "copyout: From argument below kernelbase"
	.text
#endif
	SET_SIZE(default_copyout)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_xcopyout(const void *kaddr, void *uaddr, size_t count)
{ return (0); }

#else	/* lint */

	ENTRY(default_xcopyout)
	movl	$xcopyioerr, %eax
	cmpl	$KERNELBASE, 8(%esp)	/ test uaddr < KERNELBASE
	jb	do_copy_fault
	movl	$EFAULT, %eax		/ return failure
	ret
	SET_SIZE(default_xcopyout)

#endif	/* lint */

/*
 * Copy a null terminated string from one point to another in
 * the kernel address space.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(copystr)

#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jb	copystr_panic

	cmpl	$KERNELBASE, 8(%esp)
	jb	copystr_panic
#endif DEBUG
	
	/ get the current lofault address
	movl	%gs:CPU_THREAD, %eax
	movl	T_LOFAULT(%eax), %eax
do_copystr:
	pushl	%ebp			/ setup stack frame
	movl	%esp,%ebp
	pushl	%ebx			/ save registers
	pushl	%edi
	/ save the old lofault value and install the new one
	movl	%gs:CPU_THREAD, %ebx	
	movl	T_LOFAULT(%ebx), %edi
	pushl	%edi			/ save the current lofault
	movl	%eax, T_LOFAULT(%ebx)	/ new lofault

	movl	16(%ebp),%ecx
	cmpl	$0,%ecx
	je	copystr_err1		/ maxlength == 0
	jg	copystr_doit		/ maxlength > 0
	movl	$EFAULT,%eax		/ return EFAULT
copystr_done:
	/ restore the original lofault
	popl	%edi
	movl	%gs:CPU_THREAD, %ebx	
	movl	%edi, T_LOFAULT(%ebx)	/ original lofault

	popl	%edi
	popl	%ebx
	popl	%ebp
	ret	
copystr_doit:
	movl	8(%ebp), %ebx		/ source address
	movl	12(%ebp), %edx		/ destination address
copystr_loop:
	decl	%ecx
	movb	(%ebx),%al
	movb	%al,(%edx)
	incl	%ebx
	incl	%edx
	cmpb	$0,%al
	je	copystr_null		/ null char
	cmpl	$0,%ecx
	jne	copystr_loop
copystr_err1:
	movl	$ENAMETOOLONG,%eax	/ ret code = ENAMETOOLONG
	jmp	copystr_out
copystr_null:
	xorl	%eax, %eax		/ no error
copystr_out:
	cmpl	$0,20(%ebp)		/ want length?
	je	copystr_done		/ no
	movl	16(%ebp), %edx
	subl	%ecx, %edx		/ compute length and store it
	movl	20(%ebp),%ecx
	movl	%edx,(%ecx)
	jmp	copystr_done

#ifdef DEBUG
copystr_panic:
	pushl	$.copystr_panic_msg
	call	panic

	.data
.copystr_panic_msg:
	.string	"copystr: arguments in user space"
	.text

#endif DEBUG

	SET_SIZE(copystr)

#endif	/* lint */

/*
 * Copy a null terminated string from the user address space into
 * the kernel address space.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_copyinstr(const char *uaddr, char *kaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyinstr)
#ifdef DEBUG
	cmpl	$KERNELBASE, 8(%esp)
	jb	copyinstr_panic
#endif
	movl	$copystrerr, %eax
	cmpl	$KERNELBASE, 4(%esp)	/ test uaddr < KERNELBASE
	jb	do_copystr
	movl	$EFAULT, %eax		/ return EFAULT
	ret

#ifdef DEBUG
copyinstr_panic:
	pushl	$.copyinstr_panic_msg
	call	panic

	.data
.copyinstr_panic_msg:
	.string	"copyinstr: To args not in kernel space"
	.text
#endif
	SET_SIZE(default_copyinstr)

#endif	/* lint */

/*
 * Copy a null terminated string from the kernel
 * address space to the user address space.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_copyoutstr(const char *kaddr, char *uaddr, size_t maxlength,
    size_t *lencopied)
{ return (0); }

#else	/* lint */

	ENTRY(default_copyoutstr)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jb	copyoutstr_panic
#endif
	movl	$copystrerr, %eax
	cmpl	$KERNELBASE, 8(%esp)	/ test uaddr < KERNELBASE
	jb	do_copystr
	movl	$EFAULT, %eax		/ return EFAULT
	ret

#ifdef DEBUG
copyoutstr_panic:
	pushl	$.copyoutstr_panic_msg
	call	panic

	.data
.copyoutstr_panic_msg:
	.string	"copyoutstr: From args not in kernel space"
	.text
#endif

/*
 * Fault while trying to move from or to user space.
 * Set and return error code.
 */
copystrerr:
	movl	$EFAULT, %eax		/ return EFAULT
	jmp	copystr_done
	SET_SIZE(default_copyoutstr)

#endif	/* lint */

/*
 * Fetch user word.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_fuword32(const void *addr, uint32_t *dst)
{ return (0); }

/* ARGSUSED */
int
default_fuiword32(const void *addr, uint32_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY(default_fuword32)
	ENTRY(default_fuiword32)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	(%eax), %eax		/ get the word
	movl	%eax, (%edx)		/ put the word in dst
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
	SET_SIZE(default_fuword32)
	SET_SIZE(default_fuiword32)

#endif	/* lint */

/*
 * Fetch user byte.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_fuword8(const void *addr, uint8_t *dst)
{ return (0); }

/* ARGSUSED */
int
default_fuiword8(const void *addr, uint8_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY(default_fuword8)
	ENTRY(default_fuiword8)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movb	(%eax), %al		/ get the byte
	movb	%al, (%edx)		/ put the byte in dst
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
	SET_SIZE(default_fuword8)
	SET_SIZE(default_fuiword8)

#endif	/* lint */

/*
 * Set user word.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_suword32(void *addr, uint32_t value)
{ return (0); }

/* ARGSUSED */
int
default_suiword32(void *addr, uint32_t value)
{ return (0); }

#else	/* lint */

	ENTRY(default_suword32)
	ENTRY(default_suiword32)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movl	%edx, (%eax)		/ set the word
	movl	$0, %eax		/ indicate success
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
	SET_SIZE(default_suword32)
	SET_SIZE(default_suiword32)

#endif	/* lint */

/*
 * Set user byte.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_suword8(void *addr, uint8_t value)
{ return (0); }

/* ARGSUSED */
int
default_suiword8(void *addr, uint8_t value)
{ return (0); }

#else	/* lint */

	ENTRY(default_suword8)
	ENTRY(default_suiword8)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movb	%dl, (%eax)		/ set the byte
	movl	$0, %eax		/ indicate success
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
	SET_SIZE(default_suword8)
	SET_SIZE(default_suiword8)

#endif	/* lint */

/*
 * Fetch user short (half) word.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_fuword16(const void *addr, uint16_t *dst)
{ return (0); }

#else	/* lint */

	ENTRY(default_fuword16)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movw	(%eax), %ax		/ get the half word
	movw	%ax, (%edx)		/ put the value in dst
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
	SET_SIZE(default_fuword16)

#endif	/* lint */

/*
 * Set user short word.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
default_suword16(void *addr, uint16_t value)
{ return (0); }

#else	/* lint */

	ENTRY(default_suword16)
	cmpl	$KERNELBASE, 4(%esp)
	jae	fsuerr			/ if (KERNELBASE >= addr) error
	/ set t_lofault to catch any fault
	movl	%gs:CPU_THREAD, %eax	
	lea	fsuerr, %edx
	movl	%edx, T_LOFAULT(%eax)	/ fsuerr is lofault value
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movw	%dx, (%eax)		/ set the half word
	movl	$0, %eax		/ indicate success
fsuret:
	movl	%gs:CPU_THREAD, %edx	
	movl	$0, T_LOFAULT(%edx)	/ clear t_lofault
	ret
fsuerr:
	movl	$-1, %eax		/ return error
	jmp	fsuret
	SET_SIZE(default_suword16)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
fuword32_noerr(const void *addr, uint32_t *dst)
{}

/*ARGSUSED*/
void
fuword8_noerr(const void *addr, uint8_t *dst)
{}

/*ARGSUSED*/
void
fuword16_noerr(const void *addr, uint16_t *dst)
{}

/*ARGSUSED*/
void
fulword_noerr(const void *addr, u_long *dst)
{}

#else   /* lint */

	ENTRY2(fuword32_noerr,fulword_noerr)
	movl	4(%esp), %eax		/ source address
	movl	8(%esp), %edx		/ destination address
	movl	(%eax), %eax		/ get the word
	movl	%eax, (%edx)		/ and store it
	ret
	SET_SIZE(fuword32_noerr)
	SET_SIZE(fulword_noerr)

	ENTRY(fuword16_noerr)
	movl	4(%esp), %eax		/ source address
	movl	8(%esp), %edx		/ destination address
	movw	(%eax), %ax		/ get the half word
	movw	%ax, (%edx)		/ and store it
	ret
	SET_SIZE(fuword16_noerr)

	ENTRY(fuword8_noerr)
	movl	4(%esp), %eax		/ source address
	movl	8(%esp), %edx		/ destination address
	movb	(%eax), %al		/ get the byte
	movb	%al, (%edx)		/ and store it
	ret
	SET_SIZE(fuword8_noerr)

#endif /* lint */

#if defined(lint)

/*ARGSUSED*/
void
suword32_noerr(void *addr, uint32_t value)
{}

/*ARGSUSED*/
void
suword16_noerr(void *addr, uint16_t value)
{}

/*ARGSUSED*/
void
suword8_noerr(void *addr, uint8_t value)
{}

/*ARGSUSED*/
void
subyte_noerr(void *addr, u_char value)
{}

/*ARGSUSED*/
void
sulword_noerr(void *addr, u_long value)
{}

#else

	ENTRY2(suword32_noerr,sulword_noerr)
	movl    4(%esp), %eax
        movl    8(%esp), %edx
        movl    %edx, (%eax)            / set the word
	ret
	SET_SIZE(suword32_noerr)
	SET_SIZE(sulword_noerr)

	ENTRY(suword16_noerr)
	movl    4(%esp), %eax
        movl    8(%esp), %edx
        movw    %dx, (%eax)             / set the half word
	ret
	SET_SIZE(suword16_noerr)

	ENTRY2(suword8_noerr,subyte_noerr)
	movl	4(%esp), %eax
	movl	8(%esp), %edx
	movb	%dl, (%eax)		/ set the byte
	ret
	SET_SIZE(suword8_noerr)
	SET_SIZE(subyte_noerr)

#endif /* lint */


#if defined(lint)

/*
 * Copy a block of storage - must not overlap (from + len <= to).
 * No fault handler installed (to be called under on_fault())
 */

/* ARGSUSED */
void
copyout_noerr(const void *kfrom, void *uto, size_t count)
{}

/* ARGSUSED */
void
copyin_noerr(const void *ufrom, void *kto, size_t count)
{}

/*
 * Zero a block of storage in user space
 */

/* ARGSUSED */
void
uzero(void *addr, size_t count)
{}

/*
 * copy a block of storage in user space
 */

/* ARGSUSED */
void
ucopy(const void *ufrom, void *uto, size_t ulength)
{}

#else lint

	ENTRY(copyin_noerr)

#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jae	copyin_ne_panic

	cmpl	$KERNELBASE, 8(%esp)
	jae	do_copyin_ne
copyin_ne_panic:

	pushl	$.cpyin_ne_pmsg
	call	panic
#endif
do_copyin_ne:

	jmp	do_copy

	SET_SIZE(copyin_noerr)

#ifdef DEBUG
	.data
.cpyin_ne_pmsg:
	.string "copyin_noerr: arguments not in correct address space"
	.text
#endif

	ENTRY(copyout_noerr)
#ifdef DEBUG
	cmpl	$KERNELBASE, 8(%esp)
	jae	copyout_ne_panic

	cmpl	$KERNELBASE, 4(%esp)
	jae	do_copyout_ne
copyout_ne_panic:

	pushl	$.cpyout_ne_pmsg
	call	panic
#endif DEBUG
do_copyout_ne:

	jmp	do_copy

	SET_SIZE(copyout_noerr)

#ifdef DEBUG
	.data
.cpyout_ne_pmsg:
	.string "copyout_noerr: arguments not in correct address space"
	.text
#endif



	ENTRY(uzero)
#ifdef	DEBUG
	cmpl	$KERNELBASE, 4(%esp)
	jae	uzero_panic
#endif DEBUG
	
	jmp	do_zero
#ifdef DEBUG
uzero_panic:

	pushl	$.uzero_panic_msg
	call	panic
#endif
	SET_SIZE(uzero)

#ifdef DEBUG
	.data
.uzero_panic_msg:
	.string "uzero: argument is not in user space"
	.text

#endif DEBUG
	
	
	ENTRY(ucopy)

#ifdef DEBUG
	cmpl 	$KERNELBASE, 4(%esp)
	jae	ucopy_panic

	cmpl	$KERNELBASE, 8(%esp)
	jae	ucopy_panic
#endif DEBUG

	jmp	do_copy


#ifdef DEBUG
ucopy_panic:

	pushl	$.ucopy_panic_msg
	call	panic
#endif
	SET_SIZE(ucopy)

#ifdef DEBUG
	.data
.ucopy_panic_msg:
	.string "ucopy: argument is not in user space"
	.text

#endif DEBUG
#endif lint
