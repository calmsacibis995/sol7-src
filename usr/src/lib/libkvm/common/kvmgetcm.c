/*
 * Copyright (c) 1987, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kvmgetcmd.c	2.57	98/02/22 SMI"

#define	_SYSCALL32

#include "kvm_impl.h"
#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/vmparam.h>
#include <vm/as.h>
#include <vm/seg_vn.h>
#include <vm/seg_map.h>
#include <vm/anon.h>
#include <vm/page.h>
#include <sys/swap.h>
#define	_KERNEL
#include <vm/seg_kp.h>
#undef	_KERNEL
#include <vm/seg_spt.h>
#include <sys/sysmacros.h>	/* for MIN() */

#define	KVM_PAGE_HASH_FUNC(kd, vp, off) \
	((((off) >> (kd)->pageshift) + \
	((uintptr_t)(vp) >> PAGE_HASHVPSHIFT)) & ((kd)->page_hashsz - 1))

#define	KVM_SEGKP_HASH(kd, vaddr) \
	(((uintptr_t)(vaddr) >> (kd)->pageshift) & SEGKP_HASHMASK)

static u_longlong_t page_to_physaddr(kvm_t *, struct page *);
static int anon_to_fdoffset(kvm_t *,
    struct anon *, int *, u_longlong_t *, u_long);
static int vp_to_fdoffset(kvm_t *,
    struct vnode *, int *, u_longlong_t *, offset_t);
static offset_t _anonoffset(kvm_t *, struct anon *, struct vnode **,
    offset_t *);

/*
 * VERSION FOR MACHINES WITH STACKS GROWING DOWNWARD IN MEMORY
 *
 * On program entry, the top of the stack frame looks like this:
 *
 * hi:	|-----------------------|
 *	|	unspecified	|
 *	|-----------------------|+
 *	|	   :		| \
 *	|  arg and env strings	|  > no more than NCARGS bytes
 *	|	   :		| /
 *	|-----------------------|+
 *	|	unspecified	|
 *	|-----------------------|
 *	| null auxiliary vector	|
 *	|-----------------------|
 *	|   auxiliary vector	|
 *	|   (2-word entries)	|
 *	|	   :		|
 *	|-----------------------|
 *	|	(char *)0	|
 *	|-----------------------|
 *	|  ptrs to env strings	|
 *	|	   :		|
 *	|-----------------------|
 *	|	(char *)0	|
 *	|-----------------------|
 *	|  ptrs to arg strings	|
 *	|   (argc = # of ptrs)	|
 *	|	   :		|
 *	|-----------------------|
 *	|	  argc		|
 * low:	|-----------------------|
 */

static int getseg(kvm_t *, struct seg *, caddr_t, struct seg *);
static struct page *pagefind(kvm_t *, struct page *, struct vnode *, offset_t);
static int kvm_getcmd32(kvm_t *,
    struct proc *, struct user *, char ***, char ***);

/*
 * reconstruct an argv-like argument list from the target process
 */
int
kvm_getcmd(kvm_t *kd,
    struct proc *proc, struct user *u, char ***arg, char ***env)
{
	size_t asize;
	size_t esize;
	size_t offset;
	int i;
	int argc;
	char **argv = NULL;
	char **envp = NULL;
	char *str;
	char *last_str;
	char *argv_null;	/* Known null in the returned argv */
	char *envp_null;	/* Known null in the returned envp */

	if (proc->p_flag & SSYS)	/* system process */
		return (-1);

	/*
	 * If this is a 32-bit process running on a 64-bit system,
	 * then the stack is laid out using ILP32 pointers, not LP64.
	 * To minimize potential confusion, we blow it up to "LP64
	 * shaped" right here.
	 */
	if (proc->p_model != DATAMODEL_NATIVE &&
	    proc->p_model == DATAMODEL_ILP32)
		return (kvm_getcmd32(kd, proc, u, arg, env));

	/*
	 * Space for the stack, from the argument vector.  An additional
	 * word is added to guarantee a NULL word terminates the buffer.
	 */
	if (arg) {
		asize = (size_t)proc->p_usrstack - (size_t)u->u_argv;
		if ((argv = malloc(asize + sizeof (uintptr_t))) == NULL) {
			KVM_ERROR_1("kvm_getcmd: malloc() failure");
			return (-1);
		}
		argv_null = (char *)argv + asize;
		*(uintptr_t *)argv_null = 0;
	}

	/*
	 * Space for the stack, from the environment vector.  An additional
	 * word is added to guarantee a NULL word terminates the buffer.
	 */
	if (env) {
		esize = (size_t)proc->p_usrstack - (size_t)u->u_envp;
		if ((envp = malloc(esize + sizeof (uintptr_t))) == NULL) {
			KVM_ERROR_1("kvm_getcmd: malloc() failure");
			if (argv)
				free(argv);
			return (-1);
		}
		envp_null = (char *)envp + esize;
		*(uintptr_t *)envp_null = 0;
	}

	argc = u->u_argc;

