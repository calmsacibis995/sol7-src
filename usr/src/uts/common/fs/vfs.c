/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	(c) 1986,1987,1988,1989,1993  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)vfs.c	1.69	98/01/23 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/buf.h>
#include <sys/swap.h>
#include <sys/debug.h>
#include <sys/vnode.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/pathname.h>
#include <sys/bootconf.h>

#include <vm/page.h>

/*
 * VFS global data.
 */
vnode_t *rootdir;		/* pointer to root vnode. */

static struct vfs root;
struct vfs *rootvfs = &root;	/* pointer to root vfs; head of VFS list. */
struct vfs **rvfs_head;		/* array of vfs ptrs for vfs hash list */
kmutex_t *rvfs_lock;		/* array of locks for vfs hash list */
int vfshsz = 512;		/* # of heads/locks in vfs hash arrays */
				/* must be power of 2!	*/

/*
 * VFS system calls: mount, umount, syssync, statfs, fstatfs, statvfs,
 * fstatvfs, and sysfs moved to common/syscall.
 */

/*
 * Update every mounted file system.  We call the vfs_sync operation of
 * each file system type, passing it a NULL vfsp to indicate that all
 * mounted file systems of that type should be updated.
 */
void
vfs_sync(int flag)
{
	int i;

	for (i = 1; i < nfstype; i++) {
		RLOCK_VFSSW();
		if (vfssw[i].vsw_vfsops) {
		    (void) (*vfssw[i].vsw_vfsops->vfs_sync)(NULL, flag, CRED());
		}
		RUNLOCK_VFSSW();
	}
}

void
sync(void)
{
	vfs_sync(0);
}

/*
 * External routines.
 */

krwlock_t vfssw_lock;	/* lock accesses to vfssw */

/*
 * Lock for accessing the vfs linked list.  Initialized in vfs_mountroot(),
 * but otherwise should be accessed only via vfs_list_lock() and
 * vfs_list_unlock().
 */
static kmutex_t vfslist;

/*
 * vfs_mountroot is called by main() to mount the root filesystem.
 */
void
vfs_mountroot(void)
{

	rw_init(&vfssw_lock, NULL, RW_DEFAULT, NULL);

	/*
	 * Alloc the vfs hash bucket array and locks
	 */
	rvfs_head = kmem_zalloc(vfshsz * sizeof (struct vfs *), KM_SLEEP);
	rvfs_lock = kmem_zalloc(vfshsz * sizeof (kmutex_t), KM_SLEEP);

	/*
	 * Call machine-dependent routine "rootconf" to choose a root
	 * file system type.
	 */
	if (rootconf())
		cmn_err(CE_PANIC, "vfs_mountroot: cannot mount root");
	/*
	 * Get vnode for '/'.  Set up rootdir, u.u_rdir and u.u_cdir
	 * to point to it.  These are used by lookuppn() so that it
	 * knows where to start from ('/' or '.').
	 */
	if (VFS_ROOT(rootvfs, &rootdir))
		cmn_err(CE_PANIC, "vfs_mountroot: no root vnode");
	u.u_cdir = rootdir;
	VN_HOLD(u.u_cdir);
	u.u_rdir = NULL;
	/*
	 * Notify the module code that it can begin using the
	 * root filesystem instead of the boot program's services.
	 */
	modrootloaded = 1;
}

/*
 * Common mount code.  Called from the system call entry point, from autofs,
 * and from pxfs.
 *
 * Takes the effective file system type, mount arguments, the mount point
 * vnode, flags specifying whether the mount is a remount and whether it
 * should be entered into the vfs list, and credentials.  Fills in its vfspp
 * parameter with the mounted file system instance's vfs.
 *
 * Note that the effective file system type is specified as a string.  It may
 * be null, in which case it's determined from the mount arguments, and may
 * differ from the type specified in the mount arguments; this is a hook to
 * allow interposition when instantiating file system instances.
 *
 * The caller is responsible for releasing its own hold on the mount point
 * vp (this routine does its own hold when necessary).
 * Also note that for remounts, the mount point vp should be the vnode for
 * the root of the file system rather than the vnode that the file system
 * is mounted on top of.
 */
