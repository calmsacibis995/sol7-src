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

#pragma ident	"@(#)putwchar.c	1.8	97/12/06 SMI"	/* from JAE2.0 1.0 */

/*LINTLIBRARY*/

/*
 * A subroutine version of the macro putchar
 */

#include "mse_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <widec.h>
#include "libc.h"

#undef putwchar

wint_t
putwchar(wint_t c)
{
	return (putwc(c, stdout));
}

wint_t
__putwchar_xpg5(wint_t c)
{
	return (__putwc_xpg5(c, stdout));
}
