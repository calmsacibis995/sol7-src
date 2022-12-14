/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All rights reserved.  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1994,1996-1997 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)fdops.c	1.43	97/09/23 SMI"

#include "sys/types.h"
#include "sys/param.h"
#include "sys/cmn_err.h"
#include "sys/debug.h"
#include "sys/dirent.h"
#include "sys/errno.h"
#include "sys/file.h"
#include "sys/inline.h"
#include "sys/kmem.h"
#include "sys/pathname.h"
#include "sys/resource.h"
#include "sys/statvfs.h"
#include "sys/sysmacros.h"
#include "sys/systm.h"
#include "sys/uio.h"
#include "sys/vfs.h"
#include "sys/vnode.h"
#include "sys/cred.h"
#include "sys/user.h"
#include "sys/t_lock.h"
#include "fs/fs_subr.h"

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>

extern struct vfsops fdvfsops;
extern void fdinit();

static struct vfssw vfw = {
	"fd",
	(int(*)())fdinit,
	&fdvfsops,
	0
};

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops, "filesystem for fd", &vfw
};

static struct modlinkage modlinkage = {
	MODREV_1, (void *)&modlfs, NULL
};

_init()
{
	return (mod_install(&modlinkage));
}

/*
 * Cannot unload this module (for now).  Hence no _fini() routine.
 */

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}

#define	round(r)	(((r)+sizeof (int)-1)&(~(sizeof (int)-1)))
#define	fdtoi(n)	((n)+100)

#define	FDDIRSIZE 14
struct fddirect {
	short	d_ino;
	char	d_name[FDDIRSIZE];
};

#define	FDROOTINO	2
#define	FDSDSIZE	sizeof (struct fddirect)
#define	FDNSIZE		10

int			fdfstype = 0;
static int		fdmounted = 0;
static struct vfs	*fdvfs;
static dev_t		fddev;
static major_t		fdrdev;

static kmutex_t		fdfs_lock;

static struct vnode fdvroot;

static int	fdopen(), fdclose(), fdread(), fdgetattr(), fdaccess();
static int	fdlookup(), fdcreate(), fdreaddir();
static void	fdinactive();

static struct vnodeops fdvnodeops = {
	fdopen,
	fdclose,
	fdread,		/* read */
	fs_nosys,	/* write */
	fs_nosys,	/* ioctl */
	fs_nosys,	/* setfl */
	fdgetattr,
	fs_nosys,	/* setattr */
	fdaccess,
	fdlookup,
	fdcreate,
	fs_nosys,	/* remove */
	fs_nosys,	/* link */
	fs_nosys,	/* rename */
	fs_nosys,	/* mkdir */
	fs_nosys,	/* rmdir */
	fdreaddir,
	fs_nosys,	/* symlink */
	fs_nosys,	/* readlink */
	fs_nosys,	/* fsync */
	fdinactive,
	fs_nosys,	/* fid */
	fs_rwlock,	/* rwlock */
	fs_rwunlock,	/* rwunlock */
	fs_nosys,	/* seek */
	fs_cmp,
	fs_nosys,	/* frlock */
	fs_nosys,	/* space */
	fs_nosys,	/* realvp */
	fs_nosys,	/* getpage */
	fs_nosys,	/* putpage */
	fs_nosys_map,	/* map */
	fs_nosys_addmap, /* addmap */
	fs_nosys,	/* delmap */
	fs_nosys_poll,	/* poll */
	fs_nosys,	/* dump */
	fs_pathconf,
	fs_nosys,	/* pageio */
	fs_nosys,	/* dumpctl */
	fs_dispose,
	fs_nosys,
	fs_fab_acl,
	fs_nosys	/* shrlock */
};

static int	fdget();

/* ARGSUSED */
static int
fdopen(vpp, mode, cr)
	struct vnode **vpp;
	int mode;
	cred_t *cr;
{
	if ((*vpp)->v_type != VDIR) {
		mutex_enter(&(*vpp)->v_lock);
		(*vpp)->v_flag |= VDUP;
		mutex_exit(&(*vpp)->v_lock);
	}
	return (0);
}

/* ARGSUSED */
static int
fdclose(vp, flag, count, offset, cr)
	struct vnode *vp;
	int flag;
	int count;
	offset_t offset;
	struct cred *cr;
{
	return (0);
}

