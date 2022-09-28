/*
 * Copyright (c) 1994 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cprbooter.c	1.26	98/01/13 SMI"

/*
 * cprbooter - First pass of the resume bootstrapping process, which is
 * architected similar to ufsboot. It is responsible for:
 *	1. Get OBP bootargs info and reset boot environment (OBP boot-file
 *	   property in the /options node) to the original value before the
 *	   system was being suspened.
 * 	2. Open cpr statefile /.CPR
 *	3. Read in the memory usage information (the memory bitmap) from
 *	   the statefile
 *	4. According to the memory bitmap, allocate all the physical memory
 *	   that is needed by the resume kernel.
 *	5. Allocate all the virtual memory that is needed for the resume
 *	   bootstrapping operation.
 *	6. Close the statefile.
 *	7. Read in the cprboot program.
 *	8. Hands off the execution control to cprboot.
 *
 * Note: The boot-file OBP property has been assigned to the following value
 * by the suspend process:
 *			-F cprbooter cprboot
 * This is to indicate the system has been suspended and the resume
 * bootstrapping will be initiated the next time the system is being booted.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/cpr.h>
#include <sys/cpr_impl.h>
#include <sys/prom_plat.h>
#include <sys/debug/debug.h>
#include <sys/fs/ufs_fs.h>

#if	(CPR_PROM_RETAIN_CNT != 1)
	ASSERTION FAILURE, can't handle multiple prom retains
#endif

/* Adopted from ../stand/boot/sparc/common/bootops.c */
#define	skip_whitespc(cp) while (cp && (*cp == '\t' || *cp == '\n' || \
	*cp == '\r' || *cp == ' ')) cp++;

/*
 * The elf header of cprboot is validated by the bootblk.
 */

/*
 * External procedures declarations
 */
extern void	_start(void *, void *, void *, void *);
extern void	exitto(func_t, caddr_t);
extern void	cprboot_spinning_bar();
extern caddr_t	getloadbase();
extern int	(*readfile(int fd, int print))();
extern int	cpr_reset_properties();
extern int	cpr_locate_statefile(char *, char *);
extern int	cpr_statefile_open(char *, char *);
extern int	cpr_ufs_open(char *, char *);
extern int	cpr_read(int, caddr_t, size_t, u_int *);
extern int	cpr_statefile_close(int);
extern int	cpr_ufs_close(int);

/*
 * Static local procedures declarations
 */
static caddr_t	cpr_alloc(size_t);
static int	cpr_read_bitmap(int fd);
static int	cpr_claim_mem();
static int	check_bootargs(char **, int *);
static int	getchunk(pfn_t *lopfnp, pfn_t *hipfnp);
static int	claim_chunk(pfn_t lo_pfn, pfn_t hi_pfn);

static char	*cpr_bootsec = "cprboot";	/* Secondary cprboot program */

char cpr_statefile[OBP_MAXPATHLEN];
char cpr_filesystem[OBP_MAXPATHLEN];
static caddr_t machdep_buf;
static csu_md_t m_info;

int	claim_phys_page(u_int);

static int totbitmaps, totpages;
static int totpg = 0;
int cpr_debug;
static cbd_t bitmap_desc;
static char cpr_heap[MMU_PAGESIZE * 5];	/* bitmaps for 2.5G minus overhead */

extern char _end[];			/* cprboot end addr */
static pfn_t s_pfn, e_pfn;		/* start, end pfn of cprbooter */

#define	max(a, b)	((a) > (b) ? (a) : (b))
#define	min(a, b)	((a) < (b) ? (a) : (b))
#define	roundup(x, y)   ((((x)+((y)-1))/(y))*(y))


void
clear_console()
{
	/*
	 * Clear console
	 */
	prom_printf("\033[p");
	prom_printf("\014");
	prom_printf("\033[1P");
	prom_printf("\033[18;21H");
}


/*
 * cookie - ieee1275 cif handle passed from srt0.s.
 *
 */
