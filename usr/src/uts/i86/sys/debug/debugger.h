/*
 * Copyright (c) 1986-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Global declarations used for all kernel debuggers.
 */

#ifndef _SYS_DEBUG_DEBUGGER_H
#define	_SYS_DEBUG_DEBUGGER_H

#pragma ident	"@(#)debugger.h	1.4	98/02/01 SMI"

#include <setjmp.h>
#include <sys/debug/debug.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	DEBUGNAMESZ	50	/* size of debugger's name receptacle */
#define	LINEBUFSZ	128	/* size of input buffer */

func_t ktrace;			/* kernel's trace routine */
func_t monnmi;			/* monitor's nmi routine */
int nobrk;			/* flag used to control page allocation */
int dotrace;			/* ptrace says to single step */
int dorun;			/* ptrace says to run */
int foundu;			/* found valid u symbol table entry */
int lastpm;			/* last pmeg stolen */
int lastpg;			/* last page stolen */
int pagesused;			/* total number of pages used by debugger */
int scbstop;			/* stop when scbsync routine is called */
char myname[DEBUGNAMESZ];	/* name of the debugger */
char aline[LINEBUFSZ];		/* generic buffer used for console input */
struct regs *reg;		/* pointer to debuggee's saved registers */
jmp_buf debugregs;		/* context for debugger */
jmp_buf mainregs;		/* context for debuggee */

/*
 * Because of the way typedef's work, we cannot declare abort_jmp
 * to be jmp_buf * and do reasonable things with them.  So we
 * declare another typedef which hides this.
 */
typedef long *jmp_buf_ptr;

jmp_buf_ptr abort_jmp;		/* pointer to saved context for tty interrupt */
jmp_buf_ptr nofault;		/* pointer to saved context for fault traps */
jmp_buf_ptr curregs;		/* pointer to saved context for each process */

/*
 * Standard function declarations
 */
int trap();
int fault();
func_t readfile();

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_DEBUG_DEBUGGER_H */
