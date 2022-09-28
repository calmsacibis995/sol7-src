/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_PCONTROL_H
#define	_PCONTROL_H

#pragma ident	"@(#)Pcontrol.h	1.1	97/12/23 SMI"

/*
 * Implemention-specific include file for libproc process management.
 * This is not to be seen by the clients of libproc.
 */

#include <stdio.h>
#include <gelf.h>
#include <procfs.h>
#include <rtld_db.h>
#include <libproc.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Definitions of the process control structures, internal to libproc.
 * These may change without affecting clients of libproc.
 */

typedef struct {	/* symbol table */
	Elf_Data *sym_data;	/* start of table */
	size_t	sym_symn;	/* number of entries */
	char	*sym_strs;	/* ptr to strings */
} sym_tbl_t;

typedef struct list {	/* circular doubly-linked list */
	struct list	*list_forw;
	struct list	*list_back;
} list_t;

typedef struct file_info {	/* symbol information for a mapped file */
	list_t	file_list;	/* linked list */
	char	file_pname[PRMAPSZ];	/* name from prmap_t */
	struct map_info *file_map;	/* primary (text) mapping */
	int	file_ref;	/* references from map_info_t structures */
	int	file_fd;	/* file descriptor for the mapped file */
	int	file_init;	/* 0: initialization yet to be performed */
	int	file_etype;	/* type from ehdr */
	rd_loadobj_t *file_lo;	/* load object structure from rtld_db */
	char	*file_lname;	/* name from rtld_db */
	Elf	*file_elf;	/* elf handle so we can close */
	sym_tbl_t file_symtab;	/* symbol table */
	sym_tbl_t file_dynsym;	/* dynamic symbol table */
} file_info_t;

typedef struct map_info {	/* description of an address space mapping */
	list_t	map_list;	/* linked list */
	prmap_t	map_pmap;	/* /proc description of this mapping */
	file_info_t *map_file;	/* pointer into list of mapped files */
} map_info_t;

struct ps_prochandle {
	pstatus_t orig_status;	/* remembered status on Pgrab() */
	pstatus_t status;	/* status when stopped */
	uintptr_t sysaddr;	/* address of most recent syscall instruction */
	pid_t	pid;		/* process-ID */
	int	state;		/* state of the process, see "libproc.h" */
	u_int	flags;		/* see defines below */
	u_int	agentcnt;	/* Pcreate_agent()/Pdestroy_agent() ref count */
	int	asfd;		/* /proc/<pid>/as filedescriptor */
	int	ctlfd;		/* /proc/<pid>/ctl filedescriptor */
	int	statfd;		/* /proc/<pid>/status filedescriptor */
	int	agentctlfd;	/* /proc/<pid>/lwp/agent/ctl */
	int	agentstatfd;	/* /proc/<pid>/lwp/agent/status */
	int	info_valid;	/* if zero, map and file info need updating */
	u_int	num_mappings;	/* number of map elements in map_info */
	u_int	num_files;	/* number of file elements in file_info */
	list_t	map_head;	/* head of address space mappings */
	list_t	file_head;	/* head of mapped files w/ symbol table info */
	char	*execname;	/* name of the executable file */
	auxv_t	*auxv;		/* the process's aux vector */
	rd_agent_t *rap;	/* cookie for rtld_db */
	map_info_t *map_exec;	/* the mapping for the executable file */
	map_info_t *map_ldso;	/* the mapping for ld.so.1 */
};

/* flags */
#define	CREATED		0x01	/* process was created by Pcreate() */
#define	SETSIG		0x02	/* set signal trace mask before continuing */
#define	SETFAULT	0x04	/* set fault trace mask before continuing */
#define	SETENTRY	0x08	/* set sysentry trace mask before continuing */
#define	SETEXIT		0x10	/* set sysexit trace mask before continuing */
#define	SETHOLD		0x20	/* set signal hold mask before continuing */
#define	SETREGS		0x40	/* set registers before continuing */

/* shorthand for register array */
#define	REG	status.pr_lwp.pr_reg

/*
 * Implementation functions in the process control library.
 * These are not exported to clients of the library.
 */
extern	int	Pscantext(struct ps_prochandle *);
extern	void	Pinitsym(struct ps_prochandle *);

/*
 * Simple convenience.
 */
#define	TRUE	1
#define	FALSE	0

#ifdef	__cplusplus
}
#endif

#endif	/* _PCONTROL_H */
