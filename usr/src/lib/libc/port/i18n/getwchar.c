/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1986 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	This module is created for NLS on Sep.03.86		*/

#pragma ident	"@(#)getwchar.c	1.8	97/12/06 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * A subroutine version of the macro getwchar.
 */

#include "mse_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <widec.h>
#include "libc.h"

#undef getwchar

wint_t
getwchar(void)
{
	return (getwc(stdin));
}

wint_t
__getwchar_xpg5(void)
{
	return (__getwc_xpg5(stdin));
}
