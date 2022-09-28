/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)empty.c	1.1	96/11/01 SMI"

/*
 * Well, some ranlibs, ar's or compilers react funny
 * if asked to do nothing but build empty valid files
 * I would have preferred to a no or at least a static
 * symbol here...
 */
char * _____empty__ = "empty .o file";
