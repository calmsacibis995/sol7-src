/*
 * Copyright (c) 1994-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)_Qp_qtoux.c	1.1	97/08/26 SMI"

#include "quad.h"

/*
 * _Qp_qtoux(x) returns (unsigned long)*x.
 */
unsigned long
_Qp_qtoux(const union longdouble *x)
{
	union longdouble	z;
	unsigned long		i, round;
	unsigned int		xm, fsr;

	xm = x->l.msw & 0x7fffffff;

	__quad_getfsrp(&fsr);

	/* handle nan, inf, and out-of-range cases */
	if (x->l.msw >= 0x403e0000) {
		if (x->l.msw < 0x403f0000) {
			i = 0x8000000000000000ul |
			    ((long) (xm & 0xffff) << 47) |
			    ((long) x->l.frac2 << 15) | (x->l.frac3 >> 17);
			if ((x->l.frac3 & 0x1ffff) | x->l.frac4) {
				/* signal inexact */
				if (fsr & FSR_NXM) {
					/* z = x - 2^63 */
					if (xm & 0xffff ||
					    x->l.frac2 & 0xffff0000) {
						z.l.msw = xm & 0xffff;
						z.l.frac2 = x->l.frac2;
						z.l.frac3 = x->l.frac3;
						z.l.frac4 = x->l.frac4;
					} else if (x->l.frac2 & 0xffff ||
					    x->l.frac3 & 0xffff0000) {
						z.l.msw = x->l.frac2;
						z.l.frac2 = x->l.frac3;
						z.l.frac3 = x->l.frac4;
						z.l.frac4 = 0;
					} else if (x->l.frac3 & 0xffff ||
					    x->l.frac4 & 0xffff0000) {
						z.l.msw = x->l.frac3;
						z.l.frac2 = x->l.frac4;
						z.l.frac3 = z.l.frac4 = 0;
					} else {
						z.l.msw = x->l.frac4;
						z.l.frac2 = z.l.frac3 =
						    z.l.frac4 = 0;
					}
					xm = 0x403e;
					while ((z.l.msw & 0x10000) == 0) {
						z.l.msw = (z.l.msw << 1) |
						    (z.l.frac2 >> 31);
						z.l.frac2 = (z.l.frac2 << 1) |
						    (z.l.frac3 >> 31);
						z.l.frac3 = (z.l.frac3 << 1) |
						    (z.l.frac4 >> 31);
						z.l.frac4 <<= 1;
						xm--;
					}
					z.l.msw |= (xm << 16);
					__quad_fqtox(&z, (long *)&i);
					i |= 0x8000000000000000ul;
				} else {
					fsr = (fsr & ~FSR_CEXC) | FSR_NXA |
					    FSR_NXC;
					__quad_setfsrp(&fsr);
				}
			}
			return (i);
		}
		i = 0x7fffffffffffffffl;
		if (fsr & FSR_NVM) {
			__quad_fqtox(x, (long *)&i);
		} else {
			fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
			__quad_setfsrp(&fsr);
		}
		return (i);
	}
	if (x->l.msw < 0x3fff0000) {
		if (QUAD_ISZERO(*x))
			return (0);
		if (!(x->l.msw & 0x80000000)) {
			/* small positive number, signal inexact */
			i = 0;
			if (fsr & FSR_NXM) {
				__quad_fqtox(x, (long *)&i);
			} else {
				fsr = (fsr & ~FSR_CEXC) | FSR_NXA | FSR_NXC;
				__quad_setfsrp(&fsr);
			}
			return (i);
		}
		/* out of range, signal invalid */
		i = 0x8000000000000000ul;
		if (fsr & FSR_NVM) {
			if (xm < 0x7fff0000) {
				z.l.msw = 0xfffeffff;
				z.l.frac2 = z.l.frac3 = z.l.frac4 = 0xffffffff;
			} else {
				z = *x;
			}
			__quad_fqtox(&z, (long *)&i);
		} else {
			fsr = (fsr & ~FSR_CEXC) | FSR_NVA | FSR_NVC;
			__quad_setfsrp(&fsr);
		}
		return (i);
	}

	/* now x is in the common range of unsigned and int */
	i = 0x4000000000000000l | ((long) (xm & 0xffff) << 46) |
	    ((long) x->l.frac2 << 14) | (x->l.frac3 >> 18);
	round = i & ((1l << (0x403d - (xm >> 16))) - 1);
	i >>= (0x403d - (xm >> 16));
	if (round | (x->l.frac3 & 0x3ffff) | x->l.frac4) {
		/* signal inexact */
		if (fsr & FSR_NXM) {
			__quad_fqtox(x, (long *)&i);
		} else {
			fsr = (fsr & ~FSR_CEXC) | FSR_NXA | FSR_NXC;
			__quad_setfsrp(&fsr);
		}
	}
	return (i);
}