/* ARGSUSED */
static int
fdread(vp, uiop, ioflag, cr)
	register struct vnode *vp;
	register struct uio *uiop;
	int ioflag;
	struct cred *cr;
{
	static struct fddirect dotbuf[] = {
		{ FDROOTINO, "."  },
		{ FDROOTINO, ".." }
	};
	struct fddirect dirbuf;
	register int i, n;
	int minfd, maxfd, modoff, error = 0;
	int nentries = MIN(u.u_nofiles, (rlim_t)U_CURLIMIT(&u, RLIMIT_NOFILE));
	int endoff;

	endoff = (nentries + 2) * FDSDSIZE;

	if (vp->v_type != VDIR)
		return (ENOSYS);
	/*
	 * Fake up ".", "..", and the /dev/fd directory entries.
	 */
	if (uiop->uio_loffset < (offset_t)0 ||
	    uiop->uio_loffset >= (offset_t)endoff ||
	    uiop->uio_resid <= 0)
		return (0);
	ASSERT(uiop->uio_loffset <= MAXOFF_T);
	if (uiop->uio_offset < 2*FDSDSIZE) {
		error = uiomove((caddr_t)dotbuf + uiop->uio_offset,
		    MIN(uiop->uio_resid, 2*FDSDSIZE - uiop->uio_offset),
		    UIO_READ, uiop);
		if (uiop->uio_resid <= 0 || error)
			return (error);
	}
	minfd = (uiop->uio_offset - 2*FDSDSIZE)/FDSDSIZE;
	maxfd = (uiop->uio_offset + uiop->uio_resid - 1)/FDSDSIZE;
	modoff = uiop->uio_offset % FDSDSIZE;
	for (i = 0; i < FDDIRSIZE; i++)
		dirbuf.d_name[i] = '\0';
	for (i = minfd; i < MIN(maxfd, nentries); i++) {
		n = i;
		dirbuf.d_ino = fdtoi(n);
		numtos((long)n, dirbuf.d_name);
		error = uiomove((caddr_t)&dirbuf + modoff,
		    MIN(uiop->uio_resid, FDSDSIZE - modoff),
		    UIO_READ, uiop);
		if (uiop->uio_resid <= 0 || error)
			return (error);
		modoff = 0;
	}

	return (error);
}

/* ARGSUSED */
static int
fdgetattr(vp, vap, flags, cr)
	register struct vnode *vp;
	register struct vattr *vap;
	int flags;
	cred_t *cr;
{
	if (vp->v_type == VDIR) {
		vap->va_nlink = 2;
		vap->va_size = (u_offset_t)((u.u_nofiles + 2) * FDSDSIZE);
		vap->va_mode = 0555;
		vap->va_nodeid = (ino64_t)FDROOTINO;
	} else {
		vap->va_nlink = 1;
		vap->va_size = (u_offset_t)0;
		vap->va_mode = 0666;
		vap->va_nodeid = (ino64_t)fdtoi(getminor(vp->v_rdev));
	}
	vap->va_type = vp->v_type;
	vap->va_rdev = vp->v_rdev;
	vap->va_blksize = 1024;
	vap->va_nblocks = (fsblkcnt64_t)0;
	vap->va_atime = vap->va_mtime = vap->va_ctime = hrestime;
	vap->va_uid = 0;
	vap->va_gid = 0;
	vap->va_fsid = fddev;
	vap->va_vcode = 0;
	return (0);
}

/* ARGSUSED */
static int
fdaccess(vp, mode, cr)
	register struct vnode *vp;
	register int mode;
	register cred_t *cr;
{
	return (0);
}

/* ARGSUSED */
static int
fdlookup(dp, comp, vpp, pnp, flags, rdir, cr)
	struct vnode *dp;
	register char *comp;
	struct vnode **vpp;
	struct pathname *pnp;
	int flags;
	struct vnode *rdir;
	struct cred *cr;
{
	if (comp[0] == 0 || strcmp(comp, ".") == 0 || strcmp(comp, "..") == 0) {
		VN_HOLD(dp);
		*vpp = dp;
		return (0);
	}
	return (fdget(comp, vpp));
}

/* ARGSUSED */
static int
fdcreate(dvp, comp, vap, excl, mode, vpp, cr, flag)
	struct vnode *dvp;
	char *comp;
	struct vattr *vap;
	enum vcexcl excl;
	int mode;
	struct vnode **vpp;
	cred_t *cr;
	int flag;
{
	return (fdget(comp, vpp));
}

