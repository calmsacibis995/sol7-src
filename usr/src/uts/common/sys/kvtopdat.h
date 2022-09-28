/*
 * Copyright (c) 1991-1996, by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#ifndef _SYS_KVTOPDATA_H
#define	_SYS_KVTOPDATA_H

#pragma ident	"@(#)kvtopdata.h	1.9	97/05/19 SMI"

/*
 * crash dump, libkvm support
 *
 * The kernel is not guaranteed to be loaded at a particular fixed
 * physical address.  To support virtual to physical address
 * translations a table of the relevant kernel mappings is built
 * by the routine setup_kvpm.  This table can be read from /dev/mem by
 * getting its physical address from the msgbuf header which is
 * guaranteed to be at a fixed physical address.  Although the
 * the kernel is not loaded at a fixed address, most of its
 * segments get allocated in chunks of contiguous physical memory.  The
 * ones that aren't are dynamic and have their own data structures,
 * - usually ptes - to describe the mapping.  These extra data structures
 * are in the kernel's data segment which is covered by one of the
 * ranges covered in the map below.
 *
 * NKVTOPENTS can change from one version of kvtopdata to another.  Applications
 * should use nentries to figure how many entries are in kvtopmap.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define	KVTOPD_VER	1
#define	NKVTOPENTS	200

struct kvtophdr {
	int	version;
	size_t	pagesize;
	int	nentries;
};

struct kvtopent {
	caddr_t	kvpm_vaddr;		/* kernel virtual address */
	pfn_t	kvpm_pfnum;		/* base physical page # */
	pgcnt_t	kvpm_len;		/* length in contiguous pages */
};

struct kvtopdata {
	struct kvtophdr hdr;
	struct kvtopent	kvtopmap[NKVTOPENTS];
};

#ifdef _KERNEL

extern struct kvtopdata kvtopdata;
extern void dump_kvtopdata(void);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* _SYS_KVTOPDATA_H */
