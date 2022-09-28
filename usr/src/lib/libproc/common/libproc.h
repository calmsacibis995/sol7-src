/*
 * Copyright (c) 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Interfaces available from the process control library, libproc.
 *
 * libproc provides process control functions for the /proc tools
 * (commands in /usr/proc/bin), /usr/bin/truss, and /usr/bin/gcore.
 * libproc is a private support library for these commands only.
 * It is _not_ a public interface, although it might become one
 * in the fullness of time, when the interfaces settle down.
 *
 * In the meantime, be aware that any program linked with libproc in this
 * release of Solaris is almost guaranteed to break in the next release.
 *
 * In short, do not use this header file or libproc for any purpose.
 */

#ifndef	_LIBPROC_H
#define	_LIBPROC_H

#pragma ident	"@(#)libproc.h	1.2	98/01/29 SMI"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlist.h>
#include <door.h>
#include <gelf.h>
#include <proc_service.h>
#include <rtld_db.h>
#include <procfs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/auxv.h>
#include <sys/resource.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Opaque structure tag reference to a process control structure.
 * Clients of libproc cannot look inside the process control structure.
 * The implementation of struct ps_prochandle can change w/o affecting clients.
 */
struct ps_prochandle;

extern	int	_libproc_debug;	/* set non-zero to enable debugging fprintfs */

#if defined(sparc) || defined(__sparc)
#define	R_RVAL1	R_O0		/* register holding a function return value */
#define	R_RVAL2	R_O1		/* 32 more bits for a 64-bit return value */
#define	SYSCALL32 0x91d02008	/* 32-bit syscall (ta 8) instruction */
#define	SYSCALL64 0x91d02040	/* 64-bit syscall (ta 64) instruction */
typedef	uint32_t syscall_t;	/* holds a syscall instruction */
#endif	/* sparc */

#if defined(i386) || defined(__i386)
#define	R_PC	EIP
#define	R_SP	UESP
#define	R_RVAL1	EAX		/* register holding a function return value */
#define	R_RVAL2	EDX		/* 32 more bits for a 64-bit return value */
#define	SYSCALL	0x9a		/* syscall (lcall) instruction opcode */
typedef u_char syscall_t[7];	/* holds a syscall instruction */
#endif	/* i386 */

#define	R_RVAL	R_RVAL1		/* simple function return value register */

/* maximum sizes of things */
#define	PRMAXSIG	(32 * sizeof (sigset_t) / sizeof (uint32_t))
#define	PRMAXFAULT	(32 * sizeof (fltset_t) / sizeof (uint32_t))
#define	PRMAXSYS	(32 * sizeof (sysset_t) / sizeof (uint32_t))

/* State values returned by Pstate() */
#define	PS_RUN	1	/* process is running */
#define	PS_STOP	2	/* process is stopped */
#define	PS_LOST	3	/* process is lost to control (EAGAIN) */
#define	PS_DEAD	4	/* process is terminated */

/* Flags accepted by Pgrab() */
#define	PGRAB_RETAIN	0x01	/* Retain tracing flags, else clear flags */
#define	PGRAB_FORCE	0x02	/* Open the process w/o O_EXCL */

/* Error codes from Pcreate() */
#define	C_STRANGE	-1	/* Unanticipated error, perror() was called */
#define	C_FORK		1	/* Unable to fork */
#define	C_PERM		2	/* No permission (file set-id or unreadable) */
#define	C_NOEXEC	3	/* Cannot find executable file */
#define	C_INTR		4	/* Interrupt received while creating */
#define	C_LP64		5	/* Program is _LP64, self is _ILP32 */

/* Error codes from Pgrab() */
#define	G_STRANGE	-1	/* Unanticipated error, perror() was called */
#define	G_NOPROC	1	/* No such process */
#define	G_ZOMB		2	/* Zombie process */
#define	G_PERM		3	/* No permission */
#define	G_BUSY		4	/* Another process has control */
#define	G_SYS		5	/* System process */
#define	G_SELF		6	/* Process is self */
#define	G_INTR		7	/* Interrupt received while grabbing */
#define	G_LP64		8	/* Process is _LP64, self is _ILP32 */