int
domount(char *fsname, struct mounta *uap, vnode_t *vp, struct cred *credp,
	struct vfs **vfspp)
{
	struct vfssw	*vswp;
	struct vfsops	*vfsops;
	struct vfs	*vfsp;
	int		error;
	int		ovflags;
	int		remount = uap->flags & MS_REMOUNT;

	/*
	 * Perform mount point checks.
	 */
	ASSERT(vp->v_count > 0);

	/*
	 * Prevent path name resolution from proceeding past
	 * the mount point.
	 */
	if (vn_vfswlock(vp)) {
		return (EBUSY);
	}

	/*
	 * Verify that it's legitimate to establish a mount on
	 * the prospective mount point.
	 */
	if (vp->v_vfsmountedhere != NULL) {
		/*
		 * The mount point lock was obtained after some
		 * other thread raced through and established a mount.
		 */
		vn_vfsunlock(vp);
		return (EBUSY);
	}
	if (vp->v_flag & VNOMOUNT) {
		vn_vfsunlock(vp);
		return (EINVAL);
	}

	/*
	 * Find the ops vector to use to invoke the file system-specific mount
	 * method.  If the fsname argument is non-NULL, use it directly.
	 * Otherwise, dig the file system type information out of the mount
	 * arguments.
	 *
	 * A side effect is to lock the vfssw.
	 *
	 * Mount arguments can be specified in several ways, which are
	 * distinguished by flag bit settings.  The preferred way is to set
	 * MS_DATA, indicating a 6 argument mount with the file system type
	 * supplied as a character string.  If MS_FSS is set, then SVR3
	 * compatibility is desired and there are 4 mount arguments, with the
	 * file system type specified as a numeric index.  If neither flag bit
	 * is set, then the mount format predates support for multiple file
	 * system types, and the file system type is assumed to be the same as
	 * that of the root.
	 *
	 * A further wrinkle is that some callers don't set MS_FSS and MS_DATA
	 * consistently with these conventions.  To handle them, we check to
	 * see whether the pointer to the file system name has a numeric value
	 * less than 256.  If so, we treat it as an index.
	 */
	if (fsname != NULL) {
		if ((vswp = vfs_getvfssw(fsname)) == NULL) {
			vn_vfsunlock(vp);
			return (EINVAL);
		} else {
			vfsops = vswp->vsw_vfsops;
		}
	} else if (uap->flags & (MS_DATA|MS_FSS)) {
		size_t n;
		u_int fstype;
		char name[FSTYPSZ];

		if ((fstype = (u_int)uap->fstype) < 256) {
			if (fstype == 0 || fstype >= nfstype ||
			    !ALLOCATED_VFSSW(&vfssw[fstype])) {
				vn_vfsunlock(vp);
				return (EINVAL);
			}
			(void) strcpy(name, vfssw[fstype].vsw_name);
		} else {
			/*
			 * Handle either kernel or user address space.
			 */
			if (uap->flags & MS_SYSSPACE) {
				error = copystr(uap->fstype, name,
				    FSTYPSZ, &n);
			} else {
				error = copyinstr(uap->fstype, name,
				    FSTYPSZ, &n);
			}
			if (error) {
				if (error == ENAMETOOLONG)
					error = EINVAL;
				vn_vfsunlock(vp);
				return (error);
			}
		}

		if ((vswp = vfs_getvfssw(name)) == NULL) {
			vn_vfsunlock(vp);
			return (EINVAL);
		} else {
			vfsops = vswp->vsw_vfsops;
		}
	} else {
		vfsops = rootvfs->vfs_op;
		RLOCK_VFSSW();
	}
	/* vfssw was implicitly locked in vfs_getvfssw or explicitly here */
	ASSERT(VFSSW_LOCKED());

	if ((uap->flags & MS_DATA) == 0) {
		uap->dataptr = NULL;
		uap->datalen = 0;
	}

