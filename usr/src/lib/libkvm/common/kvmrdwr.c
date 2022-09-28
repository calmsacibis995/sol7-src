/*
 * Copyright (c) 1987-1992, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kvmrdwr.c	2.28	98/02/22 SMI"

#include "kvm_impl.h"
#include <sys/param.h>
#include <sys/vmmac.h>
#include <sys/file.h>
#include <sys/fcntl.h>

/*
 * The first routine is used by crash(1m), sigh.
 */
ssize_t kvm_as_read(kvm_t *, struct as *, uintptr_t, void *, size_t);
static ssize_t kvm_as_write(kvm_t *, struct as *, uintptr_t, void *, size_t);
static ssize_t kvm_as_rdwr(kvm_t *, struct as *, uintptr_t, void *,
    size_t, ssize_t (*)(int, void *, size_t));

ssize_t
kvm_uread(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
	return (kvm_as_read(kd, &kd->Uas, addr, buf, nbytes));
}

ssize_t
kvm_uwrite(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
	return (kvm_as_write(kd, &kd->Uas, addr, buf, nbytes));
}

ssize_t
kvm_kread(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
	return (kvm_as_read(kd, &kd->Kas, addr, buf, nbytes));
}

ssize_t
kvm_kwrite(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
	return (kvm_as_write(kd, &kd->Kas, addr, buf, nbytes));
}

/*
 * Used directly by crash(1m).
 */
ssize_t
kvm_as_read(kvm_t *kd, struct as *as, uintptr_t addr, void *buf, size_t nbytes)
{
	return (kvm_as_rdwr(kd, as, addr, buf, nbytes, read));
}

static ssize_t
kvm_as_write(kvm_t *kd, struct as *as, uintptr_t addr, void *buf,
    size_t nbytes)
{
	if (!(kd->flags & KVMD_WRITE))	/* opened for write? */
		return (-1);
	return (kvm_as_rdwr(kd, as, addr, buf, nbytes,
	    (ssize_t (*)(int, void *, size_t))write));
}

static ssize_t
kvm_as_rdwr(kvm_t *kd, struct as *as, uintptr_t addr, void *buf,
    size_t nbytes, ssize_t (*rdwr)(int, void *, size_t))
{
	u_long a;
	u_long end;
	long n;
	ssize_t cnt = 0;
	char *bp;
	long i;
	int fd;
	u_longlong_t paddr;

	if (addr == 0) {
		KVM_ERROR_1("kvm_as_rdwr: address == 0");
		return (-1);
	}

	if (kd->corefd == -1) {
		KVM_ERROR_1("kvm_as_rdwr: no corefile descriptor");
		return (-1);
	}

	/*
	 * If virtual addresses may be used, by all means use them.
	 * Reads can go fast through /dev/kmem on a live system, but
	 * /dev/kmem does not allow writes to kernel text, so we
	 * take the long path thru /dev/mem if /dev/kmem fails.
	 * _kvm_physaddr() will detect any bad addresses.
	 */
	if ((as->a_segs.list == kd->Kas.a_segs.list) && (kd->virtfd != -1)) {
		if (lseek(kd->virtfd, (off_t)addr, L_SET) != -1) {
			cnt = (*rdwr)(kd->virtfd, buf, nbytes);
			if (cnt == nbytes)
				return (cnt);
			KVM_PERROR_2("%s: i/o error (ro page?)", kd->virt);
		} else {
			KVM_PERROR_2("%s: seek error", kd->virt);
		}
	}
	cnt = 0;
	end = addr + nbytes;
	for (a = addr, bp = buf; a < end; a += n, bp += n) {
		n = kd->pagesz - (a & kd->pageoffset);
		if ((a + n) > end)
			n = end - a;
		_kvm_physaddr(kd, as, a, &fd, &paddr);
		if (fd == -1) {
			KVM_ERROR_1("kvm_rdwr: _kvm_physaddr failed");
			goto out;
		}

		/* see if file is of condensed type */
		switch (_uncondense(kd, fd, &paddr)) {
		case -1:
			KVM_ERROR_2("%s: uncondense error", kd->core);
			goto out;
		case -2:
			/* paddr corresponds to a hole */
			if (rdwr == read) {
				(void) memset(bp, 0, n);
			}
			/* writes are ignored */
			cnt += n;
			continue;
		}

		/* could be physical memory or swap */
		if (llseek(fd, (offset_t)paddr, L_SET) == -1) {
			KVM_PERROR_2("%s: seek error", kd->core);
			goto out;
		}

		if ((i = (*rdwr)(fd, bp, n)) > 0)
			cnt += i;

		if (i != n) {
			KVM_PERROR_2("%s: i/o error", kd->core);
			break;
		}
	}

out:
	return (cnt > 0 ? cnt : -1);
}

int
_uncondense(kvm_t *kd, int fd, u_longlong_t *paddrp)
{
	struct condensed *cdp;

	/* There ought to be a better way! */
	if (fd == kd->corefd)
		cdp = kd->corecdp;
	else if (fd == kd->swapfd)
		cdp = kd->swapcdp;
	else {
		KVM_ERROR_1("uncondense: unknown fd");
		return (-1);
	}

	if (cdp) {
		u_longlong_t paddr = *paddrp;
		off_t *atoo = cdp->cd_atoo;
		off_t offset;
		int chunksize = cdp->cd_chunksize;
		long i;

		i = paddr / chunksize;
		if (i < cdp->cd_atoosize) {
			if ((offset = atoo[i]) == NULL)
				return (-2);	/* Hole */
		} else {
			/*
			 * An attempt to read/write beyond the end of
			 * the logical file; convert to the equivalent
			 * offset, and let a read hit EOF and a write do
			 * an extension.
			 */
			offset = (i - cdp->cd_atoosize) * chunksize +
			    cdp->cd_dp->dump_dumpsize;
		}

		*paddrp = offset + (paddr % chunksize);
	}
	return (0);
}

/*
 * Compatibility routines.
 *
 * Long ago and far away, these routines were the sole interface to
 * libkvm.  They used to distinguish whether or not the "address"
 * was in the kernel address space or the user address space by
 * comparing the value with kernelbase.
 *
 * Now that we have kernel and user in separate contexts on some
 * platforms, that has become an increasingly bad interface.
 */

#undef kvm_read

ssize_t
kvm_read(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
#ifdef _LP64
	/*
	 * Since kernelbase is probably below most user addresses,
	 * and most users of libkvm expect the kernel address space,
	 * we just assume our callers really meant the kernel.
	 * If they want access to the user address space, they should
	 * use kvm_uread etc.
	 */
	return (kvm_kread(kd, addr, buf, nbytes));
#else
	/*
	 * This version is correct for all implementations
	 * except for sun4u, where kernelbase is lower than userlimit.
	 * Again, we're doing about the best we can do here.
	 */
	if (addr >= (uintptr_t)kd->kernelbase)
		return (kvm_kread(kd, addr, buf, nbytes));
	return (kvm_uread(kd, addr, buf, nbytes));
#endif
}

#undef kvm_write

ssize_t
kvm_write(kvm_t *kd, uintptr_t addr, void *buf, size_t nbytes)
{
#ifdef _LP64
	return (kvm_kwrite(kd, addr, buf, nbytes));
#else
	if (addr >= (uintptr_t)kd->kernelbase)
		return (kvm_kwrite(kd, addr, buf, nbytes));
	return (kvm_uwrite(kd, addr, buf, nbytes));
#endif
}
