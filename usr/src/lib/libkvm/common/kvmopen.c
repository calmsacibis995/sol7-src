/*
 * Copyright (c) 1987-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kvmopen.c	2.58	98/02/22 SMI"

#include "kvm_impl.h"
#include <stdio.h>
#include <nlist.h>
#include <varargs.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/swap.h>
#include <sys/sysmacros.h>	/* for roundup() */
#include <sys/proc.h>
#include <sys/thread.h>
#include <assert.h>
#include <dlfcn.h>
#include <sys/systeminfo.h>
#include <sys/dumphdr.h>
#include <libelf.h>

/*
 * This used to be in sys/param.h in 4.1.
 * It now lives in sys/fs/ufs_fs.h, which is a strange place for
 * non-fs specific macros.  Anyhow, we just copy it to avoid hauling
 * in a whole load of fs-specific include files.
 */
#define	isset(a, i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))

static struct nlist knl[] = {
	{ "practive" },
#define	X_PROC		0
	{ "nproc" },
#define	X_NPROC		1
	{ "segvn_ops" },
#define	X_SEGVN		2
	{ "segmap_ops" },
#define	X_SEGMAP	3
	{ "segdev_ops" },
#define	X_SEGDEV	4
	{ "swapinfo" },
#define	X_SWAPINFO	5
	{ "pagenum_offset" },
#define	X_PAGENUM_OFST	6
	{ "page_hash" },
#define	X_PAGE_HASH	7
	{ "page_hashsz" },
#define	X_PAGE_HASHSZ	8
	{ "econtig" },
#define	X_ECONTIG	9
	{ "kas" },
#define	X_KAS		10
	{ "segkmem_ops" },
#define	X_SEGKMEM	11
	{ "segkp_ops" },
#define	X_SEGKP		12
	{ "kvp" },
#define	X_KVP		13
	{ "_pagesize" },
#define	X_PAGESIZE	14
	{ "_pageshift" },
#define	X_PAGESHIFT	15
	{ "_pagemask" },
#define	X_PAGEMASK	16
	{ "_pageoffset" },
#define	X_PAGEOFFSET	17
	{ "_kernelbase" },
#define	X_KERNELBASE	18
	{ "_usrstack" },
#define	X_USRSTACK	19
	{ "kvseg" },
#define	X_KVSEG		20
	{ "segspt_shmops" },
#define	X_SEGSPT	21
	{ "end" },
#define	X_END		22
	{ "" },
};

#define	knlsize	(sizeof (knl) / sizeof (struct nlist))

static int readkvar(kvm_t *, u_longlong_t, void *, size_t, char *, char *);
static int getkvtopdata(kvm_t *, char *, char *);
static int __kvm_getkvar(kvm_t *, uintptr_t, void *, size_t, char *, char *);

/*
 * Want to share these routines with lkvm_pd.so, so
 * cant declare them "static".
 */

static int condensed_setup(int, struct condensed **, char *, char *);
static void condensed_takedown(struct condensed *);

/*
 * XXX-As currently the <stdarg.h> in our build environment is not done
 *     properly, the full function prototype for the following cannot
 *     be presented.  This applies to __kvm_openerror() as well.
 */
static void openperror();
void __kvm_openerror();

/*
 * Iterate through the list of in-core segment structures starting with
 * ssegp and ending with esegp, freeing any memory we allocated for each seg.
 */
static void
free_seglist(kvm_t *kd, struct seg *ssegp, struct seg *esegp)
{
	struct seg *segp, *nsegp;

	for (segp = ssegp; segp != esegp; segp = nsegp) {
		if (segp->s_as->a_lrep != AS_LREP_LINKEDLIST) {
			nsegp = segp->s_next.skiplist->segs[0];
			free(segp->s_next.skiplist);
		} else
			nsegp = segp->s_next.list;

		if (segp->s_as != &kd->Kas)
			free(segp->s_as);

		if (segp != ssegp)
			free(segp);
	}
}