	if (argv) {
		/* read the whole initial stack */
		if (kvm_uread(kd,
		    (uintptr_t)u->u_argv, argv, asize) != asize) {
			KVM_ERROR_1("kvm_getcmd: kvm_uread() failure");
			free(argv);
			if (envp)
				free(envp);
			return (-1);
		}
		argv[argc] = 0;
		if (envp) {
			/*
			 * Copy it to the malloc()d space for the envp array
			 */
			(void) memcpy(envp, &argv[argc + 1], esize);
		}
	} else if (envp) {
		/* read most of the initial stack (excluding argv) */
		if (kvm_uread(kd,
		    (uintptr_t)u->u_envp, envp, esize) != esize) {
			KVM_ERROR_1("kvm_getcmd: kvm_uread() failure");
			free(envp);
			return (-1);
		}
	}

	/*
	 * Relocate and sanity check the argv array.  Entries which have
	 * been explicity nulled are left that way.  Entries which have
	 * been replaced are pointed to a null string.  Well behaved apps
	 * don't do any of this.
	 */
	if (argv) {
		/* relocate the argv[] addresses */
		offset = (char *)argv - (char *)u->u_argv;
		for (i = 0; i < argc; i++) {
			if (argv[i] != NULL) {
				str = (argv[i] += offset);
				if (str < (char *)argv ||
				    str >= (char *)argv + asize)
					argv[i] = argv_null;
			}
		}
		argv[i] = NULL;
		*arg = argv;
	}

	/*
	 * Relocate and sanity check the envp array.  A null entry indicates
	 * the end of the environment.  Entries which point outside of the
	 * initial stack are replaced with what must have been the initial
	 * value based on the known ordering of the string table by the
	 * kernel.  If stack corruption prevents the calculation of the
	 * location of an initial string value, a pointer to a null string
	 * is returned.  To return a null pointer would prematurely terminate
	 * the list.  Well behaved apps do set pointers outside of the
	 * initial stack via the putenv(3C) library routine.
	 */
	if (envp) {

		/*
		 * Determine the start of the environment strings as one
		 * past the last argument string.
		 */
		offset = (char *)envp - (char *)u->u_envp;

		if (kvm_uread(kd,
		    (uintptr_t)u->u_argv + (argc - 1) * sizeof (char **),
		    &last_str, sizeof (last_str)) != sizeof (last_str))
			last_str = envp_null;
		else {
			last_str += offset;
			if (last_str < (char *)envp ||
			    last_str >= (char *)envp + esize)
				last_str = envp_null;
		}

		/*
		 * Relocate the envp[] addresses, while ensuring that we
		 * don't return bad addresses.
		 */
		for (i = 0; envp[i] != NULL; i++) {
			str = (envp[i] += offset);
			if (str < (char *)envp || str >= (char *)envp + esize) {
				if (last_str != envp_null)
					envp[i] = last_str +
					    strlen(last_str) + 1;
				else
					envp[i] = envp_null;
			}
			last_str = envp[i];
		}
		envp[i] = NULL;
		*env = envp;
	}

	return (0);
}

#define	RoundUp(v, t)	(((v) + sizeof (t) - 1) & ~(sizeof (t) - 1))

static int
kvm_getcmd32(kvm_t *kd,
    struct proc *p, struct user *u, char ***arg, char ***env)
{
#if defined(_LP64) || defined(lint)
	size_t size32;
	void *stack32;
	int i, argc, envc, auxc;
	size_t asize, esize;
	char **argv = NULL;
	char **envp = NULL;
	size_t strpoolsz;
	int aptrcount;
	int eptrcount;
	caddr_t stackp;
	ptrdiff_t reloc;
	char *str;

	assert((p->p_flag & SSYS) == 0 &&
	    p->p_model != DATAMODEL_NATIVE &&
	    p->p_model == DATAMODEL_ILP32);

	/*
	 * Bring the entire stack into memory first, size it
	 * as an LP64 user stack, then allocate and copy into
	 * the buffer(s) to be returned to the caller.
	 */
	size32 = (size_t)p->p_usrstack - (size_t)u->u_argv;
	if ((stack32 = malloc(size32)) == NULL) {
		KVM_ERROR_1("kvm_getcmd32: malloc() failure");
		return (-1);
	}
	if (kvm_uread(kd, (uintptr_t)u->u_argv, stack32, size32) != size32) {
		KVM_ERROR_1("kvm_getcmd32: kvm_uread() failure");
		free(stack32);
		return (-1);
	}

	/*
	 * Find the interesting sizes of a 32-bit stack.
	 */
	argc = u->u_argc;
	stackp = (caddr_t)stack32 + ((1 + argc) * sizeof (caddr32_t));

	for (envc = 0; *(caddr32_t *)stackp; envc++) {
		stackp += sizeof (caddr32_t);
		if ((stackp - (caddr_t)stack32) >= size32) {
			free(stack32);
			return (-1);
		}
	}

