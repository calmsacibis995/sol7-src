/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cpr_dump.c	1.62	98/01/13 SMI"

/*
 * Fill in and write out the cpr state file
 *	1. Allocate and write headers, ELF and cpr dump header
 *	2. Allocate bitmaps according to phys_install
 *	3. Tag kernel pages into corresponding bitmap
 *	4. Write bitmaps to state file
 *	5. Write actual physical page data to state file
 */

#include <sys/types.h>
#include <sys/param.h>

#include <sys/systm.h>

#include <sys/vm.h>
#include <sys/memlist.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/fs/ufs_inode.h>
#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <sys/cpr.h>
#include <sys/conf.h>

/* Local defines and variables */
#define	CPRBUFS		256			/* no. of cpr write buffer */
#define	CPRBUFSZ	(CPRBUFS * DEV_BSIZE)	/* cpr write buffer size */
#define	BTOb(bytes)	((bytes) << 3)		/* Bytes to bits, log2(NBBY) */
#define	bTOB(bits)	((bits) >> 3)		/* bits to Bytes, log2(NBBY) */

static u_int cpr_pages_tobe_dumped;
static u_int cpr_regular_pgs_dumped;

static int cpr_dump_regular_pages(vnode_t *);
static int cpr_count_upages(char *, bitfunc_t);
static int cpr_compress_and_write(vnode_t *, u_int, u_int, u_int);
static int cpr_flush_write(vnode_t *);

int cpr_contig_pages(vnode_t *, int);
int cpr_count_pages(caddr_t, size_t, char *, bitfunc_t);
int cpr_count_kpages(char *, bitfunc_t);
int cpr_count_volatile_pages(char *, bitfunc_t);

void cpr_clear_bitmaps(char *);

extern int i_cpr_save_sensitive_kpages();
extern int cpr_test_mode;

ctrm_t cpr_term;

u_char *cpr_buf;
int cpr_buf_size;

u_char *cpr_pagedata;
int cpr_pagedata_size;

int cpr_bufs_allocated;
int cpr_bitmaps_allocated;

static u_char *cpr_wptr;	/* keep track of where to write to next */
static int cpr_file_bn;		/* cpr state-file block offset */
static int cpr_disk_writes_ok;
static size_t cpr_dev_space = 0;

u_char cpr_pagecopy[CPR_MAXCONTIG * MMU_PAGESIZE];



/*
 * Allocate pages for buffers used in writing out the statefile
 */
static int
cpr_alloc_bufs()
{
	char *allocerr;

	allocerr = "cpr: Unable to allocate memory for cpr buffer";
	cpr_buf_size = mmu_btopr(CPRBUFSZ);
	cpr_buf = kmem_getpages(cpr_buf_size, KM_NOSLEEP);
	if (cpr_buf == NULL) {
		cmn_err(CE_WARN, allocerr);
		return (ENOMEM);
	}

	cpr_pagedata_size = (CPR_MAXCONTIG + 1);
	cpr_pagedata = kmem_getpages(cpr_pagedata_size, KM_NOSLEEP);
	if (cpr_pagedata == NULL) {
		kmem_freepages(cpr_buf, cpr_buf_size);
		cpr_buf = NULL;
		cmn_err(CE_WARN, allocerr);
		return (ENOMEM);
	}

	return (0);
}


/*
 * Allocate bitmaps according to phys_install memlist in the kernel.
 * Two types of bitmaps will be created, REGULAR_BITMAP and
 * VOLATILE_BITMAP. REGULAR_BITMAP contains kernel and some user pages
 * that need to be resumed. VOLATILE_BITMAP contains data used during
 * cpr_dump. Since the contents of volatile pages are no longer needed
 * after statefile is dumped to disk, they will not be saved. However
 * they need to be claimed during resume so that the resumed kernel
 * can free them. That's why we need two types of bitmaps. Remember
 * they can not be freed during suspend because they are used during
 * the process of dumping the statefile.
 */
