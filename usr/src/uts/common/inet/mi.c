/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)mi.c	1.52	97/12/06 SMI"

#include <sys/types.h>
#include <inet/common.h>
#undef MAX	/* Defined in sysmacros.h */
#undef MIN
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/sysmacros.h>
#include <inet/nd.h>
#include <inet/mi.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/strlog.h>
#include <sys/ddi.h>
#include <sys/suntpi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>	/* For ASSERT */

#define	ISDIGIT(ch)	((ch) >= '0' && (ch) <= '9')
#define	ISUPPER(ch)	((ch) >= 'A' && (ch) <= 'Z')
#define	tolower(ch)	('a' + ((ch) - 'A'))

#define	MI_IS_TRANSPARENT(mp)	(mp->b_cont && \
	(mp->b_cont->b_rptr != mp->b_cont->b_wptr))

/*
 * Double linked list of type MI_O with a mi_head_t as the head.
 * Used for mi_open_comm etc.
 */

typedef struct mi_o_s {
	struct mi_o_s	*mi_o_next;
	struct mi_o_s	*mi_o_prev;
	dev_t		mi_o_dev;
} MI_O, *MI_OP;

/*
 * List head for MI_O doubly linked list.
 * The list consist of two parts: first a list of driver instances sorted
 * by increasing minor numbers then an unsorted list of module instances
 * and detached instances.
 *
 * The insert pointer is where driver instance are inserted in the list.
 * Module and detached instances are always inserted at the tail of the list.
 *
 * The range of free minor numbers is used for O(1)  minor number assignment.
 * A scan of the complete list is performed when this range runs
 * out thereby reestablishing the largest unused range of minor numbers.
 *
 * The module_dev is used to give almost unique numbers to module instances.
 * This is only needed for mi_strlog which uses the mi_o_dev field when
 * logging messages.
 */

typedef struct mi_head_s {
	struct mi_o_s	mh_o;	/* Contains head of doubly linked list */
	minor_t	mh_min_minor;	/* Smallest free minor number */
	minor_t	mh_max_minor;	/* Largest free minor number */
	MI_OP	mh_insert;	/* Insertion point for alloc */
	int	mh_module_dev;	/* Wraparound number for use when MODOPEN */
} mi_head_t;

#ifdef	_KERNEL
typedef	struct stroptions *STROPTP;
typedef union T_primitives *TPRIMP;

/* Timer block states. */
#define	TB_RUNNING	1
#define	TB_IDLE		2
/*
 * Could not stop/free before putq
 */
#define	TB_RESCHED	3	/* mtb_time_left contains tick count */
#define	TB_CANCELLED	4
#define	TB_TO_BE_FREED	5


typedef struct mtb_s {
	int		mtb_state;
	timeout_id_t	mtb_tid;
	queue_t		*mtb_q;
	MBLKP		mtb_mp;
	clock_t		mtb_time_left;
} MTB, *MTBP;

static int	mi_timer_fire(MTBP mtb);
static	void mi_tpi_addr_and_opt(MBLKP mp, char *addr, t_scalar_t addr_length,
    char *opt, t_scalar_t opt_length);

/* Maximum minor number to use */
static minor_t mi_maxminor = MAXMIN;

#ifndef MPS

/* We cannot find a use of this API, wrapping it until further investigation */
int
mi_adjmsg(MBLKP mp, ssize_t len_to_trim)
{
	return (adjmsg(mp, len_to_trim));
}
#endif


#ifndef NATIVE_ALLOC
/* ARGSUSED1 */
void *
mi_alloc(size_t size, uint pri)
{
	MBLKP	mp;

	if ((mp = allocb(size + sizeof (MBLKP), pri)) != NULL) {
		((MBLKP *)mp->b_rptr)[0] = mp;
		mp->b_rptr += sizeof (MBLKP);
		mp->b_wptr = mp->b_rptr + size;
		return (mp->b_rptr);
	}
	return (NULL);
}
#endif

#ifdef NATIVE_ALLOC_KMEM
/* ARGSUSED1 */
void *
mi_alloc(size_t size, uint pri)
{
	size_t *ptr;

	size += sizeof (size);
	if (ptr = kmem_alloc(size, KM_NOSLEEP)) {
		*ptr = size;
		return (ptr + 1);
	}
	return (NULL);
}

/* ARGSUSED1 */
void *
mi_alloc_sleep(size_t size, uint pri)
{
	size_t *ptr;

	size += sizeof (size);
	ptr = kmem_alloc(size, KM_SLEEP);
	*ptr = size;
	return (ptr + 1);
}
#endif


int mi_rescan_debug = 0;

int
mi_close_comm(void **mi_headp, queue_t *q)
{
	IDP ptr;

	ptr = q->q_ptr;
	mi_close_unlink(mi_headp, ptr);
	mi_close_free(ptr);
	q->q_ptr = WR(q)->q_ptr = NULL;
	return (0);
}

void
mi_close_unlink(void **mi_headp, IDP ptr)
{
	mi_head_t	*mi_head = *(mi_head_t **)mi_headp;
	MI_OP		mi_o;

	mi_o = (MI_OP)ptr;
	if (!mi_o)
		return;
	mi_o--;

	if (mi_o->mi_o_next == NULL) {
		/* Not in list */
		ASSERT(mi_o->mi_o_prev == NULL);
		return;
	}

	/* If we are the insertion point move it to the next guy */
	if (mi_head->mh_insert == mi_o) {
		if (mi_rescan_debug)
			printf("mi_close_com: moving insert\n");
		mi_head->mh_insert = mi_o->mi_o_next;
	}

	/*
	 * If we are either edge of the current range update the current
	 * range.
	 */
	/* LINTED: warning: statement has no consequent: if */
	if (mi_o->mi_o_dev == OPENFAIL) {
		/* Already detached */
	} else if (mi_o->mi_o_dev < MAXMIN) {
		if (mi_o->mi_o_dev == mi_head->mh_min_minor - 1) {
			if (mi_o->mi_o_prev == &mi_head->mh_o) {
				/* First one */
				mi_head->mh_min_minor = 0;
			} else {
				mi_head->mh_min_minor =
					mi_o->mi_o_prev->mi_o_dev + 1;
			}
			if (mi_rescan_debug)
				printf(
				    "mi_close_comm: removed min %d, new %d\n",
					(int)mi_o->mi_o_dev,
					mi_head->mh_min_minor);
		}
		if (mi_o->mi_o_dev == mi_head->mh_max_minor) {
			if (mi_o->mi_o_next == &mi_head->mh_o) {
				/* Last one */
				mi_head->mh_max_minor = mi_maxminor;
			} else {
				mi_head->mh_max_minor =
				    mi_o->mi_o_next->mi_o_dev;
			}
			if (mi_rescan_debug)
				printf("mi_close_comm: removed minor %d, "
				    "new %d\n",
				    (int)mi_o->mi_o_dev,
				    mi_head->mh_max_minor);
		}
	}
	/* Unlink from list */
	ASSERT(mi_o->mi_o_next != NULL);
	ASSERT(mi_o->mi_o_prev != NULL);
	ASSERT(mi_o->mi_o_next->mi_o_prev == mi_o);
	ASSERT(mi_o->mi_o_prev->mi_o_next == mi_o);

	mi_o->mi_o_next->mi_o_prev = mi_o->mi_o_prev;
	mi_o->mi_o_prev->mi_o_next = mi_o->mi_o_next;
	mi_o->mi_o_next = mi_o->mi_o_prev = NULL;

	mi_o->mi_o_dev = (dev_t)OPENFAIL;

	if (mi_head->mh_insert != NULL) {
		ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
		    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
	}

	/* If list now empty free the list head */
	if (mi_head->mh_o.mi_o_next == &mi_head->mh_o) {
		ASSERT(mi_head->mh_o.mi_o_prev == &mi_head->mh_o);
		*mi_headp = nilp(void *);
		mi_free((IDP)mi_head);
	}
}