	if (u->u_auxv[0].a_type != AT_NULL) {
		stackp += sizeof (caddr32_t);
		for (auxc = 0; *(int32_t *)stackp; auxc++) {
			stackp += 2 * sizeof (caddr32_t);
			if ((stackp - (caddr_t)stack32) >= size32) {
				free(stack32);
				return (-1);
			}
		}
		auxc++;		/* terminating AT_NULL record */
	}

	/*
	 * Compute the sizes of the stuff we're going to allocate or copy.
	 */
	eptrcount = (envc + 1) + 2 * auxc;
	aptrcount = (argc + 1) + eptrcount;
	strpoolsz = size32 - aptrcount * sizeof (caddr32_t);

	asize = aptrcount * sizeof (uintptr_t) + RoundUp(strpoolsz, uintptr_t);
	if (arg && (argv = calloc(1, asize + sizeof (uintptr_t))) == NULL) {
		KVM_ERROR_1("kvm_getcmd32: malloc() failure");
		free(stack32);
		return (-1);
	}

	esize = eptrcount * sizeof (uintptr_t) + RoundUp(strpoolsz, uintptr_t);
	if (env && (envp = calloc(1, esize + sizeof (uintptr_t))) == NULL) {
		KVM_ERROR_1("kvm_getcmd32: malloc() failure");
		if (argv)
			free(argv);
		free(stack32);
		return (-1);
	}

	/*
	 * Walk up the 32-bit stack, filling in the 64-bit argv and envp
	 * as we go.
	 */
	stackp = (caddr_t)stack32;

	/*
	 * argument vector
	 */
	if (argv) {
		for (i = 0; i < argc; i++) {
			argv[i] = (char *)(*(caddr32_t *)stackp);
			stackp += sizeof (caddr32_t);
		}
		argv[argc] = 0;
		stackp += sizeof (caddr32_t);
	} else
		stackp += (1 + argc) * sizeof (caddr32_t);

	/*
	 * environment
	 */
	if (envp) {
		for (i = 0; i < envc; i++) {
			envp[i] = (char *)(*(caddr32_t *)stackp);
			stackp += sizeof (caddr32_t);
		}
		envp[envc] = 0;
		stackp += sizeof (caddr32_t);
	} else
		stackp += (1 + envc) * sizeof (caddr32_t);

	/*
	 * auxiliary vector (skip it..)
	 */
	stackp += auxc * (sizeof (int32_t) + sizeof (uint32_t));

	/*
	 * Copy the string pool, untranslated
	 */
	if (argv)
		(void) memcpy(argv + aptrcount, (void *)stackp, strpoolsz);
	if (envp)
		(void) memcpy(envp + eptrcount, (void *)stackp, strpoolsz);

	free(stack32);

	/*
	 * Relocate the pointers to point at the newly allocated space.
	 * Use the same algorithms as kvm_getcmd to handle naughty
	 * changes to the argv and envp arrays.
	 */
	if (argv) {
		char *argv_null = (char *)argv + asize;

		reloc = (char *)(argv + aptrcount) - (char *)
		    ((caddr_t)u->u_argv + aptrcount * sizeof (caddr32_t));

		for (i = 0; i < argc; i++)
			if (argv[i] != NULL) {
				str = (argv[i] += reloc);
				if (str < (char *)argv ||
				    str >= (char *)argv + asize)
					argv[i] = argv_null;
			}

		*arg = argv;
	}

	if (envp) {
		char *envp_null = (char *)envp + esize;
		char *last_str;

		reloc = (char *)(envp + eptrcount) - (char *)
		    ((caddr_t)u->u_envp + eptrcount * sizeof (caddr32_t));

		last_str = (char *)((size_t)u->u_argv +
		    (1 + argc) * sizeof (caddr32_t) + reloc);
		if (last_str < (char *)envp ||
		    last_str >= (char *)envp + esize)
			last_str = envp_null;

		for (i = 0; i < envc; i++) {
			str = (envp[i] += reloc);
			if (str < (char *)envp ||
			    str >= (char *)envp + esize) {
				if (last_str != envp_null)
					envp[i] = (char *)((size_t)last_str +
					    strlen(last_str) + 1);
				else
					envp[i] = envp_null;
			}
			last_str = envp[i];
		}
		*env = envp;
	}
#endif	/* _LP64 || lint */
	return (0);
}

/*
 * Given a seg structure, find the appropriate seg for a given address.
 * This function is similar to getseg() below, but is called when oseg
 * refers to one of our in-core segments.  This means that s_as and s_next
 * refer to in-core copies of structures we have read from the crash dump
 * previously.
 */
