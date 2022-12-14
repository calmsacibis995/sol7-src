/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * $Source: /mit/kerberos/src/include/RCS/lsb_addr_comp.h,v $
 * $Author: jtkohl $
 * $Header: lsb_addr_comp.h,v 4.0 89/01/23 15:44:46 jtkohl Exp $
 *
 * Copyright 1988 by the Massachusetts Institute of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 *
 * Comparison macros to emulate LSBFIRST comparison results of network
 * byte-order quantities
 */

#ifndef	_KERBEROS_LSB_ADDR_COMP_H
#define	_KERBEROS_LSB_ADDR_COMP_H

#pragma ident	"@(#)lsb_addr_comp.h	1.5	98/01/06 SMI"

#include <kerberos/mit-copyright.h>
#include <kerberos/osconf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef LSBFIRST
#define	lsb_net_ulong_less(x, y)	((x < y) ? -1 : ((x > y) ? 1 : 0))
#define	lsb_net_ushort_less(x, y)	((x < y) ? -1 : ((x > y) ? 1 : 0))
#else
/* MSBFIRST */
#define	u_char_comp(x, y) \
	(((x) > (y)) ? (1) : (((x) == (y)) ? (0) : (-1)))
/* This is gross, but... */
#define	lsb_net_ulong_less(x, y) long_less_than((uchar_t *)&x, (uchar_t *)&y)
#define	lsb_net_ushort_less(x, y) short_less_than((uchar_t *)&x, (uchar_t *)&y)

#define	long_less_than(x, y) \
	(u_char_comp((x)[3], (y)[3]) ? u_char_comp((x)[3], (y)[3]) : \
	    (u_char_comp((x)[2], (y)[2]) ? u_char_comp((x)[2], (y)[2]) : \
	    (u_char_comp((x)[1], (y)[1]) ? u_char_comp((x)[1], (y)[1]) : \
	    (u_char_comp((x)[0], (y)[0])))))
#define	short_less_than(x, y) \
	(u_char_comp((x)[1], (y)[1]) ? u_char_comp((x)[1], (y)[1]) : \
	    (u_char_comp((x)[0], (y)[0])))

#endif /* LSBFIRST */

#ifdef	__cplusplus
}
#endif

#endif	/* _KERBEROS_LSB_ADDR_COMP_H */