void
mi_close_free(IDP ptr)
{
	MI_OP		mi_o;

	mi_o = (MI_OP)ptr;
	if (!mi_o)
		return;
	mi_o--;

	ASSERT(mi_o->mi_o_next == NULL && mi_o->mi_o_prev == NULL);
	mi_free((IDP)mi_o);
}

/*
 * mi_copyin - takes care of transparent or non-transparent ioctl for the
 * calling function so that they have to deal with just M_IOCDATA type
 * and not worry about M_COPYIN.
 *
 * mi_copyin checks to see if the ioctl is transparent or non transparent.
 * In case of a non_transparent ioctl, it packs the data into a M_IOCDATA
 * message and puts it back onto the current queue for further processing.
 * In case of transparent ioctl, it sends a M_COPYIN message up to the
 * streamhead so that a M_IOCDATA with the information comes back down.
 */
void
mi_copyin(queue_t *q, MBLKP mp, char *uaddr, size_t len)
{
	struct 	iocblk *iocp = (struct iocblk *)mp->b_rptr;
	struct 	copyreq *cq = (struct copyreq *)mp->b_rptr;
	struct 	copyresp *cp = (struct copyresp *)mp->b_rptr;
	int    	err;
	MBLKP	mp1;

	ASSERT(mp->b_datap->db_type == M_IOCTL && !uaddr);

	/* A transparent ioctl. Send a M_COPYIN message to the streamhead. */
	if (iocp->ioc_count == TRANSPARENT) {
		MI_COPY_COUNT(mp) = 1;
		MI_COPY_DIRECTION(mp) = MI_COPY_IN;
		cq->cq_private = mp->b_cont;
		cq->cq_size = len;
		cq->cq_flag = 0;
		bcopy(mp->b_cont->b_rptr, &cq->cq_addr, sizeof (cq->cq_addr));
		mp->b_cont = nil(MBLKP);
		mp->b_datap->db_type = M_COPYIN;
		qreply(q, mp);
		return;
	}

	/*
	 * A non-transparent ioctl. Need to convert into M_IOCDATA message.
	 *
	 * We allocate a 0 byte message block and put its address in
	 * cp_private. It also makes the b_prev field = 1 and b_next
	 * field = MI_COPY_IN for this 0 byte block. This is done to
	 * maintain compatibility with old code in mi_copy_state
	 * (which removes the empty block).
	 */
	if (mp->b_cont == NULL || msgdsize(mp->b_cont) < len) {
		err = EINVAL;
		goto err_ret;
	}

	mp1 = allocb(0, BPRI_MED);
	if (mp1 == NULL || (mp->b_cont->b_cont && !pullupmsg(mp->b_cont, -1))) {
		err = ENOMEM;
		goto err_ret;
	}

	/*
	 * Stuff mp1 in between M_IOCTL and M_DATA block so that
	 * MI_COPY_COUNT & MI_COPY_DIRECTION macros can work properly.
	 * Relink M_DATA block back to M_IOCTL block (take the 0 byte
	 * block off from between) and send the entire thing back as
	 * M_IOCDATA message. The address for the 0 byte block is
	 * still contained in cp_private field.
	 */
	mp1->b_cont = mp->b_cont;
	mp->b_cont = mp1;
	MI_COPY_COUNT(mp) = 1;
	MI_COPY_DIRECTION(mp) = MI_COPY_IN;
	mp->b_cont = mp1->b_cont;
	mp1->b_cont = nil(MBLKP);

	mp->b_datap->db_type = M_IOCDATA;
	cp->cp_private = mp1;
	cp->cp_rval = NULL;
	put(q, mp);
	return;

err_ret:
	iocp->ioc_error = err;
	iocp->ioc_count = 0;
	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = nil(MBLKP);
	}
	mp->b_datap->db_type = M_IOCACK;
	qreply(q, mp);
}

void
mi_copyout(queue_t *q, MBLKP mp)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;
	struct copyreq *cq = (struct copyreq *)iocp;
	struct copyresp *cp = (struct copyresp *)cq;
	MBLKP	mp1;
	MBLKP	mp2;

	if (mp->b_datap->db_type != M_IOCDATA || !mp->b_cont) {
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	/* Check completion of previous copyout operation. */
	mp1 = mp->b_cont;
	if ((int)cp->cp_rval || !mp1->b_cont) {
		mi_copy_done(q, mp, (int)cp->cp_rval);
		return;
	}
	if (!mp1->b_cont->b_cont && !MI_IS_TRANSPARENT(mp)) {
		mp1->b_next = nil(MBLKP);
		mp1->b_prev = nil(MBLKP);
		mp->b_cont = mp1->b_cont;
		freeb(mp1);
		mp1 = mp->b_cont;
		mp1->b_next = nil(MBLKP);
		mp1->b_prev = nil(MBLKP);
		iocp->ioc_count = mp1->b_wptr - mp1->b_rptr;
		iocp->ioc_error = 0;
		mp->b_datap->db_type = M_IOCACK;
		qreply(q, mp);
		return;
	}
	if (MI_COPY_DIRECTION(mp) == MI_COPY_IN) {
		/* Set up for first copyout. */
		MI_COPY_DIRECTION(mp) = MI_COPY_OUT;
		MI_COPY_COUNT(mp) = 1;
	} else {
		++MI_COPY_COUNT(mp);
	}
	cq->cq_private = mp1;
	/* Find message preceding last. */
	for (mp2 = mp1; mp2->b_cont->b_cont; mp2 = mp2->b_cont)
		;
	if (mp2 == mp1)
		bcopy((char *)mp1->b_rptr, (char *)&cq->cq_addr,
		    sizeof (cq->cq_addr));
	else
		cq->cq_addr = (char *)mp2->b_cont->b_next;
	mp1 = mp2->b_cont;
	mp->b_datap->db_type = M_COPYOUT;
	mp->b_cont = mp1;
	mp2->b_cont = nil(MBLKP);
	mp1->b_next = nil(MBLKP);
	cq->cq_size = mp1->b_wptr - mp1->b_rptr;
	cq->cq_flag = 0;
	qreply(q, mp);
}

MBLKP
mi_copyout_alloc(queue_t *q, MBLKP mp, char *uaddr, size_t len)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;
	MBLKP	mp1;

	if (mp->b_datap->db_type == M_IOCTL) {
		if (iocp->ioc_count != TRANSPARENT) {
			mp1 = allocb(0, BPRI_MED);
			if (!mp1) {
				iocp->ioc_error = ENOMEM;
				iocp->ioc_count = 0;
				freemsg(mp->b_cont);
				mp->b_cont = nil(MBLKP);
				mp->b_datap->db_type = M_IOCACK;
				qreply(q, mp);
				return (nil(MBLKP));
			}
			mp1->b_cont = mp->b_cont;
			mp->b_cont = mp1;
		}
		MI_COPY_COUNT(mp) = 0;
		MI_COPY_DIRECTION(mp) = MI_COPY_OUT;
		/* Make sure it looks clean to mi_copyout. */
		mp->b_datap->db_type = M_IOCDATA;
		((struct copyresp *)iocp)->cp_rval = NULL;
	}
	mp1 = allocb(len, BPRI_MED);
	if (!mp1) {
		if (q)
			mi_copy_done(q, mp, ENOMEM);
		return (nil(MBLKP));
	}
	linkb(mp, mp1);
	mp1->b_next = (MBLKP)uaddr;
	return (mp1);
}