/* Flags accepted by Prelease */
#define	PRELEASE_CLEAR	0x10	/* Clear all tracing flags */
#define	PRELEASE_RETAIN	0x20	/* Retain final tracing flags */
#define	PRELEASE_HANG	0x40	/* Leave the process stopped */
#define	PRELEASE_KILL	0x80	/* Terminate the process */

typedef	struct {	/* argument descriptor for system call (Psyscall) */
	long	arg_value;	/* value of argument given to system call */
	void	*arg_object;	/* pointer to object in controlling process */
	char	arg_type;	/* AT_BYVAL, AT_BYREF */
	char	arg_inout;	/* AI_INPUT, AI_OUTPUT, AI_INOUT */
	u_short	arg_size;	/* if AT_BYREF, size of object in bytes */
} argdes_t;

typedef	struct {	/* return values from system call (Psyscall) */
	int	sys_errno;	/* syscall error number */
	long	sys_rval1;	/* primary return value from system call */
	long	sys_rval2;	/* second return value from system call */
} sysret_t;

/* values for type */
#define	AT_BYVAL	1
#define	AT_BYREF	2

/* values for inout */
#define	AI_INPUT	1
#define	AI_OUTPUT	2
#define	AI_INOUT	3

/* maximum number of syscall arguments */
#define	MAXARGS		8

/* maximum size in bytes of a BYREF argument */
#define	MAXARGL		(4*1024)

/* Kludges to make things work on Solaris 2.6 */
#if !defined(_LP64) && !defined(PR_MODEL_UNKNOWN)
#define	PR_MODEL_UNKNOWN 0
#define	PR_MODEL_ILP32	0	/* process data model is ILP32 */
#define	PR_MODEL_LP64	2	/* process data model is LP64 */
#define	PR_MODEL_NATIVE	PR_MODEL_ILP32
#define	pr_dmodel	pr_filler[0]
#define	STACK_BIAS	0
#endif

/*
 * Function prototypes for routines in the process control package.
 */
extern	struct ps_prochandle *Pcreate(const char *, char *const *,
			int *, char *, size_t);
extern	const char *Pcreate_error(int);
extern	struct ps_prochandle *Pgrab(pid_t, int, int *);
extern	const char *Pgrab_error(int);
extern	int	Preopen(struct ps_prochandle *);
extern	void	Prelease(struct ps_prochandle *, int);
extern	void	Pfree(struct ps_prochandle *);

extern	int	Pasfd(struct ps_prochandle *);
extern	int	Pctlfd(struct ps_prochandle *);
extern	int	Pcreate_agent(struct ps_prochandle *);
extern	void	Pdestroy_agent(struct ps_prochandle *);
extern	int	Pwait(struct ps_prochandle *, u_int);
extern	int	Pstop(struct ps_prochandle *, u_int);
extern	int	Pstate(struct ps_prochandle *);
extern	pstatus_t *Pstatus(struct ps_prochandle *);
extern	int	Pgetareg(struct ps_prochandle *, int, prgreg_t *);
extern	int	Pputareg(struct ps_prochandle *, int, prgreg_t);
extern	int	Psetrun(struct ps_prochandle *, int, int);
extern	ssize_t	Pread(struct ps_prochandle *, void *, size_t, uintptr_t);
extern	ssize_t	Pwrite(struct ps_prochandle *, const void *, size_t, uintptr_t);
extern	int	Pclearsig(struct ps_prochandle *);
extern	int	Pclearfault(struct ps_prochandle *);
extern	int	Psetbkpt(struct ps_prochandle *, uintptr_t, u_long *);
extern	int	Pdelbkpt(struct ps_prochandle *, uintptr_t, u_long);
extern	int	Pxecbkpt(struct ps_prochandle *, u_long);
extern	int	Psetflags(struct ps_prochandle *, long);
extern	int	Punsetflags(struct ps_prochandle *, long);
extern	int	Psignal(struct ps_prochandle *, int, int);
extern	int	Pfault(struct ps_prochandle *, int, int);
extern	int	Psysentry(struct ps_prochandle *, int, int);
extern	int	Psysexit(struct ps_prochandle *, int, int);
extern	void	Psetsignal(struct ps_prochandle *, const sigset_t *);
extern	void	Psetfault(struct ps_prochandle *, const fltset_t *);
extern	void	Psetsysentry(struct ps_prochandle *, const sysset_t *);
extern	void	Psetsysexit(struct ps_prochandle *, const sysset_t *);
extern	void	Psync(struct ps_prochandle *);
extern	sysret_t Psyscall(struct ps_prochandle *, int, u_int, argdes_t *);
extern	int	Pisprocdir(struct ps_prochandle *, const char *);

