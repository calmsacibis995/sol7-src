/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)auto_vnops.c	1.47	98/01/23 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/vfs.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/pathname.h>
#include <sys/dirent.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/tiuser.h>
#include <sys/cmn_err.h>
#include <sys/stat.h>
#include <sys/mode.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/fs/autofs.h>
#include <rpcsvc/autofs_prot.h>
#include <fs/fs_subr.h>

/*
 *  Vnode ops for autofs
 */
static int auto_open(vnode_t **, int, cred_t *);
static int auto_close(vnode_t *, int, int, offset_t, cred_t *);
static int auto_getattr(vnode_t *, vattr_t *, int, cred_t *);
static int auto_setattr(vnode_t *, vattr_t *, int, cred_t *);
static int auto_access(vnode_t *, int, int, cred_t *);
static int auto_lookup(vnode_t *, char *, vnode_t **,
	pathname_t *, int, vnode_t *, cred_t *);
static int auto_create(vnode_t *, char *, vattr_t *, vcexcl_t,
	int, vnode_t **, cred_t *, int);
static int auto_remove(vnode_t *, char *, cred_t *);
static int auto_link(vnode_t *, vnode_t *, char *, cred_t *);
static int auto_rename(vnode_t *, char *, vnode_t *, char *, cred_t *);
static int auto_mkdir(vnode_t *, char *, vattr_t *, vnode_t **, cred_t *);
static int auto_rmdir(vnode_t *, char *, vnode_t *, cred_t *);
static int auto_readdir(vnode_t *, uio_t *, cred_t *, int *);
static int auto_symlink(vnode_t *, char *, vattr_t *, char *, cred_t *);
static int auto_readlink(vnode_t *, struct uio *, cred_t *);
static int auto_fsync(vnode_t *, int, cred_t *);
static void auto_inactive(vnode_t *, cred_t *);
static void auto_rwlock(vnode_t *, int);
static void auto_rwunlock(vnode_t *vp, int);
static int auto_seek(vnode_t *vp, offset_t, offset_t *);
static int auto_cmp(vnode_t *, vnode_t *);

static int auto_trigger_mount(vnode_t *, cred_t *, vnode_t **);

vnodeops_t auto_vnodeops = {
	auto_open,	/* open */
	auto_close,	/* close */
	fs_nosys,	/* read */
	fs_nosys,	/* write */
	fs_nosys,	/* ioctl */
	fs_setfl,	/* setfl */
	auto_getattr,	/* getattr */
	auto_setattr,	/* setattr */
	auto_access,	/* access */
	auto_lookup,	/* lookup */
	auto_create,	/* create */
	auto_remove,	/* remove */
	auto_link,	/* link */
	auto_rename,	/* rename */
	auto_mkdir,	/* mkdir */
	auto_rmdir,	/* rmdir */
	auto_readdir,	/* readdir */
	auto_symlink,	/* symlink */
	auto_readlink,	/* readlink */
	auto_fsync,	/* fsync */
	auto_inactive,	/* inactive */
	fs_nosys,	/* fid */
	auto_rwlock,	/* rwlock */
	auto_rwunlock,	/* rwunlock */
	auto_seek,	/* seek */
	auto_cmp,	/* cmp */
	fs_nosys,	/* frlock */
	fs_nosys,	/* space */
	fs_nosys,	/* realvp */
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* map */
	fs_nosys_addmap, /* addmap */
	fs_nosys,	/* delmap */
	fs_poll,	/* poll */
	fs_nosys,	/* dump */
	fs_pathconf,	/* pathconf */
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_nodispose,	/* dispose */
	fs_nosys,	/* setsecattr */
	fs_fab_acl,	/* getsecattr */
	fs_nosys	/* shrlock */
};

/* ARGSUSED */
static int
auto_open(vnode_t **vpp, int flag, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_open: *vpp=%p\n", (void *)*vpp));

	error = auto_trigger_mount(*vpp, cred, &newvp);
	if (error)
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is now mounted on.
		 */
		VN_RELE(*vpp);
		*vpp = newvp;
		error = VOP_ACCESS(*vpp, VREAD, 0, cred);
		if (!error)
			error = VOP_OPEN(vpp, flag, cred);
	}

done:
	AUTOFS_DPRINT((5, "auto_open: *vpp=%p error=%d\n", (void *)*vpp,
	    error));
	return (error);
}

/* ARGSUSED */
static int
auto_close(vnode_t *vp, int flag, int count, offset_t offset, cred_t *cred)
{
	return (0);
}