int
main(void *cookie)
{
	int	fd;
	ssize_t	machdep_len;
	char	*specialstate;
	int	reusable;
	cdd_t	cdump;
	char	*str;
	func_t	jmp2;			/* Start of cprboot */
	caddr_t	lb;

	prom_init("cprbooter", cookie);
	DEBUG4(errp("\nboot: boot /platform/sun4u/cprbooter\n"));

	clear_console();
	prom_printf("Restoring the System. Please Wait... ");

	/*
	 * parse boot args looking for block special statefile name
	 * and/or reusable flag
	 */
	if (check_bootargs(&specialstate, &reusable))
		return (-1);

	str = "cprbooter:";
	if (!reusable) {
		/*
		 * Restore the original values of the nvram properties modified
		 * during suspend.  Note: if we can't get this info from the
		 * defaults file, the state file may be obsolete or bad, so we
		 * abort.  However, failure to restore one or more properties
		 * is NOT fatal (better to continue the resume).
		 */
		if (cpr_reset_properties() == -1) {
			errp("%s Cannot read saved nvram info, "
			    "please reboot.\n", str);
			return (-1);
		}
	}
	if (!specialstate) {
		/*
		 * Pass the pathname of the statefile and its filesystem
		 * to cprboot, so that cprboot doesn't need to get them again.
		 */
		if (cpr_locate_statefile(cpr_statefile, cpr_filesystem) == -1) {
			errp("%s Cannot find statefile; "
			    "please do a normal boot.\n", str);
			return (-1);
		}
	} else {
		cpr_filesystem[0] = '\0';
		(void) strcpy(cpr_statefile, specialstate);
	}

	if ((fd = cpr_statefile_open(cpr_statefile, cpr_filesystem)) == -1) {
		if (!specialstate)
			errp("Can't open %s on %s, please reboot\n",
			    cpr_statefile, cpr_filesystem);
		else
			errp("Can't open %s, please reboot\n",
			    cpr_statefile);
		return (-1);
	}

	/*
	 * Devices get left at block 1
	 */
	if (specialstate)
		(void) prom_seek(fd, 0LL);

	if (cpr_read_cdump(fd, &cdump, CPR_MACHTYPE_4U)) {
		(void) cpr_statefile_close(fd);
		errp("can't read statefile dump header, please reboot\n");
		return (-1);
	}
	totbitmaps = cdump.cdd_bitmaprec;
	totpages = cdump.cdd_dumppgsize;
	DEBUG4(errp("%s	totbitmaps %d totpages %d\n",
	    str, totbitmaps, totpages));

	if ((machdep_len = cpr_get_machdep_len(fd)) == -1)
		return (-1);

	/*
	 * Note: Memory obtained from cpr_alloc() comes from the heap
	 * and doesn't need to be freed, since we are going to exit
	 * to the kernel in any case.
	 */
	if ((machdep_buf = cpr_alloc((size_t)machdep_len)) == 0) {
		errp("Can't alloc machdep buffer\n");
		return (-1);
	}

	if (cpr_read_machdep(fd, machdep_buf, machdep_len) == -1)
		return (-1);
	bcopy(machdep_buf, &m_info, sizeof (m_info));

#ifdef CPRBOOT_DEBUG
	for (i = 0; i < m_info.dtte_cnt; i++) {
		errp("dtte: no %d va_tag %lx ctx %x tte %lx\n",
		m_info.dtte[i].no, m_info.dtte[i].va_tag,
		m_info.dtte[i].ctx, m_info.dtte[i].tte);
	}

	for (i = 0; i < m_info.itte_cnt; i++) {
		errp("itte: no %d va_tag %lx ctx %x tte %lx\n",
		m_info.itte[i].no, m_info.itte[i].va_tag,
		m_info.itte[i].ctx, m_info.itte[i].tte);
	}
#endif CPRBOOT_DEBUG

	/*
	 * Read in the statefile bitmap
	 */
	if (cpr_read_bitmap(fd)) {
		(void) cpr_statefile_close(fd);
		errp("Can't read statefile bitmap, please reboot\n");
		return (-1);
	}

	/*
	 * Close the statefile and the disk device
	 * So that prom will free up some more memory.
	 */
	(void) cpr_statefile_close(fd);

	/*
	 * Claim physical memory from OBP for the resume kernel
	 */
	if (cpr_claim_mem()) {
		errp("Can't claim phys memory for resume, please reboot\n");
		return (-1);
	}

	if ((fd = cpr_ufs_open(CPRBOOT_PATH, prom_bootpath())) == -1) {
		errp("Can't open %s, please reboot\n", CPRBOOT_PATH);
		return (-1);
	}
	DEBUG4(errp("%s opened cprboot\n", str));

	if ((jmp2 = readfile(fd, 0)) != (func_t)-1) {
		(void) cpr_ufs_close(fd);
	} else {
		(void) cpr_ufs_close(fd);
		errp("Failed to read cprboot, please reboot\n");
		return (-1);
	}

	lb = getloadbase();
	DEBUG1(errp("%s exitto(0x%p, 0x%p)\n", str, jmp2, lb));
	(void) exitto(jmp2, lb);
	/*NOTREACHED*/
}

