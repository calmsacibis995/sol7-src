/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_USER_H
#define	_SYS_USER_H

#pragma ident	"@(#)user.h	1.53	98/02/18 SMI"

#include <sys/types.h>
#include <sys/signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * struct exdata is visible in and out of the kernel. This is because it
 * is referenced in <sys/core.h> which doesn't have this kind of magic.
 */
struct exdata {
	struct vnode	*vp;
	size_t	ux_tsize;	/* text size */
	size_t	ux_dsize;	/* data size */
	size_t	ux_bsize;	/* bss size */
	size_t	ux_lsize;	/* lib size */
	long	ux_nshlibs;	/* number of shared libs needed */
	short	ux_mach;	/* machine type */
	short	ux_mag;		/* magic number MUST be here */
	off_t	ux_toffset;	/* file offset to raw text */
	off_t	ux_doffset;	/* file offset to raw data */
	off_t	ux_loffset;	/* file offset to lib sctn */
	caddr_t	ux_txtorg;	/* start addr of text in mem */
	caddr_t	ux_datorg;	/* start addr of data in mem */
	caddr_t	ux_entloc;	/* entry location */
};

#ifdef	__cplusplus
}
#endif

#if defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/siginfo.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/auxv.h>
#include <sys/errno.h>
#include <sys/t_lock.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The user structure; one allocated per process.  Contains all the
 * per-process data that doesn't need to be referenced while the
 * process is swapped.
 */

/*
 * User file descriptors are allocate dynamically, in multiples
 * of NFPCHUNK.
 */

#define	NFPCHUNK 24

struct uf_entry {
	struct file *uf_ofile;
	short  uf_pofile;
	short  uf_refcnt;
};
typedef struct uf_entry uf_entry_t;

#define	PSARGSZ		80	/* Space for exec arguments (used by ps(1)) */

#define	PSCOMSIZ	14

#define	SYSMASKLEN	9	/* Number of longs in syscall bit masks */

#define	MAXCOMLEN	16	/* <= MAXNAMLEN, >= sizeof (ac_comm) */

typedef struct {		/* kernel syscall set type */
	long	word[SYSMASKLEN];
} k_sysset_t;

/*
 * __KERN_NAUXV_IMPL is defined as a convenience sizing mechanism
 * for the portions of the kernel that care about aux vectors.
 *
 * Applications that need to know how many aux vectors the kernel
 * supplies should use the proc(4) interface to read /proc/PID/auxv.
 *
 * This value should not be changed in a patch.
 */
#if defined(sparc) || defined(__sparc)
#define	__KERN_NAUXV_IMPL 19
#elif defined(i386) || defined(__i386)
#define	__KERN_NAUXV_IMPL 21
#endif /* sparc || _sparc */

struct execsw;