/*
 * If we're not dealing with a live kernel, this function can be used to
 * preload a linked list of segment structures from the crash dump files.
 * Beginning with ssegp, which addresses a struct seg previously read
 * from the dump file, we read each subsequent segment in the list and
 * replace the s_as and s_next information in the struct seg with pointers
 * to our in-core copies, which we malloc space for as we go.  The in-core
 * copies can be subsequently used by getseg().
 */
static int
load_seglist(kvm_t *kd, uintptr_t kas_addr, struct seg *ssegp)
{
	struct seg **segpp, *osegp, *segp = ssegp;
	uintptr_t addr;

	while (segp != NULL) {
		if (segp->s_as != (struct as *)kas_addr) {
			addr = (uintptr_t)segp->s_as;
			segp->s_as = malloc(sizeof (struct as));

			if (segp->s_as == NULL) {
				segp->s_next.skiplist = NULL;
				goto error;
			}

			if (kvm_kread(kd, addr, segp->s_as,
			    sizeof (struct as)) != sizeof (struct as)) {

				free(segp->s_as);
				segp->s_next.skiplist = NULL;
				goto error;
			}
		} else
			segp->s_as = &kd->Kas;

		if (segp->s_as->a_lrep != AS_LREP_LINKEDLIST) {
			addr = (uintptr_t)segp->s_next.skiplist;
			segp->s_next.skiplist = malloc(sizeof (seg_skiplist));

			if (segp->s_next.skiplist == NULL) {
				if (segp->s_as != &kd->Kas)
					free(segp->s_as);
				goto error;
			}

			if (kvm_kread(kd, addr, segp->s_next.skiplist,
			    sizeof (seg_skiplist)) != sizeof (seg_skiplist)) {

				if (segp->s_as != &kd->Kas)
					free(segp->s_as);
				free(segp->s_next.skiplist);
				goto error;
			}

			addr = (uintptr_t)segp->s_next.skiplist->segs[0];
			segpp = &segp->s_next.skiplist->segs[0];

		} else {
			addr = (uintptr_t)segp->s_next.list;
			segpp = &segp->s_next.list;
		}

		if (addr != 0) {
			osegp = segp;
			segp = *segpp = malloc(sizeof (struct seg));

			if (segp == NULL)
				goto error;

			if (kvm_kread(kd, addr, segp,
			    sizeof (struct seg)) != sizeof (struct seg)) {
				free(segp);
				goto error;
			}

			segp->s_prev = osegp;
		} else
			segp = *segpp = NULL;
	}

	ssegp->s_prev = NULL;
	return (0);

error:
	free_seglist(kd, ssegp, segp);
	return (-1);
}

/*
 * Open kernel namelist and core image files and initialize libkvm
 */
