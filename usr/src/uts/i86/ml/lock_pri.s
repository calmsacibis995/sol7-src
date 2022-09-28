/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)lock_prim.s	1.56	98/02/01 SMI"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/thread.h>
#include <sys/cpuvar.h>
#include <vm/page.h>
#include <sys/mutex_impl.h>
#else	/* lint */
#include "assym.h"
#endif	/* lint */

#include <sys/asm_linkage.h>
#include <sys/asm_misc.h>
#include <sys/regset.h>
#include <sys/rwlock_impl.h>
#include <sys/lockstat.h>

/*
 * lock_try(lp), ulock_try(lp)
 *	- returns non-zero on success.
 *	- doesn't block interrupts so don't use this to spin on a lock.
 *	
 *      ulock_try() is for a lock in the user address space.
 *      For all V7/V8 sparc systems they are same since the kernel and
 *      user are mapped in a user' context.
 *      For V9 platforms the lock_try and ulock_try are different impl.
 *	ulock_try() is here for compatiblility reasons only. We also
 * 	added in some checks under #ifdef DEBUG.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
lock_try(lock_t *lp)
{ return (0); }

/* ARGSUSED */
int
ulock_try(lock_t *lp)
{ return (0); }

#else	/* lint */

	ENTRY(lock_try)
	movl	$-1,%edx
	movl	4(%esp),%ecx		/ ecx = lock addr
	xorl	%eax,%eax
	xchgb	%dl, (%ecx)	/using dl will avoid partial
	testb	%dl,%dl		/stalls on P6 ?
	setz	%al
.lock_try_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx	/ edx = thread addr
	testl	%eax, %eax
	jnz	lockstat_enter_wrapper
	ret
	SET_SIZE(lock_try)

	ENTRY(ulock_try)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)		/ test uaddr < KERNELBASE
	jb	ulock_pass			/ uaddr < KERNELBASE, proceed

	pushl	$.ulock_panic_msg
	call	panic

#endif /* DEBUG */

ulock_pass:
	movl	$1,%eax
	movl	4(%esp),%ecx
	xchgb	%al, (%ecx)
	xorb	$1, %al
	ret
	SET_SIZE(ulock_try)

#ifdef DEBUG
	.data
.ulock_panic_msg:
	.string "ulock_try: Argument is above KERNELBASE"
	.text
#endif	/* DEBUG */

#endif	/* lint */

/*
 * lock_clear(lp)
 *	- unlock lock without changing interrupt priority level.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear(lock_t *lp)
{}

/* ARGSUSED */
void
ulock_clear(lock_t *lp)
{}

#else	/* lint */

	ENTRY(disp_lock_exit_high)
	ALTENTRY(lock_clear)
	movl	4(%esp), %eax
	movb	$0, (%eax)
.lock_clear_lockstat_patch_point:
	ret
	jmp	disp_lockstat_exit_wrapper
	SET_SIZE(lock_clear)
	SET_SIZE(disp_lock_exit_high)

	ENTRY(ulock_clear)
#ifdef DEBUG
	cmpl	$KERNELBASE, 4(%esp)		/ test uaddr < KERNELBASE
	jb	ulock_clr			/ uaddr < KERNELBASE, proceed

	pushl	$.ulock_clear_msg
	call	panic
#endif

ulock_clr:
	movl	4(%esp),%eax
	xorl	%ecx,%ecx
	movb	%cl, (%eax)
	ret
	SET_SIZE(ulock_clear)

#ifdef DEBUG
	.data
.ulock_clear_msg:
	.string "ulock_clear: Argument is above KERNELBASE"
	.text
#endif	/* DEBUG */


#endif	/* lint */

/*
 * lock_set_spl(lock_t *lp, int new_pil, u_short *old_pil)
 * Drops lp, sets pil to new_pil, stores old pil in *old_pil.
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_set_spl(lock_t *lp, int new_pil, u_short *old_pil)
{}

#else	/* lint */

	ENTRY(disp_lock_enter)
	call	splhigh
	movl	4(%esp), %ecx		/ ecx = lock addr
	movl	$-1, %edx
	xchgb	%dl, (%ecx)		/ try to set lock
	testb	%dl, %dl		/ did we get the lock? ...
	movl	%gs:CPU_THREAD, %edx	/ edx = thread addr (ZF unaffected)
	jnz	.dle_miss		/ ... no, go to C for the hard case
	movw	%ax, T_OLDSPL(%edx)	/ save old spl in thread