static int
getseg_incore(kvm_t *kd, struct seg *segp, caddr_t addr, struct seg *nseg)
{
	if (addr < segp->s_base) {
		do {
			if (segp->s_prev != NULL)
				segp = segp->s_prev;
			else
				goto noseg;

		} while (addr < nseg->s_base);

		if (addr >= (segp->s_base + segp->s_size))
			goto noseg;

	} else if (addr >= (segp->s_base + segp->s_size)) {
		do {
			if (segp->s_as->a_lrep == AS_LREP_LINKEDLIST)
				segp = segp->s_next.list;
			else
				segp = segp->s_next.skiplist->segs[0];

			if (segp == NULL)
				goto noseg;

		} while (addr >= (segp->s_base + segp->s_size));

		if (addr < segp->s_base)
			goto noseg;
	}

	if (nseg != segp)
		(void) memcpy(nseg, segp, sizeof (struct seg));

	return (0);

noseg:
	KVM_ERROR_2("can't find segment for user address %p", addr);
	return (-1);
}


/*
 * given a seg structure, find the appropriate seg for a given address
 * (nseg may be identical to oseg)
 */
static int
getseg(kvm_t *kd, struct seg *oseg, caddr_t addr, struct seg *nseg)
{
	if ((kd->flags & KVMD_SEGSLOADED) &&
	    (oseg == &kd->Ktextseg || oseg == &kd->Kseg))
		return (getseg_incore(kd, oseg, addr, nseg));

	if (addr < oseg->s_base) {
		do {
			if (oseg->s_prev == NULL)
				goto noseg;
			GETKVM(oseg->s_prev, nseg, "prev segment descriptor");
			oseg = nseg;
		} while (addr < nseg->s_base);
		if (addr >= (nseg->s_base + nseg->s_size))
			goto noseg;

	} else if (addr >= (oseg->s_base + oseg->s_size)) {
		struct as xas;

		GETKVM(oseg->s_as, &xas, "segment's address space");
		do {
			struct seg *next;

			if (xas.a_lrep == AS_LREP_LINKEDLIST)
				next = oseg->s_next.list;
			else {
				seg_skiplist ssl;

				GETKVM(oseg->s_next.skiplist, &ssl,
				    "segment skiplist structure");
				next = ssl.segs[0];
			}
			if (next == NULL)
				goto noseg;
			GETKVM(next, nseg, "next segment descriptor");
			oseg = nseg;
		} while (addr >= nseg->s_base + nseg->s_size);
		if (addr < nseg->s_base)
			goto noseg;

	} else if (nseg != oseg) {
		*nseg = *oseg;		/* copy if necessary */
	}
	return (0);

noseg:
	KVM_ERROR_2("can't find segment for user address %p", addr);
	return (-1);
}

static int
_kvm_get_ap(kvm_t *kd, struct anon_hdr *hdrp, struct anon **ap, ulong_t an_idx)
{
	struct anon_hdr *ahp;
	void		**achp;
	size_t		asize;

	/*
	 * XXX ugly
	 * Allow anon macros that use PAGESIZE and PAGESHIFT
	 * to work. PAGESIZE and PAGESHIFT are actually
	 * #define's of _pagesize and _pageshift.
	 */
	const unsigned long _pagesize = kd->pagesz;
	const unsigned long _pageshift = kd->pageshift;

	asize = sizeof (struct anon_hdr);
	if ((ahp = malloc(asize)) == NULL) {
		KVM_ERROR_1("no memory for anon header array");
		return (-1);
	}
	/* read anon array hdr */
	if (kvm_kread(kd, (uintptr_t)hdrp, ahp, asize) != asize) {
		KVM_ERROR_1("error reading anon array hdr");
		free(ahp);
		return (-1);
	}

	if (an_idx > ahp->size) {
		KVM_ERROR_1("error anon idx outside anon array");
		free(ahp);
		return (-1);
	}

	if ((ahp->size <= ANON_CHUNK_SIZE) || (ahp->flags & ANON_ALLOC_FORCE)) {
		asize = ahp->size * sizeof (struct anon *);
		if ((achp = malloc(asize)) == NULL) {
			KVM_ERROR_1("no memory for anon array");
			free(ahp);
			return (-1);
		}
		if (kvm_kread(kd, (uintptr_t)ahp->array_chunk, achp,
						asize) != asize) {
			KVM_ERROR_1("error reading anon chunk array");
			free(achp);
			free(ahp);
			return (-1);
		}
		*ap = achp[an_idx];
		free(achp);
		free(ahp);
		return (0);
	} else {
		void *p;
		ulong_t nchunks;

		nchunks = (ahp->size + ANON_CHUNK_OFF) >> ANON_CHUNK_SHIFT;
		asize = nchunks * sizeof (void *);
		if ((achp = malloc(asize)) == NULL) {
			KVM_ERROR_1("no memory for anon chunk array");
			free(ahp);
			return (-1);
		}

		if (kvm_kread(kd, (uintptr_t)ahp->array_chunk, achp,
						asize) != asize) {
			KVM_ERROR_1("error reading anon chunk array");
			free(achp);
			free(ahp);
			return (-1);
		}

		if ((p = achp[an_idx >> ANON_CHUNK_SHIFT]) != NULL) {
			free(achp);
			free(ahp);
			if ((achp = malloc(ANON_CHUNK_SIZE)) == NULL) {
				KVM_ERROR_1("no memory for anon chunk array");
				return (-1);
			}
			if (kvm_kread(kd, (uintptr_t)p, achp,
			    ANON_CHUNK_SIZE) != ANON_CHUNK_SIZE) {
				KVM_ERROR_1("error reading anon chunk array");
				free(achp);
				return (-1);
			}
			*ap = achp[an_idx & ANON_CHUNK_OFF];
			free(achp);
			return (0);
		} else {
			free(achp);
			free(ahp);
			*ap = NULL;
			return (0);
		}
	}
}