void
mi_copy_done(queue_t *q, MBLKP mp, int err)
{
	struct iocblk *iocp;
	MBLKP	mp1;

	if (!mp)
		return;
	if (!q || (mp->b_wptr - mp->b_rptr) < sizeof (struct iocblk)) {
		freemsg(mp);
		return;
	}
	iocp = (struct iocblk *)mp->b_rptr;
	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_error = err;

	iocp->ioc_count = 0;
	if ((mp1 = mp->b_cont) != NULL) {
		for (; mp1; mp1 = mp1->b_cont) {
			mp1->b_prev = nil(MBLKP);
			mp1->b_next = nil(MBLKP);
		}
		freemsg(mp->b_cont);
		mp->b_cont = nil(MBLKP);
	}
	qreply(q, mp);
}

int
mi_copy_state(queue_t *q, MBLKP mp, MBLKP *mpp)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;
	struct copyresp *cp = (struct copyresp *)iocp;
	MBLKP	mp1;

	mp1 = mp->b_cont;
	mp->b_cont = cp->cp_private;
	if (mp1) {
		if (mp1->b_cont && !pullupmsg(mp1, -1)) {
			mi_copy_done(q, mp, ENOMEM);
			return (-1);
		}
		linkb(mp->b_cont, mp1);
	}
	if ((int)cp->cp_rval) {
		mi_copy_done(q, mp, (int)cp->cp_rval);
		return (-1);
	}
	if (mpp && MI_COPY_DIRECTION(mp) == MI_COPY_IN)
		*mpp = mp1;
	return (MI_COPY_STATE(mp));
}

#ifndef NATIVE_ALLOC
void
mi_free(void *ptr)
{
	MBLKP	*mpp;

	if ((mpp = (MBLKP *)ptr) && mpp[-1])
		freeb(mpp[-1]);
}
#endif

#ifdef NATIVE_ALLOC_KMEM
void
mi_free(void *ptr)
{
	size_t	size;

	if (!ptr)
		return;
	if ((size = ((size_t *)ptr)[-1]) <= 0)
		cmn_err(CE_PANIC, "mi_free");

	kmem_free((void *) ((size_t *)ptr - 1), size);
}
#endif

void
mi_detach(void **mi_headp, IDP ptr)
{
	mi_head_t	*mi_head = *(mi_head_t **)mi_headp;
	MI_OP	mi_o = (MI_OP)ptr;
	MI_OP		insert;

	if (mi_o == NULL)
		return;
	mi_o--;

	/* If we are the insertion point move it to the next guy */
	if (mi_head->mh_insert == mi_o) {
		if (mi_rescan_debug)
			printf("mi_detach: moving insert\n");
		mi_head->mh_insert = mi_o->mi_o_next;
	}

	/*
	 * If we are either edge of the current range update the current
	 * range.
	 */
	if (mi_o->mi_o_dev < MAXMIN) {
		if (mi_o->mi_o_dev == mi_head->mh_min_minor - 1) {
			if (mi_o->mi_o_prev == &mi_head->mh_o) {
				/* First one */
				mi_head->mh_min_minor = 0;
			} else {
				mi_head->mh_min_minor =
				    mi_o->mi_o_prev->mi_o_dev + 1;
			}
			if (mi_rescan_debug)
				printf("mi_detach: removed min %d, new %d\n",
				    (int)mi_o->mi_o_dev,
				    mi_head->mh_min_minor);
		}
		if (mi_o->mi_o_dev == mi_head->mh_max_minor) {
			if (mi_o->mi_o_next == &mi_head->mh_o) {
				/* Last one */
				mi_head->mh_max_minor = mi_maxminor;
			} else {
				mi_head->mh_max_minor =
					mi_o->mi_o_next->mi_o_dev;
			}
			if (mi_rescan_debug)
				printf("mi_detach: removed minor %d, new %d\n",
				    (int)mi_o->mi_o_dev,
				    mi_head->mh_max_minor);
		}
	}
	/* Unlink from list */
	ASSERT(mi_o->mi_o_next != NULL);
	ASSERT(mi_o->mi_o_prev != NULL);
	ASSERT(mi_o->mi_o_next->mi_o_prev == mi_o);
	ASSERT(mi_o->mi_o_prev->mi_o_next == mi_o);

	mi_o->mi_o_next->mi_o_prev = mi_o->mi_o_prev;
	mi_o->mi_o_prev->mi_o_next = mi_o->mi_o_next;
	mi_o->mi_o_next = mi_o->mi_o_prev = NULL;
	mi_o->mi_o_dev = (dev_t)OPENFAIL;

	/* Reinsert at end of list */
	insert = &mi_head->mh_o;
	ASSERT(insert->mi_o_prev->mi_o_next == insert);
	mi_o->mi_o_next = insert;
	insert->mi_o_prev->mi_o_next = mi_o;
	mi_o->mi_o_prev = insert->mi_o_prev;
	insert->mi_o_prev = mi_o;

	/*
	 * Make sure that mh_insert is before all the MODOPEN/OPENFAIL
	 * instances.
	 */
	if (mi_head->mh_insert == &mi_head->mh_o)
		mi_head->mh_insert = mi_o;

	if (mi_head->mh_insert != NULL) {
		ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
		    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
	}
}

int
mi_iprintf(char *fmt, va_list ap, pfi_t putc_func, char *cookie)
{
	int	base;
	char	buf[(sizeof (long) * 3) + 1];
	static	char	hex_val[] = "0123456789abcdef";
	int	ch;
	int	count;
	char	*cp1;
	int	digits;
	char	*fcp;
	boolean_t	is_long;
	u_long	uval;
	long	val;
	boolean_t	zero_filled;

	if (!fmt)
		return (-1);
	count = 0;
	while (*fmt) {
		if (*fmt != '%' || *++fmt == '%') {
			count += (*putc_func)(cookie, *fmt++);
			continue;
		}
		if (*fmt == '0') {
			zero_filled = true;
			fmt++;
			if (!*fmt)
				break;
		} else
			zero_filled = false;
		base = 0;
		for (digits = 0; ISDIGIT(*fmt); fmt++) {
			digits *= 10;
			digits += (*fmt - '0');
		}
		if (!*fmt)
			break;
		is_long = false;
		if (*fmt == 'l') {
			is_long = true;
			fmt++;
		}
		if (!*fmt)
			break;
		ch = *fmt++;
		if (ISUPPER(ch)) {
			ch = tolower(ch);
			is_long = true;
		}
		switch (ch) {
		case 'c':
			count += (*putc_func)(cookie, va_arg(ap, int *));
			continue;
		case 'd':
			base = 10;
			break;
		case 'm':	/* Print out memory, 2 hex chars per byte */
			if (is_long)
				fcp = va_arg(ap, char *);
			else {
				if ((cp1 = va_arg(ap, char *)) != NULL)
					fcp = (char *)cp1;
				else
					fcp = nilp(char);
			}
			if (!fcp) {
				for (fcp = (char *)"(NULL)"; *fcp; fcp++)
					count += (*putc_func)(cookie, *fcp);
			} else {
				while (digits--) {
					int u1 = *fcp++ & 0xFF;
					count += (*putc_func)(cookie,
					    hex_val[(u1>>4)& 0xF]);
					count += (*putc_func)(cookie,
					    hex_val[u1& 0xF]);
				}
			}
			continue;
		case 'o':
			base = 8;
			break;
		case 'p':
			is_long = true;
			/* FALLTHRU */
		case 'x':
			base = 16;
			break;
		case 's':
			if (is_long)
				fcp = va_arg(ap, char *);
			else {
				if ((cp1 = va_arg(ap, char *)) != NULL)
					fcp = (char *)cp1;
				else
					fcp = nilp(char);
			}
			if (!fcp)
				fcp = (char *)"(NULL)";
			while (*fcp) {
				count += (*putc_func)(cookie, *fcp++);
				if (digits && --digits == 0)
					break;
			}
			while (digits > 0) {
				count += (*putc_func)(cookie, ' ');
				digits--;
			}
			continue;
		case 'u':
			base = 10;
			break;
		default:
			return (count);
		}
		if (is_long)
			val = va_arg(ap, long);
		else
			val = va_arg(ap, int);
		if (base == 10 && ch != 'u') {
			if (val < 0) {
				count += (*putc_func)(cookie, '-');
				val = -val;
			}
			uval = val;
		} else {
			if (is_long)
				uval = val;
			else
				uval = (u_int)val;
		}
		/* Hand overload/restore the register variable 'fmt' */
		cp1 = fmt;
		fmt = A_END(buf);
		*--fmt = '\0';
		do {
			if (fmt > buf)
				*--fmt = hex_val[uval % base];
			if (digits && --digits == 0)
				break;
		} while (uval /= base);
		if (zero_filled) {
			while (digits > 0 && fmt > buf) {
				*--fmt = '0';
				digits--;
			}
		}
		while (*fmt)
			count += (*putc_func)(cookie, *fmt++);
		fmt = cp1;
	}
	return (count);
}