.disp_lock_enter_lockstat_patch_point:
	ret
	jmp	lockstat_enter_wrapper
.dle_miss:
	addl	$T_OLDSPL, %edx		/ ecx = t_oldspl addr
	pushl	%eax			/ original pil
	pushl	%edx			/ old_pil addr
	pushl	$LOCK_LEVEL		/ new_pil
	pushl	%ecx			/ lock addr
	call	lock_set_spl_spin
	addl	$16, %esp
	ret
	SET_SIZE(disp_lock_enter)

	ENTRY(lock_set_spl)
	movl	8(%esp), %eax		/ get priority level
	pushl	%eax
	call	splr			/ raise priority level
	movl 	8(%esp), %ecx		/ ecx = lock addr
	movl	$-1, %edx
	addl	$4, %esp
	xchgb	%dl, (%ecx)		/ try to set lock
	testb	%dl, %dl		/ did we get the lock? ...
	movl	12(%esp), %edx		/ edx = olp pil addr (ZF unaffected)
	jnz	.lss_miss		/ ... no, go to C for the hard case
	movw	%ax, (%edx)		/ store old pil
.lock_set_spl_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx	/ edx = thread addr
	jmp	lockstat_enter_wrapper
.lss_miss:
	pushl	%eax			/ original pil
	pushl	%edx			/ old_pil addr
	pushl	16(%esp)		/ new_pil
	pushl	%ecx			/ lock addr
	call	lock_set_spl_spin
	addl	$16, %esp
	ret
	SET_SIZE(lock_set_spl)

#endif	/* lint */

/*
 * void
 * lock_init(lp)
 */

#if defined(lint)

/* ARGSUSED */
void
lock_init(lock_t *lp)
{}

#else	/* lint */

	ENTRY(lock_init)
	movl	4(%esp), %eax
	movb	$0, (%eax)
	ret
	SET_SIZE(lock_init)

#endif	/* lint */

/*
 * void
 * lock_set(lp)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_set(lock_t *lp)
{}

#else	/* lint */

	ENTRY(disp_lock_enter_high)
	ALTENTRY(lock_set)
	movl	4(%esp), %ecx		/ ecx = lock addr
	movl	$-1, %edx
	xchgb	%dl, (%ecx)		/ try to set lock
	testb	%dl, %dl		/ did we get it?
	jnz	lock_set_spin		/ no, go to C for the hard case
.lock_set_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx	/ edx = thread addr
	jmp	lockstat_enter_wrapper
	SET_SIZE(lock_set)
	SET_SIZE(disp_lock_enter_high)

#endif	/* lint */

/*
 * lock_clear_splx(lp, s)
 */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
lock_clear_splx(lock_t *lp, int s)
{}

#else	/* lint */

	ALTENTRY(lock_clear_splx)
	LOADCPU(%ecx)			/ ecx = cpu pointer
	movl	4(%esp), %eax		/ eax = lock addr
	movl	8(%esp), %edx		/ edx = desired pil
	movb	$0, (%eax)		/ clear lock
	cli				/ disable interrupts
	call	spl			/ magic calling sequence
.lock_clear_splx_lockstat_patch_point:
	ret
	jmp	disp_lockstat_exit_wrapper
	SET_SIZE(lock_clear_splx)

	ENTRY(disp_lock_exit)
	LOADCPU(%ecx)			/ ecx = cpu pointer
	movl	4(%esp), %eax		/ eax = lock addr
	xorl	%edx, %edx
	cmpb	%dl, CPU_KPRUNRUN(%ecx)
	jnz	.disp_lock_exit_preempt
	movb	%dl, (%eax)		/ clear lock
	movl	CPU_THREAD(%ecx), %eax
	movw	T_OLDSPL(%eax), %dx	/ edx = desired pil
	cli				/ disable interrupts
	call	spl			/ magic calling sequence
.disp_lock_exit_lockstat_patch_point:
	ret
	jmp	disp_lockstat_exit_wrapper