/*
 * Return the fd and offset for the address in segkp.
 */
static int
getsegkp(
	kvm_t *kd,
	struct seg *seg,
	caddr_t vaddr,
	int *fdp,
	u_longlong_t *offp)
{
	struct segkp_segdata segdata;
	int index;
	int stop;
	struct segkp_data **hash;
	struct segkp_data *head;
	struct segkp_data segkpdata;
	struct anon_hdr *ahp;
	struct anon *anonq;
	uintptr_t i;
	u_long off;
	int retval = 0;

	off = (uintptr_t)vaddr & kd->pageoffset;
	vaddr = (caddr_t)((uintptr_t)vaddr & kd->pagemask);

	/* get private data for segment */
	if (seg->s_data == NULL) {
		KVM_ERROR_1("getsegkp: NULL segkp_data ptr in segment");
		return (-1);
	}

	GETKVM(seg->s_data, &segdata, "segkp_data");

	/*
	 * find kpd associated with the virtual address:
	 */
	if (segdata.kpsd_hash == NULL) {
		KVM_ERROR_1("getsegkp: NULL kpsd_hash ptr in segment");
		return (-1);
	}

	if ((hash = malloc(SEGKP_HASHSZ *
	    sizeof (struct segkp_data **))) == NULL) {
		KVM_ERROR_1("getsegkp: can't allocate hash");
		return (-1);
	}

	if (kvm_kread(kd, (uintptr_t)segdata.kpsd_hash, hash,
	    SEGKP_HASHSZ * sizeof (struct segkp_data **)) !=
	    SEGKP_HASHSZ * sizeof (struct segkp_data **))
		KVM_ERROR_1("getsegkp: can't read hash table");

	index = stop = KVM_SEGKP_HASH(kd, vaddr);
	do {
		for (head = hash[index]; head; head = segkpdata.kp_next) {

			GETKVM(head, &segkpdata, "head");
			if (vaddr >= segkpdata.kp_base &&
			    vaddr <= segkpdata.kp_base + segkpdata.kp_len)
				goto found_it;
		}
		if (--index < 0)
			index = SEGKP_HASHSZ - 1; /* wrap */
	} while (index != stop);

	KVM_ERROR_2("getsegkp: can't find segment: vaddr %p", vaddr);

	retval = -1;
	goto exit_getsegkp;

found_it:

	/* OK, we have the kpd */

	GETKVM(head, &segkpdata, "head");	/* entry head pts to */
	if ((segkpdata.kp_flags & KPD_NO_ANON) == 0 &&
	    segkpdata.kp_anon == NULL) {
		KVM_ERROR_1("getsegkp: KPD_NO_ANON, NULL anon ptr in segment");
		retval = -1;
		goto exit_getsegkp;
	}

	i = (vaddr - segkpdata.kp_base) / (int)kd->pagesz;

	if (segkpdata.kp_flags & KPD_NO_ANON) {
		u_longlong_t phyadd;
		struct page *pp;
		struct page page;

		if ((pp = pagefind(kd, &page, kd->kvp,
		    (offset_t)(uintptr_t)vaddr)) == NULL) {
			KVM_ERROR_1("getsegkp: can't read page array");
			retval = -1;
			goto exit_getsegkp;
		}
		phyadd = page_to_physaddr(kd, pp);

		*fdp = kd->corefd;
		*offp = phyadd + off;
		retval = 0;
		goto exit_getsegkp;
	} else {
		ahp = segkpdata.kp_anon;

		if (ahp == NULL) {
			KVM_ERROR_1("getsegkp: NULL ahp ptr in segment");
			retval = 0;
			goto exit_getsegkp;
		}

		/* now get the anon table entry */

		if (_kvm_get_ap(kd, ahp, &anonq, segkpdata.kp_anon_idx + i)) {
			KVM_ERROR_1("getsegkp: can not get anon in segment");
			retval = -1;
			goto exit_getsegkp;
		}
		retval = anon_to_fdoffset(kd, anonq, fdp, offp, off);
		goto exit_getsegkp;
	}

exit_getsegkp:
	free(hash);
	return (retval);
}

/*
 * hashed lookup to see if a page exists for a given vnode
 * returns address of page structure in 'page', or NULL if error.
 */