	/*
	 * If this is a remount, we don't want to create a new VFS.
	 * Instead, we pass the existing one with a remount flag.
	 */
	if (remount) {
		/*
		 * Confirm that the mount point is the root vnode of the
		 * file system that is being remounted.
		 * This can happen if the user specifies a different
		 * mount point directory pathname in the (re)mount command.
		 */
		if ((vp->v_flag & VROOT) == 0) {
			vn_vfsunlock(vp);
			RUNLOCK_VFSSW();
			return (ENOENT);
		}
		/*
		 * Disallow making file systems read-only.  Ignore other flags.
		 */
		if (uap->flags & MS_RDONLY) {
			vn_vfsunlock(vp);
			RUNLOCK_VFSSW();
			return (EINVAL);
		}
		vfsp = vp->v_vfsp;
		ovflags = vfsp->vfs_flag;
		vfsp->vfs_flag |= VFS_REMOUNT;
		vfsp->vfs_flag &= ~VFS_RDONLY;
	} else {
		vfsp = kmem_alloc(sizeof (vfs_t), KM_SLEEP);
		VFS_INIT(vfsp, vfsops, NULL);
	}

	/*
	 * Lock the vfs.
	 */
	if (error = vfs_lock(vfsp)) {
		vn_vfsunlock(vp);
		if (!remount)
			kmem_free(vfsp, sizeof (struct vfs));
		RUNLOCK_VFSSW();
		return (error);
	}

	/*
	 * Invalidate cached entry for the mount point.
	 */
	dnlc_purge_vp(vp);

	/*
	 * Instantiate (or reinstantiate) the file system and
	 * splice it into the file system name space.
	 */
	error = VFS_MOUNT(vfsp, vp, uap, credp);

	if (error) {
		if (remount) {
			vfsp->vfs_flag = ovflags;
			vfs_unlock(vfsp);
		} else {
			vfs_unlock(vfsp);
			kmem_free(vfsp, sizeof (struct vfs));
		}
	} else {
		if (remount) {
			vfsp->vfs_flag &= ~VFS_REMOUNT;
		} else {
			/*
			 * Link vfsp into the name space at the mount
			 * point. Vfs_add() is responsible for
			 * holding the mount point which will be
			 * released when vfs_remove() is called.
			 */
			vfs_add(vp, vfsp, uap->flags);
			vp->v_vfsp->vfs_nsubmounts++;
		}
		vfs_unlock(vfsp);
	}
	RUNLOCK_VFSSW();
	vn_vfsunlock(vp);

	/*
	 * Return vfsp to caller.
	 */
	if (error == 0) {
		*vfspp = vfsp;
	}
	return (error);
}


int
dounmount(struct vfs *vfsp, cred_t *cr)
{
	vnode_t *coveredvp;
	int error;

	/*
	 * Get covered vnode.
	 */
	coveredvp = vfsp->vfs_vnodecovered;
	ASSERT(vn_vfswlock_held(coveredvp));

	/*
	 * Purge all dnlc entries for this vfs.
	 */
	(void) dnlc_purge_vfsp(vfsp, 0);

	VFS_SYNC(vfsp, 0, cr);

	/*
	 * Lock vnode to maintain fs status quo during unmount.  This
	 * has to be done after the sync because ufs_update tries to acquire
	 * the vfs_reflock.
	 */
	vfs_lock_wait(vfsp);

	if (error = VFS_UNMOUNT(vfsp, cr)) {
		vfs_unlock(vfsp);
		vn_vfsunlock(coveredvp);
	} else {
		--coveredvp->v_vfsp->vfs_nsubmounts;
		/*
		 * vfs_remove() will do a VN_RELE(vfsp->vfs_vnodecovered)
		 * when it frees vfsp so we do a VN_HOLD() so we can
		 * continue to use coveredvp afterwards.
		 */
		VN_HOLD(coveredvp);
		vfs_remove(vfsp);
		vn_vfsunlock(coveredvp);
		VN_RELE(coveredvp);
	}
	return (error);
}


/*
 * Vfs_unmountall() is called by uadmin() to unmount all
 * mounted file systems (except the root file system) during shutdown.
 * It follows the existing locking protocol when traversing the vfs list
 * to sync and unmount vfses. Even though there should be no
 * other thread running while the system is shutting down, it is prudent
 * to still follow the locking protocol.
 */