int
cpr_read_bitmap(int fd)
{
	char *reg_bitmap, *vlt_bitmap, *str;
	cbd_t *dp;

	str = "cpr_read_bitmap:";

	dp = &bitmap_desc;
	if (cpr_read(fd, (caddr_t)dp, sizeof (*dp), 0) < sizeof (*dp)) {
		errp("%s error reading bitmap desc\n", str);
		return (-1);
	}

	if (dp->cbd_magic != CPR_BITMAP_MAGIC) {
		errp("%s BAD MAGIC %x\n", str, dp->cbd_magic);
		return (-1);
	}

	reg_bitmap = cpr_alloc((size_t)dp->cbd_size);
	vlt_bitmap = cpr_alloc((size_t)dp->cbd_size);
	if (reg_bitmap == NULL || vlt_bitmap == NULL) {
		errp("%s cant cpr_alloc bitmaps\n", str);
		return (-1);
	}

	dp->cbd_reg_bitmap = (cpr_ptr)reg_bitmap;
	dp->cbd_vlt_bitmap = (cpr_ptr)vlt_bitmap;
	if (cpr_read(fd, reg_bitmap, dp->cbd_size, 0) < dp->cbd_size ||
	    cpr_read(fd, vlt_bitmap, dp->cbd_size, 0) < dp->cbd_size) {
		errp("%s error reading bitmaps\n", str);
		return (-1);
	}

	DEBUG4(errp("%s reg_bitmap 0x%p, vlt_bitmap 0x%p, "
	    "size %d, bits %d\n", str, reg_bitmap, vlt_bitmap,
	    dp->cbd_size, dp->cbd_size * NBBY));

	return (0);
}

/*
 * According to the bitmaps, claim as much physical memory as we can
 * for the resume kernel.
 *
 * If the un-claimable memory are used by cprbooter or by the previous
 * prom_alloc's (memory that are used by the bitmap_desc[]), we are safe;
 * Otherwise, abort.
 */
