	.ident	"@(#)syslwp.s	1.15	98/01/14 SMI"

	.file "syslwp.s"

/
/  int
/  _lwp_create (uc, flags, lwpidp)
/ 	ucontext_t *uc;
/ 	unsigned long flags;
/ 	lwpid_t *lwpidp;
/
_fwdef_(`_lwp_create'):
	MCOUNT
	movl	$LWP_CREATE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_create,.-_lwp_create

/
/  int
/  _lwp_continue (lwpid)
/ 	lwp_id_t lwpid;
/
_fwdef_(`_lwp_continue'):
	MCOUNT
	movl	$LWP_CONTINUE,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_continue,.-_lwp_continue

/
/  int
/  _lwp_suspend (lwpid)
/  	lwp_id_t lwpid;
/
_fwdef_(`_lwp_suspend'):
	MCOUNT
	movl	$LWP_SUSPEND,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_suspend,.-_lwp_suspend

/
/  int
/  _lwp_suspend2 (lwp_id_t lwpid, int *sysnump)
/
_fwdef_(`_lwp_suspend2'):
	MCOUNT
	movl	$LWP_SUSPEND,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	mov	8(%esp), %ecx
	testl	%ecx, %ecx
	jz	2f
	movl	%edx,(%ecx)
2:
	ret
	.size	_lwp_suspend2,.-_lwp_suspend2

/
/  int
/  _lwp_kill (lwpid,sig)
/ 	lwp_id_t lwpid;
/ 	int sig;
/

_fwdef_(`_lwp_kill'):
	MCOUNT
	movl	$LWP_KILL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_kill,.-_lwp_kill

/
/  lwp_id_t
/  _lwp_self ()
/
_fwdef_(`_lwp_self'):
	MCOUNT
	movl	$LWP_SELF,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	ret
	.size	_lwp_self,.-_lwp_self

/
/  void *
/  _lwp_getprivate()
/
_fwdef_(`_lwp_getprivate'):
	MCOUNT
	xorl	%eax,%eax
	movw	%gs,%ax
	andl	%eax,%eax
	je	1f
	movl	%gs:0,%eax
1:
	ret
	.size	_lwp_getprivate,.-_lwp_getprivate

/
/  void
/  _lwp_setprivate()
/
_fwdef_(`_lwp_setprivate'):
	MCOUNT
	xorl	%eax,%eax
	movw	%gs,%ax
	andl	%eax,%eax
	jne	1f
	_prologue_
	pushl	_esp_(4)
	call	_fref_(__setupgs)
	addl	$4,%esp
	movw	%ax,%gs
	_epilogue_
	ret

1:
	movl	_esp_(4),%eax
	movl	%eax,%gs:0
	xorl	%eax,%eax
	ret
	.size	_lwp_setprivate,.-_lwp_setprivate

/
/  int
/  _lwp_wait (lwpid, departed)
/ 	lwp_id_t lwpid;
/ 	lwpid_t *departed;
/
_fwdef_(`_lwp_wait'):
	MCOUNT
	movl	$LWP_WAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	2f
	cmpb	$ERESTART,%al
	je	1f
	ret
1:
	movb	$EINTR,%al
	ret
2:
	xorl	%eax,%eax
	ret
	.size	_lwp_wait,.-_lwp_wait

/
/  void
/  _lwp_exit ()
/
_fwdef_(`_lwp_exit'):
	MCOUNT
	movl	$LWP_EXIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	/ Not reached
	.size	_lwp_exit,.-_lwp_exit

/
/  int
/  ___lwp_mutex_wakeup (mp)
/ 	mutex_t *mp;
/
.globl	___lwp_mutex_wakeup
.type	___lwp_mutex_wakeup, @function
___lwp_mutex_wakeup:
	MCOUNT
	movl	$LWP_MUTEX_WAKEUP,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	___lwp_mutex_wakeup,.-___lwp_mutex_wakeup

/
/  int
/  ___lwp_mutex_lock (mp)
/ 	mutex_t *mp;
/
.globl	___lwp_mutex_lock
.type	___lwp_mutex_lock, @function
___lwp_mutex_lock:
	MCOUNT
	movl	$LWP_MUTEX_LOCK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	cmpb	$ERESTART,%al
	je	___lwp_mutex_lock
	cmpb	$EINTR,%al
	je	___lwp_mutex_lock
	ret
1:
	xorl	%eax,%eax
	ret
	.size	___lwp_mutex_lock,.-___lwp_mutex_lock

/
/  int
/  ___lwp_cond_wait (cvp,mp,ts)
/  	lwp_cond_t *cvp;
/ 	lwp_mutex_t *mp;
/ 	timestruc_t *ts;
/
.globl	___lwp_cond_wait
.type	___lwp_cond_wait, @function
___lwp_cond_wait:
	MCOUNT
	movl	$LWP_COND_WAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	2f
	cmpb	$ERESTART,%al
	je	1f
	ret
1:
	movb	$EINTR,%al
	ret
2:
	xorl	%eax,%eax
	ret
	.size	___lwp_cond_wait,.-___lwp_cond_wait

/
/  int
/  _lwp_sema_wait (sp);
/ 	lwp_sema_t *sp;
/
_fwdef_(`_lwp_sema_wait'):
	MCOUNT
	movl	$LWP_SEMA_WAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	2f
	cmpb	$ERESTART,%al
	je	1f
	ret
1:
	movb	$EINTR,%al
	ret
2:
	xorl	%eax,%eax
	ret
	.size	_lwp_sema_wait,.-_lwp_sema_wait

/
/  int
/  _lwp_sema_post (sp);
/ 	lwp_sema_t *sp;
/
_fwdef_(`_lwp_sema_post'):
	MCOUNT
	movl	$LWP_SEMA_POST,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	2f
	cmpb	$ERESTART,%al
	je	1f
	ret
1:
	movb	$EINTR,%al
	ret
2:
	ret
	.size	_lwp_sema_post,.-_lwp_sema_post

/
/  int
/  _lwp_sema_trywait(lwp_sema_t *);
/
_fwdef_(`_lwp_sema_trywait'):
	MCOUNT
	movl	$LWP_SEMA_TRYWAIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_sema_trywait,.-_lwp_sema_trywait

/
/  int
/  _lwp_info (infop);
/ 	struct lwpinfo *infop;
/
_fwdef_(`_lwp_info'):
	MCOUNT
	movl	$LWP_INFO,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_info,.-_lwp_info

/ return the pointer to the private data, it follows the
/ private data itself in the %gs segment
/
_fwdef_(`_getpriptr'):
	MCOUNT
	movl	%gs:4,%eax
	ret
	.size	_getpriptr,.-_getpriptr

/
/  int
/  _lwp_sigredirect (lwpid,sig)
/ 	lwp_id_t lwpid;
/ 	int sig;
/

_fwdef_(`_lwp_sigredirect'):
	MCOUNT
	movl	$LWP_SIGREDIRECT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	_lwp_sigredirect,.-_lwp_sigredirect

	.globl	__cerror
/
/  int
/  _lwp_schedctl(flags, upcall_did, addrp)
/ 	unsigned int flags;
/ 	int upcall_did;
/	sc_shared_t **addrp;
/
_fwdef_(`_lwp_schedctl'):
	MCOUNT
	movl	$SCHEDCTL,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	noerror
	_prologue_
_m4_ifdef_(`DSHLIB',
	`pushl	%eax',
	`'
)
	jmp	_fref_(__cerror)
noerror:
	ret
	.size	_lwp_schedctl,.-_lwp_schedctl

/
/  int
/  ___lwp_mutex_unlock (mp)
/ 	mutex_t *mp;
/
.globl	___lwp_mutex_unlock
.type	___lwp_mutex_unlock, @function
___lwp_mutex_unlock:
	MCOUNT
	movl	$LWP_MUTEX_UNLOCK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	___lwp_mutex_unlock,.-___lwp_mutex_unlock

/
/  int
/  ___lwp_mutex_trylock (mp)
/ 	mutex_t *mp;
/
.globl	___lwp_mutex_trylock
.type	___lwp_mutex_trylock, @function
___lwp_mutex_trylock:
	MCOUNT
	movl	$LWP_MUTEX_TRYLOCK,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	___lwp_mutex_trylock,.-___lwp_mutex_trylock

/
/  int
/  ___lwp_mutex_init(mp, type)
/ 	mutex_t *mp;
/	int type;
/
.globl	___lwp_mutex_init
.type	___lwp_mutex_init, @function
___lwp_mutex_init:
	MCOUNT
	movl	$LWP_MUTEX_INIT,%eax
	lcall   $SYSCALL_TRAPNUM,$0
	jae	1f
	ret
1:
	xorl	%eax,%eax
	ret
	.size	___lwp_mutex_init,.-___lwp_mutex_init