int
mi_mpprintf(MBLKP mp, char *fmt, ...)
{
	va_list	ap;
	int	count = -1;

	va_start(ap, fmt);
	if (mp) {
		count = mi_iprintf(fmt, ap, (pfi_t)mi_mpprintf_putc,
		    (char *)mp);
		if (count != -1)
			(void) mi_mpprintf_putc((char *)mp, '\0');
	}
	va_end(ap);
	return (count);
}

int
mi_mpprintf_nr(MBLKP mp, char *fmt, ...)
{
	va_list	ap;
	int	count = -1;

	va_start(ap, fmt);
	if (mp) {
		(void) adjmsg(mp, -1);
		count = mi_iprintf(fmt, ap, (pfi_t)mi_mpprintf_putc,
		    (char *)mp);
		if (count != -1)
			(void) mi_mpprintf_putc((char *)mp, '\0');
	}
	va_end(ap);
	return (count);
}

int
mi_mpprintf_putc(char *cookie, int ch)
{
	MBLKP	mp = (MBLKP)cookie;

	while (mp->b_cont)
		mp = mp->b_cont;
	if (mp->b_wptr >= mp->b_datap->db_lim) {
		mp->b_cont = allocb(1024, BPRI_HI);
		mp = mp->b_cont;
		if (!mp)
			return (0);
	}
	*mp->b_wptr++ = (unsigned char)ch;
	return (1);
}

IDP
mi_first_ptr(void **mi_headp)
{
	mi_head_t *mi_head = *(mi_head_t **)mi_headp;
	MI_OP	mi_op;

	mi_op = mi_head->mh_o.mi_o_next;
	if (mi_op && mi_op != &mi_head->mh_o)
		return ((IDP)&mi_op[1]);
	return (nil(IDP));
}

IDP
mi_next_ptr(void **mi_headp, IDP ptr)
{
	mi_head_t *mi_head = *(mi_head_t **)mi_headp;
	MI_OP	mi_op = ((MI_OP)ptr) - 1;

	if ((mi_op = mi_op->mi_o_next) != NULL && mi_op != &mi_head->mh_o)
		return ((IDP)&mi_op[1]);
	return (nil(IDP));
}

static int
mi_dev_rescan(mi_head_t *mi_head)
{
	int	maxrange = 1;
	dev_t	prevdev = 0, nextdev = 0;
	MI_OP	maxinsert = NULL;
	MI_OP	mi_o;

	if (mi_rescan_debug)
		printf("mi_open_comm: exceeded max %d\n",
		    mi_head->mh_max_minor);

	if (mi_head->mh_o.mi_o_next == &mi_head->mh_o) {
		if (mi_rescan_debug)
			printf("mi_open_comm: nothing to scan\n");
		mi_head->mh_min_minor = 0;
		mi_head->mh_max_minor = mi_maxminor;
		mi_head->mh_insert = &mi_head->mh_o;
		ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
		    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
		return (0);
	}
	for (mi_o = mi_head->mh_o.mi_o_next; mi_o != &mi_head->mh_o &&
	    mi_o->mi_o_dev < MAXMIN; mi_o = mi_o->mi_o_next) {
		nextdev = mi_o->mi_o_dev;
		if (nextdev - prevdev > maxrange) {
			if (mi_rescan_debug)
				printf("max: %lu, %lu\n", prevdev, nextdev);
			maxrange = nextdev - prevdev;
			maxinsert = mi_o;
		}
		prevdev = mi_o->mi_o_dev;
	}
	/* Last one */
	nextdev = mi_maxminor;
	if (nextdev - prevdev > maxrange) {
		if (mi_rescan_debug)
			printf("last max: %lu, %lu\n", prevdev, nextdev);
		maxrange = nextdev - prevdev;
		maxinsert = mi_o;
	}
	if (maxinsert == NULL) {
		if (mi_rescan_debug)
			printf("No minors left\n");
		return (EBUSY);
	}
	if (maxinsert == mi_head->mh_o.mi_o_next) {
		/* First */
		if (mi_rescan_debug)
			printf("mi_open_comm: got first\n");
		prevdev = 0;
		nextdev = maxinsert->mi_o_dev;
	} else if (maxinsert == &mi_head->mh_o ||
	    maxinsert->mi_o_dev >= MAXMIN) {
		/* Last */
		if (mi_rescan_debug)
			printf("mi_open_comm: got last\n");
		prevdev = maxinsert->mi_o_prev->mi_o_dev + 1;
		nextdev = mi_maxminor;
	} else {
		prevdev = maxinsert->mi_o_prev->mi_o_dev + 1;
		nextdev = maxinsert->mi_o_dev;
	}
	ASSERT(nextdev - prevdev >= 1);
	if (mi_rescan_debug)
		printf("mi_open_comm: min %lu, max %lu\n", prevdev, nextdev);
	mi_head->mh_min_minor = prevdev;
	mi_head->mh_max_minor = nextdev;
	mi_head->mh_insert = maxinsert;
	ASSERT(mi_head->mh_insert != NULL);
	ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
	    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
	return (0);
}

/*
 * If sflag == CLONEOPEN, search for the lowest number available.
 * If sflag != CLONEOPEN then attempt to open the 'dev' given.
 */
/* ARGSUSED4 */
int
mi_open_comm(void **mi_headp, size_t size, queue_t *q, dev_t *devp,
    int flag, int sflag, cred_t *credp)
{
	int error;
	IDP ptr;

	if (q->q_ptr)
		return (0);

	ptr = mi_open_alloc(size);
	if (ptr == NULL)
		return (ENOMEM);
	q->q_ptr = WR(q)->q_ptr = ptr;
	error = mi_open_link(mi_headp, ptr, devp, flag, sflag, credp);
	if (error != 0) {
		q->q_ptr = WR(q)->q_ptr = NULL;
		mi_close_free(ptr);
	}
	return (error);
}


IDP
mi_open_alloc(size_t size)
{
	MI_OP		mi_o;

	if (size > (MAX_UINT - sizeof (MI_O)))
		return (NULL);

	if ((mi_o = (MI_OP)mi_zalloc_sleep(size + sizeof (MI_O))) == NULL)
		return (NULL);
	mi_o++;
	return ((IDP)mi_o);
}

/*
 * A NULL devp can be used to create a detached instance
 */