static int
cpr_claim_mem()
{
	u_longlong_t s_addr, e_addr;
	pfn_t lo_pfn, hi_pfn;		/* lo, high pfn of bitmap chunk */
	pfn_t lo_extent, hi_extent;	/* enclosure of cprbooter */
	int valid, mode, handled;

	/*
	 * Find the physical addresses of cprbooter _start and _end,
	 */
	(void) prom_translate_virt((caddr_t)_start, &valid, &s_addr, &mode);
	if (valid != -1) {
		errp("cpr_claim_mem: Can't xlate _start %x\n", (caddr_t)_start);
		return (-1);
	}
	if (s_addr >> 32)
		DEBUG4(errp("cpr_claim_mem: xlated _start s_addr %x%8x\n",
		(int)(s_addr >> 32), (int)(s_addr)))
	else
		DEBUG4(errp("cpr_claim_mem: xlated _start s_addr %x\n",
		(int)s_addr));

	(void) prom_translate_virt(_end, &valid, &e_addr, &mode);

	if (valid != -1) {
		errp("cpr_claim_mem: Can't xlate _end %x\n", _end);
		return (-1);
	}
	if (e_addr >> 32)
		DEBUG4(errp("cpr_claim_mem: xlated _end e_addr %x%8x\n",
		(int)(e_addr >> 32), (int)(e_addr)))
	else
		DEBUG4(errp("cpr_claim_mem: xlated _end e_addr %x\n",
		(int)e_addr));

	/*
	 * cprbooter start and end pfns
	 */
	s_pfn = ADDR_TO_PN(s_addr);
	e_pfn = ADDR_TO_PN(e_addr);

	handled = 0;			/* haven't done cprbooter yet */
	DEBUG1(errp("\ncprbooter: claiming pages... "));
	while (getchunk(&lo_pfn, &hi_pfn)) {
		static int spin_cnt;

		if ((spin_cnt++ & 0x3f) == 0)
			cprboot_spinning_bar();

		/*
		 * We need to extend any chunk that touches the image of
		 * cprbooter to include cprbooter as well, so as to keep the
		 * memory cprbooter occupies from being used by OBP for its
		 * own bookkeeping This requires lookahead in the case where
		 * we overlap or abut the beginning of cprbooter to make
		 * sure that if the next one overlaps or abuts the end we
		 * convert the request to one large contiguous one.
		 *
		 * If we've passed the cprbooter image without handling it,
		 * that means we need to sneak it in here because it did not
		 * touch another chunk
		 */
		if (!handled && hi_pfn < s_pfn - 1) {
			if (claim_chunk(s_pfn, e_pfn))
				return (-1);
			handled++;
		}
		/*
		 * If we've already done the cprbooter image
		 * or the chunk falls entirely above the cprbooter image
		 * or spans it completely
		 */
		if (handled || (lo_pfn > e_pfn + 1) ||
		    (hi_pfn >= e_pfn && lo_pfn <= s_pfn)) {
			if (claim_chunk(lo_pfn, hi_pfn))
				return (-1);
			if (!handled && (lo_pfn <= s_pfn))
				handled++;
		} else {
			/*
			 * If we're here there is overlap or abuttment
			 * but cprbooter image is not completely covered.
			 * Since we're processing from hi address, the max of
			 * the higher ends is the highest end of the enclosing
			 * space
			 */
			hi_extent = max(hi_pfn, e_pfn);		/* absolute */
			lo_extent = min(lo_pfn, s_pfn);		/* trial */
			/*
			 * while we haven't brought the low edge down to cover
			 * the cprbooter image, get a new low pfn
			 */
			while (lo_pfn > lo_extent &&
			    getchunk(&lo_pfn, &hi_pfn)) {
				if (hi_pfn < lo_extent - 1) {	/* now a gap */
					/*
					 * We've passed it and left a gap, so we
					 * have to claim two chunks
					 */
					if (claim_chunk(lo_extent, hi_extent))
						return (-1);
					/*
					 * now deal with the one we read ahead
					 */
					if (claim_chunk(lo_pfn, hi_pfn))
						return (-1);
					handled = 1;
				} else {
					lo_extent = min(lo_pfn, s_pfn);
				}
			}
			/*
			 * If we ran out of chunks without finding one below
			 * the bottom of cprbooter or we found one that exactly
			 * matches the bottom end
			 */
			if (lo_pfn >= lo_extent) {	/* ran out of chunks */
				if (claim_chunk(lo_extent, hi_extent))
					return (-1);
				handled = 1;
			}
		}
	}
	DEBUG1(errp(" \b\n"));

	DEBUG4(errp("cpr_claim_mem: totpages %d claimed %d pages\n", totpages,
	    totpg));
	return (0);
}