void
vfs_unmountall(void)
{
	struct vfs *vfsp, *head_vfsp, *last_vfsp;
	int nvfs, i;
	struct vfs **unmount_list;

	/*
	 * Construct a list of vfses that we plan to unmount.
	 * Write lock the covered vnode to avoid the race condiiton
	 * caused by another unmount. Skip those vfses that we cannot
	 * lock.
	 */
	vfs_list_lock();

	for (vfsp = rootvfs->vfs_next, head_vfsp = last_vfsp = NULL,
	    nvfs = 0; vfsp != NULL; vfsp = vfsp->vfs_next) {
		/*
		 * skip any vfs that we cannot acquire the vfslock()
		 */
		if (vfs_lock(vfsp) == 0) {
			if (vn_vfswlock(vfsp->vfs_vnodecovered) == 0) {

				/*
				 * put in the list of vfses to be unmounted
				 */
				if (last_vfsp)
					last_vfsp->vfs_list = vfsp;
				else
					head_vfsp = vfsp;
				last_vfsp = vfsp;

				nvfs++;
			} else
				vfs_unlock(vfsp);
		}
	}

	if (nvfs == 0) {
		vfs_list_unlock();
		return;
	}

	last_vfsp ->vfs_list = NULL;

	unmount_list = kmem_alloc(nvfs * sizeof (struct vfs *), KM_SLEEP);

	for (vfsp = head_vfsp, i = 0; vfsp != NULL; vfsp = vfsp->vfs_list) {
		unmount_list[i++] = vfsp;
		vfs_unlock(vfsp);
	}

	/*
	 * Once covered vnode is locked, no one can unmount the vfs.
	 * It is now safe to unlock the vfs list.
	 */
	vfs_list_unlock();

	/*
	 * Toss all dnlc entries now so that the per-vfs sync
	 * and unmount operations don't have to slog through
	 * a bunch of uninteresting vnodes over and over again.
	 */
	dnlc_purge();

	ASSERT(i == nvfs);

	for (i = 0; i < nvfs; i++)
		VFS_SYNC(unmount_list[i], SYNC_CLOSE, CRED());

	for (i = 0; i < nvfs; i++) {
		(void) dounmount(unmount_list[i], CRED());
	}

	kmem_free(unmount_list, nvfs * sizeof (struct vfs *));
}

/*
 * vfs_add is called by a specific filesystem's mount routine to add
 * the new vfs into the vfs list/hash and to cover the mounted-on vnode.
 * The vfs should already have been locked by the caller.
 *
 * coveredvp is NULL if this is the root.
 */
void
vfs_add(vnode_t *coveredvp, struct vfs *vfsp, int mflag)
{
	int newflag;

	ASSERT(vfs_lock_held(vfsp));

	newflag = vfsp->vfs_flag;
	if (mflag & MS_RDONLY)
		newflag |= VFS_RDONLY;
	else
		newflag &= ~VFS_RDONLY;
	if (mflag & MS_NOSUID)
		newflag |= VFS_NOSUID;
	else
		newflag &= ~VFS_NOSUID;

	vfs_list_add(vfsp);

	if (coveredvp != NULL) {
		ASSERT(vn_vfswlock_held(coveredvp));
		coveredvp->v_vfsmountedhere = vfsp;
		VN_HOLD(coveredvp);
	}
	vfsp->vfs_vnodecovered = coveredvp;

	vfsp->vfs_flag = newflag;
}

/*
 * Remove a vfs from the vfs list, destroy pointers to it, and then destroy
 * the vfs itself.  Called from dounmount after it's confirmed with the file
 * system that the unmount is legal.
 */