kvm_t *
kvm_open(char *namelist, char *corefile, char *swapfile, int flag, char *err)
{
	kvm_t *kd;
	struct nlist kn[knlsize];
	struct seg *seg;
	struct stat64 membuf; 	/* live mem */
	struct stat64 fbuf;	/* user-specified core */
	int	namelist_class;
	int	tmpfd;
	Elf	*elf;
	char	*ident;

	kd = malloc(sizeof (struct _kvmd));
	if (kd == NULL) {
		openperror(err, "cannot allocate space for kvm_t");
		return (NULL);
	}
	kd->namefd = -1;
	kd->corefd = -1;
	kd->virtfd = -1;
	kd->swapfd = -1;
	kd->pnext = 0;
	kd->uarea = NULL;
	kd->pbuf = NULL;
	kd->sbuf = NULL;
	kd->econtig = NULL;
	kd->sip = NULL;
	kd->corecdp = NULL;
	kd->swapcdp = NULL;
	kd->proc = NULL;
	kd->kvm_param = NULL;
	kd->kvtopmap = NULL;

	/* copy static array to dynamic storage for reentrancy purposes */
	(void) memcpy(kn, knl, sizeof (knl));

	switch (flag) {
	case O_RDWR:
		kd->flags = KVMD_WRITE;
		break;
	case O_RDONLY:
		kd->flags = 0;
		break;
	default:
		__kvm_openerror(err, "illegal flag 0x%x to kvm_open()", flag);
		goto error;
	}

	if (namelist == NULL)
		namelist = LIVE_NAMELIST;
	kd->name = namelist;
	if ((kd->namefd = open(kd->name, O_RDONLY, 0)) < 0) {
		openperror(err, "cannot open %s", kd->name);
		goto error;
	}

	if (elf_version(EV_CURRENT) == EV_NONE) {
		__kvm_openerror(err, "libelf is out of date");
		goto error;
	}
	/* reopen to get a different file offset */
	if ((tmpfd = open(kd->name, O_RDONLY, 0)) < 0) {
		openperror(err, "cannot open %s", kd->name);
		goto error;
	}
	elf = elf_begin(tmpfd, ELF_C_READ, NULL);
	if (elf_kind(elf) != ELF_K_ELF) {
		__kvm_openerror(err, "%s: not a kernel namelist", kd->name);
		goto error;
	}
	ident = elf_getident(elf, NULL);
	namelist_class = ident[EI_CLASS];
	elf_end(elf);
	(void) close(tmpfd);

	if (corefile == NULL)
		corefile = LIVE_COREFILE;
	/*
	 * Check if corefile is really /dev/mem.
	 */
	if (stat64(LIVE_COREFILE, &membuf) == 0 &&
	    stat64(corefile, &fbuf) == 0 &&
	    S_ISCHR(fbuf.st_mode) && fbuf.st_rdev == membuf.st_rdev) {
		kd->virt = LIVE_VIRTMEM;
		if ((kd->virtfd = open(kd->virt, flag, 0)) < 0) {
			openperror(err, "cannot open %s", kd->virt);
			goto error;
		}
	}
	/*
	 * We have a live kernel iff kd->virt > 0, and that can only
	 * happen if corefile is NULL or LIVE_COREFILE.
	 */
	kd->core = corefile;
	if ((kd->corefd = open(kd->core, flag, 0)) < 0) {
		openperror(err, "cannot open %s", kd->core);
		goto error;
	}

#if defined(__sparc)
#ifdef _LP64
	/* 64-bit libkvm cannot look at 32-bit kernel namelists */
	if (namelist_class == ELFCLASS32) {
		__kvm_openerror(err,
		    "mismatch between 32-bit kernel '%s' and 64-bit libkvm",
		    kd->name);
		goto error;
	}
#else
	/* 32-bit libkvm cannot look at 64-bit kernel namelists */
	if (namelist_class == ELFCLASS64) {
		__kvm_openerror(err,
		    "mismatch between 64-bit kernel '%s' and 32-bit libkvm",
		    kd->name);
		goto error;
	}
#endif
	if (kd->virtfd < 0) {
		uint32_t	corefile_magic;

		/*
		 * If we're looking at a core file, check the type and
		 * make sure it matches the namelist.
		 */
		if ((tmpfd = open(kd->core, flag, 0)) < 0) {
			openperror(err, "cannot open %s", kd->core);
			goto error;
		}
		if (read(tmpfd, &corefile_magic,
		    sizeof (corefile_magic)) != sizeof (corefile_magic)) {
			__kvm_openerror(err,
			    "cannot read core file magic number");
			goto error;
		}
		(void) close(tmpfd);
		if ((corefile_magic != DUMP_MAGIC_32) &&
		    (corefile_magic != DUMP_MAGIC_64)) {
			__kvm_openerror(err,
			    "%s is not a core file (bad magic number)",
			    kd->core);
			goto error;
		}
		if ((namelist_class == ELFCLASS32) &&
		    (corefile_magic != DUMP_MAGIC_32)) {
			__kvm_openerror(err,
			    "mismatch between 32-bit kernel '%s' "
			    "and 64-bit core file '%s'",
			    kd->name, kd->core);
			goto error;
		}
		if ((namelist_class == ELFCLASS64) &&
		    (corefile_magic != DUMP_MAGIC_64)) {
			__kvm_openerror(err,
			    "mismatch between 64-bit kernel '%s' "
			    "and 32-bit core file '%s'",
			    kd->name, kd->core);
			goto error;
		}
	}
#endif

	if (condensed_setup(kd->corefd, &kd->corecdp, kd->core, err) == -1)
		goto error;	/* condensed_setup calls openperror */
	if (swapfile != NULL) {
		kd->swap = swapfile;
		if ((kd->swapfd = open(kd->swap, O_RDONLY, 0)) < 0) {
			openperror(err, "cannot open %s", kd->swap);
			goto error;
		}
		if (condensed_setup(kd->swapfd, &kd->swapcdp, kd->swap,
				    err) == -1)
			goto error;
	}

	/*
	 * read kernel data for internal library use
	 */
	if (nlist(kd->name, kn) == -1) {
		__kvm_openerror(err, "%s: not a kernel namelist", kd->name);
		goto error;
	}

	/*
	 * fetch the translation table from the memory image
	 * so that we can translate virtual addresses in the range
	 * KERNELBASE -> econtig into physical offsets.  And no,
	 * we can't just subtract KERNELBASE from the va any more ..
	 */
	if (getkvtopdata(kd, err, "kvtopdata") != 0) {
		__kvm_openerror(err, "%s: unable to read kvtopdata", kd->core);
		goto error;
	}
	/*
	 * Initialize system parameters.
	 *
	 * XXX There should be a better kernel/libkvm interface
	 * for passing information.  At the very least, this
	 * stuff should be packaged, so that it can be done with
	 * one read.
	 */
	kd->kvm_param = malloc(sizeof (struct kvm_param));
	if (kd->kvm_param == NULL) {
		openperror(err, "cannot allocate space for kvm_param");
		goto error;
	}
	if (__kvm_getkvar(kd, kn[X_PAGENUM_OFST].n_value, &kd->pagenum_offset,
	    sizeof (kd->pagenum_offset), err, "pagenum_offset") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_PAGESIZE].n_value, &kd->pagesz,
	    sizeof (kd->pagesz), err, "pagesize") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_PAGESHIFT].n_value, &kd->pageshift,
	    sizeof (kd->pageshift), err, "pageshift") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_PAGEMASK].n_value, &kd->pagemask,
	    sizeof (kd->pagemask), err, "pagemask") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_PAGEOFFSET].n_value, &kd->pageoffset,
	    sizeof (kd->pageoffset), err, "pageoffset") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_KERNELBASE].n_value, &kd->kernelbase,
	    sizeof (kd->kernelbase), err, "kernelbase") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_KAS].n_value, &kd->Kas,
	    sizeof (kd->Kas), err, "kas") != 0)
		goto error;

	if (kd->Kas.a_lrep == AS_LREP_LINKEDLIST)
		seg = kd->Kas.a_segs.list;
	else {
		seg_skiplist ssl;

		if (__kvm_getkvar(kd, (uintptr_t)kd->Kas.a_segs.skiplist,
		    &ssl, sizeof (ssl), err, "ktextseg skiplist") != 0)
			goto error;

		seg = ssl.segs[0];
	}
	if (__kvm_getkvar(kd, (uintptr_t)seg, &kd->Ktextseg,
	    sizeof (kd->Ktextseg), err, "ktextseg") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_KVSEG].n_value, &kd->Kseg,
	    sizeof (kd->Kseg), err, "kvseg") != 0)
		goto error;

	if (__kvm_getkvar(kd, kn[X_ECONTIG].n_value, &kd->econtig,
	    sizeof (kd->econtig), err, "econtig") != 0)
		goto error;

