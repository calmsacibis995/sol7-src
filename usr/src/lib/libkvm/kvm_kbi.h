/*
 * Copyright (c) 1987-1993, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_KVM_KBI_H
#define	_KVM_KBI_H

#pragma ident	"@(#)kvm_kbi.h	1.7	98/02/22 SMI"

#include <sys/types.h>
#include <vm/as.h>
#include <vm/page.h>
#include <sys/kvtopdata.h>
#include <nlist.h>
#include <sys/user.h>
#include <sys/proc.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * This structure is shared with implementors of platform
 * dependent libkvm modules (lkvm_pd.so).  It should still
 * be considered opaque by all except implementors of
 * lkvm_pd.so modules.
 *
 * This is an uncommitted interface, and thus any reliance on it
 * by programmers is apt to result in future incompatibility for
 * their programs. Only the documented libkvm routines [man (3k)]
 * should be used by applications.
 */

struct _kvmd {			/* libkvm dynamic variables */
	int	flags;		/* flags (see below) */
	int	namefd;		/* namelist file descriptor */
	int	corefd;		/* corefile file descriptor */
	int	virtfd;		/* virtual memory file descriptor */
	int	swapfd;		/* swap file descriptor */
	char	*name;		/* saved name of namelist file */
	char	*core;		/* saved name of corefile file */
	char	*virt;		/* saved name of virtual memory file */
	char	*swap;		/* saved name of swap file */
	u_int	nproc;		/* number of process structures */
	struct proc *proc;	/* address of process table */
	struct proc *practp;	/* address of pointer to active process */
	caddr_t	econtig;	/* end of contiguous kernel memory */
	struct as Kas;		/* kernel address space */
	struct seg Ktextseg;	/* segment that covers kernel text+data */
	struct seg Kseg;	/* segment that covers kmem_alloc space */
	struct seg_ops *segvn;	/* ops vector for segvn segments */
	struct seg_ops *segmap;	/* ops vector for segmap segments */
	struct seg_ops *segdev;	/* ops vector for segdev segments */
	struct seg_ops *segkmem; /* ops vector for segkmem segments */
	struct seg_ops *segkp;	/* ops vector for segkp segments */
	struct seg_ops *segspt; /* ops vectorfor segspt segments */
	struct swapinfo *sip;	/* ptr to linked list of swapinfo structures */
	struct swapinfo *swapinfo; /* kernel virtual addr of 1st swapinfo */
	struct vnode *kvp;	/* vp used with segkp/no anon */
	long	page_hashsz;	/* number of buckets in page hash list */
	struct page **page_hash; /* address of page hash list */
	struct proc *pbuf;	/* pointer to process table cache */
	struct proc *pnext;	/* next proc pointer */
	struct user *uarea;
	char	*sbuf;		/* pointer to process stack cache */
	int	stkpg;		/* page in stack cache */
	struct seg useg;	/* segment structure for user pages */
	struct as Uas;		/* default user address space */
	struct condensed	{
		struct dumphdr	*cd_dp;	/* dumphdr pointer */
		off_t	*cd_atoo;	/* pointer to offset array */
		int	cd_atoosize;	/* number of elements in array */
		int	cd_chunksize;	/* size of each chunk, in bytes */
	} *corecdp;	/* if not null, core file is condensed */
	struct condensed *swapcdp; /* if not null, swap file is condensed */
	struct kvtophdr kvtophdr; /* for __kvm_kvtop() */
	struct kvtopent *kvtopmap;
	struct kvm_param *kvm_param;
};

#define	KVMD_WRITE	0x1	/* kernel opened for write */
#define	KVMD_SEGSLOADED	0x2	/* Ktextseg and Kseg have been loaded in-core */

/*
 * Platform-dependent constants.
 */
struct kvm_param {
	u_longlong_t	p_pagemask;
	uintptr_t	p_kernelbase;
	u_long		p_pagesize;
	u_int		p_pageshift;
	u_long		p_pageoffset;
	u_int		p_pagenum_offset;
};

/*
 * The following structure is used to hold platform dependent
 * info.  It is only of interest to implementors of lkvm_pd.so
 * modules.
 */
struct kvm_pdfuncs {
	int	(*kvm_segkmem_paddr)();
};

u_longlong_t	__kvm_kvtop(kvm_t *, uintptr_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _KVM_KBI_H */