/* ARGSUSED */
static int
fdreaddir(vp, uiop, cr, eofp)
	struct vnode *vp;
	register struct uio *uiop;
	struct cred *cr;
	int *eofp;
{
	/* bp holds one dirent structure */
	u_offset_t bp[DIRENT64_RECLEN(FDNSIZE) / sizeof (u_offset_t)];
	struct dirent64 *dirent = (struct dirent64 *)bp;
	int reclen, nentries;
	register int  n;
	int oresid;
	off_t off;

	if (uiop->uio_offset < 0 || uiop->uio_resid <= 0 ||
	    (uiop->uio_offset % FDSDSIZE) != 0)
		return (ENOENT);

	ASSERT(uiop->uio_loffset <= MAXOFF_T);
	oresid = uiop->uio_resid;
	nentries = MIN(u.u_nofiles, (rlim_t)U_CURLIMIT(&u, RLIMIT_NOFILE));

	while (uiop->uio_resid > 0) {
		if ((off = uiop->uio_offset) == 0) {	/* "." */
			dirent->d_ino = (ino64_t)FDROOTINO;
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '\0';
			reclen = DIRENT64_RECLEN(1);
		} else if (off == FDSDSIZE) {		/* ".." */
			dirent->d_ino = (ino64_t)FDROOTINO;
			dirent->d_name[0] = '.';
			dirent->d_name[1] = '.';
			dirent->d_name[2] = '\0';
			reclen = DIRENT64_RECLEN(2);
		} else {
			/*
			 * Return entries corresponding to the allowable
			 * number of file descriptors for this process.
			 */
			if ((n = (off-2*FDSDSIZE)/FDSDSIZE) >= nentries)
				break;
			dirent->d_ino = (ino64_t)fdtoi(n);
			numtos((long)n, dirent->d_name);
			reclen = DIRENT64_RECLEN(strlen(dirent->d_name));
		}
		dirent->d_off = (offset_t)(uiop->uio_offset + FDSDSIZE);
		dirent->d_reclen = (u_short)reclen;

		if (reclen > uiop->uio_resid) {
			/*
			 * Error if no entries have been returned yet.
			 */
			if (uiop->uio_resid == oresid)
				return (EINVAL);
			break;
		}
		/*
		 * uiomove() updates both resid and offset by the same
		 * amount.  But we want offset to change in increments
		 * of FDSDSIZE, which is different from the number of bytes
		 * being returned to the user.  So we set uio_offset
		 * separately, ignoring what uiomove() does.
		 */
		if (uiomove((caddr_t)dirent, reclen, UIO_READ, uiop))
			return (EFAULT);
		uiop->uio_offset = off + FDSDSIZE;
	}
	if (eofp)
		*eofp = ((uiop->uio_offset-2*FDSDSIZE)/FDSDSIZE >= nentries);
	return (0);
}

/* ARGSUSED */
static void
fdinactive(vp, cr)
	register struct vnode *vp;
	cred_t *cr;
{
	mutex_enter(&vp->v_lock);
	ASSERT(vp->v_count >= 1);
	if (--vp->v_count != 0 || vp->v_type == VDIR) {
		mutex_exit(&vp->v_lock);
		return;
	}
	mutex_exit(&vp->v_lock);
	mutex_destroy(&vp->v_lock);
	kmem_free((caddr_t)vp, sizeof (vnode_t));
}

static int
fdget(char *comp, struct vnode **vpp)
{
	register int n = 0;
	register struct vnode *vp;

	while (*comp) {
		if (*comp < '0' || *comp > '9')
			return (ENOENT);
		n = 10 * n + *comp++ - '0';
	}
	vp = kmem_zalloc(sizeof (struct vnode), KM_SLEEP);
	vp->v_type = VCHR;
	vp->v_vfsp = fdvfs;
	vp->v_vfsmountedhere = NULL;
	vp->v_op = &fdvnodeops;
	vp->v_count = 1;
	vp->v_data = NULL;
	vp->v_flag = VNOMAP;
	vp->v_rdev = makedevice(fdrdev, n);
	*vpp = vp;
	return (0);
}

static int	fdmount(), fdunmount(), fdroot(), fdstatvfs();

struct vfsops fdvfsops = {
	fdmount,
	fdunmount,
	fdroot,
	fdstatvfs,
	fs_sync,
	fs_nosys,	/* vget */
	fs_nosys,	/* mountroot */
	fs_nosys	/* swapvp */
};