.disp_lock_exit_preempt:
	pushl	%eax			/ eax = lock addr
	call	disp_lock_exit_nopreempt
	movl	$-1, (%esp)
	call	kpreempt
	addl	$4, %esp
	ret
	SET_SIZE(disp_lock_exit)

	ALTENTRY(disp_lock_exit_nopreempt)
	LOADCPU(%ecx)			/ ecx = cpu pointer
	movl	4(%esp), %eax		/ eax = lock addr
	xorl	%edx, %edx
	movb	%dl, (%eax)		/ clear lock
	movl	CPU_THREAD(%ecx), %eax
	movw	T_OLDSPL(%eax), %dx	/ edx = desired pil
	cli				/ disable interrupts
	call	spl			/ magic calling sequence
.disp_lock_exit_nopreempt_lockstat_patch_point:
	ret
	ALTENTRY(disp_lockstat_exit_wrapper)
	movl	%gs:CPU_THREAD, %edx		/ edx = thread addr
	incb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat++
	pushl	%edx				/ save thread pointer
	pushl	%edx				/ push owner
	pushl	$1				/ puch refcnt
	pushl	$LS_SPIN_LOCK_HOLD		/ push event
	pushl	16(%esp)			/ push caller
	pushl	24(%esp)			/ push lock
	call	*lockstat_exit
	addl	$20, %esp
	popl	%edx				/ restore thread pointer
	decb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat--
	ret
	SET_SIZE(disp_lockstat_exit_wrapper)
	SET_SIZE(disp_lock_exit_nopreempt)

#endif	/* lint */

/*
 * mutex_enter() and mutex_exit().
 * 
 * These routines handle the simple cases of mutex_enter() (adaptive
 * lock, not held) and mutex_exit() (adaptive lock, held, no waiters).
 * If anything complicated is going on we punt to mutex_vector_enter().
 *
 * mutex_tryenter() is similar to mutex_enter() but returns zero if
 * the lock cannot be acquired, nonzero on success.
 *
 * If mutex_exit() gets preempted in the window between checking waiters
 * and clearing the lock, we can miss wakeups.  Disabling preemption
 * in the mutex code is prohibitively expensive, so instead we detect
 * mutex preemption by examining the trapped PC in the interrupt path.
 * If we interrupt a thread in mutex_exit() that has not yet cleared
 * the lock, cmnint() resets its PC back to the beginning of
 * mutex_exit() so it will check again for waiters when it resumes.
 *
 * The lockstat code below is activated when the lockstat driver
 * calls lockstat_hot_patch() to hot-patch the kernel mutex code.
 * Note that we don't need to test lockstat_event_mask here -- we won't
 * patch this code in unless we're gathering ADAPTIVE_HOLD lockstats.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
mutex_enter(kmutex_t *lp)
{}
/* ARGSUSED */
int
mutex_tryenter(kmutex_t *lp)
{ return (0); }

/* ARGSUSED */
void
mutex_exit(kmutex_t *lp)
{}

#else

	ENTRY_NP(mutex_enter)
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	xorl	%eax, %eax			/ eax = 0 (unheld adaptive)
	lock
	cmpxchgl %edx, (%ecx)
	jnz	mutex_vector_enter
.mutex_enter_lockstat_patch_point:
	ret
	ALTENTRY(lockstat_enter_wrapper)	/ expects edx=thread, ecx=lock
	incb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat++
	pushl	%edx				/ save thread pointer
	pushl	%edx				/ push owner
	pushl	8(%esp)				/ push caller
	pushl	%ecx				/ push lock
	call	*lockstat_enter
	addl	$12, %esp
	popl	%edx				/ restore thread pointer
	decb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat--
	movl	$1, %eax			/ return success for tryenter
	ret
	SET_SIZE(lockstat_enter_wrapper)
	SET_SIZE(mutex_enter)

	ENTRY(mutex_tryenter)
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	xorl	%eax, %eax			/ eax = 0 (unheld adaptive)
	lock
	cmpxchgl %edx, (%ecx)
	jnz	mutex_vector_tryenter
	movl	%ecx, %eax
.mutex_tryenter_lockstat_patch_point:
	ret
	jmp	lockstat_enter_wrapper
	SET_SIZE(mutex_tryenter)

	.globl mutex_exit_critical_size

mutex_exit_critical_size = .mutex_exit_committed - mutex_exit

	ENTRY(mutex_exit)
	movl	%gs:CPU_THREAD, %edx
	movl	4(%esp), %ecx
	cmpl	%edx, (%ecx)
	jne	mutex_vector_exit		/ wrong type or wrong owner
	movl	$0, (%ecx)			/ clear owner AND lock
