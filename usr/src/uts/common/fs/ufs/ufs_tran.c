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
 * 	Copyright (c) 1986-1989,1996-1998 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)ufs_trans.c	1.63	98/02/06 SMI"

#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/t_lock.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/thread.h>
#include <sys/vfs.h>
#include <sys/errno.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_trans.h>
#include <sys/fs/ufs_inode.h>
#include <sys/fs/ufs_fs.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/fs/ufs_quota.h>
#include <sys/fs/ufs_panic.h>
#include <sys/fs/ufs_bio.h>
#include <sys/cmn_err.h>
#include <sys/file.h>
#include <sys/debug.h>

kmutex_t	ufs_trans_lock;

/*
 * SDS 4.0 and earlier versions of SDS can't handle direct writes to trans
 *	Later versions of SDS will enable writes to trans devices by
 *	setting ufs_trans_directio.
 */
int ufs_trans_directio;

void
ufs_trans_init()
{
	mutex_init(&ufs_trans_lock, NULL, MUTEX_DEFAULT, NULL);
}

static struct ufstrans	*ufstrans;

struct ufstrans *
ufs_trans_set(dev_t dev, struct ufstransops *ops, void *data)
{
	struct ufstrans	*ut;

	mutex_enter(&ufs_trans_lock);
	ut = ufstrans;
	while (ut != NULL && ut->ut_dev != dev)
		ut = ut->ut_next;

	if (ut == NULL) {
		ut = (struct ufstrans *)kmem_zalloc(sizeof (*ut), KM_SLEEP);
		ut->ut_dev = dev;
		ut->ut_data = data;
		ut->ut_ops = ops;
		ut->ut_next = ufstrans;
		ufstrans = ut;
	}
	mutex_exit(&ufs_trans_lock);
	return (ut);
}

void
ufs_trans_reset(dev_t dev)
{
	struct ufstrans	*ut;
	struct ufstrans	**utp;

	mutex_enter(&ufs_trans_lock);
	for (ut = NULL, utp = &ufstrans; *utp != NULL; utp = &ut->ut_next) {
		ut = *utp;
		if (ut->ut_dev == dev) {
			*utp = ut->ut_next;
			kmem_free(ut, sizeof (*ut));
			break;
		}
	}
	mutex_exit(&ufs_trans_lock);
}

/*
 * The embedded logging will replace any entry entered by the trans device
 */
struct ufstrans *
ufs_trans_replace(dev_t dev, struct ufstransops *ops, void *data)
{
	struct ufstrans	*ut, *myut;
	struct ufstrans	**utp;

	mutex_enter(&ufs_trans_lock);
	myut = NULL;
	for (utp = &ufstrans; *utp != NULL; utp = &ut->ut_next) {
		ut = *utp;
		if (ut->ut_dev != dev)
			continue;
		ASSERT(ut->ut_ops == ops);
		myut = ut;
		break;
	}
	/*
	 * Ignore if our entry is already on the list. Otherwise, add it
	 */
	if (myut == NULL) {
		myut = (struct ufstrans *)kmem_zalloc(sizeof (*myut), KM_SLEEP);
		myut->ut_dev = dev;
		myut->ut_data = data;
		myut->ut_ops = ops;
		myut->ut_next = ufstrans;
		ufstrans = myut;
	}
	mutex_exit(&ufs_trans_lock);
	return (myut);
}

/*
 * mounting a fs; check for metatrans device
 */
struct ufstrans *
ufs_trans_get(dev_t dev, struct vfs *vfsp)
{
	struct ufstrans	*ut;

	mutex_enter(&ufs_trans_lock);
	for (ut = ufstrans; ut; ut = ut->ut_next)
		if (ut->ut_dev == dev) {
			ut->ut_vfsp = vfsp;
			ut->ut_validfs = UT_MOUNTED;
			break;
		}
	mutex_exit(&ufs_trans_lock);
	return (ut);
}
/*
 * check for trans device
 */
int
ufs_trans_check(dev_t dev)
{
	struct ufstrans	*ut;

	mutex_enter(&ufs_trans_lock);
	for (ut = ufstrans; ut; ut = ut->ut_next)
		if (ut->ut_dev == dev)
			break;
	mutex_exit(&ufs_trans_lock);
	return (ut != NULL);
}

/*
 * umounting a fs; mark metatrans device as unmounted
 */