/* ARGSUSED3 */
int
mi_open_link(void **mi_headp, IDP ptr, dev_t *devp, int flag,  int sflag,
    cred_t *credp)
{
	mi_head_t	*mi_head = *(mi_head_t **)mi_headp;
	MI_OP		insert;
	MI_OP		mi_o;
	dev_t		dev;

	if (!mi_head) {
		mi_head = (mi_head_t *)mi_zalloc_sleep(sizeof (mi_head_t));
		*mi_headp = (void *)mi_head;
		/* Setup doubly linked list */
		mi_head->mh_o.mi_o_next = &mi_head->mh_o;
		mi_head->mh_o.mi_o_prev = &mi_head->mh_o;
		mi_head->mh_o.mi_o_dev = 0;	/* For asserts only */
		/* mi_dev_rescan will setup the initial values */
	}
	ASSERT(ptr != NULL);
	mi_o = (MI_OP)ptr;
	mi_o--;
	if (sflag == MODOPEN) {
		devp = nilp(dev_t);
		/*
		 * Set device number to MAXMIN + incrementing number
		 * and insert at tail.
		 */
		dev = MAXMIN + ++mi_head->mh_module_dev;
		insert = &mi_head->mh_o;
	} else if (devp == NULL) {
		/* Detached open - insert a tail */
		dev = (dev_t)OPENFAIL;
		insert = &mi_head->mh_o;
	} else {
		if (sflag == CLONEOPEN) {
			if (mi_head->mh_min_minor >= mi_head->mh_max_minor) {
				int error;

				error = mi_dev_rescan(mi_head);
				if (error)
					return (error);
			}
			insert = mi_head->mh_insert;
			dev = mi_head->mh_min_minor++;
		} else {
			dev = geteminor(*devp);
			for (insert = mi_head->mh_o.mi_o_next;
			    insert != &mi_head->mh_o;
			    insert = insert->mi_o_next) {
				if (insert->mi_o_dev > dev) {
					/* found free slot */
					break;
				}
				if (insert->mi_o_dev == dev)
					return (EBUSY);
			}
		}
	}
	/* Insert before "insert" */
	ASSERT(insert->mi_o_prev->mi_o_next == insert);
	mi_o->mi_o_dev = dev;
	mi_o->mi_o_next = insert;
	insert->mi_o_prev->mi_o_next = mi_o;
	mi_o->mi_o_prev = insert->mi_o_prev;
	insert->mi_o_prev = mi_o;

	/*
	 * Make sure that mh_insert is before all the MODOPEN/OPENFAIL
	 * instances.
	 */
	if (dev > MAXMIN && mi_head->mh_insert == &mi_head->mh_o)
		mi_head->mh_insert = mi_o;

	if (mi_head->mh_insert != NULL) {
		ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
		    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
	}

	if (devp)
		*devp = makedevice(getemajor(*devp), (minor_t)dev);
	return (0);
}

/*
 * Swap the information maintained by mi_open* and mi_close* between ptr1
 * and ptr2.
 */
void
mi_swap(void **mi_headp, IDP ptr1, IDP ptr2)
{
	mi_head_t	*mi_head = *(mi_head_t **)mi_headp;
	MI_OP		mi_o1, mi_o2;
	MI_OP		next, prev;
	dev_t		dev;

	ASSERT(mi_head != NULL);
	mi_o1 = (MI_OP)ptr1;
	mi_o2 = (MI_OP)ptr2;
	mi_o1--;
	mi_o2--;

	ASSERT(mi_o1 != mi_o2);

	ASSERT(mi_o1->mi_o_next->mi_o_prev == mi_o1);
	ASSERT(mi_o1->mi_o_prev->mi_o_next == mi_o1);
	ASSERT(mi_o2->mi_o_next->mi_o_prev == mi_o2);
	ASSERT(mi_o2->mi_o_prev->mi_o_next == mi_o2);

	/* Swap the dev fields */
	dev = mi_o1->mi_o_dev;
	mi_o1->mi_o_dev = mi_o2->mi_o_dev;
	mi_o2->mi_o_dev = dev;

	/*
	 * The "neighbor" case has to be handled separately.
	 */
	if (mi_o1->mi_o_next == mi_o2 || mi_o2->mi_o_next == mi_o1) {
		/* Make o1 always be the first of the two */
		if (mi_o2->mi_o_next == mi_o1) {
			MI_OP	tmp;

			tmp = mi_o1;
			mi_o1 = mi_o2;
			mi_o2 = tmp;
		}
		ASSERT(mi_o1->mi_o_next == mi_o2);
		ASSERT(mi_o2->mi_o_prev == mi_o1);
		mi_o1->mi_o_next = mi_o2->mi_o_next;
		mi_o2->mi_o_prev = mi_o1->mi_o_prev;

		mi_o1->mi_o_prev = mi_o2;
		mi_o2->mi_o_next = mi_o1;

		mi_o1->mi_o_next->mi_o_prev = mi_o1;
		mi_o2->mi_o_prev->mi_o_next = mi_o2;
	} else {
		/* Swap the prev and next fields */
		next = mi_o1->mi_o_next;
		prev = mi_o1->mi_o_prev;

		mi_o1->mi_o_next = mi_o2->mi_o_next;
		mi_o1->mi_o_prev = mi_o2->mi_o_prev;

		mi_o2->mi_o_next = next;
		mi_o2->mi_o_prev = prev;

		/* Change pointer points to the mi_o's */
		mi_o1->mi_o_next->mi_o_prev = mi_o1;
		mi_o1->mi_o_prev->mi_o_next = mi_o1;

		mi_o2->mi_o_next->mi_o_prev = mi_o2;
		mi_o2->mi_o_prev->mi_o_next = mi_o2;
	}

	/* Update mh_insert to match dev swap */
	if (mi_head->mh_insert == mi_o1) {
		if (mi_rescan_debug)
			printf("mi_swap: moving insert 1\n");
		mi_head->mh_insert = mi_o2;
	} else if (mi_head->mh_insert == mi_o2) {
		if (mi_rescan_debug)
			printf("mi_swap: moving insert 2\n");
		mi_head->mh_insert = mi_o1;
	}
	if (mi_head->mh_insert != NULL) {
		ASSERT(mi_head->mh_insert->mi_o_dev <= MAXMIN ||
		    mi_head->mh_insert->mi_o_prev->mi_o_dev <= MAXMIN);
	}

	ASSERT(mi_o1->mi_o_next->mi_o_prev == mi_o1);
	ASSERT(mi_o1->mi_o_prev->mi_o_next == mi_o1);
	ASSERT(mi_o2->mi_o_next->mi_o_prev == mi_o2);
	ASSERT(mi_o2->mi_o_prev->mi_o_next == mi_o2);
}


uint8_t *
mi_offset_param(mblk_t *mp, size_t offset, size_t len)
{
	size_t	msg_len;

	if (!mp)
		return (nilp(uint8_t));
	msg_len = mp->b_wptr - mp->b_rptr;
	if (msg_len == 0 || offset > msg_len || len > msg_len ||
	    (offset + len) > msg_len || len == 0)
		return (nilp(uint8_t));
	return (&mp->b_rptr[offset]);
}

uint8_t *
mi_offset_paramc(mblk_t *mp, size_t offset, size_t len)
{
	uint8_t	*param;

	for (; mp; mp = mp->b_cont) {
		int type = mp->b_datap->db_type;
		if (datamsg(type)) {
			if (param = mi_offset_param(mp, offset, len))
				return (param);
			if (offset < mp->b_wptr - mp->b_rptr)
				break;
			offset -= mp->b_wptr - mp->b_rptr;
		}
	}
	return (nilp(uint8_t));
}

void
mi_panic(char *fmt, ...)
{
	va_list	ap;
	char	lbuf[256];
	char	*buf = lbuf;
	va_start(ap, fmt);
	(void) mi_iprintf(fmt, ap, (pfi_t)mi_sprintf_putc, (char *)&buf);
	(void) mi_sprintf_putc((char *)&buf, '\0');
	va_end(ap);
	cmn_err(CE_PANIC, lbuf);
	return;
	/* NOTREACHED */
}