.mutex_exit_committed:
.mutex_exit_lockstat_patch_point:
	ret
	incb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat++
	pushl	%edx				/ save thread pointer
	pushl	%edx				/ push owner
	pushl	$1				/ push refcnt
	pushl	$LS_ADAPTIVE_MUTEX_HOLD		/ push event
	pushl	16(%esp)			/ push caller
	pushl	%ecx				/ push lock
	call	*lockstat_exit
	addl	$20, %esp
	popl	%edx				/ restore thread pointer
	decb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat--
	ret
	SET_SIZE(mutex_exit)

#endif	/* lint */

/*
 * rw_enter() and rw_exit().
 *
 * These routines handle the simple cases of rw_enter (write-locking an unheld
 * lock or read-locking a lock that's neither write-locked nor write-wanted)
 * and rw_exit (no waiters or not the last reader).  If anything complicated
 * is going on we punt to rw_enter_sleep() and rw_exit_wakeup(), respectively.
 */
#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
rw_enter(krwlock_t *lp, krw_t rw)
{}

/* ARGSUSED */
void
rw_exit(krwlock_t *lp)
{}

#else
	ENTRY(rw_enter)
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	movl	4(%esp), %ecx			/ ecx = lock ptr
	cmpl	$RW_WRITER, 8(%esp)
	je	.rw_write_enter
	incl	T_KPRI_REQ(%edx)		/ THREAD_KPRI_REQUEST()
	movl	(%ecx), %eax			/ eax = old rw_wwwh value
	testl	$RW_WRITE_LOCKED|RW_WRITE_WANTED, %eax
	jnz	rw_enter_sleep
	leal	RW_READ_LOCK(%eax), %edx	/ edx = new rw_wwwh value
	lock
	cmpxchgl %edx, (%ecx)			/ try to grab read lock
	jnz	rw_enter_sleep
.rw_read_enter_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	jmp	lockstat_enter_wrapper
.rw_write_enter:
	orl	$RW_WRITE_LOCKED, %edx		/ edx = write-locked value
	xorl	%eax, %eax			/ eax = unheld value
	lock
	cmpxchgl %edx, (%ecx)			/ try to grab write lock
	jnz	rw_enter_sleep
.rw_write_enter_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	jmp	lockstat_enter_wrapper
	SET_SIZE(rw_enter)

	ENTRY(rw_exit)
	movl	4(%esp), %ecx			/ ecx = lock ptr
	movl	(%ecx), %eax			/ eax = old rw_wwwh value
	cmpl	$RW_READ_LOCK, %eax		/ single-reader, no waiters?
	jne	.rw_not_single_reader
	xorl	%edx, %edx			/ edx = new value (unheld)
.rw_read_exit:
	lock
	cmpxchgl %edx, (%ecx)			/ try to drop read lock
	jnz	rw_exit_wakeup
	movl	%gs:CPU_THREAD, %edx		/ edx = thread ptr
	decl	T_KPRI_REQ(%edx)		/ THREAD_KPRI_RELEASE()
.rw_read_exit_lockstat_patch_point:
	ret
	shrl	$RW_HOLD_COUNT_SHIFT, %eax
	incb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat++
	pushl	%edx				/ save thread pointer
	pushl	%edx				/ push owner
	pushl	%eax				/ push refcnt
	pushl	$LS_RW_READER_HOLD		/ push event
	pushl	16(%esp)			/ push caller
	pushl	%ecx				/ push lock
	call	*lockstat_exit
	addl	$20, %esp
	popl	%edx				/ restore thread pointer
	decb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat--
	ret
.rw_not_single_reader:
	testl	$RW_WRITE_LOCKED, %eax		/ write-locked or write-wanted?
	jnz	.rw_write_exit
	leal	-RW_READ_LOCK(%eax), %edx	/ edx = new value
	cmpl	$RW_READ_LOCK, %edx
	jge	.rw_read_exit			/ not last reader, safe to drop
	jmp	rw_exit_wakeup			/ last reader with waiters
.rw_write_exit:
	movl	%gs:CPU_THREAD, %eax		/ eax = thread ptr
	xorl	%edx, %edx			/ edx = new value (unheld)
	orl	$RW_WRITE_LOCKED, %eax		/ eax = write-locked value
	lock
	cmpxchgl %edx, (%ecx)			/ try to drop read lock
	jnz	rw_exit_wakeup
