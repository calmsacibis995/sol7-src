/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)auto_vfsops.c	1.33	98/01/23 SMI"

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/proc.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/tiuser.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/pathname.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <sys/dnlc.h>
#include <fs/fs_subr.h>
#include <sys/fs/autofs.h>
#include <rpcsvc/autofs_prot.h>
#include <sys/note.h>
#include <sys/modctl.h>

static int autofs_init(vfssw_t *, int);

static major_t autofs_major;
static minor_t autofs_minor;
static kmutex_t autofs_minor_lock;
kmutex_t fnnode_list_lock;
struct fnnode *fnnode_list;

static struct vfsops auto_vfsops;

static vfssw_t vfw = {
	"autofs",
	autofs_init,
	&auto_vfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
static struct modlfs modlfs = {
	&mod_fsops, "filesystem for autofs", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

/*
 * There are not enough stubs for rpcmod so we must force load it
 */
char _depends_on[] = "strmod/rpcmod misc/rpcsec";

/*
 * This is the module initialization routine.
 */
_init(void)
{
	return (mod_install(&modlinkage));
}

_fini(void)
{
	/*
	 * Don't allow the autofs module to be unloaded for now.
	 */
	return (EBUSY);
}

_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int autofs_fstype;

/*
 * autofs VFS operations
 */
static int auto_mount(vfs_t *, vnode_t *, struct mounta *, cred_t *);
static int auto_unmount(vfs_t *, cred_t *);
static int auto_root(vfs_t *, vnode_t **);
static int auto_statvfs(vfs_t *, struct statvfs64 *);

static struct vfsops auto_vfsops = {
	auto_mount,	/* mount */
	auto_unmount,	/* unmount */
	auto_root,	/* root */
	auto_statvfs,	/* statvfs */
	fs_sync,	/* sync */
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys	/* swapvp */
};

static void
unmount_init(void)
{
	while (thread_create(NULL, NULL, auto_do_unmount,
	    NULL, 0, &p0, TS_RUN, 60) == NULL) {
		/*
		 * couldn't create unmount thread, most likely because
		 * we're low in memory, delay 20 seconds and try again.
		 */
		cmn_err(CE_WARN,
		    "autofs: unmount thread create failed - retrying");
		delay(20 * hz);
	}
}

int
autofs_init(vfssw_t *vswp, int fstype)
{
	autofs_fstype = fstype;
	ASSERT(autofs_fstype != 0);
	/*
	 * Associate VFS ops vector with this fstype
	 */
	vswp->vsw_vfsops = &auto_vfsops;

	mutex_init(&autofs_minor_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&fnnode_count_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&fnnode_list_lock, NULL, MUTEX_DEFAULT, NULL);
	fnnode_list = NULL;

	/*
	 * Assign unique major number for all autofs mounts
	 */
	if ((autofs_major = getudev()) == (major_t)-1) {
		cmn_err(CE_WARN,
		    "autofs: autofs_init: can't get unique device number");
		mutex_destroy(&autofs_minor_lock);
		mutex_destroy(&fnnode_count_lock);
		mutex_destroy(&fnnode_list_lock);
		return (1);
	}

	unmount_init();

	return (0);
}

/* ARGSUSED */
static int
auto_mount(vfs_t *vfsp, vnode_t *vp, struct mounta *uap, cred_t *cr)
{
	int error = 0;
	size_t len = 0;
	struct autofs_args args;
	fninfo_t *fnip = NULL;
	vnode_t *rootvp = NULL;
	fnnode_t *rootfnp = NULL;
	char *data = uap->dataptr;
	char datalen = uap->datalen;
	dev_t autofs_dev;
	char strbuff[MAXPATHLEN+1];
	vnode_t *kvp;

	AUTOFS_DPRINT((4, "auto_mount: vfs %p vp %p\n", (void *)vfsp,
	    (void *)vp));

	if (!suser(cr))
		return (EPERM);

	/*
	 * Get arguments
	 */
	if (uap->flags & MS_SYSSPACE) {
		if (datalen != sizeof (args))
			return (EINVAL);
		error = kcopy(data, &args, sizeof (args));
	} else {
		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (datalen != sizeof (args))
				return (EINVAL);
			error = copyin(data, &args, sizeof (args));
		} else {
			struct autofs_args32 args32;

			if (datalen != sizeof (args32))
				return (EINVAL);
			error = copyin(data, &args32, sizeof (args32));

			args.addr.maxlen = args32.addr.maxlen;
			args.addr.len = args32.addr.len;
			args.addr.buf = (char *)args32.addr.buf;
			args.path = (char *)args32.path;
			args.opts = (char *)args32.opts;
			args.map = (char *)args32.map;
			args.subdir = (char *)args32.subdir;
			args.key = (char *)args32.key;
			args.mount_to = args32.mount_to;
			args.rpc_to = args32.rpc_to;
			args.direct = args32.direct;
		}
	}
	if (error)
		return (EFAULT);

	/*
	 * For a remount, only update mount information
	 * i.e. default mount options, map name, etc.
	 */
	if (uap->flags & MS_REMOUNT) {
		fnip = vfstofni(vfsp);
		if (fnip == NULL)
			return (EINVAL);

		if (args.direct == 1)
			fnip->fi_flags |= MF_DIRECT;
		else
			fnip->fi_flags &= ~MF_DIRECT;
		fnip->fi_mount_to = args.mount_to;
		fnip->fi_rpc_to = args.rpc_to;

		/*
		 * Get default options
		 */
		if (uap->flags & MS_SYSSPACE)
			error = copystr(args.opts, strbuff, sizeof (strbuff),
			    &len);
		else
			error = copyinstr(args.opts, strbuff, sizeof (strbuff),
			    &len);
		if (error)
			return (EFAULT);

		kmem_free(fnip->fi_opts, fnip->fi_optslen);
		fnip->fi_opts = kmem_alloc(len, KM_SLEEP);
		fnip->fi_optslen = (int)len;
		bcopy(strbuff, fnip->fi_opts, len);

		/*
		 * Get context/map name
		 */
		if (uap->flags & MS_SYSSPACE)
			error = copystr(args.map, strbuff, sizeof (strbuff),
			    &len);
		else
			error = copyinstr(args.map, strbuff, sizeof (strbuff),
			    &len);
		if (error)
			return (EFAULT);

		kmem_free(fnip->fi_map, fnip->fi_maplen);
		fnip->fi_map = kmem_alloc(len, KM_SLEEP);
		fnip->fi_maplen = (int)len;
		bcopy(strbuff, fnip->fi_map, len);

		return (0);
	}

	/*
	 * Allocate fninfo struct and attach it to vfs
	 */
	fnip = kmem_zalloc(sizeof (*fnip), KM_SLEEP);
	fnip->fi_mountvfs = vfsp;

	fnip->fi_mount_to = args.mount_to;
	fnip->fi_rpc_to = args.rpc_to;
	fnip->fi_refcnt = 0;
	vfsp->vfs_bsize = AUTOFS_BLOCKSIZE;
	vfsp->vfs_fstype = autofs_fstype;

	/*
	 * Assign a unique device id to the mount
	 */
	mutex_enter(&autofs_minor_lock);
	do {
		autofs_minor = (autofs_minor + 1) & L_MAXMIN32;
		autofs_dev = makedevice(autofs_major, autofs_minor);
	} while (vfs_devsearch(autofs_dev));
	mutex_exit(&autofs_minor_lock);

	vfsp->vfs_dev = autofs_dev;
	vfs_make_fsid(&vfsp->vfs_fsid, autofs_dev, autofs_fstype);
	vfsp->vfs_data = (void *)fnip;
	vfsp->vfs_bcount = 0;

	/*
	 * Get daemon address
	 */
	fnip->fi_addr.len = args.addr.len;
	fnip->fi_addr.maxlen = fnip->fi_addr.len;
	fnip->fi_addr.buf = kmem_alloc(args.addr.len, KM_SLEEP);
	if (uap->flags & MS_SYSSPACE)
		error = kcopy(args.addr.buf, fnip->fi_addr.buf, args.addr.len);
	else
		error = copyin(args.addr.buf, fnip->fi_addr.buf, args.addr.len);
	if (error) {
		error = EFAULT;
		goto errout;
	}

	/*
	 * Get path for mountpoint
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.path, strbuff, sizeof (strbuff), &len);
	else
		error = copyinstr(args.path, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_path = kmem_alloc(len, KM_SLEEP);
	fnip->fi_pathlen = (int)len;
	bcopy(strbuff, fnip->fi_path, len);

	/*
	 * Get default options
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.opts, strbuff, sizeof (strbuff), &len);
	else
		error = copyinstr(args.opts, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_opts = kmem_alloc(len, KM_SLEEP);
	fnip->fi_optslen = (int)len;
	bcopy(strbuff, fnip->fi_opts, len);

	/*
	 * Get context/map name
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.map, strbuff, sizeof (strbuff), &len);
	else
		error = copyinstr(args.map, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_map = kmem_alloc(len, KM_SLEEP);
	fnip->fi_maplen = (int)len;
	bcopy(strbuff, fnip->fi_map, len);

	/*
	 * Get subdirectory within map
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.subdir, strbuff, sizeof (strbuff), &len);
	else
		error = copyinstr(args.subdir, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_subdir = kmem_alloc(len, KM_SLEEP);
	fnip->fi_subdirlen = (int)len;
	bcopy(strbuff, fnip->fi_subdir, len);

	/*
	 * Get the key
	 */
	if (uap->flags & MS_SYSSPACE)
		error = copystr(args.key, strbuff, sizeof (strbuff), &len);
	else
		error = copyinstr(args.key, strbuff, sizeof (strbuff), &len);
	if (error) {
		error = EFAULT;
		goto errout;
	}
	fnip->fi_key = kmem_alloc(len, KM_SLEEP);
	fnip->fi_keylen = (int)len;
	bcopy(strbuff, fnip->fi_key, len);

	/*
	 * Is this a direct mount?
	 */
	if (args.direct == 1)
		fnip->fi_flags |= MF_DIRECT;

	/*
	 * Setup netconfig.
	 * Can I pass in knconf as mount argument? what
	 * happens when the daemon gets restarted?
	 */
	if ((error = lookupname("/dev/ticotsord", UIO_SYSSPACE, FOLLOW,
	    NULLVPP, &kvp)) != 0) {
		cmn_err(CE_WARN, "autofs: lookupname: %d", error);
		goto errout;
	}

	fnip->fi_knconf.knc_rdev = kvp->v_rdev;
	fnip->fi_knconf.knc_protofmly = NC_LOOPBACK;
	fnip->fi_knconf.knc_semantics = NC_TPI_COTS_ORD;
	VN_RELE(kvp);

	/*
	 * Make the root vnode
	 */
	rootfnp = auto_makefnnode(VDIR, vfsp, fnip->fi_path, cr);
	if (rootfnp == NULL) {
		error = ENOMEM;
		goto errout;
	}
	rootvp = fntovn(rootfnp);
	VN_HOLD(rootvp);

	rootvp->v_flag |= VROOT;
	rootfnp->fn_mode = AUTOFS_MODE;
	rootfnp->fn_parent = rootfnp;
	/* account for ".." entry */
	rootfnp->fn_linkcnt = rootfnp->fn_size = 1;
	fnip->fi_rootvp = rootvp;

	/*
	 * Add to list of top level AUTOFS' if it is being mounted by
	 * a user level process.
	 */
	if (!(uap->flags & MS_SYSSPACE)) {
		mutex_enter(&fnnode_list_lock);
		if (fnnode_list == NULL)
			fnnode_list = rootfnp;
		else {
			rootfnp->fn_next = fnnode_list;
			fnnode_list = rootfnp;
		}
		mutex_exit(&fnnode_list_lock);
	}

	AUTOFS_DPRINT((5, "auto_mount: vfs %p root %p fnip %p return %d\n",
	    (void *)vfsp, (void *)rootvp, (void *)fnip, error));

	return (0);

errout:
	ASSERT(fnip != NULL);
	ASSERT((uap->flags & MS_REMOUNT) == 0);

	if (fnip->fi_addr.buf != NULL)
		kmem_free(fnip->fi_addr.buf, fnip->fi_addr.len);
	if (fnip->fi_path != NULL)
		kmem_free(fnip->fi_path, fnip->fi_pathlen);
	if (fnip->fi_opts != NULL)
		kmem_free(fnip->fi_opts, fnip->fi_optslen);
	if (fnip->fi_map != NULL)
		kmem_free(fnip->fi_map, fnip->fi_maplen);
	if (fnip->fi_subdir != NULL)
		kmem_free(fnip->fi_subdir, fnip->fi_subdirlen);
	if (fnip->fi_key != NULL)
		kmem_free(fnip->fi_key, fnip->fi_keylen);
	kmem_free(fnip, sizeof (*fnip));

	AUTOFS_DPRINT((5, "auto_mount: vfs %p root %p fnip %p return %d\n",
	    (void *)vfsp, (void *)rootvp, (void *)fnip, error));

	return (error);
}

/* ARGSUSED */
static int
auto_unmount(vfs_t *vfsp, cred_t *cr)
{
	fninfo_t *fnip;
	vnode_t *rvp;
	fnnode_t *rfnp, *fnp, **fnpp;

	fnip = vfstofni(vfsp);
	AUTOFS_DPRINT((4, "auto_unmount vfsp %p fnip %p\n", (void *)vfsp,
	    (void *)fnip));

	if (!suser(cr))
		return (EPERM);

	ASSERT(vn_vfswlock_held(vfsp->vfs_vnodecovered));
	rvp = fnip->fi_rootvp;
	rfnp = vntofn(rvp);

	if (rvp->v_count > 1)
		return (EBUSY);

	if (rfnp->fn_dirents != NULL)
		return (EBUSY);

	/*
	 * The root vnode is on the linked list of root fnnodes only if
	 * this was not a trigger node. Since we have no way of knowing,
	 * if we don't find it, then we assume it was a trigger node.
	 */
	mutex_enter(&fnnode_list_lock);
	fnpp = &fnnode_list;
	for (;;) {
		fnp = *fnpp;
		if (fnp == NULL)
			/*
			 * Must be a trigger node.
			 */
			break;
		if (fnp == rfnp) {
			*fnpp = fnp->fn_next;
			fnp->fn_next = NULL;
			break;
		}
		fnpp = &fnp->fn_next;
	}
	mutex_exit(&fnnode_list_lock);

	ASSERT(rvp->v_count == 1);
	ASSERT(rfnp->fn_size == 1);
	ASSERT(rfnp->fn_linkcnt == 1);
	/*
	 * The following drops linkcnt to 0, therefore the disconnect is
	 * not attempted when auto_inactive() is called by
	 * vn_rele(). This is necessary because we have nothing to get
	 * disconnected from since we're the root of the filesystem. As a
	 * side effect the node is not freed, therefore I should free the
	 * node here.
	 *
	 * XXX - I really need to think of a better way of doing this.
	 */
	rfnp->fn_size--;
	rfnp->fn_linkcnt--;

	/*
	 * release of last reference causes node
	 * to be freed
	 */
	VN_RELE(rvp);
	rfnp->fn_parent = NULL;

	auto_freefnnode(rfnp);

	kmem_free(fnip->fi_addr.buf, fnip->fi_addr.len);
	kmem_free(fnip->fi_path, fnip->fi_pathlen);
	kmem_free(fnip->fi_map, fnip->fi_maplen);
	kmem_free(fnip->fi_subdir, fnip->fi_subdirlen);
	kmem_free(fnip->fi_key, fnip->fi_keylen);
	kmem_free(fnip->fi_opts, fnip->fi_optslen);
	kmem_free(fnip, sizeof (*fnip));
	AUTOFS_DPRINT((5, "auto_unmount: return=0\n"));

	return (0);
}


/*
 * find root of autofs
 */
static int
auto_root(vfs_t *vfsp, vnode_t **vpp)
{
	*vpp = (vnode_t *)vfstofni(vfsp)->fi_rootvp;
	VN_HOLD(*vpp);

	AUTOFS_DPRINT((5, "auto_root: vfs %p, *vpp %p\n", (void *)vfsp,
	    (void *)*vpp));
	return (0);
}

/*
 * Get file system statistics.
 */
static int
auto_statvfs(vfs_t *vfsp, struct statvfs64 *sbp)
{
	dev32_t d32;

	AUTOFS_DPRINT((4, "auto_statvfs %p\n", (void *)vfsp));

	bzero(sbp, sizeof (*sbp));
	sbp->f_bsize	= vfsp->vfs_bsize;
	sbp->f_frsize	= sbp->f_bsize;
	sbp->f_blocks	= (fsblkcnt64_t)0;
	sbp->f_bfree	= (fsblkcnt64_t)0;
	sbp->f_bavail	= (fsblkcnt64_t)0;
	sbp->f_files	= (fsfilcnt64_t)0;
	sbp->f_ffree	= (fsfilcnt64_t)0;
	sbp->f_favail	= (fsfilcnt64_t)0;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sbp->f_fsid	= d32;
	(void) strcpy(sbp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sbp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sbp->f_namemax = MAXNAMELEN;
	(void) strcpy(sbp->f_fstr, MNTTYPE_AUTOFS);

	return (0);
}