static struct page *
pagefind(kvm_t *kd, struct page *page, struct vnode *vp, offset_t off)
{
	struct page *pp;

	GETKVMNULL((kd->page_hash + KVM_PAGE_HASH_FUNC(kd, vp, off)), &pp,
	    "initial hashed page struct ptr");
	while (pp != NULL) {
		GETKVMNULL(pp, page, "hashed page structure");
		if ((page->p_vnode == vp) && (page->p_offset == off)) {
			return (pp);
		}
		pp = page->p_hash;
	}
	return (NULL);
}

/*
 * Used directly by savecore(1m), under '#ifdef SAVESWAP'
 *
 * Convert a pointer into an anon array into an offset (in bytes)
 * into the swap file. For live systems the file descriptor for the associated
 * swapfile is returned in kd->swapfd, and the offset is into that file.
 * Since each individual swap file has a separate swapinfo structure, we cache
 * the linked list of swapinfo structures in order to do this calculation
 * faster.  For dead systems the offset is an offset into a virtual swapfile
 * formed by concatenating all the real files. Also, save the
 * real vp and offset for the vnode that contains this page.
 */
static offset_t
_anonoffset(kvm_t *kd, struct anon *ap, struct vnode **vp, offset_t *vpoffset)
{
	struct swapinfo *sip;
	struct anon anon;

	sip = kd->sip;

	/*
	 * First time through, read in all the swapinfo structures.
	 * Open each swap file and store the fd in si_hint.
	 * Re-use the si_allocs field to store swap offset. This will
	 * only be used on a dump with a dead swapfile.
	 */
	if (sip == NULL) {
		struct swapinfo **spp;
		struct swapinfo *sn;
		ssize_t soff = 0;
		char *pname;

		sn = kd->swapinfo;
		spp = &kd->sip;
		for (; sn != NULL; spp = &(*spp)->si_next, sn = *spp) {
			*spp = malloc(sizeof (*sn));
			if (*spp == NULL) {
				KVM_PERROR_1("no memory for swapinfo");
				break;
			}
			if (kvm_kread(kd, (uintptr_t)sn, *spp, sizeof (*sn))
			    != sizeof (*sn)) {
				KVM_ERROR_1("error reading swapinfo");
				free(*spp);
				break;
			}
			pname = (*spp)->si_pname;
			(*spp)->si_pname = malloc((*spp)->si_pnamelen + 10);
			if ((*spp)->si_pname == NULL) {
				KVM_PERROR_1("no memory for swapinfo");
				break;
			}
			if (kvm_kread(kd, (uintptr_t)pname,
			    (*spp)->si_pname, (*spp)->si_pnamelen)
			    != (*spp)->si_pnamelen) {
				KVM_ERROR_1("error reading swapinfo");
				break;
			}
			(*spp)->si_hint = open((*spp)->si_pname, O_RDONLY, 0);
			if ((*spp)->si_hint == -1) {
				KVM_ERROR_2("can't open swapfile %s",
					(*spp)->si_pname);
			}
			(*spp)->si_allocs = soff;
			soff += (((*spp)->si_eoff - (*spp)->si_soff)
				>> kd->pageshift);
		}
		*spp = NULL;		/* clear final 'next' pointer */
		sip = kd->sip;		/* reset list pointer */
	}
	GETKVM(ap, &anon, "anon structure");

	/*
	 * If the anon slot has no physical vnode for backing store just
	 * return the (an_vp,an_off) that gives the name of the page. Return
	 * the swapfile offset as 0. This offset will never be used, as the
	 * caller will always look for and find the page first, thus it will
	 * never try the swapfile.
	 */
	if (anon.an_pvp == NULL) {
		*vp = anon.an_vp;
		*vpoffset = anon.an_off;
		kd->swapfd = -1;
		kd->swap = NULL;
		return ((offset_t)0);
	}

	/*
	 * If there is a physical backing store vnode, find the
	 * corresponding swapinfo. If this is a live system, set the
	 * swapfd in the kvm to be the swapfile for this swapinfo.
	 */
	for (; sip != NULL; sip = sip->si_next) {
		if ((anon.an_pvp == sip->si_vp) &&
		    (anon.an_poff >= sip->si_soff) &&
		    (anon.an_poff <= sip->si_eoff)) {
			*vp = anon.an_vp;
			*vpoffset = anon.an_off;
			if (strcmp(kd->core, LIVE_COREFILE) == 0) {
				kd->swapfd = sip->si_hint;
				kd->swap = sip->si_pname;
				return (anon.an_poff);
			} else {
				return (((offset_t)sip->si_allocs <<
				    kd->pageshift) + *vpoffset);
			}
		}
	}
	KVM_ERROR_1("can't find anon ptr in swapinfo list");
	return ((offset_t)-1);
}

/*
 * Used directly by crash(1m).
 *
 * Find physical address correspoding to an address space/offset
 * Returns -1 if address does not correspond to any physical memory
 */
u_longlong_t
kvm_physaddr(kvm_t *kd, struct as *as, uintptr_t vaddr)
{
	int fd;
	u_longlong_t off;

	(void) _kvm_physaddr(kd, as, vaddr, &fd, &off);
	return ((fd == kd->corefd) ? off : ((u_longlong_t)-1));
}