static int
auto_getattr(vnode_t *vp, vattr_t *vap, int flags, cred_t *cred)
{
	fnnode_t *fnp = vntofn(vp);
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_getattr vp %p\n", (void *)vp));

	mutex_enter(&vp->v_lock);
	if (vp->v_vfsmountedhere != NULL) {
		/*
		 * Node is mounted on.
		 */
		error = VFS_ROOT(vp->v_vfsmountedhere, &newvp);
		mutex_exit(&vp->v_lock);
		if (!error) {
			error = VOP_GETATTR(newvp, vap, flags, cred);
			VN_RELE(newvp);
		}
		return (error);
	}
	mutex_exit(&vp->v_lock);

	ASSERT(vp->v_type == VDIR || vp->v_type == VLNK);
	vap->va_uid	= 0;
	vap->va_gid	= 0;
	vap->va_nlink	= fnp->fn_linkcnt;
	vap->va_nodeid	= (u_longlong_t)fnp->fn_nodeid;
	vap->va_size	= fnp->fn_size;
	vap->va_atime	= fnp->fn_atime;
	vap->va_mtime	= fnp->fn_mtime;
	vap->va_ctime	= fnp->fn_ctime;
	vap->va_type	= vp->v_type;
	vap->va_mode	= fnp->fn_mode;
	vap->va_fsid	= vp->v_vfsp->vfs_dev;
	vap->va_rdev	= 0;
	vap->va_blksize	= MAXBSIZE;
	vap->va_nblocks	= (fsblkcnt64_t)btod(vap->va_size);
	vap->va_vcode	= 0;

	return (0);
}