void
vfs_remove(struct vfs *vfsp)
{
	vnode_t *vp;

	ASSERT(vfs_lock_held(vfsp));

	/*
	 * Can't unmount root.  Should never happen because fs will
	 * be busy.
	 */
	if (vfsp == rootvfs)
		cmn_err(CE_PANIC, "vfs_remove: unmounting root");

	vfs_list_remove(vfsp);

	/*
	 * Unhook from the file system name space.
	 */
	vp = vfsp->vfs_vnodecovered;
	ASSERT(vn_vfswlock_held(vp));
	vp->v_vfsmountedhere = NULL;
	VN_RELE(vp);

	/*
	 * Release lock and wakeup anybody waiting.
	 */
	vfs_unlock(vfsp);

	/*
	 * Deallocate the vfs.
	 */
	sema_destroy(&vfsp->vfs_reflock);
	kmem_free(vfsp, sizeof (*vfsp));
}

/*
 * Lock a filesystem to prevent access to it while mounting,
 * unmounting and syncing.  Return EBUSY immediately if lock
 * can't be acquired.
 */
int
vfs_lock(vfs_t *vfsp)
{
	if (sema_tryp(&vfsp->vfs_reflock) == 0)
		return (EBUSY);
	return (0);
}

void
vfs_lock_wait(vfs_t *vfsp)
{
	sema_p(&vfsp->vfs_reflock);
}

/*
 * Unlock a locked filesystem.
 */
void
vfs_unlock(vfs_t *vfsp)
{
	sema_v(&vfsp->vfs_reflock);
}

/*
 * Utility routine that allows a filesystem to construct its
 * fsid in "the usual way" - by munging some underlying dev_t and
 * the filesystem type number into the 64-bit fsid.  Note that
 * this implicitly relies on dev_t persistence to make filesystem
 * id's persistent.
 *
 * There's nothing to prevent an individual fs from constructing its
 * fsid in a different way, and indeed they should.
 *
 * Since we want fsids to be 32-bit quantities (so that they can be
 * exported identically by either 32-bit or 64-bit APIs, as well as
 * the fact that fsid's are "known" to NFS), we compress the device
 * number given down to 32-bits, and panic if that isn't possible.
 */
void
vfs_make_fsid(fsid_t *fsi, dev_t dev, int val)
{
	if (!cmpldev((dev32_t *)&fsi->val[0], dev))
		panic("device number too big for fsid!");
	fsi->val[1] = val;
}

int
vfs_lock_held(vfs_t *vfsp)
{
	return (sema_held(&vfsp->vfs_reflock));
}

/*
 * vfs list locking.
 *
 * Rather than manipulate the vfslist mutex directly, we abstract into lock
 * and unlock routines to allow the locking implementation to be changed for
 * clustering.
 *
 * Whenever the vfs list is modified through its hash links, the overall list
 * lock must be obtained before locking the relevant hash bucket.  But to see
 * whether a given vfs is on the list, it suffices to obtain the lock for the
 * hash bucket without getting the overall list lock.  (See getvfs() below.)
 */

void
vfs_list_lock()
{
	mutex_enter(&vfslist);
}

void
vfs_list_unlock()
{
	mutex_exit(&vfslist);
}

/*
 * Low level worker routines for adding entries to and removing entries from
 * the vfs list.
 */

void
vfs_list_add(struct vfs *vfsp)
{
	int vhno = VFSHASH(vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]);
	struct vfs **hp;

	/*
	 * Special casing for the root vfs.  This structure is allocated
	 * statically and hooked onto rootvfs at link time.  During the
	 * vfs_mountroot call at system startup time, the root file system's
	 * VFS_MOUNTROOT routine will call vfs_add with this root vfs struct
	 * as argument.  The code below must detect and handle this sepcial
	 * case.  The only apparent justification for this special casing is
	 * to ensure that the root file system appears at the head of the
	 * list.  (Other than that, the list is unordered; the implementation
	 * below places new entries immediately past the first entry.)
	 *
	 * XXX:	I'm assuming that it's ok to do normal list locking when
	 *	adding the entry for the root file system (this used to be
	 *	done with no locks held).
	 */
	vfs_list_lock();
	mutex_enter(&rvfs_lock[vhno]);
	/*
	 * Link into the vfs list proper.
	 */
	if (vfsp == &root) {
		/*
		 * Assert: This vfs is already on the list as its first entry.
		 * Thus, there's nothing to do.
		 */
		ASSERT(rootvfs == vfsp);
	} else {
		vfsp->vfs_next = rootvfs->vfs_next;
		rootvfs->vfs_next = vfsp;
	}
	/*
	 * Link into the hash table, inserting it at the end, so that LOFS
	 * with the same fsid as UFS (or other) file systems will not hide the
	 * UFS.
	 */
	for (hp = &rvfs_head[vhno]; *hp != NULL; hp = &(*hp)->vfs_hash)
		continue;
	/*
	 * hp now contains the address of the pointer to update to effect the
	 * insertion.
	 */
	vfsp->vfs_hash = NULL;
	*hp = vfsp;

	mutex_exit(&rvfs_lock[vhno]);
	vfs_list_unlock();
}

