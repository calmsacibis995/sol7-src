/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)genconst.c	1.1	97/12/22 SMI"

/*
 * genconst generates datamodel-independent constants.  Such constants
 * are enum values and constants defined via preprocessor macros.
 * Under no circumstances should this program generate structure size
 * or structure member offset information, those belong in offsets.in.
 */

#ifndef	_GENASSYM
#define	_GENASSYM
#endif

#include <libthread.h>
#include <signal.h>
#include <sys/psw.h>

/*
 * Proactively discourage anyone from referring to structures or
 * member offsets in this program.
 */
#define	struct	struct...
#define	OFFSET	OFFSET...

int
main(int argc, char *argv[])
{
	exit(0);
}
