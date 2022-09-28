/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nd.c	1.13	97/12/17 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <inet/common.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <inet/mi.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <inet/nd.h>

/* Free the table pointed to by 'ndp' */
void
nd_free(nd_pparam)
	caddr_t	*nd_pparam;
{
	ND	*nd;

	if ((nd = (ND *)ALIGN32(*nd_pparam)) != NULL) {
		if (nd->nd_tbl)
			mi_free((char *)nd->nd_tbl);
		mi_free((char *)nd);
		*nd_pparam = nil(caddr_t);
	}
}

int
nd_getset(q, nd_param, mp)
	queue_t	*q;
	caddr_t	nd_param;
	MBLKP	mp;
{
	int	err;
	IOCP	iocp;
	MBLKP	mp1;
	ND	*nd;
	NDE	*nde;
	char	*valp;
	long	avail;

	if (!nd_param)
		return (false);
	nd = (ND *)ALIGN32(nd_param);
	iocp = (IOCP)ALIGN32(mp->b_rptr);
	if (iocp->ioc_count == 0 || !(mp1 = mp->b_cont)) {
		mp->b_datap->db_type = M_IOCACK;
		iocp->ioc_count = 0;
		iocp->ioc_error = EINVAL;
		return (true);
	}
	/*
	 * NOTE - logic throughout nd_xxx assumes single data block for ioctl.
	 *	However, existing code sends in some big buffers.
	 */
	avail = iocp->ioc_count;
	if (mp1->b_cont) {
		freemsg(mp1->b_cont);
		mp1->b_cont = nil(MBLKP);
	}

	mp1->b_datap->db_lim[-1] = '\0';	/* Force null termination */
	valp = (char *)mp1->b_rptr;
	for (nde = nd->nd_tbl; ; nde++) {
		if (!nde->nde_name)
			return (false);
		if (mi_strcmp(nde->nde_name, valp) == 0)
			break;
	}
	err = EINVAL;
	while (*valp++)
		noop;
	if (!*valp || valp >= (char *)mp1->b_wptr)
		valp = nilp(char);
	switch (iocp->ioc_cmd) {
	case ND_GET:
		/*
		 * (temporary) hack: "*valp" is size of user buffer for
		 * copyout. If result of action routine is too big, free
		 * excess and return ioc_rval as buffer size needed.  Return
		 * as many mblocks as will fit, free the rest.  For backward
		 * compatibility, assume size of original ioctl buffer if
		 * "*valp" bad or not given.
		 */
		if (valp)
			avail = mi_strtol(valp, (char **)0, 10);
		/* We overwrite the name/value with the reply data */
		{
			mblk_t *mp2 = mp1;

			while (mp2) {
				mp2->b_wptr = mp2->b_rptr;
				mp2 = mp2->b_cont;
			}
		}
		err = (*nde->nde_get_pfi)(q, mp1, nde->nde_data);
		if (!err) {
			int	size_out;
			int	excess;

			iocp->ioc_rval = 0;

			/* Tack on the null */
			(void) mi_mpprintf_putc((char *)mp1, '\0');
			size_out = msgdsize(mp1);
			excess = size_out - avail;
			if (excess > 0) {
				iocp->ioc_rval = size_out;
				size_out -= excess;
				(void) adjmsg(mp1, -(excess + 1));
				(void) mi_mpprintf_putc((char *)mp1, '\0');
			}
			iocp->ioc_count = size_out;
		}
		break;

	case ND_SET:
		if (valp) {
		    if ((err = drv_priv(iocp->ioc_cr)) == 0)
			err = (*nde->nde_set_pfi)(q, mp1, valp, nde->nde_data);
		    iocp->ioc_count = 0;
		    freemsg(mp1);
		    mp->b_cont = nil(MBLKP);
		}
		break;

	default:
		break;
	}
	iocp->ioc_error = err;
	mp->b_datap->db_type = M_IOCACK;
	return (true);
}

/* ARGSUSED */
int
nd_get_default(q, mp, data)
	queue_t	*q;
	MBLKP	mp;
	caddr_t	data;
{
	return (EACCES);
}

/*
 * This routine may be used as the get dispatch routine in nd tables
 * for long variables.  To use this routine instead of a module
 * specific routine, call nd_load as
 *	nd_load(&nd_ptr, "name", nd_get_long, set_pfi, &long_variable)
 * The name of the variable followed by a space and the value of the
 * variable will be printed in response to a get_status call.
 */