typedef	struct	user {

	/*
	 * Fields that require no explicit locking
	 */
	int	u_execid;
	long	u_execsz;
	pgcnt_t	u_tsize;		/* text size (pages) */
	pgcnt_t	u_dsize;		/* data size (pages) */
	time_t u_start;
	clock_t	u_ticks;
	kcondvar_t u_cv;		/* user structure's condition var */

	/*
	 * Executable file info.
	 */
	struct exdata	u_exdata;
	auxv_t  u_auxv[__KERN_NAUXV_IMPL]; /* aux vector from exec */
	char	u_psargs[PSARGSZ];	/* arguments from exec */
	char	u_comm[MAXCOMLEN + 1];

	/*
	 * Initial values of argc, argv and envp to main(), plus
	 * the initial address of the aux vector, for /proc
	 */
	int	u_argc;
	int	u_envc;
	int	u_auxvc;
	uintptr_t u_argv;
	uintptr_t u_envp;
	uintptr_t u_auxvp;

	/*
	 * protected by p_lock
	 */
	struct vnode *u_cdir;		/* current directory */
	struct vnode *u_rdir;		/* root directory */
	mode_t	u_cmask;		/* mask for file creation */
	long	u_mem;
	short	u_nshmseg;		/* # shm segments currently attached */
	char	u_acflag;		/* accounting flag */
	char	u_systrap;		/* /proc: any syscall mask bits set? */

	/*
	 * Protected by pidlock
	 */
	k_sysset_t u_entrymask;		/* /proc syscall stop-on-entry mask */
	k_sysset_t u_exitmask;		/* /proc syscall stop-on-exit mask */
	k_sigset_t u_signodefer;	/* signals defered when caught */
	k_sigset_t u_sigonstack;	/* signals taken on alternate stack */
	k_sigset_t u_sigresethand;	/* signals reset when caught */
	k_sigset_t u_sigrestart;	/* signals that restart system calls */
	k_sigset_t u_sigmask[MAXSIG];	/* signals held while in catcher */
	void	(*u_signal[MAXSIG])();	/* Disposition of signals */

	/*
	 * Updates to individual fields in u_rlimit are not atomic and to
	 * ensure a meaningful set of numbers, p_lock is used whenever
	 * the field in u_rlimit is read/modified such as
	 * getrlimit() or setrlimit()
	 */
	struct rlimit64 u_rlimit[RLIM_NLIMITS]; /* resource usage limits */

	/*
	 * Saved rlimits.
	 * Large File Summit API requires to save the limit
	 * across system calls. For now this will happen mostly only for
	 * file size and not for other resources.
	 */
	struct rlimit64	u_saved_rlimit;

	kmutex_t u_flock;		/* lock for u_nofiles and u_flist */
	int u_nofiles;			/* number of open file slots */
	struct uf_entry *u_flist;	/* open file list */
#if defined(i386) || defined(__i386)
	void    (*u_sigreturn)();	/* For SVR532 signal handling cleanup */
#endif	/* defined(i386) || defined(__i386) */
	struct execsw *u_execsw;	/* ptr to exec switch table entry */
} user_t;

#include <sys/proc.h>			/* cannot include before user defined */

#ifdef	_KERNEL
#ifdef	sun
#define	u	(curproc->p_user)	/* user is now part of proc structure */
#endif  /* sun */
/*
 * We define macros to access the current rlimit values from user
 * structure. With large file support we consider RLIM64_INFINITY
 * as a concept and not as a number.
 */

#define	U_CURLIMIT(up, type)	\
	(((up)->u_rlimit[(type)].rlim_cur == RLIM64_INFINITY) ? \
		rlim_infinity_map[(type)] : (up)->u_rlimit[(type)].rlim_cur)
#define	U_MAXLIMIT(up, type)	\
	(((up)->u_rlimit[(type)].rlim_max == RLIM64_INFINITY) ? \
		rlim_infinity_map[(type)] : (up)->u_rlimit[(type)].rlim_max)
#define	UNLIMITED_CUR(up, type)	\
	(((up)->u_rlimit[(type)].rlim_cur == RLIM64_INFINITY) ? 1 : 0)
#define	UNLIMITED_MAX(up, type)	\
	(((up)->u_rlimit[(type)].rlim_max == RLIM64_INFINITY) ? 1 : 0)

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#else	/* defined(_KERNEL) || defined(_KMEMUSER) */

/*
 * Here, we define a fake version of struct user for programs
 * (debuggers) that use ptrace() to read and modify the saved
 * registers directly in the u-area.  ptrace() has been removed
 * from the operating system and now exists as a library function
 * in libc, built on the /proc process filesystem.  The ptrace()
 * library function provides access only to the members of the
 * fake struct user defined here.
 *
 * User-level programs that must know the real contents of struct
 * user will have to define _KMEMUSER before including <sys/user.h>.
 * Such programs also become machine specific. Carefully consider
 * the consequences of your actions.
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	PSARGSZ		80	/* Space for exec arguments (used by ps(1)) */

typedef	struct	user {
	gregset_t	u_reg;		/* user's saved registers */
	greg_t		*u_ar0;		/* address of user's saved R0 */
	char	u_psargs[PSARGSZ];	/* arguments from exec */
	void	(*u_signal[MAXSIG])();	/* Disposition of signals */
	int		u_code;		/* fault code on trap */
	caddr_t		u_addr;		/* fault PC on trap */
} user_t;

#ifdef	__cplusplus
}
#endif

#endif	/* defined(_KERNEL) || defined(_KMEMUSER) */

#endif	/* _SYS_USER_H */