void
vfs_list_remove(struct vfs *vfsp)
{
	int vhno = VFSHASH(vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]);
	struct vfs *tvfsp;

	/*
	 * Callers are responsible for preventing attempts to unmount the
	 * root.
	 */
	ASSERT(vfsp != rootvfs);

	vfs_list_lock();
	mutex_enter(&rvfs_lock[vhno]);

	/*
	 * Remove from hash.
	 */
	if (rvfs_head[vhno] == vfsp) {
		rvfs_head[vhno] = vfsp->vfs_hash;
		goto foundit;
	}
	for (tvfsp = rvfs_head[vhno]; tvfsp != NULL; tvfsp = tvfsp->vfs_hash) {
		if (tvfsp->vfs_hash == vfsp) {
			tvfsp->vfs_hash = vfsp->vfs_hash;
			goto foundit;
		}
	}
	cmn_err(CE_WARN, "vfs_list_remove: vfs not found in hash");

foundit:
	/*
	 * Remove from list.
	 */
	for (tvfsp = rootvfs; tvfsp != NULL; tvfsp = tvfsp->vfs_next) {
		if (tvfsp->vfs_next != vfsp)
			continue;
		tvfsp->vfs_next = vfsp->vfs_next;
		vfsp->vfs_next = NULL;
		break;
	}

	mutex_exit(&rvfs_lock[vhno]);
	vfs_list_unlock();

	if (tvfsp == NULL) {
		/*
		 * Couldn't find vfs to remove.
		 */
		cmn_err(CE_PANIC, "vfs_list_remove: vfs not found");
	}
}

struct vfs *
getvfs(fsid_t *fsid)
{
	struct vfs *vfsp;
	int val0 = fsid->val[0];
	int val1 = fsid->val[1];
	int vhno = VFSHASH(val0, val1);
	kmutex_t *hmp = &rvfs_lock[vhno];

	mutex_enter(hmp);
	for (vfsp = rvfs_head[vhno]; vfsp; vfsp = vfsp->vfs_hash) {
		if (vfsp->vfs_fsid.val[0] == val0 &&
		    vfsp->vfs_fsid.val[1] == val1) {
			mutex_exit(hmp);
			return (vfsp);
		}
	}
	mutex_exit(hmp);
	return (NULL);
}

/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found.
 */
struct vfs *
vfs_devsearch(dev_t dev)
{
	struct vfs *vfsp;

	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev)
			break;
	vfs_list_unlock();
	return (vfsp);
}

/*
 * Search the vfs list for a specified vfsops.
 */
struct vfs *
vfs_opssearch(struct vfsops *ops)
{
	struct vfs *vfsp;

	vfs_list_lock();
	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_op == ops)
			break;
	vfs_list_unlock();
	return (vfsp);
}

/*
 * Allocate an entry in vfssw for a file system type
 */
struct vfssw *
allocate_vfssw(char *type)
{
	struct vfssw *vswp;

	ASSERT(VFSSW_WRITE_LOCKED());
	for (vswp = &vfssw[1]; vswp < &vfssw[nfstype]; vswp++)
		if (!ALLOCATED_VFSSW(vswp)) {
			vswp->vsw_name = kmem_alloc(strlen(type) + 1, KM_SLEEP);
			(void) strcpy(vswp->vsw_name, type);
			return (vswp);
		}
	return (NULL);
}

/*
 * Impose additional layer of translation between vfstype names
 * and module names in the filesystem.
 */