static int
cpr_alloc_bitmaps()
{
	pgcnt_t map_bits;
	size_t map_bytes;
	cbd_t *dp;

	/*
	 * create bitmaps according to physmax
	 */
	map_bits = physmax + 1;
	map_bytes = bTOB(map_bits);

	DEBUG1(errp("cpr_alloc_bitmaps: bitmap bytes 0x%x, "
	    "phys page range (0x0 - 0x%lx)\n",
	    map_bytes, physmax));

	dp = &CPR->c_bitmap_desc;
	dp->cbd_reg_bitmap = (cpr_ptr)kmem_alloc(map_bytes, KM_NOSLEEP);
	dp->cbd_vlt_bitmap = (cpr_ptr)kmem_alloc(map_bytes, KM_NOSLEEP);
	if (dp->cbd_reg_bitmap == NULL || dp->cbd_vlt_bitmap == NULL)
		return (ENOMEM);

	dp->cbd_magic = CPR_BITMAP_MAGIC;
	dp->cbd_spfn = 0;
	dp->cbd_epfn = physmax;
	dp->cbd_size = map_bytes;

	return (0);
}


/*
 * CPR dump header contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. kernel return pc & ppn for resume
 *	3. current thread info
 *	4. debug level and test mode
 *	5. number of bitmaps allocated
 *	6. number of page records
 */
static int
cpr_write_header(vnode_t *vp)
{
	extern struct memlist *phys_install;
	extern u_short cpr_mach_type;
	struct memlist *pmem;
	u_int tot_physpgs, bitmap_pages;
	struct cpr_dump_desc cdump;
	int kpages, vpages, upages;

	cdump.cdd_magic = (u_int)CPR_DUMP_MAGIC;
	cdump.cdd_version = CPR_VERSION;
	cdump.cdd_machine = cpr_mach_type;
	cdump.cdd_debug = cpr_debug;
	cdump.cdd_test_mode = cpr_test_mode;
	cdump.cdd_bitmaprec = cpr_bitmaps_allocated;

	cpr_clear_bitmaps(REGULAR_BITMAP);
	cpr_clear_bitmaps(VOLATILE_BITMAP);

	/*
	 * Remember how many pages we plan to save to statefile.
	 * This information will be used for sanity checks.
	 * Untag those pages that will not be saved to statefile.
	 */
	kpages = cpr_count_kpages(REGULAR_BITMAP, cpr_setbit);
	vpages = cpr_count_volatile_pages(REGULAR_BITMAP, cpr_clrbit);
	upages = cpr_count_upages(REGULAR_BITMAP, cpr_setbit);
	cdump.cdd_dumppgsize = kpages - vpages + upages;
	cpr_pages_tobe_dumped = cdump.cdd_dumppgsize;
	DEBUG7(errp(
	    "\ncpr_write_header: kpages %d - vpages %d + upages %d = %d\n",
	    kpages, vpages, upages, cdump.cdd_dumppgsize));

	/*
	 * Some pages contain volatile data (cpr_buf and storage area for
	 * sensitive kpages), which are no longer needed after the statefile
	 * is dumped to disk.  We have already untagged them from regular
	 * bitmaps.  Now tag them into the volatile bitmaps.  The pages in
	 * volatile bitmaps will be claimed during resume, and the resumed
	 * kernel will free them.
	 */
	(void) cpr_count_volatile_pages(VOLATILE_BITMAP, cpr_setbit);

	/*
	 * Find out how many pages of bitmap are needed to represent
	 * the physical memory.
	 */
	tot_physpgs = 0;
	for (pmem = phys_install; pmem; pmem = pmem->next) {
		tot_physpgs += mmu_btop(pmem->size);
	}
	bitmap_pages = mmu_btop(PAGE_ROUNDUP(bTOB(tot_physpgs)));

	/*
	 * Export accurate statefile size for statefile allocation retry.
	 * statefile_size = all the headers + total pages +
	 * number of pages used by the bitmaps.
	 * Roundup will be done in the file allocation code.
	 */
	STAT->cs_nocomp_statefsz = sizeof (cdd_t) + sizeof (cmd_t) +
		(sizeof (cbd_t) * cdump.cdd_bitmaprec) +
		(sizeof (cpd_t) * cdump.cdd_dumppgsize) +
		mmu_ptob(cdump.cdd_dumppgsize + bitmap_pages);

	DEBUG1(errp("Accurate statefile size before compression: %d\n",
		STAT->cs_nocomp_statefsz));
	DEBUG9(errp("Accurate statefile size before compression: %d\n",
		STAT->cs_nocomp_statefsz));

	/*
	 * If the estimated statefile is not big enough,
	 * go retry now to save un-necessary operations.
	 */
	if (!(CPR->c_flags & C_COMPRESSING) &&
		(STAT->cs_nocomp_statefsz > STAT->cs_est_statefsz)) {
		DEBUG1(errp("cpr_write_header: STAT->cs_nocomp_statefsz > "
			"STAT->cs_est_statefsz\n"));
		DEBUG7(errp("cpr_write_header: STAT->cs_nocomp_statefsz > "
			"STAT->cs_est_statefsz\n"));
		return (ENOSPC);
	}

	/* now write cpr dump descriptor */
	return (cpr_write(vp, (caddr_t)&cdump, sizeof (cdd_t)));
}


