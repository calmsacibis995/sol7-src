/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

	.ident	"@(#)syslwp.s 1.7 98/01/14"
	.file "syslwp.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(_lwp_create,function)
	ANSI_PRAGMA_WEAK(_lwp_continue,function)
	ANSI_PRAGMA_WEAK(_lwp_suspend,function)
	ANSI_PRAGMA_WEAK(_lwp_suspend2,function)
	ANSI_PRAGMA_WEAK(_lwp_kill,function)
	ANSI_PRAGMA_WEAK(_lwp_self,function)
	ANSI_PRAGMA_WEAK(_lwp_wait,function)
	ANSI_PRAGMA_WEAK(_lwp_exit,function)
	ANSI_PRAGMA_WEAK(_lwp_cond_broadcast,function)
	ANSI_PRAGMA_WEAK(_lwp_cond_signal,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_wait,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_trywait,function)
	ANSI_PRAGMA_WEAK(_lwp_sema_post,function)
	ANSI_PRAGMA_WEAK(_lwp_setprivate,function)
	ANSI_PRAGMA_WEAK(_lwp_getprivate,function)
	ANSI_PRAGMA_WEAK(_lwp_info,function)
	ANSI_PRAGMA_WEAK(_lwp_schedctl,function)
	ANSI_PRAGMA_WEAK(_lwp_sigredirect,function)

#include "SYS.h"

/*
 * int
 * _lwp_create (uc, flags, lwpidp)
 *	ucontext_t *uc;
 *	unsigned long flags;
 *	lwpid_t *lwpidp;
 */
	ENTRY(_lwp_create)
	SYSTRAP(lwp_create)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_create)

/*
 * int
 * _lwp_continue (lwpid)
 *	lwp_id_t lwpid;
 */
	ENTRY(_lwp_continue)
	SYSTRAP(lwp_continue)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_continue)
/*
 * int
 * _lwp_suspend (lwpid)
 * 	lwp_id_t lwpid;
 */
	ENTRY(_lwp_suspend)
	SYSTRAP(lwp_suspend)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_suspend)

/*
 * int
 * _lwp_suspend2(lwp_id_t lwp, int *sysnump)
 */
	ENTRY(_lwp_suspend2)
	stn	%o1, [%sp + SAVE_OFFSET]	! save address of sysnump
	SYSTRAP(lwp_suspend)
	bcc,a	2f
	clr	%o0
	cmp	%o0, ERESTART
	move	%icc, EINTR, %o0
	RET
2:	ldn	[%sp + SAVE_OFFSET], %o2	! retrieve address of sysnump
	brnz,a,pt %o2, 3f			! verify addr is non-zero
	st	%o1, [%o2]			! set sysnump to rval2
3:	RET
	SET_SIZE(_lwp_suspend2)

/*
 * int
 * _lwp_kill(lwp_id_t lwpid, int sig)
 */

	ENTRY(_lwp_kill);
	SYSTRAP(lwp_kill)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_kill)
/*
 * lwp_id_t
 * _lwp_self ()
 */
	ENTRY(_lwp_self)
	SYSTRAP(lwp_self)
	SYSCERROR
	RET
	SET_SIZE(_lwp_self)

/*
 * void *
 * _lwp_getprivate()
 */
	ENTRY(_lwp_getprivate)
	SYSTRAP(lwp_getprivate)
	SYSCERROR
	RET
	SET_SIZE(_lwp_getprivate)

/*
 * void
 * _lwp_setprivate()
 */
	ENTRY(_lwp_setprivate)
	SYSTRAP(lwp_setprivate)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_setprivate)

/*
 * int
 * _lwp_wait(lwp_id_t lwpid, lwpid_t *departed)
 */
	ENTRY(_lwp_wait)
	SYSTRAP(lwp_wait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_wait)

/*
 * void
 * _lwp_exit()
 */
	ENTRY(_lwp_exit)
	SYSTRAP(lwp_exit)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_exit)

