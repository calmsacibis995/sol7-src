/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)lib_strbuf.c	1.1	96/11/01 SMI"

/*
 * lib_strbuf - library string storage
 */

#include "lib_strbuf.h"

/*
 * Storage declarations
 */
char lib_stringbuf[LIB_NUMBUFS][LIB_BUFLENGTH];
int lib_nextbuf;


/*
 * initialization routine.  Might be needed if the code is ROMized.
 */
void
init_lib()
{
	lib_nextbuf = 0;
}
