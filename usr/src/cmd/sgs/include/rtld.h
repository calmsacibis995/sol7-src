/*
 *	Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)rtld.h	1.29	98/01/20 SMI"

#ifndef		RTLD_DOT_H
#define		RTLD_DOT_H

/*
 * Global include file for the runtime linker support library.
 */
#include	<time.h>
#include	"sgs.h"
#include	"machdep.h"

#ifdef		_SYSCALL32
#include	<inttypes.h>
#endif


/*
 * Permission structure.  Used to define access with ld.so.1 link maps.
 */
typedef struct permit {
	unsigned long	p_cnt;		/* No. of p_value entries of which */
	unsigned long	p_value[1];	/* there may be more than one */
} Permit;


/*
 * Linked list of directories or filenames (built from colon seperated string).
 */
typedef struct pnode {
	const char *	p_name;
	size_t		p_len;
	void *		p_info;
	struct pnode *	p_next;
} Pnode;

typedef struct rt_map	Rt_map;

typedef struct audit_fields	Audit_fields;

/*
 * Private structure for communication between rtld_db and rtld.
 */
typedef struct rtld_db_priv {
	long	rtd_version;		/* version no. */
	size_t	rtd_objpad;		/* padding around mmap()ed objects */
} Rtld_db_priv;

#ifdef _SYSCALL32
typedef struct rtld_db_priv32 {
	int32_t	rtd_version;		/* version no. */
	uint32_t rtd_objpad;		/* padding around mmap()ed objects */
} Rtld_db_priv32;
#endif	/* _SYSCALL32 */


#define	R_RTLDDB_VERSION	1	/* current rtld_db/rtld version level */

/*
 * Runtime linker private data maintained for each shared object.  Maps are
 * connected to link map lists for `main' and possibly `rtld'.
 */
typedef	struct lm_list {
	Rt_map *	lm_head;
	Rt_map *	lm_tail;
	ulong_t		lm_flags;
	int 		lm_numobj;
	int 		lm_numinitfirst;
	int 		lm_numfinifirst;
	List		lm_interpose;
} Lm_list;

/*
 * Possible Link_map list flags (Lm_list.lm_list)
 */
#define	LML_FLG_BASELM	0x0001		/* primary link-map */
#define	LML_FLG_RTLDLM	0x0002		/* rtld link-map */
#define	LML_FLG_NOAUDIT	0x0004		/* symbol auditing disabled */
#define	LML_FLG_PLTREL	0x0008		/* special flag for PLT relocations */
					/*  in ld.so.1 on x86 (see dlused()) */
#define	LML_FLG_DELNEED	0x0010		/* deletetions required on this */
					/*	link-map list */


/*
 * Information for dlopen(), dlsym(), and dlclose() on libraries linked by rtld.
 * Each shared object referred to in a dlopen call has an associated dl_obj
 * structure.  For each such structure there is a list of the shared objects
 * on which the referenced shared object is dependent.
 */
#define	DL_MAGIC	0x580331
#define	DL_DLOPEN_0	0x561109	/* dlopen(0) magic identifier */
#define	DL_CIGAM	0x940212

typedef struct dl_obj {
	long		dl_magic;	/* DL_MAGIC */
	Permit *	dl_permit;	/* permit for this dlopen invocation */
	long		dl_usercnt;	/* count of dlopen invocations */
	long		dl_permcnt;	/* count of permit give-aways */
	List		dl_depends;	/* dependencies applicable for dlsym */
	Rt_map *	dl_lastdep;	/* last applicable dlsym dependency */
	Lm_list *	dl_list;	/* link-map list */
	long		dl_cigam;	/* DL_CIGAM */
} Dl_obj;