/*
 * CPR dump tail record contains the following information:
 *	1. header magic -- unique to cpr state file
 *	2. all misc info that needs to be passed to cprboot or resumed kernel
 */
static int
cpr_write_terminator(vnode_t *vp)
{
	cpr_term.magic = (u_int)CPR_TERM_MAGIC;
	cpr_term.va = (cpr_ptr)&cpr_term;
	cpr_term.pfn = (cpr_ext)va_to_pfn((caddr_t)&cpr_term);

	/* count the last one (flush) */
	cpr_term.real_statef_size = STAT->cs_real_statefsz +
		btod(cpr_wptr - cpr_buf) * DEV_BSIZE;

	DEBUG9(errp("cpr_dump: Real Statefile Size: %d\n",
		STAT->cs_real_statefsz));

	cpr_tod_get(&cpr_term.tm_shutdown);

	return (cpr_write(vp, (caddr_t)&cpr_term, sizeof (cpr_term)));
}

/*
 * write bitmap desc, regular and volatile bitmap to the state file.
 */
static int
cpr_write_bitmap(vnode_t *vp)
{
	cbd_t *dp;
	int error;

	dp = &CPR->c_bitmap_desc;
	if (error = cpr_write(vp, (caddr_t)dp, sizeof (*dp)))
		return (error);
	if (error = cpr_write(vp, (caddr_t)dp->cbd_reg_bitmap, dp->cbd_size))
		return (error);
	if (error = cpr_write(vp, (caddr_t)dp->cbd_vlt_bitmap, dp->cbd_size))
		return (error);

	return (0);
}


static int
cpr_write_statefile(vnode_t *vp)
{
	u_int error = 0;
	extern	int	i_cpr_dump_sensitive_kpages();
	extern	int	i_cpr_check_pgs_dumped();
	extern	int	i_cpr_count_sensitive_kpages();
	void flush_windows();
	int spages;
	char *str;

	flush_windows();

	/*
	 * to get an accurate view of kas, we need to untag sensitive
	 * pages *before* dumping them because the disk driver makes
	 * allocations and changes kas along the way.  The remaining
	 * pages referenced in the bitmaps are dumped out later as
	 * regular kpages.
	 */
	str = "cpr_write_statefile:";
	spages = i_cpr_count_sensitive_kpages(REGULAR_BITMAP, cpr_clrbit);
	DEBUG7(errp("%s untag %d sens pages\n", str, spages));

	/*
	 * now it's OK to call a driver that makes allocations
	 */
	cpr_disk_writes_ok = 1;

	/*
	 * now write out the clean sensitive kpages
	 * according to the sensitive descriptors
	 */
	error = i_cpr_dump_sensitive_kpages(vp);
	if (error) {
		DEBUG7(errp("%s cpr_dump_sensitive_kpages() failed!\n", str));
		return (error);
	}

	/*
	 * cpr_dump_regular_pages() counts cpr_regular_pgs_dumped
	 */
	error = cpr_dump_regular_pages(vp);
	if (error) {
		DEBUG7(errp("%s cpr_dump_regular_pages() failed!\n", str));
		return (error);
	}

	/*
	 * sanity check to verify the right number of pages were dumped
	 */
	error = i_cpr_check_pgs_dumped(cpr_pages_tobe_dumped,
	    cpr_regular_pgs_dumped);

	if (error) {
		errp("\n%s page count mismatch!\n", str);
#ifdef DEBUG
		if (cpr_test_mode)
			debug_enter(NULL);
#endif
	}

	return (error);
}