/*
 * Get bootargs from prom and parse it.
 * For cprbooter, bootargs has to started with "-F"; otherwise, something is
 * wrong.
 * Returns true on error.  Sets statefile path and reusable if specified.
 */
static int
check_bootargs(char **sp, int *rp)
{
	char *tp;
	int reusable = 0;
	char *special = NULL;

	/* Get bootargs from prom */
	tp = prom_bootargs();

	if (!tp || *tp == '\0') {
		errp("Null value in OBP boot-file, please reboot\n");
		return (1);
	}

	/* Has to be '-F' */
	if (!*tp || (*tp++ != '-') || (*tp++ != 'F')) {
		errp("OBP boot-file value '%s' is wrong, please reboot\n"
		    "usage is 'boot -F cprbooter cprboot [-R] [-S "
		    "<diskpath>]'\n", tp);
		return (1);
	}
	skip_whitespc(tp);
	while (*tp && *tp != ' ') /* Skip over filename1 */
		tp++;
	skip_whitespc(tp);
	if (*tp == '/')
		tp++;
	if (!*tp || strncmp(tp, cpr_bootsec, strlen(cpr_bootsec))) {
		errp("OBP boot-file value '%s' is wrong, please reboot\n"
		    "usage is 'boot -F cprbooter cprboot [-R] [-S "
		    "<diskpath>]'\n", tp);
		return (1);
	}
	/* Skip over secondary booter name */
	while (*tp && *tp != ' ')
		tp++;
	skip_whitespc(tp);
	/*
	 * do we have a block special statefile? or
	 * reusable flag?
	 */
	if (*tp == '\0') {
		*sp = special;
		*rp = reusable;
		return (0);
	}
	while (*tp && (*tp++ == '-')) {
		switch (*tp++) {
		case 'S':
		case 's':
			skip_whitespc(tp);
			if (*tp) {
				special = tp;
				while (*tp && *tp != ' ')
					tp++;
				if (*tp == ' ')
					*tp++ = '\0';
			} else {
				errp("OBP boot-file flag -S requires argument, "
				    "please reboot\n");
				return (1);
			}
			break;
		case 'R':
		case 'r':
			reusable = 1;
			break;
		default:
			errp("OBP boot-file arg unrecognized, please reboot\n");
			return (1);
		}
		skip_whitespc(tp);
	}
	if (*tp) {
		errp("OBP boot-file arg '%s' unrecognized, please reboot\n"
		    "usage is 'boot -F cprbooter cprboot [-R] [-S "
		    "<diskpath>]'\n", tp);
		return (1);
	}
	*sp = special;
	*rp = reusable;
	return (0);
}


/*
 * Allocate from BSS to avoid prom_alloc() and using any memory we need to
 * claim when restoring pages.  Memory allocate from here is never freed.
 * The cprbooter image is not reflected in the prom's physavail list,
 * due to ancient assumptions of bootblock clients.
 */
static caddr_t
cpr_alloc(size_t size)
{
	caddr_t retval;
	static caddr_t nextavail = 0;

	if (nextavail == 0)
		nextavail = (caddr_t)roundup((int)cpr_heap, sizeof (long));

	if (nextavail + size >= &cpr_heap[sizeof (cpr_heap)])
		return (0);

	retval = nextavail;
	nextavail = (caddr_t)roundup((int)nextavail + size, sizeof (long));

	return (retval);
}

/*
 * This function searches bitmaps, storing in the callers buffers
 * information about the next contiguous set of bits set.
 * It returns true if it found a chunk, else it returns false.
 */
