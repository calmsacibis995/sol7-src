/*
 * Copyright (c) 1989-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tmp_subr.c	1.26	97/07/04 SMI"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/t_lock.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/debug.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/kmem.h>
#include <sys/atomic.h>
#include <sys/fs/tmp.h>
#include <sys/fs/tmpnode.h>

#define	MODESHIFT	3

int
tmp_taccess(struct tmpnode *tp, int mode, struct cred *cred)
{
	/*
	 * Superuser always gets access
	 */
	if (cred->cr_uid == 0)
		return (0);
	/*
	 * Check access based on owner, group and
	 * public permissions in tmpnode.
	 */
	if (cred->cr_uid != tp->tn_uid) {
		mode >>= MODESHIFT;
		if (groupmember(tp->tn_gid, cred) == 0)
			mode >>= MODESHIFT;
	}
	if ((tp->tn_mode & mode) == mode)
		return (0);
	return (EACCES);
}

/*
 * Allocate zeroed memory if tmpfs_maxkmem has not been exceeded
 * or the 'musthave' flag is set.  'musthave' allocations should
 * always be subordinate to normal allocations so that tmpfs_maxkmem
 * can't be exceeded by more than a few KB.  Example: when creating
 * a new directory, the tmpnode is a normal allocation; if that
 * succeeds, the dirents for "." and ".." are 'musthave' allocations.
 */
void *
tmp_memalloc(size_t size, int musthave)
{
	static time_t last_warning;

	if (atomic_add_long_nv(&tmp_kmemspace, size) < tmpfs_maxkmem ||
	    musthave)
		return (kmem_zalloc(size, KM_SLEEP));

	atomic_add_long(&tmp_kmemspace, -size);
	if (last_warning != hrestime.tv_sec) {
		last_warning = hrestime.tv_sec;
		cmn_err(CE_WARN, "tmp_memalloc: tmpfs over memory limit");
	}
	return (NULL);
}

void
tmp_memfree(void *cp, size_t size)
{
	kmem_free(cp, size);
	atomic_add_long(&tmp_kmemspace, -size);
}
