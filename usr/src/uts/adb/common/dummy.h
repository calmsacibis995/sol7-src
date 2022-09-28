/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */
#pragma ident   "@(#)dummy.h 1.1     98/01/09 SMI"

#ifndef _DUMMY_H
#define _DUMMY_H

/*
 * Dummy structure for macrogen to determine whether it's operating in
 * LP64 mode or ILP32 mode.
 *
 * No need for any filler after int_val, 'cos we're not concerned about
 * the structure size
 */
struct __dummy {
	long	long_val;
	char	*ptr_val;
	int	int_val;
};

#endif	/* _DUMMY_H */