static char *
vfs_to_modname(char *vfstype)
{
	if (strcmp(vfstype, "proc") == 0) {
		vfstype = "procfs";
	} else if (strcmp(vfstype, "fd") == 0) {
		vfstype = "fdfs";
	} else if (strncmp(vfstype, "nfs", 3) == 0) {
		vfstype = "nfs";
	}

	return (vfstype);
}

/*
 * Find a vfssw entry given a file system type name.
 * Try to autoload the filesystem if it's not found.
 * If it's installed, return the vfssw locked to prevent unloading.
 */
struct vfssw *
vfs_getvfssw(char *type)
{
	struct vfssw *vswp;
	char	*modname;
	int rval;

	RLOCK_VFSSW();
	if ((vswp = vfs_getvfsswbyname(type)) == NULL) {
		RUNLOCK_VFSSW();
		WLOCK_VFSSW();
		if ((vswp = vfs_getvfsswbyname(type)) == NULL) {
			if ((vswp = allocate_vfssw(type)) == NULL) {
				WUNLOCK_VFSSW();
				return (NULL);
			}
		}
		WUNLOCK_VFSSW();
		RLOCK_VFSSW();
	}

	modname = vfs_to_modname(type);

	/*
	 * Try to load the filesystem.  Before calling modload(), we drop
	 * our lock on the VFS switch table, and pick it up after the
	 * module is loaded.  However, there is a potential race:  the
	 * module could be unloaded after the call to modload() completes
	 * but before we pick up the lock and drive on.  Therefore,
	 * we keep reloading the module until we've loaded the module
	 * _and_ we have the lock on the VFS switch table.
	 */
	while (!VFS_INSTALLED(vswp)) {
		RUNLOCK_VFSSW();
		if (rootdir != NULL)
			rval = modload("fs", modname);
		else {
			/*
			 * If we haven't yet loaded the root file
			 * system, then our _init won't be called until
			 * later; don't bother looping.
			 */
			rval = modloadonly("fs", modname);
			RLOCK_VFSSW();
			break;
		}
		if (rval == -1)
			return (NULL);
		RLOCK_VFSSW();
	}

	return (vswp);
}

/*
 * Find a vfssw entry given a file system type name.
 */
struct vfssw *
vfs_getvfsswbyname(char *type)
{
	int i;

	ASSERT(VFSSW_LOCKED());
	if (type == NULL || *type == '\0')
		return (NULL);

	for (i = 1; i < nfstype; i++)
		if (strcmp(type, vfssw[i].vsw_name) == 0)
			return (&vfssw[i]);

	return (NULL);
}

#define	SYNC_TIMEOUT	60

extern int sync_timeout;
static int new_bufcnt, new_pgcnt, old_bufcnt, old_pgcnt;

int
sync_making_progress(int ticks)
{
	old_bufcnt = new_bufcnt;
	old_pgcnt = new_pgcnt;

	new_bufcnt = bio_busy(ticks != 0);
	new_pgcnt = page_busy(ticks != 0);

	/*
	 * If we're making progress, get a new lease on life.
	 */
	if (new_bufcnt < old_bufcnt || new_pgcnt < old_pgcnt)
		sync_timeout = SYNC_TIMEOUT * hz;

	if (new_bufcnt == 0 && new_pgcnt == 0) {	/* sync is complete */
		sync_timeout = 0;
		printf(" done\n");
	} else if (ticks != 0) {
		if (new_bufcnt)
			printf(" [%d]", new_bufcnt);
		if (new_pgcnt)
			printf(" %d", new_pgcnt);
		delay(ticks);
		if ((sync_timeout -= ticks) <= 0) {
			sync_timeout = 0;
			printf(" cannot sync -- giving up\n");
		}
	}

	return (sync_timeout);
}

/*
 * "sync" all file systems, and return only when all writes have been
 * completed.  For use by the reboot code; it's verbose.
 */
void
vfs_syncall()
{
	if (rootdir == NULL)	/* panic during boot - no filesystems yet */
		return;

	printf("syncing file systems...");
	sync_timeout = SYNC_TIMEOUT * hz;	/* start sync timer */
	sync();
	while (sync_making_progress(hz))
		continue;
}