/*
 * creates the CPR state file, the following sections are
 * written out in sequence:
 *    - writes the cpr dump header
 *    - writes the memory usage bitmaps
 *    - writes the platform dependent info
 *    - writes the remaining user pages
 *    - writes the kernel pages
 */
int
cpr_dump(vnode_t *vp)
{
	int error;

	if (cpr_bufs_allocated == 0) {
		if (error = cpr_alloc_bufs())
			return (error);
		cpr_bufs_allocated = 1;
	}
	/* point to top of internal buffer */
	cpr_wptr = cpr_buf;

	/* initialize global variables used by the write operation */
	cpr_file_bn = 0;
	cpr_dev_space = 0;

	/* allocate bitmaps */
	if (cpr_bitmaps_allocated == 0) {
		if (error = cpr_alloc_bitmaps()) {
			cmn_err(CE_WARN, "cpr: cant allocate bitmaps");
			return (error);
		}
		cpr_bitmaps_allocated = 1;
	}

	/*
	 * set internal cross checking; we dont want to call
	 * a disk driver that makes allocations until after
	 * sensitive pages are saved
	 */
	cpr_disk_writes_ok = 0;

	/*
	 * 1253112: heap corruption due to memory allocation when dumpping
	 *	    statefile.
	 * Theoretically on Sun4u only the kernel data nucleus, kvalloc and
	 * kvseg segments can be contaminated should memory allocations happen
	 * during sddump, which is not supposed to happen after the system
	 * is quiesced. Let's call the kernel pages that tend to be affected
	 * 'sensitive kpages' here. To avoid saving inconsistent pages, we
	 * will allocate some storage space to save the clean sensitive pages
	 * aside before statefile dumping takes place. Since there may not be
	 * much memory left at this stage, the sensitive pages will be
	 * compressed before they are saved into the storage area.
	 */
	if (error = i_cpr_save_sensitive_kpages()) {
		DEBUG7(errp("cpr_dump: save_sensitive_kpages failed!\n"));
		return (error);
	}

	/*
	 * since all cpr allocations are done (space for sensitive kpages,
	 * bitmaps, cpr_buf), kas is stable, and now we can accurately
	 * count regular and sensitive kpages.
	 */
	if (error = cpr_write_header(vp)) {
		DEBUG7(errp("cpr_dump: cpr_write_header() failed!\n"));
		return (error);
	}

	if (error = i_cpr_write_machdep(vp))
		return (error);

	if (error = cpr_write_bitmap(vp))
		return (error);

	if (error = cpr_write_statefile(vp)) {
		DEBUG7(errp("cpr_dump: cpr_write_statefile() failed!\n"));
		return (error);
	}

	if (error = cpr_write_terminator(vp))
		return (error);

	if (error = cpr_flush_write(vp))
		return (error);

	return (0);
}


/*
 * Go through kas to select valid pages and mark it in the appropriate bitmap.
 * This includes pages in segkmem, segkp, segkmap and kadb.
 */
int
cpr_count_kpages(char *bitmap, bitfunc_t bitfunc)
{
	struct seg *segp;
	u_int kas_cnt = 0;
	extern u_int i_cpr_count_special_kpages();

	/*
	 * Some pages need to be taken care of differently.
	 * eg: msgbuf pages of sun4m are not in kas but they need
	 * to be saved.  On sun4u, the physical pages of msgbuf are
	 * allocated via prom_retain().  msgbuf pages are not in kas.
	 */
	kas_cnt += i_cpr_count_special_kpages(bitmap, bitfunc);

	segp = AS_SEGP(&kas, kas.a_segs);
	do {
		kas_cnt += cpr_count_pages(segp->s_base, segp->s_size,
		    bitmap, bitfunc);
		segp = AS_SEGP(&kas, segp->s_next);
	} while (segp != NULL && segp != AS_SEGP(&kas, kas.a_segs));

	DEBUG9(errp("cpr_count_kpages: kas_cnt=%d\n", kas_cnt));
	DEBUG7(errp("\ncpr_count_kpages: %d pages, 0x%x bytes\n",
		kas_cnt, mmu_ptob(kas_cnt)));
	return (kas_cnt);
}


