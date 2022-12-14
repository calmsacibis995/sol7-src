/*
 * Copyright (c) 1992 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_misc.c	1.96	98/01/30 SMI"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/cpuvar.h>
#include <sys/processor.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/pathname.h>
#include <sys/callb.h>
#include <sys/fs/ufs_inode.h>
#include <vm/anon.h>
#include <sys/fs/swapnode.h>	/* for swapfs_minfree */
#include <sys/kmem.h>
#include <sys/vmmac.h>		/* for btokmx */
#include <sys/map.h>		/* for kernelmap */
#include <vm/seg_kmem.h>	/* for kvseg */
#include <sys/cpr.h>
#include <sys/obpdefs.h>
#include <sys/conf.h>

/*
 * CPR miscellaneous support routines
 */
#define	cpr_open(path, mode,  vpp)	(vn_open(path, UIO_SYSSPACE, \
		mode, 0600, vpp, CRCREAT, 0))
#define	cpr_rdwr(rw, vp, basep, cnt)	(vn_rdwr(rw, vp,  (caddr_t)(basep), \
		cnt, 0LL, UIO_SYSSPACE, 0, (rlim64_t)MAXOFF_T, CRED(), \
		(ssize_t *)NULL))

extern void clkset(time_t);
extern int cpr_count_kpages(char *, bitfunc_t);
extern int cpr_count_volatile_pages(char *, bitfunc_t);
extern int i_cpr_count_sensitive_kpages(char *, bitfunc_t);
extern i_cpr_find_bootcpu();
extern caddr_t i_cpr_map_setup(void);
extern void i_cpr_free_memory_resources(void);

extern kmutex_t cpr_slock;
extern int cpr_buf_size;
extern unsigned char *cpr_buf;
extern int cpr_pagedata_size;
extern unsigned char *cpr_pagedata;
extern int cpr_bufs_allocated;
extern int cpr_bitmaps_allocated;

static struct cprconfig cprconfig;
static int cprconfig_loaded = 0;
static int cpr_statefile_ok(vnode_t *);
static int growth_ratio(void);
static int get_init_kratio(int npages);
static int cpr_p_online(cpu_t *, int);
static void cpr_save_mp_state(void);
static char *cpr_cprconfig_to_path(struct cprconfig *cf);
static char *cpr_build_statefile_path(struct cprconfig *);
static int cpr_verify_statefile_path(struct cprconfig *);
static int cpr_is_ufs(struct vfs *);
static int cpr_read_cprconfig(struct cprconfig *);

#define	INTEGRAL	100	/* to get 1% precision */
#define	EXTRA_RATE	2	/* add EXTRA_RATE% extra space */

int
cpr_init(int fcn)
{
	/*
	 * Allow only one suspend/resume process.
	 */
	if (mutex_tryenter(&cpr_slock) == 0)
		return (EBUSY);

	CPR->c_flags = 0;
	CPR->c_substate = 0;
	CPR->c_cprboot_magic = 0;
	CPR->c_alloc_cnt = 0;

	CPR->c_fcn = fcn;
	if (fcn == AD_CPR_REUSABLE)
		CPR->c_flags |= C_REUSABLE;
	else
		CPR->c_flags |= C_SUSPENDING;
	if (fcn == AD_CPR_COMPRESS || fcn == AD_CPR_REUSABLE ||
	    fcn == AD_CPR_TESTZ || CPR->c_fcn == AD_CPR_FORCE)
		CPR->c_flags |= C_COMPRESSING;
	/*
	 * reserve CPR_MAXCONTIG virtual pages for cpr_dump()
	 * XXX this implied argument is a bug, it should be passed
	 * XXX explicitly or should be an output arg from i_cpr_map_setup
	 * XXX (in this case we reserve 256Kb, but the code only uses 64Kb).
	 */
	CPR->c_mapping_area = i_cpr_map_setup();
	if (CPR->c_mapping_area == 0) {		/* no space in kernelmap */
		cmn_err(CE_CONT, "cpr: Unable to alloc from kernelmap.\n");
		mutex_exit(&cpr_slock);
		return (EAGAIN);
	}
	DEBUG3(errp("Reserved virtual range from %x for writing kas\n",
		CPR->c_mapping_area));

	return (0);
}

/*
 * This routine releases any resources used during the checkpoint.
 */