/*
 * int
 * ___lwp_mutex_lock(mutex_t *mp)
 */
	SYSREENTRY(___lwp_mutex_lock)
	SYSTRAP(lwp_mutex_lock)
	SYSINTR_RESTART(.restart____lwp_mutex_lock)
	RET
	SET_SIZE(___lwp_mutex_lock)

/*
 * int
 * ___lwp_mutex_wakeup(mutex_t *mp)
 */
 	ENTRY(___lwp_mutex_wakeup)
 	SYSTRAP(lwp_mutex_wakeup)
 	SYSLWPERR
 	RET
 	SET_SIZE(___lwp_mutex_wakeup)

/*
 * int
 * _lwp_cond_broadcast(lwp_cond_t *cvp)
 */
	ENTRY(_lwp_cond_broadcast)
	SYSTRAP(lwp_cond_broadcast)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_cond_broadcast)

/*
 * int
 * ___lwp_cond_wait(lwp_cond_t *cvp, lwp_mutex_t *mp, timestruc_t *ts)
 */
	ENTRY(___lwp_cond_wait)
	SYSTRAP(lwp_cond_wait);
	SYSLWPERR
	RET
	SET_SIZE(___lwp_cond_wait)

/*
 * int
 * _lwp_cond_signal(lwp_cond_t *cvp)
 */
	ENTRY(_lwp_cond_signal)
	SYSTRAP(lwp_cond_signal)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_cond_signal)

/*
 * int
 * _lwp_sema_wait(lwp_sema_t *sp)
 */
	SYSREENTRY(_lwp_sema_wait)
	SYSTRAP(lwp_sema_wait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_wait)

/*
 * int
 * _lwp_sema_trywait(lwp_sema_t *sp)
 */
	ENTRY(_lwp_sema_trywait)
	SYSTRAP(lwp_sema_trywait)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_trywait)

/*
 * int
 * _lwp_sema_post(lwp_sema_t *sp)
 */
	ENTRY(_lwp_sema_post)
	SYSTRAP(lwp_sema_post)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sema_post)

/*
 * int
 * _lwp_info(struct lwpinfo *infop)
 */
	ENTRY(_lwp_info)
	SYSTRAP(lwp_info)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_info)

/*
 * int
 * _lwp_schedctl (flags, upcall_did, addrp)
 *	unsigned long flags; 
 *	int upcall_did;
 *	sc_shared_t **addrp;
 */
	ENTRY(_lwp_schedctl)
	SYSTRAP(schedctl)
	SYSCERROR
	RET
	SET_SIZE(_lwp_schedctl)

/*
 * int
 * _lwp_sigredirect (lwpid, sig);
 *	int lwpid;
 *	int sig;
 */
	ENTRY(_lwp_sigredirect)
	SYSTRAP(lwp_sigredirect)
	SYSLWPERR
	RET
	SET_SIZE(_lwp_sigredirect)

/*
 * int
 * ___lwp_mutex_trylock (mp)
 *	mutex_t *mp;
 */
	ENTRY(___lwp_mutex_trylock)
	SYSTRAP(lwp_mutex_trylock)
 	SYSLWPERR
	RET
	SET_SIZE(___lwp_mutex_trylock)

/*
 * int
 * ___lwp_mutex_unlock (mp)
 *	mutex_t *mp;
 */
 	ENTRY(___lwp_mutex_unlock)
 	SYSTRAP(lwp_mutex_unlock)
 	SYSLWPERR
 	RET
 	SET_SIZE(___lwp_mutex_unlock)
/*
 * int
 * ___lwp_mutex_init(mp, type)
 *	mutex_t *mp;
 *	int type;
 */

	ENTRY(___lwp_mutex_init)
	SYSTRAP(lwp_mutex_init)
	SYSLWPERR
	RET
	SET_SIZE(___lwp_mutex_init)