static int
getchunk(pfn_t *lopfnp, pfn_t *hipfnp)
{
	static char *rmap, *vmap;
	static cbd_t *bdp;
	static int bit_offset;
	static int cc = 0;	/* call count */
	int endbit;

	if (cc++ == 0) {
		bdp = &bitmap_desc;
		rmap = (char *)bdp->cbd_reg_bitmap;
		vmap = (char *)bdp->cbd_vlt_bitmap;
		bit_offset = (bdp->cbd_size * NBBY) - 1;
	}

	/*
	 * ASSERT either we stopped on a zero bit or this is the first call.
	 */
	if (cc && bit_offset >= 0 &&
	    (isset(rmap, bit_offset) || isset(vmap, bit_offset))) {
		errp("getchunk: count %d, offset %d, bad bit set\n",
		    cc, bit_offset);
	}

	/*
	 * scan past consecutive 0 bits
	 */
	for (; bit_offset >= 0; bit_offset--)
		if (isset(rmap, bit_offset) || isset(vmap, bit_offset))
			break;

	if (bit_offset < 0)
		return (0);

	endbit = bit_offset--;

	/*
	 * scan for consecutive 1 bits
	 */
	while (bit_offset >= 0 &&
	    (isset(rmap, bit_offset) || isset(vmap, bit_offset))) {
		bit_offset--;
	}

	*lopfnp = bit_offset + 1;
	*hipfnp = endbit;

	return (1);
}

static int
claim_chunk(pfn_t lo_pfn, pfn_t hi_pfn)
{
	int error;
	int pages;
	u_longlong_t addr;
	struct prom_retain_info *rp;

	pages = (hi_pfn - lo_pfn + 1);
	addr = PN_TO_ADDR(lo_pfn);

	error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
	/*
	 * Might have tried to claim the chunk that includes
	 * the prom_retain'd msgbuf.  If so, trim or split and
	 * try again
	 */
	if (error) {
		rp = m_info.retain;
		/*
		 * If msgbuf is a chunk by itself, or the high
		 * end of a chunk, or the low end, then we can
		 * cope easily.
		 * If it is in the middle, then we just have to
		 * cross our fingers and hope that there is already
		 * a free chunk behind us if the prom needs one.
		 * We know that these are the only choices, since the msgbuf
		 * will be represented in the bitmap and is contiguous.
		 */
		if (lo_pfn == rp->spfn && (rp->spfn + rp->cnt - 1) == hi_pfn) {
			DEBUG4(errp("Prom-retained: skipping %x, %x pages "
			    "totpg %d\n", (int)lo_pfn, (int)hi_pfn, totpg));
			return (0);
		} else if (lo_pfn <= rp->spfn &&
		    (rp->spfn + rp->cnt - 1) == hi_pfn) {
			pages -= rp->cnt;
			error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
		} else if (lo_pfn == rp->spfn &&
		    (rp->spfn + rp->cnt - 1) < hi_pfn) {
			pages -= rp->cnt;
			addr += (rp->cnt * MMU_PAGESIZE);
			error = prom_claim_phys(pages * MMU_PAGESIZE, addr);
		} else if (lo_pfn < rp->spfn &&
		    hi_pfn > rp->spfn + rp->cnt - 1) {
			/*
			 * msgbuf is totally enclosed by the current
			 * chunk, we split into two and hope for
			 * the best
			 */
			pages = hi_pfn - (rp->spfn + rp->cnt) + 1;
			addr = PN_TO_ADDR(rp->spfn + rp->cnt);
			if (error = prom_claim_phys(pages * MMU_PAGESIZE,
			    addr)) {
				errp("cpr_claim_chunk: could not claim "
				    "hi part of chunk containing "
				    "prom_retain pages %x - %x\n",
				    rp->spfn + rp->cnt, (int)hi_pfn);
			} else {
				pages = rp->spfn - lo_pfn;
				addr = PN_TO_ADDR(lo_pfn);
				error = prom_claim_phys(pages * MMU_PAGESIZE,
				    addr);
			}
		}
	}
	if (error) {
		errp("cpr_claim_chunk: can't claim %x, %x totpg %d\n",
		    (int)lo_pfn, (int)hi_pfn, totpg);
		return (-1);
	} else {
		totpg += (hi_pfn - lo_pfn + 1);
		DEBUG4(errp("Claimed %x, %x pages totpg %d\n", (int)lo_pfn,
		    (int)hi_pfn, totpg));
		return (0);
	}
}