struct rt_map {
	Link_map	rt_public;	/* public data */
	List		rt_alias;	/* list of linked file names */
	void (*		rt_init)();	/* address of _init */
	void (*		rt_fini)();	/* address of _fini */
	char *		rt_runpath;	/* LD_RUN_PATH and its equivalent */
	Pnode *		rt_runlist;	/*	Pnode structures */
	long		rt_count;	/* reference count */
	List		rt_depends;	/* list of dependent libraries */
	List		rt_parents;	/* list of objects depend on me */
	Dl_obj *	rt_dlp;		/* pointer to a dlopened object */
	Permit *	rt_permit;	/* ids permitted to access this lm */
	unsigned long	rt_msize;	/* total memory mapped */
	unsigned long	rt_etext;	/* etext address */
	unsigned long	rt_padstart;	/* start of image (including padding) */
	unsigned long	rt_padimlen;	/* size of image (including padding */
	struct fct *	rt_fct;		/* file class table for this object */
	Pnode *		rt_filtees;	/* 	Pnode list of REFNAME(lmp) */
	Rt_map *	rt_lastdep;	/* the last dependency added */
	Sym *(*		rt_symintp)();	/* link map symbol interpreter */
	void *		rt_priv;	/* private data, object type specific */
	Lm_list * 	rt_list;	/* link map list we belong to */
	unsigned long	rt_flags;	/* state flags, see FLG below */
	unsigned int	rt_mode;	/* usage mode, see RTLD mode flags */
	dev_t		rt_stdev;	/* device id and inode number for .so */
	ino_t		rt_stino;	/*	multiple inclusion checks */
	List		rt_refrom;	/* list of referencing objects */
	const char *	rt_pathname;	/* full pathname of loaded object */
	size_t		rt_dirsz;	/*	and its size */
	List		rt_copy;	/* list of copy relocations */
	Audit_fields * 	rt_audit;	/* audit field array */
	int		rt_sortval;	/* temporary buffer to traverse graph */
	Word		rt_relacount;	/* # of RELATIVE relocations */
	Syminfo *	rt_syminfo;	/* elf .syminfo section - here */
					/*	because it is checked in */
					/*	common code */
};

#ifdef _SYSCALL32
/*
 * This stuff is for the 64-bit rtld_db to read 32-bit
 * processes out of procfs.
 */
typedef struct rt_map32		Rt_map32;

struct rt_map32 {
	Link_map32	rt_public;	/* public data */
	List32		rt_alias;	/* list of linked file names */
	uint32_t 	rt_init;	/* address of _init */
	uint32_t	rt_fini;	/* address of _fini */
	uint32_t	rt_runpath;	/* LD_RUN_PATH and its equivalent */
	uint32_t	rt_runlist;	/*	Pnode structures */
	int32_t		rt_count;	/* reference count */
	List32		rt_depends;	/* list of dependent libraries */
	List32		rt_parents;	/* list of objects depend on me */
	uint32_t	rt_dlp;		/* pointer to a dlopened object */
	uint32_t	rt_permit;	/* ids permitted to access this lm */
	uint32_t	rt_msize;	/* total memory mapped */
	uint32_t	rt_etext;	/* etext address */
	uint32_t	rt_padstart;	/* start of image (including padding) */
	uint32_t	rt_padimlen;	/* size of image (including padding */
	uint32_t	rt_fct;		/* file class table for this object */
	uint32_t	rt_filtees;	/* 	Pnode list of REFNAME(lmp) */
	Rt_map32 *	rt_lastdep;	/* the last dependency added */
	uint32_t	rt_symintp;	/* link map symbol interpreter */
	uint32_t	rt_priv;	/* private data, object type specific */
	uint32_t 	rt_list;	/* link map list we belong to */
	uint32_t	rt_flags;	/* state flags, see FLG below */
	uint32_t	rt_mode;	/* usage mode, see RTLD mode flags */
	uint32_t	rt_stdev;	/* device id and inode number for .so */
	uint32_t	rt_stino;	/*	multiple inclusion checks */
	List32		rt_refrom;	/* list of referencing objects */
	uint32_t	rt_dirname;	/* real dirname of loaded object */
	uint32_t	rt_dirsz;	/*	and its size */
	List32		rt_copy;	/* list of copy relocations */
	uint32_t 	rt_audit;	/* audit field array */
	int32_t		rt_sortval;	/* temporary buffer to traverse graph */
};