#define	getval(o, b)	\
	if (kvm_kread(kd, kn[o].n_value, 				\
	    &kd->b, sizeof (kd->b)) != sizeof (kd->b)) {		\
		__kvm_openerror(err, "%s: can't find %s", kd->name, #b);\
		goto error;						\
	}

	/* save segment mapping and process table info */
	kd->segvn = (struct seg_ops *)kn[X_SEGVN].n_value;
	kd->segmap = (struct seg_ops *)kn[X_SEGMAP].n_value;
	kd->segkmem = (struct seg_ops *)kn[X_SEGKMEM].n_value;
	kd->segdev = (struct seg_ops *)kn[X_SEGDEV].n_value;
	kd->segkp = (struct seg_ops *)kn[X_SEGKP].n_value;
	kd->segspt = (struct seg_ops *)kn[X_SEGSPT].n_value;
	kd->kvp = (struct vnode *)kn[X_KVP].n_value;

	getval(X_SWAPINFO, swapinfo);
	getval(X_PAGE_HASH, page_hash);
	getval(X_PAGE_HASHSZ, page_hashsz);

	/*
	 * In order to determine the current proc, we must first
	 * determine the current thread.
	 */
	{
		static proc_t *prc;

		(void) kvm_kread(kd, kn[X_PROC].n_value, &prc, sizeof (prc));
		kd->proc = prc;
	}
	kd->practp = (struct proc *)(kn[X_PROC].n_value);
	kd->pnext = kd->proc;
	getval(X_NPROC, nproc);

	/*
	 * If we're not dealing with a live kernel, then read the entire
	 * Ktextseg and Kseg segment chains in core to improve performance
	 * of subsequent lookups.
	 */
	if (kd->virtfd == -1) {
		struct seg tseg, kvseg;

		(void) memcpy(&tseg, &kd->Ktextseg, sizeof (struct seg));
		(void) memcpy(&kvseg, &kd->Kseg, sizeof (struct seg));

		if (load_seglist(kd, kn[X_KAS].n_value, &tseg) == -1) {
			__kvm_openerror(err, "%s: failed to preload kernel "
			    "text+data segment list", kd->core);
			goto error;
		}

		if (load_seglist(kd, kn[X_KAS].n_value, &kvseg) == -1) {
			__kvm_openerror(err, "%s: failed to preload kernel "
			    "heap segment list", kd->core);
			free_seglist(kd, &tseg, NULL);
			goto error;
		}

		(void) memcpy(&kd->Ktextseg, &tseg, sizeof (struct seg));
		(void) memcpy(&kd->Kseg, &kvseg, sizeof (struct seg));

		kd->flags |= KVMD_SEGSLOADED;
	}

	return (kd);

error:
	(void) kvm_close(kd);
	return (NULL);
}

