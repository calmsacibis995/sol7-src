/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_MI_H
#define	_INET_MI_H

#pragma ident	"@(#)mi.h	1.31	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef _KERNEL
#ifndef _VARARGS_
#include <sys/varargs.h>
#endif
#endif

#define	MI_COPY_IN		1
#define	MI_COPY_OUT		2
#define	MI_COPY_DIRECTION(mp)	(*(int *)&(mp)->b_cont->b_next)
#define	MI_COPY_COUNT(mp)	(*(int *)&(mp)->b_cont->b_prev)
#define	MI_COPY_CASE(dir, cnt)	(((cnt)<<2)|dir)
#define	MI_COPY_STATE(mp)	MI_COPY_CASE(MI_COPY_DIRECTION(mp), \
					MI_COPY_COUNT(mp))
#ifdef	_KERNEL

#ifndef MPS
/* We cannot find a use for this API, keeping it until further investigation */
extern	int	mi_adjmsg(MBLKP mp, ssize_t len_to_trim);
#endif

#ifndef NATIVE_ALLOC
extern	void *mi_alloc(size_t size, uint_t pri);
#endif

#ifdef NATIVE_ALLOC_KMEM
extern	void *mi_alloc(size_t size, uint_t pri);
extern	void *mi_alloc_sleep(size_t size, uint_t pri);
#endif

extern	queue_t	*mi_allocq(struct streamtab *st);

extern	int	mi_close_comm(void **mi_head, queue_t *q);

extern	void	mi_close_free(IDP ptr);

extern	void	mi_close_unlink(void **mi_head, IDP ptr);

extern	void	mi_copyin(queue_t *q, MBLKP mp, char *uaddr, size_t len);

extern	void	mi_copyout(queue_t *q, MBLKP mp);

extern	MBLKP	mi_copyout_alloc(queue_t *q, MBLKP mp, char *uaddr,
    size_t len);

extern	void	mi_copy_done(queue_t *q, MBLKP mp, int err);

extern	int	mi_copy_state(queue_t *q, MBLKP mp, MBLKP *mpp);

#ifndef NATIVE_ALLOC
extern	void	mi_free(void *ptr);
#endif

#ifdef NATIVE_ALLOC_KMEM
extern	void	mi_free(void *ptr);
#endif

extern	void	mi_detach(void **mi_head, IDP ptr);

extern	int	mi_iprintf(char *fmt, va_list ap, pfi_t putc_func,
			char *cookie);

extern	boolean_t	mi_link_device(queue_t *orig_q, char *name);

/* PRINTFLIKE2 */
extern	int	mi_mpprintf(MBLKP mp, char *fmt, ...);

/* PRINTFLIKE2 */
extern	int	mi_mpprintf_nr(MBLKP mp, char *fmt, ...);

extern	int	mi_mpprintf_putc(char *cookie, int ch);

extern	IDP	mi_first_ptr(void **mi_head);
extern	IDP	mi_next_ptr(void **mi_head, IDP ptr);

extern	int	mi_open_comm(void **mi_head, size_t size,
			queue_t *q, dev_t *devp, int flag, int sflag,
			cred_t *credp);

extern	IDP	mi_open_alloc(size_t size);

extern	int	mi_open_link(void **mi_head, IDP ptr,
			dev_t *devp, int flag, int sflag,
			cred_t *credp);

extern	void	mi_swap(void **mi_head, IDP ptr1, IDP ptr2);

extern	uint8_t *mi_offset_param(mblk_t *mp, size_t offset, size_t len);

extern	uint8_t *mi_offset_paramc(mblk_t *mp, size_t offset, size_t len);

/* PRINTFLIKE1 */
extern	void	mi_panic(char *fmt, ...);

extern	boolean_t	mi_set_sth_hiwat(queue_t *q, size_t size);

extern	boolean_t	mi_set_sth_lowat(queue_t *q, size_t size);

extern	boolean_t	mi_set_sth_maxblk(queue_t *q, ssize_t size);

extern	boolean_t	mi_set_sth_copyopt(queue_t *q, int copyopt);

extern	boolean_t	mi_set_sth_wroff(queue_t *q, size_t size);

/* PRINTFLIKE2 */
extern	int	mi_sprintf(char *buf, char *fmt, ...);

extern	int	mi_sprintf_putc(char *cookie, int ch);

extern	int	mi_strcmp(const char *cp1, const char *cp2);

extern	size_t	mi_strlen(const char *str);

/* PRINTFLIKE4 */
extern	int	mi_strlog(queue_t *q, char level, ushort_t flags,
		    char *fmt, ...);

extern	long	mi_strtol(char *str, char **ptr, int base);

extern	void	mi_timer(queue_t *q, MBLKP mp, clock_t tim);

extern	MBLKP	mi_timer_alloc(size_t size);

extern	void	mi_timer_free(MBLKP mp);

extern	void	mi_timer_move(queue_t *, mblk_t *);

extern	void	mi_timer_start(mblk_t *mp, clock_t tim);

extern	void	mi_timer_stop(mblk_t *);

extern	boolean_t	mi_timer_valid(MBLKP mp);

extern	MBLKP	mi_tpi_conn_con(MBLKP trailer_mp, char *src,
    t_scalar_t src_length, char *opt, t_scalar_t opt_length);

extern	MBLKP	mi_tpi_conn_ind(MBLKP trailer_mp, char *src,
    t_scalar_t src_length, char *opt, t_scalar_t opt_length,
    t_scalar_t seqnum);

extern	MBLKP	mi_tpi_discon_ind(MBLKP trailer_mp, t_scalar_t reason,
    t_scalar_t seqnum);

extern	MBLKP	mi_tpi_err_ack_alloc(MBLKP mp, t_scalar_t tlierr, int unixerr);

extern	MBLKP	mi_tpi_ok_ack_alloc(MBLKP mp);

extern	MBLKP	mi_tpi_ordrel_ind(void);

extern	MBLKP	mi_tpi_trailer_alloc(MBLKP trailer_mp, size_t size,
    t_scalar_t type);

extern	MBLKP	mi_tpi_uderror_ind(char *dest, t_scalar_t dest_length,
    char *opt, t_scalar_t opt_length, t_scalar_t error);

extern	IDP	mi_zalloc(size_t size);
extern	IDP	mi_zalloc_sleep(size_t size);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_MI_H */