int
ufs_trans_put(dev_t dev)
{
	int		error	= 0;
	struct ufstrans	*ut;

	mutex_enter(&ufs_trans_lock);
	for (ut = ufstrans; ut; ut = ut->ut_next)
		if (ut->ut_dev == dev) {
			/* hlock in progress; unmount fails */
			if (ut->ut_validfs == UT_HLOCKING)
				error = EAGAIN;
			else
				ut->ut_validfs = UT_UNMOUNTED;
			break;
		}
	mutex_exit(&ufs_trans_lock);
	return (error);
}
/*
 * hlock any file systems w/errored metatrans devices
 */
int
ufs_trans_hlock()
{
	struct ufstrans	*ut;
	struct ufsvfs	*ufsvfsp;
	struct lockfs	lockfs;
	int		error;
	int		retry	= 0;

	/*
	 * find fs's that paniced or have errored logging devices
	 */
	mutex_enter(&ufs_trans_lock);
	for (ut = ufstrans; ut; ut = ut->ut_next) {
		/*
		 * not mounted; continue
		 */
		if (ut->ut_vfsp == NULL || (ut->ut_validfs == UT_UNMOUNTED))
			continue;
		/*
		 * disallow unmounts (hlock occurs below)
		 */
		ufsvfsp = (struct ufsvfs *)ut->ut_vfsp->vfs_data;
		if (TRANS_ISERROR(ufsvfsp))
			ut->ut_validfs = UT_HLOCKING;
	}
	mutex_exit(&ufs_trans_lock);

	/*
	 * hlock the fs's that paniced or have errored logging devices
	 */
again:
	mutex_enter(&ufs_trans_lock);
	for (ut = ufstrans; ut; ut = ut->ut_next)
		if (ut->ut_validfs == UT_HLOCKING)
			break;
	mutex_exit(&ufs_trans_lock);
	if (ut == NULL)
		return (retry);
	/*
	 * hlock the file system
	 */
	ufsvfsp = (struct ufsvfs *)ut->ut_vfsp->vfs_data;
	(void) ufs_fiolfss(ufsvfsp->vfs_root, &lockfs);
	if (!LOCKFS_IS_ELOCK(&lockfs)) {
		lockfs.lf_lock = LOCKFS_HLOCK;
		lockfs.lf_flags = 0;
		lockfs.lf_comlen = 0;
		lockfs.lf_comment = NULL;
		error = ufs_fiolfs(ufsvfsp->vfs_root, &lockfs, 0);
		/*
		 * retry after awhile; another app currently doing lockfs
		 */
		if (error == EBUSY || error == EINVAL)
			retry = 1;
	} else {
		if (ufsfx_get_failure_qlen() > 0) {
			if (mutex_tryenter(&ufs_fix.uq_mutex)) {
				ufs_fix.uq_lowat = ufs_fix.uq_ne;
				cv_broadcast(&ufs_fix.uq_cv);
				mutex_exit(&ufs_fix.uq_mutex);
			}
		}
		retry = 1;
	}

	/*
	 * allow unmounts
	 */
	ut->ut_validfs = UT_MOUNTED;
	goto again;
}

/*
 * wakeup the hlock thread
 */
/*ARGSUSED*/
void
ufs_trans_onerror()
{
	mutex_enter(&ufs_hlock.uq_mutex);
	ufs_hlock.uq_ne = ufs_hlock.uq_lowat;
	cv_broadcast(&ufs_hlock.uq_cv);
	mutex_exit(&ufs_hlock.uq_mutex);
}