/*
 * internal interface that also finds swap offset if any fd is
 * either kd->corefd if in core, kd->swapfd if in swap or -1
 * if nowhere
 */
int
_kvm_physaddr(
	kvm_t *kd,
	struct as *as,
	uintptr_t vaddr,
	int *fdp,
	u_longlong_t *offp)
{
	struct seg s, *seg, *fseg;

	*fdp = -1;
	/* get first seg structure */
	seg = &s;
	if (as->a_segs.list == kd->Kas.a_segs.list) {
		fseg = (vaddr < (uintptr_t)kd->Kseg.s_base) ?
		    &kd->Ktextseg : &kd->Kseg;
	} else {
		fseg = &s;
		if (as->a_lrep == AS_LREP_LINKEDLIST) {
			GETKVM(as->a_segs.list, fseg,
			    "1st user segment descriptor");
		} else {
			seg_skiplist ssl;

			GETKVM(as->a_segs.skiplist, &ssl,
			    "1st segment skiplist structure");
			GETKVM(ssl.segs[0], fseg,
			    "1st user segment descriptor");
		}
	}

	/* Make sure we've got the right seg structure for this address */
	if (getseg(kd, fseg, (caddr_t)vaddr, seg) == -1)
		return (-1);

	if (seg->s_ops == kd->segvn) {
		/* Segvn segment */
		struct segvn_data sdata;
		u_long off;
		struct vnode *vp;
		offset_t vpoff;

		off = (uintptr_t)vaddr & kd->pageoffset;

		/* get private data for segment */
		if (seg->s_data == NULL) {
			KVM_ERROR_1("NULL segvn_data ptr in segment");
			return (-1);
		}
		GETKVM(seg->s_data, &sdata, "segvn_data");

		/* Try anonymous pages first */
		if (sdata.amp != NULL) {
			struct anon_map amap;
			struct anon *ap;

			/* get anon_map structure */
			GETKVM(sdata.amp, &amap, "anon_map");
			if (amap.ahp == NULL)
				goto notanon;
			/* get anon ptr */
			if (_kvm_get_ap(kd, amap.ahp, &ap, (sdata.anon_index +
			    (((caddr_t)vaddr - seg->s_base)
					>> kd->pageshift))))
				return (-1);
			if (ap == NULL)
				goto notanon;
			anon_to_fdoffset(kd, ap, fdp, offp, off);
			return (0);
		}
notanon:

		/* If not in anonymous; try the vnode */
		vp = sdata.vp;
		vpoff = sdata.offset + ((caddr_t)vaddr - seg->s_base);

		/*
		 * If vp is null then the page doesn't exist.
		 */
		if (vp != NULL) {
			vp_to_fdoffset(kd, vp, fdp, offp, vpoff);
		} else {
			KVM_ERROR_1("Can't translate virtual address");
			return (-1);
		}
	} else if (seg->s_ops == kd->segkmem) {
		/* Segkmem segment */
		int poff;
		u_longlong_t paddr;

		if (as->a_segs.list != kd->Kas.a_segs.list)
			return (-1);

		poff = vaddr & kd->pageoffset;	/* save offset into page */
		vaddr &= kd->pagemask;		/* rnd down to start of page */

		/*
		 * First, check if it's in kvtop.
		 */
		if ((paddr = __kvm_kvtop(kd, vaddr)) == -1) {
			struct page *pp;
			struct page pg;

			/*
			 * Next, try the page hash table.
			 */
			if ((pp = pagefind(kd, &pg, kd->kvp,
			    (offset_t)(uintptr_t)vaddr)) == NULL)
				return (-1);
			if ((paddr = page_to_physaddr(kd, pp)) == -1)
				return (-1);
		}
		*fdp = kd->corefd;
		*offp = paddr + poff;
	} else if (seg->s_ops == kd->segmap) {
		/* Segmap segment */
		struct segmap_data sdata;
		struct smap *smp;
		struct smap *smap;
		size_t smpsz;
		u_long off;
		struct vnode *vp = NULL;
		offset_t vpoff;

		off = (uintptr_t)vaddr & MAXBOFFSET;
		vaddr &= MAXBMASK;

		/* get private data for segment */
		if (seg->s_data == NULL) {
			KVM_ERROR_1("NULL segmap_data ptr in segment");
			return (-1);
		}
		GETKVM(seg->s_data, &sdata, "segmap_data");

		/* get space for smap array */
		smpsz = (seg->s_size >> MAXBSHIFT) * sizeof (struct smap);
		if (smpsz == 0) {
			KVM_ERROR_1("zero-length smap array");
			return (-1);
		}
		if ((smap = malloc(smpsz)) == NULL) {
			KVM_ERROR_1("no memory for smap array");
			return (-1);
		}

		/* read smap array */
		if (kvm_kread(kd, (uintptr_t)sdata.smd_sm,
		    smap, smpsz) != smpsz) {
			KVM_ERROR_1("error reading smap array");
			free(smap);
			return (-1);
		}

		/* index into smap array to find the right one */
		smp = &smap[((caddr_t)vaddr - seg->s_base)>>MAXBSHIFT];
		vp = smp->sm_vp;

		if (vp == NULL) {
			KVM_ERROR_1("NULL vnode ptr in smap");
			free(smap); /* no longer need this */
			return (-1);
		}
		vpoff = smp->sm_off + off;
		free(smap); /* no longer need this */
		vp_to_fdoffset(kd, vp, fdp, offp, vpoff);
	} else if (seg->s_ops == kd->segkp) {
		return (getsegkp(kd, seg, (caddr_t)vaddr, fdp, offp));
	} else if (seg->s_ops == kd->segspt) {
		struct sptshm_data shmdata;
		u_long off;
		struct anon_map amap;
		struct anon *ap;

		off = vaddr & kd->pageoffset;

		if (seg->s_data == NULL) {
			KVM_ERROR_1("NULL segspt_data ptr in segment");
			return (-1);
		}

		GETKVM(seg->s_data, &shmdata, "sptshm_data");
		GETKVM(shmdata.amp, &amap, "anon_map");
		/* get anon ptr */
		if (_kvm_get_ap(kd, amap.ahp, &ap, (0 +
			(((caddr_t)vaddr - seg->s_base) >> kd->pageshift))))
			return (-1);
		if (ap == NULL)
			return (-1);
		anon_to_fdoffset(kd, ap, fdp, offp, off);
		return (0);
	} else if (seg->s_ops == kd->segdev) {
		KVM_ERROR_1("cannot read segdev segments yet");
		return (-1);
	} else {
		KVM_ERROR_1("unknown segment type");
		return (-1);
	}
	return (0);
}

