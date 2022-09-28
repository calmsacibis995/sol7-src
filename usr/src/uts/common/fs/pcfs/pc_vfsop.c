/*
 * Copyright (c) 1990-1997 by Sun Microsystems, Inc.  All rights reserved.
 * All Rights Reserved.
 */

#pragma ident	"@(#)pc_vfsops.c 1.55	97/11/07 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/cred.h>
#include <sys/disp.h>
#include <sys/buf.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/conf.h>
#undef NFSCLIENT
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/pathname.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/mkdev.h>
#include <sys/swap.h>
#include <sys/sunddi.h>
#include <sys/dktp/fdisk.h>
#include <sys/fs/pc_label.h>
#include <sys/fs/pc_fs.h>
#include <sys/fs/pc_dir.h>
#include <sys/fs/pc_node.h>
#include <fs/fs_subr.h>
#include <sys/modctl.h>
#include <sys/vol.h>
#include <sys/dkio.h>
#include <sys/open.h>

static int pcfs_psuedo_floppy(dev_t);

static int pcfsinit(struct vfssw *, int);
static int pcfs_mount(struct vfs *, struct vnode *, struct mounta *,
	struct cred *);
static int pcfs_unmount(struct vfs *, struct cred *);
static int pcfs_root(struct vfs *, struct vnode **);
static int pcfs_statvfs(struct vfs *, struct statvfs64 *);
static int pc_syncfsnodes(struct pcfs *);
static int pcfs_sync(struct vfs *, short, struct cred *);
static int pcfs_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp);

static int pc_getfattype(dev_t, int, daddr_t *, int *);
static int pc_readfat(struct pcfs *fsp, u_char *fatp, daddr_t start,
    size_t fatsize);
static int pc_writefat(struct pcfs *fsp, daddr_t start);

struct vfsops pcfs_vfsops = {
	pcfs_mount,
	pcfs_unmount,
	pcfs_root,
	pcfs_statvfs,
	pcfs_sync,
	pcfs_vget,
	fs_nosys,	/* vfs_mountroot */
	fs_nosys	/* vfs_swapvp */
};


int pcfsdebuglevel = 0;

/*
 * pcfslock:	protects the list of mounted pc filesystems "pc_mounttab.
 * pcfs_lock:	(inside per filesystem structure "pcfs")
 *		per filesystem lock. Most of the vfsops and vnodeops are
 *		protected by this lock.
 * pcnodes_lock: protects the pcnode hash table "pcdhead", "pcfhead".
 *
 * Lock hierarchy: pcfslock > pcfs_lock > pcnodes_lock
 */
kmutex_t	pcfslock;
krwlock_t pcnodes_lock; /* protect the pcnode hash table "pcdhead", "pcfhead" */

static int pcfstype;

static struct vfssw vfw = {
	"pcfs",
	pcfsinit,
	&pcfs_vfsops,
	0
};

extern struct mod_ops mod_fsops;

static struct modlfs modlfs = {
	&mod_fsops,
	"filesystem for PC",
	&vfw
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlfs,
	NULL
};

int
_init(void)
{
	int	error;

#if !defined(lint)
	/* make sure the on-disk structures are sane */
	ASSERT(sizeof (struct pcdir) == 32);
	ASSERT(sizeof (struct pcdir_lfn) == 32);
#endif
	mutex_init(&pcfslock, NULL, MUTEX_DEFAULT, NULL);
	rw_init(&pcnodes_lock, NULL, RW_DEFAULT, NULL);
	error = mod_install(&modlinkage);
	if (error) {
		mutex_destroy(&pcfslock);
		rw_destroy(&pcnodes_lock);
	}
	return (error);
}

int
_fini(void)
{
	int	error;

	error = mod_remove(&modlinkage);
	if (error)
		return (error);
	mutex_destroy(&pcfslock);
	rw_destroy(&pcnodes_lock);
	return (0);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}


static int
pcfsinit(struct vfssw *vswp, int fstype)
{
	vswp->vsw_vfsops = &pcfs_vfsops;
	pcfstype = fstype;
	(void) pc_init();
	return (0);
}

static struct pcfs *pc_mounttab = NULL;

extern struct pcfs_args pc_tz;

/*
 * pc_mount system call
 */
static int
pcfs_mount(
	struct vfs *vfsp,
	struct vnode *mvp,
	struct mounta *uap,
	struct cred *cr)
{
	struct pcfs *fsp;
	struct vnode *bvp;
	struct vnode *devvp;
	struct pathname special;
	daddr_t dosstart;
	dev_t pseudodev;
	dev_t xdev;
	char *spnp;
	char *data = uap->dataptr;
	int datalen = uap->datalen;
	int dos_ldrive = 0;
	int error;
	int fattype;
	int spnlen;
	int wantbootpart = 0;
	struct vioc_info info;
	int rval;		/* set but not used */
	int hidden = 0;
	int foldcase = 0;
	minor_t	minor;

	if (!suser(cr))
		return (EPERM);

	PC_DPRINTF0(4, "pcfs_mount\n");
	if (mvp->v_type != VDIR) {
		return (ENOTDIR);
	}
	mutex_enter(&mvp->v_lock);
	if ((uap->flags & MS_REMOUNT) == 0 &&
	    (uap->flags & MS_OVERLAY) == 0 &&
	    (mvp->v_count != 1 || (mvp->v_flag & VROOT))) {
		mutex_exit(&mvp->v_lock);
		return (EBUSY);
	}
	mutex_exit(&mvp->v_lock);