boolean_t
mi_set_sth_hiwat(queue_t *q, size_t size)
{
	MBLKP	mp;
	STROPTP stropt;

	if (!(mp = allocb(sizeof (*stropt), BPRI_LO)))
		return (false);
	mp->b_datap->db_type = M_SETOPTS;
	mp->b_wptr += sizeof (*stropt);
	stropt = (STROPTP)mp->b_rptr;
	stropt->so_flags = SO_HIWAT;
	stropt->so_hiwat = size;
	putnext(q, mp);
	return (true);
}

boolean_t
mi_set_sth_lowat(queue_t *q, size_t size)
{
	MBLKP	mp;
	STROPTP stropt;

	if (!(mp = allocb(sizeof (*stropt), BPRI_LO)))
		return (false);
	mp->b_datap->db_type = M_SETOPTS;
	mp->b_wptr += sizeof (*stropt);
	stropt = (STROPTP)mp->b_rptr;
	stropt->so_flags = SO_LOWAT;
	stropt->so_lowat = size;
	putnext(q, mp);
	return (true);
}

/* ARGSUSED */
boolean_t
mi_set_sth_maxblk(queue_t *q, ssize_t size)
{
	MBLKP	mp;
	STROPTP stropt;

	if (!(mp = allocb(sizeof (*stropt), BPRI_LO)))
		return (false);
	mp->b_datap->db_type = M_SETOPTS;
	mp->b_wptr += sizeof (*stropt);
	stropt = (STROPTP)mp->b_rptr;
	stropt->so_flags = SO_MAXBLK;
	stropt->so_maxblk = size;
	putnext(q, mp);
	return (true);
}

boolean_t
mi_set_sth_copyopt(queue_t *q, int copyopt)
{
	MBLKP	mp;
	STROPTP stropt;

	if (!(mp = allocb(sizeof (*stropt), BPRI_LO)))
		return (false);
	mp->b_datap->db_type = M_SETOPTS;
	mp->b_wptr += sizeof (*stropt);
	stropt = (STROPTP)mp->b_rptr;
	stropt->so_flags = SO_COPYOPT;
	stropt->so_copyopt = (ushort)copyopt;
	putnext(q, mp);
	return (true);
}

boolean_t
mi_set_sth_wroff(queue_t *q, size_t size)
{
	MBLKP	mp;
	STROPTP stropt;

	if (!(mp = allocb(sizeof (*stropt), BPRI_LO)))
		return (false);
	mp->b_datap->db_type = M_SETOPTS;
	mp->b_wptr += sizeof (*stropt);
	stropt = (STROPTP)mp->b_rptr;
	stropt->so_flags = SO_WROFF;
	stropt->so_wroff = (u_short)size;
	putnext(q, mp);
	return (true);
}

int
mi_sprintf(char *buf, char *fmt, ...)
{
	va_list	ap;
	int	count = -1;
	va_start(ap, fmt);
	if (buf) {
		count = mi_iprintf(fmt, ap, (pfi_t)mi_sprintf_putc,
		    (char *)&buf);
		if (count != -1)
			(void) mi_sprintf_putc((char *)&buf, '\0');
	}
	va_end(ap);
	return (count);
}

/* Used to count without writing data */
/* ARGSUSED1 */
static int
mi_sprintf_noop(char *cookie, int ch)
{
	char	**cpp = (char **)cookie;

	(*cpp)++;
	return (1);
}

int
mi_sprintf_putc(char *cookie, int ch)
{
	char	**cpp = (char **)cookie;

	**cpp = (char)ch;
	(*cpp)++;
	return (1);
}

int
mi_strcmp(const char *cp1, const char *cp2)
{
	while (*cp1++ == *cp2++) {
		if (!cp2[-1])
			return (0);
	}
	return ((uint)cp2[-1]  & 0xFF) - ((uint)cp1[-1] & 0xFF);
}

size_t
mi_strlen(const char *str)
{
	const char *cp = str;

	while (*cp != '\0')
		cp++;
	return ((int)(cp - str));
}

int
mi_strlog(queue_t *q, char level, u_short flags, char *fmt, ...)
{
	va_list	ap;
	char	buf[200];
	char	*alloc_buf = buf;
	int	count = -1;
	char	*cp;
	short	mid;
	MI_OP	mi_op;
	int	ret;
	short	sid;

	sid = 0;
	mid = 0;
	if (q) {
		if ((mi_op = (MI_OP)q->q_ptr) != NULL)
			sid = mi_op[-1].mi_o_dev;
		mid = q->q_qinfo->qi_minfo->mi_idnum;
	}

	/* Find out how many bytes we need and allocate if necesary */
	va_start(ap, fmt);
	cp = buf;
	count = mi_iprintf(fmt, ap, mi_sprintf_noop, (char *)&cp);
	if (count > sizeof (buf) &&
	    !(alloc_buf = mi_alloc((uint)count + 2, BPRI_MED))) {
		va_end(ap);
		return (-1);
	}
	va_end(ap);

	va_start(ap, fmt);
	cp = alloc_buf;
	count = mi_iprintf(fmt, ap, mi_sprintf_putc, (char *)&cp);
	if (count != -1)
		(void) mi_sprintf_putc((char *)&cp, '\0');
	else
		alloc_buf[0] = '\0';
	va_end(ap);

	ret = strlog(mid, sid, level, flags, alloc_buf);
	if (alloc_buf != buf)
		mi_free(alloc_buf);
	return (ret);
}

long
mi_strtol(char *str, char **ptr, int base)
{
	char	*cp;
	int	digits, value;
	boolean_t	is_negative;

	cp = str;
	while (*cp == ' ' || *cp == '\t' || *cp == '\n')
		cp++;
	is_negative = (*cp == '-');
	if (is_negative)
		cp++;
	if (base == 0) {
		base = 10;
		if (*cp == '0') {
			base = 8;
			cp++;
			if (*cp == 'x' || *cp == 'X') {
				base = 16;
				cp++;
			}
		}
	}
	value = 0;
	for (; *cp; cp++) {
		if (*cp >= '0' && *cp <= '9')
			digits = *cp - '0';
		else if (*cp >= 'a' && *cp <= 'f')
			digits = *cp - 'a' + 10;
		else if (*cp >= 'A' && *cp <= 'F')
			digits = *cp - 'A' + 10;
		else
			break;
		if (digits >= base)
			break;
		value = (value * base) + digits;
	}
	if (ptr)
		*ptr = cp;
	if (is_negative)
		value = -value;
	return (value);
}

/*
 *		mi_timer mechanism.
 *
 * Each timer is represented by a timer mblk and a (streams) queue. When the
 * timer fires the timer mblk will be put on the associated streams queue
 * so that the streams module can process the timer even in its service
 * procedure.
 *
 * The interface consists of 4 entry points:
 *	mi_timer_alloc		- create a timer mblk
 *	mi_timer_free		- free a timer mblk
 *	mi_timer		- start, restart, stop, or move the
 *				  timer to a different queue
 *	mi_timer_valid		- called by streams module to verify that
 *				  the timer did indeed fire.
 */




/*
 * Start, restart, stop, or move the timer to a new queue.
 * If "tim" is -2 the timer is moved to a different queue.
 * If "tim" is -1 the timer is stopped.
 * Otherwise, the timer is stopped if it is already running, and
 * set to fire tim milliseconds from now.
 */