/*
 * convert anon pointer/offset to fd (swap, core or nothing) and offset
 */
static int
anon_to_fdoffset(
	kvm_t *kd,
	struct anon *ap,
	int *fdp,
	u_longlong_t *offp,
	u_long aoff)
{
	struct page *pp;
	struct page page;
	offset_t swapoff;
	struct vnode *vp;
	offset_t vpoff;
	u_longlong_t skaddr;

	if (ap == NULL) {
		KVM_ERROR_1("anon_to_fdoffset: null anon ptr");
		return (-1);
	}

	if ((swapoff = _anonoffset(kd, ap, &vp, &vpoff)) == -1) {
		return (-1);
	}

	/* try hash table in case page is still around */
	pp = pagefind(kd, &page, vp, vpoff);
	if (pp == NULL) {
		*fdp = kd->swapfd;
		*offp = swapoff + aoff;
		return (0);
	}

gotpage:
	/* make sure the page structure is useful */
	if (page.p_selock == -1) {
		KVM_ERROR_1("anon page is gone");
		return (-1);
	}

	/*
	 * Page is in core.
	 */
	skaddr = page_to_physaddr(kd, pp);
	if (skaddr == (uintptr_t)-1) {
		KVM_ERROR_2("anon_to_fdoffset: can't find page %p", pp);
		return (-1);
	}
	*fdp = kd->corefd;
	*offp = skaddr + aoff;
	return (0);
}

/*
 * convert vnode pointer/offset to fd (core or nothing) and offset
 */
static int
vp_to_fdoffset(
	kvm_t *kd,
	struct vnode *vp,
	int *fdp,
	u_longlong_t *offp,
	offset_t vpoff)
{
	struct page *pp;
	struct page page;
	uintptr_t off;
	u_longlong_t skaddr;

	off = vpoff & kd->pageoffset;
	vpoff &= kd->pagemask;

	if (vp == NULL) {
		KVM_ERROR_1("vp_to_fdoffset: null vp ptr");
		return (-1);
	}

	pp = pagefind(kd, &page, vp, vpoff);
	if (pp == NULL) {
		KVM_ERROR_1("vp_to_fdoffset: page not mapped in");
		return (-1);
	}

	/* make sure the page structure is useful */
	if (page.p_selock == -1) {
		KVM_ERROR_1("vp_to_fdoffset: page is gone");
		return (-1);
	}

	/*
	 * Page is in core.
	 */
	skaddr = page_to_physaddr(kd, pp);
	if (skaddr == (uintptr_t)-1) {
		KVM_ERROR_2("vp_to_fdoffset: can't find page %p", pp);
		return (-1);
	}
	*fdp = kd->corefd;
	*offp = skaddr + off;
	return (0);
}

/*
 * convert page pointer to physical address
 */
static u_longlong_t
page_to_physaddr(kvm_t *kd, struct page *pp)
{
	u_int pagenum;

	if (kvm_kread(kd, ((uintptr_t)pp) + kd->pagenum_offset,
	    &pagenum, sizeof (pagenum)) != sizeof (pagenum)) {
		KVM_ERROR_2("error reading %s", "pagenum");
		return ((u_longlong_t)-1ll);
	}
	return ((u_longlong_t)pagenum << kd->pageshift);
}