/* ARGSUSED */
int
nd_get_long(q, mp, data)
	queue_t	*q;
	MBLKP	mp;
	caddr_t	data;
{
	ulong	*lp;

	lp = (ulong *)ALIGN32(data);
	(void) mi_mpprintf(mp, "%ld", *lp);
	return (0);
}

/* ARGSUSED */
int
nd_get_names(q, mp, nd_param)
	queue_t	*q;
	MBLKP	mp;
	caddr_t	nd_param;
{
	ND	*nd;
	NDE	*nde;
	char	*rwtag;
	boolean_t	get_ok, set_ok;

	nd = (ND *)ALIGN32(nd_param);
	if (!nd)
		return (ENOENT);
	for (nde = nd->nd_tbl; nde->nde_name; nde++) {
		get_ok = nde->nde_get_pfi != nd_get_default;
		set_ok = nde->nde_set_pfi != nd_set_default;
		if (get_ok) {
			if (set_ok)
				rwtag = "read and write";
			else
				rwtag = "read only";
		} else if (set_ok)
			rwtag = "write only";
		else
			rwtag = "no read or write";
		(void) mi_mpprintf(mp, "%s (%s)", nde->nde_name, rwtag);
	}
	return (0);
}

/*
 * Load 'name' into the named dispatch table pointed to by 'ndp'.
 * 'ndp' should be the address of a char pointer cell.  If the table
 * does not exist (*ndp == 0), a new table is allocated and 'ndp'
 * is stuffed.  If there is not enough space in the table for a new
 * entry, more space is allocated.
 */
boolean_t
nd_load(nd_pparam, name, get_pfi, set_pfi, data)
	caddr_t	*nd_pparam;
	char	*name;
	pfi_t	get_pfi;
	pfi_t	set_pfi;
	caddr_t	data;
{
	ND	*nd;
	NDE	*nde;

	if (!nd_pparam)
		return (false);
	if ((nd = (ND *)ALIGN32(*nd_pparam)) == NULL) {
		if ((nd = (ND *)ALIGN32(mi_alloc(sizeof (ND), BPRI_MED)))
		    == NULL)
			return (false);
		bzero((caddr_t)nd, sizeof (ND));
		*nd_pparam = (caddr_t)nd;
	}
	if (nd->nd_tbl) {
		for (nde = nd->nd_tbl; nde->nde_name; nde++) {
			if (mi_strcmp(name, nde->nde_name) == 0)
				goto fill_it;
		}
	}
	if (nd->nd_free_count <= 1) {
		if ((nde = (NDE *)ALIGN32(mi_alloc(nd->nd_size +
					NDE_ALLOC_SIZE, BPRI_MED))) == NULL)
			return (false);
		bzero((char *)nde, nd->nd_size + NDE_ALLOC_SIZE);
		nd->nd_free_count += NDE_ALLOC_COUNT;
		if (nd->nd_tbl) {
			bcopy((char *)nd->nd_tbl, (char *)nde, nd->nd_size);
			mi_free((char *)nd->nd_tbl);
		} else {
			nd->nd_free_count--;
			nde->nde_name = "?";
			nde->nde_get_pfi = nd_get_names;
			nde->nde_set_pfi = nd_set_default;
		}
		nde->nde_data = (caddr_t)nd;
		nd->nd_tbl = nde;
		nd->nd_size += NDE_ALLOC_SIZE;
	}
	for (nde = nd->nd_tbl; nde->nde_name; nde++)
		noop;
	nd->nd_free_count--;
fill_it:
	nde->nde_name = name;
	nde->nde_get_pfi = get_pfi ? get_pfi : nd_get_default;
	nde->nde_set_pfi = set_pfi ? set_pfi : nd_set_default;
	nde->nde_data = data;
	return (true);
}

/* ARGSUSED */
int
nd_set_default(q, mp, value, data)
	queue_t	*q;
	MBLKP	mp;
	char	*value;
	caddr_t	data;
{
	return (EACCES);
}

/* ARGSUSED */
int
nd_set_long(q, mp, value, data)
	queue_t	*q;
	MBLKP	mp;
	char	*value;
	caddr_t	data;
{
	char	*end;
	ulong	*lp;
	long	new_value;

	new_value = mi_strtol(value, &end, 10);
	if (end == value)
		return (EINVAL);
	lp = (ulong *)ALIGN32(data);
	*lp = new_value;
	return (0);
}