/*
 * Translate kernel virtual address in the range KERNELBASE
 * to econtig to a physical offset in the memory image
 */
u_longlong_t
__kvm_kvtop(kvm_t *kd, uintptr_t va)
{
	int	i;
	u_int	pagesize = kd->kvtophdr.pagesize;

	for (i = 0; i < kd->kvtophdr.nentries; i++) {
		struct kvtopent *ke = kd->kvtopmap + i;

		if ((uintptr_t)ke->kvpm_vaddr <= va &&
		    va < (uintptr_t)ke->kvpm_vaddr + ke->kvpm_len * pagesize) {
			return ((u_longlong_t)ke->kvpm_pfnum * pagesize
			    + (va - (uintptr_t)ke->kvpm_vaddr));
		}
	}

	return ((u_longlong_t)-1);	/* an "impossible" address */
}

/*
 * On a kernel core dump, readkvar() can only be used to read the
 * chunks by remapping the physical addresses through _uncondense().
 * Use readumphdr() to read the dump header.
 */
static int
readkvar(kvm_t *kd, u_longlong_t paddr, void *buf, size_t size,
    char *err, char *name)
{
	assert(kd->corefd > 0);
	if (_uncondense(kd, kd->corefd, &paddr)) {
		openperror(err, "%s: uncondense error on %s", kd->core, name);
		return (-1);
	}
	if (llseek(kd->corefd, (offset_t)paddr, L_SET) == -1) {
		openperror(err, "%s: seek error on %s", kd->core, name);
		return (-1);
	}
	if (read(kd->corefd, buf, size) != size) {
		openperror(err, "%s: read error on %s", kd->core, name);
		return (-1);
	}
	return (0);
}