.rw_write_exit_lockstat_patch_point:
	ret
	movl	%gs:CPU_THREAD, %edx		/ edx = thread addr
	incb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat++
	pushl	%edx				/ save thread pointer
	pushl	%edx				/ push owner
	pushl	$1				/ push refcnt
	pushl	$LS_RW_WRITER_HOLD		/ push event
	pushl	16(%esp)			/ push caller
	pushl	%ecx				/ push lock
	call	*lockstat_exit
	addl	$20, %esp
	popl	%edx				/ restore thread pointer
	decb	T_LOCKSTAT(%edx)		/ curthread->t_lockstat--
	ret
	SET_SIZE(rw_exit)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
lockstat_hot_patch(void)
{}

#else

#define	RET	0xc3
#define	NOP	0x90

#define	HOT_PATCH(addr, event, mask, active_instr, normal_instr)	\
	movl	$normal_instr, %ecx;		\
	movl	$active_instr, %edx;		\
	movl	$lockstat_event, %eax;		\
	testb	$mask, event(%eax);		\
	jz	. + 4;				\
	movl	%edx, %ecx;			\
	pushl	$1;				\
	pushl	%ecx;				\
	pushl	$addr;				\
	call	hot_patch_kernel_text;		\
	addl	$12, %esp;

	ENTRY(lockstat_hot_patch)
	HOT_PATCH(.mutex_enter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.mutex_tryenter_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.mutex_exit_lockstat_patch_point,
		LS_ADAPTIVE_MUTEX_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.rw_write_enter_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.rw_read_enter_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.rw_write_exit_lockstat_patch_point,
		LS_RW_WRITER_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.rw_read_exit_lockstat_patch_point,
		LS_RW_READER_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.lock_set_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.lock_try_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.lock_clear_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.disp_lock_enter_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.lock_set_spl_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_ENTER, NOP, RET)
	HOT_PATCH(.disp_lock_exit_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.disp_lock_exit_nopreempt_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT, NOP, RET)
	HOT_PATCH(.lock_clear_splx_lockstat_patch_point,
		LS_SPIN_LOCK_HOLD, LSE_EXIT, NOP, RET)
	ret
	SET_SIZE(lockstat_hot_patch)

#endif	/* lint */

#if defined(lint) || defined(__lint)

void
membar_enter(void)
{}

void
membar_exit(void)
{}

void
membar_producer(void)
{}

void
membar_consumer(void)
{}

#else	/* lint */

	ENTRY(membar_enter)
	ALTENTRY(membar_exit)
	ALTENTRY(membar_producer)
	ALTENTRY(membar_consumer)
	lock
	xorl	$0, (%esp)		/ flush the write buffer
	ret
	SET_SIZE(membar_consumer)
	SET_SIZE(membar_producer)
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)

#endif	/* lint */

/*
 * thread_onproc()
 * Set thread in onproc state for the specified CPU.
 * Also set the thread lock pointer to the CPU's onproc lock.
 * Since the new lock isn't held, the store ordering is important.
 * If not done in assembler, the compiler could reorder the stores.
 */
#if defined(lint) || defined(__lint)

void
thread_onproc(kthread_id_t t, cpu_t *cp)
{
	t->t_state = TS_ONPROC;
	t->t_lockp = &cp->cpu_thread_lock;
}

#else	/* lint */

	ENTRY(thread_onproc)
	movl	4(%esp), %eax
	movl	8(%esp), %ecx
	addl	$CPU_THREAD_LOCK, %ecx	/ pointer to disp_lock while running
	movl	$ONPROC_THREAD, T_STATE(%eax)	/ set state to TS_ONPROC
	movl	%ecx, T_LOCKP(%eax)	/ store new lock pointer
	ret
	SET_SIZE(thread_onproc)

#endif	/* lint */

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
hat_mlist_enter(struct page *pp)
{}

#else	/* lint */

	.globl	hat_page_lock

	.text

	.globl	i486mmu_mlist_exit
	.globl	hat_page_lock
	.globl	hat_mlist_enter
	.globl	hat_mlist_tryenter
	.globl	hat_mlist_exit

	/ The following bits are used in pp->p_inuse

	.globl	hat_mlist_enter
	/ bit 0 inuse bit
	/ bit 1 wanted bit
	/ bit 2 if set indicates an hme has to be dropped of pp->p_mapping