void
cpr_done()
{
	cbd_t *dp;

	cpr_stat_cleanup();

	/*
	 * release memory used for bitmap
	 */
	dp = &CPR->c_bitmap_desc;
	if (dp->cbd_reg_bitmap)
		kmem_free((void *)dp->cbd_reg_bitmap, (size_t)dp->cbd_size);
	if (dp->cbd_vlt_bitmap)
		kmem_free((void *)dp->cbd_vlt_bitmap, (size_t)dp->cbd_size);
	bzero(dp, sizeof (*dp));
	cpr_bitmaps_allocated = 0;

	/*
	 * Free pages used by cpr buffers.
	 */
	if (cpr_bufs_allocated) {
		kmem_freepages(cpr_buf, cpr_buf_size);
		kmem_freepages(cpr_pagedata, cpr_pagedata_size);
		cpr_bufs_allocated = 0;
	}

	i_cpr_free_memory_resources();
	mutex_exit(&cpr_slock);
	cmn_err(CE_CONT, "\ncpr: System has been resumed.\n");
}

/*
 * Make sure that the statefile can be used as a block special statefile
 * (meaning that is exists and has nothing mounted on it)
 * Returns errno if not a valid statefile.
 */
int
cpr_check_spec_statefile(void)
{
	int err;

	if (!cprconfig_loaded) {
		if ((err = cpr_read_cprconfig(&cprconfig)) != 0)
			return (err);
		cprconfig_loaded = 1;
	}
	ASSERT(cprconfig.cf_type == CFT_SPEC);

	if (cprconfig.cf_devfs == NULL)
		return (ENXIO);

	return (cpr_verify_statefile_path(&cprconfig));

}

int
cpr_alloc_statefile()
{
	register int rc = 0;
	char *str;

	/*
	 * Statefile size validation. If checkpoint the first time, disk blocks
	 * allocation will be done; otherwise, just do file size check.
	 * if substate is C_ST_STATEF_ALLOC_RETRY, C_VP will be inited
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY) {
		str = "\n-->Retrying statefile allocation...";
		DEBUG1(errp(str));
		DEBUG7(errp(str));
		if (C_VP->v_type != VBLK)
			(void) VOP_DUMPCTL(C_VP, 1);
	} else {
		/*
		 * Open an exiting file for writing, the state file needs to be
		 * pre-allocated since we can't and don't want to do allocation
		 * during checkpoint (too much of the OS is disabled).
		 *    - do a preliminary size checking here, if it is too small,
		 *	allocate more space internally and retry.
		 *    - check the vp to make sure it's the right type.
		 */
		char *path = cpr_build_statefile_path(&cprconfig);

		if (path == NULL ||
		    cpr_verify_statefile_path(&cprconfig) != 0)
			return (ENXIO);

		if (rc = vn_open(path, UIO_SYSSPACE,
		    FCREAT|FWRITE, 0600, &C_VP, CRCREAT, 0)) {
			cmn_err(CE_WARN, "cpr: "
			    "Can't open statefile %s.", path);

			return (rc);
		}
	}

	/*
	 * Only ufs and block special statefiles supported
	 */
	if (C_VP->v_type != VREG && C_VP->v_type != VBLK)
		return (EACCES);

	if (rc = cpr_statefile_ok(C_VP)) {
		(void) VOP_CLOSE(C_VP, FWRITE, 1, (offset_t)0, CRED());
		(void) vn_remove(CPR_STATE_FILE, UIO_SYSSPACE, RMFILE);
		sync();
		return (rc);
	}

	if (C_VP->v_type != VBLK) {
		/*
		 * sync out the fs change due to the statefile reservation.
		 */
		(void) VFS_SYNC(C_VP->v_vfsp, 0, CRED());

		/*
		 * Validate disk blocks allocation for the state file.
		 * Ask the file system prepare itself for the dump operation.
		 */
		if (rc = VOP_DUMPCTL(C_VP, 0))
			return (rc);
	}
	return (0);
}

/*
 * Do a simple minded estimate of the space needed to hold the
 * state file, take compression into account (about 3:1) but
 * be fairly conservative here so we have a better chance of
 * succesful completion, because once we stop the cpr process
 * the cost to get back to userland is fairly high.
 *
 * Do disk blocks allocation for the state file if no space has
 * been allocated yet. Since the state file will not be removed,
 * allocation should only be done once.
 */