/*
 * Map VFS flags to statvfs flags.  These shouldn't really be separate
 * flags at all.
 */
u_int
vf_to_stf(u_int vf)
{
	u_int stf = 0;

	if (vf & VFS_RDONLY)
		stf |= ST_RDONLY;
	if (vf & VFS_NOSUID)
		stf |= ST_NOSUID;
	if (vf & VFS_NOTRUNC)
		stf |= ST_NOTRUNC;

	return (stf);
}

/*
 * Use old-style function prototype for vfsstray() so
 * that we can use it anywhere in the vfsops structure.
 */
int vfsstray();

/*
 * Entries for (illegal) fstype 0.
 */
/* ARGSUSED */
int
vfsstray_sync(struct vfs *vfsp, short arg, struct cred *cr)
{
	cmn_err(CE_PANIC, "stray vfs operation");
	return (0);
}

struct vfsops vfs_strayops = {
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray_sync,
	vfsstray,
	vfsstray,
	vfsstray
};

/*
 * Entries for (illegal) fstype 0.
 */
int
vfsstray(void)
{
	cmn_err(CE_PANIC, "stray vfs operation");
	return (0);
}

int vfs_EIO();
int vfs_EIO_sync(struct vfs *, short, struct cred *);

vfsops_t EIO_vfsops = {
	vfs_EIO,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO_sync,
	vfs_EIO,
	vfs_EIO,
	vfs_EIO
};

/*
 * Support for dealing with forced UFS unmounts and it's interaction with
 * LOFS. Could be used by any filesystem.
 * See bug 1203132.
 */
int
vfs_EIO(void)
{
	return (EIO);
}

/*
 * We've gotta define the op for sync seperately, since the compiler gets
 * confused if we mix and match ANSI and normal style prototypes when
 * a "short" argument is present and spits out a warning.
 */
/*ARGSUSED*/
int
vfs_EIO_sync(struct vfs *vfsp, short arg, struct cred *cr)
{
	return (EIO);
}

vfs_t EIO_vfs;

/*
 * Called from startup() to initialize all loaded vfs's
 */
void
vfsinit(void)
{
	int i;

	/*
	 * fstype 0 is (arbitrarily) invalid.
	 */
	vfssw[0].vsw_vfsops = &vfs_strayops;
	vfssw[0].vsw_name = "BADVFS";

	VFS_INIT(&EIO_vfs, &EIO_vfsops, (caddr_t)NULL);

	/*
	 * Call all the init routines.
	 */
	/*
	 * A mixture of loadable and non-loadable filesystems
	 * is tricky to support, because of contention over exactly
	 * when the filesystems vsw_init() routine should be
	 * run on the rootfs -- at this point in the boot sequence, the
	 * rootfs module has  been loaded into the table, but its _init()
	 * routine and the vsw_init() routine have yet to be called - this
	 * will happen when we actually do the proper modload() in rootconf().
	 *
	 * So we use the following heuristic.  For each name in the
	 * switch with a non-nil init routine, we look for a module
	 * of the appropriate name - if it exists, we infer that
	 * the loadable module code has either already vsw_init()-ed
	 * it, or will vsw_init() soon.  If it can't be found there, then
	 * we infer this is a statically configured filesystem so we get on
	 * and call its vsw_init() routine directly.
	 *
	 * Sigh.  There's got to be a better way to do this.
	 */
	ASSERT(VFSSW_LOCKED());		/* the root fs */
	RUNLOCK_VFSSW();
	for (i = 1; i < nfstype; i++) {
		RLOCK_VFSSW();
		if (vfssw[i].vsw_init) {
			char *modname;

			modname = vfs_to_modname(vfssw[i].vsw_name);
			/*
			 * XXX	Should probably hold the mod_lock here
			 */
			if (!mod_find_by_filename("fs", modname))
				(*vfssw[i].vsw_init)(&vfssw[i], i);
		}
		RUNLOCK_VFSSW();
	}
	RLOCK_VFSSW();
}
