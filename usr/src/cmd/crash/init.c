/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *	Copyright (c) 1991, 1996 by Sun Microsystems, Inc
 *	All rights reserved.
 *
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)init.c	1.14	97/09/07 SMI"

/*
 * This file contains code for the crash initialization.
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/var.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include "crash.h"

#define	VSIZE 160		/* version string length */

static char	version[VSIZE];		/* version strings */

kvm_t *kd;

static unsigned nproc;
uintptr_t procv;
void *_userlimit;
void *_kernelbase;

/* initialize buffers, symbols, and global variables for crash session */
void
init(void)
{
	Sym *ts_symb;
	struct stat mem_buf, file_buf;

	if ((mem = open(dumpfile, 0)) < 0)	/* open dump file */
		fatal("cannot open dump file %s\n", dumpfile);
	/*
	 * Set a flag if the dumpfile is of an active system.
	 */
	if (stat("/dev/mem", &mem_buf) == 0 && stat(dumpfile, &file_buf) == 0 &&
	    S_ISCHR(mem_buf.st_mode) && S_ISCHR(file_buf.st_mode) &&
	    mem_buf.st_rdev == file_buf.st_rdev) {
		active = 1;
		dumpfile = NULL;
	}
	if ((kd = kvm_open(namelist, dumpfile, NULL, O_RDONLY, "crash"))
	    == NULL)
		fatal("cannot open kvm - dump file %s\n", dumpfile);

	rdsymtab(); /* open and read the symbol table */

	/* check version */
	ts_symb = symsrch("version");
	if (ts_symb && (kvm_read(kd, ts_symb->st_value, version, VSIZE))
	    != VSIZE)
		fatal("could not process dumpfile with supplied namelist %s\n",
							namelist);

	if (!(V = symsrch("v")))
		fatal("var structure not found in symbol table\n");
	if (!(Start = symsrch("_start")))
		fatal("start not found in symbol table\n");
	if (!(symsrch("proc_init")))
		fatal("proc not found in symbol table\n");
	readsym("proc_init", &procv, sizeof (procv));
	readsym("nproc", &nproc, sizeof (int));
	readsym("_userlimit", &_userlimit, sizeof (_userlimit));
	readsym("_kernelbase", &_kernelbase, sizeof (_kernelbase));
	if (!(Panic = symsrch("panicstr")))
		fatal("panicstr not found in symbol table\n");

	readmem((void *)V->st_value, 1, -1, (char *)&vbuf,
		sizeof (vbuf), "var structure");

	Curthread = getcurthread();
	Procslot = getcurproc();

	/* setup break signal handling */
	if (signal(SIGINT, sigint) == SIG_IGN)
		signal(SIGINT, SIG_IGN);
}