/*
 * Deposit the tagged page to the right bitmap.
 */
int
cpr_setbit(u_int ppn, char *bitmap)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	ASSERT(ppn >= dp->cbd_spfn && ppn <= dp->cbd_epfn);

	if (bitmap && isclr(bitmap, ppn)) {
		setbit(bitmap, ppn);
		return (0);
	}

	/* already mapped */
	return (1);
}

/*
 * Clear the bitmap corresponding to a page frame.
 */
int
cpr_clrbit(u_int ppn, char *bitmap)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	ASSERT(ppn >= dp->cbd_spfn && ppn <= dp->cbd_epfn);

	if (bitmap && isset(bitmap, ppn)) {
		clrbit(bitmap, ppn);
		return (0);
	}

	/* not mapped */
	return (1);
}


/* ARGSUSED */
int
cpr_nobit(u_int ppn, char *bitmap)
{
	return (0);
}


/*
 * Go thru all pages and pick up any page not caught during the invalidation
 * stage. This is also used to save pages with cow lock or phys page lock held
 * (none zero p_lckcnt or p_cowcnt)
 */
static	int
cpr_count_upages(char *bitmap, bitfunc_t bitfunc)
{
	page_t *pp, *page0;
	int dcnt = 0, tcnt = 0, pfn;

	page0 = pp = page_first();

	do {
#ifdef sparc
		extern struct vnode prom_ppages;
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    pp->p_vnode == &prom_ppages ||
			PP_ISFREE(pp) && PP_ISAGED(pp))
#else
		if (pp->p_vnode == NULL || pp->p_vnode == &kvp ||
		    PP_ISFREE(pp) && PP_ISAGED(pp))
#endif sparc
			continue;

		pfn = page_pptonum(pp);
		if (pf_is_memory(pfn)) {
			tcnt++;
			if ((*bitfunc)(pfn, bitmap) == 0)
				dcnt++; /* dirty count */
			else
				DEBUG2(errp("uptag: already "
				    "tagged pfn=0x%x\n", pfn));
		}
	} while ((pp = page_next(pp)) != page0);

	STAT->cs_upage2statef = dcnt;
	DEBUG9(errp("cpr_count_upages: dirty=%d total=%d\n",
		dcnt, tcnt));
	DEBUG7(errp("cpr_count_upages: %d pages, 0x%x bytes\n",
		dcnt, mmu_ptob(dcnt)));
	return (dcnt);
}


/*
 * 1. Prepare cpr page descriptor and write it to file
 * 2. Compress page data and write it out
 */