#endif	/* _SYSCALL32 */


#define	REF_NEEDED	1		/* explicit (needed) dependency */
#define	REF_SYMBOL	2		/* implicit (symbol binding) */
					/*	dependency */
#define	REF_DELETE	3		/* deleted object dependency update */
#define	REF_DLOPEN	4		/* dlopen() dependency update */
#define	REF_DLCLOSE	5		/* dlclose() dependency update */
#define	REF_UNPERMIT	6		/* unnecessary permit removal */
#define	REF_DIRECT	7		/* explicit (direct binding) */
					/*	dependency */
#define	REF_NEWPERM	8		/* group perm assignment */


/*
 * Link map state flags.
 */
#define	FLG_RT_ISMAIN	0x00000001	/* object represents main executable */
#define	FLG_RT_ANALYZED	0x00000002	/* object has been analyzed */
#define	FLG_RT_SETGROUP	0x00000004	/* group establishisment required */
#define	FLG_RT_COPYTOOK	0x00000008	/* copy relocation taken */
#define	FLG_RT_OBJECT	0x00000010	/* object processing (ie. .o's) */
#define	FLG_RT_BOUND	0x00000020	/* bound to indicator */
#define	FLG_RT_DELETING	0x00000040	/* deletion in progress */
#define	FLG_RT_PROFILE	0x00000080	/* image is being profiled */
#define	FLG_RT_IMGALLOC	0x00000100	/* image is allocated (not mmap'ed) */
#define	FLG_RT_INITDONE	0x00000200	/* objects .init has be called */
#define	FLG_RT_AUX	0x00000400	/* filter is an auxiliary filter */
#define	FLG_RT_FIXED	0x00000800	/* image location is fixed */
#define	FLG_RT_PRELOAD	0x00001000	/* object was preloaded */
#define	FLG_RT_CACHED	0x00002000	/* cached version of object used */
#define	FLG_RT_RELOCING	0x00004000	/* relocation in progress */
#define	FLG_RT_LOADFLTR	0x00008000	/* trigger filtee loading */
#define	FLG_RT_AUDIT	0x00010000	/* audit enabled on this object */
#define	FLG_RT_FLTRING	0x00020000	/* filtee loading in progress */
#define	FLG_RT_FINIDONE	0x00040000	/* objects .fini done */
#define	FLG_RT_INITFRST 0x00080000	/* execute .init first */
#define	FLG_RT_NOOPEN	0x00100000	/* dlopen() not allowed */
#define	FLG_RT_FINIFRST 0x00200000	/* execute .fini first */
#define	FLG_RT_ORIGIN	0x00400000	/* $ORIGIN processing required */
#define	FLG_RT_INTRPOSE	0x00800000	/* object is an INTERPOSER */
#define	FLG_RT_DIRECT	0x01000000	/* object has DIRECT bindins enabled */


/*
 * Macros for getting to link_map data.
 */
#define	ADDR(X)		((X)->rt_public.l_addr)
#define	NAME(X)		((X)->rt_public.l_name)
#define	DYN(X)		((X)->rt_public.l_ld)
#define	NEXT(X)		((X)->rt_public.l_next)
#define	PREV(X)		((X)->rt_public.l_prev)
#define	REFNAME(X)	((X)->rt_public.l_refname)

/*
 * Macros for getting to linker private data.
 */