	if ((datalen != sizeof (struct pcfs_args)) &&
	    (datalen != sizeof (struct old_pcfs_args))) {
		return (ENOTSUP);
	} else {
		struct pcfs_args tmp_tz;

		tmp_tz.flags = 0;
		if (copyin(data, &tmp_tz, datalen)) {
			return (EFAULT);
		}
		if (datalen == sizeof (struct pcfs_args)) {
			hidden = tmp_tz.flags & PCFS_MNT_HIDDEN;
			foldcase = tmp_tz.flags & PCFS_MNT_FOLDCASE;
		}
		/*
		 * more than one pc filesystem can be mounted on x86
		 * so the pc_tz structure is now a critical region
		 */
		mutex_enter(&pcfslock);
		if (pc_mounttab == NULL)
			bcopy(&tmp_tz, &pc_tz, sizeof (struct pcfs_args));
		mutex_exit(&pcfslock);
	}
	/*
	 * Resolve path name of special file being mounted.
	 */
	if (error = pn_get(uap->spec, UIO_USERSPACE, &special)) {
		return (error);
	}
	if (error =
	    lookupname(special.pn_path, UIO_SYSSPACE, FOLLOW, NULLVPP, &bvp)) {
		/*
		 * look for suffix to special
		 * which indicates a request to mount the solaris boot
		 * partition, or a DOS logical drive on the hard disk
		 */
		spnlen = special.pn_pathlen;

		if (spnlen > 5) {
			spnp = special.pn_path + spnlen - 5;
			if (*spnp++ == ':' && *spnp++ == 'b' &&
			    *spnp++ == 'o' && *spnp++ == 'o' &&
			    *spnp++ == 't') {
				/*
				 * Looks as if they want to mount
				 * the Solaris boot partition
				 */
				wantbootpart = 1;
				dos_ldrive = 99;
				spnp = special.pn_path + spnlen - 5;
				*spnp = '\0';
				error = lookupname(special.pn_path,
				    UIO_SYSSPACE, FOLLOW, NULLVPP, &bvp);
			}
		}

		if (!wantbootpart) {
			spnp = special.pn_path + spnlen - 1;
			if (spnlen > 2 && *spnp >= 'c' && *spnp <= 'z') {
				spnlen--;
				dos_ldrive = *spnp-- - 'c' + 1;
			} else if (spnlen > 2 && *spnp >= '0' && *spnp <= '9') {
				spnlen--;
				dos_ldrive = *spnp-- - '0';
				if (spnlen > 2 && *spnp >= '0' &&
				    *spnp <= '9') {
					spnlen--;
					dos_ldrive += 10 * (*spnp-- - '0');
				}
			}
			if (spnlen > 1 && dos_ldrive && dos_ldrive <= 24 &&
			    *spnp == ':') {
				/*
				 * remove suffix so that we have a real
				 * device name
				 */
				*spnp = '\0';
				error = lookupname(special.pn_path,
				    UIO_SYSSPACE, FOLLOW, NULLVPP, &bvp);
			}
		}
		if (error) {
			pn_free(&special);
			return (error);
		}
	}
	pn_free(&special);
	if (bvp->v_type != VBLK) {
		VN_RELE(bvp);
		return (ENOTBLK);
	}
	xdev = bvp->v_rdev;
	VN_RELE(bvp);
	if (getmajor(xdev) >= devcnt) {
		return (ENXIO);
	}
	/*
	 * Ensure that this device (or logical drive) isn't already mounted,
	 * unless this is a REMOUNT request
	 */
	if (dos_ldrive) {
		mutex_enter(&pcfslock);
		for (fsp = pc_mounttab; fsp; fsp = fsp->pcfs_nxt)
			if (fsp->pcfs_xdev == xdev &&
			    fsp->pcfs_ldrv == dos_ldrive) {
				mutex_exit(&pcfslock);
				if (uap->flags & MS_REMOUNT) {
					return (0);
				} else {
					return (EBUSY);
				}
			}
		/*
		 * Assign a unique device number for the vfs
		 * The old way (getudev() + a constantly incrementing
		 * major number) was wrong because it changes vfs_dev
		 * across mounts and reboots, which breaks nfs file handles.
		 * UFS just uses the real dev_t. We can't do that because
		 * of the way pcfs opens fdisk partitons (the :c and :d
		 * partitions are on the same dev_t). Though that _might_
		 * actually be ok, since the file handle contains an
		 * absolute block number, it's probably better to make them
		 * different. So I think we should retain the original
		 * dev_t, but come up with a different minor number based
		 * on the logical drive that will _always_ come up the same.
		 * For now, we steal the upper 6 bits.
		 */
#ifdef notdef
		/* what should we do here? */
		if (((getminor(xdev) >> 12) & 0x3F) != 0)
			printf("whoops - upper bits used!\n");
#endif
		minor = ((dos_ldrive << 12) | getminor(xdev)) & MAXMIN32;
		pseudodev = makedevice(getmajor(xdev), minor);
		if (vfs_devsearch(pseudodev) != NULL) {
			mutex_exit(&pcfslock);
			if (uap->flags & MS_REMOUNT) {
				return (0);
			} else {
				return (EBUSY);
			}
		}
		mutex_exit(&pcfslock);
	} else {
		if (vfs_devsearch(xdev) != NULL)
			if (uap->flags & MS_REMOUNT) {
				return (0);
			} else {
				return (EBUSY);
			}
		pseudodev = xdev;
	}
	if (uap->flags & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;
	/*
	 * Mount the filesystem
	 */
	devvp = makespecvp(xdev, VBLK);
	if (IS_SWAPVP(devvp)) {
		VN_RELE(devvp);
		return (EBUSY);
	}

	/*
	 * special handling for PCMCIA memory card
	 * with psuedo floppies organization
	 */
	if (dos_ldrive == 0 && pcfs_psuedo_floppy(xdev)) {
		dosstart = (daddr_t)0;
		fattype = PCFS_PCMCIA_NO_CIS;
	} else {
		if (error = pc_getfattype(xdev, dos_ldrive, &dosstart,
		    &fattype)) {
			VN_RELE(devvp);
			return (error);
		}
	}

	(void) VOP_PUTPAGE(devvp, (offset_t)0, (u_int)0, B_INVAL, cr);
	fsp = kmem_zalloc((u_int)sizeof (struct pcfs), KM_SLEEP);
	fsp->pcfs_vfs = vfsp;
	fsp->pcfs_flags = fattype;
	fsp->pcfs_devvp = devvp;
	fsp->pcfs_xdev = xdev;
	fsp->pcfs_ldrv = dos_ldrive;
	fsp->pcfs_dosstart = dosstart;
	mutex_init(&fsp->pcfs_lock, NULL, MUTEX_DEFAULT, NULL);

	/* set the "nocheck" flag if volmgt is managing this volume */
	info.vii_pathlen = 0;
	info.vii_devpath = 0;
	error = cdev_ioctl(fsp->pcfs_xdev, VOLIOCINFO, (intptr_t)&info,
	    FKIOCTL|FREAD, kcred, &rval);
	if (error == 0) {
		fsp->pcfs_flags |= PCFS_NOCHK;
	}

	if (hidden)
		fsp->pcfs_flags |= PCFS_HIDDEN;
	if (foldcase)
		fsp->pcfs_flags |= PCFS_FOLDCASE;
	vfsp->vfs_dev = pseudodev;
	vfsp->vfs_fstype = pcfstype;
	vfs_make_fsid(&vfsp->vfs_fsid, pseudodev, pcfstype);
	vfsp->vfs_data = (caddr_t)fsp;
	vfsp->vfs_bcount = 0;

	error = pc_verify(fsp);
	if (error) {
		VN_RELE(devvp);
		mutex_destroy(&fsp->pcfs_lock);
		kmem_free(fsp, (u_int) sizeof (struct pcfs));
		return (error);
	}
	vfsp->vfs_bsize = fsp->pcfs_clsize;

	mutex_enter(&pcfslock);
	fsp->pcfs_nxt = pc_mounttab;
	pc_mounttab = fsp;
	mutex_exit(&pcfslock);
	return (0);
}

/*
 * vfs operations
 */

/* ARGSUSED */
static int
pcfs_unmount(
	struct vfs *vfsp,
	struct cred *cr)
{
	struct pcfs *fsp, *fsp1;