static int
cpr_compress_and_write(vnode_t *vp, u_int va, u_int pfn, u_int npg)
{
	int error = 0;
	int nbytes;
	uchar_t *datap;
	cpd_t cpd;	/* cpr page descriptor */
	void i_cpr_mapin(caddr_t, u_int, u_int);
	void i_cpr_mapout(caddr_t, u_int);
#ifdef	DEBUG
	extern u_int cpr_sum(u_char *, int);
#endif

	i_cpr_mapin(CPR->c_mapping_area, npg, pfn);

	DEBUG3(errp("cpr map in %d page from mapping_area %x to pfn %x\n",
		npg, CPR->c_mapping_area, pfn));

	/*
	 * Fill cpr page descriptor.
	 */
	cpd.cpd_magic = (u_int)CPR_PAGE_MAGIC;
	cpd.cpd_pfn = pfn;
	cpd.cpd_va = va;
	cpd.cpd_flag = 0;  /* must init to zero */
	cpd.cpd_page = npg;

	nbytes = mmu_ptob(npg);
	STAT->cs_dumped_statefsz += nbytes;

#ifdef	DEBUG
	/*
	 * Make a copy of the uncompressed data so we can checksum it.
	 * Compress that copy  so  the checksum works at the other end
	 */
	bcopy((caddr_t)CPR->c_mapping_area, (caddr_t)cpr_pagecopy,
	    mmu_ptob(npg));
	cpd.cpd_usum = cpr_sum(cpr_pagecopy, nbytes);
	cpd.cpd_flag |= CPD_USUM;

	datap = cpr_pagecopy;
#else
	datap = (uchar_t *)CPR->c_mapping_area;
#endif
	if (CPR->c_flags & C_COMPRESSING)
		cpd.cpd_length = cpr_compress(datap, nbytes, cpr_pagedata);
	else
		cpd.cpd_length = nbytes;

	if (cpd.cpd_length >= nbytes) {
		cpd.cpd_length = nbytes;
		cpd.cpd_flag &= ~CPD_COMPRESS;
	} else {
		cpd.cpd_flag |= CPD_COMPRESS;
		datap = cpr_pagedata;
#ifdef	DEBUG
		cpd.cpd_csum = cpr_sum(datap, cpd.cpd_length);
		cpd.cpd_flag |= CPD_CSUM;
#endif
	}

	/* Write cpr page descriptor */
	error = cpr_write(vp, (caddr_t)&cpd, sizeof (cpd_t));

	/* Write compressed page data */
	error = cpr_write(vp, (caddr_t)datap, cpd.cpd_length);

	/*
	 * Unmap the pages for tlb and vac flushing
	 */
	i_cpr_mapout(CPR->c_mapping_area, npg);

	if (error) {
		DEBUG1(errp("cpr_compress_and_write: vp %x va %x ", vp, va));
		DEBUG1(errp("pfn %x blk %d err %d\n", pfn, cpr_file_bn, error));
	} else {
		cpr_regular_pgs_dumped += npg;
	}

	return (error);
}


int
cpr_write(vnode_t *vp, caddr_t buffer, int size)
{
	int	error, count;
	caddr_t	fromp = buffer;
	extern int do_polled_io;

	/*
	 * This is a more expensive operation for the char special statefile,
	 * so we only compute it once; We can't precompute it in the ufs
	 * case because we grow the file and try again if it is too small
	 */
	if (cpr_dev_space == 0) {
		if (vp->v_type == VBLK) {
#ifdef _LP64
			/*
			 * for now we check the "Size" property
			 * only on 64-bit systems.
			 */
			cpr_dev_space = cdev_Size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = cdev_size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_Size(vp->v_rdev) *
				    DEV_BSIZE;
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_size(vp->v_rdev) *
				    DEV_BSIZE;
			ASSERT(cpr_dev_space);
#else
			cpr_dev_space = cdev_size(vp->v_rdev);
			if (cpr_dev_space == 0)
				cpr_dev_space = bdev_size(vp->v_rdev) *
				    DEV_BSIZE;
			ASSERT(cpr_dev_space);
#endif /* _LP64 */
		} else {
			cpr_dev_space = 1;	/* not used in this case */
		}
	}

	/*
	 * break the write into multiple part if request is large,
	 * calculate count up to buf page boundary, then write it out.
	 * repeat until done.
	 */

	while (size > 0) {
		count = MIN(size, cpr_buf + CPRBUFSZ - cpr_wptr);

		bcopy(fromp, (caddr_t)cpr_wptr, count);

		cpr_wptr += count;
		fromp += count;
		size -= count;

		if (cpr_wptr < cpr_buf + CPRBUFSZ)
			return (0);	/* buffer not full yet */
		ASSERT(cpr_wptr == cpr_buf + CPRBUFSZ);

		if (vp->v_type == VBLK) {
			if (dtob(cpr_file_bn + CPRBUFS) > cpr_dev_space)
				return (ENOSPC);
		} else {
			if (dbtob(cpr_file_bn+CPRBUFS) > VTOI(vp)->i_size)
				return (ENOSPC);
		}

		do_polled_io = 1;
		DEBUG3(errp("cpr_write: frmp=%x wptr=%x cnt=%x...",
			fromp, cpr_wptr, count));
		/*
		 * cross check, this should not happen!
		 */
		if (cpr_disk_writes_ok == 0) {
			errp("cpr_write: disk write too early!\n");
			return (EINVAL);
		}
		error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, CPRBUFS);
		DEBUG3(errp("done\n"));
		do_polled_io = 0;

		STAT->cs_real_statefsz += CPRBUFSZ;

		if (error) {
			cmn_err(CE_WARN, "cpr_write error %d", error);
			return (error);
		}
		cpr_file_bn += CPRBUFS;	/* Increment block count */
		cpr_wptr = cpr_buf;	/* back to top of buffer */
	}
	return (0);
}