static int
auto_setattr(vnode_t *vp, struct vattr *vap, int flags, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_setattr vp %p\n", (void *)vp));

	if (error = auto_trigger_mount(vp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_SETATTR(newvp, vap, flags, cred);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_setattr: error=%d\n", error));
	return (error);
}

/* ARGSUSED */
static int
auto_access(vnode_t *vp, int mode, int flags, cred_t *cred)
{
	fnnode_t *fnp = vntofn(vp);
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_access: vp=%p\n", (void *)vp));

	if (error = auto_trigger_mount(vp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is mounted on.
		 */
		error = VOP_ACCESS(newvp, mode, 0, cred);
		VN_RELE(newvp);
	} else {
		/*
		 * really interested in the autofs node, check the
		 * access on it
		 */
		ASSERT(error == 0);
		if (cred->cr_uid != 0) {
			if (cred->cr_uid != fnp->fn_uid) {
				mode >>= 3;
				if (groupmember(fnp->fn_gid, cred) == 0)
					mode >>= 3;
			}
			if ((fnp->fn_mode & mode) != mode)
				error = EACCES;
		}
	}

done:
	AUTOFS_DPRINT((5, "auto_access: error=%d\n", error));
	return (error);
}


static int
auto_lookup(
	vnode_t *dvp,
	char *nm,
	vnode_t **vpp,
	pathname_t *pnp,
	int flags,
	vnode_t *rdir,
	cred_t *cred)
{
	int error = 0;
	vnode_t *newvp = NULL;
	fninfo_t *dfnip;
	fnnode_t *dfnp = NULL;
	fnnode_t *fnp = NULL;
	char *searchnm;
	int operation;		/* either AUTOFS_LOOKUP or AUTOFS_MOUNT */

	dfnip = vfstofni(dvp->v_vfsp);
	AUTOFS_DPRINT((3, "auto_lookup: dvp=%p (%s) name=%s\n",
	    (void *)dvp, dfnip->fi_map, nm));

	if (error = VOP_ACCESS(dvp, VEXEC, 0, cred))
		return (error);

	if (nm[0] == '.' && nm[1] == 0) {
		VN_HOLD(dvp);
		*vpp = dvp;
		return (0);
	} else if (nm[0] == '.' && nm[1] == '.' && nm[2] == 0) {
		fnnode_t *pdfnp;

		pdfnp = (vntofn(dvp))->fn_parent;
		ASSERT(pdfnp != NULL);
		*vpp = fntovn(pdfnp);
		VN_HOLD(*vpp);
		return (0);
	}

top:
	dfnp = vntofn(dvp);
	searchnm = nm;
	operation = 0;

	ASSERT(dvp->v_op == &auto_vnodeops);

	AUTOFS_DPRINT((3, "auto_lookup: dvp=%p dfnp=%p\n", (void *)dvp,
	    (void *)dfnp));

	/*
	 * If a lookup or mount of this node is in progress, wait for it
	 * to finish, and return whatever result it got.
	 */
	mutex_enter(&dfnp->fn_lock);
	if (dfnp->fn_flags & (MF_LOOKUP | MF_INPROG)) {
		mutex_exit(&dfnp->fn_lock);
		error = auto_wait4mount(dfnp);
		if (error == AUTOFS_SHUTDOWN)
			error = ENOENT;
		if (error == EAGAIN)
			goto top;
		if (error)
			return (error);
	} else
		mutex_exit(&dfnp->fn_lock);

	mutex_enter(&dvp->v_lock);
	ASSERT(!vn_vfswlock_held(dvp));
	if (dvp->v_vfsmountedhere != NULL) {
		error = VFS_ROOT(dvp->v_vfsmountedhere, &newvp);
		mutex_exit(&dvp->v_lock);
		if (!error) {
			error = VOP_LOOKUP(newvp, nm, vpp, pnp,
			    flags, rdir, cred);
			VN_RELE(newvp);
		}
		return (error);
	}
	mutex_exit(&dvp->v_lock);

	rw_enter(&dfnp->fn_rwlock, RW_READER);
	error = auto_search(dfnp, nm, &fnp, cred);
	rw_exit(&dfnp->fn_rwlock);
	if (error) {
		if (dfnip->fi_flags & MF_DIRECT) {
			/*
			 * direct map.
			 */
			if (dfnp->fn_dirents) {
				/*
				 * Mount previously triggered.
				 * 'nm' not found
				 */
				error = ENOENT;
			} else {
				/*
				 * I need to contact the daemon to trigger
				 * the mount. 'dfnp' will be the mountpoint.
				 */
				operation = AUTOFS_MOUNT;
				VN_HOLD(fntovn(dfnp));
				fnp = dfnp;
				error = 0;
			}
		} else if (dvp == dfnip->fi_rootvp) {
			/*
			 * 'dfnp' is the root of the indirect AUTOFS.
			 */
			rw_enter(&dfnp->fn_rwlock, RW_WRITER);
			error = auto_search(dfnp, nm, &fnp, cred);
			if (error) {
				/*
				 * create node being looked-up and request
				 * mount on it.
				 */
				error = auto_enter(dfnp, nm, &fnp, kcred);
				if (!error)
					operation = AUTOFS_LOOKUP;
			}
			rw_exit(&dfnp->fn_rwlock);
		} else if ((dfnp->fn_dirents == NULL) &&
		    ((dvp->v_flag & VROOT) == 0) &&
		    ((fntovn(dfnp->fn_parent))->v_flag & VROOT)) {
			/*
			 * dfnp is the actual 'mountpoint' of indirect map,
			 * it is the equivalent of a direct mount,
			 * ie, /home/'user1'
			 */
			operation = AUTOFS_MOUNT;
			VN_HOLD(fntovn(dfnp));
			fnp = dfnp;
			error = 0;
			searchnm = dfnp->fn_name;
		}
	}

	if (error == EAGAIN)
		goto top;
	if (error)
		return (error);

	/*
	 * We now have the actual fnnode we're interested in.
	 * The 'MF_LOOKUP' indicates another thread is currently
	 * performing a daemon lookup of this node, therefore we
	 * wait for its completion.
	 * The 'MF_INPROG' indicates another thread is currently
	 * performing a daemon mount of this node, we wait for it
	 * to be done if we are performing a MOUNT. We don't
	 * wait for it if we are performing a LOOKUP.
	 */
	mutex_enter(&fnp->fn_lock);
	if ((fnp->fn_flags & MF_LOOKUP) ||
	    ((operation == AUTOFS_MOUNT) && (fnp->fn_flags & MF_INPROG))) {
		mutex_exit(&fnp->fn_lock);
		error = auto_wait4mount(fnp);
		VN_RELE(fntovn(fnp));
		if (error == AUTOFS_SHUTDOWN)
			error = ENOENT;
		if (error && error != EAGAIN)
			return (error);
		goto top;
	}

	if (operation == 0) {
		/*
		 * got the fnnode, and no other thread
		 * is attempting a mount or lookup on it.
		 */
		mutex_exit(&fnp->fn_lock);
		*vpp = fntovn(fnp);
		return (0);
	}

	/*
	 * Since I got to this point, it means I'm the one
	 * responsible for triggering the mount/look-up of this node.
	 */
	switch (operation) {
	case AUTOFS_LOOKUP:
		AUTOFS_BLOCK_OTHERS(fnp, MF_LOOKUP);
		mutex_exit(&fnp->fn_lock);
		error = auto_lookup_aux(fnp, searchnm, cred);
		if (!error) {
			/*
			 * Return this vnode
			 */
			*vpp = fntovn(fnp);
		} else {
			/*
			 * release our reference to this vnode
			 * and return error
			 */
			VN_RELE(fntovn(fnp));
		}
		break;
	case AUTOFS_MOUNT:
		AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
		mutex_exit(&fnp->fn_lock);
		/*
		 * auto_new_mount_thread fires up a new thread which
		 * calls automountd finishing up the work
		 */
		auto_new_mount_thread(fnp, searchnm, cred);

		/*
		 * At this point, we are simply another thread
		 * waiting for the mount to complete
		 */
		error = auto_wait4mount(fnp);
		if (error == AUTOFS_SHUTDOWN)
			error = ENOENT;

		/*
		 * now release our reference to this vnode
		 */
		VN_RELE(fntovn(fnp));
		if (!error)
			goto top;
		break;
	default:
		auto_log(CE_WARN, "auto_lookup: unknown operation %d",
		    operation);
	}

	AUTOFS_DPRINT((5, "auto_lookup: name=%s *vpp=%p return=%d\n",
	    nm, (void *)*vpp, error));

	return (error);
}

static int
auto_create(
	vnode_t *dvp,
	char *nm,
	vattr_t *va,
	vcexcl_t excl,
	int mode,
	vnode_t **vpp,
	cred_t *cred,
	int flag)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_create dvp %p nm %s\n", (void *)dvp, nm));

	if (error = auto_trigger_mount(dvp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is now mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_CREATE(newvp, nm, va, excl,
			    mode, vpp, cred, flag);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_create: error=%d\n", error));
	return (error);
}

static int
auto_remove(vnode_t *dvp, char *nm, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_remove dvp %p nm %s\n", (void *)dvp, nm));

	if (error = auto_trigger_mount(dvp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is now mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_REMOVE(newvp, nm, cred);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_remove: error=%d\n", error));
	return (error);
}

static int
auto_link(vnode_t *tdvp, vnode_t *svp, char *nm, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_link tdvp %p svp %p nm %s\n", (void *)tdvp,
	    (void *)svp, nm));

	if (error = auto_trigger_mount(tdvp, cred, &newvp))
		goto done;

	if (newvp == NULL) {
		/*
		 * an autonode can not be a link to another node
		 */
		error = ENOSYS;
		goto done;
	}

	if (newvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		VN_RELE(newvp);
		goto done;
	}

	if (svp->v_op == &auto_vnodeops) {
		/*
		 * source vp can't be an autonode
		 */
		error = ENOSYS;
		VN_RELE(newvp);
		goto done;
	}

	error = VOP_LINK(newvp, svp, nm, cred);
	VN_RELE(newvp);

done:
	AUTOFS_DPRINT((5, "auto_link error=%d\n", error));
	return (error);
}

static int
auto_rename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, cred_t *cr)
{
	vnode_t *o_newvp, *n_newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_rename odvp %p onm %s to ndvp %p nnm %s\n",
	    (void *)odvp, onm, (void *)ndvp, nnm));

	/*
	 * we know odvp is an autonode, otherwise this function
	 * could not have ever been called.
	 */
	ASSERT(odvp->v_op == &auto_vnodeops);

	if (error = auto_trigger_mount(odvp, cr, &o_newvp))
		goto done;

	if (o_newvp == NULL) {
		/*
		 * can't rename an autonode
		 */
		error = ENOSYS;
		goto done;
	}

	if (ndvp->v_op == &auto_vnodeops) {
		/*
		 * directory is AUTOFS, need to trigger the
		 * mount of the real filesystem.
		 */
		if (error = auto_trigger_mount(ndvp, cr, &n_newvp)) {
			VN_RELE(o_newvp);
			goto done;
		}

		if (n_newvp == NULL) {
			/*
			 * target can't be an autonode
			 */
			error = ENOSYS;
			VN_RELE(o_newvp);
			goto done;
		}
	} else {
		/*
		 * destination directory mount had been
		 * triggered prior to the call to this function.
		 */
		n_newvp = ndvp;
	}

	ASSERT(n_newvp->v_op != &auto_vnodeops);

	if (n_newvp->v_vfsp->vfs_flag & VFS_RDONLY) {
		error = EROFS;
		VN_RELE(o_newvp);
		if (n_newvp != ndvp)
			VN_RELE(n_newvp);
		goto done;
	}

	error = VOP_RENAME(o_newvp, onm, n_newvp, nnm, cr);
	VN_RELE(o_newvp);
	if (n_newvp != ndvp)
		VN_RELE(n_newvp);

done:
	AUTOFS_DPRINT((5, "auto_rename error=%d\n", error));
	return (error);
}

static int
auto_mkdir(vnode_t *dvp, char *nm, vattr_t *va, vnode_t **vpp, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_mkdir dvp %p nm %s\n", (void *)dvp, nm));

	ASSERT(*vpp == NULL);
	if (error = auto_trigger_mount(dvp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is now mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_MKDIR(newvp, nm, va, vpp, cred);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_mkdir: error=%d\n", error));
	return (error);
}

static int
auto_rmdir(vnode_t *dvp, char *nm, vnode_t *cdir, cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_rmdir: vp=%p nm=%s\n", (void *)dvp, nm));

	if (error = auto_trigger_mount(dvp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is now mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_RMDIR(newvp, nm, cdir, cred);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_rmdir: error=%d\n", error));
	return (error);
}

static int autofs_nobrowse = 0;

static int
auto_readdir(vnode_t *vp, uio_t *uiop, cred_t *cred, int *eofp)
{
	struct autofs_rddirargs rda;
	struct autofs_rddirres rd;
	fnnode_t *fnp = vntofn(vp);
	fnnode_t *cfnp, *nfnp;
	dirent64_t *dp, *tmp, *prev = NULL;
	u_long offset;
	u_long outcount = 0, count = 0;
	size_t namelen;
	u_long alloc_count;
	void *outbuf;
	fninfo_t *fnip = vfstofni(vp->v_vfsp);
	struct iovec *iovp;
	int error = 0;
	int reached_max = 0;
	int myeof = 0;

	AUTOFS_DPRINT((4, "auto_readdir vp=%p offset=%ld\n",
	    (void *)vp, uiop->uio_offset));

	if (eofp != NULL)
		*eofp = 0;

	iovp = uiop->uio_iov;
	alloc_count = iovp->iov_len;

	if (uiop->uio_iovcnt != 1)
		return (EINVAL);

	fnp->fn_ref_time = hrestime.tv_sec;
	fnp->fn_atime = hrestime;

	dp = outbuf = kmem_zalloc(alloc_count, KM_SLEEP);

	/*
	 * Held when getdents calls VOP_RWLOCK....
	 */
	ASSERT(RW_READ_HELD(&fnp->fn_rwlock));
	if (uiop->uio_offset >= AUTOFS_DAEMONCOOKIE) {
again:
		/*
		 * Do readdir of daemon contents only
		 * Drop readers lock and reacquire after reply.
		 */
		rw_exit(&fnp->fn_rwlock);

		count = 0;
		rda.rda_map = fnip->fi_map;
		rda.rda_offset = (u_int)uiop->uio_offset;
		rd.rd_rddir.rddir_entries = dp;
		rda.rda_count = rd.rd_rddir.rddir_size = (u_int)alloc_count;
		error = auto_calldaemon(fnip, AUTOFS_READDIR,
		    xdr_autofs_rddirargs, &rda,
		    xdr_autofs_rddirres, &rd,
		    cred, TRUE);
		/*
		 * reacquire previously dropped lock
		 */
		rw_enter(&fnp->fn_rwlock, RW_READER);

		if (!error)
			error = rd.rd_status;
		if (error) {
			if (error == AUTOFS_SHUTDOWN) {
				/*
				 * treat as empty directory
				 */
				error = 0;
				myeof = 1;
				if (eofp)
					*eofp = 1;
			}
			goto done;
		}

		if (rd.rd_rddir.rddir_size) {
			tmp = dp;
			/*
			 * Check for duplicates here
			 */
			do {
				if (auto_search(fnp, tmp->d_name, NULL, cred)) {
					/*
					 * entry not found in kernel list,
					 * include it in readdir output.
					 */
					prev = tmp;
					outcount += tmp->d_reclen;
				} else {
					/*
					 * duplicate entry
					 */
					if (tmp == dp) {
						/*
						 * first entry in array
						 */
						dp = (dirent64_t *)
						    ((caddr_t)tmp +
						    tmp->d_reclen);
					} else {
						/*
						 * at least one good entry
						 * in list
						 */
						prev->d_reclen += tmp->d_reclen;
						outcount += tmp->d_reclen;
					}
				}
				count += tmp->d_reclen;
				reached_max = count >= rd.rd_rddir.rddir_size;
				if (!reached_max) {
					tmp = (dirent64_t *)
					    ((caddr_t)tmp + tmp->d_reclen);
				}
			} while (!reached_max);

			if (outcount)
				error = uiomove(dp, outcount, UIO_READ, uiop);
			uiop->uio_offset = rd.rd_rddir.rddir_offset;
		} else {
			if (rd.rd_rddir.rddir_eof == 0) {
				/*
				 * alloc_count not large enough for one
				 * directory entry
				 */
				error = EINVAL;
			}
		}
		if (rd.rd_rddir.rddir_eof && !error) {
			myeof = 1;
			if (eofp)
				*eofp = 1;
		}
		if (!error && !myeof && outcount == 0) {
			/*
			 * call daemon with new cookie, all previous
			 * elements happened to be duplicates
			 */
			dp = outbuf;
			goto again;
		}
		goto done;
	}

	if (uiop->uio_offset == 0) {
		int this_reclen;

		/*
		 * first time: so fudge the . and ..
		 */
		this_reclen = DIRENT64_RECLEN(1);
		if (alloc_count < this_reclen) {
			error = EINVAL;
			goto done;
		}
		dp->d_ino = (ino64_t)fnp->fn_nodeid;
		dp->d_off = (off64_t)1;
		dp->d_reclen = (u_short)this_reclen;
		(void) strcpy(dp->d_name, ".");
		outcount += dp->d_reclen;
		dp = (dirent64_t *)((char *)dp + dp->d_reclen);

		this_reclen = DIRENT64_RECLEN(2);
		if (alloc_count < outcount + this_reclen) {
			error = EINVAL;
			goto done;
		}
		dp->d_reclen = (u_short)this_reclen;
		dp->d_ino = (ino64_t)fnp->fn_parent->fn_nodeid;
		dp->d_off = (off64_t)2;
		(void) strcpy(dp->d_name, "..");
		outcount += dp->d_reclen;
		dp = (dirent64_t *)((char *)dp + dp->d_reclen);
	}

	offset = 2;
	cfnp = fnp->fn_dirents;
	while (cfnp != NULL) {
		nfnp = cfnp->fn_next;
		offset = cfnp->fn_offset;
		if ((offset >= uiop->uio_offset) &&
		    (!(cfnp->fn_flags & MF_LOOKUP))) {
			int reclen;

			/*
			 * include node only if its offset is greater or
			 * equal to the one required and it is not in
			 * transient state (not being looked-up)
			 */
			namelen = strlen(cfnp->fn_name);
			reclen = (int)DIRENT64_RECLEN(namelen);
			if (outcount + reclen > alloc_count) {
				reached_max = 1;
				break;
			}
			dp->d_reclen = (u_short)reclen;
			dp->d_ino = (ino64_t)cfnp->fn_nodeid;
			if (nfnp != NULL) {
				/*
				 * get the offset of the next element
				 */
				dp->d_off = (off64_t)nfnp->fn_offset;
			} else {
				/*
				 * This is the last element, make
				 * offset one plus the current
				 */
				dp->d_off = (off64_t)cfnp->fn_offset + 1;
			}
			(void) strcpy(dp->d_name, cfnp->fn_name);
			outcount += dp->d_reclen;
			dp = (dirent64_t *)((char *)dp + dp->d_reclen);
		}
		cfnp = nfnp;
	}

	if (outcount)
		error = uiomove(outbuf, outcount, UIO_READ, uiop);
	if (!error) {
		if (reached_max) {
			/*
			 * This entry did not get added to the buffer on this,
			 * call. We need to add it on the next call therefore
			 * set uio_offset to this entry's offset.  If there
			 * wasn't enough space for one dirent, return EINVAL.
			 */
			uiop->uio_offset = offset;
			if (outcount == 0)
				error = EINVAL;
		} else if (autofs_nobrowse ||
		    auto_nobrowse_option(fnip->fi_opts) ||
		    (fnip->fi_flags & MF_DIRECT) || (fnp->fn_trigger != NULL) ||
		    (((vp->v_flag & VROOT) == 0) &&
		    ((fntovn(fnp->fn_parent))->v_flag & VROOT) &&
		    (fnp->fn_dirents == NULL))) {
			/*
			 * done reading directory entries
			 */
			uiop->uio_offset = offset + 1;
			if (eofp)
				*eofp = 1;
		} else {
			/*
			 * Need to get the rest of the entries from the daemon.
			 */
			uiop->uio_offset = AUTOFS_DAEMONCOOKIE;
		}
	}

done:
	kmem_free(outbuf, alloc_count);
	AUTOFS_DPRINT((5, "auto_readdir vp=%p offset=%ld eof=%d\n",
	    (void *)vp, uiop->uio_offset, myeof));
	return (error);
}

static int
auto_symlink(
	vnode_t *dvp,
	char *lnknm,		/* new entry */
	vattr_t *tva,
	char *tnm,		/* existing entry */
	cred_t *cred)
{
	vnode_t *newvp;
	int error;

	AUTOFS_DPRINT((4, "auto_symlink: dvp=%p lnknm=%s tnm=%s\n",
	    (void *)dvp, lnknm, tnm));

	if (error = auto_trigger_mount(dvp, cred, &newvp))
		goto done;

	if (newvp != NULL) {
		/*
		 * Node is mounted on.
		 */
		if (newvp->v_vfsp->vfs_flag & VFS_RDONLY)
			error = EROFS;
		else
			error = VOP_SYMLINK(newvp, lnknm, tva, tnm, cred);
		VN_RELE(newvp);
	} else
		error = ENOSYS;

done:
	AUTOFS_DPRINT((5, "auto_symlink: error=%d\n", error));
	return (error);
}

/* ARGSUSED */
static
auto_readlink(vnode_t *vp, struct uio *uiop, cred_t *cr)
{
	fnnode_t *fnp = vntofn(vp);
	int error;

	AUTOFS_DPRINT((4, "auto_readlink: vp=%p\n", (void *)vp));

	fnp->fn_ref_time = hrestime.tv_sec;

	if (vp->v_type != VLNK)
		error = EINVAL;
	else {
		ASSERT(!(fnp->fn_flags & (MF_INPROG | MF_LOOKUP)));
		fnp->fn_atime = hrestime;
		error = uiomove(fnp->fn_symlink, MIN(fnp->fn_symlinklen,
		    uiop->uio_resid), UIO_READ, uiop);
	}

	AUTOFS_DPRINT((5, "auto_readlink: error=%d\n", error));
	return (error);
}

/* ARGSUSED */
static int
auto_fsync(vnode_t *cp, int syncflag, cred_t *cred)
{
	return (0);
}

/* ARGSUSED */
static void
auto_inactive(vnode_t *vp, cred_t *cred)
{
	fnnode_t *fnp = vntofn(vp);
	fnnode_t *dfnp = fnp->fn_parent;
	int count;

	AUTOFS_DPRINT((4, "auto_inactive: vp=%p v_count=%u fn_link=%d\n",
	    (void *)vp, vp->v_count, fnp->fn_linkcnt));

	/*
	 * The rwlock should not be already held by this thread.
	 * The assert relies on the fact that the owner field is cleared
	 * when the lock is released.
	 */
	ASSERT(dfnp != NULL);
	ASSERT(rw_owner(&dfnp->fn_rwlock) != curthread);
	rw_enter(&dfnp->fn_rwlock, RW_WRITER);
	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count > 0);
	count = --vp->v_count;
	mutex_exit(&vp->v_lock);
	if (count == 0) {
		/*
		 * Free only if node has no subdirectories.
		 */
		if (fnp->fn_linkcnt == 1) {
			auto_disconnect(dfnp, fnp);
			rw_exit(&dfnp->fn_rwlock);
			auto_freefnnode(fnp);
			AUTOFS_DPRINT((5, "auto_inactive: (exit) vp=%p freed\n",
			    (void *)vp));
			return;
		}
	}
	rw_exit(&dfnp->fn_rwlock);

	AUTOFS_DPRINT((5, "auto_inactive: (exit) vp=%p v_count=%u fn_link=%d\n",
	    (void *)vp, vp->v_count, fnp->fn_linkcnt));
}

static void
auto_rwlock(vnode_t *vp, int write_lock)
{
	fnnode_t *fnp = vntofn(vp);
	if (write_lock)
		rw_enter(&fnp->fn_rwlock, RW_WRITER);
	else
		rw_enter(&fnp->fn_rwlock, RW_READER);
}

/* ARGSUSED */
static void
auto_rwunlock(vnode_t *vp, int write_lock)
{
	fnnode_t *fnp = vntofn(vp);
	rw_exit(&fnp->fn_rwlock);
}


/* ARGSUSED */
static int
auto_seek(struct vnode *vp, offset_t ooff, offset_t *noffp)
{
	/*
	 * Return 0 unconditionally, since we expect
	 * a VDIR all the time
	 */
	return (0);
}

static int
auto_cmp(vnode_t *vp1, vnode_t *vp2)
{
	return (vp1 == vp2);
}

/*
 * Triggers the mount if needed. If the mount has been triggered by
 * another thread, it will wait for its return status, and return it.
 * Whether the mount is triggered by this thread, another thread, or
 * if the vnode was already covered, '*newvp' is a
 * VN_HELD vnode pointing to the root of the filesystem covering 'vp'.
 * If the node is not mounted on, and should not be mounted on, '*newvp'
 * will be NULL.
 * The calling routine may use '*newvp' to do the filesystem jump.
 */
static int
auto_trigger_mount(vnode_t *vp, cred_t *cred, vnode_t **newvp)
{
	fnnode_t *fnp = vntofn(vp);
	fninfo_t *fnip = vfstofni(vp->v_vfsp);
	vnode_t *dvp;
	int delayed_ind;
	char name[AUTOFS_MAXPATHLEN];
	int error;

	AUTOFS_DPRINT((4, "auto_trigger_mount: vp=%p\n", (void *)vp));

	*newvp = NULL;
retry:
	error = 0;
	delayed_ind = 0;
	mutex_enter(&fnp->fn_lock);
	while (fnp->fn_flags & (MF_LOOKUP | MF_INPROG)) {
		/*
		 * Mount or lookup in progress,
		 * wait for it before proceeding.
		 */
		mutex_exit(&fnp->fn_lock);
		error = auto_wait4mount(fnp);
		if (error == AUTOFS_SHUTDOWN) {
			error = 0;
			goto done;
		}
		if (error && error != EAGAIN)
			goto done;
		error = 0;
		mutex_enter(&fnp->fn_lock);
	}

	if (vp->v_vfsmountedhere != NULL) {
		/*
		 * already mounted on
		 */
		mutex_exit(&fnp->fn_lock);
		mutex_enter(&vp->v_lock);
		ASSERT(!vn_vfswlock_held(vp));
		if (vp->v_vfsmountedhere != NULL) {
			error = VFS_ROOT(vp->v_vfsmountedhere, newvp);
		} else {
			mutex_exit(&vp->v_lock);
			goto retry;
		}
		mutex_exit(&vp->v_lock);
		goto done;
	}

	dvp = fntovn(fnp->fn_parent);

	if ((fnp->fn_dirents == NULL) &&
	    ((fnip->fi_flags & MF_DIRECT) == 0) &&
	    ((vp->v_flag & VROOT) == 0) &&
	    (dvp->v_flag & VROOT)) {
		/*
		 * If the parent of this node is the root of an indirect
		 * AUTOFS filesystem, this node is remountable.
		 */
		delayed_ind = 1;
	}

	if (delayed_ind ||
	    ((fnip->fi_flags & MF_DIRECT) && (fnp->fn_dirents == NULL))) {
		/*
		 * Trigger mount since:
		 * direct mountpoint with no subdirs or
		 * delayed indirect.
		 */
		AUTOFS_BLOCK_OTHERS(fnp, MF_INPROG);
		mutex_exit(&fnp->fn_lock);
		if (delayed_ind)
			(void) strcpy(name, fnp->fn_name);
		else
			(void) strcpy(name, ".");
		fnp->fn_ref_time = hrestime.tv_sec;
		auto_new_mount_thread(fnp, name, cred);
		/*
		 * At this point we're simply another thread waiting
		 * for the mount to finish.
		 */
		error = auto_wait4mount(fnp);
		if (error == EAGAIN)
			goto retry;
		if (error == AUTOFS_SHUTDOWN) {
			error = 0;
			goto done;
		}
		if (error == 0) {
			mutex_enter(&vp->v_lock);
			ASSERT(!vn_vfswlock_held(vp));
			if (vp->v_vfsmountedhere != NULL) {
				error = VFS_ROOT(vp->v_vfsmountedhere, newvp);
			} else {
				mutex_exit(&vp->v_lock);
				goto retry;
			}
			mutex_exit(&vp->v_lock);
		}
	} else
		mutex_exit(&fnp->fn_lock);

done:
	AUTOFS_DPRINT((5, "auto_trigger_mount: error=%d\n", error));
	return (error);
}