static int
cpr_statefile_ok(vnode_t *vp)
{
	struct inode *ip = VTOI(vp);
	struct fs *fs;
	char buf[1] = "1";
	const int HEADPAGES = 10; /* assume HEADPAGES are used for headers */
	const int UCOMP_RATE = 20; /* comp. ration*10 for user pages */
	int size, offset, bsize, error;
	size_t space;
	ssize_t resid;
	char *str;

	/*
	 * number of pages short for swapping.
	 */

	STAT->cs_nosw_pages = k_anoninfo.ani_mem_resv;
	if (STAT->cs_nosw_pages < 0)
		STAT->cs_nosw_pages = 0;

	DEBUG9(errp("Phys swap: max=%lu resv=%lu\n",
	    k_anoninfo.ani_max, k_anoninfo.ani_phys_resv));
	DEBUG9(errp("Mem swap: max=%ld resv=%lu\n",
	    MAX(availrmem - swapfs_minfree, 0),
	    k_anoninfo.ani_mem_resv));
	DEBUG9(errp("Total available swap: %ld\n",
		CURRENT_TOTAL_AVAILABLE_SWAP));

	/*
	 * Try different compression ratio to increase filesize.
	 */
	if (CPR->c_substate == C_ST_STATEF_ALLOC_RETRY) {
		/*
		 * character device doesn't get any bigger
		 */
		if (vp->v_type == VBLK) {
			str = "Retry statefile on special file\n";
			DEBUG1(errp(str));
			DEBUG9(errp(str));
			return (ENOMEM);
		}

		if ((CPR->c_flags & C_COMPRESSING) &&
		    CPR->c_alloc_cnt != C_MAX_ALLOC_RETRY)
			size = (size_t)((longlong_t)((longlong_t)
			    STAT->cs_est_statefsz *
			    growth_ratio()) / INTEGRAL);
		else
			size = STAT->cs_nocomp_statefsz;

		str = "Retry statefile size = %ld\n";
		DEBUG1(errp(str, size));
		DEBUG9(errp(str, size));
	} else {
		int npages, ndvram = 0;
		longlong_t ksize = 0;

		(void) callb_execute_class(CB_CL_CPR_FB, (int)&ndvram);
		str = "ndvram size = %d\n";
		DEBUG1(errp(str, ndvram));
		DEBUG9(errp(str, ndvram));

		npages = cpr_count_kpages(REGULAR_BITMAP, cpr_nobit);
		ksize = ndvram + (npages + HEADPAGES) * PAGESIZE +
			sizeof (cpd_t) * npages/CPR_MAXCONTIG;

		if (CPR->c_flags & C_COMPRESSING) {
			size = (ksize * 100 / get_init_kratio(npages)) +
			    STAT->cs_nosw_pages * PAGESIZE * 10 / UCOMP_RATE;
			DEBUG1(errp("cpr_statefile_ok: estimated size "
			    "with compression %d\n", (long)size));
		} else {
			size = ksize + STAT->cs_nosw_pages * PAGESIZE;
			DEBUG1(errp("cpr_statefile_ok: with no "
			    "compression size %d\n", (long)size));

		}

		str = "cpr_statefile_ok: ksize %ld, size %ld\n";
		DEBUG1(errp(str, (long)ksize, (long)size));
		DEBUG9(errp(str, (long)ksize, (long)size));
	}

	/*
	 * All this  is much simpler for the character special statefile
	 */
	if (vp->v_type == VBLK) {
#ifdef _LP64
		space = cdev_Size(vp->v_rdev);
		if (space == 0)
			space = cdev_size(vp->v_rdev);
		if (space == 0)
			space = bdev_Size(vp->v_rdev) * DEV_BSIZE;
		if (space == 0)
			space = bdev_size(vp->v_rdev) * DEV_BSIZE;
#else
		space = cdev_size(vp->v_rdev);
		if (space == 0) {	/* XXX this blongs in cdev_size XXX */
			space = bdev_size(vp->v_rdev) * DEV_BSIZE;
		}
		str = "statefile dev size %ld\n";
		DEBUG1(errp(str, (long)space));
		DEBUG9(errp(str, (long)space));

#endif /* _LP64 */

		/*
		 * Export the estimated filesize info, this value will be
		 * compared before dumping out the statefile in the case of
		 * no compression.
		 */
		STAT->cs_est_statefsz = size;

		str = "cpr_statefile_ok: Estimated statefile size %d, "
		    "space %ld\n";
		DEBUG1(errp(str, size, space));
		DEBUG9(errp(str, size, space));

		if (size > space)
			return (ENOMEM);
		return (0);

	} else {

		if (CPR->c_alloc_cnt++ > C_MAX_ALLOC_RETRY) {
			cmn_err(CE_CONT, "cpr: Statefile allocation retry "
			    "failed.\n");
			return (ENOMEM);
		}

		fs = ip->i_fs;

		/*
		 * Estimate space needed for the state file.
		 *
		 * State file size in bytes:
		 * 	kernel size + non-cache pte seg + Elf header size +
		 *	+ bitmap size + cpr state file headers size
		 * (round up to fs->fs_bsize)
		 */

		bsize = fs->fs_bsize;

		DEBUG9(errp("cpr_statefile_ok: before blkroundup size %d\n",
		    size));

		size = blkroundup(fs, size);

		/*
		 * Export the estimated filesize info, this value will be
		 * compared before dumping out the statefile in the case of
		 * no compression.
		 */
		STAT->cs_est_statefsz = size;

		str = "cpr_statefile_ok: Estimated statefile size %d, "
		    "i_size %lld\n";
		DEBUG1(errp(str, size, ip->i_size));
		DEBUG9(errp(str, size, ip->i_size));

		/*
		 * Check file size, if 0, allocate disk blocks for it;
		 * otherwise, just do validation.
		 */
		rw_enter(&ip->i_contents, RW_READER);
		if (ip->i_size == 0 || ip->i_size < size) {
			rw_exit(&ip->i_contents);

			/*
			 * Write 1 byte to each logincal block to reserve
			 * disk blocks space.
			 */
			for (offset = ip->i_size + (bsize - 1); offset <= size;
				offset += bsize) {
				static char *emsg_format =
					"Need %d more bytes disk space for "
					"statefile.\nCurrent statefile is %s.  "
					"See power.conf(4) for further "
					"instructions.\n";

				if (error = vn_rdwr(UIO_WRITE, vp,
				    (caddr_t)&buf, sizeof (buf),
				    (offset_t)offset, UIO_SYSSPACE,
				    0, (rlim64_t)MAXOFF_T, CRED(), &resid)) {
					if (error == ENOSPC)
						cmn_err(CE_WARN, emsg_format,
						size - offset + bsize - 1,
						cpr_cprconfig_to_path(
						    &cprconfig));
					return (error);
				}
			}
			return (0);
		}
		rw_exit(&ip->i_contents);
		return (0);
	}
}