hat_mlist_enter:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
pp_inuse_maybe_cleared:
	lock
	btsw	$P_INUSE, (%ecx)
	jc	pp_inuse_set
	ret

pp_inuse_set:
	lock
	btsw	$P_WANTED, (%ecx)	/ set the wanted bit

	pushl	$hat_page_lock
	call	mutex_enter
	addl	$0x04, %esp

	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	btw	$P_INUSE, (%ecx)
	jc	pp_inuse_stillset	/ make sure lock bit is still set

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	hat_mlist_enter

pp_inuse_stillset:
	btw	$P_WANTED, (%ecx)	/ make sure wanted is still set
					/ mlist_exit could have cleared it and
					/ another guy could have set inuse
	jc	hat_mlist_enter_sleep

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	hat_mlist_enter

hat_mlist_enter_sleep:
	movl	4(%esp), %eax
	pushl	$hat_page_lock
	leal	PP_CV(%eax), %eax
	pushl	%eax
	call	cv_wait
	addl	$0x08, %esp

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	jmp	hat_mlist_enter	

#endif	/* lint */

	

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
hat_mlist_exit(struct page *pp)
{}

#else	/* lint */

	.globl	hat_mlist_exit
hat_mlist_exit:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	lock
	btrw	$P_HMEUNLOAD,(%ecx)		/ test for hmeunload bit
	jc	hme_unload_set		/ if not equal its set call c function
	movb	(%ecx), %al
	andb	$-1![P_INUSE_VALUE|P_WANTED_VALUE|P_HMEUNLOAD_VALUE], %al	
	movb	%al, %dl
	orb	$P_INUSE_VALUE, %al
	lock
	cmpxchgb %dl, (%ecx)
	jne	pp_wanted_set		/ if equal we cleared p_inuse
	ret
hme_unload_set:
	movl	4(%esp), %eax
	pushl	%eax
	call 	i486mmu_mlist_exit	/ c function to drop hme from
					/ pp->p_mapping list
	addl	$0x04, %esp
	jmp	hat_mlist_exit
	
pp_wanted_set:
	andb	$P_HMEUNLOAD_VALUE, %al
	jne	hat_mlist_exit
	
	pushl	$hat_page_lock	/ wanted was set, we need to get 
					/ i86mmu_lock before we could signal
	call	mutex_enter
	addl	$0x04, %esp

	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	movb	(%ecx), %al
	andb	$-1![P_INUSE_VALUE|P_WANTED_VALUE|P_HMEUNLOAD_VALUE], %al	
	movb	%al, %dl
	orb	$P_INUSE_WANTED_VALUE, %al	
	lock
	cmpxchgb %dl, (%ecx)
	je	hat_mlist_exit_wakeup 
	pushl	$hat_page_lock	/ We could not clear wanted and inuse
					/ hmeunload bit could have been set
	call	mutex_exit
	addl	$0x04, %esp
	jmp	hat_mlist_exit

hat_mlist_exit_wakeup:
	movl	4(%esp), %eax
	leal	PP_CV(%eax), %eax
	pushl	%eax
	call	cv_broadcast
	addl	$0x04, %esp

	pushl	$hat_page_lock
	call	mutex_exit
	addl	$0x04, %esp
	ret
#endif	/* lint */
	
	
		
#if defined(lint) || defined(__lint)

/* ARGSUSED */
int
hat_mlist_tryenter(struct page *pp)
{ return (0); }

#else	/* lint */

	.globl	hat_mlist_tryenter
hat_mlist_tryenter:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %ecx	/ pp->p_inuse
	lock
	btsw	$P_INUSE, (%ecx)
	jc	tryenter_pp_inuse_set
	movl	$0x01, %eax
	ret

tryenter_pp_inuse_set:
	xorl	%eax, %eax
	ret
#endif	/* lint */
	

#if defined(lint) || defined(__lint)

/* ARGSUSED */
void
set_p_hmeunload(struct page *pp)
{}

#else	/* lint */
	.globl	set_p_hmeunload
set_p_hmeunload:
	movl	4(%esp), %eax
	leal	PP_INUSE(%eax), %eax
	lock
	btsw	$P_HMEUNLOAD, (%eax)
	ret

#endif	/* lint */