void
fdinit(struct vfssw *vswp, int fstype)
{
	major_t dev, rdev;

	fdfstype = fstype;
	ASSERT(fdfstype != 0);
	/*
	 * Associate VFS ops vector with this fstype.
	 */
	vswp->vsw_vfsops = &fdvfsops;

	/*
	 * Assign unique "device" numbers (reported by stat(2)).
	 */
	dev = getudev();
	rdev = getudev();
	if (dev == (major_t)-1 || rdev == (major_t)-1) {
		cmn_err(CE_WARN, "fdinit: can't get unique device numbers");
		if (dev == (major_t)-1)
			dev = 0;
		if (rdev == (major_t)-1)
			rdev = 0;
	}
	fddev = makedevice(dev, 0);
	fdrdev = rdev;
	fdmounted = 0;
	mutex_init(&fdfs_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Realistically no locking is needed here since only one fd file system
 * is mounted per system and it is always mounted on /dev/fd.  Thus the
 * vfs locking will prevent two threads from entering fdmount (if two threads
 * were trying to mount fdfs for some reason).  However, there is nothing to
 * for you to mount fdfs on /dev/fd so theoretically on thread could mount
 * fdfs on /foo while another mounts it on /dev/fd.  They could both enter
 * this routine and clobber fdvfs.  So, until fdmounted is set to 1 I need
 * to hold a mutex.  Maybe we can assume this case is uncmmon enough that
 * we don't need the mutex.
 */
/* ARGSUSED */
static int
fdmount(vfsp, mvp, uap, cr)
	struct vfs *vfsp;
	struct vnode *mvp;
	struct mounta *uap;
	struct cred *cr;
{
	register struct vnode *vp;

	if (!suser(cr))
		return (EPERM);
	if (mvp->v_type != VDIR)
		return (ENOTDIR);
	if (mvp->v_count > 1 || (mvp->v_flag & VROOT))
		return (EBUSY);
	/*
	 * Prevent duplicate mount.
	 */
	mutex_enter(&fdfs_lock);
	if (fdmounted) {
		mutex_exit(&fdfs_lock);
		return (EBUSY);
	}
	fdmounted = 1;
	mutex_exit(&fdfs_lock);
	vp = &fdvroot;
	vp->v_vfsp = vfsp;
	vp->v_vfsmountedhere = NULL;
	vp->v_op = &fdvnodeops;
	vp->v_count = 1;
	vp->v_type = VDIR;
	vp->v_data = NULL;
	vp->v_flag |= VROOT;
	vp->v_rdev = 0;
	vfsp->vfs_fstype = fdfstype;
	vfsp->vfs_data = NULL;
	vfsp->vfs_dev = fddev;
	vfs_make_fsid(&vfsp->vfs_fsid, fddev, fdfstype);
	vfsp->vfs_bsize = 1024;
	fdvfs = vfsp;
	return (0);
}

/*
 * No locking is required on unmount since fdmount insures that only one
 * fdfs mount exists in the system and the vfs locking prevents more than
 * one thread from reaching this point in the code.
 */
/* ARGSUSED */
static int
fdunmount(vfsp, cr)
	struct vfs *vfsp;
	struct cred *cr;
{
	if (!suser(cr))
		return (EPERM);
	if (fdvroot.v_count > 1)
		return (EBUSY);

	VN_RELE(&fdvroot);
	fdmounted = 0;
	fdvfs = NULL;
	return (0);
}

/* ARGSUSED */
static int
fdroot(vfsp, vpp)
	struct vfs *vfsp;
	struct vnode **vpp;
{
	struct vnode *vp = &fdvroot;

	VN_HOLD(vp);
	*vpp = vp;
	return (0);
}

/*
 * No locking required because I held the root vnode before calling this
 * function so the vfs won't disappear on me.  To be more explicit:
 * fdvroot.v_count will be greater than 1 so fdunmount will just return.
 */
static int
fdstatvfs(struct vfs *vfsp, struct statvfs64 *sp)
{
	dev32_t d32;

	bzero(sp, sizeof (*sp));
	sp->f_bsize = 1024;
	sp->f_frsize = 1024;
	sp->f_blocks = (fsblkcnt64_t)0;
	sp->f_bfree = (fsblkcnt64_t)0;
	sp->f_bavail = (fsblkcnt64_t)0;
	sp->f_files = (fsfilcnt64_t)
		(MIN(u.u_nofiles, U_CURLIMIT(&u, RLIMIT_NOFILE) + 2));
	sp->f_ffree = (fsfilcnt64_t)0;
	sp->f_favail = (fsfilcnt64_t)0;
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid = d32;
	(void) strcpy(sp->f_basetype, vfssw[fdfstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = FDNSIZE;
	(void) strcpy(sp->f_fstr, "/dev/fd");
	(void) strcpy(&sp->f_fstr[8], "/dev/fd");
	return (0);
}