void
cpr_statef_close()
{
	if (C_VP) {
		if (!cpr_reusable_mode)
			(void) VOP_DUMPCTL(C_VP, 1);
		(void) VOP_CLOSE(C_VP, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(C_VP);
		C_VP = 0;
	}
}

/*
 * Write the cprinfo structure to disk.  This contains the original
 * values of any prom properties that we are going to modify.  We fill
 * in the magic number of the file here as a signal to the booter code
 * that the state file is valid.  Be sure the file gets synced, since
 * we may be going to be shutting down the OS.
 */
int
cpr_validate_cprinfo(struct cprinfo *ci, int reusable)
{
	struct vnode *vp;
	int rc;

	ASSERT(!cpr_reusable_mode);

	if (strlen(CPR_STATE_FILE) >= MAXPATHLEN) {
		cmn_err(CE_CONT, "cpr: CPR path len %lu over limits.\n",
		    strlen(CPR_STATE_FILE));
		return (-1);
	}

	ci->ci_magic = CPR->c_cprboot_magic = CPR_DEFAULT_MAGIC;
	ci->ci_reusable = reusable;

	if ((rc = cpr_open(CPR_DEFAULT, FWRITE|FCREAT, &vp))) {
		cmn_err(CE_CONT, "cpr: Failed to open cprinfo file, "
		    "errno = %d.\n", rc);
		return (rc);
	}
	if ((rc = cpr_rdwr(UIO_WRITE,
	    vp, ci, sizeof (struct cprinfo))) != 0) {
		cmn_err(CE_CONT, "cpr: Failed writing cprinfo file, "
		    "errno = %d.\n", rc);
		(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (rc);
	}
	if ((rc = VOP_FSYNC(vp, FSYNC, CRED())) != 0)
		cmn_err(CE_WARN, "cpr: Cannot fsync generic, "
		    "errno = %d.", rc);
	(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());

	VN_RELE(vp);

	DEBUG2(errp("cpr_validate_cprinfo: magic=0x%x boot-file=%s "
	    "boot-device=%s auto-boot?=%s\n",
	    ci->ci_magic, ci->ci_bootfile,
	    ci->ci_bootdevice, ci->ci_autoboot));

	return (rc);
}

/*
 * Clear the magic number in the defaults file.  This tells the booter
 * program that the state file is not current and thus prevents
 * any attempt to restore from an obsolete state file.
 */
void
cpr_clear_cprinfo(struct cprinfo *ci)
{
	struct vnode *vp;
	int rc = 0;

	if (CPR->c_cprboot_magic == CPR_DEFAULT_MAGIC) {
		ci->ci_magic = 0;
		if (rc = cpr_open(CPR_DEFAULT, FWRITE|FCREAT, &vp)) {
			cmn_err(CE_CONT, "cpr: "
			    "Failed to open defaults file %s, errno = %d.\n",
			    CPR_DEFAULT, rc);

			return;
		}

		(void) cpr_rdwr(UIO_WRITE, vp, ci, sizeof (struct cprinfo));
		(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
		VN_RELE(vp);
	}
}

/*
 * If the cpr default file is invalid, then we must not be in reusable mode
 * if it is valid, it tells us our mode
 */
int
cpr_get_reusable_mode(void)
{
	struct vnode *vp;
	int rc = 0;
	struct cprinfo ci;

	if (rc = cpr_open(CPR_DEFAULT, FREAD, &vp)) {
		return (0);
	}

	rc = cpr_rdwr(UIO_READ, vp, &ci, sizeof (struct cprinfo));
	(void) VOP_CLOSE(vp, FWRITE, 1, (offset_t)0, CRED());
	VN_RELE(vp);
	if (!rc && ci.ci_magic == CPR_DEFAULT_MAGIC)
		return (ci.ci_reusable);

	return (0);
}

/*
 * clock/time related routines
 */
static time_t   cpr_time_stamp;


void
cpr_tod_get(cpr_time_t *ctp)
{
	timestruc_t ts;

	mutex_enter(&tod_lock);
	ts = tod_get();
	mutex_exit(&tod_lock);
	ctp->tv_sec = (time32_t)ts.tv_sec;
	ctp->tv_nsec = (int32_t)ts.tv_nsec;
}


void
cpr_save_time()
{
	cpr_time_stamp = hrestime.tv_sec;
}

/*
 * correct time based on saved time stamp or hardware clock
 */
void
cpr_restore_time()
{
	clkset(cpr_time_stamp);
}

/*
 * need to grow statefile by the following times.
 */
static int
growth_ratio()
{
	return (((longlong_t)STAT->cs_nocomp_statefsz * INTEGRAL) /
	    STAT->cs_dumped_statefsz + EXTRA_RATE);
}

struct comp_ratio {
	int spage;	/* low mem page # */
	int epage;	/* high mem page boundry */
	int ratio;	/* rate * 100 */
};

/*
 * tunable compression ratios for kernel pages
 * The # of kernel pages and their comp ratios fairly closely follow
 * the following 2nd derivative curve:
 *
 *	ratio = 2.1 + sqrt((npages - 2320)/3160)
 *
 * The table below is generated from the above formular which is modified
 * to produce conservative numbers.
 */
static struct comp_ratio init_kratio[] = {
	0,	2300,	210,		/* kernel pages for 16 MB */
	2300,	2500,	220,		/* kernel pages for 16 MB */
	2500,	2700,	230,		/* kernel pages for 16 MB */
	2700,	2900,	242,		/* kernel pages for 16/32 MB */
	2900,	3100,	248,		/* kernel pages for 16/32 MB */
	3100,	3300,	252,		/* kernel pages for 32 MB */
	3300,	3500,	258,		/* kernel pages for 32 MB */
	3500,	3700,	263,		/* kernel pages for 32/48 MB */
	3700,	3900,	268,		/* kernel pages for 32/48 MB */
	3900,	4100,	273,		/* kernel pages for 32/48/64 MB */
	4100,	4300,	280,		/* kernel pages for 32/48/64 MB */
	4300,	4600,	285,		/* kernel pages for 64 MB */
	4600,	15000,	290 		/* kernel pages for 64 MB */
};

static int
get_init_kratio(int npages)
{
	int i;

	for (i = 0; i < sizeof (init_kratio)/sizeof (struct comp_ratio); i++) {
		if (init_kratio[i].spage < npages &&
		    init_kratio[i].epage >= npages)
			return (init_kratio[i].ratio);
	}
	/*
	 * otherwise return no compression
	 */
	return (100);
}

/*
 * CPU ONLINE/OFFLINE CODE
 */
int
cpr_mp_offline()
{
	cpu_t *cp, *bootcpu;
	char *str;
	int rc = 0;

	/*
	 * Do nothing for UP.
	 */
	if (ncpus == 1)
		return (0);

	str = "on CPU %d (%p)\n";
	DEBUG1(errp(str, CPU->cpu_id, CPU));
	DEBUG9(errp(str, CPU->cpu_id, CPU));
	cpr_save_mp_state();

	bootcpu = cpu[i_cpr_find_bootcpu()];
	if (!CPU_ACTIVE(bootcpu))
		if ((rc = cpr_p_online(bootcpu, P_ONLINE)))
			return (rc);

	cp = cpu_list;
	do {
		if (cp == bootcpu)
			continue;
		if (cp->cpu_flags & CPU_OFFLINE)
			continue;
		if ((rc = cpr_p_online(cp, P_OFFLINE)))
			return (rc);
	} while ((cp = cp->cpu_next) != cpu_list);


	return (rc);
}

int
cpr_mp_online()
{
	cpu_t *cp, *bootcpu = CPU;
	char *str;
	int rc = 0;

	/*
	 * Do nothing for UP.
	 */
	if (ncpus == 1)
		return (0);

	str = "on CPU %d (%p)\n";
	DEBUG1(errp(str, CPU->cpu_id, CPU));
	DEBUG9(errp(str, CPU->cpu_id, CPU));

	/*
	 * cpr_save_mp_state() sets CPU_CPR_ONLINE in cpu_cpr_flags
	 * to indicate a cpu was online at the time of cpr_suspend();
	 * now restart those cpus that were marked as CPU_CPR_ONLINE
	 * and actually are offline.
	 */
	for (cp = bootcpu->cpu_next; cp != bootcpu; cp = cp->cpu_next) {
		if (CPU_CPR_IS_OFFLINE(cp))
			continue;
		if (CPU_ACTIVE(cp))
			continue;
		if ((rc = cpr_p_online(cp, P_ONLINE)))
			return (rc);
	}

	/*
	 * turn off the boot cpu if it was offlined
	 */
	if (CPU_CPR_IS_OFFLINE(bootcpu)) {
		if ((rc = cpr_p_online(bootcpu, P_OFFLINE)))
			return (rc);
	}
	return (0);
}

static void
cpr_save_mp_state()
{
	cpu_t *cp;

	cp = cpu_list;
	do {
		cp->cpu_cpr_flags &= ~CPU_CPR_ONLINE;
		if (CPU_ACTIVE(cp))
			CPU_SET_CPR_FLAGS(cp, CPU_CPR_ONLINE);
	} while ((cp = cp->cpu_next) != cpu_list);
}

/*
 * The followings are CPR MP related routines.
 *
 */

static int
cpr_p_online(cpu_t *cp, int state)
{
	char *str;
	int rc;

	str = "changing cpu %d to state %d\n";
	DEBUG1(errp(str, cp->cpu_id, state));
	DEBUG9(errp(str, cp->cpu_id, state));

	mutex_enter(&cpu_lock);
	switch (state) {
	case P_ONLINE:
		rc = cpu_online(cp);
		break;
	case P_OFFLINE:
		rc = cpu_offline(cp);
		break;
	}
	mutex_exit(&cpu_lock);
	if (rc) {
		cmn_err(CE_WARN, "cpr: Failed to change processor %d to "
		    "state %d, (errno %d).", cp->cpu_id, state, rc);
	}
	return (rc);
}

/*
 * Concatenates the file system and path name fields of the cprconfig structure
 */
static char *
cpr_cprconfig_to_path(struct cprconfig *cf)
{
	static char full_path[MAXNAMELEN];

	/*
	 * If the file system isn't "/", it adds a separator "/" between
	 * the file system and path name. If the first character of path
	 * name is "/", it ignores it.
	 */
	(void) sprintf(full_path, "%s%s%s", cf->cf_fs, strcmp(cf->cf_fs, "/") ?
	    "/" : "", cf->cf_path + (*cf->cf_path == '/' ? 1 : 0));
	return (full_path);
}

static int
cpr_read_cprconfig(struct cprconfig *cf)
{
	struct vnode *vp;
	int err;

	if ((err = vn_open(CPR_CONFIG, UIO_SYSSPACE, FREAD, 0, &vp, 0, 0))
									!= 0) {
		cmn_err(CE_CONT, "cpr: Unable to open "
		    "configuration file %s, errno = %d.\n", CPR_CONFIG, err);
		return (err);
	}

	if ((err = cpr_rdwr(UIO_READ, vp, cf, sizeof (*cf))) != 0) {
		cmn_err(CE_CONT, "cpr: Unable to read config file %s.  "
		    "Errno %d.\n", CPR_CONFIG, err);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (err);
	}

	if (cf->cf_magic != CPR_CONFIG_MAGIC) {
		cmn_err(CE_CONT, "cpr: invalid config file %s.\n"
		    "Rerun pmconfig(1M)\n", CPR_CONFIG);
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
		return (EINVAL);
	}

	(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
	VN_RELE(vp);
	return (err);
}
/*
 * Construct the pathname of the state file and return a pointer to
 * caller.  Read the config file to get the mount point of the
 * filesystem and the pathname within fs.
 */
static char *
cpr_build_statefile_path(struct cprconfig *cf)
{

	if (!cprconfig_loaded) {
		if (cpr_read_cprconfig(cf) != 0)
			return (NULL);
		cprconfig_loaded = 1;
	}

	switch (cf->cf_type) {
	case CFT_UFS:
		if (strlen(cf->cf_path) + strlen(cf->cf_fs) >= MAXNAMELEN - 1) {
			cmn_err(CE_CONT, "cpr: Statefile path too long.\n");
			return (NULL);
		}

		return (cpr_cprconfig_to_path(cf));
	case CFT_SPEC:
		return (cf->cf_devfs);
	default:
		cmn_err(CE_PANIC, "cpr: invalid statefile type");
		/*NOTREACHED*/
	}
}

int
cpr_statefile_is_spec(void)
{
	if (!cprconfig_loaded) {
		if (cpr_read_cprconfig(&cprconfig) != 0)
			return (0);
		cprconfig_loaded = 1;
	}
	return (cprconfig.cf_type == CFT_SPEC);
}

char *
cpr_get_statefile_prom_path(void)
{
	struct cprconfig *cf = &cprconfig;

	ASSERT(cprconfig_loaded);
	ASSERT(cf->cf_magic == CPR_CONFIG_MAGIC);
	ASSERT(cf->cf_type == CFT_SPEC);
	return (cf->cf_dev_prom);
}

/*
 * Verify that the information in the configuration file regarding the
 * location for the statefile is still valid, depending on cf_type.
 * for CFT_UFS, cf_fs must still be a mounted filesystem, it must be
 *	mounted on the same device as when pmconfig was last run,
 *	and the translation of that device to a node in the prom's
 *	device tree must be the same as when pmconfig was last run.
 * for CFT_SPEC, cf_path must be the path to a block special file,
 *	it must have no file system mounted on it,
 *	and the translation of that device to a node in the prom's
 *	device tree must be the same as when pmconfig was last run.
 */
static int
cpr_verify_statefile_path(struct cprconfig *cf)
{
	struct vfs *vfsp = NULL;
	struct ufsvfs *ufsvfsp = (struct ufsvfs *)rootvfs->vfs_data;
	struct ufsvfs *ufsvfsp_save = ufsvfsp;
	int error;
	struct vnode *vp;
	char *slash, *tail, *longest;
	union {
		char un_devpath[OBP_MAXPATHLEN];
		char un_sfpath[MAXNAMELEN];
	} un;
#define	devpath	un.un_devpath
#define	sfpath	un.un_sfpath

	ASSERT(cprconfig_loaded);
	/*
	 * We need not worry about locking or the timing of releasing
	 * the vnode, since we are single-threaded now.
	 */

	switch (cf->cf_type) {
	case CFT_SPEC:
		if (strlen(cf->cf_path) > sizeof (sfpath)) {
			cmn_err(CE_CONT, "cpr: Statefile pathname too long.\n");
			return (ENAMETOOLONG);
		}
		if ((error = lookupname(cf->cf_devfs,
		    UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) != 0) {
			cmn_err(CE_CONT, "cpr: Lookup failed for "
			    "cpr statefile device %s.\n", cf->cf_devfs);
			return (error);
		}
		if (vp->v_type != VBLK) {
			cmn_err(CE_CONT, "cpr: statefile device must be "
			    "block special file.\n");
			VN_RELE(vp);
			return (ENOTSUP);
		}
		if (vfs_devsearch(vp->v_rdev) != NULL) {
			cmn_err(CE_CONT, "cpr:  statefile device must not "
			    "have a file system mounted on it.\n");
			VN_RELE(vp);
			return (ENOTSUP);
		}
		if (IS_SWAPVP(vp)) {
			cmn_err(CE_CONT, "cpr:  statefile device must not "
			    "be configure as swap file.\n");
			VN_RELE(vp);
			return (ENOTSUP);
		}
		VN_RELE(vp);
		if ((error = i_devname_to_promname(cf->cf_devfs, devpath)) !=
		    0 || strcmp(devpath, cf->cf_dev_prom) != 0) {
			cmn_err(CE_CONT, "cpr: Device path for statefile "
			    "has changed from %s to %s.\tPlease rerun "
			    "pmconfig(1m).\n", cf->cf_dev_prom, devpath);
			return (error);
		}
		return (0);
	case CFT_UFS:
		break;		/* don't indent all the original code */
	default:
		cmn_err(CE_PANIC, "cpr: invalid cf_type");
	}

	/*
	 * The original code for UFS statefile
	 */
	if (strlen(cf->cf_fs) + strlen(cf->cf_path) + 2 > sizeof (sfpath)) {
		cmn_err(CE_CONT, "cpr: Statefile pathname too long.\n");
		return (ENAMETOOLONG);
	}

	bzero(sfpath, sizeof (sfpath));
	(void) strcpy(sfpath, cpr_cprconfig_to_path(cf));

	if (*sfpath != '/') {
		cmn_err(CE_CONT, "cpr: Statefile "
		    "pathname %s must begin with a /.\n", sfpath);
		return (EINVAL);
	}

	/*
	 * Find the longest prefix of the statefile pathname which
	 * is the mountpoint of a filesystem.  This string must
	 * match the cf_fs field we read from the config file.  Other-
	 * wise the user has changed things without running pmconfig.
	 */
	tail = longest = sfpath + 1;	/* pt beyond the leading "/" */
	while ((slash = strchr(tail, '/')) != NULL) {
		*slash = '\0';	  /* temporarily terminate the string */
		if ((error = lookupname(sfpath,
		    UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) != 0) {
			*slash = '/';
			cmn_err(CE_CONT, "cpr: A directory in the "
			    "statefile path %s was not found.\n", sfpath);
			VN_RELE(vp);

			return (error);
		}

		for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
			ufsvfsp = (struct ufsvfs *)vfsp->vfs_data;
			if (ufsvfsp != NULL && ufsvfsp->vfs_root == vp)
				break;
		}
		/*
		 * If we have found a filesystem mounted on the current
		 * path prefix, remember the end of the string in
		 * "longest".  If it happens to be the the exact fs
		 * saved in the configuration file, save the current
		 * ufsvfsp so we can make additional checks further down.
		 */
		if (vfsp != NULL) {
			longest = slash;
			if (strcmp(cf->cf_fs, sfpath) == 0)
				ufsvfsp_save = ufsvfsp;
		}

		VN_RELE(vp);
		*slash = '/';
		tail = slash + 1;
	}
	*longest = '\0';
	if (vfsp == NULL)
		vfsp = rootvfs;
	if (cpr_is_ufs(vfsp) == 0 || strcmp(cf->cf_fs, sfpath) != 0) {
		cmn_err(CE_CONT, "cpr: Filesystem containing "
		    "the statefile when pmconfig was run (%s) has "
		    "changed to %s.  Please rerun pmconfig(1m).\n",
		    cf->cf_fs, sfpath);
		return (EINVAL);
	}

	if ((error = lookupname(cf->cf_devfs,
	    UIO_SYSSPACE, FOLLOW, NULLVPP, &vp)) != 0) {
		cmn_err(CE_CONT, "cpr: Lookup failed for "
		    "cpr statefile device %s.\n", cf->cf_devfs);
		return (error);
	}

	if (ufsvfsp_save->vfs_devvp->v_rdev != vp->v_rdev) {
		cmn_err(CE_CONT, "cpr: Filesystem containing "
		    "statefile no longer mounted on device %s.  "
		    "See power.conf(4).\n", cf->cf_devfs);
		VN_RELE(vp);
		return (ENXIO);
	}
	VN_RELE(vp);

	if ((error = i_devname_to_promname(cf->cf_devfs,
	    devpath)) != 0 || strcmp(devpath, cf->cf_dev_prom) != 0) {
		cmn_err(CE_CONT, "cpr: Device path for statefile fs has "
		    "changed from %s to %s.\tPlease rerun pmconfig(1m).\n",
		    cf->cf_dev_prom, devpath);
		return (error);
	}

	return (0);
}


int
cpr_is_supported(void)
{
	return (cpr_is_ufs(rootvfs));
}

/*
 * XXX The following routines need to be in the vfs source code.
 */

static int
cpr_is_ufs(struct vfs *vfsp)
{
	char *fsname;

	fsname = vfssw[vfsp->vfs_fstype].vsw_name;
	return (strcmp(fsname, "ufs") == 0);
}

int
cpr_reusable_mount_check(void)
{
	struct vfs *vfsp;
	char *fsname;

	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_flag & VFS_RDONLY)
			continue;
		fsname = vfssw[vfsp->vfs_fstype].vsw_name;
		if (strcmp(fsname, "nfs") == 0)
			continue;
		if (strcmp(fsname, "tmpfs") == 0)
			continue;
		if (strcmp(fsname, "namefs") == 0)
			continue;
		if (strcmp(fsname, "proc") == 0)
			continue;
		if (strcmp(fsname, "lofs") == 0)
			continue;
		if (strcmp(fsname, "fd") == 0)
			continue;
		if (strcmp(fsname, "autofs") == 0)
			continue;
		cmn_err(CE_CONT, "cpr: a filesystem of type %s is mounted "
		    "read/write.\nReusable statefile requires no writeable "
		    "filesystem types except nfs or tmpfs.\n", fsname);
		return (EINVAL);
	}
	return (0);
}

/*
 * Force a fresh read of the cprinfo per uadmin 3 call
 */
void
cpr_forget_cprconfig(void)
{
	cprconfig_loaded = 0;
}