/*
 * Function prototypes for system calls forced on the victim process.
 */
extern	int	pr_open(struct ps_prochandle *, const char *, int, mode_t);
extern	int	pr_creat(struct ps_prochandle *, const char *, mode_t);
extern	int	pr_close(struct ps_prochandle *, int);
extern	int	pr_door_info(struct ps_prochandle *, int, struct door_info *);
extern	void	*pr_mmap(struct ps_prochandle *,
			void *, size_t, int, int, int, off_t);
extern	void	*pr_zmap(struct ps_prochandle *,
			void *, size_t, int, int);
extern	int	pr_munmap(struct ps_prochandle *, void *, size_t);
extern	int	pr_memcntl(struct ps_prochandle *,
			caddr_t, size_t, int, caddr_t, int, int);
extern	int	pr_sigaction(struct ps_prochandle *,
			int, const struct sigaction *, struct sigaction *);
extern	int	pr_getitimer(struct ps_prochandle *,
			int, struct itimerval *);
extern	int	pr_setitimer(struct ps_prochandle *,
			int, const struct itimerval *, struct itimerval *);
extern	int	pr_ioctl(struct ps_prochandle *, int, int, void *, size_t);
extern	int	pr_fcntl(struct ps_prochandle *, int, int, void *);
extern	int	pr_stat(struct ps_prochandle *, const char *, struct stat *);
extern	int	pr_lstat(struct ps_prochandle *, const char *, struct stat *);
extern	int	pr_fstat(struct ps_prochandle *, int, struct stat *);
extern	int	pr_statvfs(struct ps_prochandle *, const char *, statvfs_t *);
extern	int	pr_fstatvfs(struct ps_prochandle *, int, statvfs_t *);
extern	int	pr_getrlimit(struct ps_prochandle *,
			int, struct rlimit *);
extern	int	pr_setrlimit(struct ps_prochandle *,
			int, const struct rlimit *);
extern	int	pr_lwp_exit(struct ps_prochandle *);
extern	int	pr_exit(struct ps_prochandle *, int);
extern	int	pr_waitid(struct ps_prochandle *,
			idtype_t, id_t, siginfo_t *, int);
extern	off_t	pr_lseek(struct ps_prochandle *, int, off_t, int);
extern	offset_t pr_llseek(struct ps_prochandle *, int, offset_t, int);
extern	int	pr_rename(struct ps_prochandle *, const char *, const char *);
extern	int	pr_link(struct ps_prochandle *, const char *, const char *);
extern	int	pr_unlink(struct ps_prochandle *, const char *);

/*
 * lwp iteration interface.
 */
typedef int proc_lwp_f(void *, const lwpstatus_t *);
extern int proc_lwp_iter(struct ps_prochandle *, proc_lwp_f *, void *);

/*
 * Symbol table interfaces.
 */

/*
 * Pseudo-names passed to proc_lookup_by_name() for well-known load objects.
 * NOTE: It is required that PR_OBJ_EXEC and PR_OBJ_LDSO exactly match
 * the definitions of PS_OBJ_EXEC and PS_OBJ_LDSO from <proc_service.h>.
 */
#define	PR_OBJ_EXEC	((const char *)0)	/* search the executable file */
#define	PR_OBJ_LDSO	((const char *)1)	/* search ld.so.1 */
#define	PR_OBJ_EVERY	((const char *)-1)	/* search every load object */

/*
 * 'object_name' is the name of a load object obtained from an
 * iteration over the process's address space mappings (proc_mapping_iter),
 * or an iteration over the process's mapped objects (proc_object_iter),
 * or else it is one of the special PR_OBJ_* values above.
 */
extern	int	proc_lookup_by_name(struct ps_prochandle *,
			const char *object_name, const char *sym_name,
			GElf_Sym *sym);