void
ufs_trans_sbupdate(struct ufsvfs *ufsvfsp, struct vfs *vfsp, top_t topid)
{
	if (curthread->t_flag & T_DONTBLOCK) {
		sbupdate(vfsp);
		return;
	} else {

		if (panicstr && TRANS_ISTRANS(ufsvfsp))
			return;

		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_ASYNC(ufsvfsp, topid, TOP_SBUPDATE_SIZE);
		sbupdate(vfsp);
		TRANS_END_ASYNC(ufsvfsp, topid, TOP_SBUPDATE_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;
	}
}

ufs_trans_syncip(struct inode *ip, int bflags, int iflag, top_t topid)
{
	int		error;
	struct ufsvfs	*ufsvfsp;

	if (curthread->t_flag & T_DONTBLOCK)
		return (ufs_syncip(ip, bflags, iflag));
	else {
		ufsvfsp = ip->i_ufsvfs;
		if (ufsvfsp == NULL)
			return (0);

		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_ASYNC(ufsvfsp, topid, TOP_SYNCIP_SIZE);
		error = ufs_syncip(ip, bflags, iflag);
		TRANS_END_ASYNC(ufsvfsp, topid, TOP_SYNCIP_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;
	}
	return (error);
}

void
ufs_trans_iupdat(struct inode *ip, int waitfor)
{
	struct ufsvfs	*ufsvfsp;

	if (curthread->t_flag & T_DONTBLOCK) {
		rw_enter(&ip->i_contents, RW_WRITER);
		ufs_iupdat(ip, waitfor);
		rw_exit(&ip->i_contents);
		return;
	} else {
		ufsvfsp = ip->i_ufsvfs;

		if (panicstr && TRANS_ISTRANS(ufsvfsp))
			return;

		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_ASYNC(ufsvfsp, TOP_IUPDAT, TOP_IUPDAT_SIZE(ip));
		rw_enter(&ip->i_contents, RW_WRITER);
		ufs_iupdat(ip, waitfor);
		rw_exit(&ip->i_contents);
		TRANS_END_ASYNC(ufsvfsp, TOP_IUPDAT, TOP_IUPDAT_SIZE(ip));
		curthread->t_flag &= ~T_DONTBLOCK;
	}
}

void
ufs_trans_sbwrite(struct ufsvfs *ufsvfsp, top_t topid)
{
	if (curthread->t_flag & T_DONTBLOCK) {
		mutex_enter(&ufsvfsp->vfs_lock);
		ufs_sbwrite(ufsvfsp);
		mutex_exit(&ufsvfsp->vfs_lock);
		return;
	} else {

		if (panicstr && TRANS_ISTRANS(ufsvfsp))
			return;

		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_ASYNC(ufsvfsp, topid, TOP_SBWRITE_SIZE);
		mutex_enter(&ufsvfsp->vfs_lock);
		ufs_sbwrite(ufsvfsp);
		mutex_exit(&ufsvfsp->vfs_lock);
		TRANS_END_ASYNC(ufsvfsp, topid, TOP_SBWRITE_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;
	}
}

/*ARGSUSED*/
int
ufs_trans_push_si(struct ufstrans *ut, delta_t dtyp, int ignore)
{
	struct ufsvfs	*ufsvfsp;
	struct fs	*fs;

	ufsvfsp = (struct ufsvfs *)ut->ut_vfsp->vfs_data;
	fs = ufsvfsp->vfs_fs;
	mutex_enter(&ufsvfsp->vfs_lock);
	TRANS_LOG(ufsvfsp, (char *)fs->fs_u.fs_csp,
		ldbtob(fsbtodb(fs, fs->fs_csaddr)), fs->fs_cssize);
	mutex_exit(&ufsvfsp->vfs_lock);
	return (0);
}

/*ARGSUSED*/
int
ufs_trans_push_buf(struct ufstrans *ut, delta_t dtyp, daddr_t bno)
{
	ufsvfs_t	*ufsvfsp	= (ufsvfs_t *)(ut->ut_vfsp->vfs_data);
	struct buf	*bp;

	bp = (struct buf *)UFS_GETBLK(ufsvfsp, ut->ut_dev, bno, 1);
	if (bp == NULL)
		return (ENOENT);

	if (bp->b_flags & B_DELWRI) {
		/*
		 * Do not use brwrite() here since the buffer is already
		 * marked for retry or not by the code that called
		 * TRANS_BUF().
		 */
		UFS_BWRITE(ufsvfsp, bp);
		return (0);
	}
	/*
	 * If we did not find the real buf for this block above then
	 * clear the dev so the buf won't be found by mistake
	 * for this block later.  We had to allocate at least a 1 byte
	 * buffer to keep brelse happy.
	 */
	if (bp->b_bufsize == 1) {
		bp->b_dev = (o_dev_t)NODEV;
		bp->b_edev = NODEV;
		bp->b_flags = B_KERNBUF;
	}
	brelse(bp);
	return (ENOENT);
}

/*ARGSUSED*/
ufs_trans_push_inode(struct ufstrans *ut, delta_t dtyp, ino_t ino)
{
	int		error;
	struct inode	*ip;
#ifdef QUOTA
	struct ufsvfs	*ufsvfsp = (struct ufsvfs *)ut->ut_vfsp->vfs_data;


	/*
	 * Make the quota subsystem non-quiescent (if the file system has
	 * not been forcibly unmounted).
	 */
	if (ufsvfsp)
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
#endif /* QUOTA */

	error = ufs_iget(ut->ut_vfsp, ino, &ip, kcred);

#ifdef QUOTA
	if (ufsvfsp)
		rw_exit(&ufsvfsp->vfs_dqrwlock);
#endif /* QUOTA */
	if (error)
		return (ENOENT);

	if (ip->i_flag & (IUPD|IACC|ICHG|IMOD|IMODACC|IATTCHG)) {
		rw_enter(&ip->i_contents, RW_WRITER);
		ufs_iupdat(ip, 1);
		rw_exit(&ip->i_contents);
		VN_RELE(ITOV(ip));
		return (0);
	}
	VN_RELE(ITOV(ip));
	return (ENOENT);
}
/*
 * DEBUG ROUTINES
 *	These routines maintain the metadata map (matamap)
 */
/*
 * update the metadata map at mount
 */
static int
ufs_trans_mata_mount_scan(struct inode *ip, void *arg)
{
	/*
	 * wrong file system; keep looking
	 */
	if (ip->i_ufsvfs != (struct ufsvfs *)arg)
		return (0);

	/*
	 * load the metadata map
	 */
	rw_enter(&ip->i_contents, RW_WRITER);
	ufs_trans_mata_iget(ip);
	rw_exit(&ip->i_contents);
	return (0);
}
void
ufs_trans_mata_mount(struct ufsvfs *ufsvfsp)
{
	struct fs	*fs	= ufsvfsp->vfs_fs;
	ino_t		ino;
	int		i;

	/*
	 * put static metadata into matamap
	 *	superblock
	 *	cylinder groups
	 *	inode groups
	 *	existing inodes
	 */
	TRANS_MATAADD(ufsvfsp, ldbtob(SBLOCK), fs->fs_sbsize);

	for (ino = i = 0; i < fs->fs_ncg; ++i, ino += fs->fs_ipg) {
		TRANS_MATAADD(ufsvfsp,
		    ldbtob(fsbtodb(fs, cgtod(fs, i))), fs->fs_cgsize);
		TRANS_MATAADD(ufsvfsp,
		    ldbtob(fsbtodb(fs, itod(fs, ino))),
		    fs->fs_ipg * sizeof (struct dinode));
	}
	(void) ufs_scan_inodes(0, ufs_trans_mata_mount_scan, ufsvfsp);
}
/*
 * clear the metadata map at umount
 */
void
ufs_trans_mata_umount(struct ufsvfs *ufsvfsp)
{
	TRANS_MATACLR(ufsvfsp);
}

/*
 * summary info (may be extended during growfs test)
 */
void
ufs_trans_mata_si(struct ufsvfs *ufsvfsp, struct fs *fs)
{
	TRANS_MATAADD(ufsvfsp, ldbtob(fsbtodb(fs, fs->fs_csaddr)),
			fs->fs_cssize);
}
/*
 * scan an allocation block (either inode or true block)
 */
static void
ufs_trans_mata_direct(
	struct inode *ip,
	daddr_t *fragsp,
	daddr32_t *blkp,
	unsigned int nblk)
{
	int		i;
	daddr_t		frag;
	u_long		nb;
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	for (i = 0; i < nblk && *fragsp; ++i, ++blkp)
		if ((frag = *blkp) != 0) {
			if (*fragsp > fs->fs_frag) {
				nb = fs->fs_bsize;
				*fragsp -= fs->fs_frag;
			} else {
				nb = *fragsp * fs->fs_fsize;
				*fragsp = 0;
			}
			TRANS_MATAADD(ufsvfsp, ldbtob(fsbtodb(fs, frag)), nb);
		}
}
/*
 * scan an indirect allocation block (either inode or true block)
 */
static void
ufs_trans_mata_indir(
	struct inode *ip,
	daddr_t *fragsp,
	daddr_t frag,
	int level)
{
	struct ufsvfs *ufsvfsp	= ip->i_ufsvfs;
	struct fs *fs = ufsvfsp->vfs_fs;
	int ne = fs->fs_bsize / (int)sizeof (daddr32_t);
	int i;
	struct buf *bp;
	daddr32_t *blkp;
	o_mode_t ifmt = ip->i_mode & IFMT;

	bp = UFS_BREAD(ufsvfsp, ip->i_dev, fsbtodb(fs, frag), fs->fs_bsize);
	if (bp->b_flags & B_ERROR) {
		brelse(bp);
		return;
	}
	blkp = bp->b_un.b_daddr;

	if (level || (ifmt == IFDIR) || (ifmt == IFSHAD) ||
	    (ip == ip->i_ufsvfs->vfs_qinod))
		ufs_trans_mata_direct(ip, fragsp, blkp, ne);

	if (level)
		for (i = 0; i < ne && *fragsp; ++i, ++blkp)
			ufs_trans_mata_indir(ip, fragsp, *blkp, level-1);
	brelse(bp);
}
/*
 * put appropriate metadata into matamap for this inode
 */
void
ufs_trans_mata_iget(struct inode *ip)
{
	int		i;
	daddr_t		frags	= dbtofsb(ip->i_fs, ip->i_blocks);
	o_mode_t	ifmt 	= ip->i_mode & IFMT;

	if (frags && ((ifmt == IFDIR) || (ifmt == IFSHAD) ||
	    (ip == ip->i_ufsvfs->vfs_qinod)))
		ufs_trans_mata_direct(ip, &frags, &ip->i_db[0], NDADDR);

	if (frags)
		ufs_trans_mata_direct(ip, &frags, &ip->i_ib[0], NIADDR);

	for (i = 0; i < NIADDR && frags; ++i)
		if (ip->i_ib[i])
			ufs_trans_mata_indir(ip, &frags, ip->i_ib[i], i);
}
/*
 * freeing possible metadata (block of user data)
 */
void
ufs_trans_mata_free(struct ufsvfs *ufsvfsp, offset_t mof, off_t nb)
{
	TRANS_MATADEL(ufsvfsp, mof, nb);

}
/*
 * allocating metadata
 */
void
ufs_trans_mata_alloc(
	struct ufsvfs *ufsvfsp,
	struct inode *ip,
	daddr_t frag,
	u_long nb,
	int indir)
{
	struct fs	*fs	= ufsvfsp->vfs_fs;
	o_mode_t	ifmt 	= ip->i_mode & IFMT;

	if (indir || ((ifmt == IFDIR) || (ifmt == IFSHAD) ||
	    (ip == ip->i_ufsvfs->vfs_qinod)))
		TRANS_MATAADD(ufsvfsp, ldbtob(fsbtodb(fs, frag)), nb);
}
/*
 * END DEBUG ROUTINES
 */

/*
 * ufs_trans_dir is used to declare a directory delta
 */
int
ufs_trans_dir(struct inode *ip, off_t offset)
{
	daddr_t	bn;
	int	contig = 0, error;
	int	dolock;

	ASSERT(ip);
	dolock = (rw_owner(&ip->i_contents) != curthread);
	if (dolock)
		rw_enter(&ip->i_contents, RW_WRITER);
	error = bmap_read(ip, (u_offset_t)offset, &bn, &contig);
	if (dolock)
		rw_exit(&ip->i_contents);
	if (error || (bn == UFS_HOLE)) {
		cmn_err(CE_WARN, "ufs_trans_dir - could not get block"
		    " number error = %d bn = %d\n", error, (int)bn);
		if (error == 0)	/* treat UFS_HOLE as an I/O error */
			error = EIO;
		return (error);
	}
	TRANS_DELTA(ip->i_ufsvfs, ldbtob(bn), DIRBLKSIZ, DT_DIR, 0, 0);
	return (error);
}
/*ARGSUSED*/
int
ufs_trans_push_quota(struct ufstrans *ut, delta_t dtyp, struct dquot *dqp)
{
	struct ufsvfs	*ufsvfsp = dqp->dq_ufsvfsp;

	/*
	 * Make the quota subsystem non-quiescent (ufsvfsp can be NULL
	 * if the DQ_ERROR is set).
	 */
	if (ufsvfsp)
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
	mutex_enter(&dqp->dq_lock);

	/*
	 * If this transaction has been cancelled by closedq_scan_inode(),
	 * then bail out now.  We don't call dqput() in this case because
	 * it has already been done.
	 */
	if ((dqp->dq_flags & DQ_TRANS) == 0) {
		mutex_exit(&dqp->dq_lock);
		if (ufsvfsp)
			rw_exit(&ufsvfsp->vfs_dqrwlock);
		return (0);
	}

	if (dqp->dq_flags & DQ_ERROR) {
		/*
		 * Paranoia to make sure that there is at least one
		 * reference to the dquot struct.  We are done with
		 * the dquot (due to an error) so clear logging
		 * specific markers.
		 */
		ASSERT(dqp->dq_cnt >= 1);
		dqp->dq_flags &= ~DQ_TRANS;
		dqput(dqp);
		mutex_exit(&dqp->dq_lock);
		if (ufsvfsp)
			rw_exit(&ufsvfsp->vfs_dqrwlock);
		return (1);
	}

	if (dqp->dq_flags & (DQ_MOD | DQ_BLKS | DQ_FILES)) {
		ASSERT((dqp->dq_mof != UFS_HOLE) && (dqp->dq_mof != 0));
		TRANS_LOG(ufsvfsp, (caddr_t)&dqp->dq_dqb,
		    dqp->dq_mof, (int)sizeof (struct dqblk));
		/*
		 * Paranoia to make sure that there is at least one
		 * reference to the dquot struct.  Clear the
		 * modification flag because the operation is now in
		 * the log.  Also clear the logging specific markers
		 * that were set in ufs_trans_quota().
		 */
		ASSERT(dqp->dq_cnt >= 1);
		dqp->dq_flags &= ~(DQ_MOD | DQ_TRANS);
		dqput(dqp);
	}

	/*
	 * At this point, the logging specific flag should be clear,
	 * but add paranoia just in case something has gone wrong.
	 */
	ASSERT((dqp->dq_flags & DQ_TRANS) == 0);
	mutex_exit(&dqp->dq_lock);
	if (ufsvfsp)
		rw_exit(&ufsvfsp->vfs_dqrwlock);
	return (0);
}

/*
 * ufs_trans_quota take in a uid, allocates the disk space, placing the
 * quota record into the metamap, then declares the delta.
 */
/*ARGSUSED*/
void
ufs_trans_quota(struct dquot *dqp)
{

	struct inode	*qip = dqp->dq_ufsvfsp->vfs_qinod;

	ASSERT(qip);
	ASSERT(MUTEX_HELD(&dqp->dq_lock));
	ASSERT(dqp->dq_flags & DQ_MOD);
	ASSERT(dqp->dq_mof != 0);
	ASSERT(dqp->dq_mof != UFS_HOLE);

	/*
	 * Mark this dquot to indicate that we are starting a logging
	 * file system operation for this dquot.  Also increment the
	 * reference count so that the dquot does not get reused while
	 * it is on the mapentry_t list.  DQ_TRANS is cleared and the
	 * reference count is decremented by ufs_trans_push_quota.
	 *
	 * If the file system is force-unmounted while there is a
	 * pending quota transaction, then closedq_scan_inode() will
	 * clear the DQ_TRANS flag and decrement the reference count.
	 *
	 * Since deltamap_add() drops multiple transactions to the
	 * same dq_mof and ufs_trans_push_quota() won't get called,
	 * we use DQ_TRANS to prevent repeat transactions from
	 * incrementing the reference count (or calling TRANS_DELTA()).
	 */
	if ((dqp->dq_flags & DQ_TRANS) == 0) {
		dqp->dq_flags |= DQ_TRANS;
		dqp->dq_cnt++;
		TRANS_DELTA(qip->i_ufsvfs, dqp->dq_mof, sizeof (struct dqblk),
		    DT_QR, ufs_trans_push_quota, (u_long)dqp);
	}
}

void
ufs_trans_dqrele(struct dquot *dqp)
{
	struct ufsvfs	*ufsvfsp;


	if (curthread->t_flag & T_DONTBLOCK) {
		dqrele(dqp);
		return;
	} else {
		ufsvfsp = dqp->dq_ufsvfsp;

		curthread->t_flag |= T_DONTBLOCK;
		TRANS_BEGIN_ASYNC(ufsvfsp, TOP_QUOTA, TOP_QUOTA_SIZE);
		dqrele(dqp);
		TRANS_END_ASYNC(ufsvfsp, TOP_QUOTA, TOP_QUOTA_SIZE);
		curthread->t_flag &= ~T_DONTBLOCK;
	}
}

/*
 * Calculate the amount of log space that needs to be reserved for this
 * trunc request.  If the amount of log space is too large, then
 * calculate the the size that the requests needs to be split into.
 */
int ufs_trans_max_resv = TOP_MAX_RESV;	/* will be adjusted for testing */
long ufs_trans_avgbfree = 0;		/* will be adjusted for testing */

static void
ufs_trans_trunc_resv(
	struct inode *ip,
	u_offset_t length,
	int *resvp,
	u_offset_t *residp)
{
	long		ncg;
	int		resv;
	daddr_t		nblk, nblock;
	u_offset_t	size;
	long		avgbfree;
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	/*
	 * Assume that the request will fit in 1 or 2 cg's
	 *    *resvp is the amount of log space to reserve (in bytes).
	 *    when nonzero, *residp is the number of bytes to truncate.
	 */
	*resvp = TOP_TRUNC_USUAL_RESV;
	*residp = 0;

	/*
	 * Trunc'ing up doesn't take much log space
	 */
	if (length >= ip->i_size)
		return;

	/*
	 * size of trunc in fs blocks
	 */
	size = ip->i_size - length;
	nblk = lblkno(fs, size);

	/*
	 * Adjust avgbfree (for testing)
	 */
	avgbfree = (ufs_trans_avgbfree) ? 1 : ufsvfsp->vfs_avgbfree + 1;

	/*
	 * request will probably alter 1 or 2 cg's; done
	 */
	if (nblk < avgbfree)
		return;
	/*
	 * Request will take greater than average amount of log space
	 */

	/*
	 * file's space in fs blocks (adjusted for sparse files)
	 */
	nblock = dbtofsb(fs, ip->i_blocks);
	nblk = MIN(nblk, nblock);

	/*
	 * average number of cg's needed for request
	 */
	ncg = nblk / avgbfree;
	if (ncg > fs->fs_ncg)
		ncg = fs->fs_ncg;

	/*
	 * maximum amount of log space needed for request
	 */
	resv = (ncg * fs->fs_cgsize) + TOP_TRUNC_USUAL_RESV;

	/*
	 * This request takes too much log space; it will be split
	 */
	if (resv > ufs_trans_max_resv) {
		*resvp = ufs_trans_max_resv;
		*residp = ((ufs_trans_max_resv / fs->fs_cgsize) * avgbfree)
						<< fs->fs_bshift;
	} else
		*resvp = resv;
}

int
ufs_trans_itrunc(struct inode *ip, u_offset_t length, int flags, cred_t *cr)
{
	int 		err, issync, resv, tflags;
	u_offset_t	resid, size;
	int		do_block	= 0;
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	/*
	 * Not logging; just do the trunc
	 */
	if (!TRANS_ISTRANS(ufsvfsp)) {
#ifdef QUOTA
		/*
		 * Make the quota subsystem non-quiescent.
		 */
		rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
#endif /* QUOTA */
		rw_enter(&ip->i_contents, RW_WRITER);
		err = ufs_itrunc(ip, length, flags, cr);
		rw_exit(&ip->i_contents);
#ifdef QUOTA
		rw_exit(&ufsvfsp->vfs_dqrwlock);
#endif /* QUOTA */
		return (err);
	}

	/*
	 * within the lockfs protocol but *not* part of a transaction
	 */
	do_block = curthread->t_flag & T_DONTBLOCK;
	curthread->t_flag |= T_DONTBLOCK;

	/*
	 * Trunc the file (in pieces, if necessary)
	 */
again:
	ufs_trans_trunc_resv(ip, length, &resv, &resid);
	TRANS_BEGIN_CSYNC(ufsvfsp, issync, TOP_ITRUNC, resv);
#ifdef QUOTA
	/*
	 * Make the quota subsystem non-quiescent.
	 */
	rw_enter(&ufsvfsp->vfs_dqrwlock, RW_READER);
#endif /* QUOTA */
	rw_enter(&ip->i_contents, RW_WRITER);
	if (resid) {
		/*
		 * sanity check on resid
		 */
		size = ip->i_size;
		if (resid > (size - length))
			resid = size - length;
		/*
		 * Partially trunc file down to desired size (length).
		 *	Only retain I_FREE on the last partial trunc
		 */
		tflags = ((size - resid) == length) ? flags : flags & ~I_FREE;
		err = ufs_itrunc(ip, size - resid, tflags, cr);
		if (ip->i_size == length)
			resid = 0;
	} else
		err = ufs_itrunc(ip, length, flags, cr);
	if (!do_block)
		curthread->t_flag &= ~T_DONTBLOCK;
	rw_exit(&ip->i_contents);
#ifdef QUOTA
	rw_exit(&ufsvfsp->vfs_dqrwlock);
#endif /* QUOTA */
	TRANS_END_CSYNC(ufsvfsp, err, issync, TOP_ITRUNC, resv);

	/*
	 * Trunc'ing a large file in a full file system will
	 * take awhile.  Try to speed up the process by trunc'ing
	 * the file in larger and larger pieces as more and more
	 * space is freed from the file system.
	 */
	if ((err == 0) && resid) {
		ufsvfsp->vfs_avgbfree = fs->fs_cstotal.cs_nbfree / fs->fs_ncg;
		goto again;
	}

	return (err);
}

/*
 * Calculate the amount of log space that needs to be reserved for this
 * write request.  If the amount of log space is too large, then
 * calculate the the size that the requests needs to be split into.
 */
void
ufs_trans_write_resv(
	struct inode *ip,
	struct uio *uio,
	int *resvp,
	int *residp)
{
	long		ncg;
	int		resv;
	daddr_t		nblk;
	long		avgbfree;
	struct ufsvfs	*ufsvfsp	= ip->i_ufsvfs;
	struct fs	*fs		= ufsvfsp->vfs_fs;

	/*
	 * Assume that the request will fit in 1 or 2 cg's
	 *    *resvp is the amount of log space to reserve (in bytes).
	 *    when nonzero, *residp is the number of bytes to write.
	 */
	*resvp = TOP_WRITE_USUAL_RESV;
	*residp = 0;

	/*
	 * request size in fs blocks
	 */
	nblk = lblkno(fs, blkroundup(fs, uio->uio_resid));

	/*
	 * Adjust avgbfree (for testing)
	 */
	avgbfree = (ufs_trans_avgbfree) ? 1 : ufsvfsp->vfs_avgbfree + 1;

	/*
	 * request will fit in 1 or 2 cg's; done
	 */
	if (nblk < avgbfree)
		return;
	/*
	 * Request will take greater than average amount of log space
	 */

	/*
	 * maximum number of cg's needed for request
	 */
	ncg = nblk / avgbfree;
	if (ncg > fs->fs_ncg)
		ncg = fs->fs_ncg;

	/*
	 * maximum amount of log space needed for request
	 */
	resv = (ncg * fs->fs_cgsize) + TOP_WRITE_USUAL_RESV;

	/*
	 * This request takes too much log space; it will be split
	 */
	if (resv > ufs_trans_max_resv) {
		*resvp = ufs_trans_max_resv;
		*residp = ((ufs_trans_max_resv / fs->fs_cgsize) * avgbfree)
						<< fs->fs_bshift;
	} else
		*resvp = resv;
}
/*
 * Issue write request.
 *
 * Split a large request into smaller chunks.
 */
int
ufs_trans_write(
	struct inode *ip,
	struct uio *uio,
	int ioflag,
	cred_t *cr,
	int resv,
	long resid)
{
	long		realresid;
	int		err;
	struct ufsvfs	*ufsvfsp = ip->i_ufsvfs;

	/*
	 * since the write is too big and would "HOG THE LOG" it needs to
	 * be broken up and done in pieces.  NOTE, the caller will
	 * issue the EOT after the request has been completed
	 */
	realresid = uio->uio_resid;

again:
	/*
	 * Perform partial request (uiomove will update uio for us)
	 *	Request is split up into "resid" size chunks until
	 *	"realresid" bytes have been transferred.
	 */
	uio->uio_resid = MIN(resid, realresid);
	realresid -= uio->uio_resid;
	err = wrip(ip, uio, ioflag, cr);

	/*
	 * Error or request is done; caller issues final EOT
	 */
	if (err || uio->uio_resid || (realresid == 0)) {
		uio->uio_resid += realresid;
		return (err);
	}

	/*
	 * Generate EOT for this part of the request
	 */
	rw_exit(&ip->i_contents);
	if (ioflag & (FSYNC|FDSYNC)) {
		TRANS_END_SYNC(ufsvfsp, err, TOP_WRITE_SYNC, resv);
	} else {
		TRANS_END_ASYNC(ufsvfsp, TOP_WRITE, resv);
	}

	/*
	 * Generate BOT for next part of the request
	 */
	if (ioflag & (FSYNC|FDSYNC)) {
		TRANS_BEGIN_SYNC(ufsvfsp, TOP_WRITE_SYNC, resv);
	} else {
		TRANS_BEGIN_ASYNC(ufsvfsp, TOP_WRITE, resv);
	}
	rw_enter(&ip->i_contents, RW_WRITER);
	/*
	 * Error during EOT (probably device error while writing commit rec)
	 */
	if (err)
		return (err);
	goto again;
}