static int
readkvirt(kvm_t *kd, uintptr_t vaddr, void *buf, size_t size,
    char *err, char *name)
{
	assert(kd->virtfd > 0);
	if (lseek(kd->virtfd, (off_t)vaddr, L_SET) == -1) {
		openperror(err, "%s: seek error on %s", kd->virt, name);
		return (-1);
	}
	if (read(kd->virtfd, buf, size) != size) {
		openperror(err, "%s: read error on %s", kd->virt, name);
		return (-1);
	}
	return (0);
}

static int
getkvtopdata(kvm_t *kd, char *err, char *name)
{
	u_longlong_t kvtopaddr = 0;
	struct dumphdr *dp;
	static struct nlist nl[] = {
		{ "kvtopdata" },
		{ "" }
	};
	size_t maplen;

	/*
	 * If the core image is a dump, we retrieve kvtopdata
	 * by using the physical address stored in the dump header.
	 * Otherwise, we simply nlist to get the virtual address.
	 * First, read the header...
	 */
	if (kd->virtfd == -1) {
		if (llseek(kd->corefd, (offset_t)0, L_SET) == -1) {
			__kvm_openerror(err,
			    "%s: seek error on dumphdr", kd->core);
			return (-1);
		}
		dp = (struct dumphdr *)mmap(0, sizeof (struct dumphdr),
		    PROT_READ, MAP_PRIVATE, kd->corefd, 0);
		if (dp == (struct dumphdr *)-1) {
			__kvm_openerror(err,
			    "%s: unable to locate kvtopdata", kd->core);
			return (-1);
		}
		kvtopaddr = dp->dump_kvtop_paddr;
		(void) munmap((caddr_t)dp, sizeof (struct dumphdr));

		if (readkvar(kd, kvtopaddr, &kd->kvtophdr,
		    sizeof (kd->kvtophdr), err, name) == -1) {
			__kvm_openerror(err, "%s: unable to read %s",
			    kd->core, name);
			return (-1);
		}
	} else {
		if (nlist(kd->name, nl) == -1) {
			__kvm_openerror(err,
			    "%s: unable to locate kvtopdata", kd->core);
			return (-1);
		}
		if (readkvirt(kd, nl[0].n_value, &kd->kvtophdr,
		    sizeof (kd->kvtophdr), err, "kvtophdr") == -1) {
			__kvm_openerror(err,
			    "%s: unable to read %s", kd->core, name);
			return (-1);
		}
	}

	/*
	 * Sanity check.
	 */
	if (kd->kvtophdr.version != KVTOPD_VER) {
		__kvm_openerror(err, "%s: corrupt %s", kd->core, name);
		return (-1);
	}

	/*
	 * Allocate space for the maps.
	 */
	maplen = kd->kvtophdr.nentries * sizeof (struct kvtopent);
	kd->kvtopmap = malloc(maplen);
	if (kd->kvtopmap == NULL) {
		__kvm_openerror(err, "%s: unable to allocate kvtopmaps",
		    kd->core);
		return (-1);
	}

	/*
	 * ... and now read the map entries.
	 */
	if (kd->virtfd == -1) {
		if (readkvar(kd, kvtopaddr + sizeof (struct kvtophdr),
		    kd->kvtopmap, maplen, err, name) == -1) {
			__kvm_openerror(err, "%s: unable to read kvtopmaps",
			    kd->core);
			return (-1);
		}
	} else {
		if (readkvirt(kd, nl[0].n_value + sizeof (struct kvtophdr),
		    kd->kvtopmap, maplen, err, "kvtopmap") == -1) {
			__kvm_openerror(err, "%s: unable to read kvtopmap",
			    kd->core, name);
			return (-1);
		}
	}
	return (0);
}