	PC_DPRINTF0(4, "pcfs_unmount\n");
	fsp = VFSTOPCFS(vfsp);
	/*
	 * We don't have to lock fsp because the VVFSLOCK in vfs layer will
	 * prevent lookuppn from crossing the mount point.
	 */
	if (fsp->pcfs_nrefs) {
		return (EBUSY);
	}

	/* now there should be no pcp node on pcfhead or pcdhead. */

	mutex_enter(&pcfslock);
	if (fsp == pc_mounttab) {
		pc_mounttab = fsp->pcfs_nxt;
	} else {
		for (fsp1 = pc_mounttab; fsp1 != NULL; fsp1 = fsp1->pcfs_nxt)
			if (fsp1->pcfs_nxt == fsp)
				fsp1->pcfs_nxt = fsp->pcfs_nxt;
	}
	mutex_exit(&pcfslock);

	if (fsp->pcfs_fatp != (u_char *)0) {
		pc_invalfat(fsp);
	}
	VN_RELE(fsp->pcfs_devvp);
	mutex_destroy(&fsp->pcfs_lock);
	kmem_free(fsp, (u_int) sizeof (struct pcfs));
	return (0);
}

/*
 * find root of pcfs
 */
static int
pcfs_root(
	struct vfs *vfsp,
	struct vnode **vpp)
{
	struct pcfs *fsp;
	struct pcnode *pcp;

	fsp = VFSTOPCFS(vfsp);
	(void) pc_lockfs(fsp);
	pcp = pc_getnode(fsp, (daddr_t)0, 0, (struct pcdir *)0);
	PC_DPRINTF2(9, "pcfs_root(0x%p) pcp= 0x%p\n",
	    (void *)vfsp, (void *)pcp);
	pc_unlockfs(fsp);
	*vpp = PCTOV(pcp);
	pcp->pc_flags |= PC_EXTERNAL;
	return (0);
}

/*
 * Get file system statistics.
 */
static int
pcfs_statvfs(
	struct vfs *vfsp,
	struct statvfs64 *sp)
{
	struct pcfs *fsp;
	int error;
	dev32_t d32;

	fsp = VFSTOPCFS(vfsp);
	error = pc_getfat(fsp);
	if (error)
		return (error);
	bzero(sp, sizeof (*sp));
	sp->f_bsize = sp->f_frsize = fsp->pcfs_clsize;
	sp->f_blocks = (fsblkcnt64_t)fsp->pcfs_ncluster;
	sp->f_bavail = sp->f_bfree = (fsblkcnt64_t)pc_freeclusters(fsp);
	sp->f_files = (fsfilcnt64_t)-1;
	sp->f_ffree = (fsfilcnt64_t)-1;
	sp->f_favail = (fsfilcnt64_t)-1;
#ifdef notdef
	(void) cmpldev(&d32, fsp->pcfs_devvp->v_rdev);
#endif /* notdef */
	(void) cmpldev(&d32, vfsp->vfs_dev);
	sp->f_fsid = d32;
	(void) strcpy(sp->f_basetype, vfssw[vfsp->vfs_fstype].vsw_name);
	sp->f_flag = vf_to_stf(vfsp->vfs_flag);
	sp->f_namemax = PCFNAMESIZE;
	return (0);
}

static int
pc_syncfsnodes(struct pcfs *fsp)
{
	struct pchead *hp;
	struct pcnode *pcp;
	int error;

	PC_DPRINTF0(7, "pcfs_syncfsnodes\n");
	(void) pc_lockfs(fsp);
	if (!(error = pc_syncfat(fsp))) {
		hp = pcfhead;
		while (hp < & pcfhead [ NPCHASH ]) {
			rw_enter(&pcnodes_lock, RW_READER);
			pcp = hp->pch_forw;
			while (pcp != (struct pcnode *)hp) {
				if (VFSTOPCFS(PCTOV(pcp) -> v_vfsp) == fsp)
					if (error = pc_nodesync(pcp))
						break;
				pcp = pcp -> pc_forw;
			}
			rw_exit(&pcnodes_lock);
			if (error)
				break;
			hp++;
		}
	}
	pc_unlockfs(fsp);
	return (error);
}

/*
 * Flush any pending I/O.
 */
/*ARGSUSED*/
static int
pcfs_sync(
	struct vfs *vfsp,
	short flag,
	struct cred *cr)
{
	struct pcfs *fsp;
	int error = 0;