#define	ALIAS(X)	((X)->rt_alias)
#define	INIT(X)		((X)->rt_init)
#define	FINI(X)		((X)->rt_fini)
#define	RPATH(X)	((X)->rt_runpath)
#define	RLIST(X)	((X)->rt_runlist)
#define	COUNT(X)	((X)->rt_count)
#define	DEPENDS(X)	((X)->rt_depends)
#define	PARENTS(X)	((X)->rt_parents)
#define	DLP(X)		((X)->rt_dlp)
#define	PERMIT(X)	((X)->rt_permit)
#define	MSIZE(X)	((X)->rt_msize)
#define	ETEXT(X)	((X)->rt_etext)
#define	FCT(X)		((X)->rt_fct)
#define	FILTEES(X)	((X)->rt_filtees)
#define	LASTDEP(X)	((X)->rt_lastdep)
#define	SYMINTP(X)	((X)->rt_symintp)
#define	LIST(X)		((X)->rt_list)
#define	FLAGS(X)	((X)->rt_flags)
#define	MODE(X)		((X)->rt_mode)
#define	REFROM(X)	((X)->rt_refrom)
#define	PADSTART(X)	((X)->rt_padstart)
#define	PADIMLEN(X)	((X)->rt_padimlen)
#define	PATHNAME(X)	((X)->rt_pathname)
#define	DIRSZ(X)	((X)->rt_dirsz)
#define	COPY(X)		((X)->rt_copy)
#define	AUDIT(X)	((X)->rt_audit)
#define	RELACOUNT(X)	((X)->rt_relacount)
#define	SYMINFO(X)	((X)->rt_syminfo)


/*
 * Flags for lookup_sym (and hence find_sym) routines.
 */
#define	LKUP_DEFT	0x00		/* Simple lookup request */
#define	LKUP_SPEC	0x01		/* Special ELF lookup (allows address */
					/*	resolutions to plt[] entries) */
#define	LKUP_LDOT	0x02		/* Indicates the original A_OUT */
					/*	symbol had a leading `.' */
#define	LKUP_FIRST	0x04		/* Lookup symbol in first link map */
					/*	only */
#define	LKUP_COPY	0x08		/* Lookup symbol for a COPY reloc, do */
					/*	not bind to symbol at head */
#define	LKUP_NOINT	0x10		/* Do not look in special */
					/*	'interposing' cache */

/*
 * Data structure for calling lookup_sym()
 */
typedef struct {
	const char *	sl_name;	/* sym name */
	Permit *	sl_permit;	/* permit allowed to view */
	Rt_map *	sl_cmap;	/* callers link-map */
	Rt_map *	sl_imap;	/* initial link-map to search */
	unsigned long	sl_rsymndx;	/* referencing reloc symndx */
} Slookup;

extern Sym *		lookup_sym(Slookup *, Rt_map **, int);

extern Lm_list		lml_main;	/* the `main's link map list */
extern Lm_list		lml_rtld;	/* rtld's link map list */
extern Lm_list *	lml_list[];


/*
 * Runtime cache structure.
 */
typedef	struct	rtc_head {
	Word	rtc_version;		/* version of cache file */
	time_t	rtc_time;		/* time cache file was created */
	Addr	rtc_base;		/* base address of mapped cache */
	Addr	rtc_begin;		/* address range reservation required */
	Addr	rtc_end;		/*	for cached objects */
	Xword	rtc_size;		/* size of cache data in bytes */
	Word	rtc_flags;		/* various flags */
	List *	rtc_objects;		/* offset of object cache list */
	Pnode *	rtc_libpath;		/* offset of libpath list (Pnode) */
	Pnode *	rtc_trusted;		/* offset of trusted dir list (Pnode) */
} Rtc_head;

typedef struct	rtc_obj {
	Word	rtc_hash;		/* hash value of the input filename */
	char *	rtc_name;		/* input filename */
	char *	rtc_cache;		/* associated cache filename */
} Rtc_obj;

#define	RTC_FLG_DEBUG	0x01

/*
 * Cache version definition values.
 */
#define	RTC_VER_NONE	0
#define	RTC_VER_CURRENT	1
#define	RTC_VER_NUM	2

/*
 * Function prototypes.
 */
extern	int	rt_dldump(Rt_map *, const char *, int, int, Addr);

#if	defined(__sparc) || defined(__sparcv9) || defined(__i386)
extern	void	elf_plt_write(unsigned int *, unsigned long *);
#else
#error Unknown architecture!
#endif

#endif