static int
__kvm_getkvar(kvm_t *kd, uintptr_t kaddr, void *buf, size_t size, char *err,
    char *name)
{
	u_longlong_t paddr;

	paddr = __kvm_kvtop(kd, kaddr);
	return (readkvar(kd, paddr, buf, size, err, name));
}

static int
condensed_setup(int fd, struct condensed **cdpp, char *name, char *err)
{
	struct dumphdr *dp = (struct dumphdr *)-1;
	struct condensed *cdp;
	char *bitmap = (char *)-1;
	off_t *atoo = NULL;
	off_t offset;
	long bitmapsize;
	long pagesize;
	long chunksize;
	long nchunks;
	int i;
	int rc = 0;

	*cdpp = cdp = NULL;

	/* See if beginning of file is a dumphdr */
	dp = (struct dumphdr *)mmap((caddr_t)0, sizeof (struct dumphdr),
	    PROT_READ, MAP_PRIVATE, fd, (off_t)0);

	if (dp == (struct dumphdr *)-1) {
		/* This is okay; assume not condensed */
		KVM_ERROR_2("cannot mmap %s; assuming non_condensed", name);
		goto not_condensed;
	}
	if (dp->dump_magic != DUMP_MAGIC ||
	    (dp->dump_flags & DF_VALID) == 0) {
		/* not condensed */
		goto not_condensed;
	}

	if ((dp->dump_flags & DF_COMPLETE) == 0) {
		__kvm_openerror(err, "%s is not complete, some kernel "
		    "data may not be available.", name);
	}

	pagesize = dp->dump_pagesize;

	/*
	 * Get the bitmap, which begins at the second page in the dumpfile.
	 * We can't simply start the mmap() there, however, because
	 * dump_pagesize may not be a multiple of the current pagesize.
	 * Therefore we start the mapping at offset 0 and add
	 * dump_pagesize to whatever mmap() gives us.
	 */
	bitmapsize = dp->dump_bitmapsize;
	bitmap = (char *)mmap((caddr_t)0, pagesize + bitmapsize, PROT_READ,
	    MAP_PRIVATE, fd, (off_t)0);
	if (bitmap == (caddr_t)-1) {
		openperror(err, "cannot mmap %s's bitmap", name);
		goto error;
	}
	bitmap += pagesize;

	/* allocate the address to offset table */
	if ((atoo = calloc(bitmapsize * NBBY, sizeof (*atoo))) == NULL) {
		openperror(err, "cannot allocate %s's uncondensing table",
		    name);
		goto error;
	}

	/* allocate the condensed structure */
	if ((cdp = malloc(sizeof (struct condensed))) == NULL) {
		openperror(err, "cannot allocate %s's struct condensed",
		    name);
		goto error;
	}
	cdp->cd_atoo = atoo;
	cdp->cd_atoosize = bitmapsize * NBBY;
	cdp->cd_dp = dp;
	*cdpp = cdp;

	cdp->cd_chunksize = chunksize = pagesize * dp->dump_chunksize;
	offset = pagesize + roundup(bitmapsize, pagesize);
	nchunks = dp->dump_nchunks;

	for (i = 0; nchunks > 0; i++) {
		if (isset(bitmap, i)) {
			atoo[i] = offset;
			offset += chunksize;
			nchunks--;
		}
	}

	goto done;

error:
	rc = -1;
not_condensed:
	if (cdp) {
		free(cdp);
		*cdpp = NULL;
	}
	if (dp != (struct dumphdr *)-1)
		(void) munmap((caddr_t)dp, sizeof (*dp));
	if (atoo)
		free(atoo);
done:
	if (bitmap != (caddr_t)-1)
		(void) munmap(bitmap - pagesize, pagesize + bitmapsize);

	return (rc);
}

static void
condensed_takedown(struct condensed *cdp)
{
	struct dumphdr *dp;
	off_t *atoo;

	if (cdp) {
		if ((dp = cdp->cd_dp) != (struct dumphdr *)-1) {
			(void) munmap((caddr_t)dp, sizeof (*dp));
		}
		if ((atoo = cdp->cd_atoo) != NULL) {
			free(atoo);
		}
		free(cdp);
	}
}