int
cpr_flush_write(vnode_t *vp)
{
	int	nblk;
	int	error;
	extern int do_polled_io;

	/*
	 * Calculate remaining blocks in buffer, rounded up to nearest
	 * disk block
	 */
	nblk = btod(cpr_wptr - cpr_buf);

	do_polled_io = 1;
	error = VOP_DUMP(vp, (caddr_t)cpr_buf, cpr_file_bn, nblk);
	do_polled_io = 0;

	cpr_file_bn += nblk;
	if (error) {
		DEBUG2(errp("cpr_flush_write: error (%d)\n", error));
		return (error);
	}
	return (error);
}

void
cpr_clear_bitmaps(char *bitmap)
{
	cbd_t *dp;

	dp = &CPR->c_bitmap_desc;
	bzero(bitmap, (size_t)dp->cbd_size);
}

int
cpr_contig_pages(vnode_t *vp, int flag)
{
	int chunks = 0, error = 0;
	u_int i, j;
	cbd_t *dp;
	u_int	spin_cnt = 0, spfn, totbit;
	extern	int i_cpr_compress_and_save();

	dp = &CPR->c_bitmap_desc;
	spfn = dp->cbd_spfn;
	totbit = BTOb(dp->cbd_size);
	i = 0; /* Beginning of bitmap */
	j = 0;
	while (i < totbit) {
		while ((j < CPR_MAXCONTIG) && ((j + i) < totbit)) {
			if (isset((char *)dp->cbd_reg_bitmap, j+i))
				j++;
			else /* not contiguous anymore */
				break;
		}

		if (j) {
			chunks++;
			if (flag == SAVE_TO_STORAGE) {
				error = i_cpr_compress_and_save(
				    chunks, spfn + i, j);
				if (error)
					return (error);
			} else if (flag == WRITE_TO_STATEFILE) {
				error = cpr_compress_and_write(vp, 0,
				    spfn + i, j);
				if (error)
					return (error);
				else {
					spin_cnt++;
					if ((spin_cnt & 0x5F) == 1)
						cpr_spinning_bar();
				}
			}
		}

		i += j;
		if (j != CPR_MAXCONTIG) {
			/* Stopped on a non-tagged page */
			i++;
		}

		j = 0;
	}

	if (flag == STORAGE_DESC_ALLOC)
		return (chunks);
	else
		return (0);
}


int
cpr_count_pages(caddr_t sva, size_t size, char *bitmap, bitfunc_t bitfunc)
{
	caddr_t	va, eva;
	u_int	count = 0, pfn;

	eva = sva + PAGE_ROUNDUP(size);
	for (va = sva; va < eva; va += MMU_PAGESIZE) {
		pfn = va_to_pfn(va);
		if (pfn != (u_int)-1 && pf_is_memory(pfn)) {
			if ((*bitfunc)(pfn, bitmap) == 0)
				count++;
		}
	}
	return (count);
}


int
cpr_count_volatile_pages(char *bitmap, bitfunc_t bitfunc)
{
	int count = 0;
	extern int i_cpr_count_storage_pages();

	if (cpr_buf) {
		count += cpr_count_pages((caddr_t)cpr_buf,
		    mmu_ptob(cpr_buf_size), bitmap, bitfunc);
	}
	if (cpr_pagedata) {
		count += cpr_count_pages((caddr_t)cpr_pagedata,
		    mmu_ptob(cpr_pagedata_size), bitmap, bitfunc);
	}
	count += i_cpr_count_storage_pages(bitmap, bitfunc);

	DEBUG7(errp("cpr_count_vpages: %d pages, 0x%x bytes\n",
	    count, mmu_ptob(count)));
	return (count);
}


static int
cpr_dump_regular_pages(vnode_t *vp)
{
	int error = 0;

	cpr_regular_pgs_dumped = 0;
	error = cpr_contig_pages(vp, WRITE_TO_STATEFILE);
	if (!error)
		DEBUG7(errp("cpr_dump_regular_pages() done.\n"));
	return (error);
}