	/* this prevents the filesystem from being umounted. */
	mutex_enter(&pcfslock);
	if (vfsp != NULL) {
		error = pc_syncfsnodes(VFSTOPCFS(vfsp));
	} else {
		fsp = pc_mounttab;
		while (fsp != NULL) {
			error = pc_syncfsnodes(fsp);
			if (error) break;
			fsp = fsp->pcfs_nxt;
		}
	}
	mutex_exit(&pcfslock);
	return (error);
}

int
pc_lockfs(struct pcfs *fsp)
{
	if ((fsp->pcfs_flags & PCFS_LOCKED) && (fsp->pcfs_owner == curthread)) {
		fsp->pcfs_count++;
	} else {
		mutex_enter(&fsp->pcfs_lock);
		if (fsp->pcfs_flags & PCFS_LOCKED)
			panic("pc_lockfs");
		(void) pc_getfat(fsp);
		fsp->pcfs_flags |= PCFS_LOCKED;
		fsp->pcfs_owner = curthread;
		fsp->pcfs_count++;
	}
	return (0);
}

void
pc_unlockfs(struct pcfs *fsp)
{

	if ((fsp->pcfs_flags & PCFS_LOCKED) == 0)
		panic("pc_unlockfs");
	if (--fsp->pcfs_count < 0)
		panic("pc_unlockfs: count");
	if (fsp->pcfs_count == 0) {
		fsp->pcfs_flags &= ~PCFS_LOCKED;
		fsp->pcfs_owner = 0;
		mutex_exit(&fsp->pcfs_lock);
	}
}

/*
 * Get the FAT type for the DOS medium.
 * DOS floppies always have 12-bit FATs.
 * DOS logical drives on the hard disk have to be located through the
 * FDISK partition table. This routine reads in block 0 of the device,
 * and scans the partition table for the Primary or Extended DOS partition.
 */
static int
pc_getfattype(
	dev_t dev,
	int ldrive,
	daddr_t *strtsectp,
	int *fattypep)
{
	struct vnode *devvp;
	struct ipart dosp[FD_NUMPART];	/* incore fdisk partition structure */
	struct mboot *dosp_ptr;		/* boot structure pointer */
	daddr_t diskblk;		/* Disk block to get */
	daddr_t baseblk = 0;		/* base of Extended DOS disk */
	daddr_t partbias;		/* base of Extended DOS partition */
	buf_t *bp = NULL;		/* Disk buffer pointer */
	int xnumsect;
	int i;				/* Loop counter	*/
	int rval = 0;
	u_char xsysid, sysid = 0;	/* System ID characters  */

	*strtsectp = (daddr_t)0;
	if (ldrive <= 0) {
		PC_DPRINTF0(4, "pc_getfattype: floppy has 12-bit FAT\n");
		*fattypep = 0;
		return (0);
	}
	/*
	 *  Device is not a floppy disk.  So assume a hard disk,
	 *  which must have a fdisk partition table in first sector.
	 *  The logical drive to be mounted can be the primary DOS partition
	 *  or in the Extended DOS partition.
	 */
	PC_DPRINTF2(5, "pc_getfattype: dev=%x  ldrive=%x  ", (int)dev, ldrive);
	devvp = makespecvp(dev, VBLK);
	if (rval = VOP_OPEN(&devvp, FREAD, CRED())) {
		PC_DPRINTF1(1, "pc_getfattype: open error=%d\n", rval);
		return (rval);
	}
	/*
	 *  Read block 0 from device to get the fdisk table
	 */
	bp = bread(dev, (daddr_t)0, PC_SECSIZE);
	if (bp->b_flags & B_ERROR) {
		PC_DPRINTF0(1, "pc_getfattype: read error\n");
		rval = EIO;
		goto out;
	}
	/*
	 *  Check for signature at end of boot block for good value.
	 *  If not then error with invalid request.
	 */
	dosp_ptr = (struct mboot *)bp->b_un.b_addr;
	if (ltohs(dosp_ptr->signature) != MBB_MAGIC) {
		cmn_err(CE_NOTE, "!pcfs: DOS signature error");
		rval = EINVAL;
		goto out;
	}
	/*
	 *  Copy from disk block into memory aligned structure for fdisk usage.
	 */
	bcopy(dosp_ptr->parts, dosp, sizeof (struct ipart)*FD_NUMPART);

	if (ldrive == 99) {
		/*
		 * This logical drive denotes a request for the Solaris
		 * boot partition.
		 */
		for (i = 0; i < FD_NUMPART; i++) {
			if (dosp[i].systid == X86BOOT)
				break;
		}
		if (i == FD_NUMPART) {
			cmn_err(CE_NOTE, "!pcfs: no boot partition");
			rval = EINVAL;
			goto out;
		}
		sysid = dosp[i].systid;
		*strtsectp = ltohi(dosp[i].relsect);
	} else if (ldrive == 1) {
		/*
		 * the first logical drive is C,
		 * which is the Primary DOS partition.
		 */
		for (i = 0; i < FD_NUMPART; i++) {
			if (dosp[i].systid == DOS_SYSFAT12 ||
			    dosp[i].systid == DOS_SYSFAT16 ||
			    dosp[i].systid == DOS_FAT32 ||
			    dosp[i].systid == DOS_FAT32_LBA ||
			    dosp[i].systid == DOS_FAT16P_LBA ||
			    dosp[i].systid == DIAGPART ||
			    dosp[i].systid == DOS_SYSHUGE)
				break;
		}
		if (i == FD_NUMPART) {
			cmn_err(CE_NOTE, "!pcfs: no primary partition");
			rval = EINVAL;
			goto out;
		}
		sysid = dosp[i].systid;
		*strtsectp = ltohi(dosp[i].relsect);
	} else {
		/*
		 * Logical drives D through Z (2 through 24) reside in the
		 * Extended DOS partition.
		 * First find the Extended DOS partition in the master fdisk
		 * partition table and prepare to walk the linked list of
		 * extended DOS partition tables.  Use the relsect value
		 * (which is the offset from the beginning of the disk) for
		 * the next extended fdisk table.  The first entry in the
		 * extended fdisk table is the local partition; the second
		 * entry maybe an entry for another extended DOS partition.
		 * If there is another extended DOS partition the address
		 * is determined from the relsect value plus the relsect
		 * value of the master extended DOS partition.
		 */
		for (i = 0; i < FD_NUMPART; i++) {
			if (dosp[i].systid == EXTDOS)
				break;
		}
		if (i == FD_NUMPART) {
			cmn_err(CE_NOTE, "!pcfs: no extended partition");
			rval = EINVAL;
			goto out;
		}
		diskblk = partbias = ltohi(dosp[i].relsect);
		xsysid = dosp[i].systid;
		xnumsect = ltohi(dosp[i].numsect);
		while (--ldrive && xsysid == EXTDOS) {
			brelse(bp);
			bp = bread(dev, diskblk, PC_SECSIZE);
			if (bp->b_flags & B_ERROR) {
				PC_DPRINTF0(1, "pc_getfattype: read error\n");
				rval = EIO;
				goto out;
			}
			dosp_ptr = (struct mboot *)bp->b_un.b_addr;
			if (ltohs(dosp_ptr->signature) != MBB_MAGIC) {
				cmn_err(CE_NOTE,
				    "!pcfs: extended partition signature err");
				rval = EINVAL;
				goto out;
			}
			bcopy(dosp_ptr->parts, dosp,
			    2 * sizeof (struct ipart));
			sysid = dosp[0].systid;
			xsysid = dosp[1].systid;
			baseblk = diskblk;
			diskblk = ltohi(dosp[1].relsect) + partbias;
		}
		*strtsectp = ltohi(dosp[0].relsect) + baseblk;
		if (xnumsect < (*strtsectp - partbias)) {
			cmn_err(CE_NOTE,
			    "!pcfs: extended partition values bad");
			rval = EINVAL;
			goto out;
		}
	}
	/*
	 * Check the sysid value of the logical drive.
	 * Return the correct value for the type of FAT found.
	 * Else return a value of -1 for unknown FAT type.
	 */
	if ((sysid == DOS_FAT32) || (sysid == DOS_FAT32_LBA)) {
		*fattypep = PCFS_FAT32 | PCFS_NOCHK;
		PC_DPRINTF0(4, "pc_getfattype: 32-bit FAT\n");
	} else if ((sysid == DOS_SYSFAT16) || (sysid == DOS_SYSHUGE) ||
	    (sysid == DIAGPART) ||
	    (sysid == DOS_FAT16P_LBA) || (sysid == DOS_FAT16_LBA)) {
		*fattypep = PCFS_FAT16 | PCFS_NOCHK;
		PC_DPRINTF0(4, "pc_getfattype: 16-bit FAT\n");
	} else if (sysid == DOS_SYSFAT12) {
		*fattypep = PCFS_NOCHK;
		PC_DPRINTF0(4, "pc_getfattype: 12-bit FAT\n");
	} else if (sysid == X86BOOT) {
		struct bootsec *bootp;
		uint overhead;
		uint numclusters;
		int secsize;

		brelse(bp);
		bp = bread(dev, *strtsectp, PC_SECSIZE);
		if (bp->b_flags & B_ERROR) {
			PC_DPRINTF0(1, "pc_getfattype: read error\n");
			rval = EIO;
			goto out;
		}
		bootp = (struct bootsec *)bp->b_un.b_addr;

		/* get the sector size - may be more than 512 bytes */
		secsize = (int)ltohs(bootp->bps[0]);
		/*
		 * Check for bogus sector size -
		 *	fat should be at least 1 sector
		 */
		if (secsize < 512 || (int)ltohs(bootp->fatsec) < 1 ||
		    bootp->nfat < 1 || bootp->spcl < 1) {
			cmn_err(CE_NOTE, "!pcfs: FAT size error");
			rval = EINVAL;
			goto out;
		}

		overhead = bootp->nfat * ltohs(bootp->fatsec);
		overhead += ltohs(bootp->res_sec[0]);
		overhead += (ltohs(bootp->rdirents[0]) *
		    sizeof (struct pcdir)) / secsize;

		numclusters = ((ltohs(bootp->numsect[0]) ?
		    ltohs(bootp->numsect[0]) : ltohi(bootp->totalsec)) -
		    overhead) / bootp->spcl;

		if (numclusters > DOS_F12MAXC) {
			PC_DPRINTF0(4, "pc_getfattype: 16-bit FAT BOOTPART\n");
			*fattypep = PCFS_FAT16 | PCFS_NOCHK | PCFS_BOOTPART;
		} else {
			PC_DPRINTF0(4, "pc_getfattype: 12-bit FAT BOOTPART\n");
			*fattypep = PCFS_NOCHK | PCFS_BOOTPART;
		}
	} else {
		cmn_err(CE_NOTE, "!pcfs: unknown FAT type");
		rval = EINVAL;
	}

/*
 *   Release the buffer used
 */
out:
	if (bp != NULL)
		brelse(bp);
	(void) VOP_CLOSE(devvp, FREAD, 1, (offset_t)0, CRED());
	return (rval);
}


/*
 * Get the boot parameter block and file allocation table.
 * If there is an old FAT, invalidate it.
 */
int
pc_getfat(struct pcfs *fsp)
{
	struct vfs *vfsp = PCFSTOVFS(fsp);
	struct buf *tp = 0;
	struct buf *bp = 0;
	u_char *fatp = NULL;
	u_char *fat_changemap = NULL;
	u_int *afatp;
	u_int *xfatp;
	struct bootsec *bootp;
	struct fat32_bootsec *f32b;
	struct vnode *devvp;
	int count;
	int error;
	int fatsize;
	int fat_changemapsize;
	int flags = 0;
	int nfat;
	int secsize;
	int secno;
	int fatsec;

	PC_DPRINTF0(5, "pc_getfat\n");
	devvp = fsp->pcfs_devvp;
	if (fsp->pcfs_fatp) {
		/*
		 * There is a FAT in core.
		 * If there are open file pcnodes or we have modified it or
		 * it hasn't timed out yet use the in core FAT.
		 * Otherwise invalidate it and get a new one
		 */
#ifdef notdef
		if (fsp->pcfs_frefs ||
		    (fsp->pcfs_flags & PCFS_FATMOD) ||
		    (hrestime.tv_sec < fsp->pcfs_fattime)) {
			return (0);
		} else {
			pc_invalfat(fsp);
		}
#endif /* notdef */
		return (0);
	}
	/*
	 * Open block device mounted on.
	 */
	error = VOP_OPEN(&devvp,
	    (vfsp->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE,
	    CRED());
	if (error) {
		PC_DPRINTF1(1, "pc_getfat: open error=%d\n", error);
		return (error);
	}
	/*
	 * Get boot parameter block and check it for validity
	 *
	 * For media with a 1k sector-size, fd_strategy() requires
	 * the I/O size to be a 1k multiple; since the sector-size
	 * is not yet known, always read 1k here.
	 */
	tp = bread(fsp->pcfs_xdev, fsp->pcfs_dosstart, PC_SECSIZE * 2);
	if (tp->b_flags & (B_ERROR | B_STALE)) {
		PC_DPRINTF0(1, "pc_getfat: boot block error\n");
		flags = tp->b_flags & B_ERROR;
		error = EIO;
		goto out;
	}
	tp->b_flags |= B_STALE | B_AGE;
	bootp = (struct bootsec *)tp->b_un.b_addr;

	/* get the sector size - may be more than 512 bytes */
	secsize = (int)ltohs(bootp->bps[0]);
	/* check for bogus sector size - fat should be at least 1 sector */
	if (IS_FAT32(fsp)) {
		f32b = (struct fat32_bootsec *)bootp;
		fatsec = ltohi(f32b->f_fatlength);
	} else {
		fatsec = ltohs(bootp->fatsec);
	}
	if (secsize < 512 || fatsec < 1 || bootp->nfat < 1) {
		cmn_err(CE_NOTE, "!pcfs: FAT size error");
		error = EINVAL;
		goto out;
	}

	switch (bootp->mediadesriptor) {
	default:
		cmn_err(CE_NOTE, "!pcfs: media-descriptor error, 0x%x",
		    bootp->mediadesriptor);
		error = EINVAL;
		goto out;

	case MD_FIXED:
		/*
		 * PCMCIA psuedo floppy is type MD_FIXED,
		 * but is accessed like a floppy
		 */
		if (!(fsp->pcfs_flags & PCFS_PCMCIA_NO_CIS)) {
			if (fsp->pcfs_ldrv <= 0) {
				/*
				 * do not mount explicit fdisk partition
				 */
				cmn_err(CE_NOTE,
				    "!pcfs: invalid logical drive");
				error = EINVAL;
				goto out;
			}
			/*
			 * need access to fdisk table to determine FAT type
			 */
			fsp->pcfs_flags |= PCFS_NOCHK;
		}
		/* FALLTHRU */
	case SS8SPT:
	case DS8SPT:
	case SS9SPT:
	case DS9SPT:
	case DS18SPT:
	case DS9_15SPT:
		/*
		 * all floppy media are assumed to have 12-bit FATs
		 * and a boot block at sector 0
		 */
		fsp->pcfs_secsize = secsize;
		fsp->pcfs_sdshift = secsize / DEV_BSIZE - 1;
		fsp->pcfs_entps = secsize / sizeof (struct pcdir);
		fsp->pcfs_spcl = (int)bootp->spcl;
		fsp->pcfs_fatsec = fatsec;
		fsp->pcfs_spt = (int)ltohs(bootp->spt);
		fsp->pcfs_rdirsec = (int)ltohs(bootp->rdirents[0])
		    * sizeof (struct pcdir) / secsize;
		fsp->pcfs_clsize = fsp->pcfs_spcl * secsize;
		fsp->pcfs_fatstart = fsp->pcfs_dosstart +
		    (daddr_t)ltohs(bootp->res_sec[0]);
		fsp->pcfs_rdirstart = fsp->pcfs_fatstart +
		    (bootp->nfat * fsp->pcfs_fatsec);
		fsp->pcfs_datastart = fsp->pcfs_rdirstart + fsp->pcfs_rdirsec;
		if (IS_FAT32(fsp))
			fsp->pcfs_rdirstart = ltohi(f32b->f_rootcluster);
		fsp->pcfs_ncluster = (((int)(ltohs(bootp->numsect[0]) ?
		    ltohs(bootp->numsect[0]) : ltohi(bootp->totalsec))) -
		    fsp->pcfs_datastart + fsp->pcfs_dosstart) / fsp->pcfs_spcl;
		fsp->pcfs_numfat = (int)bootp->nfat;
		fsp->pcfs_nxfrecls = PCF_FIRSTCLUSTER;
		break;
	}

	/*
	 * Get FAT and check it for validity
	 */
	fatsize = fsp->pcfs_fatsec * fsp->pcfs_secsize;
	fatp = kmem_alloc(fatsize, KM_SLEEP);
	error = pc_readfat(fsp, fatp, fsp->pcfs_fatstart, fatsize);
	if (error) {
		flags = B_ERROR;
		goto out;
	}
	fat_changemapsize = (fatsize / fsp->pcfs_clsize) + 1;
	fat_changemap = kmem_zalloc(fat_changemapsize, KM_SLEEP);

	if (fatp[0] != bootp->mediadesriptor ||
	    fatp[1] != 0xFF || fatp[2] != 0xFF) {
		cmn_err(CE_NOTE, "!pcfs: FAT signature error");
		error = EINVAL;
		goto out;
	}
	if (fatp[3] != 0xFF && (fsp->pcfs_flags & PCFS_FAT16))
		if (fsp->pcfs_fatsec <= 12) {
			/*
			 * We have a 12-bit FAT, rather than a 16-bit FAT.
			 * Ignore what the fdisk table says.
			 */
			PC_DPRINTF0(2, "pc_getfattype: forcing 12-bit FAT\n");
			fsp->pcfs_flags ^= PCFS_FAT16;
		} else {
			cmn_err(CE_NOTE, "!pcfs: FAT signature error");
			error = EINVAL;
			goto out;
		}

	/*
	 * Get alternate FATs and check for consistency
	 */
	for (nfat = 1; nfat < fsp->pcfs_numfat; nfat++) {
		u_char *tempfatp;

		tempfatp = kmem_alloc(fatsize, KM_SLEEP);
		secno = fsp->pcfs_fatstart + nfat*fsp->pcfs_fatsec;
		error = pc_readfat(fsp, tempfatp, secno, fatsize);
		if (error) {
			cmn_err(CE_NOTE, "!pcfs: alternate FAT error");
			flags = B_ERROR;
			kmem_free(tempfatp, fatsize);
			goto out;
		}
		for (xfatp = (u_int *)fatp, afatp = (u_int *)tempfatp,
		    count = fatsize / sizeof (u_int);
		    count--; /* null */) {
			if (*xfatp++ != *afatp++) {
				cmn_err(CE_NOTE,
				    "!pcfs: alternate FAT corrupted");
				break;
			}
		}
		kmem_free(tempfatp, fatsize);
	}
	fsp->pcfs_fatsize = fatsize;
	fsp->pcfs_fatp = fatp;
	fsp->pcfs_fat_changemapsize = fat_changemapsize;
	fsp->pcfs_fat_changemap = fat_changemap;
	fsp->pcfs_fattime = hrestime.tv_sec + PCFS_DISKTIMEOUT;

	brelse(tp);
	tp = NULL;
	if (IS_FAT32(fsp)) {
		/* get fsinfo */
		struct fat32_boot_fsinfo fsinfo_disk;

		fsp->f32fsinfo_sector = ltohs(f32b->f_infosector);
		tp = bread(fsp->pcfs_xdev,
		    fsp->pcfs_dosstart + pc_dbdaddr(fsp, fsp->f32fsinfo_sector),
		    PC_SECSIZE);
		if (tp->b_flags & (B_ERROR | B_STALE)) {
			cmn_err(CE_NOTE, "!pcfs: error reading fat32 fsinfo");
			flags = tp->b_flags & B_ERROR;
			brelse(tp);
			tp = NULL;
			error = EIO;
			goto out;
		}
		tp->b_flags |= B_STALE | B_AGE;
		bcopy((void *)(tp->b_un.b_addr + FAT32_BOOT_FSINFO_OFF),
		    &fsinfo_disk, sizeof (struct fat32_boot_fsinfo));
		brelse(tp);
		tp = NULL;

		/* translated fields */
		fsp->fsinfo_native.fs_signature =
		    ltohi(fsinfo_disk.fs_signature);
		fsp->fsinfo_native.fs_free_clusters =
		    ltohi(fsinfo_disk.fs_free_clusters);
		if (fsp->fsinfo_native.fs_signature != FAT32_FS_SIGN) {
			cmn_err(CE_NOTE,
			    "!pcfs: fat32 fsinfo signature mismatch.");
			error = EINVAL;
			goto out;
		}
	}

	return (0);

out:
	cmn_err(CE_NOTE, "!pcfs: illegal disk format");
	if (tp)
		brelse(tp);
	if (bp)
		brelse(bp);
	if (fatp)
		kmem_free(fatp, fatsize);
	if (fat_changemap)
		kmem_free(fat_changemap, fat_changemapsize);

	if (flags) {
		pc_gldiskchanged(fsp);
	}
	(void) VOP_CLOSE(devvp, (vfsp->vfs_flag & VFS_RDONLY) ?
	    FREAD : FREAD|FWRITE, 1, (offset_t)0, CRED());
	return (error);
}

int
pc_syncfat(struct pcfs *fsp)
{
	struct buf *bp;
	int nfat;
	int	error;
	struct fat32_boot_fsinfo fsinfo_disk;

	PC_DPRINTF0(7, "pcfs_syncfat\n");
	if ((fsp->pcfs_fatp == (u_char *)0) || !(fsp->pcfs_flags & PCFS_FATMOD))
		return (0);
	/*
	 * write out all copies of FATs
	 */
	fsp->pcfs_flags &= ~PCFS_FATMOD;
	fsp->pcfs_fattime = hrestime.tv_sec + PCFS_DISKTIMEOUT;
	for (nfat = 0; nfat < fsp->pcfs_numfat; nfat++) {
		error = pc_writefat(fsp,
		    fsp->pcfs_fatstart + nfat*fsp->pcfs_fatsec);
		if (error) {
			pc_gldiskchanged(fsp);
			return (EIO);
		}
	}
	pc_clear_fatchanges(fsp);
	PC_DPRINTF0(6, "pcfs_syncfat: wrote out FAT\n");
	/* write out fsinfo */
	if (IS_FAT32(fsp)) {
		bp = bread(fsp->pcfs_xdev,
		    fsp->pcfs_dosstart + pc_dbdaddr(fsp, fsp->f32fsinfo_sector),
		    PC_SECSIZE);
		if (bp->b_flags & (B_ERROR | B_STALE)) {
			brelse(bp);
			return (EIO);
		}
		bcopy((void *)(bp->b_un.b_addr + FAT32_BOOT_FSINFO_OFF),
		    &fsinfo_disk, sizeof (struct fat32_boot_fsinfo));
		/* translate fields */
		fsinfo_disk.fs_free_clusters =
		    htoli(fsp->fsinfo_native.fs_free_clusters);
		fsinfo_disk.fs_next_cluster = (uint32_t)FSINFO_UNKNOWN;
		bcopy(&fsinfo_disk,
		    (void *)(bp->b_un.b_addr + FAT32_BOOT_FSINFO_OFF),
		    sizeof (struct fat32_boot_fsinfo));
		bwrite2(bp);
		error = geterror(bp);
		brelse(bp);
		if (error) {
			pc_gldiskchanged(fsp);
			return (EIO);
		}
	}
	return (0);
}

void
pc_invalfat(struct pcfs *fsp)
{
	struct pcfs *xfsp;
	int mount_cnt = 0;

	PC_DPRINTF0(7, "pc_invalfat\n");
	if (fsp->pcfs_fatp == (u_char *)0)
		panic("pc_invalfat");
	/*
	 * Release FAT
	 */
	kmem_free(fsp->pcfs_fatp, fsp->pcfs_fatsize);
	fsp->pcfs_fatp = NULL;
	kmem_free(fsp->pcfs_fat_changemap, fsp->pcfs_fat_changemapsize);
	fsp->pcfs_fat_changemap = NULL;
	/*
	 * Invalidate all the blocks associated with the device.
	 * Not needed if stateless.
	 */
	mutex_enter(&pcfslock);
	for (xfsp = pc_mounttab; xfsp; xfsp = xfsp->pcfs_nxt)
		if (xfsp != fsp && xfsp->pcfs_xdev == fsp->pcfs_xdev)
			mount_cnt++;
	mutex_exit(&pcfslock);
	if (!mount_cnt)
		binval(fsp->pcfs_xdev);
	/*
	 * close mounted device
	 */
	(void) VOP_CLOSE(fsp->pcfs_devvp,
	    (PCFSTOVFS(fsp)->vfs_flag & VFS_RDONLY) ? FREAD : FREAD|FWRITE,
	    1, (offset_t)0, CRED());
}

void
pc_badfs(struct pcfs *fsp)
{
	cmn_err(CE_WARN, "corrupted PC file system on dev %x.%x\n",
	    getmajor(fsp->pcfs_devvp->v_rdev),
	    getminor(fsp->pcfs_devvp->v_rdev));
}

/*
 * The problem with supporting NFS on the PCFS filesystem is that there
 * is no good place to keep the generation number. The only possible
 * place is inside a directory entry. There are a few words that we
 * don't use - they store NT & OS/2 attributes, and the creation/last access
 * time of the file - but it seems wrong to use them. In addition, directory
 * entries come and go. If a directory is removed completely, its directory
 * blocks are freed and the generation numbers are lost. Whereas in ufs,
 * inode blocks are dedicated for inodes, so the generation numbers are
 * permanently kept on the disk.
 */
static int
pcfs_vget(struct vfs *vfsp, struct vnode **vpp, struct fid *fidp)
{
	struct pcnode *pcp;
	struct pc_fid *pcfid;
	struct pcfs *fsp;
	struct pcdir *ep;
	daddr_t eblkno;
	int eoffset;
	struct buf *bp;
	int error;
	pc_cluster32_t	cn;

	pcfid = (struct pc_fid *)fidp;
	fsp = VFSTOPCFS(vfsp);

	error = pc_lockfs(fsp);
	if (error) {
		*vpp = NULL;
		return (error);
	}

	if (pcfid->pcfid_block == 0) {
		pcp = pc_getnode(fsp, (daddr_t)0, 0, (struct pcdir *)0);
		pcp->pc_flags |= PC_EXTERNAL;
		*vpp = PCTOV(pcp);
		pc_unlockfs(fsp);
		return (0);
	}
	eblkno = pcfid->pcfid_block;
	eoffset = pcfid->pcfid_offset;
	if ((pc_dbtocl(fsp,
	    eblkno - fsp->pcfs_dosstart) >= fsp->pcfs_ncluster) ||
	    (eoffset > fsp->pcfs_clsize)) {
		pc_unlockfs(fsp);
		*vpp = NULL;
		return (EINVAL);
	}

	if (eblkno >= fsp->pcfs_datastart || (eblkno-fsp->pcfs_rdirstart)
	    < (fsp->pcfs_rdirsec & ~(fsp->pcfs_spcl - 1))) {
		bp = bread(fsp->pcfs_xdev, eblkno, fsp->pcfs_clsize);
	} else {
		bp = bread(fsp->pcfs_xdev, eblkno,
		    (int)(fsp->pcfs_datastart - eblkno) * fsp->pcfs_secsize);
	}
	if (bp->b_flags & (B_ERROR | B_STALE)) {
		error = geterror(bp);
		brelse(bp);
		if (error)
			pc_gldiskchanged(fsp);
		*vpp = NULL;
		pc_unlockfs(fsp);
		return (error);
	}
	ep = (struct pcdir *)(bp->b_un.b_addr + eoffset);
	/*
	 * Ok, if this is a valid file handle that we gave out,
	 * then simply ensuring that the creation time matches,
	 * the entry has not been deleted, and it has a valid first
	 * character should be enough.
	 *
	 * Unfortunately, verifying that the <blkno, offset> _still_
	 * refers to a directory entry is not easy, since we'd have
	 * to search _all_ directories starting from root to find it.
	 * That's a high price to pay just in case somebody is forging
	 * file handles. So instead we verify that as much of the
	 * entry is valid as we can:
	 *
	 * 1. The starting cluster is 0 (unallocated) or valid
	 * 2. It is not an LFN entry
	 * 3. It is not hidden (unless mounted as such)
	 * 4. It is not the label
	 */
	cn = pc_getstartcluster(fsp, ep);
	/*
	 * if the starting cluster is valid, but not valid according
	 * to pc_validcl(), force it to be to simplify the following if.
	 */
	if (cn == 0)
		cn = PCF_FIRSTCLUSTER;
	if (IS_FAT32(fsp)) {
		if (cn == PCF_LASTCLUSTER32)
			cn = PCF_FIRSTCLUSTER;
	} else {
		if (cn == PCF_LASTCLUSTER)
			cn = PCF_FIRSTCLUSTER;
	}
	if ((!pc_validcl(fsp, cn)) ||
	    (PCDL_IS_LFN(ep)) ||
	    (PCA_IS_HIDDEN(fsp, ep->pcd_attr)) ||
	    ((ep->pcd_attr & PCA_LABEL) == PCA_LABEL)) {
		bp->b_flags |= B_STALE | B_AGE;
		brelse(bp);
		pc_unlockfs(fsp);
		return (EINVAL);
	}
	if ((ep->pcd_crtime.pct_time == pcfid->pcfid_ctime) &&
	    (ep->pcd_filename[0] != PCD_ERASED) &&
	    (pc_validchar(ep->pcd_filename[0]) ||
		(ep->pcd_filename[0] == '.' && ep->pcd_filename[1] == '.'))) {
		pcp = pc_getnode(fsp, eblkno, eoffset, ep);
		pcp->pc_flags |= PC_EXTERNAL;
		*vpp = PCTOV(pcp);
	} else {
		*vpp = NULL;
	}
	bp->b_flags |= B_STALE | B_AGE;
	brelse(bp);
	pc_unlockfs(fsp);
	return (0);
}

/*
 * if device is a PCMCIA psuedo floppy, return 1
 * otherwise, return 0
 */
static int
pcfs_psuedo_floppy(dev_t rdev)
{
	int rval;	/* ignored */
	int error;
	int err;
	struct dk_cinfo info;

	if ((err = dev_open(&rdev, FREAD, OTYP_CHR, CRED())) != 0) {
		PC_DPRINTF1(1, "pcfs_psuedo_floppy: dev_open err=%d\n", err);
		return (0);
	}

	error = cdev_ioctl(rdev, DKIOCINFO, (intptr_t)&info, FKIOCTL,
	    CRED(), &rval);

	if ((err = dev_close(rdev, FREAD, OTYP_CHR, CRED())) != 0) {
		PC_DPRINTF1(1, "pcfs_psuedo_floppy: dev_close err=%d\n", err);
		return (0);
	}


	if ((error == 0) && (info.dki_ctype == DKC_PCMCIA_MEM) &&
		(info.dki_flags & DKI_PCMCIA_PFD))
		return (1);
	else
		return (0);
}

/*
 * Unfortunately, FAT32 fat's can be pretty big (On a 1 gig jaz drive, about
 * a meg), so we can't bread() it all in at once. This routine reads a
 * fat a chunk at a time.
 */
static int
pc_readfat(struct pcfs *fsp, u_char *fatp, daddr_t start, size_t fatsize)
{
	struct buf *bp;
	size_t off;
	size_t readsize;

	readsize = fsp->pcfs_clsize;
	for (off = 0; off < fatsize; off += readsize, fatp += readsize) {
		if (readsize > (fatsize - off))
			readsize = fatsize - off;
		bp = bread(fsp->pcfs_xdev,
		    pc_dbdaddr(fsp, start) +
			pc_cltodb(fsp, pc_lblkno(fsp, off)),
		    readsize);
		if (bp->b_flags & (B_ERROR | B_STALE)) {
			brelse(bp);
			return (EIO);
		}
		bp->b_flags |= B_STALE | B_AGE;
		bcopy(bp->b_un.b_addr, fatp, readsize);
		brelse(bp);
	}
	return (0);
}

/*
 * We write the FAT out a _lot_, in order to make sure that it
 * is up-to-date. But on a FAT32 system (large drive, small clusters)
 * the FAT might be a couple of megabytes, and writing it all out just
 * because we created or deleted a small file is painful (especially
 * since we do it for each alternate FAT too). So instead, for FAT16 and
 * FAT32 we only write out the bit that has changed. We don't clear
 * the 'updated' fields here because the caller might be writing out
 * several FATs, so the caller must use pc_clear_fatchanges() after
 * all FATs have been updated.
 */
static int
pc_writefat(struct pcfs *fsp, daddr_t start)
{
	struct buf *bp;
	size_t off;
	size_t writesize;
	int	error;
	u_char *fatp = fsp->pcfs_fatp;
	size_t fatsize = fsp->pcfs_fatsize;

	writesize = fsp->pcfs_clsize;
	for (off = 0; off < fatsize; off += writesize, fatp += writesize) {
		if (writesize > (fatsize - off))
			writesize = fatsize - off;
		if (!pc_fat_is_changed(fsp, pc_lblkno(fsp, off))) {
			continue;
		}
		bp = ngeteblk(writesize);
		bp->b_edev = fsp->pcfs_xdev;
		bp->b_dev = cmpdev(bp->b_edev);
		bp->b_blkno = pc_dbdaddr(fsp, start) +
		    pc_cltodb(fsp, pc_lblkno(fsp, off));
		bcopy(fatp, bp->b_un.b_addr, writesize);
		bwrite2(bp);
		error = geterror(bp);
		brelse(bp);
		if (error) {
			return (error);
		}
	}
	return (0);
}

/*
 * Mark the FAT cluster that 'cn' is stored in as modified.
 */
void
pc_mark_fat_updated(struct pcfs *fsp, pc_cluster32_t cn)
{
	pc_cluster32_t	bn;
	size_t		size;

	/* which fat block is the cluster number stored in? */
	if (IS_FAT32(fsp)) {
		size = sizeof (pc_cluster32_t);
		bn = pc_lblkno(fsp, cn * size);
		fsp->pcfs_fat_changemap[bn] = 1;
	} else if (IS_FAT16(fsp)) {
		size = sizeof (pc_cluster16_t);
		bn = pc_lblkno(fsp, cn * size);
		fsp->pcfs_fat_changemap[bn] = 1;
	} else {
		offset_t off;
		pc_cluster32_t nbn;

		ASSERT(IS_FAT12(fsp));
		off = cn + (cn >> 1);
		bn = pc_lblkno(fsp, off);
		fsp->pcfs_fat_changemap[bn] = 1;
		/* does this field wrap into the next fat cluster? */
		nbn = pc_lblkno(fsp, off + 1);
		if (nbn != bn) {
			fsp->pcfs_fat_changemap[nbn] = 1;
		}
	}
}

/*
 * return whether the FAT cluster 'bn' is updated and needs to
 * be written out.
 */
int
pc_fat_is_changed(struct pcfs *fsp, pc_cluster32_t bn)
{
	return (fsp->pcfs_fat_changemap[bn] == 1);
}