extern	int	proc_lookup_by_addr(struct ps_prochandle *,
			uintptr_t addr,
			char *sym_name_buffer, size_t symbufsize,
			GElf_Sym *sym);

typedef int proc_map_f(void *, const prmap_t *, const char *object_name);
extern int proc_mapping_iter(struct ps_prochandle *, proc_map_f *, void *);
extern int proc_object_iter(struct ps_prochandle *, proc_map_f *, void *);

extern const prmap_t *proc_addr_to_map(struct ps_prochandle *, uintptr_t addr);
#if 0	/* XXX not yet */
extern const prmap_t *proc_name_to_map(struct ps_prochandle *,
			const char *object_name);
#endif

extern char *proc_execname(struct ps_prochandle *, char *, size_t);
extern char *proc_objname(struct ps_prochandle *, uintptr_t, char *, size_t);
extern char *proc_getenv(struct ps_prochandle *, const char *,
			char *, size_t);

/*
 * Symbol table iteration interface.
 */
typedef int proc_sym_f(void *, const GElf_Sym *sym, const char *sym_name);
extern int proc_symbol_iter(struct ps_prochandle *, const char *object_name,
			int which, int type, proc_sym_f *, void *);
/*
 * 'which' selects which symbol table and can be one of the following.
 */
#define	PR_SYMTAB	1
#define	PR_DYNSYM	2
/*
 * 'type' selects the symbols of interest by binding and type.
 * It is a bit-mask of one or more of the following.
 */
#define	BIND_LOCAL	0x0001
#define	BIND_GLOBAL	0x0002
#define	BIND_WEAK	0x0004
#define	BIND_ANY (BIND_LOCAL|BIND_GLOBAL|BIND_WEAK)
#define	TYPE_NOTYPE	0x0100
#define	TYPE_OBJECT	0x0200
#define	TYPE_FUNC	0x0400
#define	TYPE_SECTION	0x0800
#define	TYPE_FILE	0x1000
#define	TYPE_ANY (TYPE_NOTYPE|TYPE_OBJECT|TYPE_FUNC|TYPE_SECTION|TYPE_FILE)

/*
 * This returns the rtld_db agent handle for the process.
 * The handle will become invalid at the next successful exec() and
 * must not be used beyond that point (see proc_reset_maps(), below).
 */
extern	rd_agent_t *proc_rd_agent(struct ps_prochandle *);

/*
 * This should be called when an RD_DLACTIVITY event with the
 * RD_CONSISTENT state occurs via librtld_db's event mechanism.
 * This makes libproc's address space mappings and symbol tables current.
 */
extern	void	proc_update_maps(struct ps_prochandle *);

/*
 * This must be called after the victim process performs a successful
 * exec() if any of the symbol table interface functions have been called
 * prior to that point.  This is essential because an exec() invalidates
 * all previous symbol table and address space mapping information.
 * It is always safe to call, but if it is called other than after an
 * exec() by the victim process it just causes unnecessary overhead.
 *
 * The rtld_db agent handle obtained from a previous call to
 * proc_rd_agent() is made invalid by proc_reset_maps() and
 * proc_rd_agent() must be called again to get the new habdle.
 */
extern	void	proc_reset_maps(struct ps_prochandle *);

/*
 * Stack frame iteration interface.
 */
typedef int proc_stack_f(void *, uintptr_t pc, u_int argc, const long *argv);
extern int proc_stack_iter(struct ps_prochandle *, const prgregset_t,
			proc_stack_f *, void *);

/*
 * Miscellaneous function prototypes.
 */
extern	char	*proc_dirname(const char *, char *, size_t);
extern	int	proc_get_auxv(pid_t, auxv_t *, int);
extern	int	proc_get_cred(pid_t, prcred_t *, int);
extern	int	proc_get_psinfo(pid_t, psinfo_t *);
extern	int	proc_get_status(pid_t, pstatus_t *);
extern	pid_t	proc_pidarg(const char *, psinfo_t *);
extern	ssize_t	proc_read_string(int, char *, size_t, off_t);
extern	char	*proc_fltname(int, char *, size_t);
extern	char	*proc_signame(int, char *, size_t);
extern	char	*proc_sysname(int, char *, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _LIBPROC_H */