/*
 * Close files associated with libkvm
 */
int
kvm_close(kvm_t *kd)
{
	struct swapinfo *sn;

	if (kd->proc != NULL)
		(void) kvm_setproc(kd);	/* rewind process table state */
	if (kd->namefd != -1)
		(void) close(kd->namefd);
	if (kd->corefd != -1)
		(void) close(kd->corefd);
	if (kd->virtfd != -1)
		(void) close(kd->virtfd);
	if (kd->swapfd != -1)
		(void) close(kd->swapfd);
	if (kd->pbuf != NULL)
		free(kd->pbuf);
	if (kd->sbuf != NULL)
		free(kd->sbuf);
	if (kd->kvtopmap != NULL)
		free(kd->kvtopmap);
	while ((sn = kd->sip) != NULL) {
		if (sn->si_hint != -1)
			(void) close(sn->si_hint);
		kd->sip = sn->si_next;
		free(sn->si_pname);
		free(sn);
	}

	condensed_takedown(kd->corecdp);
	condensed_takedown(kd->swapcdp);

	if (kd->kvm_param != NULL)
		free(kd->kvm_param);

	if (kd->flags & KVMD_SEGSLOADED) {
		free_seglist(kd, &kd->Ktextseg, NULL);
		free_seglist(kd, &kd->Kseg, NULL);
	}

	free(kd);
	return (0);
}

/*
 * Reset the position pointers for kvm_nextproc().
 * This routine is here so that kvmnextproc.o isn't always included.
 */
int
kvm_setproc(kvm_t *kd)
{
	proc_t *pract;
	/*
	 * Need to reread practive because processes could
	 * have been added to the table since the kvm_open() call.
	 * This addresses bugid #1117308.
	 */
	(void) kvm_kread(kd, (uintptr_t)kd->practp, &pract, sizeof (pract));
	kd->proc = pract;

	kd->pnext = kd->proc;		/* rewind the position ptr */
	return (0);
}

/*
 * __kvm_openerror - print error messages for kvm_open() if prefix is non-NULL
 */
/*VARARGS*/
void
__kvm_openerror(va_alist)
	va_dcl
{
	va_list args;
	char *prefix, *fmt;

	va_start(args);

	prefix = va_arg(args, char *);
	if (prefix == NULL)
		goto done;

	(void) fprintf(stderr, "%s: ", prefix);
	fmt = va_arg(args, char *);
	(void) vfprintf(stderr, fmt, args);
	(void) putc('\n', stderr);

done:
	va_end(args);
}

/*VARARGS*/
static void
openperror(va_alist)
	va_dcl
{
	va_list args;
	char *prefix, *fmt;

	va_start(args);

	prefix = va_arg(args, char *);
	if (prefix == NULL)
		goto done;

	(void) fprintf(stderr, "%s: ", prefix);
	fmt = va_arg(args, char *);
	(void) vfprintf(stderr, fmt, args);
	(void) fprintf(stderr, ": ");
	perror("");

done:
	va_end(args);
}

/* debugging routines */
#ifdef _KVM_DEBUG

int _kvm_debug = _KVM_DEBUG;

/*VARARGS*/
void
_kvm_error(va_alist)
	va_dcl
{
	va_list args;
	char *fmt;

	if (!_kvm_debug)
		return;

	va_start(args);

	(void) fprintf(stderr, "libkvm: ");
	fmt = va_arg(args, char *);
	(void) vfprintf(stderr, fmt, args);
	(void) putc('\n', stderr);

	va_end(args);
}

/*VARARGS*/
void
_kvm_perror(va_alist)
	va_dcl
{
	va_list args;
	char *fmt;

	if (!_kvm_debug)
		return;

	va_start(args);

	(void) fprintf(stderr, "libkvm: ");
	fmt = va_arg(args, char *);
	(void) vfprintf(stderr, fmt, args);
	perror("");

	va_end(args);
}

#endif _KVM_DEBUG