void
mi_timer(queue_t *q, MBLKP mp, clock_t tim)
{
	MTBP	mtb;
	int	state;

	if (!q || !mp || (mp->b_rptr - mp->b_datap->db_base) != sizeof (MTB))
		return;
	mtb = (MTBP)mp->b_datap->db_base;
	ASSERT(mp->b_datap->db_type == M_PCSIG);
	if (tim >= 0) {
		mtb->mtb_q = q;
		state = mtb->mtb_state;
		tim = MSEC_TO_TICK(tim);
		if (state == TB_RUNNING) {
			if (untimeout(mtb->mtb_tid) < 0) {
				/* Message has already been putq */
				ASSERT(mtb->mtb_q->q_first == mp ||
				    mp->b_prev || mp->b_next);
				mtb->mtb_state = TB_RESCHED;
				mtb->mtb_time_left = tim;
				/* mi_timer_valid will start timer */
				return;
			}
		} else if (state != TB_IDLE) {
			ASSERT(state != TB_TO_BE_FREED);
			if (state == TB_CANCELLED) {
				ASSERT(mtb->mtb_q->q_first == mp ||
				    mp->b_prev || mp->b_next);
				mtb->mtb_state = TB_RESCHED;
				mtb->mtb_time_left = tim;
				/* mi_timer_valid will start timer */
				return;
			}
			if (state == TB_RESCHED) {
				ASSERT(mtb->mtb_q->q_first == mp ||
				    mp->b_prev || mp->b_next);
				mtb->mtb_time_left = tim;
				/* mi_timer_valid will start timer */
				return;
			}
		}
		mtb->mtb_state = TB_RUNNING;
		mtb->mtb_tid = timeout((pfv_t)mi_timer_fire, mtb, tim);
		return;
	}
	switch (tim) {
	case -1:
		mi_timer_stop(mp);
		break;
	case -2:
		mi_timer_move(q, mp);
		break;
	default:
		(void) mi_panic("mi_timer: bad tim value: %ld", tim);
		break;
	}
}

/*
 * Allocate an M_PCSIG timer message. The space between db_base and
 * b_rptr is used by the mi_timer mechanism, and after b_rptr there are
 * "size" bytes that the caller can use for its own purposes.
 *
 * Note that db_type has to be a priority message since otherwise
 * the putq will not cause the service procedure to run when
 * there is flow control.
 */
MBLKP
mi_timer_alloc(size_t size)
{
	MBLKP	mp;
	MTBP	mtb;

	if ((mp = allocb(size + sizeof (MTB), BPRI_HI)) != NULL) {
		mp->b_datap->db_type = M_PCSIG;
		mtb = (MTBP)mp->b_datap->db_base;
		mp->b_rptr = (u_char *)&mtb[1];
		mp->b_wptr = mp->b_rptr + size;
		mtb->mtb_state = TB_IDLE;
		mtb->mtb_mp = mp;
		mtb->mtb_q = nil(queue_t *);
		return (mp);
	}
	return (nil(MBLKP));
}

/*
 * timeout() callback function.
 * Put the message on the current queue.
 * If the timer is stopped or moved to a different queue after
 * it has fired then mi_timer() and mi_timer_valid() will clean
 * things up.
 */
static int
mi_timer_fire(MTBP mtb)
{
	ASSERT(mtb == (MTBP)mtb->mtb_mp->b_datap->db_base);
	ASSERT(mtb->mtb_mp->b_datap->db_type == M_PCSIG);
	return (putq(mtb->mtb_q, mtb->mtb_mp));
}

/*
 * Logically free a timer mblk (that might have a pending timeout().)
 * If the timer has fired and the mblk has been put on the queue then
 * mi_timer_valid will free the mblk.
 */

void
mi_timer_free(MBLKP mp)
{
	MTBP	mtb;
	int	state;

	if (!mp	|| (mp->b_rptr - mp->b_datap->db_base) != sizeof (MTB))
		return;
	mtb = (MTBP)mp->b_datap->db_base;
	state = mtb->mtb_state;
	if (state == TB_RUNNING) {
		if (untimeout(mtb->mtb_tid) < 0) {
			/* Message has already been putq */
			ASSERT(mtb->mtb_q->q_first == mp ||
			    mp->b_prev || mp->b_next);
			mtb->mtb_state = TB_TO_BE_FREED;
			/* mi_timer_valid will free the mblk */
			return;
		}
	} else if (state != TB_IDLE) {
		/* Message has already been putq */
		ASSERT(mtb->mtb_q->q_first == mp ||
		    mp->b_prev || mp->b_next);
		ASSERT(state != TB_TO_BE_FREED);
		mtb->mtb_state = TB_TO_BE_FREED;
		/* mi_timer_valid will free the mblk */
		return;
	}
	ASSERT(mtb->mtb_q ==  NULL || mtb->mtb_q->q_first != mp);
	freemsg(mp);
}

/*
 * Called from mi_timer(,,-2)
 */
void
mi_timer_move(queue_t *q, MBLKP mp)
{
	MTBP	mtb;
	clock_t	tim;

	if (!q || !mp || (mp->b_rptr - mp->b_datap->db_base) != sizeof (MTB))
		return;

	mtb = (MTBP)mp->b_datap->db_base;
	/*
	 * Need to untimeout and restart to make
	 * sure that the mblk is not about to be putq on the old queue
	 * by mi_timer_fire.
	 */
	if (mtb->mtb_state == TB_RUNNING) {
		if ((tim = untimeout(mtb->mtb_tid)) < 0) {
			/*
			 * Message has already been putq. Move from old queue
			 * to new queue.
			 */
			ASSERT(mtb->mtb_q->q_first == mp ||
			    mp->b_prev || mp->b_next);
			rmvq(mtb->mtb_q, mp);
			ASSERT(mtb->mtb_q->q_first != mp &&
			    mp->b_prev == NULL && mp->b_next == NULL);
			mtb->mtb_q = q;
			(void) putq(mtb->mtb_q, mp);
			return;
		}
		mtb->mtb_q = q;
		mtb->mtb_state = TB_RUNNING;
		mtb->mtb_tid = timeout((pfv_t)mi_timer_fire, mtb, tim);
	} else if (mtb->mtb_state != TB_IDLE) {
		ASSERT(mtb->mtb_state != TB_TO_BE_FREED);
		/*
		 * Message is already sitting on queue. Move to new queue.
		 */
		ASSERT(mtb->mtb_q->q_first == mp ||
		    mp->b_prev || mp->b_next);
		rmvq(mtb->mtb_q, mp);
		ASSERT(mtb->mtb_q->q_first != mp &&
		    mp->b_prev == NULL && mp->b_next == NULL);
		mtb->mtb_q = q;
		(void) putq(mtb->mtb_q, mp);
	} else
		mtb->mtb_q = q;
}

/*
 * Called from mi_timer(,,-1)
 */
void
mi_timer_stop(MBLKP mp)
{
	MTBP	mtb;
	int	state;

	if (!mp || (mp->b_rptr - mp->b_datap->db_base) != sizeof (MTB))
		return;

	mtb = (MTBP)mp->b_datap->db_base;
	state = mtb->mtb_state;
	if (state == TB_RUNNING) {
		if (untimeout(mtb->mtb_tid) < 0) {
			/* Message has already been putq */
			ASSERT(mtb->mtb_q->q_first == mp ||
			    mp->b_prev || mp->b_next);
			mtb->mtb_state = TB_CANCELLED;
		} else {
			mtb->mtb_state = TB_IDLE;
		}
	} else if (state == TB_RESCHED) {
		ASSERT(mtb->mtb_q->q_first == mp ||
		    mp->b_prev || mp->b_next);
		mtb->mtb_state = TB_CANCELLED;
	}
}

/*
 * The user of the mi_timer mechanism is required to call mi_timer_valid() for
 * each M_PCSIG message processed in the service procedures.
 * mi_timer_valid will return "true" if the timer actually did fire.
 */

boolean_t
mi_timer_valid(MBLKP mp)
{
	MTBP	mtb;
	int	state;

	if (!mp	|| (mp->b_rptr - mp->b_datap->db_base) != sizeof (MTB) ||
	    mp->b_datap->db_type != M_PCSIG)
		return (false);
	mtb = (MTBP)mp->b_datap->db_base;
	state = mtb->mtb_state;
	if (state != TB_RUNNING) {
		ASSERT(state != TB_IDLE);
		if (state == TB_TO_BE_FREED) {
			/*
			 * mi_timer_free was called after the message
			 * was putq'ed.
			 */
			freemsg(mp);
			return (false);
		}
		if (state == TB_CANCELLED) {
			/* The timer was stopped after the mblk was putq'ed */
			mtb->mtb_state = TB_IDLE;
			return (false);
		}
		if (state == TB_RESCHED) {
			/*
			 * The timer was stopped and then restarted after
			 * the mblk was putq'ed.
			 * mtb_time_left contains the number of ticks that
			 * the timer was restarted with.
			 */
			mtb->mtb_state = TB_RUNNING;
			mtb->mtb_tid = timeout((pfv_t)mi_timer_fire,
			    mtb, mtb->mtb_time_left);
			return (false);
		}
	}
	mtb->mtb_state = TB_IDLE;
	return (true);
}

static void
mi_tpi_addr_and_opt(MBLKP mp, char *addr, t_scalar_t addr_length,
    char *opt, t_scalar_t opt_length)
{
	struct T_unitdata_ind	*tudi;

	/*
	 * This code is used more than just for unitdata ind
	 * (also for T_CONN_IND and T_CONN_CON) and
	 * relies on correct functioning on the happy
	 * coincidence that the the address and option buffers
	 * represented by length/offset in all these primitives
	 * are isomorphic in terms of offset from start of data
	 * structure
	 */
	tudi = (struct T_unitdata_ind *)mp->b_rptr;
	tudi->SRC_offset = (t_scalar_t)(mp->b_wptr - mp->b_rptr);
	tudi->SRC_length = addr_length;
	if (addr_length > 0) {
		bcopy(addr, (char *)mp->b_wptr, addr_length);
		mp->b_wptr += addr_length;
	}
	tudi->OPT_offset = (t_scalar_t)(mp->b_wptr - mp->b_rptr);
	tudi->OPT_length = opt_length;
	if (opt_length > 0) {
		bcopy(opt, (char *)mp->b_wptr, opt_length);
		mp->b_wptr += opt_length;
	}
}

MBLKP
mi_tpi_conn_con(MBLKP trailer_mp, char *src, t_scalar_t src_length, char *opt,
    t_scalar_t opt_length)
{
	size_t	len;
	MBLKP	mp;

	len = sizeof (struct T_conn_con) + src_length + opt_length;
	if ((mp = mi_tpi_trailer_alloc(trailer_mp, len, T_CONN_CON)) != NULL) {
		mp->b_wptr = &mp->b_rptr[sizeof (struct T_conn_con)];
		mi_tpi_addr_and_opt(mp, src, src_length, opt, opt_length);
	}
	return (mp);
}

MBLKP
mi_tpi_conn_ind(MBLKP trailer_mp, char *src, t_scalar_t src_length, char *opt,
    t_scalar_t opt_length, t_scalar_t seqnum)
{
	size_t	len;
	MBLKP	mp;

	len = sizeof (struct T_conn_ind) + src_length + opt_length;
	if ((mp = mi_tpi_trailer_alloc(trailer_mp, len, T_CONN_IND)) != NULL) {
		mp->b_wptr = &mp->b_rptr[sizeof (struct T_conn_ind)];
		mi_tpi_addr_and_opt(mp, src, src_length, opt, opt_length);
		((struct T_conn_ind *)mp->b_rptr)->SEQ_number = seqnum;
		mp->b_datap->db_type = M_PROTO;
	}
	return (mp);
}

MBLKP
mi_tpi_discon_ind(MBLKP trailer_mp, t_scalar_t reason, t_scalar_t seqnum)
{
	MBLKP	mp;
	struct T_discon_ind	*tdi;

	if ((mp = mi_tpi_trailer_alloc(trailer_mp,
	    sizeof (struct T_discon_ind), T_DISCON_IND)) != NULL) {
		tdi = (struct T_discon_ind *)mp->b_rptr;
		tdi->DISCON_reason = reason;
		tdi->SEQ_number = seqnum;
	}
	return (mp);
}

/*
 * Allocate and fill in a TPI err ack packet using the 'mp' passed in
 * for the 'error_prim' context as well as sacrifice.
 */
MBLKP
mi_tpi_err_ack_alloc(MBLKP mp, t_scalar_t tlierr, int unixerr)
{
	struct T_error_ack	*teackp;
	t_scalar_t error_prim;

	if (!mp)
		return (nil(MBLKP));
	error_prim = ((TPRIMP)mp->b_rptr)->type;
	if ((mp = tpi_ack_alloc(mp, sizeof (struct T_error_ack),
	    M_PCPROTO, T_ERROR_ACK)) != NULL) {
		teackp = (struct T_error_ack *)mp->b_rptr;
		teackp->ERROR_prim = error_prim;
		teackp->TLI_error = tlierr;
		teackp->UNIX_error = unixerr;
	}
	return (mp);
}

MBLKP
mi_tpi_ok_ack_alloc(MBLKP mp)
{
	t_scalar_t correct_prim;

	if (!mp)
		return (nil(MBLKP));
	correct_prim = ((TPRIMP)mp->b_rptr)->type;
	if ((mp = tpi_ack_alloc(mp, sizeof (struct T_ok_ack),
	    M_PCPROTO, T_OK_ACK)) != NULL)
		((struct T_ok_ack *)mp->b_rptr)->CORRECT_prim = correct_prim;
	return (mp);
}

MBLKP
mi_tpi_ordrel_ind(void)
{
	MBLKP	mp;

	if ((mp = allocb(sizeof (struct T_ordrel_ind), BPRI_HI)) != NULL) {
		mp->b_datap->db_type = M_PROTO;
		((struct T_ordrel_ind *)mp->b_rptr)->PRIM_type = T_ORDREL_IND;
		mp->b_wptr += sizeof (struct T_ordrel_ind);
	}
	return (mp);
}

MBLKP
mi_tpi_trailer_alloc(MBLKP trailer_mp, size_t size, t_scalar_t type)
{
	MBLKP	mp;

	if ((mp = allocb(size, BPRI_MED)) != NULL) {
		mp->b_cont = trailer_mp;
		mp->b_datap->db_type = M_PROTO;
		((union T_primitives *)mp->b_rptr)->type = type;
		mp->b_wptr += size;
	}
	return (mp);
}

MBLKP
mi_tpi_uderror_ind(char *dest, t_scalar_t dest_length, char *opt,
    t_scalar_t opt_length, t_scalar_t error)
{
	size_t	len;
	MBLKP	mp;
	struct T_uderror_ind	*tudei;

	len = sizeof (struct T_uderror_ind) + dest_length + opt_length;
	if ((mp = allocb(len, BPRI_HI)) != NULL) {
		mp->b_datap->db_type = M_PROTO;
		tudei = (struct T_uderror_ind *)mp->b_rptr;
		tudei->PRIM_type = T_UDERROR_IND;
		tudei->ERROR_type = error;
		mp->b_wptr = &mp->b_rptr[sizeof (struct T_uderror_ind)];
		mi_tpi_addr_and_opt(mp, dest, dest_length, opt, opt_length);
	}
	return (mp);
}

IDP
mi_zalloc(size_t size)
{
	IDP	ptr;

	if (ptr = mi_alloc(size, BPRI_LO))
		bzero(ptr, size);
	return (ptr);
}

IDP
mi_zalloc_sleep(size_t size)
{
	IDP	ptr;

	if (ptr = mi_alloc_sleep(size, BPRI_LO))
		bzero(ptr, size);
	return (ptr);
}

#endif	/* _KERNEL */
